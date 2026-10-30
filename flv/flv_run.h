#if !defined(__FLV_RUN_H__)
#define __FLV_RUN_H__

#include "nc_types.h"
#include "nc_string.h"
#include "fli.h"
#include "flv_lower.h"
#include "flv_bind.h"

struct Arena;

// @defines____________________________________________________________________
#define FLV_CHUNK_SAMPLES    4096
#define FLV_CHUNK_MASK_WORDS (FLV_CHUNK_SAMPLES / 64)

#define FLV_I64_HOLE I64_MAX

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

typedef u8 FLVRenderKind;
enum : u8 {
    FLV_RENDER_KIND_NULL,
    FLV_RENDER_KIND_CURVE,
    FLV_RENDER_KIND_TABLE,
    FLV_RENDER_KIND_TEXT,
    FLV_RENDER_KIND_FLAG,
    FLV_RENDER_KIND_COUNT
};

// @types______________________________________________________________________
struct FLVTimeRange {
    u64 StartUSecs;
    u64 EndUSecs;
};

struct FLVSelection {
    FLVTimeRange* V;
    u64           Count;
};

struct FLVIndexRange {
    u64 Head;
    u64 Tail;
};

struct FLVColumn {
    void*         Data;
    u64           Count;
    FLIScalarKind Kind;
    u8            ElementSize;
    u8            __Padding[6];
};

struct FLVRenderCurve {
    u64* TimeUSecs;
    f64* Values;
    u64  Count;
    u32  LabelStringIndex;
    u8   Axis;
    u8   __Padding[3];
};

struct FLVRenderFlag {
    FLVTimeRange* Ranges;
    u64           Count;
    u64           TotalDurationUSecs;
    u32           LabelStringIndex;
    u32           __Padding;
};

struct FLVRenderItem {
    FLVRenderKind Kind;
    u8            IsMissing;
    u16           __Padding;
    u32           AnalysisIndex;
    u32           FlightIndex;
    u32           EmitIndex;

    union {
        FLVRenderCurve Curve;
        FLVRenderFlag  Flag;
        Str8           Text;
    };
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
    FLVIndexRange*      Ranges;
    u64                 RangesCount;
    u64                 RegionSampleCount;
    u64                 ChunkBase;
    u64                 ChunkCount;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
void FLVKernelInit(void);
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
