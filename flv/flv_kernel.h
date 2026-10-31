#if !defined(__FLV_KERNEL_H__)
#define __FLV_KERNEL_H__

#include "nc_types.h"
#include "nc_string.h"

struct FLVLane;
struct FLVRegion;

// @defines____________________________________________________________________
typedef u8 FLVElementKind;
enum : u8 {
    FLV_ELEMENT_KIND_NONE,
    FLV_ELEMENT_KIND_I64,
    FLV_ELEMENT_KIND_F64,
    FLV_ELEMENT_KIND_MASK,
    FLV_ELEMENT_KIND_SELECTION,
    FLV_ELEMENT_KIND_SINGLE_I64,
    FLV_ELEMENT_KIND_SINGLE_F64,
    FLV_ELEMENT_KIND_COUNT
};

typedef u16 FLVOperand;
enum : u16 {
    FLV_OPERAND_KIND_TEMP,
    FLV_OPERAND_KIND_SLOT,
    FLV_OPERAND_KIND_CONST,
    FLV_OPERAND_KIND_NONE
};

#define FLV_OPERAND_KIND_SHIFT 14
#define FLV_OPERAND_INDEX_MASK 0x3FFFU
#define FLV_OPERAND_NONE 0xFFFFU

#define FLV_KERNEL_FLAG_RUN       (1 << 0)
#define FLV_KERNEL_FLAG_SCALAR    (1 << 1)
#define FLV_KERNEL_FLAG_REDUCTION (1 << 2)
#define FLV_KERNEL_FLAG_WINDOW    (1 << 3)
#define FLV_KERNEL_FLAG_EMIT      (1 << 4)
#define FLV_KERNEL_FLAG_ROLLING   (1 << 5)

#define FLV_ARITH_HEAD(Type)                                                \
    if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) { \
        FLVTempSetMissing(Lane, Op.Dst);                                    \
                                                                            \
        return;                                                             \
    }                                                                       \
                                                                            \
    Type* Dst = (Type*) FLVTempPtr(Lane, Op.Dst);                           \
    u64   Count = Lane->ChunkCount

#define FLV_OP_ADD(A, B)     ((A) + (B))
#define FLV_OP_SUB(A, B)     ((A) - (B))
#define FLV_OP_MUL(A, B)     ((A) * (B))
#define FLV_OP_DIV_I64(A, B) ((B) ? (A) / (B) : FLV_I64_HOLE)
#define FLV_OP_DIV_F64(A, B) ((A) / (B))
#define FLV_OP_MOD_I64(A, B) ((B) ? (A) % (B) : FLV_I64_HOLE)
#define FLV_OP_MOD_F64(A, B) FMod((A), (B))

#define FLV_EVENT_ID_ARMED    10
#define FLV_EVENT_ID_DISARMED 11

#define FLV_KERNEL_LIST(X)                                                                                  \
    X(NONE,                0, NONE, 0)                                                                      \
    X(LOAD_I64,            1,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(LOAD_F64,            1,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(LOAD_INSTANCE,       0,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CONVERT_I64_F64,     1,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(ADD_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(ADD_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(ADD_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(ADD_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(SUB_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(SUB_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(SUB_I64_RS,          2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(SUB_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(SUB_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(SUB_F64_RS,          2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MUL_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MUL_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MUL_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MUL_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(DIV_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(DIV_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(DIV_I64_RS,          2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(DIV_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(DIV_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(DIV_F64_RS,          2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MOD_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MOD_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MOD_I64_RS,          2,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MOD_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MOD_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MOD_F64_RS,          2,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(NEG_I64,             1,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(NEG_F64,             1,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(ABS_I64,             1,  I64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(ABS_F64,             1,  F64, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LT_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LT_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LT_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LT_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LE_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LE_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LE_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_LE_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GT_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GT_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GT_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GT_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GE_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GE_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GE_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_GE_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_EQ_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_EQ_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_EQ_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_EQ_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_NE_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_NE_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_NE_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(CMP_NE_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MASK_AND,            2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MASK_OR,             2, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MASK_NOT,            1, MASK, FLV_KERNEL_FLAG_RUN)                                                    \
    X(MEAN_I64,            1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(MEAN_F64,            1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(STDDEV_I64,          1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(STDDEV_F64,          1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(MIN_I64,             1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(MIN_F64,             1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(MAX_I64,             1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(MAX_F64,             1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(SUM_I64,             1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(SUM_F64,             1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(FIRST_I64,           1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(FIRST_F64,           1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(LAST_I64,            1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(LAST_F64,            1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(REDUCE_COUNT,        1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(DIFF_I64,            1, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)                           \
    X(DIFF_F64,            1, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)                           \
    X(RATE_I64,            1, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)                           \
    X(RATE_F64,            1, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)                           \
    X(ROLLING_MEAN_I64,    2, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW | FLV_KERNEL_FLAG_ROLLING) \
    X(ROLLING_MEAN_F64,    2, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW | FLV_KERNEL_FLAG_ROLLING) \
    X(ROLLING_MIN_I64,     2, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW | FLV_KERNEL_FLAG_ROLLING) \
    X(ROLLING_MIN_F64,     2, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW | FLV_KERNEL_FLAG_ROLLING) \
    X(ROLLING_MAX_I64,     2, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW | FLV_KERNEL_FLAG_ROLLING) \
    X(ROLLING_MAX_F64,     2, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW | FLV_KERNEL_FLAG_ROLLING) \
    X(SEL_FROM_MASK,       1, SELECTION,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION)                  \
    X(SEL_FROM_TIME_RANGE, 2, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                                           \
    X(SEL_MODE,            1, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                                           \
    X(SEL_EVENT,           1, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                                           \
    X(SEL_ARMED,           0, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                                           \
    X(SEL_AND,             2, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                                           \
    X(SEL_OR,              2, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                                           \
    X(SEL_NOT,             1, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                                           \
    X(SEL_DURATION,        1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_ADD_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_ADD_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_SUB_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_SUB_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_MUL_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_MUL_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_DIV_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_DIV_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_MOD_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_MOD_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_NEG_I64,           1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_NEG_F64,           1, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_ABS_I64,           1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_ABS_F64,           1, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_LT_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_LT_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_LE_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_LE_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_GT_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_GT_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_GE_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_GE_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_EQ_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_EQ_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_NE_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CMP_NE_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_AND,               2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_OR,                2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_NOT,               1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(S_CONVERT_I64_F64,   1, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                                           \
    X(EMIT_CURVE,          1, NONE, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_EMIT)                             \
    X(EMIT_TABLE_COLUMN,   1, NONE, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_EMIT)                             \
    X(EMIT_FLAG,           1, NONE, FLV_KERNEL_FLAG_SCALAR | FLV_KERNEL_FLAG_EMIT)                          \
    X(EMIT_MISSING,        0, NONE, FLV_KERNEL_FLAG_SCALAR | FLV_KERNEL_FLAG_EMIT)

#define X(Name, Inputs, Dst, Flags) FLV_KERNEL_##Name,

typedef u16 FLVKernelID;
enum : u16 {
    FLV_KERNEL_LIST(X)
    FLV_KERNEL_COUNT
};

#undef X


// @types______________________________________________________________________
struct FLVKernelInfo {
    u8 InputCount;
    u8 DstKind;
    u8 Flags;
    u8 __Padding;
};

struct FLVOp {
    u16 Kernel;
    FLVOperand Dst;
    FLVOperand LHS;
    FLVOperand RHS;
};
STATIC_ASSERT(sizeof(FLVOp) == 8, FLVOpSizeCheck);

union FLVNumber {
    f64 F64;
    i64 I64;
};

struct FLVReduction {
    FLVNumber Summation;
    FLVNumber Minimum;
    FLVNumber Maximum;
    FLVNumber Head;
    FLVNumber Tail;
    f64       SummationSquared;
    f64       Shift;
    u64       Count;
};

struct FLVWindow {
    u64*       Times;
    FLVNumber* Values;
    u64        Capacity;
    u64        Head;
    u64        Count;
    FLVNumber  Summation;
    FLVNumber  Prev;
    u64        PrevTimeUSecs;
    b32        HasPrev;
    u32        __Padding;
};

struct FLVSelectionBuild {
    r1u64* V;
    u64    Count;
    u64    Capacity;
    u64    OpenStartUSecs;
    u64    OpenEndUSecs;
    b32    IsOpen;
    u32    __Padding;
};


// @defines____________________________________________________________________
#define FLV_KERNEL_FUNC_SIG(X) void X(FLVLane* Lane, FLVOp Op)
typedef FLV_KERNEL_FUNC_SIG(FLVKernelFuncType);

// @runtime____________________________________________________________________
extern FLVKernelInfo const FLV_KERNEL_INFO_TABLE[FLV_KERNEL_COUNT];
extern Str8 const FLV_KERNEL_NAME_TABLE[FLV_KERNEL_COUNT];
extern FLVKernelFuncType* const FLV_KERNEL_TABLE[FLV_KERNEL_COUNT];

// @functions__________________________________________________________________
void FLVKernelsRegionBegin(FLVLane* Lane, FLVRegion const* Region);
void FLVKernelsRegionEnd(FLVLane* Lane, FLVRegion const* Region);
void FLVKernelsRangeReset(FLVLane* Lane, FLVRegion const* Region);
f64 FLVScalarF64(FLVLane* Lane, FLVOperand Operand);
i64 FLVScalarI64(FLVLane* Lane, FLVOperand Operand);

INTERNAL FLVOperand
FLVOperandAlloc(u16 Kind, u32 Index)
{
    ASSERT(Index <= FLV_OPERAND_INDEX_MASK);

    FLVOperand Result = (FLVOperand) (
        (Kind << FLV_OPERAND_KIND_SHIFT) | Index
    );

    return Result;
}

INTERNAL u16
FLVGetOperandKind(FLVOperand Operand)
{
    return Operand >> FLV_OPERAND_KIND_SHIFT;
}

INTERNAL u32
FLVGetOperandIndex(FLVOperand Operand)
{
    return Operand & FLV_OPERAND_INDEX_MASK;
}

#endif // __FLV_KERNEL_H__
