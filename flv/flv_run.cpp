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

// FIXME(nc): this is fucking stupid to have taking up space in the binary
global u64 const FLV_MASK_ALL[FLV_CHUNK_MASK_WORDS] = {
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX, 
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX, 
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX, 
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX, 
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX, 
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX, 
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX, 
    U64_MAX, U64_MAX, U64_MAX, U64_MAX,
    U64_MAX, U64_MAX, U64_MAX, U64_MAX
};

INTERNAL
FLV_KERNEL_FUNC_SIG(FLVKernelStub)
{
    LogError(
        "kernel %S is not implemented yet",
        PRINT_STR(FLV_KERNEL_NAME_TABLE[Op.Kernel])
    );
    FLVTempSetMissing(Lane, Op.Dst);
}

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
    FLIFileInfo* Infos = FLITableFromName(
        FLI, 
        FileInfo, 
        FILE_INFO, 
        &InfosCount
    );

    MemZero(Lane, sizeof(*Lane));
    Lane->Program = Program;
    Lane->Slots = Slots;
    Lane->FLI = FLI;
    Lane->SlotsCount = SlotsCount;
    Lane->FlightIndex = FlightIndex;
    Lane->MemPool = MemPool;
    Lane->ResultMemPool = ResultMemPool;
    Lane->Columns = ArenaPushArrayZero(MemPool, FLVColumn, SlotsCount);
    Lane->InstanceColumns = ArenaPushArrayZero(MemPool, FLVColumn, SlotsCount);
    Lane->Temps = ArenaPushArrayZero(MemPool, void*, Program->TempCount);
    Lane->SelectionBuilds = ArenaPushArrayZero(
        MemPool,
        FLVSelectionBuild,
        Program->TempCount
    );
    Lane->Reductions = ArenaPushArrayZero(
        MemPool,
        FLVReduction,
        Program->TempCount
    );
    Lane->Windows = ArenaPushArrayZero(
        MemPool,
        FLVWindow,
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
        FLVColumn* Instance = &Lane->InstanceColumns[Index];
 
        if (Slot->IsMissing)
            continue;
 
        if (Slot->InstanceColumn != FLV_COLUMN_NONE) {
            FLIColumn* Record = FLIElementFromNameIndex(
                FLI,
                Columns,
                COLUMNS,
                Slot->InstanceColumn
            );
 
            Instance->Data = FLIColumnRawDataFromIndex(
                FLI,
                Slot->InstanceColumn,
                &Instance->Count
            );
            Instance->Kind = Record->Kind;
            Instance->ElementSize = (u8) FLI_SCALAR_KIND_SIZE_TABLE[Record->Kind];
        }
 
        if (Slot->Column == FLV_COLUMN_NONE)
            continue;
 
        Column->Data = FLIColumnRawDataFromIndex(FLI, Slot->Column, &Column->Count);
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
    FLVRegionReductionsBegin(Lane, Region);
    FLVRegionWindowsBegin(Lane, Region);
    FLVRegionEmitsBegin(Lane, Region);

    for (u64 RangeIndex = 0; RangeIndex < Lane->RangesCount; ++RangeIndex) {
        FLVIndexRange Range = Lane->Ranges[RangeIndex];

        FLVRangeWindowsReset(Lane, Region);

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
    FLVRegionReductionsEnd(Lane, Region);
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
    u32 Index = 0;
 
    Lane->MissingTemps = 0;
 
    while (Index < Program->RegionCount) {
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
 
            case FLV_REGION_KIND_JUMP_IF_FALSE: {
                b32 IsFalse = (
                    FLVTempIsMissing(Lane, Region->Operand) ||
                    FLVScalarI64(Lane, Region->Operand) == 0
                );
 
                Index = IsFalse 
                    ? Region->Target 
                    : Index + 1;
            } break;
 
            case FLV_REGION_KIND_JUMP: {
                Index = Region->Target;
            } break;
        }
    }
}

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
