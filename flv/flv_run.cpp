#include "nc_defines.h"
#include "nc_types.h"
#include "nc_memory.h"
#include "nc_arena.h"
#include "nc_string.h"

#include "fli.h"

#include "flv_run.h"
#include "flv_lower.h"
#include "flv_kernel.h"
#include "flv_bind.h"
#include "flv_token.h"

b32
FLVColumnInit(
    Access* Acc,
    FLIKey Key,
    FLIParsed* FLI,
    u32 ColumnIndex,
    u64 EndTimeUSecs,
    OUT FLVColumn* Column
) {
    FLIColumn* Record = FLIElementFromNameIndex(FLI, Columns, COLUMNS, ColumnIndex);
    FLISectionData Section = FLIColumnDataFromIndex(
        Acc,
        Key,
        FLI,
        ColumnIndex,
        EndTimeUSecs,
        &Column->Count
    );

    Column->Data = Section.Data;
    Column->Deltas = NULL;
    Column->Scale = Record->Multiplier;
    Column->Constant = Record->Min;
    Column->Kind = Record->Kind;
    Column->Encoding = Record->Encoding;
    Column->ElementSize = (u8) FLI_SCALAR_KIND_SIZE_TABLE[Record->Kind];
    Column->IsMissing = (
        Column->Encoding != FLI_COLUMN_ENCODING_CONSTANT &&
        !Column->Data
    );

    if (Column->Encoding == FLI_COLUMN_ENCODING_DELTA_TIME && Column->Data) {
        Column->Deltas = (u32 const*) (
            (u64 const*) Column->Data + FLITimeBlockCount(Column->Count)
        );
    }

    return Section.IsReady;
}

b32
FLVLaneInit(
    FLVLane* Lane,
    Access* Acc,
    Arena* MemPool,
    Arena* ResultMemPool,
    FLVProgram const* Program,
    FLIKey Flight,
    FLIParsed* FLI,
    FLVBoundSlot const* Slots,
    u32 SlotsCount,
    u32 FlightIndex,
    u64 EndTimeUSecs
) {
    u64 InfosCount = 0;
    FLIFileInfo* Infos = FLITableFromName(
        FLI, 
        FileInfo, 
        FILE_INFO, 
        &InfosCount
    );

    MemZero(Lane, sizeof(*Lane));
    Lane->Acc = Acc;
    Lane->Flight = Flight;
    Lane->EndTimeUSecs = EndTimeUSecs;
    Lane->Program = Program;
    Lane->Slots = Slots;
    Lane->FLI = FLI;
    Lane->SlotsCount = SlotsCount;
    Lane->FlightIndex = FlightIndex;
    Lane->MemPool = MemPool;
    Lane->ResultMemPool = ResultMemPool;
    Lane->Columns = ArenaPushArrayZero(MemPool, FLVColumn, SlotsCount);
    Lane->InstanceColumns = ArenaPushArrayZero(MemPool, FLVColumn, SlotsCount);
    Lane->TimeColumns = ArenaPushArrayZero(MemPool, FLVColumn, SlotsCount);
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
    Lane->TableRefs = ArenaPushArrayZero(
        MemPool,
        FLVTableRef,
        Program->EmitCount
    );
    Lane->EmitBuilds = ArenaPushArrayZero(
        MemPool,
        FLVEmitBuild,
        Program->EmitCount
    );

    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        FLVEmit const* Emit = &Program->Emits[Index];
        u32 Head = Index;

        if (Emit->DisplayKind != TOKEN_KIND_TABLE) {
            ++Index;
            continue;
        }

        while (
            Index < Program->EmitCount &&
            Program->Emits[Index].DisplayKind == TOKEN_KIND_TABLE &&
            Program->Emits[Index].Group == Emit->Group
        ) {
            Lane->TableRefs[Index].Head = Head;
            ++Index;
        }

        Lane->TableRefs[Head].Count = Index - Head;
        Lane->TableRefs[Head].Stamp = U64_MAX;
    }

    if (InfosCount) {
        FLIFileInfo* Info = &Infos[InfosCount - 1];

        Lane->FlightEndUSecs = Info->EndTimeUSecs - Info->StartTimeUSecs;
    }

    Lane->ChunkTimeDecode = ArenaPushArray(MemPool, u64, FLV_CHUNK_SAMPLES);

    b32 Result = TRUE;

    for (u32 Index = 0; Index < SlotsCount; ++Index) {
        FLVBoundSlot const* Slot = &Slots[Index];

        if (Slot->IsMissing)
            continue;

        if (Slot->TimeColumn != FLV_COLUMN_NONE) {
            Result &= FLVColumnInit(
                Acc, 
                Flight, 
                FLI, 
                Slot->TimeColumn, 
                EndTimeUSecs, 
                &Lane->TimeColumns[Index]
            );
        }

        if (Slot->InstanceColumn != FLV_COLUMN_NONE) {
            Result &= FLVColumnInit(
                Acc, 
                Flight, 
                FLI, 
                Slot->InstanceColumn, 
                EndTimeUSecs, 
                &Lane->InstanceColumns[Index]
            );
        }

        if (Slot->Column != FLV_COLUMN_NONE) {
            Result &= FLVColumnInit(
                Acc, 
                Flight, 
                FLI, 
                Slot->Column, 
                EndTimeUSecs, 
                &Lane->Columns[Index]
            );
        }
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
        Lane->Items[Index].Flight = Flight;
        Lane->Items[Index].EmitIndex = Index;
    }

    return Result;
}

INTERNAL FLVColumn const*
FLVBaseTimeColumn(FLVLane* Lane, u32 BaseSlot)
{
    FLVBoundSlot const* Slot = &Lane->Slots[BaseSlot];
    FLVColumn const* Result = NULL;

    if (
        !Slot->IsMissing &&
        Slot->TimeColumn != FLV_COLUMN_NONE &&
        !Lane->TimeColumns[BaseSlot].IsMissing
    ) {
        Result = &Lane->TimeColumns[BaseSlot];
    }

    return Result;
}

INTERNAL u64
FLVLowerBoundU64(FLVColumn const* Time, u64 Key)
{
    u64 Low = 0;
    u64 High = Time->Count;

    while (Low < High) {
        u64 Mid = Low + (High - Low) / 2;

        if (FLVTimeAt(Time, Mid) < Key)
            Low = Mid + 1;
        else
            High = Mid;
    }

    return Low;
}

INTERNAL u64
FLVUpperBoundU64(FLVColumn const* Time, u64 Key)
{
    u64 Low = 0;
    u64 High = Time->Count;

    while (Low <= High) {
        u64 Mid = Low + (High - Low) / 2;

        if (FLVTimeAt(Time, Mid) < Key)
            Low = Mid + 1;
        else
            High = Mid;
    }

    return Low;
}

INTERNAL void
FLVRegionRanges(FLVLane* Lane, FLVRegion const* Region)
{
    FLVColumn const* Time = Lane->BaseTime;
    u64 Count = Time->Count;

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
        u64 Head = FLVLowerBoundU64(Time, Sel->V[Index].Min);
        u64 Tail = FLVUpperBoundU64(Time, Sel->V[Index].Max);

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
FLVChunkTimeDecode(FLVLane* Lane)
{
    FLVColumn const* Time = Lane->BaseTime;
    u64 const* Data = (u64 const*) Time->Data;
    u64 Base = Lane->ChunkBase;

    Lane->ChunkTime = Data + Base;

    if (Time->Encoding == FLI_COLUMN_ENCODING_DELTA_TIME) {
        u64* Dst = Lane->ChunkTimeDecode;

        for (u64 Index = 0; Index < Lane->ChunkCount; ++Index) {
            u64 Sample = Base + Index;

            Dst[Index] = Data[Sample >> FLI_TIME_BLOCK_SHIFT] + Time->Deltas[Sample];
        }

        Lane->ChunkTime = Dst;
    }
}

INTERNAL void
FLVRegionRun(FLVLane* Lane, FLVRegion const* Region)
{
    FLVProgram const* Program = Lane->Program;

    Lane->Region = Region;
    Lane->BaseTime = FLVBaseTimeColumn(Lane, Region->BaseSlot);
    Lane->Mask = NULL;
    FLVChunkTimeDecode(Lane);

    if (!Lane->BaseTime || FLVTempIsMissing(Lane, Region->RangeSelection)) {
        FLVRegionMarkMissing(Lane, Region);

        return;
    }

    FLVRegionRanges(Lane, Region);
    FLVKernelsRegionBegin(Lane, Region);
    Lane->ChunkPosition = 0;

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

            Lane->ChunkPosition += Lane->ChunkCount;
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

FLVRunSet*
FLVRunSetInit(
    Arena* MemPool,
    FLVLowered const* Lowered,
    FLVSlot const* Slots,
    FLVSlotName const* SlotNames,
    u32 SlotsCount,
    FLIKey const* Keys,
    FLIParsed** Flights,
    u32 FlightCount
) {
    FLVRunSet* Set = ArenaPushStructZero(MemPool, FLVRunSet);
 
    Set->Programs = Lowered->Program;
    Set->Flights = Flights;
    Set->Keys = Keys;
    Set->SlotsCount = SlotsCount;
    Set->ProgramsCount = Lowered->ProgramCount;
    Set->FlightsCount = FlightCount;
    Set->ItemsCount = FlightCount * Lowered->ProgramCount;
    Set->Bound = ArenaPushArray(MemPool, FLVBoundSlot*, FlightCount);
    Set->EmitBase = ArenaPushArray(MemPool, u32, Lowered->ProgramCount + 1);
    Set->Items = ArenaPushArrayZero(MemPool, FLVRunItem, Set->ItemsCount);
 
    for (u32 Flight = 0; Flight < FlightCount; ++Flight) {
        Set->Bound[Flight] = FLVBindSlots(
            MemPool,
            Flights[Flight],
            Slots,
            SlotNames,
            SlotsCount
        );
    }
 
    Set->EmitBase[0] = 0;
 
    for (u32 Program = 0; Program < Lowered->ProgramCount; ++Program) {
        Set->EmitBase[Program + 1] = Set->EmitBase[Program] + Lowered->Program[Program].EmitCount;
    }
 
    Set->Flags = ArenaPushArrayZero(
        MemPool,
        FLVFlagSummary,
        (u64) Set->EmitBase[Lowered->ProgramCount] * FlightCount
    );
 
    for (u32 Index = 0; Index < Set->ItemsCount; ++Index)
        Set->Items[Index].ResultMemPool = ArenaAlloc();
 
    return Set;
}
 
void
FLVRunSetExecute(FLVRunSet* Set)
{
    r1u64 Range = LaneRange(Set->ItemsCount);
    TempArena Scratch = GetScratch(NULL, 0);
 
    for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
        Access* Acc = AccessOpen();
        FLVRunItem* Item = &Set->Items[Index];
        u32 Flight = (u32) (Index / Set->ProgramsCount);
        u32 Program = (u32) (Index % Set->ProgramsCount);
        FLVLane Lane = {};

        FLVLaneInit(
            &Lane,
            Acc,
            Scratch.MemPool,
            Item->ResultMemPool,
            &Set->Programs[Program],
            Set->Keys[Flight],
            Set->Flights[Flight],
            Set->Bound[Flight],
            Set->SlotsCount,
            Flight,
            U64_MAX
        );
        FLVLaneRun(&Lane);
        Item->Items = Lane.Items;
        AccessClose(Acc);
    }

    ReleaseScratch(Scratch);
}
 
void
FLVRunSetSummarize(FLVRunSet* Set)
{
    r1u64 Range = LaneRange(Set->ItemsCount);
 
    for (u64 Index = Range.Min; Index < Range.Max; ++Index) {
        FLVRunItem const* Item = &Set->Items[Index];
        u32 Flight = (u32) (Index / Set->ProgramsCount);
        u32 Program = (u32) (Index % Set->ProgramsCount);
        FLVProgram const* P = &Set->Programs[Program];
 
        for (u32 Emit = 0; Emit < P->EmitCount; ++Emit) {
            if (P->Emits[Emit].DisplayKind != TOKEN_KIND_FLAG)
                continue;
 
            FLVRenderItem const* R = &Item->Items[Emit];
            FLVFlagSummary* F = FLVFlagAt(Set, Program, Emit, Flight);
 
            F->IsMissing = R->IsMissing;
 
            if (!R->IsMissing) {
                F->RangesCount = R->Flag.Count;
                F->TotalDurationUSecs = R->Flag.TotalDurationUSecs;
                F->IsSet = R->Flag.Count > 0;
            }
        }
    }
}

Str8List
FLVRenderItemsPrint(
    Arena* MemPool, 
    Access* Acc,
    FLVProgram const* Program, 
    FLIParsed* FLI,
    FLVRenderItem* Items
) {
    Str8List Result = {};

    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (
            Program->Emits[Index].DisplayKind == TOKEN_KIND_TABLE &&
            Index && 
            Program->Emits[Index - 1].DisplayKind == TOKEN_KIND_TABLE &&
            Program->Emits[Index - 1].Group == Program->Emits[Index].Group
        ) {
            continue;
        }

        FLVRenderItem const* Item = &Items[Index];
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
                    u64 Head = FLVSeriesFirstSet(
                        Curve->Mask, 
                        Curve->SampleCount
                    );
                    u64 Tail = FLVSeriesLastSet(
                        Curve->Mask, 
                        Curve->SampleCount
                    );
                    u64 HeadSample = FLVSeriesSampleIndex(
                        Curve->Ranges, 
                        Curve->RangesCount, 
                        Head
                    );
                    u64 TailSample = FLVSeriesSampleIndex(
                        Curve->Ranges, 
                        Curve->RangesCount, 
                        Tail
                    );
                    FLVColumn Time = {};
                    FLVColumn Value = {};

                    FLVColumnInit(
                        Acc, 
                        Item->Flight, 
                        FLI, 
                        Curve->TimeColumn, 
                        U64_MAX, 
                        &Time
                    );

                    if (Curve->ValueColumn != FLV_COLUMN_NONE) {
                        FLVColumnInit(
                            Acc, 
                            Item->Flight, 
                            FLI, 
                            Curve->ValueColumn, 
                            U64_MAX, 
                            &Value
                        );
                    }

                    ListPush(
                        MemPool,
                        &Result,
                        ArenaPushStrFmt(
                            MemPool,
                            "  emit %u \"%S\": curve, %llu of %llu samples, %s, axis %u, first (%llu us, %f), last (%llu us, %f)",
                            Index,
                            PRINT_STR(Label),
                            Curve->Count,
                            Curve->SampleCount,
                            (Curve->ValueColumn != FLV_COLUMN_NONE) 
                                ? "column ref" 
                                : "f32 values",
                            Curve->Axis,
                            FLVTimeAt(&Time, HeadSample),
                            FLVCurveValueAt(&Value, Curve, Head),
                            FLVTimeAt(&Time, TailSample),
                            FLVCurveValueAt(&Value, Curve, Tail)
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

            case FLV_RENDER_KIND_TABLE: {
                FLVRenderTable const* Table = &Item->Table;
                u64 Row0 = FLVSeriesFirstSet(Table->Mask, Table->SampleCount);
                u64 Row0Sample = FLVSeriesSampleIndex(
                    Table->Ranges, 
                    Table->RangesCount, 
                    Row0
                );
 
                ListPush(
                    MemPool,
                    &Result,
                    ArenaPushStrFmt(
                        MemPool,
                        "  emit %u: table, %llu rows of %llu samples, %u columns",
                        Index,
                        Table->RowCount,
                        Table->SampleCount,
                        Table->ColumnCount
                    )
                );
 
                for (u32 C = 0; C < Table->ColumnCount; ++C) {
                    FLVRenderTableColumn const* Column = &Table->Columns[C];
                    Str8 Name = Program->Strings[Column->LabelStringIndex];
                    Str8 Cell = "-"_s8;
 
                    if (!Column->IsMissing && Table->RowCount) {
                        if (Column->Column != FLV_COLUMN_NONE) {
                            FLVColumn Ref = {};

                            FLVColumnInit(Acc, Item->Flight, FLI, Column->Column, U64_MAX, &Ref);
                            Cell = ArenaPushStrFmt(
                                MemPool, 
                                "%f", 
                                FLVColumnF64At(&Ref, Row0Sample)
                            );
                        } else {
                            u8 const* Data = (u8 const*) Column->Values;

                            if (Column->Kind == FLI_SCALAR_KIND_F64)
                                Cell = ArenaPushStrFmt(MemPool, "%f", *(f64*) Data);
                            else
                                Cell = ArenaPushStrFmt(MemPool, "%lld", *(i64*) Data);
                        }
                    }
 
                    ListPush(
                        MemPool,
                        &Result,
                        ArenaPushStrFmt(
                            MemPool,
                            "      %S: kind %u, width %u, %s, row 0 = %S",
                            PRINT_STR(Name),
                            Column->Kind,
                            Column->Width,
                            (Column->Column != FLV_COLUMN_NONE) 
                                ? "column ref" 
                                : "values",
                            PRINT_STR(Cell)
                        )
                    );
                }
            } break;
        }
    }

    return Result;
}
