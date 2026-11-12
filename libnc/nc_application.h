#if !defined(__NC_APPLICATION_H__)
#define __NC_APPLICATION_H__

#include "nc_types.h"
#include "nc_string.h"
#include "nc_system.h"
#include "nc_ui.h"
#include "nc_content.h"
#include "nc_metadesk.h"
#include "nc_config.h"

#if !defined(NC_DATA_DIR)
    #define NC_DATA_DIR "flap"
#endif

// @defines____________________________________________________________________
enum AppRegistersSlot {
    APP_REGISTERS_SLOT_NULL,
    APP_REGISTERS_SLOT_WINDOW,
    APP_REGISTERS_SLOT_PANEL,
    APP_REGISTERS_SLOT_TAB,
    APP_REGISTERS_SLOT_VIEW,
    APP_REGISTERS_SLOT_PREV_TAB,
    APP_REGISTERS_SLOT_DST_PANEL,
    APP_REGISTERS_SLOT_CONFIG,
    APP_REGISTERS_SLOT_CONFIG_LIST,
    APP_REGISTERS_SLOT_FILE_PATH,
    APP_REGISTERS_SLOT_CURSOR,
    APP_REGISTERS_SLOT_MARK,
    APP_REGISTERS_SLOT_FLIGHT_INDEX,
    APP_REGISTERS_SLOT_EXPRESSION,
    APP_REGISTERS_SLOT_UI_KEY,
    APP_REGISTERS_SLOT_FORCE_CONFIRM,
    APP_REGISTERS_SLOT_STRING,
    APP_REGISTERS_SLOT_COMMAND_NAME,
    APP_REGISTERS_SLOT_DIRECTION,
    APP_REGISTERS_SLOT_OS_EVENT,
    APP_REGISTERS_SLOT_COUNT
};

enum AppCommandKind {
    APP_COMMAND_KIND_NULL,
    APP_COMMAND_KIND_EXIT,
    APP_COMMAND_KIND_OPEN_PALETTE,
    APP_COMMAND_KIND_RUN_COMMAND,
    APP_COMMAND_KIND_OS_EVENT,
    APP_COMMAND_KIND_POP_UP_ACCEPT,
    APP_COMMAND_KIND_POP_UP_CANCEL,
    APP_COMMAND_KIND_RESET_TO_DEFAULT_BINDINGS,
    APP_COMMAND_KIND_NEW_PANEL_LEFT,
    APP_COMMAND_KIND_NEW_PANEL_UP,
    APP_COMMAND_KIND_NEW_PANEL_RIGHT,
    APP_COMMAND_KIND_NEW_PANEL_DOWN,
    APP_COMMAND_KIND_SPLIT_PANEL,
    APP_COMMAND_KIND_NEXT_PANEL,
    APP_COMMAND_KIND_PREV_PANEL,
    APP_COMMAND_KIND_FOCUS_PANEL,
    APP_COMMAND_KIND_FOCUS_PANEL_RIGHT,
    APP_COMMAND_KIND_FOCUS_PANEL_LEFT,
    APP_COMMAND_KIND_FOCUS_PANEL_UP,
    APP_COMMAND_KIND_FOCUS_PANEL_DOWN,
    APP_COMMAND_KIND_UNDO,
    APP_COMMAND_KIND_REDO,
    APP_COMMAND_KIND_GO_BACK,
    APP_COMMAND_KIND_GO_FORWARD,
    APP_COMMAND_KIND_CLOSE_PANEL,
    APP_COMMAND_KIND_FOCUS_TAB,
    APP_COMMAND_KIND_NEXT_TAB,
    APP_COMMAND_KIND_PREV_TAB,
    APP_COMMAND_KIND_MOVE_TAB_RIGHT,
    APP_COMMAND_KIND_MOVE_TAB_LEFT,
    APP_COMMAND_KIND_OPEN_TAB,
    APP_COMMAND_KIND_BUILD_TAB,
    APP_COMMAND_KIND_DUPLICATE_TAB,
    APP_COMMAND_KIND_COPY_TAB_FULL_PATH,
    APP_COMMAND_KIND_CLOSE_TAB,
    APP_COMMAND_KIND_MOVE_VIEW,
    APP_COMMAND_KIND_SET_TAB_VIEW,
    APP_COMMAND_KIND_SET_TAB_FILE,
    APP_COMMAND_KIND_SET_CURRENT_PATH,
    APP_COMMAND_KIND_OPEN,
    APP_COMMAND_KIND_SWITCH,
    APP_COMMAND_KIND_SHOW_FILE_IN_EXPLORER,
    APP_COMMAND_KIND_NEW_SESSION,
    APP_COMMAND_KIND_OPEN_SESSION,
    APP_COMMAND_KIND_OPEN_RECENT_SESSION,
    APP_COMMAND_KIND_SAVE_SESSION,
    APP_COMMAND_KIND_WRITE_SESSION_DATA,
    APP_COMMAND_KIND_SESSION_SETTINGS,
    APP_COMMAND_KIND_EDIT,
    APP_COMMAND_KIND_ACCEPT,
    APP_COMMAND_KIND_CANCEL,
    APP_COMMAND_KIND_MOVE_LEFT,
    APP_COMMAND_KIND_MOVE_RIGHT,
    APP_COMMAND_KIND_MOVE_UP,
    APP_COMMAND_KIND_MOVE_DOWN,
    APP_COMMAND_KIND_MOVE_LEFT_SELECT,
    APP_COMMAND_KIND_MOVE_RIGHT_SELECT,
    APP_COMMAND_KIND_MOVE_UP_SELECT,
    APP_COMMAND_KIND_MOVE_DOWN_SELECT,
    APP_COMMAND_KIND_MOVE_LEFT_CHUNK,
    APP_COMMAND_KIND_MOVE_RIGHT_CHUNK,
    APP_COMMAND_KIND_MOVE_UP_CHUNK,
    APP_COMMAND_KIND_MOVE_DOWN_CHUNK,
    APP_COMMAND_KIND_MOVE_LEFT_CHUNK_SELECT,
    APP_COMMAND_KIND_MOVE_RIGHT_CHUNK_SELECT,
    APP_COMMAND_KIND_MOVE_UP_CHUNK_SELECT,
    APP_COMMAND_KIND_MOVE_DOWN_CHUNK_SELECT,
    APP_COMMAND_KIND_MOVE_UP_PAGE,
    APP_COMMAND_KIND_MOVE_DOWN_PAGE,
    APP_COMMAND_KIND_MOVE_UP_WHOLE,
    APP_COMMAND_KIND_MOVE_DOWN_WHOLE,
    APP_COMMAND_KIND_MOVE_UP_PAGE_SELECT,
    APP_COMMAND_KIND_MOVE_DOWN_PAGE_SELECT,
    APP_COMMAND_KIND_MOVE_UP_WHOLE_SELECT,
    APP_COMMAND_KIND_MOVE_DOWN_WHOLE_SELECT,
    APP_COMMAND_KIND_MOVE_HOME,
    APP_COMMAND_KIND_MOVE_END,
    APP_COMMAND_KIND_MOVE_HOME_SELECT,
    APP_COMMAND_KIND_MOVE_END_SELECT,
    APP_COMMAND_KIND_SELECT_ALL,
    APP_COMMAND_KIND_DELETE_SINGLE,
    APP_COMMAND_KIND_DELETE_CHUNK,
    APP_COMMAND_KIND_BACKSPACE_SINGLE,
    APP_COMMAND_KIND_BACKSPACE_CHUNK,
    APP_COMMAND_KIND_COPY,
    APP_COMMAND_KIND_CUT,
    APP_COMMAND_KIND_PASTE,
    APP_COMMAND_KIND_INSERT_TEXT,
    APP_COMMAND_KIND_MOVE_NEXT,
    APP_COMMAND_KIND_MOVE_PREV,
    APP_COMMAND_KIND_CENTRE_CURSOR,
    APP_COMMAND_KIND_CONTAIN_CURSOR,
    APP_COMMAND_KIND_GOTO_TIME,
    APP_COMMAND_KIND_SEARCH,
    APP_COMMAND_KIND_SEARCH_BACKWARDS,
    APP_COMMAND_KIND_FIND_NEXT,
    APP_COMMAND_KIND_FIND_PREV,
    APP_COMMAND_KIND_ENABLE_CONFIG,
    APP_COMMAND_KIND_DISABLE_CONFIG,
    APP_COMMAND_KIND_SELECT_CONFIG,
    APP_COMMAND_KIND_DESELECT_CONFIG,
    APP_COMMAND_KIND_REMOVE_CONFIG,
    APP_COMMAND_KIND_NAME_CONFIG,
    APP_COMMAND_KIND_DUPLICATE_CONFIG,
    APP_COMMAND_KIND_SAVE_TO_SESSION,
    APP_COMMAND_KIND_EDIT_SESSION_THEME,
    APP_COMMAND_KIND_ADD_THEME_COLOUR,
    APP_COMMAND_KIND_FORK_THEME,
    APP_COMMAND_KIND_SAVE_THEME,
    APP_COMMAND_KIND_SAVE_AND_SET_THEME,
    APP_COMMAND_KIND_PICK_FILE,
    APP_COMMAND_KIND_PICK_FOLDER,
    APP_COMMAND_KIND_PICK_FILE_OR_FOLDER,
    APP_COMMAND_KIND_FILTER,
    APP_COMMAND_KIND_SET_THEME,
    APP_COMMAND_KIND_TOGGLE_DEV_MENU,
    APP_COMMAND_KIND_RECORD_SESSION_AS_LAST_OPENED,
    APP_COMMAND_KIND_RECORD_RECENT_SESSION,
    APP_COMMAND_KIND_QUERY,
    APP_COMMAND_KIND_PUSH_QUERY,
    APP_COMMAND_KIND_RECORD_FILE_IN_SESSION,
    APP_COMMAND_KIND_COMPLETE_QUERY,
    APP_COMMAND_KIND_CANCEL_QUERY,
    APP_COMMAND_KIND_UPDATE_QUERY,
    APP_COMMAND_KIND_OPEN_FILES,
    APP_COMMAND_KIND_OPEN_VIEWS,
    APP_COMMAND_KIND_OPEN_FLV,
    APP_COMMAND_KIND_COUNT,
    APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD = APP_COMMAND_KIND_OPEN_FILES
};

typedef u32 AppQueryKind;
enum : u32 {
    APP_QUERY_KIND_ALLOW_FILES      = (1 << 0),
    APP_QUERY_KIND_ALLOW_FOLDERS    = (1 << 1),
    APP_QUERY_KIND_KEEP_OLD_INPUT   = (1 << 2),
    APP_QUERY_KIND_SELECT_OLD_INPUT = (1 << 3),
    APP_QUERY_KIND_FLOATING         = (1 << 4),
    APP_QUERY_KIND_REQUIRED         = (1 << 5)
};

typedef u32 AppCommandKindFlags;
enum : u32 {
    APP_COMMAND_KIND_FLAG_LIST_IN_UI     = (1 << 0),
    APP_COMMAND_KIND_FLAG_LIST_IN_TAB    = (1 << 1),
    APP_COMMAND_KIND_FLAG_LIST_IN_FILE   = (1 << 2)
};

typedef u32 AppCommandBindingButtonFlag;
enum : u32 {
    APP_COMMAND_BINDING_BTN_FLAG_ADD_NEW = (1 << 0),
    APP_COMMAND_BINDING_BTN_FLAG_NO_EDIT = (1 << 1)
};

enum AppDragDropState {
    APP_DRAG_DROP_STATE_NULL,
    APP_DRAG_DROP_STATE_DRAGGING,
    APP_DRAG_DROP_STATE_DROPPING,
    APP_DRAG_DROP_STATE_COUNT
};

enum AppIconKind {
    APP_ICON_KIND_NULL,
    APP_ICON_KIND_FOLDER,
    APP_ICON_KIND_FILE,
    APP_ICON_KIND_INFO,
    APP_ICON_KIND_WARNING,
    APP_ICON_KIND_LEFT_ARROW,
    APP_ICON_KIND_RIGHT_ARROW,
    APP_ICON_KIND_UP_ARROW,
    APP_ICON_KIND_DOWN_ARROW,
    APP_ICON_KIND_LEFT_CARET,
    APP_ICON_KIND_RIGHT_CARET,
    APP_ICON_KIND_UP_CARET,
    APP_ICON_KIND_DOWN_CARET,
    APP_ICON_KIND_CHECK_HOLLOW,
    APP_ICON_KIND_CHECK_FILLED,
    APP_ICON_KIND_RADIO_HOLLOW,
    APP_ICON_KIND_RADIO_FILLED,
    APP_ICON_KIND_ADD,
    APP_ICON_KIND_X,
    APP_ICON_KIND_FIND,
    APP_ICON_KIND_DOT,
    APP_ICON_KIND_COUNT
};

typedef u32 AppThemePreset;
enum : u32 {
    APP_THEME_PRESET_AYU_DARK,
    APP_THEME_PRESET_DEFAULT_DARK,
    APP_THEME_PRESET_DEFAULT_LIGHT,
    APP_THEME_PRESET_GITHUB_DARK,
    APP_THEME_PRESET_GITHUB_LIGHT,
    APP_THEME_PRESET_ONE_DARK_PRO,
    APP_THEME_PRESET_DRACULA,
    APP_THEME_PRESET_EVA_LIGHT,
    APP_THEME_PRESET_COUNT
};

enum AppQueryExpressionKind {
    APP_QUERY_EXPR_KIND_NULL,
    APP_QUERY_EXPR_KIND_COMMANDS,
    APP_QUERY_EXPR_KIND_TAB_COMMANDS,
    APP_QUERY_EXPR_KIND_RECENT_FILES,
    APP_QUERY_EXPR_KIND_FLV_FILES,
    APP_QUERY_EXPR_KIND_RECENT_SESSIONS,
    APP_QUERY_EXPR_KIND_THEMES,
    APP_QUERY_EXPR_KIND_SESSION_SETTINGS,
    APP_QUERY_EXPR_KIND_CONFIG,
    APP_QUERY_EXPR_KIND_CONFIG_CHILD,
    APP_QUERY_EXPR_KIND_FILE_PATH,
    APP_QUERY_EXPR_KIND_COUNT
};

#define AppPluralFromCodeName(X)         (AppVocabInfoFromCodeName(X)->CodeNamePlural)
#define AppDisplayFromCodeName(X)        (AppVocabInfoFromCodeName(X)->DisplayName)
#define AppDisplayPluralFromCodeName(X)  (AppVocabInfoFromCodeName(X)->DisplayNamePlural)
#define AppIconKindFromCodeName(X)       (AppVocabInfoFromCodeName(X)->IconKind)
#define AppSingularFromCodeNamePlural(X) (AppVocabInfoFromCodeNamePlural(X)->CodeName)
#define GetAppViewState(X) (X*)          AppViewStateFromSize(sizeof(X))
#define AppStoreViewParameterF32(X, Y)   AppStoreViewParameter((X), "%ff", (Y))
#define AppStoreViewParameterI64(X, Y)   AppStoreViewParameter((X), "%lld", (Y))
#define AppStoreViewParameterU64(X, Y)   AppStoreViewParameter((X), "0x%llX", (Y))
#define Registers()                      (&APP_STATE->HeadRegisters->V)
#define BaseRegisters()                  (&APP_STATE->BaseRegisters.V)

#define APP_REGISTERS_DEFAULT_INIT(X)           \
    X.Window = Registers()->Window,             \
    X.Panel = Registers()->Panel,               \
    X.Tab = Registers()->Tab,                   \
    X.View = Registers()->View,                 \
    X.PrevTab = Registers()->PrevTab,           \
    X.DstPanel = Registers()->DstPanel,         \
    X.Config = Registers()->Config,             \
    X.ConfigList = Registers()->ConfigList,     \
    X.FilePath = Registers()->FilePath,         \
    X.Cursor = Registers()->Cursor,             \
    X.Mark = Registers()->Mark,                 \
    X.FlightIndex = Registers()->FlightIndex,   \
    X.Expression = Registers()->Expression,     \
    X.UI = Registers()->UI,                     \
    X.ForceConfirm = Registers()->ForceConfirm, \
    X.String = Registers()->String,             \
    X.CommandName = Registers()->CommandName,   \
    X.Direction = Registers()->Direction,       \
    X.OSEvent = Registers()->OSEvent            \

#define AppPushRegisters(...) [&]() -> AppRegisters* { \
    AppRegisters __Registers = {};                     \
    APP_REGISTERS_DEFAULT_INIT(__Registers);           \
    __VA_ARGS__;                                       \
    return __AppPushRegisters(&__Registers);           \
}()

#define AppRegistersScope(...) DEFER(AppPushRegisters(__VA_ARGS__), AppPopRegisters())

#define AppCmd(X, ...) [&](){                                        \
    AppRegisters __Registers = {};                                   \
    APP_REGISTERS_DEFAULT_INIT(__Registers);                         \
    __VA_ARGS__;                                                     \
    AppPushCmd(APP_COMMAND_KIND_INFO_TABLE[X].String, &__Registers); \
}()

#define AppSetAutoCompleteRegisters(X, ...) [&]() {   \
    AppRegisters __Registers = {};                    \
    APP_REGISTERS_DEFAULT_INIT(__Registers);          \
    __VA_ARGS__;                                      \
    __AppSetAutoCompleteRegisters((X), &__Registers); \
}()

#define APP_VIEW_UI_FUNC_SIG(X)  void X(r2f32 Rect)
#define APP_VIEW_UI_FUNC_NAME(X) __AppViewUI##X
#define APP_VIEW_UI_FUNC_DEF(X)  APP_VIEW_UI_FUNC_SIG(APP_VIEW_UI_FUNC_NAME(X))
typedef APP_VIEW_UI_FUNC_SIG(AppViewUIFuncType);

// @types______________________________________________________________________
struct AppViewUIRule {
    Str8               Name;
    AppViewUIFuncType* ViewUIFunc;
};

struct AppViewUIRuleNode {
    AppViewUIRuleNode* Next;
    AppViewUIRule      V;
};

struct AppViewUIRuleSlot {
    AppViewUIRuleNode* Head;
    AppViewUIRuleNode* Tail;
};

struct AppViewUIRuleMap {
    AppViewUIRuleSlot* Slots;
    u64                SlotsCount;
};

struct AppAutoCompleteCursorInfo {
    Str8    ListExpr;
    Str8    Filter;
    r1u64   ReplacedRanged;
    Str8    CalleeExpr;
    MDNode* ArgSchema;
};

struct AppRegisters {
    ConfigID     Window;
    ConfigID     Panel;
    ConfigID     Tab;
    ConfigID     View;
    ConfigID     PrevTab;
    ConfigID     DstPanel;
    ConfigID     Config;
    ConfigIDList ConfigList;
    Str8         FilePath;
    TextPoint    Cursor;
    TextPoint    Mark;
    u32          FlightIndex;
    Str8         Expression;
    UIKey        UI;
    b32          ForceConfirm;
    Str8         String;
    Str8         CommandName;
    Direction2D  Direction;
    InputEvent*  OSEvent;
};

struct AppRegistersNode {
    AppRegistersNode* Next;
    AppRegisters      V;
};

struct AppQuery {
    AppQueryKind     Kind;
    AppRegistersSlot Slot;
    Str8             Expression;
    Str8             ViewName;
};

struct AppCommandKindInfo {
    Str8                String;
    Str8                Description;
    Str8                SearchTags;
    Str8                ContextFilter;
    AppCommandKindFlags Flags;
    AppQuery            Query;
};

struct AppVocabInfo {
    Str8        CodeName;
    Str8        CodeNamePlural;
    Str8        DisplayName;
    Str8        DisplayNamePlural;
    AppIconKind IconKind;
};

struct AppVocabInfoMapNode {
    AppVocabInfoMapNode* SingleNext;
    AppVocabInfoMapNode* PluralNext;
    AppVocabInfo         V;
};

struct AppVocabInfoMapSlot {
    AppVocabInfoMapNode* Head;
    AppVocabInfoMapNode* Tail;
};

struct AppVocabInfoMap {
    AppVocabInfoMapSlot* SingleSlots;
    u64                  SingleSlotsCount;
    AppVocabInfoMapSlot* PluralSlots;
    u64                  PluralSlotsCount;
};

struct AppNameSchemaInfo {
    Str8 Name;
    b32  IsView;
    Str8 Schema;
};

struct AppCommand {
    Str8          Name;
    AppRegisters* Registers;
};

struct AppCommandNode {
    AppCommandNode* Next;
    AppCommandNode* Prev;
    AppCommand      Command;
};

struct AppCommandList {
    AppCommandNode* Head;
    AppCommandNode* Tail;
    u64             Count;
};

struct AppArenaExt {
    AppArenaExt* Next;
    Arena*       MemPool;
};

struct AppViewState {
    AppViewState*   HashNext;
    AppViewState*   HashPrev;
    ConfigID        CfgID;
    u64             LastFrameIndexTouched;
    u64             LastFrameIndexBuilt;
    f32             LoadingT;
    f32             LoadingTTarget;
    u64             LoadingProgressV;
    u64             LoadingProgressVTarget;
    UIScrollPoint2D ScrollPosition;
    Arena*          MemPool;
    u64             MemPoolResetPosition;
    AppArenaExt*    HeadArenaExt;
    AppArenaExt*    TailArenaExt;
    void*           UserData;
    b32             QueryIsOpen;
    TextPoint       QueryCursor;
    TextPoint       QueryMark;
    u8              QueryBuffer[KB(1)];
    u64             QueryStringSize;
    Str8            QueryCommandName;
    b32             ContentsAreFocused;
};

struct AppViewStateSlot {
    AppViewState* Head;
    AppViewState* Tail;
};

struct AppDropCompletionTask {
    AppDropCompletionTask* Next;
    b32                    Cfg;
    Str8List               Paths;
};

struct AppState {
    Arena*                    MemPool;
    b32                       ShouldQuit;
    u64                       FrameIndex;
    i32                       FrameDepth;
    f32                       FrameDeltaTime;
    Access*                   FrameAccess;
    Arena*                    FrameMemPools[2];
    Str8List                  TabFilePaths;
    u64                       FrameTimeUSecsHistory[64];
    u64                       NumFramesRequested;
    f64                       TimeSecs;
    u64                       TimeUSecs;
    Arena*                    SessionPathMemPool;
    Str8                      SessionPath;
    Arena*                    CommandMemPools[2];
    AppCommandList            Commands[2];
    u64                       CommandsGeneration;
    Arena*                    CommandOutputMemPool;
    Str8List                  CommandOutputs;
    AppRegistersNode          BaseRegisters;
    AppRegistersNode*         HeadRegisters;
    u64                       ViewStateSlotsCount;
    AppViewStateSlot*         ViewStateSlots;
    AppViewState*             FreeViewState;
    ConfigID                  ViewStateLastAccessedID;
    AppViewState*             ViewStateLastAccessed;
    ConfigInputMap*           KeyMap;
    f32                       CatchAllAnimationRate;
    f32                       ScrollingAnimationRate;
    f32                       TooltipAnimationRate;
    f32                       RichHoverAnimationRate;
    CKey                      SessionConfigStringKey;
    CKey                      TransientConfigStringKey;
    MDNodePtrList*            Schemas;
    MDNode*                   ThemePresetTrees[APP_THEME_PRESET_COUNT];
    AppVocabInfoMap           VocabInfoMap;
    UIKey                     PopUpKey;
    b32                       PopUpActive;
    f32                       PopUpT;
    Arena*                    PopUpMemPool;
    AppCommandList            PopUpCommands;
    Str8                      PopUpTitle;
    Str8                      PopUpDescription;
    b32                       TextEditMode;
    AppRegisters*             HoverRegisters;
    AppRegistersSlot          HoverRegistersSlot;
    AppRegisters*             NextHoverRegisters;
    AppRegistersSlot          NextHoverRegistersSlot;
    UIKey                     DropCompletionKey;
    UIKey                     ContextMenuKey;
    Arena*                    DragDropMemPool;
    AppRegisters*             DragDropRegisters;
    AppRegistersSlot          DragDropRegistersSlot;
    AppDragDropState          DragDropState;
    ConfigState*              Config;
    ConfigSchemaTable*        CfgSchemaTable;
    ConfigNode*               RootConfig;
    ConfigNodePtrSlot*        ConfigIDSlots;
    u64                       ConfigIDSlotsCount;
    u64                       ConfigIDGeneration;
    ConfigID                  ConfigLastAccessedID;
    ConfigNode*               ConfigLastAccessed;
    Arena*                    BindChangeMemPool;
    b32                       BindChangeActive;
    ConfigID                  BindChangeBindingID;
    Str8                      BindChangeCommandName;
    Arena*                    ThemePathMemPool;
    b32                       AltMenuBarEnabled;
    AppViewUIRuleMap*         ViewUIRuleMap;

    u64                       FramesAlive;
    UIState*                  UI;
    UITheme*                  Theme;
    b32                       DevMenuIsOpen;
    b32                       MenuBarFocused;
    b32                       MenuBarFocusedOnPress;
    b32                       MenuBarKeyHeld;
    b32                       MenuBarFocusPressStarted;
    Arena*                    DropCompletionMemPool;
    ConfigID                  DropCompletionPanel;
    AppDropCompletionTask*    HeadDropCompletionTask;
    b32                       QueryIsActive;
    Arena*                    QueryMemPool;
    AppRegisters*             QueryRegs;
    ConfigID                  QueryLastViewID;
    u8                        ErrorBuffer[512];
    u64                       ErrorStrSize;
    f32                       ErrorT;
    UIEventList               UIEvents;
    u64                       AutoCompletePrevFrameIndex;
    Arena*                    AutoCompleteMemPool;
    AppRegisters*             AutoCompleteRegs;
    AppAutoCompleteCursorInfo AutoCompleteCursorInfo;
    u64                       HoverEvalFirstTimeUSecs;
    u64                       HoverEvalLastTimeUSecs;
    Arena*                    HoverEvalMemPool;
    Str8                      HoverEvalString;
    b32                       HoverEvalFocused;
    v2i32                     HoverEvalSpawnPosition;

    TextPoint                 QueryCursor;
    TextPoint                 QueryMark;
    u8                        QueryBuffer[KB(1)];
    u64                       QueryStringSize;
    UIScrollPoint             QueryScroll;
    v2i64                     QueryListCursor;
    v2i64                     QueryListMark;
    u64                       ErrorFrameIndex;
    UIKey                     BindChangeUIKey;
    ConfigID                  TabCtxMenuTab;
    TextPoint                 TabCtxMenuCursor;
    TextPoint                 TabCtxMenuMark;
    u8                        TabCtxMenuFileBuffer[KB(1)];
    u64                       TabCtxMenuFileSize;
};

struct BindingTableEntry {
    Str8          String;
    ConfigBinding Binding;
};

struct AppQueryResult {
    Str8                String;
    Str8                DisplayString;
    Str8                Description;
    Str8                SearchTags;
    AppIconKind         IconKind;
    ConfigID            CfgID;
    AppCommandKindInfo* CommandInfo;
    void*               UserData;
};

struct AppQueryResultNode {
    AppQueryResultNode* Next;
    AppQueryResultNode* Prev;
    AppQueryResult      V;
};

struct AppQueryResultList {
    AppQueryResultNode* Head;
    AppQueryResultNode* Tail;
    u64                 Count;
};

struct AppSchemaIRExt {
    ConfigNode*   Config;
    MDNodePtrList Schemas;
};

struct AppSchemaExpandAccel {
    Str8Array Commands;
    MDNode**  Children;
    u64       ChildrenCount;
};

struct AppConfigsIRExt {
    Str8               ConfigName;
    Str8Array          Commands;
    ConfigNodePtrArray Configs;
    r1u64              CommandIndexRange;
    r1u64              ConfigsIndexRange;
};

// @runtime____________________________________________________________________
extern AppState* APP_STATE;
extern ConfigID APP_LAST_DRAG_DROP_PANEL;
extern ConfigID APP_LAST_DRAG_DROP_PREV_TAB;

EXTERN_C_LINK_BEGIN
extern AppCommandKindInfo APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_COUNT];
extern BindingTableEntry APP_DEFAULT_BINDING_TABLE[81];
extern Str8 APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_COUNT];
extern Str8 APP_THEME_PRESET_DISPLAY_STR_TABLE[APP_THEME_PRESET_COUNT];
extern Str8 APP_THEME_PRESET_CODE_STR_TABLE[APP_THEME_PRESET_COUNT];
extern Str8 APP_THEME_PRESET_CONFIG_STR_TABLE[APP_THEME_PRESET_COUNT];
extern AppNameSchemaInfo APP_NAME_SCHEMA_INFO_TABLE[27];
extern Str8 APP_REGISTERS_SLOT_CODE_NAME_TABLE[APP_REGISTERS_SLOT_COUNT];
extern r1u64 APP_REGISTERS_SLOT_RANGE_TABLE[APP_REGISTERS_SLOT_COUNT];
extern Str8 APP_TAB_FAST_PATH_VIEW_NAME_TABLE[APP_COMMAND_KIND_COUNT - APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD];
extern Str8 APP_TAB_FAST_PATH_QUERY_NAME[APP_COMMAND_KIND_COUNT - APP_COMMAND_KIND_FIRST_TAB_FAST_PATH_CMD];
extern AppVocabInfo APP_VOCAB_INFO_TABLE[232];
EXTERN_C_LINK_END

APP_VIEW_UI_FUNC_DEF(Null);

global readonly AppViewUIRule APP_NULL_VIEW_UI_RULE = {
    {},
    APP_VIEW_UI_FUNC_NAME(Null)
};

// @functions__________________________________________________________________
AppVocabInfo* AppVocabInfoFromCodeName(Str8 CodeName);
AppVocabInfo* AppVocabInfoFromCodeNamePlural(Str8 CodeName);
void ListPush(Arena* MemPool, ConfigIDList* List, ConfigID ID);
ConfigIDList ListCpy(Arena* MemPool, ConfigIDList* List);
void CopyRegisters(Arena* MemPool, AppRegisters* Dst, AppRegisters* Src);
AppRegisters* CopyRegisters(Arena* MemPool, AppRegisters* Regs);
void ListPush(Arena* MemPool, AppCommandList* List, Str8 Name, AppRegisters* Regs);
b32 AppDragIsActive(void);
void AppDragBegin(AppRegistersSlot Slot);
b32 AppDragDrop(void);
void AppDragKill(void);
void AppSetHoverRegisters(AppRegistersSlot Slot);
AppRegisters* AppGetHoverRegisters(void);
b32 AppIsProjectFiltered(ConfigNode* Config);
v4f32 AppHSVAFromConfig(ConfigNode* Config);
v4f32 AppColourFromConfig(ConfigNode* Config);
b32 AppIsDisabled(ConfigNode* Config);
Str8 LabelFromConfig(ConfigNode* Config);
Str8 ExpressionFromConfig(ConfigNode* Config);
Str8 PathFromConfig(ConfigNode* Config);
MDNodePtrList AppSchemasFromName(Str8 Name);
Str8 AppDefaultSettingFromNames(Str8 SchemaName, Str8 SettingName);
Str8 AppSettingFromNameStr8(Str8 Name);
b32 AppSettingFromNameB32(Str8 Name);
u64 AppSettingFromNameU64(Str8 Name);
f32 AppSettingFromNameF32(Str8 Name);
ConfigNode* AppImmediateConfigFromKey(Str8 String);
ConfigNode* AppImmediateConfigFromKey(char* Fmt, ...);
AppViewState* AppViewStateFromConfig(ConfigNode* Config);
Arena* AppViewMemPool(void);
UIScrollPoint2D AppViewScrollPosition(void);
Str8 AppViewQueryCommand(void);
Str8 AppViewQueryInput(void);
Str8 AppViewSettingFromNameStr8(Str8 Name);
b32 AppViewSettingFromNameB32(Str8 Name);
u64 AppViewSettingFromNameU64(Str8 Name);
f32 AppViewSettingFromNameF32(Str8 Name);

Str8 AppViewFilePath(void);     // NOTE(nc): may not need this, just use string query via name 'file'

void* AppViewStateFromSize(u64 Size);
Arena* AppPushViewMemPool(void);
void AppStoreViewExprStr(Str8 String);
Str8 AppEvalStrFromFilePath(Arena* MemPool, Str8 String);
void AppStoreViewLoadingInfo(b32 IsLoading, u64 Progress, u64 ProgressTarget);
void AppStoreViewScrollPosition(UIScrollPoint2D Position);
void AppStoreViewParameter(Str8 Key, Str8 Value);
void AppStoreViewParameter(Str8 Key, char* Fmt, ...);
Str8 AppPushConsoleTitle(Arena* MemPool);
void UILoadingOverlay(r2f32 Rect, f32 LoadingT, u64 ProgressV, u64 ProgressVTarget);
void AppRequestFrame(void);
Arena* AppFrameMemPool(void);
void AppRegistersFillSlotFromStr(AppRegistersSlot Slot, Str8 QueryExpr, Str8 String);
AppCommandKind AppCommandKindFromStr(Str8 String);
AppCommandKindInfo* AppCommandKindInfoFromStr(Str8 String);
void AppPushCmd(Str8 Name, AppRegisters* Regs);
b32 AppNextCommand(AppCommand** Command);
b32 AppNextViewCommand(AppCommand** Command);
AppRegisters* __AppPushRegisters(AppRegisters* Regs);
AppRegisters* AppPopRegisters(void);
void ListPush(Arena* MemPool, ConfigNodePtrList* List, ConfigNode* Config);
void ListPush(Arena* MemPool, AppQueryResultList* List, AppQueryResult Entry);
Str8 AppExpressionFromConfig(ConfigNode* Config);
Str8 AppLabelFromConfig(ConfigNode* Config);
MDNode* AppThemeTreeFromName(Arena* MemPool, Access* Acc, Str8 Name);
AppViewUIRule* AppViewUIRuleFromStr(Str8 String);
void __AppSetAutoCompleteRegisters(Str8 Expr, AppRegisters* Regs);
void AppSetHoverEval(v2i32 Position, Str8 String);
AppQueryResultList AppQueryResultsFromExpression(Arena* MemPool, Str8 Expression, Str8 Input);
void AppQueryViewUI(r2f32 Rect);
FancyStrList AppTitleFStrFromConfig(Arena* MemPool, ConfigNode* Config, b32 IncludeExtras);
UISignal AppIconButton(AppIconKind Kind, FMRangeList* Matches, Str8 String);
UISignal AppIconButton(AppIconKind Kind, FMRangeList* Matches, char* Fmt, ...);
UISignal AppMenuBarButton(Str8 String);
void AppCommandBindingButtons(Str8 Name, Str8 Filter, u64 Limit, AppCommandBindingButtonFlag Flags);
UISignal AppCommandSpecButton(Str8 Name);
void AppCommandListMenuButtons(Str8* CommandNames, u64 CommandNamesCount, u32* FastPointCodePoints);
void AppInit(CommandLine* CLI);
void AppFrame(void);
void AppWindowFrame(void);

extern void AppViewUI(r2f32 Rect);
extern void AppFileRecorded(Str8 Path);

#endif // __NC_APPLICATION_H__
