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

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
void FLAPTable(FLAPTableParams* Params, r2f32 Rect);
void FLAPFlagList(FLAPFlagListParams* Params, r2f32 Rect);

INTERNAL UI_BOX_CUSTOM_DRAW(FLAPFlaggedRowDraw);

#endif // __FLAP_WIDGETS_H__
