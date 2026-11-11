#include "libnc.h"

#include "fli.h"
#include "fli_bin.h"
#include "fli_main.h"

#include "flv_intern.h"
#include "flv_token.h"
#include "flv_lexer.h"
#include "flv_type.h"
#include "flv_symbol_table.h"
#include "flv_stdlib.h"
#include "flv_schema.h"
#include "flv_parser.h"
#include "flv_type_check.h"
#include "flv_bind.h"
#include "flv_lower.h"
#include "flv_compiler.h"
#include "flv_run.h"
#include "flv_visualisation.h"
#include "flv_compile.h"
#include "flv_kernel.h"

#include "flap_views.h"

#include "fli.cpp"
#include "fli_bin.cpp"
#include "fli_main.cpp"

#include "flv_intern.cpp"
#include "flv_token.cpp"
#include "flv_lexer.cpp"
#include "flv_type.cpp"
#include "flv_symbol_table.cpp"
#include "flv_stdlib.cpp"
#include "flv_schema.cpp"
#include "flv_parser.cpp"
#include "flv_type_check.cpp"
#include "flv_bind.cpp"
#include "flv_lower.cpp"
#include "flv_compiler.cpp"
#include "flv_compile.cpp"
#include "flv_kernel.cpp"
#include "flv_run.cpp"

#include "flap_views.cpp"

#define FLAP_MAX_LANE_COUNT 256

struct FLAPListViewState {
    v2i64 Cursor;
    v2i64 Mark;
};

void
AppFileRecorded(Str8 Path)
{
    Str8 Ext = StrSkipLastDot(Path);
    b32 IsLog = (
        StrMatch(Ext, "fli"_s8, STR_MATCH_ALL_CASES) ||
        StrMatch(Ext, "bin"_s8, STR_MATCH_ALL_CASES)
    );

    if (IsLog) {
        FLIKey PathKey = FLIKeyFromPathTimeStamp(Path, 0);

        FLIOpen(PathKey);
    }
}

void
AppViewUI(r2f32 Rect)
{
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);
    Str8 ViewName = View->String;
    ConfigNode* QueryRoot = ConfigNodeChildFromStr(View, "query"_s8);
    ConfigNode* InputRoot = ConfigNodeChildFromStr(QueryRoot, "input"_s8);
    ConfigNode* CmdRoot = ConfigNodeChildFromStr(QueryRoot, "cmd"_s8);
    Str8 CurrentInput = InputRoot->Head->String;
    UISignal QueryEditSig = {};
 
    if (VS->QueryIsOpen) {
        Str8 CommandName = CmdRoot->Head->String;
        AppCommandKindInfo* CommandInfo = AppCommandKindInfoFromStr(CommandName);
 
        VS->QueryStringSize = MIN(sizeof(VS->QueryBuffer), CurrentInput.Size);
        MemCpy(
            VS->QueryBuffer,
            CurrentInput.Str,
            VS->QueryStringSize
        );
 
        if (!VS->QueryCursor.Column) {
            VS->QueryMark = TxtPt(1, 1);
            VS->QueryCursor = TxtPt(1, VS->QueryStringSize + 1);
        }
 
        Rect.Y0 += 1.0f;

        b32 QueryIsFocused = !VS->ContentsAreFocused;
        UIBox* SearchRow = EMPTY_UI_BOX_VALUE;
 
        UITag("menu_bar"_s8) {
            UISetNextFocusHot(
                QueryIsFocused ? UI_FOCUS_KIND_ON : UI_FOCUS_KIND_NULL
            );
            UISetNextFocusActive(
                QueryIsFocused ? UI_FOCUS_KIND_ON : UI_FOCUS_KIND_NULL
            );
            UISetNextChildLayoutAxis(AXIS_2D_X);
            SearchRow = UIBuildBoxFromStr(
                UI_BOX_KIND_DRAW_BACKGROUND,
                "###search"_s8
            );
            SearchRow->DefaultNavFocusActiveKey = QueryIsFocused
                ? UIKeyFromStr(SearchRow->Key, "###search_edit"_s8)
                : EMPTY_UI_KEY_VALUE;
            SearchRow->DefaultNavFocusNextActiveKey = SearchRow->DefaultNavFocusActiveKey;

            if (
                SearchRow->Kind & UI_BOX_KIND_FOCUS_ACTIVE &&
                !(SearchRow->Kind & UI_BOX_KIND_FOCUS_ACTIVE_DISABLED)
            ) {
                APP_STATE->TextEditMode = TRUE;
            }
 
            UIParent(SearchRow) {
                UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                    UITag("weak"_s8) {
                        if (CommandInfo->IconKind != APP_ICON_KIND_NULL)
                            UILabel(APP_ICON_KIND_TEXT_TABLE[CommandInfo->IconKind]);

                        if (CommandName.Size)
                        UIPreferredWidth(UI_TEXT_DIM(2.0f, 1.0f)) {
                            UILabel(CommandInfo->DisplayName);
                        }
                    }
                }
 
                UIPreferredWidth(UI_PERCENT(1.0f, 0.0f)) {
                    UITag("alt"_s8) {
                        QueryEditSig = UILineEdit(
                            &VS->QueryCursor,
                            &VS->QueryMark,
                            VS->QueryBuffer,
                            sizeof(VS->QueryBuffer),
                            &VS->QueryStringSize,
                            CurrentInput,
                            "###search_edit"_s8
                        );
                    }
                }

                if (UI_PRESSED(QueryEditSig)) {
                    VS->ContentsAreFocused = FALSE;
                    AppCmd(APP_COMMAND_KIND_FOCUS_PANEL);
                }

                UIPreferredWidth(UI_PX(3.0f, 1.0f)) {
                    UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                        UISignal CloseSig = UIButton(
                            "%S###search_close",
                            PRINT_STR(APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_X])
                        );

                        if (UI_CLICKED(CloseSig))
                            VS->QueryIsOpen = FALSE;
                    }
                }
            }
        }
 
        if (InputRoot == EMPTY_CFG_NODE_VALUE) {
            InputRoot = ConfigNodeChildFromStrOrAlloc(
                APP_STATE->Config,
                QueryRoot,
                "input"_s8
            );
        }
 
        ConfigNodeNewReplace(
            APP_STATE->Config,
            InputRoot,
            Str(VS->QueryBuffer, VS->QueryStringSize)
        );
    }
 
    for (
        UIEvent* Evt = NULL;
        UINextEvent(&Evt);
    ) {
        if (
            Evt->Kind == UI_EVENT_KIND_PRESS &&
            Evt->Input == INPUT_KIND_LEFT_MOUSE_BTN &&
            InRange(Rect, Evt->Position)
        ) {
            VS->ContentsAreFocused = TRUE;
            break;
        }
    }
 
    UIBox* ViewContainer = EMPTY_UI_BOX_VALUE;
 
    UIWidthFill() {
        UIHeightFill() {
            UISetNextChildLayoutAxis(AXIS_2D_Y);
            ViewContainer = UIBuildBoxFromKey(0, EMPTY_UI_KEY_VALUE);
        }
    }
 
    UIParent(ViewContainer) {
        UIFocus(
            VS->QueryIsOpen && 
            !VS->ContentsAreFocused 
                ? UI_FOCUS_KIND_OFF 
                : UI_FOCUS_KIND_NULL
        ) {
            if (StrMatch(ViewName, "files"_s8, 0)) {
            } else if (StrMatch(ViewName, "views"_s8, 0)) {
            } else {
                FLAPViewTabUI(Rect);
            }
        }
    }
 
    if (VS->QueryIsOpen) {
        UIFocus(UI_FOCUS_KIND_ON) {
            if (UIIsFocusActive() && UISlotPress(UI_EVENT_ACTION_SLOT_CANCEL)) {
                VS->QueryIsOpen = FALSE;
                VS->QueryStringSize = 0;
            }
 
            if (
                UI_COMMITTED(QueryEditSig) ||
                (
                    UIIsFocusActive() &&
                    UISlotPress(UI_EVENT_ACTION_SLOT_ACCEPT)
                )
            ) {
                Str8 CommandName = AppViewQueryCommand();
                Str8 Input = Str(VS->QueryBuffer, VS->QueryStringSize);
                AppCommandKindInfo* CommandKindInfo = AppCommandKindInfoFromStr(
                    CommandName
                );
 
                AppRegistersScope() {
                    AppRegistersFillSlotFromStr(
                        CommandKindInfo->Query.Slot,
                        ""_s8,
                        Input
                    );
                    AppCmd(APP_COMMAND_KIND_COMPLETE_QUERY);
                }

                if (
                    StrMatch(
                        CommandName,
                        APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_FILTER].String,
                        0
                    )
                ) {
                    VS->ContentsAreFocused = TRUE;
                }
            }
        }
    }
 
    VS->LastFrameIndexBuilt = APP_STATE->FrameIndex;
}

INTERNAL void
AsyncThreadEntryPoint(void* Params)
{
    LaneContext LaneCtx = *(LaneContext*) Params;
 
    SetLaneContext(LaneCtx);
    GetTLS()->IS_ASYNC_THREAD = TRUE;
    ThreadSetName("[ASYNC %llu]", LaneIndex());
 
    for (;;) {
        if (!LaneIndex()) {
            LOCK_SCOPE(ASYNC_TICK_BEGIN_MTX) {
                if (!AtomicLoadU32(&ASYNC_LOOP_REPEAT, MEM_ORDER_SEQ_CST)) {
                    CondVarWait(
                        ASYNC_TICK_BEGIN_COND_VAR,
                        ASYNC_TICK_BEGIN_MTX,
                        TimeNow() + MILLISECONDS(100)
                    );
                }
            }

            AtomicExchangeU32(
                &ASYNC_LOOP_REPEAT, 
                0, 
                MEM_ORDER_SEQ_CST
            );
            AtomicExchangeU32(
                &ASYNC_LOOP_REPEAT_HIGH_PRIORITY, 
                0, 
                MEM_ORDER_SEQ_CST
            );
        }
 
        LaneSync();
        OCAsyncTick();
        CAsyncTick();
        FSAsyncTick();
        LogAsyncTick();
        FLIAsyncTick();
        LaneSync();

        if (!LaneIndex())
            AsyncTickEnd();
 
        b32 ShouldQuit = FALSE;
 
        if (!LaneIndex())
            ShouldQuit = AtomicLoadU32(&GLOBAL_ASYNC_EXIT, MEM_ORDER_SEQ_CST);
 
        LaneSyncU64(&ShouldQuit, 0);
 
        if (ShouldQuit)
            break;
    }
}

internal void
FLAPInteractive(CommandLine* CLI)
{
    TempArena Scratch = GetScratch(NULL, 0);
 
    MDInit();
    ConfigInit();
    FLVCompilerInit();
    TerminalInit();
    RendInit(CLI);
    DrawInit();
    UIInit();
    FLAPViewsInit();
 
    Handle* AsyncThreads = NULL;
    u64 LaneBroadcastValue = 0;
    u64 MainThreadCount = 1;
    u64 AsyncThreadsCount = GetSystemProperties()->LogicalProcessorCount;
    u64 MainThreadCountClamped = MIN(AsyncThreadsCount, MainThreadCount);
 
    AsyncThreadsCount -= MainThreadCountClamped;
 
    Str8 AsyncThreadsCountString = CommandLineString(CLI, "threads"_s8);
 
    if (AsyncThreadsCountString.Size)
        AsyncThreadsCount = U64FromStr(AsyncThreadsCountString);
 
    AsyncThreadsCount = CLAMP(1, AsyncThreadsCount, FLAP_MAX_LANE_COUNT);
 
    Handle Barrier = BarrierAlloc(AsyncThreadsCount);
    LaneContext* LaneCtxs = ArenaPushArrayZero(
        Scratch.MemPool,
        LaneContext,
        AsyncThreadsCount
    );
 
    ASYNC_THREADS_COUNT = AsyncThreadsCount;
    AsyncThreads = ArenaPushArrayZero(
        Scratch.MemPool, 
        Handle, 
        ASYNC_THREADS_COUNT
    );
 
    for (u64 Index = 0; Index < ASYNC_THREADS_COUNT; ++Index) {
        LaneCtxs[Index].Index = Index;
        LaneCtxs[Index].Count = ASYNC_THREADS_COUNT;
        LaneCtxs[Index].Barrier = Barrier;
        LaneCtxs[Index].BroadcastMemory = &LaneBroadcastValue;
        AsyncThreads[Index] = ThreadLaunch(
            AsyncThreadEntryPoint, 
            &LaneCtxs[Index]
        );
    }
 
    AppInit(CLI);
 
    for (; !APP_STATE->ShouldQuit; )
        AppFrame();
 
    AtomicIncFetchU32(&GLOBAL_ASYNC_EXIT, MEM_ORDER_SEQ_CST);
    AsyncWake(FALSE);
 
    for (u32 Index = 0; Index < ASYNC_THREADS_COUNT; ++Index)
        ThreadJoin(AsyncThreads[Index], U64_MAX);
 
    BarrierRelease(Barrier);
    RendRelease();
    TerminalRelease();
    ReleaseScratch(Scratch);
}

void 
EntryPoint(CommandLine* CLI)
{
#if defined(NC_FLI)
    FLIEntryPoint(CLI);
#else
    FLAPInteractive(CLI);
#endif
}
