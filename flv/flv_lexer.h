#if !defined(__FLV_LEXER_H__)
#define __FLV_LEXER_H__

#include "nc_types.h"
#include "nc_string.h"
#include "flv_token.h"
#include "flv_intern.h"
#include "nc_simd.h"

struct LexerState {
    TokenKind* Kinds;
    u32*       Offsets;
    u32*       Ends;
    u64        Count;
    u64        EndsCount;
};

// NOTE(nathan): AVX extensions
void _mm256_pext(v256* Vec, v256 Mask, u64* Count);
v256 _mm256_cmpistrm_any(v128 Match, v256 Vec);
v256 _mm256_cmpistrm_rng(v128 Ranges, u64 RangesCount, v256 Vec);

void UpdateState(LexerState* State, v256 Kinds, v256 Locations, u64 Count, u32 StartIndex);
TokenArray LexString(Arena* MemPool, Str8 String, NamePool* Names);
// u8 Hash(u64 Value);
// void BuildKeywordLUT(u16* Table);
v256 ClassifySingleCharacter(v256 Input);
// void FindKeywords(LexerState* State, u16 const* Table, Str8 Source);
v256 NonZeroMask(const v256 Vec);
void FindTokenIndices(v256* TokenTags, v256* TokenIndex, u64* Count);
void ReplaceWhitespace(v256* Vec);
void ReplaceTokenBody(v256* Vec);
b32 IsEmpty(v256 Vec);
v256 RunSubLexers(v256* CurrentVec, v256* NextVec, const v256 CurrentSourceVec, u8 LastChar, b32* ChContinue, b32* EscContinue, b32* StrContinue, b32* LnCommContinue, b32* BlockCommContinue);
v256 NumericPeriodsMask(v256 CurrentVec, v256 NextVec, u8 LastChar);
void LexSingleCharSymbol(v256 * CurrentVec, v256 NextVec, v256* Tags, u8 LastChar);
v256 PeekNextSingle(v256 CurrentVec, v256 NextVec);
v256 PeekNextDouble(v256 CurrentVec, v256 NextVec);
v256 GetMask(const u32 Mask);
void RemovePrefix(v256* Vec, u64 Prefix);
void LexDoubleCharSymbol(v256* CurrentVec, v256* NextVec, v256* Tags);
void LexTripleCharSymbol(v256* CurrentVec, v256* NextVec, v256* Tags);
v256 AlphaMask(v256 Vec);
void LexIdentifiers(v256 CurrentVec, v256* Tags, b32 LastEmpty);
v256 NumericMask(v256 Vec);
void LexNumericConstants(const v256 CurrentVec, v256* Tags, const b32 LastEmpty);
void LexTextLiteral(v256* CurrentVec, v256* Tags, const u8 Delim, b32* ShouldContinue, const TokenKind Kind, const v256 CurrentSourceVec, b32* EscContinue);
void LexInlineComments(v256* CurrentVec, v256 NextVec, b32* LnCommContinue);
u32* LineOffsetsFromSource(Arena* MemPool, Str8 Source, OUT u32* Count);
u32 LineFromOffset(u32 const* LineOffsets, u32 LineCount, u32 Offset);

#endif // __FLV_LEXER_H__
