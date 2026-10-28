#include "flv_stdlib.h"
#include "nc_defines.h"
#include "nc_types.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_string.h"
#include "nc_log.h"
#include "fli.h"
#include "flv_run.h"
#include "flv_lower.h"
#include "flv_kernel.h"
#include "flv_bind.h"

FLVKernelFuncType* FLV_KERNEL_TABLE[FLV_KERNEL_COUNT] = {};

// @operands___________________________________________________________________
INTERNAL f64
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

INTERNAL i64
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

INTERNAL FLVSelection*
FLVSelectionPtr(FLVLane* Lane, FLVOperand Operand)
{
    return (FLVSelection*) FLVTempPtr(Lane, Operand);
}

INTERNAL FLVSelection
FLVSelectionAlloc(FLVLane* Lane, u64 Capacity)
{
    FLVSelection Result = {};

    Result.V = ArenaPushArray(Lane->MemPool, FLVTimeRange, Capacity);
    Result.Count = 0;

    return Result;
}

// @kernels____________________________________________________________________
INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelStub)
{
    LogError(
        "kernel %S is not implemented yet",
        PRINT_STR(FLV_KERNEL_NAME_TABLE[Op.Kernel])
    );
    FLVTempSetMissing(Lane, Op.Dst);
}

#define FLV_LOAD_LOOP(SrcType, DstType)                            \
    do {                                                           \
        SrcType const* Src = (SrcType const*) Column->Data + Base; \
        DstType*       Dst = (DstType*) Out;                       \
                                                                   \
        for (u64 Index = 0; Index < Count; ++Index)                \
            Dst[Index] = (DstType) Src[Index];                     \
    } while (FALSE)

INTERNAL void
FLVLoadChunk(FLVLane* Lane, FLVOp Op, b32 ToF64)
{
    u32 Slot = FLVGetOperandIndex(Op.LHS);
    FLVColumn const* Column = &Lane->Columns[Slot];
    void* Out = FLVTempPtr(Lane, Op.Dst);
    u64 Base = Lane->ChunkBase;
    u64 Count = Lane->ChunkCount;

    ASSERT(FLVGetOperandKind(Op.LHS) == FLV_OPERAND_KIND_SLOT);

    if (!Column->Data) {
        FLVTempSetMissing(Lane, Op.Dst);

        return;
    }

    if (ToF64) {
        switch (Column->Kind) {
            default: {
                FLVTempSetMissing(Lane, Op.Dst);
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
                FLVTempSetMissing(Lane, Op.Dst);
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
    FLVLoadChunk(Lane, Op, FALSE);
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelLoadF64)
{
    FLVLoadChunk(Lane, Op, TRUE);
}

// NOTE(nc): compares write one bit per sample. Words are cleared first so the
//         : bits past ChunkCount are zero; mask consumers rely on that.
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
        u64*        Mask = (u64*) FLVTempPtr(Lane, Op.Dst);                      \
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
        Type        B = ScalarFunc(Lane, Op.RHS);                               \
        u64*        Mask = (u64*) FLVTempPtr(Lane, Op.Dst);                     \
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
    u64*       Dst = (u64*) FLVTempPtr(Lane, Op.Dst);

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

// NOTE(nc): NOT must not set bits past ChunkCount
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

// NOTE(nc): a SEL_FROM_MASK op accumulates runs of set bits across every chunk
//         : of its region. The run start and end are the times of the first and
//         : last true sample, so the range is exact on this base and converts to
//         : any other base by time.
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

            Build->V[Build->Count].StartUSecs = Build->OpenStartUSecs;
            Build->V[Build->Count].EndUSecs = Build->OpenEndUSecs;
            ++Build->Count;
            Build->IsOpen = FALSE;
        }
    }
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelSelFromTimeRange)
{
    FLVSelection* Sel = FLVSelectionPtr(Lane, Op.Dst);
    u64 StartUSecs = FLVScalarTimeUSecs(Lane, Op.LHS);
    u64 EndUSecs = FLVScalarTimeUSecs(Lane, Op.RHS);

    if (EndUSecs > Lane->FlightEndUSecs)
        EndUSecs = Lane->FlightEndUSecs;

    *Sel = FLVSelectionAlloc(Lane, 1);

    if (StartUSecs <= EndUSecs) {
        Sel->V[0].StartUSecs = StartUSecs;
        Sel->V[0].EndUSecs = EndUSecs;
        Sel->Count = 1;
    }
}

// NOTE(nc): a mode holds from its MODE record until the next MODE record, or
//         : the end of the flight. Adjacent records of one mode merge.
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
            Sel->V[Sel->Count - 1].EndUSecs == Modes[Index].TimeUSecs
        ) {
            Sel->V[Sel->Count - 1].EndUSecs = EndUSecs;
        } else {
            Sel->V[Sel->Count].StartUSecs = Modes[Index].TimeUSecs;
            Sel->V[Sel->Count].EndUSecs = EndUSecs;
            ++Sel->Count;
        }
    }
}

// NOTE(nc): an event is a point in time; its selection is a zero-length range
//         : per occurrence. Useful for flags and markers, empty as a filter.
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

        Sel->V[Sel->Count].StartUSecs = Events[Index].TimeUSecs;
        Sel->V[Sel->Count].EndUSecs = Events[Index].TimeUSecs;
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
            Sel->V[Sel->Count].StartUSecs = ArmedUSecs;
            Sel->V[Sel->Count].EndUSecs = Events[Index].TimeUSecs;
            ++Sel->Count;
            IsArmed = FALSE;
        }
    }

    if (IsArmed) {
        Sel->V[Sel->Count].StartUSecs = ArmedUSecs;
        Sel->V[Sel->Count].EndUSecs = Lane->FlightEndUSecs;
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
        FLVTimeRange RangeA = A->V[IndexA];
        FLVTimeRange RangeB = B->V[IndexB];
        u64 StartUSecs = MAX(RangeA.StartUSecs, RangeB.StartUSecs);
        u64 EndUSecs = MIN(RangeA.EndUSecs, RangeB.EndUSecs);

        if (StartUSecs <= EndUSecs) {
            Dst->V[Dst->Count].StartUSecs = StartUSecs;
            Dst->V[Dst->Count].EndUSecs = EndUSecs;
            ++Dst->Count;
        }

        if (RangeA.EndUSecs < RangeB.EndUSecs)
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
        FLVTimeRange Next = {};
        b32 TakeA = (
            IndexB >= B->Count ||
            (IndexA < A->Count && A->V[IndexA].StartUSecs <= B->V[IndexB].StartUSecs)
        );

        if (TakeA)
            Next = A->V[IndexA++];
        else
            Next = B->V[IndexB++];

        if (Dst->Count && Next.StartUSecs <= Dst->V[Dst->Count - 1].EndUSecs) {
            Dst->V[Dst->Count - 1].EndUSecs = MAX(
                Dst->V[Dst->Count - 1].EndUSecs,
                Next.EndUSecs
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
        if (A->V[Index].StartUSecs > Cursor) {
            Dst->V[Dst->Count].StartUSecs = Cursor;
            Dst->V[Dst->Count].EndUSecs = A->V[Index].StartUSecs - 1;
            ++Dst->Count;
        }

        Cursor = A->V[Index].EndUSecs + 1;
    }

    if (Cursor <= Lane->FlightEndUSecs) {
        Dst->V[Dst->Count].StartUSecs = Cursor;
        Dst->V[Dst->Count].EndUSecs = Lane->FlightEndUSecs;
        ++Dst->Count;
    }
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
        Curve->Values = ArenaPushArray(Lane->ResultMemPool, f64, Lane->RegionSampleCount);
        Curve->Count = 0;
        Curve->LabelStringIndex = Emit->LabelStringIndex;
        Curve->Axis = Emit->Axis;
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
    Flag->Ranges = ArenaPushArray(Lane->ResultMemPool, FLVTimeRange, Sel->Count);
    Flag->Count = Sel->Count;
    Flag->TotalDurationUSecs = 0;
    Flag->LabelStringIndex = Emit->LabelStringIndex;
    MemCpy(Flag->Ranges, Sel->V, Sel->Count * sizeof(FLVTimeRange));

    for (u64 Index = 0; Index < Sel->Count; ++Index)
        Flag->TotalDurationUSecs += Sel->V[Index].EndUSecs - Sel->V[Index].StartUSecs;
}

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelEmitMissing)
{
    Lane->Items[FLVGetOperandIndex(Op.RHS)].IsMissing = TRUE;
}

void
FLVKernelInit(void)
{
    for (u32 Index = 0; Index < FLV_KERNEL_COUNT; ++Index)
        FLV_KERNEL_TABLE[Index] = FLVKernelStub;

    FLV_KERNEL_TABLE[FLV_KERNEL_LOAD_I64] = FLVKernelLoadI64;
    FLV_KERNEL_TABLE[FLV_KERNEL_LOAD_F64] = FLVKernelLoadF64;

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
}

// @lane_______________________________________________________________________
INTERNAL u64
FLVTempSize(u8 Kind)
{
    u64 Result = 0;

    switch (Kind) {
        default: {} break;

        case FLV_ELEMENT_KIND_I64:
        case FLV_ELEMENT_KIND_F64: {
            Result = FLV_CHUNK_SAMPLES * sizeof(u64);
        } break;

        case FLV_ELEMENT_KIND_MASK: {
            Result = FLV_CHUNK_MASK_WORDS * sizeof(u64);
        } break;

        case FLV_ELEMENT_KIND_SINGLE_I64:
        case FLV_ELEMENT_KIND_SINGLE_F64: {
            Result = sizeof(u64);
        } break;

        case FLV_ELEMENT_KIND_SELECTION: {
            Result = sizeof(FLVSelection);
        } break;
    }

    return Result;
}

void
FLVLaneInit(
    FLVLane* Lane,
    Arena* MemPool,
    Arena* ResultMemPool,
    FLVProgram const* Program,
    FLIParsed* FLI,
    FLVBoundSlot const* Slots,
    u32 SlotsCount,
    u32 FlightIndex
) {
    u64 InfosCount = 0;
    FLIFileInfo* Infos = FLITableFromName(FLI, FileInfo, FILE_INFO, &InfosCount);

    MemZero(Lane, sizeof(*Lane));
    Lane->Program = Program;
    Lane->Slots = Slots;
    Lane->FLI = FLI;
    Lane->SlotsCount = SlotsCount;
    Lane->FlightIndex = FlightIndex;
    Lane->MemPool = MemPool;
    Lane->ResultMemPool = ResultMemPool;
    Lane->Columns = ArenaPushArrayZero(MemPool, FLVColumn, SlotsCount);
    Lane->Temps = ArenaPushArrayZero(MemPool, void*, Program->TempCount);
    Lane->SelectionBuilds = ArenaPushArrayZero(
        MemPool,
        FLVSelectionBuild,
        Program->TempCount
    );
    Lane->Items = ArenaPushArrayZero(
        ResultMemPool,
        FLVRenderItem,
        Program->EmitCount
    );

    if (InfosCount > 1) {
        FLIFileInfo* Info = &Infos[InfosCount - 1];

        Lane->FlightEndUSecs = Info->EndTimeUSecs - Info->StartTimeUSecs;
    }

    for (u32 Index = 0; Index < SlotsCount; ++Index) {
        FLVBoundSlot const* Slot = &Slots[Index];
        FLVColumn* Column = &Lane->Columns[Index];

        if (Slot->IsMissing || Slot->Column == FLV_COLUMN_NONE)
            continue;

        Column->Data = FLIColumnRawDataFromIndex(
            FLI,
            Slot->Column,
            &Column->Count
        );
        Column->Kind = Slot->Kind;
        Column->ElementSize = (u8) FLI_SCALAR_KIND_SIZE_TABLE[Slot->Kind];
    }

    for (u32 Index = 0; Index < Program->TempCount; ++Index) {
        u64 Size = FLVTempSize(Program->TempKind[Index]);

        if (Size) {
            Lane->Temps[Index] = ArenaPushSize(
                MemPool,
                Size,
                NC_SIMD_ALIGN256,
                TRUE
            );
        }
    }

    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        Lane->Items[Index].AnalysisIndex = Program->AnalysisIndex;
        Lane->Items[Index].FlightIndex = FlightIndex;
        Lane->Items[Index].EmitIndex = Index;
    }
}

INTERNAL FLVColumn const*
FLVBaseTimeColumn(FLVLane* Lane, u32 BaseSlot, OUT FLVColumn* Storage)
{
    FLVBoundSlot const* Slot = &Lane->Slots[BaseSlot];
    FLVColumn const* Result = NULL;

    if (!Slot->IsMissing && Slot->TimeColumn != FLV_COLUMN_NONE) {
        Storage->Data = FLIColumnRawDataFromIndex(
            Lane->FLI,
            Slot->TimeColumn,
            &Storage->Count
        );
        Storage->Kind = FLI_SCALAR_KIND_U64;
        Storage->ElementSize = sizeof(u64);
        Result = Storage;
    }

    return Result;
}

INTERNAL u64
FLVLowerBoundU64(u64 const* V, u64 Count, u64 Key)
{
    u64 Low = 0;
    u64 High = Count;

    while (Low < High) {
        u64 Mid = Low + (High - Low) / 2;

        if (V[Mid] < Key)
            Low = Mid + 1;
        else
            High = Mid;
    }

    return Low;
}

INTERNAL u64
FLVUpperBoundU64(u64 const* V, u64 Count, u64 Key)
{
    u64 Low = 0;
    u64 High = Count;

    while (Low < High) {
        u64 Mid = Low + (High - Low) / 2;

        if (V[Mid] <= Key)
            Low = Mid + 1;
        else
            High = Mid;
    }

    return Low;
}

// NOTE(nc): time ranges -> index ranges on this region's base. No selection
//         : means the whole column.
INTERNAL void
FLVRegionRanges(FLVLane* Lane, FLVRegion const* Region)
{
    u64 const* Time = (u64 const*) Lane->BaseTime->Data;
    u64 Count = Lane->BaseTime->Count;

    Lane->RangesCount = 0;
    Lane->RegionSampleCount = 0;

    if (Region->RangeSelection == FLV_OPERAND_NONE) {
        Lane->Ranges = ArenaPushArray(Lane->MemPool, FLVIndexRange, 1);
        Lane->Ranges[0].Head = 0;
        Lane->Ranges[0].Tail = Count;
        Lane->RangesCount = 1;
        Lane->RegionSampleCount = Count;

        return;
    }

    FLVSelection const* Sel = FLVSelectionPtr(Lane, Region->RangeSelection);

    Lane->Ranges = ArenaPushArray(Lane->MemPool, FLVIndexRange, Sel->Count);

    for (u64 Index = 0; Index < Sel->Count; ++Index) {
        u64 Head = FLVLowerBoundU64(Time, Count, Sel->V[Index].StartUSecs);
        u64 Tail = FLVUpperBoundU64(Time, Count, Sel->V[Index].EndUSecs);

        if (Tail <= Head)
            continue;

        Lane->Ranges[Lane->RangesCount].Head = Head;
        Lane->Ranges[Lane->RangesCount].Tail = Tail;
        Lane->RegionSampleCount += Tail - Head;
        ++Lane->RangesCount;
    }
}

// NOTE(nc): every SEL_FROM_MASK in the region gets room for the worst case, one
//         : range per two samples. The arena only commits what is touched.
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
        Build->V = ArenaPushArray(Lane->MemPool, FLVTimeRange, Build->Capacity);
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

            Build->V[Build->Count].StartUSecs = Build->OpenStartUSecs;
            Build->V[Build->Count].EndUSecs = Build->OpenEndUSecs;
            ++Build->Count;
            Build->IsOpen = FALSE;
        }

        Sel->V = Build->V;
        Sel->Count = Build->Count;
    }
}

INTERNAL void
FLVRegionMarkMissing(FLVLane* Lane, FLVRegion const* Region)
{
    for (u32 Index = 0; Index < Region->OpCount; ++Index) {
        FLVOp Op = Lane->Program->Ops[Region->HeadOp + Index];
        FLVKernelInfo const* Info = &FLV_KERNEL_INFO_TABLE[Op.Kernel];

        if (Info->Flags & FLV_KERNEL_FLAG_EMIT)
            Lane->Items[FLVGetOperandIndex(Op.RHS)].IsMissing = TRUE;
        else
            FLVTempSetMissing(Lane, Op.Dst);
    }
}

INTERNAL void
FLVRegionRun(FLVLane* Lane, FLVRegion const* Region)
{
    FLVColumn TimeStorage = {};
    FLVProgram const* Program = Lane->Program;

    Lane->Region = Region;
    Lane->BaseTime = FLVBaseTimeColumn(Lane, Region->BaseSlot, &TimeStorage);
    Lane->Mask = NULL;

    if (!Lane->BaseTime || FLVTempIsMissing(Lane, Region->RangeSelection)) {
        FLVRegionMarkMissing(Lane, Region);

        return;
    }

    FLVRegionRanges(Lane, Region);
    FLVRegionSelectionBuildsBegin(Lane, Region);
   FLVRegionEmitsBegin(Lane, Region);

    for (u64 RangeIndex = 0; RangeIndex < Lane->RangesCount; ++RangeIndex) {
        FLVIndexRange Range = Lane->Ranges[RangeIndex];

        // TODO(nc): 8e: reset window kernel state here, once per range

        for (
            u64 Base = Range.Head;
            Base < Range.Tail;
            Base += FLV_CHUNK_SAMPLES
        ) {
            Lane->ChunkBase = Base;
            Lane->ChunkCount = MIN(FLV_CHUNK_SAMPLES, Range.Tail - Base);
            Lane->Mask = NULL;

            for (u32 OpIndex = 0; OpIndex < Region->OpCount; ++OpIndex) {
                FLVOp Op = Program->Ops[Region->HeadOp + OpIndex];

                FLV_KERNEL_TABLE[Op.Kernel](Lane, Op);

                // NOTE(nc): the region mask is a temp produced inside this region;
                //         : once its op has run, the consumers after it see it
                if (
                    Op.Dst != FLV_OPERAND_NONE &&
                    Op.Dst == Region->Mask &&
                    !FLVTempIsMissing(Lane, Op.Dst)
                ) {
                    Lane->Mask = (u64 const*) FLVTempPtr(Lane, Op.Dst);
                }
            }
        }
    }

    FLVRegionSelectionBuildsEnd(Lane, Region);
    Lane->Region = NULL;
    Lane->BaseTime = NULL;
    Lane->Mask = NULL;
}

INTERNAL void
FLVRegionScalar(FLVLane* Lane, FLVRegion const* Region)
{
    FLVProgram const* Program = Lane->Program;

    Lane->Region = Region;
    Lane->ChunkBase = 0;
    Lane->ChunkCount = 0;

    for (u32 OpIndex = 0; OpIndex < Region->OpCount; ++OpIndex) {
        FLVOp Op = Program->Ops[Region->HeadOp + OpIndex];

        FLV_KERNEL_TABLE[Op.Kernel](Lane, Op);
    }

    Lane->Region = NULL;
}

void
FLVLaneRun(FLVLane* Lane)
{
    FLVProgram const* Program = Lane->Program;

    Lane->MissingTemps = 0;

    for (u32 Index = 0; Index < Program->RegionCount; ) {
        FLVRegion const* Region = &Program->Regions[Index];

        switch (Region->Kind) {
            default: {
                ASSERT(FALSE);
                ++Index;
            } break;

            case FLV_REGION_KIND_RUN: {
                FLVRegionRun(Lane, Region);
                ++Index;
            } break;

            case FLV_REGION_KIND_SCALAR: {
                FLVRegionScalar(Lane, Region);
                ++Index;
            } break;

            // TODO(nc): 8d
            case FLV_REGION_KIND_JUMP_IF_FALSE:
            case FLV_REGION_KIND_JUMP: {
                LogError("jump regions are not implemented yet");
                ++Index;
            } break;
        }
    }
}

// @print______________________________________________________________________
Str8List
FLVRenderItemsPrint(Arena* MemPool, FLVLane const* Lane)
{
    Str8List Result = {};
    FLVProgram const* Program = Lane->Program;

    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        FLVRenderItem const* Item = &Lane->Items[Index];
        Str8 Label = Program->Strings[Program->Emits[Index].LabelStringIndex];

        if (Item->IsMissing) {
            ListPush(
                MemPool,
                &Result,
                ArenaPushStrFmt(
                    MemPool,
                    "  emit %u \"%S\": missing",
                    Index,
                    PRINT_STR(Label)
                )
            );
            continue;
        }

        switch (Item->Kind) {
            default: {
                ListPush(
                    MemPool,
                    &Result,
                    ArenaPushStrFmt(
                        MemPool,
                        "  emit %u \"%S\": not produced",
                        Index,
                        PRINT_STR(Label)
                    )
                );
            } break;

            case FLV_RENDER_KIND_CURVE: {
                FLVRenderCurve const* Curve = &Item->Curve;

                if (Curve->Count) {
                    ListPush(
                        MemPool,
                        &Result,
                        ArenaPushStrFmt(
                            MemPool,
                            "  emit %u \"%S\": curve, %llu samples, axis %u, first (%llu us, %f), last (%llu us, %f)",
                            Index,
                            PRINT_STR(Label),
                            Curve->Count,
                            Curve->Axis,
                            Curve->TimeUSecs[0],
                            Curve->Values[0],
                            Curve->TimeUSecs[Curve->Count - 1],
                            Curve->Values[Curve->Count - 1]
                        )
                    );
                } else {
                    ListPush(
                        MemPool,
                        &Result,
                        ArenaPushStrFmt(
                            MemPool,
                            "  emit %u \"%S\": curve, 0 samples",
                            Index,
                            PRINT_STR(Label)
                        )
                    );
                }
            } break;

            case FLV_RENDER_KIND_FLAG: {
                FLVRenderFlag const* Flag = &Item->Flag;

                ListPush(
                    MemPool,
                    &Result,
                    ArenaPushStrFmt(
                        MemPool,
                        "  emit %u \"%S\": flag, %llu ranges, %llu us total",
                        Index,
                        PRINT_STR(Label),
                        Flag->Count,
                        Flag->TotalDurationUSecs
                    )
                );

                for (
                    u64 RangeIndex = 0;
                    RangeIndex < MIN(Flag->Count, 5);
                    ++RangeIndex
                ) {
                    ListPush(
                        MemPool,
                        &Result,
                        ArenaPushStrFmt(
                            MemPool,
                            "      [%llu us .. %llu us]",
                            Flag->Ranges[RangeIndex].StartUSecs,
                            Flag->Ranges[RangeIndex].EndUSecs
                        )
                    );
                }
            } break;
        }
    }

    return Result;
}
