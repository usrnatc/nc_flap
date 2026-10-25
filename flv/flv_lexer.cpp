#include "nc_defines.h"
#include "nc_types.h"
#include "nc_string.h"
#include "nc_simd.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "flv_lexer.h"
#include "flv_token.h"
#include "flv_intern.h"

// TODO(nathan): Investigate parallel -- can run sublexers in parallel

global const Str8 KEYWORDS[] = {
    ""_s8,
    "import"_s8,
    "if"_s8,
    "else"_s8,
    "var"_s8,
    "graph"_s8,
    "table"_s8,
    "text"_s8,
    "flag"_s8
};

global const TokenKind KEYWORD_KINDS[] = {
    (TokenKind) 0,
    TOKEN_KIND_IMPORT,
    TOKEN_KIND_IF,
    TOKEN_KIND_ELSE,
    TOKEN_KIND_VAR,
    TOKEN_KIND_GRAPH,
    TOKEN_KIND_TABLE,
    TOKEN_KIND_TEXT,
    TOKEN_KIND_FLAG

};

global const u64 KEYWORDS_COUNT = ARRAY_COUNT(KEYWORDS);
global u32 KEYWORD_NAME_BASE = NAME_INDEX_INVALID_VALUE;

// ((((val >> 32) ^ val) & 0xffffffff) * (uint64_t)3523216747) >> 32;
//    for (int i = 1; i < num_keywords; ++i) {
//     char keyword_copy[9] = {0};
//     strncpy(keyword_copy, keywords[i], 8);

//     uint64_t val = *((uint64_t*) keyword_copy);
//     uint8_t hash_val = hash(val);

//     lookup[hash_val] = i;
// }
// internal force_inline u16 const*
// BuildKeywordLUT(void)
// {
//     local u16 LUT[256] = {};
//     local b32 Initialised = FALSE;

//     // NOTE(nathan): fit string into 64 bits, hash => LUT[hash] = index in KEYWORDS
//     if (!Initialised) {
//         LUT[0x92] = 1;          // const   
//         LUT[0xC6] = 2;          // type    
//         LUT[0x64] = 3;          // for     
//         LUT[0xAA] = 4;          // while   
//         LUT[0x82] = 5;          // return  
//         LUT[0x89] = 6;          // struct  
//         LUT[0xAD] = 7;          // union   
//         LUT[0xDF] = 8;          // enum    
//         LUT[0x9B] = 9;          // break   
//         LUT[0xBE] = 10;         // goto    
//         LUT[0x08] = 11;         // case    
//         LUT[0xF1] = 12;         // switch  
//         LUT[0x36] = 13;         // continue
//         LUT[0x69] = 14;         // mod
//         LUT[0x02] = 15;         // if
//         LUT[0x10] = 16;         // else
//         LUT[0x03] = 17;         // volatile
//         LUT[0xD4] = 18;         // align
//         LUT[0xF3] = 19;         // import
//         LUT[0x7D] = 20;         // default
//         LUT[0x8A] = 21;         // sizeof
//         LUT[0x5A] = 22;         // cast
//         Initialised = TRUE;
//     }

//     return LUT;
// }

void
LexerInternKeywords(NamePool* Names)
{
    for (u64 Index = 1; Index < KEYWORDS_COUNT; ++Index) {
        NameIndex Name = InternName(Names, KEYWORDS[Index]);

        if (Index == 1)
            KEYWORD_NAME_BASE = Name.V;

        ASSERT_ALWAYS(Name.V == KEYWORD_NAME_BASE + (Index - 1));
    }
}

INTERNAL void
PushEnds(LexerState* State, u32 Bits, u32 StartIndex)
{
    while (Bits) {
        State->Ends[State->EndsCount++] = StartIndex + Ctz32(Bits);
        Bits &= Bits - 1;
    }
}

void 
UpdateState(
    LexerState* State,
    v256 Kinds,
    v256 Offsets,
    u64 Count, 
    u32 StartIndex
) {
    if (!Count)
        return;

    _mm256_storeu_si256((v256*) (State->Kinds + State->Count), Kinds);

    ALIGNAS(NC_SIMD_ALIGN256) u8 TokenOffsets[NC_SIMD_ALIGN256];

    _mm256_store_si256((v256*) TokenOffsets, Offsets);

    for (u64 Index = 0; Index < Count; ++Index)
        State->Offsets[State->Count + Index] = TokenOffsets[Index] + StartIndex;

    State->Count += Count;
}

void 
_mm256_pext(v256* Vec, v256 Mask, u64* Count)
{
    const u64* Vec64 = (u64*) Vec;
    const u64* Mask64 = (u64*) &Mask;

    *Count = 0;

    for (u64 Index = 0; Index < 4; ++Index) {
        const u64 Temp = _pext_u64(Vec64[Index], Mask64[Index]);
        *(u64*) ((u8*) Vec + *Count) = Temp;
        *Count += _mm_popcnt_u64(Mask64[Index]) >> 3;
    }
}

v256
_mm256_cmpistrm_any(v128 Match, v256 Vec)
{
    v128 LowVec = _mm256_extractf128_si256(Vec, 0);
    v128 HighVec = _mm256_extractf128_si256(Vec, 1);
    v128 LowOutsideRangeMask = _mm_cmpestrm(Match, 16, LowVec, 16, (1 << 6));
    v128 HighOutsideRangeMask = _mm_cmpestrm(Match, 16, HighVec, 16, (1 << 6));

    return _mm256_set_m128i(HighOutsideRangeMask, LowOutsideRangeMask);
}

v256
_mm256_cmpistrm_rng(v128 Ranges, u64 RangesCount, v256 Vec)
{
    v128 LowVec = _mm256_extractf128_si256(Vec, 0);
    v128 HighVec = _mm256_extractf128_si256(Vec, 1);
    v128 LowOutsideRangeMask = _mm_cmpestrm(
        Ranges, 
        RangesCount, 
        LowVec, 
        16, 
        (1 << 6) | (1 << 2)
    );
    v128 HighOutsideRangeMask = _mm_cmpestrm(
        Ranges, 
        RangesCount, 
        HighVec, 
        16, 
        (1 << 6) | (1 << 2)
    );

    return _mm256_set_m128i(HighOutsideRangeMask, LowOutsideRangeMask);
}

TokenArray 
LexString(Arena* MemPool, Str8 String, NamePool* Names)
{
    LexerState State = {};
    TokenArray Result = {};
    u64 BufferSize = String.Size + 4;
    TempArena Scratch = GetScratch(&MemPool, 1);

    ASSERT(KEYWORD_NAME_BASE != NAME_INDEX_INVALID_VALUE);

    if (String.Size) {
        State.Kinds = ArenaPushArrayEx(
            Scratch.MemPool,
            TokenKind,
            BufferSize,
            NC_SIMD_ALIGN256
        );
        State.Offsets = ArenaPushArrayEx(
            Scratch.MemPool,
            u32,
            BufferSize,
            NC_SIMD_ALIGN256
        );
        State.Ends = ArenaPushArrayEx(
            Scratch.MemPool,
            u32,
            BufferSize,
            NC_SIMD_ALIGN256
        );

        u8 LastChar = 0;
        u32 PendingEnd = U32_MAX;
        b32 ChContinue = FALSE;
        b32 EscContinue = FALSE;
        b32 StrContinue = FALSE;
        b32 LnCommContinue = FALSE;
        b32 BlockCommContinue = FALSE;
        v256 CurrentVec = _mm256_loadu_si256((v256*) String.Str);
        v256 CurrentSourceVec = CurrentVec;

        for (u64 Index = 0; Index < String.Size; Index += NC_SIMD_ALIGN256) {
            v256 NextVec = _mm256_loadu_si256(
                (v256*) (String.Str + Index + NC_SIMD_ALIGN256)
            );
            const v256 NextSourceVec = NextVec;
            v256 Tags = RunSubLexers(
                &CurrentVec,
                &NextVec,
                CurrentSourceVec,
                LastChar,
                &ChContinue,
                &EscContinue,
                &StrContinue,
                &LnCommContinue,
                &BlockCommContinue
            );
            u64 Count = 0;
            v256 Indices = {};

            FindTokenIndices(&Tags, &Indices, &Count);
            UpdateState(&State, Tags, Indices, Count, SafeTruncateU64(Index));

            u32 Live = _mm256_movemask_epi8(NonZeroMask(CurrentVec));

            if (PendingEnd != U32_MAX && !(Live & 1))
                State.Ends[State.EndsCount++] = PendingEnd;

            PushEnds(
                &State, 
                Live & ~(Live >> 1) & 0x7FFFFFFFU, 
                SafeTruncateU64(Index)
            );
            LastChar = (u8) _mm256_extract_epi8(
                CurrentVec, 
                NC_SIMD_ALIGN256 - 1
            );

            if (LastChar)
                PendingEnd = SafeTruncateU64(Index) + (NC_SIMD_ALIGN256 - 1);
            else
                PendingEnd = U32_MAX;

            CurrentVec = NextVec;
            CurrentSourceVec = NextSourceVec;
        }

        if (PendingEnd != U32_MAX)
            State.Ends[State.EndsCount++] = PendingEnd;
    }

    u64 EndCursor = 0;

    Result.TokensCapacity = SafeTruncateU64(State.Count + 1);
    Result.Tokens = ArenaPushArray(
        MemPool,
        Token,
        Result.TokensCapacity
    );
    Result.Names = ArenaPushArray(
        MemPool,
        NameIndex,
        Result.TokensCapacity
    );

    for (u64 Index = 0; Index < State.Count; ++Index) {
        Token* Tok = &Result.Tokens[Index];
        u8 Kind = State.Kinds[Index];
        u32 Offset = State.Offsets[Index];
        NameIndex Name = {
            NAME_INDEX_INVALID_VALUE
        };

        if (Kind == TOKEN_KIND_IDENT || Kind == TOKEN_KIND_NUMBER) {
            u32 End = State.Ends[EndCursor++];

            ASSERT(End >= Offset);

            Tok->Count = (u16) (End + 1 - Offset);
        } else if (Index + 1 < State.Count) {
            Tok->Count = (u16) (State.Offsets[Index + 1] - Offset);
        } else {
            Tok->Count = (u16) (String.Size - Offset);
        }

        if (Kind == TOKEN_KIND_IDENT) {
            Name = InternName(
                Names, 
                Str(String.Str + Offset, Tok->Count)
            );

            if (Name.V - KEYWORD_NAME_BASE < KEYWORDS_COUNT - 1)
                Kind = KEYWORD_KINDS[Name.V - KEYWORD_NAME_BASE + 1];
        }

        Tok->Kind = Kind;
        Tok->Offset = Offset;
        Result.Names[Index] = Name;
    }

    Token* Tok = &Result.Tokens[State.Count];

    Tok->Kind = TOKEN_KIND_EOF;
    Tok->Offset = SafeTruncateU64(String.Size);
    Tok->Count = 0;
    Result.Names[State.Count].V = NAME_INDEX_INVALID_VALUE;
    Result.TokensCount = SafeTruncateU64(State.Count + 1);
    ReleaseScratch(Scratch);

    return Result;
}

u32*
LineOffsetsFromSource(Arena* MemPool, Str8 Source, OUT u32* Count)
{
    u32 LineCount = 1;

    for (u64 Index = 0; Index < Source.Size; ++Index)
        LineCount += (Source.Str[Index] == '\n');

    u32* Result = ArenaPushArray(MemPool, u32, LineCount);
    u32 Cursor = 1;

    *Result = 0;

    for (u64 Index = 0; Index < Source.Size; ++Index) {
        if (Source.Str[Index] == '\n')
            Result[Cursor++] = SafeTruncateU64(Index + 1);
    }

    *Count = LineCount;

    return Result;
}

u32
LineFromOffset(u32 const* LineOffsets, u32 LineCount, u32 Offset)
{
    u32 Low = 0;
    i32 High = LineCount;

    while (High - Low > 1) {
        u32 Mid = Low + (High - Low) / 2;

        if (LineOffsets[Mid] <= Offset)
            Low = Mid;
        else
            High = Mid;
    }

    return Low + 1;
}

// TokenArray 
// LexString(Arena* MemPool, Str8 String, NamePool* Names)
// {
//     LexerState State = {};
//     u64 BufferSize = String.Size + 4;
//     TempArena Scratch = GetScratch(&MemPool, 1);

//     if (String.Size) {
//         State.Kinds = ArenaPushArrayEx(
//             Scratch.MemPool,
//             TokenKind,
//             BufferSize,
//             NC_SIMD_ALIGN256
//         );
//         State.Offsets = ArenaPushArrayEx(
//             Scratch.MemPool,
//             u32,
//             BufferSize,
//             NC_SIMD_ALIGN256
//         );

//         u8 LastChar = 0;
//         b32 ChContinue = FALSE;
//         b32 EscContinue = FALSE;
//         b32 StrContinue = FALSE;
//         b32 LnCommContinue = FALSE;
//         b32 BlockCommContinue = FALSE;
//         v256 CurrentVec = _mm256_loadu_si256((v256*) String.Str);
//         v256 CurrentSourceVec = CurrentVec;

//         for (u64 Index = 0; Index < String.Size; Index += NC_SIMD_ALIGN256) {
//             v256 NextVec = _mm256_loadu_si256((v256*) (String.Str + Index + NC_SIMD_ALIGN256));
//             const v256 NextSourceVec = NextVec;
//             v256 Tags = RunSubLexers(
//                 &CurrentVec,
//                 &NextVec,
//                 CurrentSourceVec,
//                 LastChar,
//                 &ChContinue,
//                 &EscContinue,
//                 &StrContinue,
//                 &LnCommContinue,
//                 &BlockCommContinue
//             );
//             u64 Count = 0;
//             v256 Indices;

//             FindTokenIndices(&Tags, &Indices, &Count);
//             UpdateState(&State, Tags, Indices, Count, Index);
//             _mm256_storeu_si256((v256*) (String.Str + Index), CurrentVec);
//             LastChar = String.Str[Index + (NC_SIMD_ALIGN256 - 1)];
//             CurrentVec = NextVec;
//             CurrentSourceVec = NextSourceVec;
//         }

//         u16 const* LUT = BuildKeywordLUT();

//         FindKeywords(&State, LUT, String);
//     }

//     TokenArray Result = {};

//     if (State.Count) {
//         Result.TokensCapacity = State.Count + 1;
//         Result.Tokens = ArenaPushArray(MemPool, Token, Result.TokensCapacity);
//         Result.Names = ArenaPushArray(MemPool, NameIndex, Result.TokensCapacity);
//         MemSet(Result.Names, 0xFF, Result.TokensCapacity * sizeof(NameIndex));

//         for (u64 Index = 0; Index < State.Count; ++Index) {
//             Token* Tok = &Result.Tokens[Index];

//             Tok->Kind = State.Kinds[Index];
//             Tok->Offset = State.Offsets[Index];

//             if (Index + 1 < State.Count) {
//                 Tok->Count = (State.Offsets[Index + 1] - Tok->Offset);
//             } else {
//                 Tok->Count = (String.Size - Tok->Offset);
//             }

//             if (Tok->Kind == TOKEN_KIND_IDENT || Tok->Kind == TOKEN_KIND_NUMBER) {
//                 u32 TokEnd = Tok->Offset + Tok->Count;

//                 while (TokEnd > Tok->Offset && !String.Str[TokEnd - 1])
//                     --TokEnd;

//                 Tok->Count = TokEnd - Tok->Offset;
//             }

//             if (Tok->Kind == TOKEN_KIND_IDENT) {
//                 Result.Names[Index] = InternName(
//                     Names, 
//                     Str(String.Str + Tok->Offset, Tok->Count)
//                 );
//             }
//         }

//         Token* Tok = &Result.Tokens[State.Count];

//         Tok->Kind = TOKEN_KIND_EOF;
//         Tok->Offset = String.Size;
//         Tok->Count = 0;
//         Result.TokensCount = State.Count + 1;
//     }

//     ReleaseScratch(Scratch);

//     return Result;
// }

// force_inline u8 
// Hash(u64 Value)
// {
//     return ((((Value >> 32) ^ Value) & 0xFFFFFFFF) * (u64) 3523216747) >> 32;
// }

v256
ClassifySingleCharacter(v256 Input)
{
    v256 LowerNibbleMask = _mm256_set_epi8(
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F
    );
    v256 LUT1 = _mm256_set_epi8(
        3,  15,  15,  11,  15,  3,  1,  1,
        0,   1,   1,   0,   0,  0,  1,  0,
        3,  15,  15,  11,  15,  3,  1,  1,
        0,   1,   1,   0,   0,  0,  1,  0
    );
    v256 Mask1 = _mm256_shuffle_epi8(
        LUT1, 
        _mm256_and_si256(LowerNibbleMask, Input)
    );

    Input = _mm256_srli_epi32(Input, 4);

    v256 LUT2 = _mm256_set_epi8(
        0,  0,  0,  0,  0,  0,  0,  0,
        8,  0,  4,  0,  2,  1,  0,  0,
        0,  0,  0,  0,  0,  0,  0,  0,
        8,  0,  4,  0,  2,  1,  0,  0
    );
    v256 Mask2 = _mm256_shuffle_epi8(
        LUT2, 
        _mm256_and_si256(LowerNibbleMask, Input)
    );
    v256 Mask = _mm256_and_si256(Mask1, Mask2);
    v256 Cmp = _mm256_cmpeq_epi8(Mask, _mm256_setzero_si256());

    return _mm256_xor_si256(Cmp, _mm256_set1_epi8(-1));
}

// for (int i = 0; i < tok_array->size; i += VECTOR_SIZE) {
//     const __m256i current_vec = load_vector((const char *) tok_array->token_types + i);
//     const __m256i mask = _mm256_cmpeq_epi8(
//         current_vec,
//         _mm256_set1_epi8(TOK_IDENT)
//     );

//     int size;
//     uint8_t token_indices[] __attribute__((aligned(32))) = {
//         0, 1, 2, 3, 4, 5, 6, 7,
//         8, 9, 10, 11, 12, 13, 14, 15,
//         16, 17, 18, 19, 20, 21, 22, 23,
//         24, 25, 26, 27, 28, 29, 30, 31
//     };
//     mm256_pext((__m256i *) token_indices, mask, &size);

//     for (int j = 0; j < size; ++j) {
//         int pos = i + token_indices[j];
//         char str[9] = {0};
//         strncpy(str, tok_array->src + tok_array->token_locs[pos], 8);

//         const uint64_t val = *((uint64_t*) str);
//         const uint8_t hash_val = hash(val);
//         const uint8_t keyword_pos = lookup[hash_val];

//         bool are_equal = strcmp(
//                 tok_array->src + tok_array->token_locs[pos],
//                 keywords[keyword_pos]
//             ) == 0;
//         TokenType keyword_type = keyword_types[keyword_pos];

//         // are_equal ? keyword_id : TOK_IDENT
//         tok_array->token_types[pos] = TOK_IDENT ^ ((keyword_type ^ TOK_IDENT) & -!!are_equal);;
//     }
// }

// void 
// FindKeywords(LexerState* State, u16 const* Table, Str8 Source)
// {
//     for (u64 Index = 0; Index < State->Count; ++Index) {
//         if (State->Kinds[Index] != TOKEN_KIND_IDENT)
//             continue;

//         u32 CurrentOffset = State->Offsets[Index];
//         u32 NextOffset = (Index + 1 < State->Count) ? State->Offsets[Index + 1] : Source.Size;
//         u32 EndOffset = NextOffset;

//         while (EndOffset > CurrentOffset && !Source.Str[EndOffset - 1])
//             --EndOffset;

//         u32 TokenLength = EndOffset - CurrentOffset;

//         if (TokenLength <= 8) {
//             u64 Value = 0;
//             
//             MemCpy(&Value, Source.Str + CurrentOffset, TokenLength);

//             const u8 ValueHash = Hash(Value);
//             const u16 KeywordIndex = Table[ValueHash];

//             if (
//                 KeywordIndex && 
//                 KeywordIndex < (u16) KEYWORDS_COUNT &&
//                 KEYWORDS[KeywordIndex].Size == TokenLength
//             ) {
//                 if (MemCmp(
//                         Source.Str + CurrentOffset, 
//                         KEYWORDS[KeywordIndex].Str, 
//                         TokenLength
//                     )
//                 ) {
//                     State->Kinds[Index] = KEYWORD_KINDS[KeywordIndex];
//                 }
//             }
//         }
//     }
// }

v256
NonZeroMask(const v256 Vec)
{
    v256 Mask = _mm256_cmpeq_epi8(Vec, _mm256_setzero_si256());

    Mask = _mm256_xor_si256(Mask, _mm256_set1_epi32(-1));

    return Mask;
}

void 
FindTokenIndices(v256* TokenTags, v256* TokenIndex, u64* Count)
{
    v256 Mask = NonZeroMask(*TokenTags);

    *TokenIndex = _mm256_setr_epi8(
         0,  1,  2,  3,  4,  5,  6,  7,
         8,  9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23,
        24, 25, 26, 27, 28, 29, 30, 31
    );
    _mm256_pext(TokenIndex, Mask, Count);
    _mm256_pext(TokenTags, Mask, Count);
}

// TODO(nathan): maybe don't throw away the newline information but instead
//             : use them to create a list of newline offsets that we can use
//             : to determine line numbers in error messages.
//             : i.e. error at offset 20: 
//             : for i in [0..len(newline-list)-1] -> list[i] >= 20, return (i + 1)
void 
ReplaceWhitespace(v256* Vec)
{
    v256 WhiteSpaceMask = _mm256_cmpeq_epi8(*Vec, _mm256_set1_epi8(' '));

    WhiteSpaceMask = _mm256_or_si256(
        WhiteSpaceMask, 
        _mm256_cmpeq_epi8(
            *Vec, 
            _mm256_set1_epi8('\n')
        )
    );
    WhiteSpaceMask = _mm256_or_si256(
        WhiteSpaceMask, 
        _mm256_cmpeq_epi8(
            *Vec, 
            _mm256_set1_epi8('\t')
        )
    );
    WhiteSpaceMask = _mm256_or_si256(
        WhiteSpaceMask, 
        _mm256_cmpeq_epi8(
            *Vec, 
            _mm256_set1_epi8('\r')
        )
    );
    *Vec = _mm256_blendv_epi8(*Vec, _mm256_setzero_si256(), WhiteSpaceMask);
}

void 
ReplaceTokenBody(v256* Vec)
{
    v256 Mask = _mm256_cmpeq_epi8(*Vec, _mm256_set1_epi8(TOKEN_KIND_PROG));

    *Vec = _mm256_blendv_epi8(*Vec, _mm256_setzero_si256(), Mask);
}

b32 
IsEmpty(v256 Vec)
{
    v256 Cmp = _mm256_cmpeq_epi8(Vec, _mm256_setzero_si256());

    return (_mm256_movemask_epi8(Cmp) == 0xFFFFFFFF);
}

v256
RunSubLexers(
    v256* CurrentVec, 
    v256* NextVec, 
    const v256 CurrentSourceVec, 
    u8 LastChar, 
    b32* ChContinue, 
    b32* EscContinue, 
    b32* StrContinue, 
    b32* LnCommContinue, 
    b32* BlockCommContinue
) {
    v256 Tags = _mm256_setzero_si256();

    LexInlineComments(CurrentVec, *NextVec, LnCommContinue);

    if (!IsEmpty(*CurrentVec)) {
        LexTripleCharSymbol(CurrentVec, NextVec, &Tags);
        LexDoubleCharSymbol(CurrentVec, NextVec, &Tags);
        LexSingleCharSymbol(CurrentVec, *NextVec, &Tags, LastChar);
        ReplaceWhitespace(CurrentVec);
        LexIdentifiers(*CurrentVec, &Tags, !LastChar);
        LexNumericConstants(*CurrentVec, &Tags, !LastChar);

        b32 __Temp = *EscContinue;

        LexTextLiteral(
            CurrentVec,
            &Tags,
            '\'',
            ChContinue,
            TOKEN_KIND_CHAR_LIT,
            CurrentSourceVec,
            &__Temp
        );
        LexTextLiteral(
            CurrentVec,
            &Tags,
            '"',
            StrContinue,
            TOKEN_KIND_STR_LIT,
            CurrentSourceVec,
            EscContinue
        );
        ReplaceTokenBody(&Tags);
    }

    return Tags;
}

v256
NumericPeriodsMask(v256 CurrentVec, v256 NextVec, u8 LastChar)
{
    u64 IsPeriod = _mm256_movemask_epi8(
        _mm256_cmpeq_epi8(
            CurrentVec, 
            _mm256_set1_epi8('.')
        )
    );
    u64 HasNumAfter = _mm256_movemask_epi8(
        NumericMask(
            PeekNextSingle(
                CurrentVec, 
                NextVec
            )
        )
    );
    u64 HasNumBefore = _mm256_movemask_epi8(NumericMask(CurrentVec));

    HasNumBefore = (HasNumBefore << 1) | (LastChar >= '0' && LastChar <= '9');

    return GetMask(IsPeriod & (HasNumBefore | HasNumAfter));
}

void 
LexSingleCharSymbol(
    v256* CurrentVec, 
    v256 NextVec, 
    v256* Tags, 
    u8 LastChar
) {
    v256 Mask = ClassifySingleCharacter(*CurrentVec);

    Mask = _mm256_xor_si256(
        Mask, 
        NumericPeriodsMask(*CurrentVec, NextVec, LastChar)
    );
    *Tags = _mm256_blendv_epi8(*Tags, *CurrentVec, Mask);
    *CurrentVec = _mm256_blendv_epi8(
        *CurrentVec, 
        _mm256_setzero_si256(), 
        Mask
    );
}

v256
PeekNextSingle(v256 CurrentVec, v256 NextVec)
{
    return _mm256_alignr_epi8(
        _mm256_permute2x128_si256(NextVec, CurrentVec, 3),
        CurrentVec,
        1
    );
}

v256
PeekNextDouble(v256 CurrentVec, v256 NextVec)
{
    return _mm256_alignr_epi8(
        _mm256_permute2x128_si256(NextVec, CurrentVec, 3),
        CurrentVec,
        2
    );
}

v256
GetMask(const u32 Mask)
{
    v256 VMask = _mm256_set1_epi32(Mask);
    const v256 Shuffle = _mm256_setr_epi64x(
        0x0000000000000000, 
        0x0101010101010101, 
        0x0202020202020202, 
        0x0303030303030303
    );

    VMask = _mm256_shuffle_epi8(VMask, Shuffle);

    const v256 BitMask = _mm256_set1_epi64x(0x7FBFDFEFF7FBFDFE);

    VMask = _mm256_or_si256(VMask, BitMask);

    return _mm256_cmpeq_epi8(VMask, _mm256_set1_epi64x(-1));
}

void 
RemovePrefix(v256* Vec, u64 Prefix)
{
    *Vec = _mm256_blendv_epi8(
        _mm256_setzero_si256(),
        *Vec,
        _mm256_setr_epi64x(
            0xFFFFFFFFFFFFFFFF - Prefix,
            0xFFFFFFFFFFFFFFFF,
            0xFFFFFFFFFFFFFFFF,
            0xFFFFFFFFFFFFFFFF
        )
    );
}

void 
LexDoubleCharSymbol(v256* CurrentVec, v256* NextVec, v256* Tags)
{
    const v256 Shifted1 = PeekNextSingle(*CurrentVec, *NextVec);
    char const FirstBytes[] = ":.!><|&+-*/^%=";
    u32 FirstMasks[ARRAY_COUNT(FirstBytes) - 1];

    for (u8 Index = 0; Index < ARRAY_COUNT(FirstBytes) - 1; ++Index) {
        FirstMasks[Index] = _mm256_movemask_epi8(
            _mm256_cmpeq_epi8(
                *CurrentVec, 
                _mm256_set1_epi8(
                    FirstBytes[Index]
                )
            )
        );
    }
    
    char const SecondBytes[] = "=.|&><+-";
    u32 SecondMasks[ARRAY_COUNT(SecondBytes) - 1];

    for (u8 Index = 0; Index < ARRAY_COUNT(SecondBytes) - 1; ++Index) {
        SecondMasks[Index] = _mm256_movemask_epi8(
            _mm256_cmpeq_epi8(
                Shifted1, 
                _mm256_set1_epi8(
                    SecondBytes[Index]
                )
            )
        );
    }

    u8 const PunctuationData[][2] = {
        // { 0, 0},
        { 1, 1},
        { 2, 0},
        { 3, 0},
        { 4, 0},
        { 5, 2},
        { 6, 3},
        { 3, 4},
        { 4, 5},
        { 7, 6},
        { 8, 7},
        { 8, 4},
        { 7, 0},
        { 8, 0},
        { 9, 0},
        {10, 0},
        {11, 0},
        { 5, 0},
        { 6, 0},
        {12, 0},
        {13, 0}
    };
    u32 Mask = 0;

    for (u8 Index = 0; Index < ARRAY_COUNT(PunctuationData); ++Index) {
        const u8 X = PunctuationData[Index][0];
        const u8 Y = PunctuationData[Index][1];

        Mask |= (FirstMasks[X] & SecondMasks[Y]);
    }

    Mask ^= Mask & (Mask << 1) & (Mask >> 1);
    Mask ^= Mask & (Mask << 1);

    v256 Kinds = _mm256_sub_epi8(
        _mm256_adds_epu8(
            *CurrentVec, 
            Shifted1
        ),
        _mm256_set1_epi8(2)
    );

    *Tags = _mm256_blendv_epi8(*Tags, Kinds, GetMask(Mask));
    *Tags = _mm256_blendv_epi8(
        *Tags, 
        _mm256_setzero_si256(), 
        GetMask(Mask << 1)
    );
    *CurrentVec = _mm256_blendv_epi8(
        *CurrentVec, 
        _mm256_setzero_si256(), 
        GetMask(Mask | Mask << 1)
    );

    u8 Carry = (
        (Mask & (1 << (NC_SIMD_ALIGN256 - 1))) >> (NC_SIMD_ALIGN256 - 1)
    ) * 0xFF;

    RemovePrefix(NextVec, Carry);
}

// FIXME(nc): can probs trust the compiler to optimise all of this but
//          : there is no reason we cannot unroll these loops ourselves
//          : I doubt that we will add more tyiple char symbols in the future
void 
LexTripleCharSymbol(v256* CurrentVec, v256* NextVec, v256* Tags)
{
    v256 SingleShift = _mm256_alignr_epi8(
        _mm256_permute2x128_si256(*NextVec, *CurrentVec, 3),
        *CurrentVec,
        1
    );
    v256 DoubleShift = _mm256_alignr_epi8(
        _mm256_permute2x128_si256(*NextVec, *CurrentVec, 3),
        *CurrentVec,
        2
    );
    char const* FirstTwoBytes = "<>";
    u32 FirstMasks[2] = {};
    u32 SecondMasks[2] = {};
    u32 ThirdMask[1] = {};

    // TODO(nc): unroll
    for (u32 Index = 0; Index < 2; ++Index) {
        FirstMasks[Index] = _mm256_movemask_epi8(
            _mm256_cmpeq_epi8(
                *CurrentVec,
                _mm256_set1_epi8(FirstTwoBytes[Index])
            )
        );
        SecondMasks[Index] = _mm256_movemask_epi8(
            _mm256_cmpeq_epi8(
                SingleShift,
                _mm256_set1_epi8(FirstTwoBytes[Index])
            )
        );
    }

    char const* ThirdBytes = "=";

    ThirdMask[0] = _mm256_movemask_epi8(
        _mm256_cmpeq_epi8(
            DoubleShift,
            _mm256_set1_epi8(ThirdBytes[0])
        )
    );

    const u8 PunctuationData[2][3] = {
        {1, 1, 1},      // <<=
        {2, 2, 1}       // >>=
    };
    u32 Mask = 0;

    // TODO(nc): unroll
    for (u32 Index = 0; Index < 2; ++Index) {
        const u8 X = PunctuationData[Index][0];
        const u8 Y = PunctuationData[Index][1];
        const u8 Z = PunctuationData[Index][2];

        Mask |= (FirstMasks[X] & SecondMasks[Y] & ThirdMask[Z]);
    }

    // NOTE(nc): CurrentVec + SingleShift + DoubleShift
    v256 TokenTypes = _mm256_add_epi8(
        *CurrentVec,
        _mm256_add_epi8(
            SingleShift,
            DoubleShift
        )
    );

    // NOTE(nc): update tags
    *Tags = _mm256_blendv_epi8(
        *Tags,
        TokenTypes,
        GetMask(Mask)
    );

    // NOTE(nc): remove tail bytes tags
    *Tags = _mm256_blendv_epi8(
        *Tags,
        _mm256_setzero_si256(),
        GetMask(Mask << 1 | Mask << 2)
    );

    // NOTE(nc): remove symbols found
    *CurrentVec = _mm256_blendv_epi8(
        *CurrentVec,
        _mm256_setzero_si256(),
        GetMask(Mask | Mask << 1 | Mask << 2)
    );

    // NOTE(nc): remove chars from NextVec if they are a continuation of a
    //         : current symbol
    u64 Carry = (
        ((Mask & (1 << 31)) >> 31) * 0xFFFF +
        ((Mask & (1 << 30)) >> 30) * 0xFF
    );

    RemovePrefix(NextVec, Carry);
}

v256
AlphaMask(v256 Vec)
{
    v128 Ranges = _mm_set_epi8(
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        'z', 
        'a', 
        'Z', 
        'A'
    );

    return _mm256_cmpistrm_rng(Ranges, 4, Vec);
}

void 
LexIdentifiers(v256 CurrentVec, v256* Tags, b32 LastEmpty)
{
    v256 HaveAlpha = AlphaMask(CurrentVec);
    v256 HaveUnderscore = _mm256_cmpeq_epi8(
        CurrentVec, 
        _mm256_set1_epi8('_')
    );
    v256 IdentStartMask = _mm256_or_si256(HaveAlpha, HaveUnderscore);
    u32 HasWhiteSpaceBefore = _mm256_movemask_epi8(
        _mm256_cmpeq_epi8(
            CurrentVec, 
            _mm256_setzero_si256()
        )
    );

    HasWhiteSpaceBefore = (HasWhiteSpaceBefore << 1) | !!LastEmpty;
    IdentStartMask = _mm256_and_si256(
        IdentStartMask, 
        GetMask(HasWhiteSpaceBefore)
    );
    *Tags = _mm256_blendv_epi8(
        *Tags, 
        _mm256_set1_epi8(TOKEN_KIND_IDENT), 
        IdentStartMask
    );
}

v256
NumericMask(v256 Vec)
{
    v128 Ranges = _mm_set_epi8(
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        0, 
        '9', 
        '0'
    );

    return _mm256_cmpistrm_rng(Ranges, 2, Vec);
}

void 
LexNumericConstants(
    const v256 CurrentVec, 
    v256* Tags, 
    const b32 LastEmpty
) {
    v256 NumStartMask = NumericMask(CurrentVec);
    u32 HasWhitespaceBefore = _mm256_movemask_epi8(
        _mm256_cmpeq_epi8(
            CurrentVec, 
            _mm256_setzero_si256()
        )
    );

    HasWhitespaceBefore = (HasWhitespaceBefore << 1) | !!LastEmpty;

    v256 IsNumPeriod = _mm256_cmpeq_epi8(CurrentVec, _mm256_set1_epi8('.'));

    NumStartMask = _mm256_and_si256(
        NumStartMask, 
        GetMask(HasWhitespaceBefore)
    );
    NumStartMask = _mm256_or_si256(
        NumStartMask, 
        _mm256_and_si256(
            IsNumPeriod, 
            GetMask(HasWhitespaceBefore)
        )
    );
    *Tags = _mm256_blendv_epi8(
        *Tags, 
        _mm256_set1_epi8(TOKEN_KIND_NUMBER), 
        NumStartMask
    );
}

void 
LexInlineComments(v256* CurrentVec, v256 NextVec, b32* LnCommContinue)
{
    const v256 Shifted1 = PeekNextSingle(*CurrentVec, NextVec);
    v256 HaveSlash = _mm256_cmpeq_epi8(*CurrentVec, _mm256_set1_epi8('/'));
    v256 HaveSlashAfter = _mm256_cmpeq_epi8(Shifted1, _mm256_set1_epi8('/'));
    u32 Starts = _mm256_movemask_epi8(
        _mm256_and_si256(
            HaveSlash, 
            HaveSlashAfter
        )
    );
    u32 Ends = _mm256_movemask_epi8(
        _mm256_cmpeq_epi8(
            *CurrentVec,
            _mm256_set1_epi8('\n')
        )
    );
    u32 RegionU32 = 0;
    u32 Pos = 0;
    b32 Inside = *LnCommContinue;

    while (Pos < NC_SIMD_ALIGN256) {
        u32 Tail = ~0U << Pos;

        if (!Inside) {
            u32 Rest = Starts & Tail;

            if (!Rest)
                break;

            Pos = _tzcnt_u32(Rest);
            Tail = ~0U << Pos;
            Inside = TRUE;
        }

        u32 Rest = Ends & Tail;
        u32 End = Rest
            ? _tzcnt_u32(Rest)
            : NC_SIMD_ALIGN256;
        u32 Head = (End == NC_SIMD_ALIGN256)
            ? ~0U
            : ((1U << End) - 1);

        RegionU32 |= Tail & Head;
        Inside = (End == NC_SIMD_ALIGN256);
        Pos = End + 1;
    }

    *LnCommContinue = Inside;
    *CurrentVec = _mm256_blendv_epi8(
        *CurrentVec,
        _mm256_setzero_si256(),
        GetMask(RegionU32)
    );
}

void 
LexTextLiteral(
    v256* CurrentVec, 
    v256* Tags, 
    const u8 Delim, 
    b32* ShouldContinue, 
    const TokenKind Kind, 
    const v256 CurrentSourceVec, 
    b32* EscContinue
) {
    u32 IsDelim = _mm256_movemask_epi8(
        _mm256_and_si256(
            _mm256_cmpeq_epi8(
                *CurrentVec,
                _mm256_set1_epi8(Delim)
            ),
            _mm256_cmpeq_epi8(
                *Tags,
                _mm256_setzero_si256()
            )
        )
    );

    u32 IsBackslash = _mm256_movemask_epi8(
        _mm256_cmpeq_epi8(
            *CurrentVec,
            _mm256_set1_epi8('\\')
        )
    );

    const u32 O = 0xAAAAAAAA;
    const u32 E = 0x55555555;

    IsBackslash = (IsBackslash ^ (*EscContinue)) & IsBackslash;

    u64 EscapedCh = (
        (
            (
                (
                    IsBackslash + (IsBackslash & ~(IsBackslash << 1) & E)
                ) & ~IsBackslash
            ) & ~E
        ) | 
        (
            (
                (
                    IsBackslash + ((IsBackslash & ~(IsBackslash << 1)) & O)
                ) & ~IsBackslash
            ) & E
        )
    );

    EscapedCh ^= *EscContinue;
    IsDelim ^= (EscapedCh & IsDelim);

    u32 Region = _mm_cvtsi128_si32(
        _mm_clmulepi64_si128(
            _mm_set_epi32(0, 0, 0, IsDelim ^ (*ShouldContinue)),
            _mm_set1_epi8(-1),
            0
        )
    );

    *Tags = _mm256_blendv_epi8(
        *Tags,
        _mm256_setzero_si256(),
        GetMask(Region)
    );
    *ShouldContinue = (Region >> 31) & 1;
    *EscContinue = ((IsBackslash >> 31) & 1) & (!((EscapedCh >> 31) & 1));
    IsDelim &= Region;
    *Tags = _mm256_blendv_epi8(
        *Tags,
        _mm256_set1_epi8(TOKEN_KIND_PROG),
        GetMask(Region)
    );
    *Tags = _mm256_blendv_epi8(
        *Tags,
        _mm256_set1_epi8(Kind),
        GetMask(IsDelim)
    );
    *CurrentVec = _mm256_blendv_epi8(
        *CurrentVec,
        CurrentSourceVec,
        GetMask(Region)
    );
}
