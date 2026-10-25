#if !defined(__FLV_TOKEN_H__)
#define __FLV_TOKEN_H__

#include "nc_defines.h"
#include "nc_types.h"
#include "nc_string.h"
#include "flv_base_types.h"

struct Arena;

// @defines____________________________________________________________________
typedef u32 TokenIndex;

typedef u8 TokenKind;
enum : u8 {
    TOKEN_KIND_EXCLAIM            = '!',
    TOKEN_KIND_OPEN_PAREN         = '(',
    TOKEN_KIND_ClOSE_PAREN        = ')',
    TOKEN_KIND_STAR               = '*',
    TOKEN_KIND_PLUS               = '+',
    TOKEN_KIND_MINUS              = '-',
    TOKEN_KIND_SLASH              = '/',
    TOKEN_KIND_COLON              = ':',
    TOKEN_KIND_SEMICOLON          = ';',
    TOKEN_KIND_LESS               = '<',
    TOKEN_KIND_EQUAL              = '=',
    TOKEN_KIND_GREATER            = '>',
    TOKEN_KIND_OPEN_SQ_BRACK      = '[',
    TOKEN_KIND_CLOSE_SQ_BRACK     = ']',
    TOKEN_KIND_BIT_OR             = '|',
    TOKEN_KIND_BIT_AND            = '&',
    TOKEN_KIND_TILDE              = '~',
    TOKEN_KIND_OPEN_BRACK         = '{',
    TOKEN_KIND_CLOSE_BRACK        = '}',
    TOKEN_KIND_PERIOD             = '.',
    TOKEN_KIND_CARET              = '^',
    TOKEN_KIND_PERCENT            = '%',
    TOKEN_KIND_COMMA              = ',',
    TOKEN_KIND_QUESTION           = '?',
    TOKEN_KIND_DOLLAR_SIGN        = '$',

    TOKEN_KIND_RANGE              = '.' + '.' - 2,
    TOKEN_KIND_EXCLAIM_EQ         = '!' + '=' - 2,
    TOKEN_KIND_GREATER_EQ         = '>' + '=' - 2,
    TOKEN_KIND_LESS_EQ            = '<' + '=' - 2,
    TOKEN_KIND_LOG_OR             = '|' + '|' - 2,
    TOKEN_KIND_LOG_AND            = '&' + '&' - 2,
    TOKEN_KIND_R_SHIFT            = '>' + '>' - 2,
    TOKEN_KIND_L_SHIFT            = '<' + '<' - 2,
    TOKEN_KIND_INCR               = '+' + '+' - 2,
    TOKEN_KIND_DECR               = '-' + '-' - 2,
    TOKEN_KIND_ARROW              = '-' + '>' - 2,
    TOKEN_KIND_PLUS_EQ            = '+' + '=' - 2,
    TOKEN_KIND_MINUS_EQ           = '-' + '=' - 2,
    TOKEN_KIND_TIMES_EQ           = '*' + '=' - 2,
    TOKEN_KIND_DIV_EQ             = '/' + '=' - 2,
    TOKEN_KIND_CARET_EQ           = '^' + '=' - 2,
    TOKEN_KIND_BIT_OR_EQ          = '|' + '=' - 2,
    TOKEN_KIND_BIT_AND_EQ         = '&' + '=' - 2,
    TOKEN_KIND_PERCENT_EQ         = '%' + '=' - 2,
    TOKEN_KIND_EQ_EQ              = '=' + '=' - 2,

    TOKEN_KIND_LESS_LESS_EQ       = 181,
    TOKEN_KIND_GREATER_GREATER_EQ = 185,

    TOKEN_KIND_VAR                = 3,
    TOKEN_KIND_GRAPH              = 4,
    TOKEN_KIND_TABLE              = 5,
    TOKEN_KIND_TEXT               = 6,
    TOKEN_KIND_FLAG               = 7,
    TOKEN_KIND_IF                 = 8,
    TOKEN_KIND_ELSE               = 9,
    TOKEN_KIND_IMPORT             = 10,

    TOKEN_KIND_CHAR_LIT           = 202,
    TOKEN_KIND_STR_LIT            = 203,

    TOKEN_KIND_IDENT              = 1,
    TOKEN_KIND_NUMBER             = 2,

    TOKEN_KIND_EOF                = 0,
    TOKEN_KIND_PROG               = U8_MAX
};

// @types______________________________________________________________________
struct Token {
    TokenKind Kind;
    u16       Count;
    u32       Offset;
};
STATIC_ASSERT(sizeof(Token) <= sizeof(u64), TokenSizeCheck);

struct TokenArray {
    Token*     Tokens;
    NameIndex* Names;
    u32        TokensCount;
    u32        TokensCapacity;
};
STATIC_ASSERT(sizeof(TokenArray) <= sizeof(u256), TokenArraySizeCheck);

struct ParsedFile {
    Str8       Path;
    Str8       Source;
    TokenArray Tokens;
    u32*       LineOffsets;
    u32        LineCount;
    b32        IsOpen;
};

struct SourceLoc {
    Str8 Path;
    u32  Line;
    u32  Column;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
TokenArray ArrayAlloc(Arena* MemPool, u64 Count);
u32 ArrayPush(TokenArray* Array, __m256i Types, __m256i Locs, u32 Count, u32 ArrOff);
Str8 TokStr(Arena* MemPool, Token* Tok, Str8 Source);
Str8 TokStr(u8 Kind);

#endif // __FLV_TOKEN_H__
