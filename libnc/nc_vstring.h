#if !defined(__NC_VSTRING_H__)
#define __NC_VSTRING_H__

#include "nc_types.h"
#include "nc_math.h"
#include "nc_string.h"

struct Arena;

// @defines____________________________________________________________________
#if NC_SIMD_LEVEL >= 8
    #define VCHUNK_WIDTH 64
    #define VCHUNK_SHIFT  6
#elif NC_SIMD_LEVEL >= 6
    #define VCHUNK_WIDTH 32
    #define VCHUNK_SHIFT  5
#elif NC_SIMD_LEVEL >= 2 || defined(NC_CPU_NEON)
    #define VCHUNK_WIDTH 16
    #define VCHUNK_SHIFT  4
#else
    #define VCHUNK_WIDTH 8
    #define VCHUNK_SHIFT 3
#endif

#define VCHUNK_COUNT(X)        (((X) + VCHUNK_WIDTH - 1) >> VCHUNK_SHIFT)
#define VSTR_PADDED_SIZE(Size) ((Size) + VCHUNK_WIDTH)
#define PRINT_VSTR(X)          (int) (X).Size, (X).Str

#define V_PATTERN_MAX        64
#define V_NEEDLE_SET_MAX      8
#define V_KEY_PREFIX_SIZE     8
#define V_PATTERN_MAX_ERRORS  8

typedef u8 VClassKind;
enum : u8 {
    V_CLASS_KIND_SPACE  = (1 << 0),
    V_CLASS_KIND_DIGIT  = (1 << 1),
    V_CLASS_KIND_ALPHA  = (1 << 2),
    V_CLASS_KIND_SLASH  = (1 << 3),
    V_CLASS_KIND_HEX    = (1 << 4),
    V_CLASS_KIND_IDENT  = (1 << 5),
    V_CLASS_KIND_QUOTE  = (1 << 6),
    V_CLASS_KIND_ESCAPE = (1 << 7)
};

typedef u32 VFuzzyKind;
enum : u32 {
    V_FUZZY_PREFIX_BONUS   = (1 << 0),
    V_FUZZY_BOUNDARY_BONUS = (1 << 1),
    V_FUZZY_CONTIGUOUS     = (1 << 2),
    V_FUZZY_ALL_CASES      = U32_MAX
};

// @types______________________________________________________________________
struct VStr8 {
    u8* Str;
    u64 Size;
};

struct VStr8Array {
    VStr8* Data;
    u64    Count;
};

struct VClassTable {
    u8 Lo[16];
    u8 Hi[16];
    u8 Combine[8];
};

struct VCursor {
    VStr8              String;
    VClassTable const* Table;
    VClassKind         Kind;
    u8                 Byte;
    u64                Chunk;
    u64                TokenOffset;
    u64                Mask;
};

struct VNeedle {
    u8 Memory[32 + 2 * VCHUNK_WIDTH];
};

struct VNeedleSet {
    u8 Memory[64 + V_NEEDLE_SET_MAX * 16 + 3 * 32];
};

struct VNeedleMatch {
    u64 Offset;
    u32 Needle;
};

struct VPattern {
    u8 Memory[2064];
};

struct VDigits {
    u64 Value;
    u32 Count;
    b32 HasOverflow;
};

struct VUTF8Info {
    b32 IsValid;
    b32 IsASCII;
    u64 CodePoints;
    u64 ErrorOffset;
};

// @runtime____________________________________________________________________
extern const VClassTable V_CLASS_TABLE_ASCII;
extern const VClassTable V_CLASS_TABLE_JSON;
extern const VClassTable V_CLASS_TABLE_PATH;

// @functions__________________________________________________________________
VStr8 VStr(Arena* MemPool, Str8 String);
VStr8 VStrUnsafe(Str8 String);
VStr8 VStrUnsafe(u8* CString, u64 Size);
Str8 Str(VStr8 String);
VStr8 VStrSub(VStr8 String, r1u64 Range);
VStr8 VStrSkip(VStr8 String, u64 Min);
VStr8 VStrChop(VStr8 String, u64 NMax);
VStr8 ArenaPushVStrCpy(Arena* MemPool, Str8 String);
VStr8 ArenaPushVStrCat(Arena* MemPool, Str8 StringA, Str8 StringB);
VStr8 ArenaPushVStrFmt(Arena* MemPool, char* Fmt, ...);
VStr8 ArenaPushVStrFmtV(Arena* MemPool, char const* Fmt, va_list Args);
VStr8Array VStrArrayFromList(Arena* MemPool, Str8List* List);
VStr8 ReadVFileContents(Arena* MemPool, Str8 FilePath);
VClassTable VClassTableAlloc(void);
b32 TablePush(VClassTable* Table, VClassKind Kind, u8 Char);
b32 TablePush(VClassTable* Table, VClassKind Kind, u8 Lo, u8 Hi);
b32 TablePush(VClassTable* Table, VClassKind Kind, Str8 Chars);
VCursor VCursorBegin(VStr8 String, u8 Byte);
VCursor VCursorBegin(VStr8 String, VClassTable const* Table, VClassKind Kind);
b32 VCursorNext(VCursor* Cursor, OUT u64* Offset);
b32 VCursorNextToken(VCursor* Cursor, OUT Str8* Token);
b32 VCursorNextToken(VCursor* Cursor, b32 KeepEmpties, OUT Str8* Token);
Str8List StrSplit(Arena* MemPool, VStr8 String, u8 const* Splits, u32 SplitsCount, b32 KeepEmpties);
Str8List StrSplitLines(Arena* MemPool, VStr8 String);
u64 VStrFindByte(VStr8 String, u8 Byte, u64 StartOffset);
u64 VStrFindLastByte(VStr8 String, u8 Byte);
u64 VStrFindClass(VStr8 String, VClassTable const* Table, VClassKind Kind, u64 StartOffset);
u64 VStrFindNotClass(VStr8 String, VClassTable const* Table, VClassKind Kind, u64 StartOffset);
u64 VStrFindLastClass(VStr8 String, VClassTable const* Table, VClassKind Kind, u64 StartOffset);
u64 VStrCountByte(VStr8 String, u8 Byte);
u64 VStrCountClass(VStr8 String, VClassTable const* Table, VClassKind Kind);
VNeedle VNeedleFromStr(Str8 Needle, StrMatchKind Kind);
u64 VStrFind(VStr8 String, VNeedle const* Needle, u64 StartOffset);
u64 StrFindSubStr(VStr8 String, Str8 SubString, u64 StartOffset, StrMatchKind Kind);
VNeedleSet VNeedleSetFromStrs(Str8 const* Needles, u32 Count, StrMatchKind Kind);
VNeedleMatch VNeedleSetFind(VStr8 String, VNeedleSet const* Set, u64 StartOffset);
u32 VNeedleSetMatch(VNeedleSet const* Set, Str8 String);
b32 StrMatch(VStr8 StringA, VStr8 StringB, StrMatchKind Kind);
b32 StrMatch(VStr8 StringA, Str8 StringB, StrMatchKind Kind);
i32 VStrCmp(VStr8 StringA, VStr8 StringB);
i32 VStrCmpFold(VStr8 StringA, VStr8 StringB);
u64 VStrMatchPrefix(VStr8 StringA, VStr8 StringB);
u64 VStrMatchSuffix(VStr8 StringA, VStr8 StringB);
b32 PathMatchNormalised(VStr8 PathA, VStr8 PathB);
u64 VStrFindInArray(VStr8Array Strings, Str8 String);
VPattern VPatternFromStr(Str8 Pattern, StrMatchKind Kind);
u64 VPatternFind(VStr8 String, VPattern const* Pattern, u64 StartOffset);
u64 VPatternFindApprox(VStr8 String, VPattern const* Pattern, u32 MaxErrors, u64 StartOffset);
u32 VEditDistance(VPattern const* Pattern, Str8 String);
u32 VEditDistance(Str8 StringA, Str8 StringB);
FMRangeList FuzzyFind(Arena* MemPool, Str8 Needle, VStr8 String);
i32 VFuzzyScore(Str8 Needle, Str8 Candidate, VFuzzyKind Kind);
void VFuzzyScore(Str8 Needle, Str8 const* Candidates, u64 Count, VFuzzyKind Kind, OUT i32* Scores);
u64 VStrFindEndOfStr(VStr8 String, u64 StartOffset);
u64 VStrSkipSpace(VStr8 String, u64 StartOffset);
b32 VStrNeedsEscape(VStr8 String);
VUTF8Info VUTF8Inspect(VStr8 String);
b32 VUTF8Validate(VStr8 String);
b32 VIsASCII(VStr8 String);
u64 VCodePointCount(VStr8 String);
u64 VUTF8SnapToLead(VStr8 String, u64 Offset);
u64 VUTF8NextLead(VStr8 String, u64 Offset);
u64 VUTF8PrevLead(VStr8 String, u64 Offset);
u64 VUTF8OffsetFromCodePoint(VStr8 String, u64 CodePointIndex);
Str16 Str16FromStr8(Arena* MemPool, VStr8 String);
Str32 Str32FromStr8(Arena* MemPool, VStr8 String);
VStr8 ToLower(Arena* MemPool, VStr8 String);
VStr8 ToUpper(Arena* MemPool, VStr8 String);
u64 Hash(VStr8 String);
u64 Hash(u64 Seed, VStr8 String);
u128 BigHash(VStr8 String);
u128 BigHash(u64 Seed, VStr8 String);
u32 VCRC32C(u32 Seed, VStr8 String);
VDigits VDigitsFromStr(VStr8 String, u32 Base);
u64 U64FromStr(VStr8 String, u32 Base = 10);
i64 I64FromStr(VStr8 String, u32 Base = 10);
f64 F64FromStr(VStr8 String);
b32 StrIsInteger(VStr8 String, u32 Base);
u32 VStrFromU64(u8* Dst, u64 Value);
u32 VStrFromI64(u8* Dst, i64 Value);
i32 VStrFromU64Hex(u8* Dst, u64 Value, b32 MakeUpper);
u64 VLineCount(VStr8 String);
TextPoint VTextPointFromOffset(VStr8 String, u64 Offset);
u64 VOffsetFromTextPoint(VStr8 String, TextPoint Point);
r1u64 VLineRange(VStr8 String, i64 Line);
VStr8 VStrLine(VStr8 String, i64 Line);
Str8TextPointPair Str8TextPointPairFromStr(VStr8 String);
VStr8 StrSkipChopWhitespace(VStr8 String);
VStr8 StrChopLastSlash(VStr8 String);
VStr8 StrSkipLastSlash(VStr8 String);
VStr8 StrChopLastDot(VStr8 String);
VStr8 StrSkipLastDot(VStr8 String);
VStr8 StrTrimLastSlash(VStr8 String);
VStr8 PathConvertSlashes(Arena* MemPool, VStr8 Path, PathStyle Style);
VStr8 RawFromEscapedStr8(Arena* MemPool, VStr8 String);
VStr8 EscapedFromRawStr8(Arena* MemPool, VStr8 String);
VStr8 VStrReplaceByte(Arena* MemPool, VStr8 String, u8 StartOffset, u8 EndOffset);
void VStrReplaceByte(VStr8 String, u8 StartOffset, u8 EndOffset);

#if defined(NC_LANG_CPP)
    INTERNAL b32 operator==(VStr8 StringA, VStr8 StringB);
    INTERNAL b32 operator!=(VStr8 StringA, VStr8 StringB);
    INTERNAL b32 operator<(VStr8 StringA, VStr8 StringB);
#endif

#endif // __NC_VSTRING_H__
