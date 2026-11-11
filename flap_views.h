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
#define FLAP_VIEW_UI_FUNC_SIG(X) void X(r2f32 Rect)
typedef FLAP_VIEW_UI_FUNC_SIG(FLAPViewUIFuncType);

// @types______________________________________________________________________
struct FLAPViewRule {
    u64                 Key;
    Str8                Name;
    FLAPViewUIFuncType* ViewUIFunc;
};

struct FLAPViewRuleNode {
    FLAPViewRuleNode* Next;
    FLAPViewRuleNode* Prev;
    FLAPViewRule      V;
};

struct FLAPViewRuleSlot {
    FLAPViewRuleNode* Head;
    FLAPViewRuleNode* Tail;
};

struct FLAPListViewState {
    v2i64 Cursor;
    v2i64 Mark;
};

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

struct FLAPViewsState {
    Arena*            MemPool;
    FLAPViewRuleSlot* Slots;
    u64               SlotsCount;
};

// @runtime____________________________________________________________________
extern FLAPViewsState* FLAP_VIEWS_STATE;
extern FLAPViewRuleNode* const EMPTY_FLAP_VIEW_RULE_NODE_VALUE;

// @functions__________________________________________________________________
void FLAPViewsInit(void);

INTERNAL UI_BOX_CUSTOM_DRAW(FLAPFlaggedRowDraw);

// TODO(nc): empty stub for missing view funcs?
FLAP_VIEW_UI_FUNC_SIG(FLAPFilesViewUI);
FLAP_VIEW_UI_FUNC_SIG(FLAPViewsViewUI);
FLAP_VIEW_UI_FUNC_SIG(FLAPViewTabUI);

#endif // __FLAP_VIEWS_H__
