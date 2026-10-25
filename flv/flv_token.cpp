#include "nc_defines.h"
#include "nc_types.h"
#include "nc_arena.h"
#include "flv_token.h"

TokenArray 
ArrayAlloc(Arena* MemPool, u64 Count)
{
    TokenArray Result = {};

    Result.TokensCapacity = Count;
    Result.Tokens = ArenaPushArrayZeroEx(
        MemPool, 
        Token, 
        Result.TokensCapacity, 
        NC_SIMD_ALIGN256
    );

    return Result;
}

u32
ArrayPush(
    TokenArray* Array, 
    __m256i Types, 
    __m256i Locs, 
    u32 Count, 
    u32 ArrOff
) {
     u8 ALIGNAS(NC_SIMD_ALIGN256) Kinds[NC_SIMD_ALIGN256];
     u32 ALIGNAS(NC_SIMD_ALIGN256) Offsets[NC_SIMD_ALIGN256];

    _mm256_store_si256((__m256i*) Kinds, Types);

    const __m256i ArrOffV = _mm256_set1_epi32(ArrOff);
    const u64* Locations = (const u64*) &Locs;

    for (u8 Index = 0; Index < 4; ++Index) {
        __m128i LocationSliceU8 = _mm_set_epi64x(0, Locations[Index]);
        __m256i LocationSliceU32 = _mm256_cvtepu8_epi32(LocationSliceU8);

        LocationSliceU32 = _mm256_add_epi32(LocationSliceU32, ArrOffV);
        _mm256_store_si256((__m256i*) (Offsets + Index * 8), LocationSliceU32);
    }

    Token* Destination = Array->Tokens + Array->TokensCount;
    u32 LastTokenIndex = Array->TokensCount + Count - 1;

    for (u32 Index = 0; Index < Count - 1; ++Index) {
        Destination[Index].Kind = (TokenKind) Kinds[Index];
        Destination[Index].Offset = Offsets[Index];
        Destination[Index].Count = (u16) (Offsets[Index + 1] - Offsets[Index]);
    }

    Destination[Count - 1].Kind = (TokenKind) Kinds[Count - 1];
    Destination[Count - 1].Offset = Offsets[Count - 1];
    Destination[Count - 1].Count = 0;     // NOTE(nathan): will be filled by lexer
    Array->TokensCount += Count;

    return LastTokenIndex;
}

Str8
TokStr(Arena* MemPool, Token* Tok, Str8 Source)
{
    Str8 Result = {};

    switch (Tok->Kind) {
        default: {
            Result = ArenaPushStrFmt(MemPool, (char*) "UNKNOWN TOK [%u]", Tok->Kind);
        } break;

        case TOKEN_KIND_IDENT: {
            Str8 IdentName = Str(Source.Str + Tok->Offset, Tok->Count);

            Result = ArenaPushStrFmt(MemPool, (char*) "identifer<%.*s>", PRINT_STR(IdentName));
        } break;

        case TOKEN_KIND_NUMBER: {
            Str8 RawNumber = Str(Source.Str + Tok->Offset, Tok->Count);

            Result = ArenaPushStrFmt(MemPool, (char*) "number<%.*s>", PRINT_STR(RawNumber));
        } break;

        case TOKEN_KIND_CONST: {
            Result = "const"_s8;
        } break;

        case TOKEN_KIND_TYPE: {
            Result = "type"_s8;
        } break;

        case TOKEN_KIND_FOR: {
            Result = "for"_s8;
        } break;

        case TOKEN_KIND_ELSE: {
            Result = "else"_s8;
        } break;

        case TOKEN_KIND_IF: {
            Result = "if"_s8;
        } break;

        case TOKEN_KIND_CONTINUE: {
            Result = "continue"_s8;
        } break;

        case TOKEN_KIND_WHILE: {
            Result = "while"_s8;
        } break;

        case TOKEN_KIND_RETURN: {
            Result = "return"_s8;
        } break;

        case TOKEN_KIND_EXCLAIM: {
            Result = "!"_s8;
        } break;

        case TOKEN_KIND_OPEN_PAREN: {
            Result = "("_s8;
        } break;

        case TOKEN_KIND_ClOSE_PAREN: {
            Result = ")"_s8;
        } break;

        case TOKEN_KIND_STAR: {
            Result = "*"_s8;
        } break;

        case TOKEN_KIND_PLUS: {
            Result = "+"_s8;
        } break;

        case TOKEN_KIND_MINUS: {
            Result = "-"_s8;
        } break;

        case TOKEN_KIND_SLASH: {
            Result = "/"_s8;
        } break;

        case TOKEN_KIND_COLON: {
            Result = ":"_s8;
        } break;

        case TOKEN_KIND_SEMICOLON: {
            Result = ";"_s8;
        } break;

        case TOKEN_KIND_LESS: {
            Result = "<"_s8;
        } break;

        case TOKEN_KIND_EQUAL: {
            Result = "="_s8;
        } break;

        case TOKEN_KIND_GREATER: {
            Result = ">"_s8;
        } break;

        case TOKEN_KIND_OPEN_SQ_BRACK: {
            Result = "["_s8;
        } break;

        case TOKEN_KIND_CLOSE_SQ_BRACK: {
            Result = "]"_s8;
        } break;

        case TOKEN_KIND_BIT_OR: {
            Result = "|"_s8;
        } break;

        case TOKEN_KIND_BIT_AND: {
            Result = "&"_s8;
        } break;

        case TOKEN_KIND_OPEN_BRACK: {
            Result = "{"_s8;
        } break;

        case TOKEN_KIND_CLOSE_BRACK: {
            Result = "}"_s8;
        } break;

        case TOKEN_KIND_TILDE: {
            Result = "~"_s8;
        } break;

        case TOKEN_KIND_RANGE: {
            Result = ".."_s8;
        } break;

        case TOKEN_KIND_EXCLAIM_EQ: {
            Result = "!="_s8;
        } break;

        case TOKEN_KIND_GREATER_EQ: {
            Result = ">="_s8;
        } break;

        case TOKEN_KIND_LESS_EQ: {
            Result = "<="_s8;
        } break;

        case TOKEN_KIND_LOG_OR: {
            Result = "||"_s8;
        } break;

        case TOKEN_KIND_LOG_AND: {
            Result = "&&"_s8;
        } break;

        case TOKEN_KIND_R_SHIFT: {
            Result = ">>"_s8;
        } break;

        case TOKEN_KIND_L_SHIFT: {
            Result = "<<"_s8;
        } break;

        case TOKEN_KIND_INCR: {
            Result = "++"_s8;
        } break;

        case TOKEN_KIND_DECR: {
            Result = "--"_s8;
        } break;

        case TOKEN_KIND_ARROW: {
            Result = "->"_s8;
        } break;

        case TOKEN_KIND_PLUS_EQ: {
            Result = "+="_s8;
        } break;

        case TOKEN_KIND_MINUS_EQ: {
            Result = "-="_s8;
        } break;

        case TOKEN_KIND_TIMES_EQ: {
            Result = "*="_s8;
        } break;

        case TOKEN_KIND_DIV_EQ: {
            Result = "/="_s8;
        } break;

        case TOKEN_KIND_CARET_EQ: {
            Result = "^="_s8;
        } break;

        case TOKEN_KIND_BIT_OR_EQ: {
            Result = "|="_s8;
        } break;

        case TOKEN_KIND_BIT_AND_EQ: {
            Result = "&="_s8;
        } break;

        case TOKEN_KIND_PERCENT_EQ: {
            Result = "%%="_s8;
        } break;

        case TOKEN_KIND_EOF: {
            Result = "EOF"_s8;
        } break;

        case TOKEN_KIND_PROG: {
            Result = "PROG"_s8;
        } break;

        case TOKEN_KIND_PERIOD: {
            Result = "."_s8;
        } break;

        case TOKEN_KIND_CARET: {
            Result = "^"_s8;
        } break;

        case TOKEN_KIND_PERCENT: {
            Result = "%%"_s8;
        } break;

        case TOKEN_KIND_EQ_EQ: {
            Result = "=="_s8;
        } break;

        case TOKEN_KIND_COMMA: {
            Result = ","_s8;
        } break;

        case TOKEN_KIND_STRUCT: {
            Result = "struct"_s8;
        } break;

        case TOKEN_KIND_UNION: {
            Result = "union"_s8;
        } break;

        case TOKEN_KIND_ENUM: {
            Result = "enum"_s8;
        } break;

        case TOKEN_KIND_BREAK: {
            Result = "break"_s8;
        } break;

        case TOKEN_KIND_CASE: {
            Result = "case"_s8;
        } break;

        case TOKEN_KIND_SWITCH: {
            Result = "switch"_s8;
        } break;

        case TOKEN_KIND_DEFAULT: {
            Result = "default"_s8;
        } break;

        case TOKEN_KIND_SIZEOF: {
            Result = "sizeof"_s8;
        } break;

        case TOKEN_KIND_CAST: {
            Result = "cast"_s8;
        } break;

        case TOKEN_KIND_LESS_LESS_EQ: {
            Result = "<<="_s8;
        } break;

        case TOKEN_KIND_GREATER_GREATER_EQ: {
            Result = ">>="_s8;
        } break;
    }

    return Result;
}

Str8
TokStr(u8 Kind)
{
    Str8 Result = {};

    switch (Kind) {
        default: {
            Result = "UNKNOWN TOK"_s8;
        } break;

        case TOKEN_KIND_CONST: {
            Result = "const"_s8;
        } break;

        case TOKEN_KIND_TYPE: {
            Result = "type"_s8;
        } break;

        case TOKEN_KIND_FOR: {
            Result = "for"_s8;
        } break;

        case TOKEN_KIND_ELSE: {
            Result = "else"_s8;
        } break;

        case TOKEN_KIND_IF: {
            Result = "if"_s8;
        } break;

        case TOKEN_KIND_CONTINUE: {
            Result = "continue"_s8;
        } break;

        case TOKEN_KIND_WHILE: {
            Result = "while"_s8;
        } break;

        case TOKEN_KIND_RETURN: {
            Result = "return"_s8;
        } break;

        case TOKEN_KIND_EXCLAIM: {
            Result = "!"_s8;
        } break;

        case TOKEN_KIND_OPEN_PAREN: {
            Result = "("_s8;
        } break;

        case TOKEN_KIND_ClOSE_PAREN: {
            Result = ")"_s8;
        } break;

        case TOKEN_KIND_STAR: {
            Result = "*"_s8;
        } break;

        case TOKEN_KIND_PLUS: {
            Result = "+"_s8;
        } break;

        case TOKEN_KIND_MINUS: {
            Result = "-"_s8;
        } break;

        case TOKEN_KIND_SLASH: {
            Result = "/"_s8;
        } break;

        case TOKEN_KIND_COLON: {
            Result = ":"_s8;
        } break;

        case TOKEN_KIND_SEMICOLON: {
            Result = ";"_s8;
        } break;

        case TOKEN_KIND_LESS: {
            Result = "<"_s8;
        } break;

        case TOKEN_KIND_EQUAL: {
            Result = "="_s8;
        } break;

        case TOKEN_KIND_GREATER: {
            Result = ">"_s8;
        } break;

        case TOKEN_KIND_OPEN_SQ_BRACK: {
            Result = "["_s8;
        } break;

        case TOKEN_KIND_CLOSE_SQ_BRACK: {
            Result = "]"_s8;
        } break;

        case TOKEN_KIND_BIT_OR: {
            Result = "|"_s8;
        } break;

        case TOKEN_KIND_BIT_AND: {
            Result = "&"_s8;
        } break;

        case TOKEN_KIND_OPEN_BRACK: {
            Result = "{"_s8;
        } break;

        case TOKEN_KIND_CLOSE_BRACK: {
            Result = "}"_s8;
        } break;

        case TOKEN_KIND_TILDE: {
            Result = "~"_s8;
        } break;

        case TOKEN_KIND_RANGE: {
            Result = ".."_s8;
        } break;

        case TOKEN_KIND_EXCLAIM_EQ: {
            Result = "!="_s8;
        } break;

        case TOKEN_KIND_GREATER_EQ: {
            Result = ">="_s8;
        } break;

        case TOKEN_KIND_LESS_EQ: {
            Result = "<="_s8;
        } break;

        case TOKEN_KIND_LOG_OR: {
            Result = "||"_s8;
        } break;

        case TOKEN_KIND_LOG_AND: {
            Result = "&&"_s8;
        } break;

        case TOKEN_KIND_R_SHIFT: {
            Result = ">>"_s8;
        } break;

        case TOKEN_KIND_L_SHIFT: {
            Result = "<<"_s8;
        } break;

        case TOKEN_KIND_INCR: {
            Result = "++"_s8;
        } break;

        case TOKEN_KIND_DECR: {
            Result = "--"_s8;
        } break;

        case TOKEN_KIND_ARROW: {
            Result = "->"_s8;
        } break;

        case TOKEN_KIND_PLUS_EQ: {
            Result = "+="_s8;
        } break;

        case TOKEN_KIND_MINUS_EQ: {
            Result = "-="_s8;
        } break;

        case TOKEN_KIND_TIMES_EQ: {
            Result = "*="_s8;
        } break;

        case TOKEN_KIND_DIV_EQ: {
            Result = "/="_s8;
        } break;

        case TOKEN_KIND_CARET_EQ: {
            Result = "^="_s8;
        } break;

        case TOKEN_KIND_BIT_OR_EQ: {
            Result = "|="_s8;
        } break;

        case TOKEN_KIND_BIT_AND_EQ: {
            Result = "&="_s8;
        } break;

        case TOKEN_KIND_PERCENT_EQ: {
            Result = "%%="_s8;
        } break;

        case TOKEN_KIND_EOF: {
            Result = "EOF"_s8;
        } break;

        case TOKEN_KIND_PROG: {
            Result = "PROG"_s8;
        } break;

        case TOKEN_KIND_PERIOD: {
            Result = "."_s8;
        } break;

        case TOKEN_KIND_CARET: {
            Result = "^"_s8;
        } break;

        case TOKEN_KIND_PERCENT: {
            Result = "%%"_s8;
        } break;

        case TOKEN_KIND_EQ_EQ: {
            Result = "=="_s8;
        } break;

        case TOKEN_KIND_COMMA: {
            Result = ","_s8;
        } break;

        case TOKEN_KIND_STRUCT: {
            Result = "struct"_s8;
        } break;

        case TOKEN_KIND_UNION: {
            Result = "union"_s8;
        } break;

        case TOKEN_KIND_ENUM: {
            Result = "enum"_s8;
        } break;

        case TOKEN_KIND_BREAK: {
            Result = "break"_s8;
        } break;

        case TOKEN_KIND_CASE: {
            Result = "case"_s8;
        } break;

        case TOKEN_KIND_SWITCH: {
            Result = "switch"_s8;
        } break;

        case TOKEN_KIND_DEFAULT: {
            Result = "default"_s8;
        } break;

        case TOKEN_KIND_SIZEOF: {
            Result = "sizeof"_s8;
        } break;

        case TOKEN_KIND_CAST: {
            Result = "cast"_s8;
        } break;

        case TOKEN_KIND_LESS_LESS_EQ: {
            Result = "<<="_s8;
        } break;

        case TOKEN_KIND_GREATER_GREATER_EQ: {
            Result = ">>="_s8;
        } break;
    }

    return Result;
}
