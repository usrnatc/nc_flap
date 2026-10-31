#if !defined(__FLV_VISUALISATION_H__)
#define __FLV_VISUALISATION_H__

#include "nc_types.h"
#include "nc_math.h"
#include "nc_string.h"

// @defines____________________________________________________________________
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
struct FLVRenderCurve {
    u64* TimeUSecs;
    f64* Values;
    u64  Count;
    u32  LabelStringIndex;
    u8   Axis;
    u8   __Padding[3];
};

struct FLVRenderFlag {
    r1u64* Ranges;
    u64    Count;
    u64    TotalDurationUSecs;
    u32    LabelStringIndex;
    u32    __Padding;
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

// @runtime____________________________________________________________________


// @funcions___________________________________________________________________


#endif // __FLV_VISUALISATION_H__
