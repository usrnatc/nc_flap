#if !defined(__FLAP_VIEWS_H__)
#define __FLAP_VIEWS_H__

#include "nc_types.h"
#include "nc_math.h"
#include "nc_string.h"
#include "nc_ui.h"

#include "fli.h"

#include "flv_lower.h"
#include "flv_visualisation.h"
#include "flv_compile.h"

// @defines____________________________________________________________________


// @types______________________________________________________________________
struct FLAPViewTabState {
    UIScrollPoint Scroll[FLV_LAYOUT_MAX];
    v2i64         Cursor[FLV_LAYOUT_MAX];
    v2i64         Mark[FLV_LAYOUT_MAX];
    f32*          Percents[FLV_LAYOUT_MAX];
    u32           PercentsCount[FLV_LAYOUT_MAX];
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

struct FLAPViewTabCtx {
    Access*           Acc;
    FLVUnit const*    Unit;
    FLIKey const*     Keys;
    u64               KeysCount;
    u32               Flight;
    u32               __Padding;
    FLIParsed*        FLI;
    FLAPViewTabState* State;
};

struct FLAPFlightSet {
    FLIKeyArray Keys;
    u32         Selected;
    u32         __Padding;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
void FLAPViewTabUI(r2f32 Rect);

INTERNAL UI_BOX_CUSTOM_DRAW(FLAPFlaggedRowDraw);

#endif // __FLAP_VIEWS_H__
