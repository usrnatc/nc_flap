#if !defined(__FLV_RUN_H__)
#define __FLV_RUN_H__

#include "nc_types.h"
#include "nc_string.h"

#include "fli.h"

#include "flv_kernel.h"
#include "flv_lower.h"
#include "flv_bind.h"
#include "flv_compiler.h"
#include "flv_visualisation.h"

struct Arena;

// @defines____________________________________________________________________
#define FLV_CHUNK_SAMPLES    4096
#define FLV_CHUNK_MASK_WORDS (FLV_CHUNK_SAMPLES / 64)
#define FLV_I64_HOLE         I64_MAX

// @types______________________________________________________________________
struct FLVSelection {
    r1u64* V;
    u64    Count;
};

struct FLVTableRef {
    u32 Head;
    u32 Count;
    u64 Stamp;
    u64 RowBase;
};

struct FLVFlagSummary {
    u64 RangesCount;
    u64 TotalDurationUSecs;
    u8  IsMissing;
    u8  IsSet;
    u8  __Padding[6];
};

struct FLVRunItem {
    Arena*         ResultMemPool;
    FLVRenderItem* Items;
};

struct FLVRunSet {
    FLVProgram const* Programs;
    FLIParsed**       Flights;
    FLIKey const*     Keys;
    FLVBoundSlot**    Bound;
    u32*              EmitBase;
    FLVRunItem*       Items;
    FLVFlagSummary*   Flags;
    u32               SlotsCount;
    u32               ProgramsCount;
    u32               FlightsCount;
    u32               ItemsCount;
};

struct FLVLane {
    Access*             Acc;
    FLVProgram const*   Program;
    FLVBoundSlot const* Slots;
    FLIParsed*          FLI;
    FLIKey              Flight;
    u32                 SlotsCount;
    u32                 FlightIndex;
    u64                 FlightEndUSecs;
    u64                 EndTimeUSecs;
    FLVColumn*          TimeColumns;
    Arena*              MemPool;
    Arena*              ResultMemPool;
    FLVColumn*          Columns;
    FLVColumn*          InstanceColumns;
    void**              Temps;
    FLVSelectionBuild*  SelectionBuilds;
    FLVReduction*       Reductions;
    FLVWindow*          Windows;
    u64                 MissingTemps;
    FLVRenderItem*      Items;
    FLVTableRef*        TableRefs;
    FLVEmitBuild*       EmitBuilds;
    FLVRegion const*    Region;
    FLVColumn const*    BaseTime;
    u64 const*          Mask;
    u64 const*          MaskAll;
    r1u64*              Ranges;
    u64                 RangesCount;
    u64                 RegionSampleCount;
    u64 const*          ChunkTime;
    u64*                ChunkTimeDecode;
    u64                 ChunkBase;
    u64                 ChunkCount;
    u64                 ChunkPosition;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
b32 FLVLaneInit(FLVLane* Lane, Access* Acc, Arena* MemPool, Arena* ResultMemPool, FLVProgram const* Program, FLIKey Flight, FLIParsed* FLI, FLVBoundSlot const* Slots, u32 SlotsCount, u32 FlightIndex, u64 EndTimeUSecs);
void FLVLaneRun(FLVLane* Lane);
Str8List FLVRenderItemsPrint(Arena* MemPool, FLVProgram const* Program, FLIParsed* FLI, FLVRunItem* Items);
FLVRunSet* FLVRunSetInit(Arena* MemPool, FLVLowered const* Lowered, FLVSlot const* Slots, FLVSlotName const* SlotNames, u32 SlotsCount, FLIKey const* Keys, FLIParsed** Flights, u32 FlightsCount);
void FLVRunSetExecute(FLVRunSet* Set);
void FLVRunSetSummarise(FLVRunSet* Set);

INTERNAL FLVRunItem*
FLVRunItemAt(FLVRunSet* Set, u32 Flight, u32 Program)
{
    FLVRunItem* Result = &Set->Items[Flight * Set->ProgramsCount + Program];

    return Result;
}

INTERNAL FLVFlagSummary*
FLVFlagAt(FLVRunSet* Set, u32 Program, u32 Emit, u32 Flight)
{
    FLVFlagSummary* Result = &Set->Flags[
        (Set->EmitBase[Program] + Emit) * Set->FlightsCount + Flight
    ];

    return Result;
}

INTERNAL void*
FLVTempPtr(FLVLane* Lane, FLVOperand Operand)
{
    ASSERT(FLVGetOperandKind(Operand) == FLV_OPERAND_KIND_TEMP);

    return Lane->Temps[FLVGetOperandIndex(Operand)];
}

INTERNAL FLVSelection*
FLVSelectionPtr(FLVLane* Lane, FLVOperand Operand)
{
    return (FLVSelection*) FLVTempPtr(Lane, Operand);
}

INTERNAL b32
FLVTempIsMissing(FLVLane const* Lane, FLVOperand Operand)
{
    b32 Result = FALSE;

    if (FLVGetOperandKind(Operand) == FLV_OPERAND_KIND_TEMP)
        Result = (Lane->MissingTemps >> FLVGetOperandIndex(Operand)) & 1;

    return Result;
}

INTERNAL void
FLVTempSetMissing(FLVLane* Lane, FLVOperand Operand)
{
    if (FLVGetOperandKind(Operand) == FLV_OPERAND_KIND_TEMP)
        Lane->MissingTemps |= 1ULL << FLVGetOperandIndex(Operand);
}

INTERNAL b32
FLVMaskBit(u64 const* Mask, u64 Index)
{
    return (Mask[Index >> 6] >> (Index & 63)) & 1;
}

#endif // __FLV_RUN_H__
