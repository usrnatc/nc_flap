#if !defined(__FLV_KERNEL_H__)
#define __FLV_KERNEL_H__

#include "nc_types.h"
#include "nc_string.h"

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

#define FLV_KERNEL_FLAG_RUN       (1 << 0)
#define FLV_KERNEL_FLAG_SCALAR    (1 << 1)
#define FLV_KERNEL_FLAG_REDUCTION (1 << 2)
#define FLV_KERNEL_FLAG_WINDOW    (1 << 3)
#define FLV_KERNEL_FLAG_EMIT      (1 << 4)

#define FLV_KERNEL_LIST(X)                                                                 \
    X(NONE,                0, NONE, 0)                                                     \
    X(LOAD_I64,            1,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(LOAD_F64,            1,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(LOAD_INSTANCE,       0,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(CONVERT_I64_F64,     1,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(ADD_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(ADD_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(ADD_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(ADD_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(SUB_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(SUB_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(SUB_I64_RS,          2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(SUB_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(SUB_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(SUB_F64_RS,          2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MUL_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MUL_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MUL_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MUL_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(DIV_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(DIV_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(DIV_I64_RS,          2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(DIV_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(DIV_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(DIV_F64_RS,          2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MOD_I64_T,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MOD_I64_S,           2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MOD_I64_RS,          2,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MOD_F64_T,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MOD_F64_S,           2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(MOD_F64_RS,          2,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(NEG_I64,             1,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(NEG_F64,             1,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(ABS_I64,             1,  I64, FLV_KERNEL_FLAG_RUN)                                   \
    X(ABS_F64,             1,  F64, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LT_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LT_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LT_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LT_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LE_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LE_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LE_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_LE_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GT_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GT_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GT_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GT_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GE_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GE_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GE_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_GE_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_EQ_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_EQ_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_EQ_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_EQ_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_NE_I64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_NE_I64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_NE_F64_T,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(CMP_NE_F64_S,        2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(MASK_AND,            2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(MASK_OR,             2, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(MASK_NOT,            1, MASK, FLV_KERNEL_FLAG_RUN)                                   \
    X(MEAN_I64,            1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(MEAN_F64,            1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(STDDEV_I64,          1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(STDDEV_F64,          1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(MIN_I64,             1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(MIN_F64,             1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(MAX_I64,             1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(MAX_F64,             1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(SUM_I64,             1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(SUM_F64,             1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(FIRST_I64,           1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(FIRST_F64,           1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(LAST_I64,            1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(LAST_F64,            1, SINGLE_F64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(REDUCE_COUNT,        1, SINGLE_I64, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(DIFF_I64,            1, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(DIFF_F64,            1, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(RATE_I64,            1, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(RATE_F64,            1, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(ROLLING_MEAN_I64,    2, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(ROLLING_MEAN_F64,    2, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(ROLLING_MIN_I64,     2, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(ROLLING_MIN_F64,     2, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(ROLLING_MAX_I64,     2, I64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(ROLLING_MAX_F64,     2, F64,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_WINDOW)          \
    X(SEL_FROM_MASK,       1, SELECTION,  FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_REDUCTION) \
    X(SEL_FROM_TIME_RANGE, 2, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                          \
    X(SEL_MODE,            1, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                          \
    X(SEL_EVENT,           1, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                          \
    X(SEL_ARMED,           0, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                          \
    X(SEL_AND,             2, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                          \
    X(SEL_OR,              2, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                          \
    X(SEL_NOT,             1, SELECTION,  FLV_KERNEL_FLAG_SCALAR)                          \
    X(SEL_DURATION,        1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_ADD_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_ADD_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_SUB_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_SUB_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_MUL_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_MUL_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_DIV_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_DIV_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_MOD_I64,           2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_MOD_F64,           2, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_NEG_I64,           1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_NEG_F64,           1, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_ABS_I64,           1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_ABS_F64,           1, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_LT_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_LT_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_LE_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_LE_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_GT_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_GT_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_GE_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_GE_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_EQ_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_EQ_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_NE_I64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CMP_NE_F64,        2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_AND,               2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_OR,                2, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_NOT,               1, SINGLE_I64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(S_CONVERT_I64_F64,   1, SINGLE_F64, FLV_KERNEL_FLAG_SCALAR)                          \
    X(EMIT_CURVE,          1, NONE, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_EMIT)            \
    X(EMIT_TABLE_COLUMN,   1, NONE, FLV_KERNEL_FLAG_RUN | FLV_KERNEL_FLAG_EMIT)            \
    X(EMIT_FLAG,           1, NONE, FLV_KERNEL_FLAG_SCALAR | FLV_KERNEL_FLAG_EMIT)         \
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

// @runtime____________________________________________________________________
extern FLVKernelInfo const FLV_KERNEL_INFO_TABLE[FLV_KERNEL_COUNT];
extern Str8 const FLV_KERNEL_NAME_TABLE[FLV_KERNEL_COUNT];

// @functions__________________________________________________________________


#endif // __FLV_KERNEL_H__
