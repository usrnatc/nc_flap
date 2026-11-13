#if !defined(__NC_EVAL_H__)
#define __NC_EVAL_H__

#include "nc_types.h"
#include "nc_math.h"
#include "nc_string.h"
#include "nc_memory.h"

// @defines____________________________________________________________________
enum EvalMsgKind {
    EVAL_MSG_KIND_NULL,
    EVAL_MSG_KIND_MALFORMED_INPUT,
    EVAL_MSG_KIND_MISSING_INFO,
    EVAL_MSG_KIND_RESOLUTION_FAILURE,
    EVAL_MSG_KIND_INTERPRETATION_ERROR,
    EVAL_MSG_KIND_COUNT
};

typedef u8 EvalEvalOp;
enum : u8 {
    EVAL_EVAL_OP_MEM_READ          = 0,
    EVAL_EVAL_OP_CONST_U8          = 1,
    EVAL_EVAL_OP_CONST_U16         = 2,
    EVAL_EVAL_OP_CONST_U32         = 3,
    EVAL_EVAL_OP_CONST_U64         = 4,
    EVAL_EVAL_OP_CONST_U128        = 5,
    EVAL_EVAL_OP_CONST_STR         = 6,
    EVAL_EVAL_OP_ABS               = 7,
    EVAL_EVAL_OP_NEG               = 8,
    EVAL_EVAL_OP_ADD               = 9,
    EVAL_EVAL_OP_SUB               = 10,
    EVAL_EVAL_OP_MUL               = 11,
    EVAL_EVAL_OP_DIV               = 12,
    EVAL_EVAL_OP_MOD               = 13,
    EVAL_EVAL_OP_LEFT_SHIFT        = 14,
    EVAL_EVAL_OP_RIGHT_SHIFT       = 15,
    EVAL_EVAL_OP_BIT_AND           = 16,
    EVAL_EVAL_OP_BIT_OR            = 17,
    EVAL_EVAL_OP_BIT_XOR           = 18,
    EVAL_EVAL_OP_BIT_NOT           = 19,
    EVAL_EVAL_OP_LOG_AND           = 20,
    EVAL_EVAL_OP_LOG_OR            = 21,
    EVAL_EVAL_OP_LOG_NOT           = 22,
    EVAL_EVAL_OP_EQ_EQ             = 23,
    EVAL_EVAL_OP_NOT_EQ            = 24,
    EVAL_EVAL_OP_LESS_EQ           = 25,
    EVAL_EVAL_OP_GREATER_EQ        = 26,
    EVAL_EVAL_OP_LESS              = 27,
    EVAL_EVAL_OP_GREATER           = 28,
    EVAL_EVAL_OP_TRUNC             = 29,
    EVAL_EVAL_OP_TRUNC_SIGNED      = 30,
    EVAL_EVAL_OP_CONVERT           = 31,
    EVAL_EVAL_OP_PICK              = 32,
    EVAL_EVAL_OP_POP               = 33,
    EVAL_EVAL_OP_INSERT            = 34,
    EVAL_EVAL_OP_VALUE_READ        = 35,
    EVAL_EVAL_OP_BYTE_SWAP         = 36,
    EVAL_EVAL_OP_PARTIAL_VALUE     = 37,
    EVAL_EVAL_OP_PARTIAL_VALUE_BIT = 38,
    EVAL_EVAL_OP_SWAP              = 39,
    EVAL_EVAL_OP_COUNT
};

typedef u8 EvalEvalTypeGroup;
enum : u8 {
    EVAL_EVAL_TYPE_GROUP_OTHER,
    EVAL_EVAL_TYPE_GROUP_UNSIGNED,
    EVAL_EVAL_TYPE_GROUP_SIGNED,
    EVAL_EVAL_TYPE_GROUP_F32,
    EVAL_EVAL_TYPE_GROUP_F64,
    EVAL_EVAL_TYPE_GROUP_F128,
    EVAL_EVAL_TYPE_GROUP_COUNT
};

typedef u8 EvalEvalConversionKind;
enum : u8 {
    EVAL_EVAL_CONV_KIND_NOOP,
    EVAL_EVAL_CONV_KIND_LEGAL,
    EVAL_EVAL_CONV_KIND_OTHER_TO_OTHER,
    EVAL_EVAL_CONV_KIND_TO_OTHER,
    EVAL_EVAL_CONV_KIND_FROM_OTHER,
    EVAL_EVAL_CONV_KIND_COUNT
};

enum {
    EVAL_IR_EXT_KIND_START = EVAL_EVAL_OP_COUNT,
    EVAL_IR_EXT_KIND_SET_SPACE,
    EVAL_IR_EXT_KIND_BYTECODE,
    EVAL_IR_EXT_KIND_COUNT
};

enum EvalOpKind {
    EVAL_OP_KIND_NULL,
    EVAL_OP_KIND_UNARY_PREFIX,
    EVAL_OP_KIND_BINARY
};

typedef u64 EvalSpaceKind;
enum : u64 {
    EVAL_SPACE_KIND_NULL,
    EVAL_SPACE_KIND_FILE,
    EVAL_SPACE_KIND_FILESYSTEM,
    EVAL_SPACE_KIND_HASH_STORE_KEY,
    EVAL_SPACE_KIND_FIRST_USER_DEFINED
};

enum EvalTokenKind {
    EVAL_TOKEN_KIND_NULL,
    EVAL_TOKEN_KIND_IDENT,
    EVAL_TOKEN_KIND_NUMERIC,
    EVAL_TOKEN_KIND_STR_LIT,
    EVAL_TOKEN_KIND_CHAR_LIT,
    EVAL_TOKEN_KIND_SYMBOL,
    EVAL_TOKEN_KIND_COMMENT,
    EVAL_TOKEN_KIND_COUNT
};

enum EvalTypeKind {
    EVAL_TYPE_KIND_NULL,
    EVAL_TYPE_KIND_CHAR8,
    EVAL_TYPE_KIND_CHAR16,
    EVAL_TYPE_KIND_CHAR32,
    EVAL_TYPE_KIND_UCHAR8,
    EVAL_TYPE_KIND_UCHAR16,
    EVAL_TYPE_KIND_UCHAR32,
    EVAL_TYPE_KIND_U8,
    EVAL_TYPE_KIND_U16,
    EVAL_TYPE_KIND_U32,
    EVAL_TYPE_KIND_U64,
    EVAL_TYPE_KIND_U128,
    EVAL_TYPE_KIND_U256,
    EVAL_TYPE_KIND_U512,
    EVAL_TYPE_KIND_I8,
    EVAL_TYPE_KIND_I16,
    EVAL_TYPE_KIND_I32,
    EVAL_TYPE_KIND_I64,
    EVAL_TYPE_KIND_I128,
    EVAL_TYPE_KIND_I256,
    EVAL_TYPE_KIND_I512,
    EVAL_TYPE_KIND_BOOL,
    EVAL_TYPE_KIND_F16,
    EVAL_TYPE_KIND_F32,
    EVAL_TYPE_KIND_F64,
    EVAL_TYPE_KIND_PTR,
    EVAL_TYPE_KIND_ARRAY,
    EVAL_TYPE_KIND_ENUM,
    EVAL_TYPE_KIND_STRUCT,
    EVAL_TYPE_KIND_INCOMPLETE_ENUM,
    EVAL_TYPE_KIND_INCOMPLETE_STRUCT,
    EVAL_TYPE_KIND_SET,
    EVAL_TYPE_KIND_LENS,
    EVAL_TYPE_KIND_LENS_SPEC,
    EVAL_TYPE_KIND_META_EXPR,
    EVAL_TYPE_KIND_META_DISPLAY_NAME,
    EVAL_TYPE_KIND_META_DESCRIPTION,
    EVAL_TYPE_KIND_COUNT,
    EVAL_TYPE_KIND_FIRST_BASIC      = EVAL_TYPE_KIND_CHAR8,
    EVAL_TYPE_KIND_LAST_BASIC       = EVAL_TYPE_KIND_F64,
    EVAL_TYPE_KIND_FIRST_INTEGER    = EVAL_TYPE_KIND_CHAR8,
    EVAL_TYPE_KIND_LAST_INTEGER     = EVAL_TYPE_KIND_I512,
    EVAL_TYPE_KIND_FIRST_SIGNED0    = EVAL_TYPE_KIND_CHAR8,
    EVAL_TYPE_KIND_LAST_SIGNED0     = EVAL_TYPE_KIND_CHAR32,
    EVAL_TYPE_KIND_FIRST_SIGNED1    = EVAL_TYPE_KIND_I8,
    EVAL_TYPE_KIND_LAST_SIGNED1     = EVAL_TYPE_KIND_I512,
    EVAL_TYPE_KIND_FIRST_INCOMPLETE = EVAL_TYPE_KIND_INCOMPLETE_ENUM,
    EVAL_TYPE_KIND_LAST_INCOMPLETE  = EVAL_TYPE_KIND_INCOMPLETE_STRUCT,
    EVAL_TYPE_KIND_FIRST_META       = EVAL_TYPE_KIND_META_EXPR,
    EVAL_TYPE_KIND_LAST_META        = EVAL_TYPE_KIND_META_DESCRIPTION
};

typedef u32 EvalExprKind;
enum : u32 {
    EVAL_EXPR_KIND_NULL,
    EVAL_EXPR_KIND_REF,
    EVAL_EXPR_KIND_ARRAY_INDEX,
    EVAL_EXPR_KIND_MEMBER_ACCESS,
    EVAL_EXPR_KIND_CAST,
    EVAL_EXPR_KIND_POS,
    EVAL_EXPR_KIND_NEG,
    EVAL_EXPR_KIND_LOG_NOT,
    EVAL_EXPR_KIND_BIT_NOT,
    EVAL_EXPR_KIND_MUL,
    EVAL_EXPR_KIND_DIV,
    EVAL_EXPR_KIND_MOD,
    EVAL_EXPR_KIND_ADD,
    EVAL_EXPR_KIND_SUB,
    EVAL_EXPR_KIND_L_SHIFT,
    EVAL_EXPR_KIND_R_SHIFT,
    EVAL_EXPR_KIND_LESS,
    EVAL_EXPR_KIND_LESS_EQ,
    EVAL_EXPR_KIND_GREATER,
    EVAL_EXPR_KIND_GREATER_EQ,
    EVAL_EXPR_KIND_EQ_EQ,
    EVAL_EXPR_KIND_NOT_EQ,
    EVAL_EXPR_KIND_BIT_AND,
    EVAL_EXPR_KIND_BIT_XOR,
    EVAL_EXPR_KIND_BIT_OR,
    EVAL_EXPR_KIND_LOG_AND,
    EVAL_EXPR_KIND_LOG_OR,
    EVAL_EXPR_KIND_TERN,
    EVAL_EXPR_KIND_CALL,
    EVAL_EXPR_KIND_LEAF_BYTECODE,
    EVAL_EXPR_KIND_LEAF_STR_LIT,
    EVAL_EXPR_KIND_LEAF_U64,
    EVAL_EXPR_KIND_LEAF_F64,
    EVAL_EXPR_KIND_LEAF_F32,
    EVAL_EXPR_KIND_LEAF_IDENT,
    EVAL_EXPR_KIND_LEAF_OFFSET,
    EVAL_EXPR_KIND_LEAF_VALUE,
    EVAL_EXPR_KIND_LEAF_FILE_PATH,
    EVAL_EXPR_KIND_TYPE_IDENT,
    EVAL_EXPR_KIND_TYPE_PTR,
    EVAL_EXPR_KIND_ARRAY,
    EVAL_EXPR_KIND_COUNT
};

enum EvalInterpCode {
    EVAL_INTERP_CODE_OKAY,
    EVAL_INTERP_CODE_DIV_BY_ZERO,
    EVAL_INTERP_CODE_BAD_OP,
    EVAL_INTERP_CODE_BAD_OP_TYPES,
    EVAL_INTERP_CODE_INSUFFICIENT_STACK_SPACE,
    EVAL_INTERP_CODE_COUNT
};

enum EvalMode {
    EVAL_MODE_NULL,
    EVAL_MODE_VALUE,
    EVAL_MODE_OFFSET
};

typedef u32 EvalTypeFlag;
enum : u32 {
    EVAL_TYPE_FLAG_IS_PLAIN_TEXT              = (1 << 0),
    EVAL_TYPE_FLAG_IS_PATH_TEXT               = (1 << 1),
    EVAL_TYPE_FLAG_IS_NOT_TEXT                = (1 << 2),
    EVAL_TYPE_FLAG_IS_NOT_EDITABLE            = (1 << 3),
    EVAL_TYPE_FLAG_EDITABLE_CHILDREN          = (1 << 4),
    EVAL_TYPE_FLAG_INHERITED_BY_MEMBERS       = (1 << 5),
    EVAL_TYPE_FLAG_INHERITED_BY_ELEMENTS      = (1 << 6),
    EVAL_TYPE_FLAG_ARRAY_LIKE_EXPANSION       = (1 << 7),
    EVAL_TYPE_FLAG_STUB_SINGLE_LINE_EXPANSION = (1 << 8)
};

typedef u32 EvalCacheBundleFlag;
enum : u32 {
    EVAL_CACHE_BUNDLE_FLAG_PARSE     = (1 << 0),
    EVAL_CACHE_BUNDLE_FLAG_IR_TREE   = (1 << 1),
    EVAL_CACHE_BUNDLE_FLAG_BYTECODE  = (1 << 2),
    EVAL_CACHE_BUNDLE_FLAG_INTERPRET = (1 << 3)
};

#define EvalParentKey(X) DEFER(EvalPushParentKey(X), EvalPopParentKey())

#define EvalParseFromKey(X)      EvalParseFromBundle(EvalCacheBundleFromKey(X))
#define EvalIRTreeFromKey(X)     EvalIRTreeFromBundle(EvalCacheBundleFromKey(X))
#define EvalByteCodeFromKey(X)   EvalByteCodeFromBundle(EvalCacheBundleFromKey(X))
#define EvalInterpFromKey(X)     EvalInterpFromBundle(EvalCacheBundleFromKey(X))
#define EvalEvalFromKey(X)       EvalEvalFromBundle(EvalCacheBundleFromKey(X))
#define EvalValueFromKey(X)      (EvalValueEvalFromEval(EvalEvalFromKey(X)).Value)
#define EvalParseFromStr(X)      EvalParseFromBundle(EvalCacheBundleFromKey(EvalKeyFromStr(X)))
#define EvalIRTreeFromStr(X)     EvalIRTreeFromBundle(EvalCacheBundleFromKey(EvalKeyFromStr(X)))
#define EvalByteCodeFromStr(X)   EvalByteCodeFromBundle(EvalCacheBundleFromKey(EvalKeyFromStr(X)))
#define EvalInterpFromStr(X)     EvalInterpFromBundle(EvalCacheBundleFromKey(EvalKeyFromStr(X)))
#define EvalEvalFromStr(X)       EvalEvalFromKey(EvalKeyFromStr(X))
#define EvalEvalFromStrFmt(...)  EvalEvalFromKey(EvalKeyFromStr(__VA_ARGS__))
#define EvalValueFromStr(X)      EvalValueEvalFromEval(EvalEvalFromStr(X)).Value
#define EvalValueFromStrFmt(...) EvalValueEvalFromEval(EvalEvalFromStrFmt(__VA_ARGS__)).Value
#define EvalEvalWrap(X, Y)       EvalEvalFromKey(EvalKeyWrap((X).Key, (Y)))
#define EvalEvalWrapFmt(X, ...)  EvalEvalFromKey(EvalKeyWrap((X).Key, __VA_ARGS__))
#define EvalEvalFromExpr(X)      EvalEvalFromKey(EvalKeyFromExpr(X))
#define EvalValueFromExpr(X)     EvalValueEvalFromEval(EvalEvalFromExpr(X)).Value

typedef u32 EvalTypeUnwrapFlag;
enum : u32 {
    EVAL_TYPE_UNWRAP_FLAG_MODS           = (1 << 0),
    EVAL_TYPE_UNWRAP_FLAG_LENSES         = (1 << 1),
    EVAL_TYPE_UNWRAP_FLAG_META           = (1 << 2),
    EVAL_TYPE_UNWRAP_FLAG_ENUMS          = (1 << 3),
    EVAL_TYPE_UNWRAP_FLAG_BITFIELDS      = (1 << 4),
    EVAL_TYPE_UNWRAP_FLAG_ALL            = U32_MAX,
    EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE = (EVAL_TYPE_UNWRAP_FLAG_ALL & ~EVAL_TYPE_UNWRAP_FLAG_BITFIELDS)
};

#define EVAL_EVAL_CTRL_BITS(X, Y, Z)  (((X) << 8) | ((Y) << 4) | ((Z)))
#define EVAL_DECODE_FROM_CTRL_BITS(X) (((X) >> 8) & 0xFF)
#define EVAL_POP_FROM_CTRL_BITS(X)    (((X) >> 4) & 0xF)
#define EVAL_PUSH_FROM_CTRL_BITS(X)   ((X) & 0xF)

enum EvalIdentifierResolutionPath {
    EVAL_IDENT_RES_PATH_PARENT_EXPR,
    EVAL_IDENT_RES_PATH_PARENT_EXPR_MEMBER,
    EVAL_IDENT_RES_PATH_BUILTIN_CONSTANTS,
    EVAL_IDENT_RES_PATH_BUILTIN_TYPES,
    EVAL_IDENT_RES_PATH_MACROS,
    EVAL_IDENT_RES_PATH_COUNT
};

#define EvalTokenAtX(X, Y) (((Y)->V <= (X) && (X) < (Y)->V + (Y)->Count) ? (*(X)) : *EMPTY_EVAL_TOKEN_VALUE);

typedef u32 EVStringFlag;
enum : u32 {
    EV_STR_FLAG_READ_ONLY_DISPLAY_RULES = (1 << 0),
    EV_STR_FLAG_PRETTY_NAMES            = (1 << 1),
    EV_STR_FLAG_DISABLE_STRS            = (1 << 2),
    EV_STR_FLAG_DISABLE_CHARS           = (1 << 3),
    EV_STR_FLAG_DISABLE_STR_QUOTES      = (1 << 4)
};

typedef u32 EvalSpaceRangeFlag;
enum : u32 {
    EVAL_SPACE_RNG_FLAG_ANY_BYTE_BAD     = (1 << 0),
    EVAL_SPACE_RNG_FLAG_ANY_BYTE_CHANGED = (1 << 1),
    EVAL_SPACE_RNG_FLAG_STALE            = (1 << 2)
};

// @types______________________________________________________________________
struct EvalKey {
    u64 V;
};

struct EvalMsg {
    EvalMsg*    Next;
    EvalMsgKind Kind;
    r1u64       Range;
    Str8        Text;
};

struct EvalMsgList {
    EvalMsg*    Head;
    EvalMsg*    Tail;
    EvalMsgKind MaxKind;
    u64         Count;
};

union EvalValue {
    u512 U512;
    u256 U256;
    u128 U128;
    u64  U64;
    u32  U32;
    u16  U16;
    u8   U8;
    i64  I64;
    i32  I32;
    i16  I16;
    i8   I8;
    f64  F64;
    f32  F32;
};

struct EvalOp {
    EvalOp*    Next;
    EvalEvalOp OpCode;
    EvalValue  Value;
    Str8       String;
};

struct EvalOpList {
    EvalOp* Head;
    EvalOp* Tail;
    u64     OpCount;
    u64     EncodedSize;
};

struct EvalOpInfo {
    EvalOpKind Kind;
    i64        Precedence;
    Str8       Pre;
    Str8       Sep;
    Str8       Post;
    Str8       Chain;
};

struct EvalSpaceRangeInfo {
    u64*               ByteBadFlags;
    u64*               ByteChangedFlags;
    EvalSpaceRangeFlag Flags;
};

struct EvalSpace {
    EvalSpaceKind Kind;

    union {
        u64 U64s[3];

        struct {
            u64 U64;
            u128 U128;
        };
    };
};

struct EvalTypeKeyNode {
    EvalTypeKeyNode* Next;
    EvalTypeKey      V;
};

struct EvalTypeKeyList {
    EvalTypeKeyNode* Head;
    EvalTypeKeyNode* Tail;
    u64              Count;
};

struct EvalToken {
    EvalTokenKind Kind;
    r1u64         Range;
};

struct EvalTokenChunkNode {
    EvalTokenChunkNode* Next;
    EvalToken*          V;
    u64                 Count;
    u64                 Capacity;
};

struct EvalTokenChunkList {
    EvalTokenChunkNode* Head;
    EvalTokenChunkNode* Tail;
    u64                 NodeCount;
    u64                 TotalCount;
};

struct EvalTokenArray {
    EvalToken* V;
    u64        Count;
};

struct EvalExpr {
    EvalExpr*    Head;
    EvalExpr*    Tail;
    EvalExpr*    Next;
    EvalExpr*    Prev;
    EvalExpr*    Ref;
    r1u64        Range;
    EvalExprKind Kind;
    EvalMode     Mode;
    EvalSpace    Space;
    EvalTypeKey  TypeKey;
    EvalValue    Value;
    Str8         String;
    Str8         Qualifier;
    Str8         Disambiguator;
    Str8         ByteCode;
};

struct EvalExprChain {
    EvalExpr* Head;
    EvalExpr* Tail;
};

struct EvalExprNode {
    EvalExprNode* Next;
    EvalExpr*     V;
};

struct EvalExprList {
    EvalExprNode* Head;
    EvalExprNode* Tail;
    u64           Count;
};

struct EvalParse {
    EvalTokenArray Tokens;
    EvalToken*     PrevToken;
    EvalExpr*      Expr;
    EvalExpr*      PrevExpr;
    EvalMsgList    Msgs;
};

struct EvalIRNode {
    EvalIRNode* Head;
    EvalIRNode* Tail;
    EvalIRNode* Next;
    EvalEvalOp  Op;
    EvalSpace   Space;
    Str8        String;
    EvalValue   Value;
};

struct EvalIRTreeAndType {
    EvalIRNode*        Root;
    EvalTypeKey        TypeKey;
    void*              UserData;
    EvalMode           Mode;
    b32                AutoHook;
    EvalMsgList        Msgs;
    EvalIRTreeAndType* Prev;
};

struct EvalInterp {
    EvalValue      Value;
    EvalSpace      Space;
    EvalInterpCode Code;
};

struct EvalEval {
    EvalKey           Key;
    EvalKey           ParentKey;
    Str8              String;
    EvalExpr*         Expr;
    EvalIRTreeAndType IRTree;
    Str8              ByteCode;
    EvalInterpCode    Code;
    EvalValue         Value;
    EvalSpace         Space;
    EvalMsgList       Msgs;
};

struct EvalIRExt {
    void* UserData;
};

struct EvalTypeExpandInfo {
    void* UserData;
    u64   ExprCount;
};

struct EvalInterpretContext {
    EvalSpace PrimarySpace;
};

struct EVKey {
    u64 ParentHash;
    u64 ChildID;
};

struct EVExpandNode {
    EVExpandNode* HashNext;
    EVExpandNode* HashPrev;
    EVExpandNode* Head;
    EVExpandNode* Tail;
    EVExpandNode* Next;
    EVExpandNode* Prev;
    EVExpandNode* Parent;
    EVKey         Key;
    b32           Expanded;
};

struct EVExpandSlot {
    EVExpandNode* Head;
    EVExpandNode* Tail;
};

struct EVView {
    Arena*        MemPool;
    EVExpandSlot* ExpandSlots;
    u64           ExpandSlotsCount;
    EVExpandNode* FreeExpandNode;
};

struct EVExpandInfo {
    void* UserData;
    u64   RowCount;
    b32   SingleItem;
    b32   AddNewRow;
    b32   RowsDefaultExpanded;
};

// @defines____________________________________________________________________
#define EVAL_TYPE_IR_EXT_FUNC_SIG(X)              EvalIRExt X(Arena* MemPool, EvalExpr* Expr, EvalIRTreeAndType* IRTree)
#define EVAL_TYPE_IR_EXT_FUNC_NAME(X)             GLUE(__EvalTypeIRExt, X)
#define EVAL_TYPE_IR_EXT_FUNC_DEF(X)              EVAL_TYPE_IR_EXT_FUNC_SIG(EVAL_TYPE_IR_EXT_FUNC_NAME(X))
#define EVAL_TYPE_ACCESS_FUNC_SIG(X)              EvalIRTreeAndType X(Arena* MemPool, EvalIRTreeAndType* Overridden, EvalExpr* Expr, EvalIRTreeAndType* LHSIRTree)
#define EVAL_TYPE_ACCESS_FUNC_NAME(X)             GLUE(__EvalTypeAcess, X)
#define EVAL_TYPE_ACCESS_FUNC_DEF(X)              EVAL_TYPE_ACCESS_FUNC_SIG(EVAL_TYPE_ACCESS_FUNC_NAME(X))
#define EVAL_TYPE_EXPAND_INFO_FUNC_SIG(X)         EvalTypeExpandInfo X(Arena* MemPool, EvalEval Eval, Str8 Filter)
#define EVAL_TYPE_EXPAND_INFO_FUNC_NAME(X)        GLUE(__EvalTypeExpandInfo, X)
#define EVAL_TYPE_EXPAND_INFO_FUNC_DEF(X)         EVAL_TYPE_EXPAND_INFO_FUNC_SIG(EVAL_TYPE_EXPAND_INFO_FUNC_NAME(X))
#define EVAL_TYPE_EXPAND_RANGE_FUNC_SIG(X)        void X(Arena* MemPool, void* UserData, EvalEval Eval, Str8 Filter, r1u64 IndexRange, OUT EvalEval* Evals)
#define EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(X)       GLUE(__EvalTypeExpandRange, X)
#define EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(X)        EVAL_TYPE_EXPAND_RANGE_FUNC_SIG(EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(X))
#define EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_SIG(X)  u64 X(void* UserData, u64 Num)
#define EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_NAME(X) GLUE(__EvalTypeExpandIDFromNum, X)
#define EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_DEF(X)  EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_SIG(EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_NAME(X))
#define EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_SIG(X)  u64 X(void* UserData, u64 ID)
#define EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_NAME(X) GLUE(__EvalTypeExpandNumFromID, X)
#define EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_DEF(X)  EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_SIG(EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_NAME(X))

typedef EVAL_TYPE_IR_EXT_FUNC_SIG(EvalTypeIRExtFuncType);
typedef EVAL_TYPE_ACCESS_FUNC_SIG(EvalTypeAccessFuncType);
typedef EVAL_TYPE_EXPAND_INFO_FUNC_SIG(EvalTypeExpandInfoFuncType);
typedef EVAL_TYPE_EXPAND_RANGE_FUNC_SIG(EvalTypeExpandRangeFuncType);
typedef EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_SIG(EvalTypeExpandIDFromNumFuncType);
typedef EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_SIG(EvalTypeExpandNumFromIDFuncType);

typedef u64 EvalSpaceGenFuncType(EvalSpace Space);
typedef b32 EvalSpaceReadFuncType(EvalSpace Space, OUT void* Data, OUT EvalSpaceRangeInfo* RangeInfo, r1u64 OffsetRange);
typedef b32 EvalSpaceWriteFuncType(EvalSpace Space, OUT void* Data, r1u64 OffsetRange);

#define EvalTypeKeyConstruct(...) [&]() -> EvalTypeKey { \
    EvalConstructedTypeParams __Params = {};             \
    __Params.Kind = EVAL_TYPE_KIND_NULL;                 \
    __VA_ARGS__;                                         \
    return __EvalTypeKeyConstruct(&__Params);            \
}()

#define EV_EXPAND_RULE_INFO_FUNC_SIG(X) EVExpandInfo X(Arena* MemPool, EVView* View, Str8 Filter, EvalExpr* Expr)
#define EV_EXPAND_RULE_INFO_FUNC_NAME(X) __EVExpandRuleInfo##X
#define EV_EXPAND_RULE_INFO_FUNC_DEF(X) EV_EXPAND_RULE_INFO_FUNC_SIG(EV_EXPAND_RULE_INFO_FUNC_NAME(X))
typedef EV_EXPAND_RULE_INFO_FUNC_SIG(EVExpandRuleInfoHookFuncType);

#define TablePushNew(X, Y, ...) [&]() { \
    EVExpandRule __Rule = {};           \
    __VA_ARGS__;                        \
    TablePush((X), (Y), &__Rule);       \
}()

// @types______________________________________________________________________
struct EvalTypeExpandRule {
    EvalTypeExpandInfoFuncType*      ExpandInfoFunc;
    EvalTypeExpandRangeFuncType*     ExpandRangeFunc;
    EvalTypeExpandIDFromNumFuncType* ExpandIDFromNumFunc;
    EvalTypeExpandNumFromIDFuncType* ExpandNumFromIDFunc;
};

struct EvalType {
    EvalTypeKind            Kind;
    EvalTypeFlag            Flags;
    Str8                    Name;
    u64                     ByteSize;
    u64                     Count;
    u64                     Depth;
    u32                     Offset;
    EvalTypeKey             DirectTypeKey;
    EvalTypeKey             OwnerTypeKey;
    EvalTypeKey*            ParamTypeKeys;
    EvalExpr**              Args;
    EvalTypeIRExtFuncType*  IRExtFunc;
    EvalTypeAccessFuncType* AccessFunc;
    EvalTypeExpandRule      ExpandRule;
};

struct EvalConstructedTypeParams {
    EvalTypeKind            Kind;
    EvalTypeFlag            Flags;
    Str8                    Name;
    EvalTypeKey             DirectKey;
    u64                     Count;
    u64                     Depth;
    EvalExpr**              Args;
    EvalTypeIRExtFuncType*  IRExtFunc;
    EvalTypeAccessFuncType* AccessFunc;
    EvalTypeExpandRule      ExpandRule;
};

struct EvalConstructedTypeNode {
    EvalConstructedTypeNode*  KeyNext;
    EvalConstructedTypeNode*  ContentNext;
    EvalTypeKey               Key;
    EvalConstructedTypeParams Params;
    u64                       ByteSize;
};

struct EvalConstructedTypeSlot {
    EvalConstructedTypeNode* Head;
    EvalConstructedTypeNode* Tail;
};

struct EvalStr2ExprMapNode {
    EvalStr2ExprMapNode* HashNext;
    Str8                 String;
    EvalExpr*            Expr;
    u64                  PoisonCount;
};

struct EvalStr2ExprMapSlot {
    EvalStr2ExprMapNode* Head;
    EvalStr2ExprMapNode* Tail;
};

struct EvalStr2ExprMap {
    u64                  SlotsCount;
    EvalStr2ExprMapSlot* Slots;
};

struct EvalStr2TypeKeyNode {
    EvalStr2TypeKeyNode* Next;
    Str8                 String;
    EvalTypeKey          Key;
};

struct EvalStr2TypeKeySlot {
    EvalStr2TypeKeyNode* Head;
    EvalStr2TypeKeyNode* Tail;
};

struct EvalStr2TypeKeyMap {
    u64                  SlotsCount;
    EvalStr2TypeKeySlot* Slots;
};

struct EvalBaseContext {
    EvalSpaceGenFuncType*   SpaceGenFunc;
    EvalSpaceReadFuncType*  SpaceReadFunc;
    EvalSpaceWriteFuncType* SpaceWriteFunc;
};

struct EvalIRContext {
    EvalStr2ExprMap* MacroMap;
};

struct EvalTypeCacheNode {
    EvalTypeCacheNode* Next;
    EvalTypeKey        Key;
    EvalType*          Type;
};

struct EvalTypeCacheSlot {
    EvalTypeCacheNode* Head;
    EvalTypeCacheNode* Tail;
};

struct EvalStrIDNode {
    EvalStrIDNode* HashNext;
    EvalStrIDNode* IDNext;
    u64            ID;
    Str8           String;
};

struct EvalStrIDSlot {
    EvalStrIDNode* Head;
    EvalStrIDNode* Tail;
};

struct EvalStrIDMap {
    u64            IDSlotsCount;
    EvalStrIDSlot* IDSlots;
    u64            HashSlotsCount;
    EvalStrIDSlot* HashSlots;
};

struct EvalCacheBundle {
    EvalCacheBundleFlag Flags;
    EvalKey             Key;
    EvalKey             ParentKey;
    Str8                String;
    EvalParse           Parse;
    EvalIRTreeAndType   IRTree;
    Str8                ByteCode;
    EvalInterp          Interp;
    u64                 SpaceGeneration;
    EvalMsgList         Msgs;
};

struct EvalCacheNode {
    EvalCacheNode*  StringNext;
    EvalCacheNode*  KeyNext;
    EvalCacheBundle Bundle;
};

struct EvalCacheLookup {
    EvalCacheNode* Node;
    u64            Hash;
};

struct EvalCacheSlot {
    EvalCacheNode* Head;
    EvalCacheNode* Tail;
};

struct EvalCacheParentNode {
    EvalCacheParentNode* Next;
    EvalKey              Key;
};

struct EvalCache {
    Arena*                    MemPool;
    u64                       EvalMemPoolStartPosition;
    u64                       KeyIDGeneration;
    u64                       KeySlotsCount;
    EvalCacheSlot*            KeySlots;
    u64                       StringSlotsCount;
    EvalCacheSlot*            StringSlots;
    EvalCacheParentNode*      HeadParentNode;
    EvalCacheParentNode*      FreeParentNode;
    u64                       ConstructedIDGeneration;
    u64                       ConstructedContentSlotsCount;
    u64                       ConstructedKeySlotsCount;
    EvalConstructedTypeSlot*  ConstructedContentSlots;
    EvalConstructedTypeSlot*  ConstructedKeySlots;
    EvalTypeKey               FileTypeKey;
    EvalTypeKey               FolderTypeKey;
    u64                       TypeCacheSlotsCount;
    EvalTypeCacheSlot*        TypeCacheSlots;
    b32                       DisallowAutoHooks;
    b32                       DisallowChainedFastPaths;
    u64                       StringIDGeneration;
    EvalStrIDMap*             StringIDMap;
};

struct EvalIdentifierResolutionRule {
    EvalIdentifierResolutionPath* Paths;
    u64                           PathsCount;
};

struct EvalIRCacheNode {
    EvalIRCacheNode*  Next;
    EvalExpr*         Expr;
    EvalIRNode*       OverriddenNode;
    EvalIRTreeAndType IRTree;
};

struct EvalIRCacheSlot {
    EvalIRCacheNode* Head;
    EvalIRCacheNode* Tail;
};

struct EvalIRState {
    Arena*                    MemPool;
    u64                       MemPoolEvalStartPosition;
    EvalIRContext*            Ctx;
    EvalIRTreeAndType*        OverriddenIRTree;
    b32                       DisallowAutoHooks;
    b32                       DisallowChainedFastPaths;
    u64                       StringIDGeneration;
    EvalStrIDMap*             StringIDMap;
    u64                       IRCacheSlotsCount;
    EvalIRCacheSlot*          IRCacheSlots;
};

struct EvalConversionMatrixNode {
    EvalEvalConversionKind DstTypeGroups[EVAL_EVAL_TYPE_GROUP_COUNT];
};

struct EVExpandRule {
    Str8                          String;
    EVExpandRuleInfoHookFuncType* InfoHookFunc;
};

struct EVExpandRuleNode {
    EVExpandRuleNode* Next;
    EVExpandRule      V;
};

struct EVExpandRuleSlot {
    EVExpandRuleNode* Head;
    EVExpandRuleNode* Tail;
};

struct EVExpandRuleTable {
    EVExpandRuleSlot* Slots;
    u64               SlotsCount;
};

struct EvalExpandRuleTagPair {
    EVExpandRule* Rule;
    EvalExpr*     Tag;
};

struct EVBlock {
    EVBlock*            Head;
    EVBlock*            Tail;
    EVBlock*            Next;
    EVBlock*            Prev;
    EVBlock*            Parent;
    EVKey               Key;
    u64                 SplitRelativeIndex;
    Str8                String;
    EvalEval            Eval;
    Str8                Filter;
    EvalTypeExpandInfo  TypeExpandInfo;
    EvalTypeExpandRule* TypeExpandRule;
    EVExpandInfo        VisualExpandInfo;
    EVExpandRule*       VisualExpandRule;
    u64                 RowCount;
};

struct EVBlockTree {
    EVBlock* Root;
    u64      TotalRowCount;
    u64      TotalItemCount;
};

struct EVBlockRange {
    EVBlock* Block;
    r1u64    Range;
};

struct EVBlockRangeNode {
    EVBlockRangeNode* Next;
    EVBlockRange      V;
};

struct EVBlockRangeList {
    EVBlockRangeNode* Head;
    EVBlockRangeNode* Tail;
    u64               Count;
};

struct EVRow {
    EVBlock* Block;
    EVKey    Key;
    u64      VisualSize;
    Str8     EditString;
    EvalEval Eval;
};

struct EVWindowedRowNode {
    EVWindowedRowNode* Next;
    u64                VisualSizeSkipped;
    u64                VisualSizeChopped;
    EVRow              Row;
};

struct EVWindowedRowList {
    EVWindowedRowNode* Head;
    EVWindowedRowNode* Tail;
    u64                Count;
    u64                CountBeforeVisual;
    u64                CountBeforeSemantic;
};

struct EVStrParams {
    EVStringFlag Flags;
    u32          Radix;
    u32          MinDigits;
    u8           DigitGroupSeparator;
    Str8         Filter;
    b32          LimitStrings;
    u64          LimitStringsSize;
};

struct EVStrIterTask {
    EVStrIterTask* Next;
    EVStrParams    Params;
    EvalEval       Eval;
    u64            Index;
    i32            Depth;
    b32            RedirectToSetsAndStructs;
    void*          UserData;
};

struct EVStrIter {
    EVStrIterTask* HeadTask;
    EVStrIterTask* FreeTask;
};

// @runtime____________________________________________________________________
EXTERN_C_LINK_BEGIN
extern Str8 EVAL_TOKEN_KIND_STRINGS[EVAL_TOKEN_KIND_COUNT];
extern Str8 EVAL_TYPE_KIND_BASIC_STRING_TABLE[35];
extern u8 EVAL_TYPE_KIND_BASIC_BYTE_SIZE_TABLE[35];
extern Str8 EVAL_EXPR_KIND_STRINGS[EVAL_EXPR_KIND_COUNT];
extern EvalOpInfo EVAL_EXPR_KIND_OP_INFO_TABLE[EVAL_EXPR_KIND_COUNT];
extern Str8 EVAL_INTERP_CODE_DISPLAY_STRINGS[EVAL_INTERP_CODE_COUNT];
extern u16 EVAL_EVAL_OP_CTRL_BITS_TABLE[EVAL_EVAL_OP_COUNT + 1];
extern EvalIdentifierResolutionPath EVAL_DEFAULT_IDENT_RES_PATHS[EVAL_IDENT_RES_PATH_COUNT];
extern EvalIdentifierResolutionPath EVAL_CALLABLE_IDENT_RES_PATHS[EVAL_IDENT_RES_PATH_COUNT];
extern EvalConversionMatrixNode EVAL_EVAL_TYPE_GROUP_CONV_KIND_MATRIX[8];
EXTERN_C_LINK_END

constexpr EvalTypeKey EMPTY_EVAL_TYPE_KEY_VALUE = {};
constexpr EvalKey EMPTY_EVAL_KEY_VALUE = {};
constexpr EvalIdentifierResolutionRule EVAL_DEFAULT_IDENT_RES_RULE = {
    EVAL_DEFAULT_IDENT_RES_PATHS,
    ARRAY_COUNT(EVAL_DEFAULT_IDENT_RES_PATHS)
};
constexpr EvalIdentifierResolutionRule EVAL_CALLABLE_IDENT_RES_RULE = {
    EVAL_CALLABLE_IDENT_RES_PATHS,
    ARRAY_COUNT(EVAL_CALLABLE_IDENT_RES_PATHS)
};
constexpr EVKey EMPTY_EV_KEY_VALUE = {};

extern EvalStr2ExprMap* const EMPTY_EVAL_STR2EXPR_MAP_VALUE;
extern EvalExpr* const EMPTY_EVAL_EXPR_VALUE;
extern EvalIRNode* const EMPTY_EVAL_IR_NODE_VALUE;
extern EvalEval* const EMPTY_EVAL_EVAL_VALUE;
extern EvalCacheBundle* const EMPTY_EVAL_CACHE_BUNDLE_VALUE;
extern EvalToken* const EMPTY_EVAL_TOKEN_VALUE;
extern EVBlock* const EMPTY_EV_BLOCK_VALUE;

global readonly EvalType EMPTY_EVAL_TYPE_VALUE = {
    EVAL_TYPE_KIND_NULL
};

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Default);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Default);
EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_DEF(Identity);
EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_DEF(Identity);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(OnlyAndOmit);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(OnlyAndOmit);
EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Folder);
EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Folder);
EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_DEF(Folder);
EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_DEF(Folder);

global readonly EvalTypeExpandRule EVAL_DEFAULT_TYPE_EXPAND_RULES = {
    EVAL_TYPE_EXPAND_INFO_FUNC_NAME(Default),
    EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(Default),
    EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_NAME(Identity),
    EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_NAME(Identity)
};

EVAL_TYPE_ACCESS_FUNC_DEF(Default);

EV_EXPAND_RULE_INFO_FUNC_DEF(Null);

global readonly EVExpandRule EV_NULL_EXPAND_RULE = {
    {},
    EV_EXPAND_RULE_INFO_FUNC_NAME(Null)
};

// @functions__________________________________________________________________
void EvalInit(void);
EvalEvalOp EvalOpCodeFromExprKind(EvalExprKind Kind);
b32 EvalExprKindIsCmp(EvalExprKind Kind);
void ListPush(Arena* MemPool, EvalTypeKeyList* List, EvalTypeKey Key);
void ListPushFront(Arena* MemPool, EvalTypeKeyList* List, EvalTypeKey Key);
EvalTypeKeyList ListCpy(Arena* MemPool, EvalTypeKeyList* List);
void EvalMessage(Arena* MemPool, EvalMsgList* Msgs, EvalMsgKind Kind, r1u64 Range, Str8 String);
void EvalMessage(Arena* MemPool, EvalMsgList* Msgs, EvalMsgKind Kind, r1u64 Range, char* Fmt, ...);
void ListCat(EvalMsgList* Dst, EvalMsgList* Src);
EvalMsgList ListCpy(Arena* MemPool, EvalMsgList* List);
EvalSpace EvalSpaceAlloc(EvalSpaceKind Kind);
EvalStr2ExprMap EvalStr2ExprMapAlloc(Arena* MemPool, u64 SlotsCount);
void MapInsert(Arena* MemPool, EvalStr2ExprMap* Map, Str8 String, EvalExpr* Expr);
void EvalStr2ExprMapPoisonIncrement(EvalStr2ExprMap* Map, Str8 String);
void EvalStr2ExprMapPoisonDecrement(EvalStr2ExprMap* Map, Str8 String);
EvalExpr* EvalStr2ExprMapLookup(EvalStr2ExprMap* Map, Str8 String);
EvalStr2TypeKeyMap EvalStr2TypeKeyMapAlloc(Arena* MemPool, u64 SlotsCount);
void MapInsert(Arena* MemPool, EvalStr2TypeKeyMap* Map, Str8 String, EvalTypeKey TypeKey);
EvalTypeKey EvalStr2TypeKeyMapLookup(EvalStr2TypeKeyMap* Map, Str8 String);
EvalCache* EvalCacheAlloc(void);
void EvalCacheRelease(EvalCache* Cache);
void EvalCacheSelect(EvalCache* Cache);
void EvalBaseContextSelect(EvalBaseContext* Ctx);
void EvalIRContextSelect(EvalIRContext* Ctx);
EvalKey EvalPushParentKey(EvalKey Key);
EvalKey EvalPopParentKey(void);
EvalKey EvalKeyFromStr(Str8 String);
EvalKey EvalKeyFromStr(char* Fmt, ...);
EvalKey EvalKeyFromExpr(EvalExpr* Expr);
EvalCacheBundle* EvalCacheBundleFromKey(EvalKey Key);
EvalParse EvalParseFromBundle(EvalCacheBundle* Bundle);
EvalIRTreeAndType EvalIRTreeFromBundle(EvalCacheBundle* Bundle);
Str8 EvalByteCodeFromBundle(EvalCacheBundle* Bundle);
EvalInterp EvalInterpFromBundle(EvalCacheBundle* Bundle);
Str8 EvalFullExprFromKey(Arena* MemPool, EvalKey Key);
EvalEval EvalEvalFromBundle(EvalCacheBundle* Bundle);
EvalEval EvalValueEvalFromEval(EvalEval Eval);
u64 EvalIDFromStr(Str8 String);
Str8 EvalStrFromID(u64 ID);
EvalKey EvalKeyWrap(EvalKey Key, Str8 String);
EvalKey EvalKeyWrap(EvalKey Key, char* Fmt, ...);
r1u64 EvalRangeFromEval(EvalEval Eval);
EvalEvalTypeGroup EvalTypeGroupFromKind(EvalTypeKind Kind);
b32 EvalTypeKindIsInteger(EvalTypeKind Kind);
b32 EvalTypeKindIsSigned(EvalTypeKind Kind);
b32 EvalTypeKindIsBasicOrEnum(EvalTypeKind Kind);
b32 EvalTypeKindIsPtrOrRef(EvalTypeKind Kind);
EvalTypeKey EvalTypeKeyBasic(EvalTypeKind Kind);
EvalTypeKey EvalTypeKeyExt(EvalTypeKind Kind, u32 TypeIndex, u32 PIDIndex);
u64 Hash(EvalConstructedTypeParams* Params);
EvalTypeKey __EvalTypeKeyConstruct(EvalConstructedTypeParams* Params);
EvalTypeKey EvalTypeKeyConstructArray(EvalTypeKey ElementTypeKey, u64 Count, EvalTypeFlag Flags);
EvalTypeKey EvalTypeKeyConstructMetaExpr(EvalTypeKey TypeKey, Str8 Expr);
EvalTypeKey EvalTypeKeyConstructMetaDisplayName(EvalTypeKey TypeKey, Str8 Name);
EvalTypeKey EvalTypeKeyConstructMetaDescription(EvalTypeKey TypeKey, Str8 Desc);
EvalTypeKind EvalTypeKindFromKey(EvalTypeKey Key);
u64 EvalByteSizeFromKey(EvalTypeKey Key);
EvalType* EvalPushTypeFromKey(Arena* MemPool, EvalTypeKey Key);
EvalTypeExpandRule* EvalExpandRuleFromTypeKey(EvalTypeKey Key);
EvalTypeKey EvalTypeKeyDirect(EvalTypeKey Key);
EvalTypeKey EvalTypeKeyPromote(EvalTypeKey Key);
EvalTypeKey EvalTypeKeyUnwrap(EvalTypeKey Key, EvalTypeUnwrapFlag Flags);
void EvalTypeLHSFromStr(Arena* MemPool, EvalTypeKey Key, OUT Str8List* List, u32 Prec, b32 ShouldSkipReturn);
void EvalTypeRHSFromStr(Arena* MemPool, EvalTypeKey Key, OUT Str8List* List, u32 Prec);
Str8 EvalTypeStrFromKey(Arena* MemPool, EvalTypeKey Key);
EvalTypeKey EvalDefaultExpansionTypeFromKey(EvalTypeKey Key);
EvalType* EvalTypeFromKey(EvalTypeKey Key);
void EvalInterpContextSelect(EvalInterpretContext* Ctx);
u64 EvalSpaceGen(EvalSpace Space);
b32 EvalSpaceRead(EvalSpace Space, OUT void* Data, OUT EvalSpaceRangeInfo* RangeInfo, r1u64 Range);
b32 EvalSpaceWrite(EvalSpace Space, IN void* Data, r1u64 Range);
EvalInterp EvalInterpret(Str8 ByteCode);
void ListPushOp(Arena* MemPool, EvalOpList* List, EvalEvalOp OpCode, EvalValue Value);
void ListPushConstU(Arena* MemPool, EvalOpList* List, u64 X);
void ListPushByteCode(Arena* MemPool, EvalOpList* List, Str8 ByteCode);
void ListPushSetSpace(Arena* MemPool, EvalOpList* List, EvalSpace Space);
void ListPushStrLit(Arena* MemPool, EvalOpList* List, Str8 String);
void ListCat(EvalOpList* Dst, EvalOpList* Src);
EvalIRNode* EvalPushIRNode(Arena* MemPool, EvalEvalOp Op);
void EvalIRNodePushChild(EvalIRNode* Parent, EvalIRNode* Child);
EvalIRNode* EvalIRTreeConstU(Arena* MemPool, u64 Value);
EvalIRNode* EvalIRTreeUnaryOp(Arena* MemPool, EvalEvalOp Op, EvalEvalTypeGroup Group, EvalIRNode* Child);
EvalIRNode* EvalIRTreeBinaryOp(Arena* MemPool, EvalEvalOp Op, EvalEvalTypeGroup Group, u64 OperandSize, EvalIRNode* LHS, EvalIRNode* RHS);
EvalIRNode* EvalIRTreeBinaryOpU(Arena* MemPool, EvalEvalOp Op, u64 OperandSize, EvalIRNode* LHS, EvalIRNode* RHS);
EvalIRNode* EvalIRTreeByteCode(Arena* MemPool, Str8 ByteCode);
EvalIRNode* EvalIRTreeStrLit(Arena* MemPool, Str8 String);
EvalIRNode* EvalIRTreeSetSpace(Arena* MemPool, EvalSpace Space, EvalIRNode* Child);
EvalIRNode* EvalIRTreeMemReadType(Arena* MemPool, EvalIRNode* Child, EvalTypeKey TypeKey);
EvalIRNode* EvalIRTreeConvertLo(Arena* MemPool, EvalIRNode* Child, OUT EvalEvalTypeGroup To, IN EvalEvalTypeGroup From);
EvalIRNode* EvalIRTreeTrunc(Arena* MemPool, EvalIRNode* Child, EvalTypeKey TypeKey);
EvalIRNode* EvalIRTreeConvertHi(Arena* MemPool, EvalIRNode* Child, OUT EvalTypeKey To, IN EvalTypeKey From);
EvalIRNode* EvalIRTreeResolveToValue(Arena* MemPool, EvalMode FromMode, EvalIRNode* Tree, EvalTypeKey TypeKey);
EvalIRTreeAndType EvalPushIRTreeAndType(Arena* MemPool, EvalIRTreeAndType* RootParent, EvalIdentifierResolutionRule* IdentResRule, b32 DisallowAutoHooks, b32 DisallowChainedFastPaths, EvalExpr* RootExpr);
void EvalAppendOpList(Arena* MemPool, EvalIRNode* Root, EvalSpace* CurrentSpace, OUT EvalOpList* List);
EvalOpList EvalOpListFromIRTree(Arena* MemPool, EvalIRNode* Root);
Str8 EvalByteCodeFromOpList(Arena* MemPool, EvalOpList* List);
EvalExpr* EvalExprIRExtMemberAccess(Arena* MemPool, EvalExpr* LHS, EvalIRTreeAndType* LHSIRTree, Str8 MemberName);
void ListPush(Arena* MemPool, EvalTokenChunkList* List, u64 ChunkSize, EvalToken* Token);
EvalTokenArray EvalTokenArrayFromChunkList(Arena* MemPool, EvalTokenChunkList* List);
EvalTokenArray EvalTokenArrayFromText(Arena* MemPool, Str8 Text);
EvalTokenArray EvalTokenArrayAlloc(EvalToken* Head, EvalToken* Tail);
EvalExpr* EvalPushExpr(Arena* MemPool, EvalExprKind Kind, r1u64 Range);
void EvalExprPushChild(EvalExpr* Parent, EvalExpr* Child);
EvalExpr* EvalExprRef(Arena* MemPool, EvalExpr* Ref);
EvalExpr* EvalExprCpy(Arena* MemPool, EvalExpr* Expr);
void ListPush(Arena* MemPool, EvalExprList* List, EvalExpr* Expr);
void EvalAppendStrsFromExpr(Arena* MemPool, EvalExpr* Expr, Str8 ParentExprString, OUT Str8List* List);
Str8 EvalStrFromExpr(Arena* MemPool, EvalExpr* Expr, Str8 ParentExprString);
EvalTypeKey EvalLeafBuiltinTypeKeyFromName(Str8 Name);
EvalTypeKey EvalLeafTypeKeyFromName(Str8 Name);
EvalTypeKey EvalTypeKeyFromExpr(EvalExpr* Expr);
EvalParse EvalPushParseFromStr(Arena* MemPool, Str8 Text, EvalTokenArray Tokens, i64 MaxPrecedence, u64 MaxChainCount);
EvalParse EvalPushParseFromStr(Arena* MemPool, Str8 Text);
EvalEvalConversionKind EvalEvalConvKindFromTypeGroups(EvalEvalTypeGroup From, EvalEvalTypeGroup To);
i32 EvalEvalOpTypeGroupAreCompatible(EvalEvalOp Op, EvalEvalTypeGroup Group);
EVKey EVKeyAlloc(u64 ParentHash, u64 ChildID);
EVKey EVKeyRoot(void);
u64 Hash(EVKey Key);
EvalTypeKey EVExpansionTypeFromKey(EvalTypeKey TypeKey);
b32 EVTypeKeyAndModeIsExpandable(EvalTypeKey TypeKey, EvalMode Mode);
b32 EVTypeKeyIsEditable(EvalTypeKey TypeKey);
EVView* EVViewAlloc(void);
void EVViewRelease(EVView* View);
EVExpandNode* EVExpandNodeFromKey(EVView* View, EVKey Key);
b32 EVExpansionFromKey(EVView* View, EVKey Key);
void EVKeySetExpansion(EVView* View, EVKey ParentKey, EVKey Key, b32 Expanded);
void TablePush(Arena* MemPool, EVExpandRuleTable* Table, EVExpandRule* Info);
void EVSelectExpandRuleTable(EVExpandRuleTable* Table);
EVExpandRule* EVExpandRuleFromStr(Str8 String);
EVExpandRule* EVExpandRuleFromTypeKey(EvalTypeKey TypeKey);
EVBlockTree EVBlockTreeFromEval(Arena* MemPool, EVView* View, Str8 Filter, EvalEval RootEval);
u64 EVDepthFromBlock(EVBlock* Block);
u64 EVBlockIDFromNum(EVBlock* Block, u64 Num);
u64 EVBlockNumFromID(EVBlock* Block, u64 ID);
EVBlockRangeList EVBlockRangeListFromTree(Arena* MemPool, EVBlockTree* BlockTree);
EVBlockRange EVBlockRangeFromNum(EVBlockRangeList* BlockRanges, u64 Num);
EVKey EVKeyFromNum(EVBlockRangeList* BlockRanges, u64 Num);
u64 EVNumFromKey(EVBlockRangeList* BlockRanges, EVKey Key);
u64 EVVNumFromNum(EVBlockRangeList* BlockRanges, u64 Num);
u64 EVNumFromVNum(EVBlockRangeList* BlockRanges, u64 VNum);
EVWindowedRowList EVWindowedRowListFromBlockRangeList(Arena* MemPool, EVView* View, EVBlockRangeList* BlockRanges, r1u64 VNumRange);
EVRow* EVRowFromNum(Arena* MemPool, EVView* View, EVBlockRangeList* BlockRanges, u64 Num);
EVWindowedRowList EVRowsFromNumRange(Arena* MemPool, EVView* View, EVBlockRangeList* BlockRanges, r1u64 NumRange);
b32 EVEvalIsExpandable(EvalEval Eval);
b32 EVRowIsExpandable(EVRow* Row);
b32 EVRowIsEditable(EVRow* Row);
Str8 EVStrFromASCII(Arena* MemPool, u8 Char);
Str8 EVStrFromSimpleTypedEval(Arena* MemPool, EVStrParams* Params, EvalEval Eval);
Str8 EVEscapedFromRawStr(Arena* MemPool, Str8 Raw);
EVStrIter* EVStrIterBegin(Arena* MemPool, EvalEval Eval, EVStrParams* Params);
b32 EVStrIterNext(Arena* MemPool, EVStrIter* Iter, OUT Str8* String);

#if defined(NC_LANG_CPP)
    internal b32 
    operator==(const EvalKey& KeyA, const EvalKey& KeyB)
    {
        return (KeyA.V == KeyB.V);
    }

    internal b32
    operator!=(const EvalKey& KeyA, const EvalKey& KeyB)
    {
        return !(KeyA == KeyB);
    }

    internal b32 
    operator==(const EvalSpace& SpaceA, const EvalSpace& SpaceB)
    {
        return MemCmpStruct(&SpaceA, &SpaceB);
    }

    internal b32
    operator!=(const EvalSpace& SpaceA, const EvalSpace& SpaceB)
    {
        return !(SpaceA == SpaceB);
    }

    internal b32
    operator==(const EvalTypeKey& KeyA, const EvalTypeKey& KeyB)
    {
        return MemCmpStruct(&KeyA, &KeyB);
    }

    internal b32
    operator!=(const EvalTypeKey& KeyA, const EvalTypeKey& KeyB)
    {
        return !(KeyA == KeyB);
    }

    internal b32
    operator==(
        const EvalConstructedTypeParams& ParamsA, 
        const EvalConstructedTypeParams& ParamsB
    ) {
        b32 Result = (
            ParamsA.Kind != EVAL_TYPE_KIND_LENS &&
            ParamsA.Kind == ParamsB.Kind &&
            ParamsA.Flags == ParamsB.Flags &&
            StrMatch(ParamsA.Name, ParamsB.Name, 0) &&
            ParamsA.DirectKey == ParamsB.DirectKey &&
            ParamsA.Count == ParamsB.Count &&
            ParamsA.Depth == ParamsB.Depth
        );

        return Result;
    }

    internal b32
    operator!=(
        const EvalConstructedTypeParams& ParamsA, 
        const EvalConstructedTypeParams& ParamsB
    ) {
        return !(ParamsA == ParamsB);
    }

    internal b32
    operator==(const EVKey& KeyA, const EVKey& KeyB)
    {
        return MemCmpStruct(&KeyA, &KeyB);
    }

    internal b32
    operator!=(const EVKey& KeyA, const EVKey& KeyB)
    {
        return !(KeyA == KeyB);
    }
#endif

#endif // __NC_EVAL_H__
