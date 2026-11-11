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

struct FLAPViewsState {
    Arena*            MemPool;
    FLAPViewRuleSlot* Slots;
    u64               SlotsCount;
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

// @runtime____________________________________________________________________
extern FLAPViewsState* FLAP_VIEWS_STATE;
extern FLAPViewRuleNode* const EMPTY_FLAP_VIEW_RULE_NODE_VALUE;

// @functions__________________________________________________________________
void FLAPViewsInit(void);

// TODO(nc): empty stub for missing view funcs?
FLAP_VIEW_UI_FUNC_SIG(FLAPFilesViewUI);
FLAP_VIEW_UI_FUNC_SIG(FLAPViewsViewUI);
FLAP_VIEW_UI_FUNC_SIG(FLAPViewTabUI);

#endif // __FLAP_VIEWS_H__
