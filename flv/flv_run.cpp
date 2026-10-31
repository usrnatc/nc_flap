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
        u64 Size = FLV_ELEMENT_KIND_SIZE_TABLE[Program->TempKind[Index]];

        if (Size) {
            Lane->Temps[Index] = ArenaPushSize(
                MemPool,
                Size,
                NC_SIMD_ALIGN256,
                TRUE
            );
        }
    }

    u64* MaskAll = ArenaPushArray(MemPool, u64, FLV_CHUNK_MASK_WORDS);

    MemSet(MaskAll, 0xFF, FLV_CHUNK_MASK_WORDS * sizeof(u64));
    Lane->MaskAll = MaskAll;

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
        Lane->Ranges = ArenaPushArray(Lane->MemPool, r1u64, 1);
        Lane->Ranges[0].Min = 0;
        Lane->Ranges[0].Max = Count;
        Lane->RangesCount = 1;
        Lane->RegionSampleCount = Count;

        return;
    }

    FLVSelection const* Sel = FLVSelectionPtr(Lane, Region->RangeSelection);

    Lane->Ranges = ArenaPushArray(Lane->MemPool, r1u64, Sel->Count);

    for (u64 Index = 0; Index < Sel->Count; ++Index) {
        u64 Head = FLVLowerBoundU64(Time, Count, Sel->V[Index].Min);
        u64 Tail = FLVUpperBoundU64(Time, Count, Sel->V[Index].Max);

        if (Tail <= Head)
            continue;

        Lane->Ranges[Lane->RangesCount].Min = Head;
        Lane->Ranges[Lane->RangesCount].Max = Tail;
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
    FLVKernelsRegionBegin(Lane, Region);

    for (u64 RangeIndex = 0; RangeIndex < Lane->RangesCount; ++RangeIndex) {
        r1u64 Range = Lane->Ranges[RangeIndex];

        FLVKernelsRangeReset(Lane, Region);

        for (
            u64 Base = Range.Min;
            Base < Range.Max;
            Base += FLV_CHUNK_SAMPLES
        ) {
            Lane->ChunkBase = Base;
            Lane->ChunkCount = MIN(FLV_CHUNK_SAMPLES, Range.Max - Base);
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
    FLVKernelsRegionEnd(Lane, Region);
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
                            Flag->Ranges[RangeIndex].Min,
                            Flag->Ranges[RangeIndex].Max
                        )
                    );
                }
            } break;
        }
    }

    return Result;
}
