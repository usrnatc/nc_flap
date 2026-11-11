#include "nc_application.h"
#include "nc_arena.h"
#include "nc_config.h"
#include "nc_draw.h"
#include "nc_memory.h"
#include "nc_ui_components.h"

#include "flv_token.h"
#include "flv_run.h"

#include "flap_views.h"
#include "flap_widgets.h"

FLAPViewRuleNode        __EMPTY_FLAP_VIEW_RULE_NODE_VALUE;
FLAPViewRuleNode* const EMPTY_FLAP_VIEW_RULE_NODE_VALUE = &__EMPTY_FLAP_VIEW_RULE_NODE_VALUE;

FLAPViewsState* FLAP_VIEWS_STATE = NULL;

void 
FLAPViewsInit(void)
{
    EMPTY_FLAP_VIEW_RULE_NODE_VALUE->Next = EMPTY_FLAP_VIEW_RULE_NODE_VALUE;
    EMPTY_FLAP_VIEW_RULE_NODE_VALUE->Prev = EMPTY_FLAP_VIEW_RULE_NODE_VALUE;

    Arena* MemPool = ArenaAlloc();

    FLAP_VIEWS_STATE = ArenaPushArrayZero(
        MemPool, 
        FLAPViewsState, 
        1
    );
    FLAP_VIEWS_STATE->MemPool = MemPool;
    FLAP_VIEWS_STATE->SlotsCount = 64;
    FLAP_VIEWS_STATE->Slots = ArenaPushArrayZero(
        MemPool,
        FLAPViewRuleSlot,
        FLAP_VIEWS_STATE->SlotsCount
    );

    for (u64 Index = 0; Index < FLAP_VIEWS_STATE->SlotsCount; ++Index) {
        FLAPViewRuleSlot* Slot = &FLAP_VIEWS_STATE->Slots[Index];

        Slot->Head = EMPTY_FLAP_VIEW_RULE_NODE_VALUE;
        Slot->Tail = EMPTY_FLAP_VIEW_RULE_NODE_VALUE;
    }

    struct ViewInfo {
        Str8                Name;
        FLAPViewUIFuncType* ViewUIFunc;
    } ViewInfos[] = {
        {
            "files"_s8,
            FLAPFilesViewUI
        },
        {
            "views"_s8,
            FLAPViewsViewUI
        },
        {
            "view"_s8,
            FLAPViewTabUI
        }
    };

    for (u64 Index = 0; Index < ARRAY_COUNT(ViewInfos); ++Index) {
        FLAPViewRuleNode* NewNode = ArenaPushArrayZero(
            MemPool, 
            FLAPViewRuleNode, 
            1
        );
        ViewInfo Info = ViewInfos[Index];
        u64 ViewNameHash = Hash(Info.Name);
        u64 SlotIndex = ViewNameHash % FLAP_VIEWS_STATE->SlotsCount;
        FLAPViewRuleSlot* Slot = &FLAP_VIEWS_STATE->Slots[SlotIndex];
        FLAPViewRule ViewRule = {};

        ViewRule.Key = ViewNameHash;
        ViewRule.Name = Info.Name;
        ViewRule.ViewUIFunc = Info.ViewUIFunc;
        NewNode->V = ViewRule;
        NewNode->Next = EMPTY_FLAP_VIEW_RULE_NODE_VALUE;
        NewNode->Prev = EMPTY_FLAP_VIEW_RULE_NODE_VALUE;
        DLL_PUSH_BACK_EX(
            EMPTY_FLAP_VIEW_RULE_NODE_VALUE,
            Slot->Head,
            Slot->Tail,
            NewNode,
            Next,
            Prev
        );
    }
}

FLAPViewUIFuncType*
FLAPViewUIFuncFromName(Str8 Name)
{
    FLAPViewUIFuncType* Result = NULL;

    if (FLAP_VIEWS_STATE) {
        u64 NameHash = Hash(Name);
        u64 SlotIndex = NameHash % FLAP_VIEWS_STATE->SlotsCount;
        FLAPViewRuleSlot* Slot = &FLAP_VIEWS_STATE->Slots[SlotIndex];
        FLAPViewRuleNode* FoundNode = EMPTY_FLAP_VIEW_RULE_NODE_VALUE;

        for (
            FLAPViewRuleNode* Node = Slot->Head;
            Node && Node != EMPTY_FLAP_VIEW_RULE_NODE_VALUE;
            Node = Node->Next
        ){
            if (Node->V.Key == NameHash) {
                FoundNode = Node;
                break;
            }
        }

        if (FoundNode != EMPTY_FLAP_VIEW_RULE_NODE_VALUE)
            Result = FoundNode->V.ViewUIFunc;
    }

    return Result;
}

internal FLAPFlightSet
FLAPFlightSetFromConfig(Arena* MemPool)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    ConfigNodePtrList FileList = ConfigNodeTopLevelListFromStr(
        Scratch.MemPool,
        "file"_s8
    );
    ConfigNodePtrArray Files = ConfigNodePtrArrayFromList(
        Scratch.MemPool,
        &FileList
    );
    ConfigNode* Transient = ConfigNodeChildFromStr(
        ConfigNodeRoot(),
        "transient"_s8
    );
    Str8 SelectedPath = ConfigNodeChildFromStr(
        Transient,
        "flight"_s8
    )->Head->String;
    FLAPFlightSet Result = {};
 
    Result.Keys.Count = Files.Count;
    Result.Keys.V = ArenaPushArrayZero(MemPool, FLIKey, Result.Keys.Count);
 
    for (u64 Index = 0; Index < Files.Count; ++Index) {
        Str8 Path = Files.V[Index]->Head->String;
 
        Result.Keys.V[Index] = FLIKeyFromPathTimeStamp(Path, 0);
 
        if (SelectedPath.Size && PathMatchNormalised(Path, SelectedPath))
            Result.Selected = (u32) Index;
    }
 
    ReleaseScratch(Scratch);
 
    return Result;
}

INTERNAL f32**
FLAPTablePercents(FLAPViewTabCtx* Ctx, u32 NodeIndex, u32 Count)
{
    FLAPViewTabState* State = Ctx->State;
 
    if (State->PercentsCount[NodeIndex] != Count) {
        f32 Share = 1.0f / (f32) Count;
 
        State->Percents[NodeIndex] = ArenaPushArray(
            AppViewMemPool(), 
            f32, 
            Count
        );
        State->PercentsCount[NodeIndex] = Count;
 
        for (u32 Index = 0; Index < Count; ++Index)
            State->Percents[NodeIndex][Index] = Share;
    }
 
    f32** Result = ArenaPushArray(UIBuildMemPool(), f32*, Count);
 
    for (u32 Index = 0; Index < Count; ++Index)
        Result[Index] = &State->Percents[NodeIndex][Index];
 
    return Result;
}

INTERNAL void
FLAPAnalysisUI(FLAPViewTabCtx* Ctx, u32 NodeIndex, r2f32 Rect)
{
    FLVLayoutNode const* Node = &Ctx->Unit->Lowered.Layout[NodeIndex];
    FLVProgram const* Program = &Ctx->Unit->Lowered.Program[
        Node->AnalysisIndex
    ];
    Str8 Title = Ctx->Unit->LayoutNames[NodeIndex];
    FLVRenderItem* Items = FLVResultFromUnit(
        Ctx->Acc,
        Ctx->Unit,
        Ctx->Keys,
        Ctx->KeysCount,
        Ctx->Keys[Ctx->Flight],
        Node->AnalysisIndex,
        0
    );
 
    if (!Title.Size)
        Title = ArenaPushStrFmt(UIBuildMemPool(), "%u", Node->Ordinal);
 
    UISetNextChildLayoutAxis(AXIS_2D_Y);
 
    UIBox* Box = UIBuildBoxFromStrFmt(
        UI_BOX_KIND_CLIP,
        "###analysis_%u",
        NodeIndex
    );
 
    UIParent(Box) {
        UITag("menu_bar"_s8) {
            UIPreferredSize(AXIS_2D_Y, UI_PX(1.0f, 1.0f)) {
                UIWidthFill() {
                    UISetNextFlags(UI_BOX_KIND_DRAW_BACKGROUND);
                    UILabel(Title);
                }
            }
        }
 
        Rect.Y0 += 1.0f;
 
        if (!Items) {
            UITag("weak"_s8) {
                UIWidthFill() {
                    UILabel("running..."_s8);
                }
            }
 
            AppRequestFrame();
        } else {
            u32 TableEmit = U32_MAX;
            u32 FlagCount = 0;
 
            for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
                u8 Kind = Program->Emits[Index].DisplayKind;
 
                if (
                    Kind == TOKEN_KIND_TABLE &&
                    TableEmit == U32_MAX &&
                    !Items[Index].IsMissing &&
                    Items[Index].Kind == FLV_RENDER_KIND_TABLE
                ) {
                    TableEmit = Index;
                }
 
                if (Kind == TOKEN_KIND_FLAG)
                    ++FlagCount;
 
                // TODO(nathan): curves are held until the TUI plot exists.
                if (Kind == TOKEN_KIND_GRAPH) {
                    Str8 Label = Program->Strings[
                        Program->Emits[Index].LabelStringIndex
                    ];
 
                    UITag("weak"_s8) {
                        UIWidthFill() {
                            if (Items[Index].IsMissing) {
                                UILabel("graph %S: missing", PRINT_STR(Label));
                            } else {
                                UILabel(
                                    "graph %S: axis %u, %llu samples",
                                    PRINT_STR(Label),
                                    Items[Index].Curve.Axis,
                                    Items[Index].Curve.Count
                                );
                            }
                        }
                    }
 
                    Rect.Y0 += 1.0f;
                }
            }
 
            if (TableEmit != U32_MAX) {
                FLAPTableParams Params = {};

                Params.Acc = Ctx->Acc;
                Params.FLI = Ctx->FLI;
                Params.Program = Program;
                Params.Items = Items;
                Params.Percents = FLAPTablePercents(
                    Ctx,
                    NodeIndex,
                    Items[TableEmit].Table.ColumnCount + 1
                );
                Params.Scroll = &Ctx->State->Scroll[NodeIndex];
                Params.Cursor = &Ctx->State->Cursor[NodeIndex];
                Params.Mark = &Ctx->State->Mark[NodeIndex];
                Params.Flight = Ctx->Keys[Ctx->Flight];
                Params.HeadEmit = TableEmit;
                Params.Key = NodeIndex;
                FLAPTable(&Params, Rect);
            } else if (FlagCount) {
                FLAPFlagListParams Params = {};

                Params.Program = Program;
                Params.Items = Items;
                Params.Scroll = &Ctx->State->Scroll[NodeIndex];
                Params.Cursor = &Ctx->State->Cursor[NodeIndex];
                Params.Mark = &Ctx->State->Mark[NodeIndex];
                FLAPFlagList(&Params, Rect);
            }
        }
    }
}

INTERNAL Str8
FLAPLayoutKey(FLAPViewTabCtx* Ctx, u32 NodeIndex, Str8 ParentKey)
{
    FLVLayoutNode const* Node = &Ctx->Unit->Lowered.Layout[NodeIndex];
    Str8 Name = Ctx->Unit->LayoutNames[NodeIndex];
    Str8 Result = {};

    if (!Name.Size)
        Name = ArenaPushStrFmt(UIBuildMemPool(), "%u", Node->Ordinal);

    Result = ParentKey.Size
        ? ArenaPushStrFmt(
            UIBuildMemPool(),
            "%S.%S",
            PRINT_STR(ParentKey),
            PRINT_STR(Name)
        )
        : Name;

    return Result;
}

INTERNAL void
FLAPOverrideStore(ConfigNode* View, Str8 Key, f32 PercentOfParent)
{
    ConfigNode* Overrides = ConfigNodeChildFromStrOrAlloc(
        APP_STATE->Config,
        View,
        "overrides"_s8
    );
    ConfigNode* Node = ConfigNodeChildFromStrOrAlloc(
        APP_STATE->Config,
        Overrides,
        Key
    );

    ConfigNodeNewReplace(APP_STATE->Config, Node, "%f", PercentOfParent);
}

INTERNAL FLAPLayoutRecord
FLAPLayoutRecDepthFirstPre(FLVLayoutNode const* Layout, u32 NodeIndex)
{
    FLAPLayoutRecord Result = {};
    FLVLayoutNode const* Node = &Layout[NodeIndex];

    Result.Next = U32_MAX;

    if (Node->AnalysisIndex == U32_MAX && Node->ChildCount) {
        Result.Next = Node->HeadChild;
        ++Result.PushCount;
    } else {
        for (
            u32 P = NodeIndex; 
            P != 0; 
            P = Layout[P].Parent, ++Result.PopCount
        ) {
            FLVLayoutNode const* Parent = &Layout[Layout[P].Parent];

            if (P + 1 < Parent->HeadChild + Parent->ChildCount) {
                Result.Next = P + 1;
                break;
            }
        }
    }

    return Result;
}

internal Str8
FLAPLayoutShape(Arena* MemPool, FLVUnit const* Unit) {
    FLVLayoutNode const* Layout = Unit->Lowered.Layout;
    Str8 Result = {};

    for (
        u32 Index = 0;
        Index != U32_MAX;
        Index = FLAPLayoutRecDepthFirstPre(Layout, Index).Next
    ) {
        FLVLayoutNode const* Node = &Layout[Index];

        if (Node->AnalysisIndex != U32_MAX)
            continue;

        Result = Result.Size
            ? ArenaPushStrFmt(
                MemPool,
                "%S.%u",
                PRINT_STR(Result),
                Node->ChildCount
            )
            : ArenaPushStrFmt(MemPool, "%u", Node->ChildCount);
    }

    return Result;
}

internal void
FLAPLayoutUI(FLAPViewTabCtx* Ctx, r2f32 Rect) {
    TempArena Scratch = GetScratch(NULL, 0);
    FLVLayoutNode const* Layout = Ctx->Unit->Lowered.Layout;
    ConfigNode* Overrides = ConfigNodeChildFromStr(Ctx->View, "overrides"_s8);
    FLAPLayoutTask* Tasks = ArenaPushArrayZero(
        Scratch.MemPool,
        FLAPLayoutTask,
        Ctx->Unit->Lowered.LayoutCount
    );

    Tasks[0].Rect = Rect;
    Tasks[0].PercentOfParent = 1.0f;

    for (u32 Index = 0; Index != U32_MAX; ) {
        FLVLayoutNode const* Node = &Layout[Index];
        FLAPLayoutTask* Task = &Tasks[Index];
        FLAPLayoutRecord Rec = FLAPLayoutRecDepthFirstPre(Layout, Index);

        if (Index) {
            FLVLayoutNode const* Parent = &Layout[Node->Parent];
            FLAPLayoutTask* ParentTask = &Tasks[Node->Parent];
            Axis2D ParentAxis = (Parent->Depth & 1) ? AXIS_2D_X : AXIS_2D_Y;

            if (Index != Parent->HeadChild) {
                FLAPLayoutTask* PrevTask = Task - 1;
                f32 TotalSize = ParentTask->ChildrenSize;
                UIBox* BoundaryBox = EMPTY_UI_BOX_VALUE;

                UITag("menu_bar"_s8) {
                    UISetNextPreferredSize(ParentAxis, UI_PX(1.0f, 1.0f));
                    UISetNextPreferredSize(
                        FLIP_AXIS(ParentAxis),
                        UI_PERCENT(1.0f, 0.0f)
                    );
                    UISetNextHoverCursor(
                        (ParentAxis == AXIS_2D_X)
                            ? CURSOR_KIND_LEFT_RIGHT
                            : CURSOR_KIND_UP_DOWN
                    );
                    BoundaryBox = UIBuildBoxFromStrFmt(
                        (
                            UI_BOX_KIND_MOUSE_CLICKABLE |
                            UI_BOX_KIND_DRAW_BACKGROUND |
                            UI_BOX_KIND_DRAW_HOT_EFFECTS |
                            UI_BOX_KIND_DRAW_ACTIVE_EFFECTS
                        ),
                        "###layout_boundary_%u",
                        Index
                    );
                }

                UISignal Sig = UISignalFromBox(BoundaryBox);

                if (UI_PRESSED(Sig)) {
                    v2f32 Percents = Vec(
                        PrevTask->PercentOfParent, 
                        Task->PercentOfParent
                    );

                    UIStoreDragStruct(&Percents);
                } else if (UI_DRAGGING(Sig)) {
                    v2f32 Percents = *UIGetDragStruct(v2f32);
                    f32 BothSize = (Percents.X + Percents.Y) * TotalSize;

                    if (BothSize >= 6.0f) {
                        f32 MinSize = CLAMP(
                            3.0f,
                            Percents.X * TotalSize + UIDragDelta().V[ParentAxis],
                            BothSize - 3.0f
                        );

                        FLAPOverrideStore(
                            Ctx->View,
                            PrevTask->Key,
                            MinSize / TotalSize
                        );
                        FLAPOverrideStore(
                            Ctx->View,
                            Task->Key,
                            (BothSize - MinSize) / TotalSize
                        );
                    }
                }
            }

            UISetNextPreferredSize(
                ParentAxis,
                UI_PX(Length(Task->Rect).V[ParentAxis], 0.0f)
            );
            UISetNextPreferredSize(
                FLIP_AXIS(ParentAxis),
                UI_PERCENT(1.0f, 0.0f)
            );
        }

        if (Node->AnalysisIndex != U32_MAX) {
            FLAPAnalysisUI(Ctx, Index, Task->Rect);
        } else {
            Axis2D Axis = (Node->Depth & 1) ? AXIS_2D_X : AXIS_2D_Y;
            u32 Count = Node->ChildCount;
            f32 Remaining = 1.0f;
            f32 Offset = 0.0f;
            u32 FreeCount = 0;

            Task->ChildrenSize = Length(Task->Rect).V[Axis];

            if (Count)
                Task->ChildrenSize -= (f32) (Count - 1);

            for (u32 Child = 0; Child < Count; ++Child) {
                FLAPLayoutTask* ChildTask = &Tasks[Node->HeadChild + Child];

                ChildTask->Key = FLAPLayoutKey(
                    Ctx,
                    Node->HeadChild + Child,
                    Task->Key
                );

                Str8 Value = ConfigNodeChildFromStr(
                    Overrides,
                    ChildTask->Key
                )->Head->String;

                if (Value.Size) {
                    ChildTask->PercentOfParent = (f32) F64FromStr(Value);
                    Remaining -= ChildTask->PercentOfParent;
                } else {
                    ++FreeCount;
                }
            }

            Remaining = CLAMP_BOT(0.0f, Remaining);

            for (u32 Child = 0; Child < Count; ++Child) {
                FLAPLayoutTask* ChildTask = &Tasks[Node->HeadChild + Child];

                if (ChildTask->PercentOfParent == 0.0f)
                    ChildTask->PercentOfParent = Remaining / (f32) FreeCount;

                ChildTask->Rect = Task->Rect;
                ChildTask->Rect.Point0.V[Axis] = RoundF32(
                    Task->Rect.Point0.V[Axis] + Offset
                );
                Offset += ChildTask->PercentOfParent * Task->ChildrenSize;
                ChildTask->Rect.Point1.V[Axis] = RoundF32(
                    Task->Rect.Point0.V[Axis] + Offset
                );
                Offset += 1.0f;
            }

            UISetNextChildLayoutAxis(Axis);

            UIBox* Box = UIBuildBoxFromStrFmt(0, "###layout_%u", Index);

            if (Rec.PushCount)
                UIPushParent(Box);
        }

        for (i32 Pop = 0; Pop < Rec.PopCount; ++Pop)
            UIPopParent();

        Index = Rec.Next;
    }

    ReleaseScratch(Scratch);
}

FLAP_VIEW_UI_FUNC_SIG(FLAPViewTabUI)
{
    TempArena Scratch = GetScratch(NULL, 0);
    FLAPViewTabState* State = GetAppViewState(FLAPViewTabState);
    Access* Acc = APP_STATE->FrameAccess;
    Str8 Path = AppViewFilePath();
    FLAPFlightSet Flights = FLAPFlightSetFromConfig(Scratch.MemPool);
    FLVUnit* Unit = FLVUnitFromPath(
        Acc,
        Path,
        Flights.Keys.V,
        Flights.Keys.Count,
        0
    );
    Str8 Message = ""_s8;
 
    if (!Unit) {
        Message = "compiling..."_s8;
        AppRequestFrame();
    } else if (Unit->HasError) {
        // TODO(nathan): diagnostics on the unit
        Message = "View has compile errors."_s8;
    } else if (!Flights.Keys.Count) {
        Message = "There are no flights in the list."_s8;
    } else {
        FLIParsed* FLI = FLIParsedFromKey(
            Acc,
            Flights.Keys.V[Flights.Selected],
            FALSE,
            0
        );
 
        if (FLI == EMPTY_FLI_PARSED_VALUE) {
            Message = "loading flight..."_s8;
            AppRequestFrame();
        } else {
            FLAPViewTabCtx Ctx = {};
            ConfigNode* View = ConfigNodeFromID(Registers()->View);

            if (State->UnitHash != Unit->Hash) {
                Str8 Shape = FLAPLayoutShape(Scratch.MemPool, Unit);
                ConfigNode* ShapeNode = ConfigNodeChildFromStrOrAlloc(
                    APP_STATE->Config,
                    View,
                    "shape"_s8
                );
                ConfigNode* Overrides = ConfigNodeChildFromStr(
                    View,
                    "overrides"_s8
                );

                if (!StrMatch(ShapeNode->Head->String, Shape, 0)) {
                    if (Overrides != EMPTY_CFG_NODE_VALUE) {
                        ConfigNodeReleaseAllChildren(
                            APP_STATE->Config,
                            Overrides
                        );
                    }

                    ConfigNodeNewReplace(APP_STATE->Config, ShapeNode, Shape);
                }

                State->UnitHash = Unit->Hash;
            }

            for (u32 Index = 0; Index < Unit->Lowered.LayoutCount; ++Index) {

                UIScrollPoint* Scroll = &State->Scroll[Index];
                f32 Diff = -Scroll->Offset;

                Scroll->Offset += Diff * APP_STATE->ScrollingAnimationRate;

                if (AbsF32(Diff) > 0.01f)
                    AppRequestFrame();
                else
                    Scroll->Offset = 0.0f;
            }
 
            Ctx.Acc = Acc;
            Ctx.Unit = Unit;
            Ctx.Keys = Flights.Keys.V;
            Ctx.KeysCount = Flights.Keys.Count;
            Ctx.Flight = Flights.Selected;
            Ctx.FLI = FLI;
            Ctx.State = State;
            Ctx.View = View;
 
            UISetNextPreferredSize(AXIS_2D_X, UI_PERCENT(1.0f, 0.0f));
            UISetNextPreferredSize(AXIS_2D_Y, UI_PERCENT(1.0f, 0.0f));
            FLAPLayoutUI(&Ctx, Rect);
        }
    }
 
    AppStoreViewLoadingInfo(!Unit, 0, 0);
 
    if (Message.Size) {
        UIPadding(UI_PERCENT(1.0f, 0.0f)) {
            UIWidthFill() {
                UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                    UITag("weak"_s8) {
                        UILabel(Message);
                    }
                }
            }
        }
    }
 
    ReleaseScratch(Scratch);
}

FLAP_VIEW_UI_FUNC_SIG(FLAPFilesViewUI)
{
    TempArena Scratch = GetScratch(NULL, 0);
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);
    ConfigNode* QueryRoot = ConfigNodeChildFromStr(View, "query"_s8);
    ConfigNode* CmdRoot = ConfigNodeChildFromStr(QueryRoot, "cmd"_s8);
    FLAPFilesViewState* State = GetAppViewState(
        FLAPFilesViewState
    );
    ConfigNodePtrList FileList = ConfigNodeTopLevelListFromStr(
        Scratch.MemPool,
        "file"_s8
    );
    ConfigNodePtrArray AllFiles = ConfigNodePtrArrayFromList(
        Scratch.MemPool,
        &FileList
    );
    ConfigNodePtrArray Files = {};
    UIScrollPoint2D ScrollPosition = AppViewScrollPosition();
    Str8 OpenCommandName = APP_COMMAND_KIND_INFO_TABLE[
        APP_COMMAND_KIND_OPEN
    ].String;
    Str8 Filter = {};

    if (
        VS->QueryIsOpen &&
        StrMatch(
            CmdRoot->Head->String,
            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_FILTER].String,
            0
        )
    ) {
        Filter = Str(VS->QueryBuffer, VS->QueryStringSize);
    }

    Files.V = ArenaPushArrayZero(
        Scratch.MemPool,
        ConfigNode*,
        AllFiles.Count
    );

    for (u64 Index = 0; Index < AllFiles.Count; ++Index) {
        FMRangeList Matches = FuzzyFind(
            Scratch.MemPool,
            Filter,
            AllFiles.V[Index]->Head->String
        );

        if (Matches.Count == Matches.StrPartCount)
            Files.V[Files.Count++] = AllFiles.V[Index];
    }

    State->Cursor.Y = MIN(State->Cursor.Y, (i64) Files.Count);
    State->Mark = State->Cursor;

    UITag("menu_bar"_s8) {
        UISetNextFlags(UI_BOX_KIND_DRAW_BACKGROUND);

        UINamedRow("###files_toolbar"_s8) {
            UITag("pop"_s8) {
                UIPreferredWidth(UI_PX(26.0f, 1.0f)) {
                    UISignal AddSig = AppIconButton(
                        APP_ICON_KIND_ADD,
                        NULL,
                        "Add File or Folder###add_file"_s8
                    );

                    if (UI_CLICKED(AddSig)) {
                        AppCmd(
                            APP_COMMAND_KIND_RUN_COMMAND,
                            __Registers.CommandName = OpenCommandName
                        );
                    }
                }
            }

            AppCommandBindingButtons(OpenCommandName, ""_s8, 1, 0);
        }
    }

    Rect.Y0 += 1.0f;

    if (!Files.Count) {
        UISpacer(UI_PERCENT(1.0f, 0.0f));

        UIWidthFill() {
            UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                UITag("weak"_s8) {
                    UILabel(
                        AllFiles.Count
                            ? "No file agrees with the filter."_s8
                            : "There are no files in the list."_s8
                    );
                }
            }
        }

        if (!AllFiles.Count) {
            UIRow() {
                UISpacer(UI_PERCENT(1.0f, 0.0f));

                UITag("pop"_s8) {
                    UIPreferredWidth(UI_PX(26.0f, 1.0f)) {
                        UISignal AddSig = AppIconButton(
                            APP_ICON_KIND_ADD,
                            NULL,
                            "Add File or Folder###add_file_centre"_s8
                        );

                        if (UI_CLICKED(AddSig)) {
                            AppCmd(
                                APP_COMMAND_KIND_RUN_COMMAND,
                                __Registers.CommandName = OpenCommandName
                            );
                        }
                    }
                }

                AppCommandBindingButtons(OpenCommandName, ""_s8, 1, 0);
                UISpacer(UI_PERCENT(1.0f, 0.0f));
            }
        }

        UISpacer(UI_PERCENT(1.0f, 0.0f));
    } else {
        UIScrollListParameters Params = {};
        UIScrollListSignal ListSig = {};
        r1i64 Visible = {};
        Str8 AcceptedPath = {};

        Params.Kind = UI_SCROLL_LIST_KIND_ALL;
        Params.DimensionsPX = Length(Rect);
        Params.RowHeightPX = 1.0f;
        Params.CursorRange.Max.Y = (i64) Files.Count;
        Params.ItemRange = Rng((i64) 0, (i64) Files.Count);
        Params.CursorMinIsEmptySelection[AXIS_2D_Y] = TRUE;

        UIFocus(UI_FOCUS_KIND_ON) {
            UIScrollList(
                &Params,
                &ScrollPosition.Y,
                &State->Cursor,
                &State->Mark,
                &Visible,
                &ListSig
            ) {
                for (
                    i64 Row = Visible.Min;
                    Row <= Visible.Max && Row < (i64) Files.Count;
                    ++Row
                ) {
                    Str8 Path = StrSkipLastSlash(
                        Files.V[Row]->Head->String
                    );
                    b32 IsCursor = (State->Cursor.Y == Row + 1);

                    UITag(IsCursor ? "pop"_s8 : ""_s8) {
                        UISetNextChildLayoutAxis(AXIS_2D_X);

                        UIBox* RowBox = UIBuildBoxFromStrFmt(
                            (
                                UI_BOX_KIND_MOUSE_CLICKABLE |
                                UI_BOX_KIND_DRAW_BACKGROUND |
                                UI_BOX_KIND_DRAW_HOT_EFFECTS
                            ),
                            "###file_row_%p",
                            Files.V[Row]
                        );

                        UIParent(RowBox) {
                            UIWidthFill() {
                                UIBox* LabelBox = UILabel(Path).Box;

                                if (Filter.Size) {
                                    FMRangeList Matches = FuzzyFind(
                                        Scratch.MemPool,
                                        Filter,
                                        Path
                                    );

                                    UIBoxEquipFuzzyMatchRanges(
                                        LabelBox,
                                        &Matches
                                    );
                                }
                            }
                        }

                        UISignal RowSig = UISignalFromBox(RowBox);

                        if (UI_PRESSED(RowSig)) {
                            State->Cursor.Y = Row + 1;
                            State->Mark = State->Cursor;
                        }

                        if (UI_DOUBLE_CLICKED(RowSig))
                            AcceptedPath = Files.V[Row]->Head->String;
                    }
                }
            }

            if (
                UIIsFocusActive() &&
                State->Cursor.Y > 0 &&
                UISlotPress(UI_EVENT_ACTION_SLOT_ACCEPT)
            ) {
                AcceptedPath = Files.V[State->Cursor.Y - 1]->Head->String;
            }
        }

        if (State->Cursor.Y > 0) {
            Str8 Selected = Files.V[
                State->Cursor.Y - 1
            ]->Head->String;
            ConfigNode* Transient = ConfigNodeChildFromStr(
                ConfigNodeRoot(),
                "transient"_s8
            );
            ConfigNode* Flight = ConfigNodeChildFromStrOrAlloc(
                APP_STATE->Config,
                Transient,
                "flight"_s8
            );

            if (!StrMatch(Flight->Head->String, Selected, 0)) {
                ConfigNodeNewReplace(
                    APP_STATE->Config,
                    Flight,
                    Selected
                );
            }

            Registers()->FilePath = ArenaPushStrCpy(
                AppFrameMemPool(),
                Selected
            );
            Registers()->FlightIndex = (u32) (State->Cursor.Y - 1);
        }
    }

    ScrollPosition.Y.Offset = 0.0f;
    AppStoreViewScrollPosition(ScrollPosition);
    ReleaseScratch(Scratch);
}

FLAP_VIEW_UI_FUNC_SIG(FLAPViewsViewUI)
{
    TempArena Scratch = GetScratch(NULL, 0);
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);
    ConfigNode* QueryRoot = ConfigNodeChildFromStr(View, "query"_s8);
    ConfigNode* CmdRoot = ConfigNodeChildFromStr(QueryRoot, "cmd"_s8);
    FLAPViewsViewState* State = GetAppViewState(
        FLAPViewsViewState
    );
    ConfigNodePtrList ViewList = ConfigNodeTopLevelListFromStr(
        Scratch.MemPool,
        "view"_s8
    );
    ConfigNodePtrArray AllFiles = ConfigNodePtrArrayFromList(
        Scratch.MemPool,
        &ViewList
    );
    ConfigNodePtrArray Files = {};
    UIScrollPoint2D ScrollPosition = AppViewScrollPosition();
    Str8 OpenCommandName = APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_OPEN].String;
    Str8 Filter = {};

    if (
        VS->QueryIsOpen &&
        StrMatch(
            CmdRoot->Head->String,
            APP_COMMAND_KIND_INFO_TABLE[APP_COMMAND_KIND_FILTER].String,
            0
        )
    ) {
        Filter = Str(VS->QueryBuffer, VS->QueryStringSize);
    }

    Files.V = ArenaPushArrayZero(
        Scratch.MemPool,
        ConfigNode*,
        AllFiles.Count
    );

    for (u64 Index = 0; Index < AllFiles.Count; ++Index) {
        FMRangeList Matches = FuzzyFind(
            Scratch.MemPool,
            Filter,
            AllFiles.V[Index]->Head->String
        );

        if (Matches.Count == Matches.StrPartCount)
            Files.V[Files.Count++] = AllFiles.V[Index];
    }

    State->Cursor.Y = MIN(State->Cursor.Y, (i64) Files.Count);
    State->Mark = State->Cursor;

    UITag("menu_bar"_s8) {
        UISetNextFlags(UI_BOX_KIND_DRAW_BACKGROUND);

        UINamedRow("###views_toolbar"_s8) {
            UITag("pop"_s8) {
                UIPreferredWidth(UI_PX(26.0f, 1.0f)) {
                    UISignal AddSig = AppIconButton(
                        APP_ICON_KIND_ADD,
                        NULL,
                        "Add View###add_view"_s8
                    );

                    if (UI_CLICKED(AddSig)) {
                        AppCmd(
                            APP_COMMAND_KIND_RUN_COMMAND,
                            __Registers.CommandName = OpenCommandName
                        );
                    }
                }
            }

            AppCommandBindingButtons(OpenCommandName, ""_s8, 1, 0);
        }
    }

    Rect.Y0 += 1.0f;

    if (!Files.Count) {
        UISpacer(UI_PERCENT(1.0f, 0.0f));

        UIWidthFill() {
            UITextAlignment(UI_TEXT_ALIGN_CENTRE) {
                UITag("weak"_s8) {
                    UILabel(
                        AllFiles.Count
                            ? "No view agrees with the filter."_s8
                            : "There are no views in the list."_s8
                    );
                }
            }
        }

        if (!AllFiles.Count) {
            UIRow() {
                UISpacer(UI_PERCENT(1.0f, 0.0f));

                UITag("pop"_s8) {
                    UIPreferredWidth(UI_PX(26.0f, 1.0f)) {
                        UISignal AddSig = AppIconButton(
                            APP_ICON_KIND_ADD,
                            NULL,
                            "Add View###add_view_centre"_s8
                        );

                        if (UI_CLICKED(AddSig)) {
                            AppCmd(
                                APP_COMMAND_KIND_RUN_COMMAND,
                                __Registers.CommandName = OpenCommandName
                            );
                        }
                    }
                }

                AppCommandBindingButtons(OpenCommandName, ""_s8, 1, 0);
                UISpacer(UI_PERCENT(1.0f, 0.0f));
            }
        }

        UISpacer(UI_PERCENT(1.0f, 0.0f));
    } else {
        UIScrollListParameters Params = {};
        UIScrollListSignal ListSig = {};
        r1i64 Visible = {};
        Str8 AcceptedPath = {};

        Params.Kind = UI_SCROLL_LIST_KIND_ALL;
        Params.DimensionsPX = Length(Rect);
        Params.RowHeightPX = 1.0f;
        Params.CursorRange.Max.Y = (i64) Files.Count;
        Params.ItemRange = Rng((i64) 0, (i64) Files.Count);
        Params.CursorMinIsEmptySelection[AXIS_2D_Y] = TRUE;

        UIFocus(UI_FOCUS_KIND_ON) {
            UIScrollList(
                &Params,
                &ScrollPosition.Y,
                &State->Cursor,
                &State->Mark,
                &Visible,
                &ListSig
            ) {
                for (
                    i64 Row = Visible.Min;
                    Row <= Visible.Max && Row < (i64) Files.Count;
                    ++Row
                ) {
                    Str8 Path = StrSkipLastSlash(
                        Files.V[Row]->Head->String
                    );
                    b32 IsCursor = (State->Cursor.Y == Row + 1);

                    UITag(IsCursor ? "pop"_s8 : ""_s8) {
                        UISetNextChildLayoutAxis(AXIS_2D_X);

                        UIBox* RowBox = UIBuildBoxFromStrFmt(
                            (
                                UI_BOX_KIND_MOUSE_CLICKABLE |
                                UI_BOX_KIND_DRAW_BACKGROUND |
                                UI_BOX_KIND_DRAW_HOT_EFFECTS
                            ),
                            "###view_row_%p",
                            Files.V[Row]
                        );

                        UIParent(RowBox) {
                            b32 IsOpen = FALSE;

                            for (
                                Str8Node* N = APP_STATE->TabFilePaths.Head;
                                N && !IsOpen;
                                N = N->Next
                            ) {
                                IsOpen = PathMatchNormalised(
                                    N->String,
                                    Files.V[Row]->Head->String
                                );
                            }

                            UIPreferredWidth(UI_PX(2.0f, 1.0f)) {
                                UILabel(
                                    IsOpen
                                        ? APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_DOT]
                                        : ""_s8
                                );
                            }

                            UIWidthFill() {
                                UIBox* LabelBox = UILabel(Path).Box;

                                if (Filter.Size) {
                                    FMRangeList Matches = FuzzyFind(
                                        Scratch.MemPool,
                                        Filter,
                                        Path
                                    );

                                    UIBoxEquipFuzzyMatchRanges(
                                        LabelBox,
                                        &Matches
                                    );
                                }
                            }
                        }

                        UISignal RowSig = UISignalFromBox(RowBox);

                        if (UI_PRESSED(RowSig)) {
                            State->Cursor.Y = Row + 1;
                            State->Mark = State->Cursor;
                        }

                        if (UI_DOUBLE_CLICKED(RowSig))
                            AcceptedPath = Files.V[Row]->Head->String;
                    }
                }
            }

            if (
                UIIsFocusActive() &&
                State->Cursor.Y > 0 &&
                UISlotPress(UI_EVENT_ACTION_SLOT_ACCEPT)
            ) {
                AcceptedPath = Files.V[State->Cursor.Y - 1]->Head->String;
            }
        }

        if (State->Cursor.Y > 0) {
            Registers()->FilePath = ArenaPushStrCpy(
                AppFrameMemPool(),
                Files.V[State->Cursor.Y - 1]->Head->String
            );
        }

        if (AcceptedPath.Size) {
            AppCmd(
                APP_COMMAND_KIND_BUILD_TAB,
                __Registers.String = "view"_s8,
                __Registers.FilePath = AcceptedPath
            );
        }
    }

    ScrollPosition.Y.Offset = 0.0f;
    AppStoreViewScrollPosition(ScrollPosition);
    ReleaseScratch(Scratch);
}
