#if !defined(__FLAP_VIEWS_H__)
#define __FLAP_VIEWS_H__

#include "nc_types.h"
#include "nc_math.h"
#include "nc_string.h"
#include "nc_ui.h"
#include "nc_config.h"

#include "fli.h"

#include "flv_lower.h"
#include "flv_visualisation.h"
#include "flv_compile.h"

// @defines____________________________________________________________________
typedef u32 FLAPExplorerRowKind;
enum : u32 {
    FLAP_EXPLORER_ROW_KIND_ROOT,
    FLAP_EXPLORER_ROW_KIND_FOLDER,
    FLAP_EXPLORER_ROW_KIND_FILE,
    FLAP_EXPLORER_ROW_KIND_DIVIDER
};

// @types______________________________________________________________________
struct FLAPViewTabState {
    u64           UnitHash;
    UIScrollPoint Scroll[FLV_LAYOUT_MAX];
    v2i64         Cursor[FLV_LAYOUT_MAX];
    v2i64         Mark[FLV_LAYOUT_MAX];
    f32*          Percents[FLV_LAYOUT_MAX];
    u32           PercentsCount[FLV_LAYOUT_MAX];
};

struct FLAPViewTabCtx {
    Access*           Acc;
    ConfigNode*       View;
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

struct FLAPFilesViewState {
    v2i64 Cursor;
    v2i64 Mark;
};

struct FLAPViewsViewState {
    v2i64 Cursor;
    v2i64 Mark;
};

struct FLAPLayoutRecord {
    u32 Next;
    i32 PushCount;
    i32 PopCount;
};

struct FLAPLayoutTask {
    r2f32 Rect;
    Str8  Key;
    f32   PercentOfParent;
    f32   ChildrenSize;
};

struct FLAPExplorerViewState {
    v2i64 Cursor;
    v2i64 Mark;
};

struct FLAPExplorerFolder {
    Str8 Path;
    u32  First;
    u32  Count;
    u32  Checked;
    u32  __Padding;
};

struct FLAPExplorerRow {
    ConfigNode**        Files;
    Str8                Label;
    Str8                CollapseKey;
    u32                 FilesCount;
    u32                 CheckedCount;
    FLAPExplorerRowKind Kind;
    u32                 Depth;
    b32                 IsFlights;
    b32                 IsExpanded;
};

struct FLAPExplorerRowArray {
    FLAPExplorerRow* V;
    u64              Count;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________


#endif // __FLAP_VIEWS_H__
