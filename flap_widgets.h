#if !defined(__FLAP_WIDGETS_H__)
#define __FLAP_WIDGETS_H__

#include "nc_types.h"
#include "nc_math.h"
#include "nc_string.h"
#include "nc_ui.h"

#include "fli.h"

#include "flv_lower.h"
#include "flv_visualisation.h"

// @defines____________________________________________________________________
#define FLAP_GRAPH_GUTTER 9
#define FLAP_GRAPH_SPAN_MAX_USECS (1ULL << 40)

// @types______________________________________________________________________
struct FLAPTableParams {
    Access*           Acc;
    FLIParsed*        FLI;
    FLVProgram const* Program;
    FLVRenderItem*    Items;
    f32**             Percents;
    UIScrollPoint*    Scroll;
    v2i64*            Cursor;
    v2i64*            Mark;
    FLIKey            Flight;
    u32               HeadEmit;
    u32               Key;
};

struct FLAPTableCtx {
    FLVRenderTable const* Table;
    FLVProgram const*     Program;
    FLVColumn*            Columns;
    FLVColumn             Time;
    FLVRenderFlag const** Flags;
    u32                   FlagsCount;
    u32                   __Padding;
    u64                   StartTimeUSecs;
};

struct FLAPFlagListParams {
    FLVProgram const* Program;
    FLVRenderItem*    Items;
    UIScrollPoint*    Scroll;
    v2i64*            Cursor;
    v2i64*            Mark;
};

struct FLAPGraphState {
    r1u64 TimeRangeUSecs;
    r1u64 FitRangeUSecs;
    u64   CursorTimeUSecs;
    u64   NextCursorTimeUSecs;
    b32   IsFitted;
    b32   IsFitReady;
    b32   HasCursor;
    b32   HasNextCursor;
};

struct FLAPGraphKey {
    FLVRenderCurve const* Curve;
    u64                   UnitHash;
    FLIKey                Flight;
    r1u64                 TimeRangeUSecs;
    u32                   Count;
    u32                   __Padding;
};

struct FLAPGraphColumns {
    f32*         Min;
    f32*         Max;
    u64*         Has;
    u64*         Join;
    u64          Samples;
    FLAPGraphKey Key;
    u32          Capacity;
    u32          __Padding;
};

struct FLAPGraphParams {
    Arena*            MemPool;
    Access*           Acc;
    FLIParsed*        FLI;
    FLVProgram const* Program;
    FLVRenderItem*    Items;
    FLAPGraphState*   State;
    FLAPGraphColumns* Columns;
    u64               UnitHash;
    FLIKey            Flight;
    u32               Key;
    u32               __Padding;
};

struct FLAPGraphDrawCurve {
    FLAPGraphColumns const* Columns;
    v4f32                   Colour;
    u32                     Axis;
    u32                     __Padding;
};

struct FLAPGraphDrawLabel {
    Str8  String;
    v4f32 Colour;
};

struct FLAPGraphDrawData {
    FLAPGraphDrawCurve*   Curves;
    FLAPGraphDrawLabel*   Labels;
    FLVRenderFlag const** Flags;
    r1u64                 TimeRangeUSecs;
    u64                   CursorTimeUSecs;
    r1f32                 ValueRanges[2];
    b32                   HasRange[2];
    v4f32                 LabelColour;
    v4f32                 BandColour;
    v4f32                 CursorColour;
    u32                   CurvesCount;
    u32                   LabelsCount;
    u32                   FlagsCount;
    b32                   HasCursor;
    u32                   PlotX;
    u32                   PlotY;
    u32                   Width;
    u32                   Height;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
void FLAPTable(FLAPTableParams* Params, r2f32 Rect);
void FLAPFlagList(FLAPFlagListParams* Params, r2f32 Rect);
b32 FLAPGraphColumnsFromKey(Arena* MemPool, FLAPGraphColumns* Columns, FLAPGraphKey* Key, Access* Acc, FLIParsed* FLI);
void FLAPGraph(FLAPGraphParams* Params, r2f32 Rect);

INTERNAL UI_BOX_CUSTOM_DRAW(FLAPFlaggedRowDraw);
INTERNAL UI_BOX_CUSTOM_DRAW(FLAPGraphDraw);

#endif // __FLAP_WIDGETS_H__
