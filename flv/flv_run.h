#if !defined(__FLV_RUN_H__)
#define __FLV_RUN_H__

#include "nc_types.h"
#include "nc_string.h"
#include "fli.h"
#include "flv_lower.h"
#include "flv_bind.h"
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

struct FLVColumn {
    void*         Data;
    u64           Count;
    FLIScalarKind Kind;
    u8            ElementSize;
    u8            __Padding[6];
};

struct FLVLane {
    FLVProgram const*   Program;
    FLVBoundSlot const* Slots;
    FLIParsed*          FLI;
    u32                 SlotsCount;
    u32                 FlightIndex;
    u64                 FlightEndUSecs;
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
    FLVRegion const*    Region;
    FLVColumn const*    BaseTime;
    u64 const*          Mask;
    u64 const*          MaskAll;
    r1u64*              Ranges;
    u64                 RangesCount;
    u64                 RegionSampleCount;
    u64                 ChunkBase;
    u64                 ChunkCount;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
void FLVLaneInit(FLVLane* Lane, Arena* MemPool, Arena* ResultMemPool, FLVProgram const* Program, FLIParsed* FLI, FLVBoundSlot const* Slots, u32 SlotsCount, u32 FlightIndex);
void FLVLaneRun(FLVLane* Lane);
Str8List FLVRenderItemsPrint(Arena* MemPool, FLVLane const* Lane);

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
