#include "nc_defines.h"
#include "nc_types.h"
#include "nc_math.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_string.h"
#include "nc_time.h"
#include "nc_log.h"

#include "fli.h"

#include "flv_kernel.h"
#include "flv_lower.h"
#include "flv_bind.h"
#include "flv_run.h"
#include "flv_stdlib.h"

#define X(Name, Inputs, Dst, Flags) { Inputs, FLV_ELEMENT_KIND_##Dst, Flags, 0 },
FLVKernelInfo const FLV_KERNEL_INFO_TABLE[FLV_KERNEL_COUNT] = {
    FLV_KERNEL_LIST(X)
};
#undef X

#define X(Name, Inputs, Dst, Flags) Str8Lit(#Name),
Str8 const FLV_KERNEL_NAME_TABLE[FLV_KERNEL_COUNT] = {
    FLV_KERNEL_LIST(X)
};
#undef X


INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelStub)
{
    LogError(
        "kernel %S is not implemented yet",
        PRINT_STR(FLV_KERNEL_NAME_TABLE[Op.Kernel])
    );
    FLVTempSetMissing(Lane, Op.Dst);
}

f64
FLVScalarF64(FLVLane* Lane, FLVOperand Operand)
{
    f64 Result = 0.0;

    switch (FLVGetOperandKind(Operand)) {
        default: {
            ASSERT(FALSE);
        } break;

        case FLV_OPERAND_KIND_CONST: {
            FLVConst const* Const = &Lane->Program->Consts[
                FLVGetOperandIndex(Operand)
            ];

            Result = (Const->Kind == FLV_CONST_KIND_F64)
                ? Const->F64
                : (f64) Const->I64;
        } break;

        case FLV_OPERAND_KIND_TEMP: {
            Result = *(f64 const*) FLVTempPtr(Lane, Operand);
        } break;
    }

    return Result;
}

i64
FLVScalarI64(FLVLane* Lane, FLVOperand Operand)
{
    i64 Result = 0;

    switch (FLVGetOperandKind(Operand)) {
        default: {
            ASSERT(FALSE);
        } break;

        case FLV_OPERAND_KIND_CONST: {
            FLVConst const* Const = &Lane->Program->Consts[
                FLVGetOperandIndex(Operand)
            ];

            Result = (Const->Kind == FLV_CONST_KIND_F64)
                ? (i64) Const->F64
                : Const->I64;
        } break;

        case FLV_OPERAND_KIND_TEMP: {
            Result = *(i64 const*) FLVTempPtr(Lane, Operand);
        } break;
    }

    return Result;
}

INTERNAL u64
FLVScalarTimeUSecs(FLVLane* Lane, FLVOperand Operand)
{
    u64 Result = 0;

    if (FLVGetOperandKind(Operand) == FLV_OPERAND_KIND_CONST)
        Result = Lane->Program->Consts[FLVGetOperandIndex(Operand)].TimeUSecs;
    else
        Result = *(u64 const*) FLVTempPtr(Lane, Operand);

    return Result;
}


INTERNAL FLVSelection
FLVSelectionAlloc(FLVLane* Lane, u64 Capacity)
{
    FLVSelection Result = {};

    Result.V = ArenaPushArray(Lane->MemPool, r1u64, Capacity);
    Result.Count = 0;

    return Result;
}

INTERNAL u64
FLVMaskHead(u64 const* Mask, u64 Count)
{
    u64 Result = Count;
    u64 Words = (Count + 63) >> 6;

    for (u64 Word = 0; Word < Words; ++Word) {
        if (Mask[Word]) {
            Result = MIN((Word << 6) + Ctz64(Mask[Word]), Count);
            break;
        }
    }
 
    return Result;
}
 
INTERNAL u64
FLVMaskTail(u64 const* Mask, u64 Count)
{
    u64 Result = Count;
    u64 Words = (Count + 63) >> 6;
    u64 TailBits = Count & 63;
    u64 TailMask = TailBits ? (1ULL << TailBits) - 1 : U64_MAX;

    for (u64 Word = Words; Word--; ) {
        u64 V = (Word == Words - 1) ? Mask[Word] & TailMask : Mask[Word];

        if (V) {
            Result = (Word << 6) + 64 - Clz64(V);
            break;
        }
    }
 
    return Result;
}

#define FLV_LOAD_LOOP(SrcType, DstType)                              \
    do {                                                             \
        SrcType const* __Src = (SrcType const*) Column->Data + Base; \
        DstType* __Dst = (DstType*) Result;                          \
                                                                     \
        for (u64 Index = 0; Index < Count; ++Index)                  \
            __Dst[Index] = (DstType) __Src[Index];                   \
    } while (FALSE)
INTERNAL void
FLVLoadChunk(
    FLVLane* Lane,
    FLVColumn const* Column,
    FLVOperand Dst,
    b32 ToF64
) {
    void* Result = FLVTempPtr(Lane, Dst);
    u64 Base = Lane->ChunkBase;
    u64 Count = Lane->ChunkCount;

    if (!Column->Data) {
        FLVTempSetMissing(Lane, Dst);

        return;
    }

    if (ToF64) {
        switch (Column->Kind) {
            default: {
                FLVTempSetMissing(Lane, Dst);
            } break;

            case FLI_SCALAR_KIND_U8: {
                FLV_LOAD_LOOP(u8,  f64);
            } break;

            case FLI_SCALAR_KIND_I8: {
                FLV_LOAD_LOOP(i8,  f64);
            } break;

            case FLI_SCALAR_KIND_U16: {
                FLV_LOAD_LOOP(u16, f64);
            } break;

            case FLI_SCALAR_KIND_I16: {
                FLV_LOAD_LOOP(i16, f64);
            } break;

            case FLI_SCALAR_KIND_U32: {
                FLV_LOAD_LOOP(u32, f64);
            } break;

            case FLI_SCALAR_KIND_I32: {
                FLV_LOAD_LOOP(i32, f64);
            } break;

            case FLI_SCALAR_KIND_U64: {
                FLV_LOAD_LOOP(u64, f64);
            } break;

            case FLI_SCALAR_KIND_I64: {
                FLV_LOAD_LOOP(i64, f64);
            } break;

            case FLI_SCALAR_KIND_F32: {
                FLV_LOAD_LOOP(f32, f64);
            } break;

            case FLI_SCALAR_KIND_F64: {
                FLV_LOAD_LOOP(f64, f64);
            } break;
        }
    } else {
        switch (Column->Kind) {
            default: {
                FLVTempSetMissing(Lane, Dst);
            } break;

            case FLI_SCALAR_KIND_U8: {
                FLV_LOAD_LOOP(u8,  i64);
            } break;

            case FLI_SCALAR_KIND_I8: {
                FLV_LOAD_LOOP(i8,  i64);
            } break;

            case FLI_SCALAR_KIND_U16: {
                FLV_LOAD_LOOP(u16, i64);
            } break;

            case FLI_SCALAR_KIND_I16: {
                FLV_LOAD_LOOP(i16, i64);
            } break;

            case FLI_SCALAR_KIND_U32: {
                FLV_LOAD_LOOP(u32, i64);
            } break;

            case FLI_SCALAR_KIND_I32: {
                FLV_LOAD_LOOP(i32, i64);
            } break;

            case FLI_SCALAR_KIND_U64: {
                FLV_LOAD_LOOP(u64, i64);
            } break;

            case FLI_SCALAR_KIND_I64: {
                FLV_LOAD_LOOP(i64, i64);
            } break;

            case FLI_SCALAR_KIND_F32: {
                FLV_LOAD_LOOP(f32, i64);
            } break;

            case FLI_SCALAR_KIND_F64: {
                FLV_LOAD_LOOP(f64, i64);
            } break;
        }
    }
}
#undef FLV_LOAD_LOOP

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelLoadI64)
{
    ASSERT(FLVGetOperandKind(Op.LHS) == FLV_OPERAND_KIND_SLOT);

    FLVLoadChunk(
        Lane, 
        &Lane->Columns[FLVGetOperandIndex(Op.LHS)], 
        Op.Dst, 
        FALSE
    );
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelLoadF64)
{
    ASSERT(FLVGetOperandKind(Op.LHS) == FLV_OPERAND_KIND_SLOT);

    FLVLoadChunk(
        Lane, 
        &Lane->Columns[FLVGetOperandIndex(Op.LHS)], 
        Op.Dst, 
        TRUE
    );
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelLoadInstance)
{
    FLVLoadChunk(
        Lane,
        &Lane->InstanceColumns[Lane->Region->BaseSlot],
        Op.Dst,
        FALSE
    );
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelConvertI64F64)
{
    if (FLVTempIsMissing(Lane, Op.LHS)) {
        FLVTempSetMissing(Lane, Op.Dst);
 
        return;
    }
 
    i64 const* A = (i64 const*) FLVTempPtr(Lane, Op.LHS);
    f64* Dst = (f64*) FLVTempPtr(Lane, Op.Dst);
 
    for (u64 Index = 0; Index < Lane->ChunkCount; ++Index)
        Dst[Index] = (A[Index] == FLV_I64_HOLE) ? NC_NAN : (f64) A[Index];
}

#define FLV_ARITH_KERNEL_T(Name, Type, OpM)                     \
    INTERNAL                                                    \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                        \
    {                                                           \
        FLV_ARITH_HEAD(Type);                                   \
        Type const* A = (Type const*) FLVTempPtr(Lane, Op.LHS); \
        Type const* B = (Type const*) FLVTempPtr(Lane, Op.RHS); \
                                                                \
        for (u64 Index = 0; Index < Count; ++Index)             \
            Dst[Index] = OpM(A[Index], B[Index]);               \
    }
 
#define FLV_ARITH_KERNEL_S(Name, Type, ScalarFunc, OpM)         \
    INTERNAL                                                    \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                        \
    {                                                           \
        FLV_ARITH_HEAD(Type);                                   \
        Type const* A = (Type const*) FLVTempPtr(Lane, Op.LHS); \
        Type        B = ScalarFunc(Lane, Op.RHS);               \
                                                                \
        for (u64 Index = 0; Index < Count; ++Index)             \
            Dst[Index] = OpM(A[Index], B);                      \
    }
 
#define FLV_ARITH_KERNEL_RS(Name, Type, ScalarFunc, OpM)        \
    INTERNAL                                                    \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                        \
    {                                                           \
        FLV_ARITH_HEAD(Type);                                   \
        Type A = ScalarFunc(Lane, Op.LHS);                      \
        Type const* B = (Type const*) FLVTempPtr(Lane, Op.RHS); \
                                                                \
        for (u64 Index = 0; Index < Count; ++Index)             \
            Dst[Index] = OpM(A, B[Index]);                      \
    }
 
#define FLV_ARITH_FAMILY_2(Name, OpI, OpF)                   \
    FLV_ARITH_KERNEL_T(Name##_I64_T, i64, OpI)               \
    FLV_ARITH_KERNEL_S(Name##_I64_S, i64, FLVScalarI64, OpI) \
    FLV_ARITH_KERNEL_T(Name##_F64_T, f64, OpF)               \
    FLV_ARITH_KERNEL_S(Name##_F64_S, f64, FLVScalarF64, OpF)
 
#define FLV_ARITH_FAMILY_3(Name, OpI, OpF)                     \
    FLV_ARITH_KERNEL_T(Name##_I64_T, i64, OpI)                 \
    FLV_ARITH_KERNEL_S(Name##_I64_S, i64, FLVScalarI64, OpI)   \
    FLV_ARITH_KERNEL_RS(Name##_I64_RS, i64, FLVScalarI64, OpI) \
    FLV_ARITH_KERNEL_T(Name##_F64_T, f64, OpF)                 \
    FLV_ARITH_KERNEL_S(Name##_F64_S, f64, FLVScalarF64, OpF)   \
    FLV_ARITH_KERNEL_RS(Name##_F64_RS, f64, FLVScalarF64, OpF)
 
FLV_ARITH_FAMILY_2(ADD, FLV_OP_ADD, FLV_OP_ADD)
FLV_ARITH_FAMILY_3(SUB, FLV_OP_SUB, FLV_OP_SUB)
FLV_ARITH_FAMILY_2(MUL, FLV_OP_MUL, FLV_OP_MUL)
FLV_ARITH_FAMILY_3(DIV, FLV_OP_DIV_I64, FLV_OP_DIV_F64)
FLV_ARITH_FAMILY_3(MOD, FLV_OP_MOD_I64, FLV_OP_MOD_F64)
 
#undef FLV_ARITH_FAMILY_3
#undef FLV_ARITH_FAMILY_2
#undef FLV_ARITH_KERNEL_RS
#undef FLV_ARITH_KERNEL_S
#undef FLV_ARITH_KERNEL_T
#undef FLV_ARITH_HEAD
 
#define FLV_UNARY_KERNEL(Name, SrcType, DstType, Expr)                \
    INTERNAL                                                          \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                              \
    {                                                                 \
        if (FLVTempIsMissing(Lane, Op.LHS)) {                         \
            FLVTempSetMissing(Lane, Op.Dst);                          \
                                                                      \
            return;                                                   \
        }                                                             \
                                                                      \
        SrcType const* A = (SrcType const*) FLVTempPtr(Lane, Op.LHS); \
        DstType* Dst = (DstType*) FLVTempPtr(Lane, Op.Dst);           \
                                                                      \
        for (u64 Index = 0; Index < Lane->ChunkCount; ++Index)        \
            Dst[Index] = (Expr);                                      \
    }
 
FLV_UNARY_KERNEL(NEG_I64, i64, i64, -A[Index])
FLV_UNARY_KERNEL(NEG_F64, f64, f64, -A[Index])
FLV_UNARY_KERNEL(ABS_I64, i64, i64, (A[Index] < 0) ? -A[Index] : A[Index])
FLV_UNARY_KERNEL(ABS_F64, f64, f64, (A[Index] < 0.0) ? -A[Index] : A[Index])
 
#undef FLV_UNARY_KERNEL
#undef FLV_OP_MOD_F64
#undef FLV_OP_MOD_I64
#undef FLV_OP_DIV_F64
#undef FLV_OP_DIV_I64
#undef FLV_OP_MUL
#undef FLV_OP_SUB
#undef FLV_OP_ADD

#define FLV_CMP_KERNEL_T(Name, Type, OpSym)                                      \
    INTERNAL                                                                     \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                                         \
    {                                                                            \
        if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) {  \
            FLVTempSetMissing(Lane, Op.Dst);                                     \
                                                                                 \
            return;                                                              \
        }                                                                        \
                                                                                 \
        Type const* A = (Type const*) FLVTempPtr(Lane, Op.LHS);                  \
        Type const* B = (Type const*) FLVTempPtr(Lane, Op.RHS);                  \
        u64* Mask = (u64*) FLVTempPtr(Lane, Op.Dst);                             \
                                                                                 \
        MemSet(Mask, 0, FLV_CHUNK_MASK_WORDS * sizeof(u64));                     \
                                                                                 \
        for (u64 Index = 0; Index < Lane->ChunkCount; ++Index)                   \
            Mask[Index >> 6] |= (u64) (A[Index] OpSym B[Index]) << (Index & 63); \
    }

#define FLV_CMP_KERNEL_S(Name, Type, OpSym, ScalarFunc)                         \
    INTERNAL                                                                    \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                                        \
    {                                                                           \
        if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) { \
            FLVTempSetMissing(Lane, Op.Dst);                                    \
                                                                                \
            return;                                                             \
        }                                                                       \
                                                                                \
        Type const* A = (Type const*) FLVTempPtr(Lane, Op.LHS);                 \
        Type B = ScalarFunc(Lane, Op.RHS);                                      \
        u64* Mask = (u64*) FLVTempPtr(Lane, Op.Dst);                            \
                                                                                \
        MemSet(Mask, 0, FLV_CHUNK_MASK_WORDS * sizeof(u64));                    \
                                                                                \
        for (u64 Index = 0; Index < Lane->ChunkCount; ++Index)                  \
            Mask[Index >> 6] |= (u64) (A[Index] OpSym B) << (Index & 63);       \
    }

#define FLV_CMP_FAMILY(Cmp, OpSym)                          \
    FLV_CMP_KERNEL_T(Cmp##_I64_T, i64, OpSym)               \
    FLV_CMP_KERNEL_S(Cmp##_I64_S, i64, OpSym, FLVScalarI64) \
    FLV_CMP_KERNEL_T(Cmp##_F64_T, f64, OpSym)               \
    FLV_CMP_KERNEL_S(Cmp##_F64_S, f64, OpSym, FLVScalarF64)

FLV_CMP_FAMILY(CMP_LT, <)
FLV_CMP_FAMILY(CMP_LE, <=)
FLV_CMP_FAMILY(CMP_GT, >)
FLV_CMP_FAMILY(CMP_GE, >=)
FLV_CMP_FAMILY(CMP_EQ, ==)
FLV_CMP_FAMILY(CMP_NE, !=)

#undef FLV_CMP_FAMILY
#undef FLV_CMP_KERNEL_S
#undef FLV_CMP_KERNEL_T

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelMaskAnd)
{
    if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    u64 const* LHS = (u64 const*) FLVTempPtr(Lane, Op.LHS);
    u64 const* RHS = (u64 const*) FLVTempPtr(Lane, Op.RHS);
    u64* Dst = (u64*) FLVTempPtr(Lane, Op.Dst);

    for (u64 Index = 0; Index < FLV_CHUNK_MASK_WORDS; ++Index)
        Dst[Index] = LHS[Index] & RHS[Index];
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelMaskOr)
{
    if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    u64 const* LHS = (u64 const*) FLVTempPtr(Lane, Op.LHS);
    u64 const* RHS = (u64 const*) FLVTempPtr(Lane, Op.RHS);
    u64*       Dst = (u64*) FLVTempPtr(Lane, Op.Dst);

    for (u64 Index = 0; Index < FLV_CHUNK_MASK_WORDS; ++Index)
        Dst[Index] = LHS[Index] | RHS[Index];
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelMaskNot)
{
    if (FLVTempIsMissing(Lane, Op.LHS)) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    u64 const* LHS = (u64 const*) FLVTempPtr(Lane, Op.LHS);
    u64*       Dst = (u64*) FLVTempPtr(Lane, Op.Dst);
    u64        FullWords = Lane->ChunkCount >> 6;
    u64        TailBits = Lane->ChunkCount & 63;

    for (u64 Index = 0; Index < FullWords; ++Index)
        Dst[Index] = ~LHS[Index];

    if (TailBits)
        Dst[FullWords] = ~LHS[FullWords] & ((1ULL << TailBits) - 1);

    for (
        u64 Index = FullWords + !!TailBits;
        Index < FLV_CHUNK_MASK_WORDS;
        ++Index
    ) {
        Dst[Index] = 0;
    }
}

#define FLV_REDUCE_KERNEL(Name, Type, Field, Body)                        \
    INTERNAL                                                              \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                                  \
    {                                                                     \
        FLVReduction* R = &Lane->Reductions[FLVGetOperandIndex(Op.Dst)];  \
                                                                          \
        if (FLVTempIsMissing(Lane, Op.LHS)) {                             \
            FLVTempSetMissing(Lane, Op.Dst);                              \
                                                                          \
            return;                                                       \
        }                                                                 \
                                                                          \
        u64 const* Mask = Lane->Mask ? Lane->Mask : Lane->MaskAll;        \
        Type const* A = (Type const*) FLVTempPtr(Lane, Op.LHS);           \
        u64 Head = FLVMaskHead(Mask, Lane->ChunkCount);                   \
        u64 Tail = FLVMaskTail(Mask, Lane->ChunkCount);                   \
                                                                          \
        if (Head >= Tail)                                                 \
            return;                                                       \
                                                                          \
        if (!R->Count) {                                                  \
            R->Head.Field = A[Head];                                      \
            R->Minimum.Field = A[Head];                                   \
            R->Maximum.Field = A[Head];                                   \
            R->Shift = (f64) A[Head];                                     \
        }                                                                 \
                                                                          \
        R->Tail.Field = A[Tail - 1];                                      \
                                                                          \
        for (u64 Index = Head; Index < Tail; ++Index) {                   \
            Type X = A[Index];                                            \
            u64  Bit = FLVMaskBit(Mask, Index);                           \
                                                                          \
            Body;                                                         \
            R->Count += Bit;                                              \
        }                                                                 \
    }
 
#define FLV_BODY_NONE

#define FLV_BODY_SUM(Field, Zero) R->Summation.Field += Bit ? X : Zero
#define FLV_BODY_MEAN             R->Summation.F64 += Bit ? (f64) X : 0.0

#define FLV_BODY_STDDEV                                                   \
    f64 D = Bit ? (f64) X - R->Shift : 0.0;                               \
    R->Summation.F64 += D;                                                \
    R->SummationSquared += D * D

#define FLV_BODY_MIN(Field) R->Minimum.Field = (Bit && X < R->Minimum.Field) ? X : R->Minimum.Field
#define FLV_BODY_MAX(Field) R->Maximum.Field = (Bit && X > R->Maximum.Field) ? X : R->Maximum.Field
 
FLV_REDUCE_KERNEL(MEAN_I64,     i64, I64, FLV_BODY_MEAN)
FLV_REDUCE_KERNEL(MEAN_F64,     f64, F64, FLV_BODY_MEAN)
FLV_REDUCE_KERNEL(STDDEV_I64,   i64, I64, FLV_BODY_STDDEV)
FLV_REDUCE_KERNEL(STDDEV_F64,   f64, F64, FLV_BODY_STDDEV)
FLV_REDUCE_KERNEL(MIN_I64,      i64, I64, FLV_BODY_MIN(I64))
FLV_REDUCE_KERNEL(MIN_F64,      f64, F64, FLV_BODY_MIN(F64))
FLV_REDUCE_KERNEL(MAX_I64,      i64, I64, FLV_BODY_MAX(I64))
FLV_REDUCE_KERNEL(MAX_F64,      f64, F64, FLV_BODY_MAX(F64))
FLV_REDUCE_KERNEL(SUM_I64,      i64, I64, FLV_BODY_SUM(I64, 0))
FLV_REDUCE_KERNEL(SUM_F64,      f64, F64, FLV_BODY_SUM(F64, 0.0))
FLV_REDUCE_KERNEL(FIRST_I64,    i64, I64, FLV_BODY_NONE)
FLV_REDUCE_KERNEL(FIRST_F64,    f64, F64, FLV_BODY_NONE)
FLV_REDUCE_KERNEL(LAST_I64,     i64, I64, FLV_BODY_NONE)
FLV_REDUCE_KERNEL(LAST_F64,     f64, F64, FLV_BODY_NONE)
FLV_REDUCE_KERNEL(REDUCE_COUNT, i64, I64, FLV_BODY_NONE)
 
#undef FLV_BODY_MAX
#undef FLV_BODY_MIN
#undef FLV_BODY_STDDEV
#undef FLV_BODY_MEAN
#undef FLV_BODY_SUM
#undef FLV_BODY_NONE
#undef FLV_REDUCE_KERNEL

#define FLV_RING(W, Index) ((Index) & ((W)->Capacity - 1))
 
#define FLV_WINDOW_HEAD(Type)                                             \
    FLVWindow* W = &Lane->Windows[FLVGetOperandIndex(Op.Dst)];            \
                                                                          \
    if (FLVTempIsMissing(Lane, Op.LHS)) {                                 \
        FLVTempSetMissing(Lane, Op.Dst);                                  \
                                                                          \
        return;                                                           \
    }                                                                     \
                                                                          \
    Type const* A = (Type const*) FLVTempPtr(Lane, Op.LHS);               \
    u64 const*  T = (u64 const*) Lane->BaseTime->Data + Lane->ChunkBase;  \
    u64 Count = Lane->ChunkCount
 
#define FLV_DIFF_KERNEL(Name, Type, Field, Hole)                          \
    INTERNAL                                                              \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                                  \
    {                                                                     \
        FLV_WINDOW_HEAD(Type);                                            \
        Type* Dst = (Type*) FLVTempPtr(Lane, Op.Dst);                     \
                                                                          \
        if (!Count)                                                       \
            return;                                                       \
                                                                          \
        Dst[0] = W->HasPrev ? A[0] - W->Prev.Field : Hole;                \
                                                                          \
        for (u64 Index = 1; Index < Count; ++Index)                       \
            Dst[Index] = A[Index] - A[Index - 1];                         \
                                                                          \
        W->Prev.Field = A[Count - 1];                                     \
        W->HasPrev = TRUE;                                                \
    }
 
FLV_DIFF_KERNEL(DIFF_I64, i64, I64, FLV_I64_HOLE)
FLV_DIFF_KERNEL(DIFF_F64, f64, F64, NC_NAN)
 
#undef FLV_DIFF_KERNEL
 
#define FLV_RATE_KERNEL(Name, Type, Field)                                \
    INTERNAL                                                              \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                                  \
    {                                                                     \
        FLV_WINDOW_HEAD(Type);                                            \
        f64* Dst = (f64*) FLVTempPtr(Lane, Op.Dst);                       \
                                                                          \
        if (!Count)                                                       \
            return;                                                       \
                                                                          \
        Dst[0] = W->HasPrev                                               \
            ? (f64) (A[0] - W->Prev.Field) * SECONDS(1) /                 \
              (f64) (T[0] - W->PrevTimeUSecs)                             \
            : NC_NAN;                                                     \
                                                                          \
        for (u64 Index = 1; Index < Count; ++Index) {                     \
            Dst[Index] = (f64) (A[Index] - A[Index - 1]) * SECONDS(1) /   \
                (f64) (T[Index] - T[Index - 1]);                          \
        }                                                                 \
                                                                          \
        W->Prev.Field = A[Count - 1];                                     \
        W->PrevTimeUSecs = T[Count - 1];                                  \
        W->HasPrev = TRUE;                                                \
    }
 
FLV_RATE_KERNEL(RATE_I64, i64, I64)
FLV_RATE_KERNEL(RATE_F64, f64, F64)
 
#undef FLV_RATE_KERNEL
 
#define FLV_EVICT_FRONT(Body)                                                \
    while (W->Count && W->Times[FLV_RING(W, W->Head)] + Width <= T[Index]) { \
        Body;                                                                \
        ++W->Head;                                                           \
        --W->Count;                                                          \
    }
 
#define FLV_ROLLING_MEAN_KERNEL(Name, Type, Field)                          \
    INTERNAL                                                                \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                                    \
    {                                                                       \
        FLV_WINDOW_HEAD(Type);                                              \
        Type* Dst = (Type*) FLVTempPtr(Lane, Op.Dst);                       \
        u64 Width = FLVScalarTimeUSecs(Lane, Op.RHS);                       \
                                                                            \
        for (u64 Index = 0; Index < Count; ++Index) {                       \
            FLV_EVICT_FRONT(                                                \
                W->Summation.Field -= W->Values[FLV_RING(W, W->Head)].Field \
            );                                                              \
                                                                            \
            u64 Back = FLV_RING(W, W->Head + W->Count);                     \
                                                                            \
            W->Times[Back] = T[Index];                                      \
            W->Values[Back].Field = A[Index];                               \
            W->Summation.Field += A[Index];                                 \
            ++W->Count;                                                     \
            Dst[Index] = W->Summation.Field / (Type) W->Count;              \
        }                                                                   \
    }
 
#define FLV_ROLLING_EXTREME_KERNEL(Name, Type, Field, PopSym)             \
    INTERNAL                                                              \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                                  \
    {                                                                     \
        FLV_WINDOW_HEAD(Type);                                            \
        Type* Dst = (Type*) FLVTempPtr(Lane, Op.Dst);                     \
        u64 Width = FLVScalarTimeUSecs(Lane, Op.RHS);                     \
                                                                          \
        for (u64 Index = 0; Index < Count; ++Index) {                     \
            FLV_EVICT_FRONT({});                                          \
                                                                          \
            while (                                                       \
                W->Count &&                                               \
                W->Values[FLV_RING(W, W->Head + W->Count - 1)].Field      \
                    PopSym A[Index]                                       \
            ) {                                                           \
                --W->Count;                                               \
            }                                                             \
                                                                          \
            u64 Back = FLV_RING(W, W->Head + W->Count);                   \
                                                                          \
            W->Times[Back] = T[Index];                                    \
            W->Values[Back].Field = A[Index];                             \
            ++W->Count;                                                   \
            Dst[Index] = W->Values[FLV_RING(W, W->Head)].Field;           \
        }                                                                 \
    }
 
FLV_ROLLING_MEAN_KERNEL(ROLLING_MEAN_I64, i64, I64)
FLV_ROLLING_MEAN_KERNEL(ROLLING_MEAN_F64, f64, F64)
FLV_ROLLING_EXTREME_KERNEL(ROLLING_MIN_I64, i64, I64, >=)
FLV_ROLLING_EXTREME_KERNEL(ROLLING_MIN_F64, f64, F64, >=)
FLV_ROLLING_EXTREME_KERNEL(ROLLING_MAX_I64, i64, I64, <=)
FLV_ROLLING_EXTREME_KERNEL(ROLLING_MAX_F64, f64, F64, <=)
 
#undef FLV_ROLLING_EXTREME_KERNEL
#undef FLV_ROLLING_MEAN_KERNEL
#undef FLV_EVICT_FRONT
#undef FLV_WINDOW_HEAD
#undef FLV_RING

#define FLV_S_HEAD                                                              \
    if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) {     \
        FLVTempSetMissing(Lane, Op.Dst);                                        \
                                                                                \
        return;                                                                 \
    }
 
#define FLV_S_BINARY(Name, Type, DstType, ScalarFunc, Expr)   \
    INTERNAL                                                  \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                      \
    {                                                         \
        FLV_S_HEAD                                            \
                                                              \
        Type A = ScalarFunc(Lane, Op.LHS);                    \
        Type B = ScalarFunc(Lane, Op.RHS);                    \
        DstType* Dst = (DstType*) FLVTempPtr(Lane, Op.Dst);   \
                                                              \
        *Dst = (Expr);                                        \
    }
 
#define FLV_S_UNARY(Name, Type, DstType, ScalarFunc, Expr)    \
    INTERNAL                                                  \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                      \
    {                                                         \
        FLV_S_HEAD                                            \
                                                              \
        Type A = ScalarFunc(Lane, Op.LHS);                    \
        DstType* Dst = (DstType*) FLVTempPtr(Lane, Op.Dst);   \
                                                              \
        *Dst = (Expr);                                        \
    }
 
#define FLV_S_DIVIDE(Name, Expr)                              \
    INTERNAL                                                  \
    FLV_KERNEL_FUNC_SIG(FLVKernel##Name)                      \
    {                                                         \
        FLV_S_HEAD                                            \
                                                              \
        i64 A = FLVScalarI64(Lane, Op.LHS);                   \
        i64 B = FLVScalarI64(Lane, Op.RHS);                   \
        i64* Dst = (i64*) FLVTempPtr(Lane, Op.Dst);           \
                                                              \
        if (!B) {                                             \
            FLVTempSetMissing(Lane, Op.Dst);                  \
                                                              \
            return;                                           \
        }                                                     \
                                                              \
        *Dst = (Expr);                                        \
    }
 
FLV_S_BINARY(S_ADD_I64, i64, i64, FLVScalarI64, A + B)
FLV_S_BINARY(S_ADD_F64, f64, f64, FLVScalarF64, A + B)
FLV_S_BINARY(S_SUB_I64, i64, i64, FLVScalarI64, A - B)
FLV_S_BINARY(S_SUB_F64, f64, f64, FLVScalarF64, A - B)
FLV_S_BINARY(S_MUL_I64, i64, i64, FLVScalarI64, A * B)
FLV_S_BINARY(S_MUL_F64, f64, f64, FLVScalarF64, A * B)
FLV_S_DIVIDE(S_DIV_I64, A / B)
FLV_S_BINARY(S_DIV_F64, f64, f64, FLVScalarF64, A / B)
FLV_S_DIVIDE(S_MOD_I64, A % B)
FLV_S_BINARY(S_MOD_F64, f64, f64, FLVScalarF64, FMod(A, B))
FLV_S_UNARY(S_NEG_I64, i64, i64, FLVScalarI64, -A)
FLV_S_UNARY(S_NEG_F64, f64, f64, FLVScalarF64, -A)
FLV_S_UNARY(S_ABS_I64, i64, i64, FLVScalarI64, Abs64(A))
FLV_S_UNARY(S_ABS_F64, f64, f64, FLVScalarF64, AbsF64(A))
FLV_S_BINARY(S_CMP_LT_I64, i64, i64, FLVScalarI64, A < B)
FLV_S_BINARY(S_CMP_LT_F64, f64, i64, FLVScalarF64, A < B)
FLV_S_BINARY(S_CMP_LE_I64, i64, i64, FLVScalarI64, A <= B)
FLV_S_BINARY(S_CMP_LE_F64, f64, i64, FLVScalarF64, A <= B)
FLV_S_BINARY(S_CMP_GT_I64, i64, i64, FLVScalarI64, A > B)
FLV_S_BINARY(S_CMP_GT_F64, f64, i64, FLVScalarF64, A > B)
FLV_S_BINARY(S_CMP_GE_I64, i64, i64, FLVScalarI64, A >= B)
FLV_S_BINARY(S_CMP_GE_F64, f64, i64, FLVScalarF64, A >= B)
FLV_S_BINARY(S_CMP_EQ_I64, i64, i64, FLVScalarI64, A == B)
FLV_S_BINARY(S_CMP_EQ_F64, f64, i64, FLVScalarF64, A == B)
FLV_S_BINARY(S_CMP_NE_I64, i64, i64, FLVScalarI64, A != B)
FLV_S_BINARY(S_CMP_NE_F64, f64, i64, FLVScalarF64, A != B)
FLV_S_BINARY(S_AND, i64, i64, FLVScalarI64, (A != 0) && (B != 0))
FLV_S_BINARY(S_OR,  i64, i64, FLVScalarI64, (A != 0) || (B != 0))
FLV_S_UNARY(S_NOT, i64, i64, FLVScalarI64, A == 0)
FLV_S_UNARY(S_CONVERT_I64_F64, i64, f64, FLVScalarI64, (f64) A)
 
#undef FLV_S_DIVIDE
#undef FLV_S_UNARY
#undef FLV_S_BINARY
#undef FLV_S_HEAD

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelDuration)
{
    if (FLVTempIsMissing(Lane, Op.LHS)) {
        FLVTempSetMissing(Lane, Op.Dst);
 
        return;
    }
 
    FLVSelection const* Sel = FLVSelectionPtr(Lane, Op.LHS);
    i64* Dst = (i64*) FLVTempPtr(Lane, Op.Dst);
    u64 Total = 0;
 
    for (u64 Index = 0; Index < Sel->Count; ++Index)
        Total += Sel->V[Index].Max - Sel->V[Index].Min;
 
    *Dst = (i64) Total;
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelFromMask)
{
    FLVSelectionBuild* Build = &Lane->SelectionBuilds[
        FLVGetOperandIndex(Op.Dst)
    ];

    if (FLVTempIsMissing(Lane, Op.LHS)) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    u64 const* Mask = (u64 const*) FLVTempPtr(Lane, Op.LHS);
    u64 const* Time = (u64 const*) Lane->BaseTime->Data + Lane->ChunkBase;

    for (u64 Index = 0; Index < Lane->ChunkCount; ++Index) {
        b32 Bit = FLVMaskBit(Mask, Index);

        if (Bit) {
            if (!Build->IsOpen) {
                Build->IsOpen = TRUE;
                Build->OpenStartUSecs = Time[Index];
            }

            Build->OpenEndUSecs = Time[Index];
        } else if (Build->IsOpen) {
            ASSERT(Build->Count < Build->Capacity);

            Build->V[Build->Count].Min = Build->OpenStartUSecs;
            Build->V[Build->Count].Max = Build->OpenEndUSecs;
            ++Build->Count;
            Build->IsOpen = FALSE;
        }
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelFromTimeRange)
{
    FLVSelection* Sel = FLVSelectionPtr(Lane, Op.Dst);
    u64 Min = FLVScalarTimeUSecs(Lane, Op.LHS);
    u64 EndUSecs = FLVScalarTimeUSecs(Lane, Op.RHS);

    if (EndUSecs > Lane->FlightEndUSecs)
        EndUSecs = Lane->FlightEndUSecs;

    *Sel = FLVSelectionAlloc(Lane, 1);

    if (Min <= EndUSecs) {
        Sel->V[0].Min = Min;
        Sel->V[0].Max = EndUSecs;
        Sel->Count = 1;
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelMode)
{
    FLVSelection* Sel = FLVSelectionPtr(Lane, Op.Dst);
    u32 Slot = FLVGetOperandIndex(Op.LHS);
    FLVBoundSlot const* Bound = &Lane->Slots[Slot];
    u64 ModesCount = 0;
    FLIMode* Modes = FLITableFromName(Lane->FLI, Modes, MODES, &ModesCount);

    ASSERT(FLVGetOperandKind(Op.LHS) == FLV_OPERAND_KIND_SLOT);

    if (Bound->IsMissing || Bound->ModeNumber == FLV_MODE_NUMBER_NONE) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    *Sel = FLVSelectionAlloc(Lane, ModesCount);

    for (u64 Index = 1; Index < ModesCount; ++Index) {
        if (Modes[Index].Mode != Bound->ModeNumber)
            continue;

        u64 EndUSecs = (Index + 1 < ModesCount)
            ? Modes[Index + 1].TimeUSecs
            : Lane->FlightEndUSecs;

        if (
            Sel->Count &&
            Sel->V[Sel->Count - 1].Max == Modes[Index].TimeUSecs
        ) {
            Sel->V[Sel->Count - 1].Max = EndUSecs;
        } else {
            Sel->V[Sel->Count].Min= Modes[Index].TimeUSecs;
            Sel->V[Sel->Count].Max = EndUSecs;
            ++Sel->Count;
        }
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelEvent)
{
    FLVSelection* Sel = FLVSelectionPtr(Lane, Op.Dst);
    i64 ID = FLVScalarI64(Lane, Op.LHS);
    u64 EventsCount = 0;
    FLIEvt* Events = FLITableFromName(Lane->FLI, Events, EVENTS, &EventsCount);

    *Sel = FLVSelectionAlloc(Lane, EventsCount);

    for (u64 Index = 1; Index < EventsCount; ++Index) {
        if (Events[Index].ID != (u8) ID)
            continue;

        Sel->V[Sel->Count].Min = Events[Index].TimeUSecs;
        Sel->V[Sel->Count].Max = Events[Index].TimeUSecs;
        ++Sel->Count;
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelArmed)
{
    FLVSelection* Sel = FLVSelectionPtr(Lane, Op.Dst);
    u64 EventsCount = 0;
    FLIEvt* Events = FLITableFromName(Lane->FLI, Events, EVENTS, &EventsCount);
    b32 IsArmed = FALSE;
    u64 ArmedUSecs = 0;

    *Sel = FLVSelectionAlloc(Lane, EventsCount + 1);

    for (u64 Index = 1; Index < EventsCount; ++Index) {
        if (Events[Index].ID == FLV_EVENT_ID_ARMED && !IsArmed) {
            IsArmed = TRUE;
            ArmedUSecs = Events[Index].TimeUSecs;
        } else if (Events[Index].ID == FLV_EVENT_ID_DISARMED && IsArmed) {
            Sel->V[Sel->Count].Min = ArmedUSecs;
            Sel->V[Sel->Count].Max = Events[Index].TimeUSecs;
            ++Sel->Count;
            IsArmed = FALSE;
        }
    }

    if (IsArmed) {
        Sel->V[Sel->Count].Min = ArmedUSecs;
        Sel->V[Sel->Count].Max = Lane->FlightEndUSecs;
        ++Sel->Count;
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelAnd)
{
    if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    FLVSelection const* A = FLVSelectionPtr(Lane, Op.LHS);
    FLVSelection const* B = FLVSelectionPtr(Lane, Op.RHS);
    FLVSelection* Dst = FLVSelectionPtr(Lane, Op.Dst);
    u64 IndexA = 0;
    u64 IndexB = 0;

    *Dst = FLVSelectionAlloc(Lane, A->Count + B->Count);

    while (IndexA < A->Count && IndexB < B->Count) {
        r1u64 RangeA = A->V[IndexA];
        r1u64 RangeB = B->V[IndexB];

        Dst->V[Dst->Count++] = Intersect(RangeA, RangeB);

        // u64 Min = MAX(RangeA.Min, RangeB.Min);
        // u64 EndUSecs = MIN(RangeA.Max, RangeB.Max);

        // if (Min <= EndUSecs) {
        //     Dst->V[Dst->Count].Min = Min;
        //     Dst->V[Dst->Count].Max = EndUSecs;
        //     ++Dst->Count;
        // }

        if (RangeA.Max < RangeB.Max)
            ++IndexA;
        else
            ++IndexB;
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelOr)
{
    if (FLVTempIsMissing(Lane, Op.LHS) || FLVTempIsMissing(Lane, Op.RHS)) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    FLVSelection const* A = FLVSelectionPtr(Lane, Op.LHS);
    FLVSelection const* B = FLVSelectionPtr(Lane, Op.RHS);
    FLVSelection* Dst = FLVSelectionPtr(Lane, Op.Dst);
    u64 IndexA = 0;
    u64 IndexB = 0;

    *Dst = FLVSelectionAlloc(Lane, A->Count + B->Count);

    while (IndexA < A->Count || IndexB < B->Count) {
        r1u64 Next = {};
        b32 TakeA = (
            IndexB >= B->Count ||
            (
               IndexA < A->Count && 
               A->V[IndexA].Min <= B->V[IndexB].Min
            )
        );

        if (TakeA)
            Next = A->V[IndexA++];
        else
            Next = B->V[IndexB++];

        if (
            Dst->Count && 
            Next.Min <= Dst->V[Dst->Count - 1].Max
        ) {
            Dst->V[Dst->Count - 1].Max = MAX(
                Dst->V[Dst->Count - 1].Max,
                Next.Max
            );
        } else {
            Dst->V[Dst->Count] = Next;
            ++Dst->Count;
        }
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelNot)
{
    if (FLVTempIsMissing(Lane, Op.LHS)) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    FLVSelection const* A = FLVSelectionPtr(Lane, Op.LHS);
    FLVSelection* Dst = FLVSelectionPtr(Lane, Op.Dst);
    u64 Cursor = 0;

    *Dst = FLVSelectionAlloc(Lane, A->Count + 1);

    for (u64 Index = 0; Index < A->Count; ++Index) {
        if (A->V[Index].Min > Cursor) {
            Dst->V[Dst->Count].Min = Cursor;
            Dst->V[Dst->Count].Max = A->V[Index].Min - 1;
            ++Dst->Count;
        }

        Cursor = A->V[Index].Max + 1;
    }

    if (Cursor <= Lane->FlightEndUSecs) {
        Dst->V[Dst->Count].Min = Cursor;
        Dst->V[Dst->Count].Max = Lane->FlightEndUSecs;
        ++Dst->Count;
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelEmitCurve)
{
    u32 EmitIndex = FLVGetOperandIndex(Op.RHS);
    FLVEmit const* Emit = &Lane->Program->Emits[EmitIndex];
    FLVRenderItem* Item = &Lane->Items[EmitIndex];
    FLVRenderCurve* Curve = &Item->Curve;

    if (FLVTempIsMissing(Lane, Op.LHS)) {
        Item->IsMissing = TRUE;

        return;
    }

    ASSERT(Curve->Count + Lane->ChunkCount <= Lane->RegionSampleCount);

    u64 const* Time = (u64 const*) Lane->BaseTime->Data + Lane->ChunkBase;
    f64 const* Value = (f64 const*) FLVTempPtr(Lane, Op.LHS);

    if (Lane->Mask) {
        u64 Count = Curve->Count;

        for (u64 Index = 0; Index < Lane->ChunkCount; ++Index) {
            Curve->TimeUSecs[Count] = Time[Index];
            Curve->Values[Count] = Value[Index];
            Count += FLVMaskBit(Lane->Mask, Index);
        }

        Curve->Count = Count;
    } else {
        MemCpy(
            Curve->TimeUSecs + Curve->Count,
            Time,
            Lane->ChunkCount * sizeof(u64)
        );
        MemCpy(
            Curve->Values + Curve->Count,
            Value,
            Lane->ChunkCount * sizeof(f64)
        );
        Curve->Count += Lane->ChunkCount;
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelEmitFlag)
{
    u32 EmitIndex = FLVGetOperandIndex(Op.RHS);
    FLVEmit const* Emit = &Lane->Program->Emits[EmitIndex];
    FLVRenderItem* Item = &Lane->Items[EmitIndex];
    FLVRenderFlag* Flag = &Item->Flag;

    if (FLVTempIsMissing(Lane, Op.LHS)) {
        Item->IsMissing = TRUE;

        return;
    }

    FLVSelection const* Sel = FLVSelectionPtr(Lane, Op.LHS);

    Item->Kind = FLV_RENDER_KIND_FLAG;
    Flag->Ranges = ArenaPushArray(
       Lane->ResultMemPool, 
       r1u64, 
       Sel->Count
   );
    Flag->Count = Sel->Count;
    Flag->TotalDurationUSecs = 0;
    Flag->LabelStringIndex = Emit->LabelStringIndex;
    MemCpy(Flag->Ranges, Sel->V, Sel->Count * sizeof(r1u64));

    for (u64 Index = 0; Index < Sel->Count; ++Index)
        Flag->TotalDurationUSecs += Sel->V[Index].Max - Sel->V[Index].Min;
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelEmitMissing)
{
    Lane->Items[FLVGetOperandIndex(Op.RHS)].IsMissing = TRUE;
}

INTERNAL void
FLVRegionEmitsBegin(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];

        if (Op.Kernel != FLV_KERNEL_EMIT_CURVE)
            continue;

        u32 EmitIndex = FLVGetOperandIndex(Op.RHS);
        FLVEmit const* Emit = &Lane->Program->Emits[EmitIndex];
        FLVRenderItem* Item = &Lane->Items[EmitIndex];
        FLVRenderCurve* Curve = &Item->Curve;

        Item->Kind = FLV_RENDER_KIND_CURVE;
        Curve->TimeUSecs = ArenaPushArray(
           Lane->ResultMemPool, 
           u64, 
           Lane->RegionSampleCount
       );
        Curve->Values = ArenaPushArray(
           Lane->ResultMemPool, 
           f64, 
           Lane->RegionSampleCount
        );
        Curve->Count = 0;
        Curve->LabelStringIndex = Emit->LabelStringIndex;
        Curve->Axis = Emit->Axis;
    }
}

INTERNAL void
FLVRegionSelectionBuildsBegin(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];

        if (Op.Kernel != FLV_KERNEL_SEL_FROM_MASK)
            continue;

        FLVSelectionBuild* Build = &Lane->SelectionBuilds[
            FLVGetOperandIndex(Op.Dst)
        ];

        Build->Capacity = Lane->RegionSampleCount / 2 + 1;
        Build->V = ArenaPushArray(
            Lane->MemPool, 
            r1u64, 
            Build->Capacity
        );
        Build->Count = 0;
        Build->IsOpen = FALSE;
    }
}

INTERNAL void
FLVRegionSelectionBuildsEnd(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];

        if (Op.Kernel != FLV_KERNEL_SEL_FROM_MASK)
            continue;

        FLVSelectionBuild* Build = &Lane->SelectionBuilds[
            FLVGetOperandIndex(Op.Dst)
        ];
        FLVSelection* Sel = FLVSelectionPtr(Lane, Op.Dst);

        if (Build->IsOpen) {
            ASSERT(Build->Count < Build->Capacity);

            Build->V[Build->Count].Min = Build->OpenStartUSecs;
            Build->V[Build->Count].Max = Build->OpenEndUSecs;
            ++Build->Count;
            Build->IsOpen = FALSE;
        }

        Sel->V = Build->V;
        Sel->Count = Build->Count;
    }
}

INTERNAL b32
FLVIsReduction(FLVOp Op)
{
    b32 Result = (
        (FLV_KERNEL_INFO_TABLE[Op.Kernel].Flags & FLV_KERNEL_FLAG_REDUCTION) &&
        Op.Kernel != FLV_KERNEL_SEL_FROM_MASK
    );
 
    return Result;
}
 
INTERNAL void
FLVRegionReductionsBegin(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];
 
        if (FLVIsReduction(Op)) {
            MemZero(
                &Lane->Reductions[FLVGetOperandIndex(Op.Dst)], 
                sizeof(FLVReduction)
            );
        }
    }
}
 
INTERNAL void
FLVRegionReductionsEnd(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];
 
        if (!FLVIsReduction(Op) || FLVTempIsMissing(Lane, Op.Dst))
            continue;
 
        FLVReduction const* R = &Lane->Reductions[FLVGetOperandIndex(Op.Dst)];
        FLVNumber* Dst = (FLVNumber*) FLVTempPtr(Lane, Op.Dst);
 
        if (!R->Count) {
            FLVTempSetMissing(Lane, Op.Dst);
            continue;
        }
 
        switch (Op.Kernel) {
            default: {
                ASSERT(FALSE);
            } break;
 
            case FLV_KERNEL_MEAN_I64:
            case FLV_KERNEL_MEAN_F64: {
                Dst->F64 = R->Summation.F64 / (f64) R->Count;
            } break;
 
            case FLV_KERNEL_STDDEV_I64:
            case FLV_KERNEL_STDDEV_F64: {
                f64 M = R->Summation.F64 / (f64) R->Count;
                f64 Var = R->SummationSquared / (f64) R->Count - M * M;
 
                Dst->F64 = (Var > 0.0) ? SqrtF64(Var) : 0.0;
            } break;
 
            case FLV_KERNEL_MIN_I64:
            case FLV_KERNEL_MIN_F64: {
                *Dst = R->Minimum;
            } break;
 
            case FLV_KERNEL_MAX_I64:
            case FLV_KERNEL_MAX_F64: {
                *Dst = R->Maximum;
            } break;
 
            case FLV_KERNEL_SUM_I64:
            case FLV_KERNEL_SUM_F64: {
                *Dst = R->Summation;
            } break;
 
            case FLV_KERNEL_FIRST_I64:
            case FLV_KERNEL_FIRST_F64: {
                *Dst = R->Head;
            } break;
 
            case FLV_KERNEL_LAST_I64:
            case FLV_KERNEL_LAST_F64: {
                *Dst = R->Tail;
            } break;
 
            case FLV_KERNEL_REDUCE_COUNT: {
                Dst->I64 = (i64) R->Count;
            } break;
        }
    }
}

INTERNAL u64
FLVWindowBound(FLVLane* Lane, u64 Width)
{
    u64 const* T = (u64 const*) Lane->BaseTime->Data;
    u64 Result = 1;
 
    for (u64 RangeIndex = 0; RangeIndex < Lane->RangesCount; ++RangeIndex) {
        r1u64 Range = Lane->Ranges[RangeIndex];
        u64 Front = Range.Min;
 
        for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
            while (T[Front] + Width <= T[Index])
                ++Front;
 
            Result = MAX(Result, Index - Front + 1);
        }
    }
 
    return Result;
}

INTERNAL void
FLVRegionWindowsBegin(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];
 
        if (
            !(FLV_KERNEL_INFO_TABLE[Op.Kernel].Flags & FLV_KERNEL_FLAG_ROLLING)
        ) {
            continue;
        }
 
        FLVWindow* W = &Lane->Windows[FLVGetOperandIndex(Op.Dst)];
        u64 Width = FLVScalarTimeUSecs(Lane, Op.RHS);
 
        W->Capacity = Round64NextPow2(FLVWindowBound(Lane, Width));
        W->Times = ArenaPushArray(Lane->MemPool, u64, W->Capacity);
        W->Values = ArenaPushArray(Lane->MemPool, FLVNumber, W->Capacity);
    }
}
 
INTERNAL void
FLVRangeWindowsReset(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];
 
        if (!(FLV_KERNEL_INFO_TABLE[Op.Kernel].Flags & FLV_KERNEL_FLAG_WINDOW))
            continue;
 
        FLVWindow* W = &Lane->Windows[FLVGetOperandIndex(Op.Dst)];
 
        W->Head = 0;
        W->Count = 0;
        W->Summation.I64 = 0;
        W->HasPrev = FALSE;
    }
}

void 
FLVKernelsRegionBegin(FLVLane* Lane, FLVRegion const* Region)
{
    FLVRegionSelectionBuildsBegin(Lane, Region);
    FLVRegionReductionsBegin(Lane, Region);
    FLVRegionWindowsBegin(Lane, Region);
    FLVRegionEmitsBegin(Lane, Region);
}

void 
FLVKernelsRegionEnd(FLVLane* Lane, FLVRegion const* Region)
{
    FLVRegionSelectionBuildsEnd(Lane, Region);
    FLVRegionReductionsEnd(Lane, Region);
}

void 
FLVKernelsRangeReset(FLVLane* Lane, FLVRegion const* Region)
{}



void
FLVKernelInit(void)
{
    for (u32 Index = 0; Index < FLV_KERNEL_COUNT; ++Index)
        FLV_KERNEL_TABLE[Index] = FLVKernelStub;

    FLV_KERNEL_TABLE[FLV_KERNEL_LOAD_I64] = FLVKernelLoadI64;
    FLV_KERNEL_TABLE[FLV_KERNEL_LOAD_F64] = FLVKernelLoadF64;
    FLV_KERNEL_TABLE[FLV_KERNEL_LOAD_INSTANCE] = FLVKernelLoadInstance;
    FLV_KERNEL_TABLE[FLV_KERNEL_CONVERT_I64_F64] = FLVKernelConvertI64F64;
    FLV_KERNEL_TABLE[FLV_KERNEL_NEG_I64] = FLVKernelNEG_I64;
    FLV_KERNEL_TABLE[FLV_KERNEL_NEG_F64] = FLVKernelNEG_F64;
    FLV_KERNEL_TABLE[FLV_KERNEL_ABS_I64] = FLVKernelABS_I64;
    FLV_KERNEL_TABLE[FLV_KERNEL_ABS_F64] = FLVKernelABS_F64;

#define X2(Name)                                                             \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Name##_I64_T] = FLVKernel##Name##_I64_T;   \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Name##_I64_S] = FLVKernel##Name##_I64_S;   \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Name##_F64_T] = FLVKernel##Name##_F64_T;   \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Name##_F64_S] = FLVKernel##Name##_F64_S;
#define X3(Name)                                                             \
    X2(Name)                                                                 \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Name##_I64_RS] = FLVKernel##Name##_I64_RS; \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Name##_F64_RS] = FLVKernel##Name##_F64_RS;

    X2(ADD)
    X3(SUB)
    X2(MUL)
    X3(DIV)
    X3(MOD)

#undef X3
#undef X2

#define X(Cmp)                                                           \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Cmp##_I64_T] = FLVKernel##Cmp##_I64_T; \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Cmp##_I64_S] = FLVKernel##Cmp##_I64_S; \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Cmp##_F64_T] = FLVKernel##Cmp##_F64_T; \
    FLV_KERNEL_TABLE[FLV_KERNEL_##Cmp##_F64_S] = FLVKernel##Cmp##_F64_S;

    X(CMP_LT)
    X(CMP_LE)
    X(CMP_GT)
    X(CMP_GE)
    X(CMP_EQ)
    X(CMP_NE)

#undef X

    FLV_KERNEL_TABLE[FLV_KERNEL_MASK_AND] = FLVKernelMaskAnd;
    FLV_KERNEL_TABLE[FLV_KERNEL_MASK_OR] = FLVKernelMaskOr;
    FLV_KERNEL_TABLE[FLV_KERNEL_MASK_NOT] = FLVKernelMaskNot;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_FROM_MASK] = FLVKernelSelFromMask;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_FROM_TIME_RANGE] = FLVKernelSelFromTimeRange;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_MODE] = FLVKernelSelMode;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_EVENT] = FLVKernelSelEvent;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_ARMED] = FLVKernelSelArmed;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_AND] = FLVKernelSelAnd;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_OR] = FLVKernelSelOr;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_NOT] = FLVKernelSelNot;
    FLV_KERNEL_TABLE[FLV_KERNEL_EMIT_CURVE] = FLVKernelEmitCurve;
    FLV_KERNEL_TABLE[FLV_KERNEL_EMIT_FLAG] = FLVKernelEmitFlag;
    FLV_KERNEL_TABLE[FLV_KERNEL_EMIT_MISSING] = FLVKernelEmitMissing;
    FLV_KERNEL_TABLE[FLV_KERNEL_SEL_DURATION] = FLVKernelSelDuration;
 
#define X(Name) FLV_KERNEL_TABLE[FLV_KERNEL_##Name] = FLVKernel##Name;
 
    X(MEAN_I64)
    X(MEAN_F64)
    X(STDDEV_I64)
    X(STDDEV_F64)
    X(MIN_I64)
    X(MIN_F64)
    X(MAX_I64)
    X(MAX_F64)
    X(SUM_I64)
    X(SUM_F64)
    X(FIRST_I64)
    X(FIRST_F64)
    X(LAST_I64)
    X(LAST_F64)
    X(REDUCE_COUNT)
    X(S_ADD_I64)
    X(S_ADD_F64)
    X(S_SUB_I64)
    X(S_SUB_F64)
    X(S_MUL_I64)
    X(S_MUL_F64)
    X(S_DIV_I64)
    X(S_DIV_F64)
    X(S_MOD_I64)
    X(S_MOD_F64)
    X(S_NEG_I64)
    X(S_NEG_F64)
    X(S_ABS_I64)
    X(S_ABS_F64)
    X(S_CMP_LT_I64)
    X(S_CMP_LT_F64)
    X(S_CMP_LE_I64)
    X(S_CMP_LE_F64)
    X(S_CMP_GT_I64)
    X(S_CMP_GT_F64)
    X(S_CMP_GE_I64)
    X(S_CMP_GE_F64)
    X(S_CMP_EQ_I64)
    X(S_CMP_EQ_F64)
    X(S_CMP_NE_I64)
    X(S_CMP_NE_F64)
    X(S_AND)
    X(S_OR)
    X(S_NOT)
    X(S_CONVERT_I64_F64)
 
#undef X

#define X(Name) FLV_KERNEL_TABLE[FLV_KERNEL_##Name] = FLVKernel##Name;
 
    X(DIFF_I64)
    X(DIFF_F64)
    X(RATE_I64)
    X(RATE_F64)
    X(ROLLING_MEAN_I64) 
    X(ROLLING_MEAN_F64)
    X(ROLLING_MIN_I64)
    X(ROLLING_MIN_F64)
    X(ROLLING_MAX_I64)
    X(ROLLING_MAX_F64)
 
#undef X
}
