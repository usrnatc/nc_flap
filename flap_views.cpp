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

INTERNAL b32
FLAPFlightIsChecked(ConfigNode* File)
{
    Str8 Value = ConfigNodeChildFromStr(File, "enabled"_s8)->Head->String;
    b32 Result = (!Value.Size || U64FromStr(Value) != 0);

    return Result;
}

INTERNAL void
FLAPFlightCheckStore(ConfigNode* File, b32 IsChecked)
{
    ConfigNode* Enabled = ConfigNodeChildFromStrOrAlloc(
        APP_STATE->Config,
        File,
        "enabled"_s8
    );

    ConfigNodeNewReplace(
        APP_STATE->Config,
        Enabled,
        IsChecked ? "1"_s8 : "0"_s8
    );
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
    u32 CursorFlight = U32_MAX;

    Result.Selected = U32_MAX;
    Result.Keys.Count = Files.Count;
    Result.Keys.V = ArenaPushArrayZero(MemPool, FLIKey, Result.Keys.Count);

    for (u64 Index = 0; Index < Files.Count; ++Index) {
        Str8 Path = Files.V[Index]->Head->String;

        Result.Keys.V[Index] = FLIKeyFromPathTimeStamp(Path, 0);

        if (!FLAPFlightIsChecked(Files.V[Index]))
            continue;

        if (Result.Selected == U32_MAX)
            Result.Selected = (u32) Index;

        if (SelectedPath.Size && PathMatchNormalised(Path, SelectedPath))
            CursorFlight = (u32) Index;
    }

    if (CursorFlight != U32_MAX)
        Result.Selected = CursorFlight;
 
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

INTERNAL FLAPGraphColumns*
FLAPGraphColumnsArray(FLAPViewTabCtx* Ctx, u32 NodeIndex, u32 Count)
{
    FLAPViewTabState* State = Ctx->State;

    if (State->GraphColumnsCount[NodeIndex] != Count) {
        State->GraphColumns[NodeIndex] = ArenaPushArrayZero(
            AppViewMemPool(),
            FLAPGraphColumns,
            Count
        );
        State->GraphColumnsCount[NodeIndex] = Count;
    }

    return State->GraphColumns[NodeIndex];
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
            u32 CurveCount = 0;
 
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
 
                if (
                    Kind == TOKEN_KIND_GRAPH &&
                    !Items[Index].IsMissing &&
                    Items[Index].Kind == FLV_RENDER_KIND_CURVE
                ) {
                    ++CurveCount;
                }
            }
 
            if (CurveCount) {
                FLAPGraphParams Params = {};

                Params.Acc = Ctx->Acc;
                Params.FLI = Ctx->FLI;
                Params.Program = Program;
                Params.Items = Items;
                Params.State = &Ctx->State->Graph;
                Params.Columns = FLAPGraphColumnsArray(
                    Ctx,
                    NodeIndex,
                    Program->EmitCount
                );
                Params.MemPool = AppViewMemPool();
                Params.UnitHash = Ctx->Unit->Hash;
                Params.Flight = Ctx->Keys[Ctx->Flight];
                Params.Key = NodeIndex;
                FLAPGraph(&Params, Rect);
            } else if (TableEmit != U32_MAX) {
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

APP_VIEW_UI_FUNC_DEF(FLVView)
{
    TempArena Scratch = GetScratch(NULL, 0);
    FLAPViewTabState* State = GetAppViewState(FLAPViewTabState);
    Access* Acc = APP_STATE->FrameAccess;
    Str8 Path = AppViewFilePath();
    FLAPFlightSet Flights = FLAPFlightSetFromConfig(Scratch.MemPool);
    ConfigNodePtrList SearchPathList = ConfigNodeTopLevelListFromStr(
        Scratch.MemPool,
        "flv_search_path"_s8
    );
    ConfigNodePtrArray SearchPathNodes = ConfigNodePtrArrayFromList(
        Scratch.MemPool,
        &SearchPathList
    );
    Str8* SearchPaths = ArenaPushArray(
        Scratch.MemPool,
        Str8,
        SearchPathNodes.Count
    );

    for (u64 Index = 0; Index < SearchPathNodes.Count; ++Index)
        SearchPaths[Index] = SearchPathNodes.V[Index]->Head->String;

    FLVUnit* Unit = FLVUnitFromPath(
        Acc,
        Path,
        Flights.Keys.V,
        Flights.Keys.Count,
        SearchPaths,
        SearchPathNodes.Count,
        0
    );
    Str8 Message = ""_s8;
 
    if (!Unit) {
        Message = "compiling..."_s8;
        AppRequestFrame();
    } else if (Unit->HasError) {
        FLAPErrorListParams Params = {};

        Params.Errors = &Unit->Errors;
        Params.Scroll = &State->ErrorScroll;
        Params.Cursor = &State->ErrorCursor;
        Params.Mark = &State->ErrorMark;
        FLAPErrorList(&Params, Rect);
        State->ErrorScroll.Offset = 0.0f;
    } else if (!Flights.Keys.Count) {
        Message = "There are no flights in the list."_s8;
    } else if (Flights.Selected == U32_MAX) {
        Message = "No flight is selected."_s8;
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

            UIFocus(UI_FOCUS_KIND_ON) {
                if (UIIsFocusActive() && UIText('z'))
                    State->Graph.IsFitted = FALSE;
            }

            if (!State->Graph.IsFitted) {
                State->Graph.FitRangeUSecs = {};
                State->Graph.IsFitReady = TRUE;
            }
 
            UISetNextPreferredSize(AXIS_2D_X, UI_PERCENT(1.0f, 0.0f));
            UISetNextPreferredSize(AXIS_2D_Y, UI_PERCENT(1.0f, 0.0f));
            FLAPLayoutUI(&Ctx, Rect);

            if (
                !State->Graph.IsFitted &&
                State->Graph.IsFitReady &&
                State->Graph.FitRangeUSecs.Min < State->Graph.FitRangeUSecs.Max
            ) {
                State->Graph.TimeRangeUSecs = State->Graph.FitRangeUSecs;
                State->Graph.IsFitted = TRUE;
                AppRequestFrame();
            }

            if (
                State->Graph.HasCursor != State->Graph.HasNextCursor ||
                State->Graph.CursorTimeUSecs != State->Graph.NextCursorTimeUSecs
            ) {
                AppRequestFrame();
            }

            State->Graph.HasCursor = State->Graph.HasNextCursor;
            State->Graph.CursorTimeUSecs = State->Graph.NextCursorTimeUSecs;
            State->Graph.HasNextCursor = FALSE;
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

INTERNAL void
FLAPExplorerCollapseToggle(ConfigNode* View, Str8 Key)
{
    ConfigNode* Collapsed = ConfigNodeChildFromStrOrAlloc(
        APP_STATE->Config,
        View,
        "collapsed"_s8
    );
    ConfigNode* Node = ConfigNodeChildFromStr(Collapsed, Key);

    if (Node != EMPTY_CFG_NODE_VALUE)
        ConfigNodeRelease(APP_STATE->Config, Node);
    else
        ConfigNodeNew(APP_STATE->Config, Collapsed, Key);
}

INTERNAL void
FLAPExplorerPushList(
    Arena* MemPool,
    FLAPExplorerRowArray* Rows,
    ConfigNode* Collapsed,
    ConfigNodePtrArray All,
    Str8 ListName,
    Str8 Label,
    Str8 Filter,
    b32 IsFlights
) {
    ConfigNode** Files = ArenaPushArray(MemPool, ConfigNode*, All.Count);
    ConfigNode** Sorted = ArenaPushArray(MemPool, ConfigNode*, All.Count);
    u32* FolderOf = ArenaPushArray(MemPool, u32, All.Count);
    FLAPExplorerFolder* Folders = ArenaPushArrayZero(
        MemPool,
        FLAPExplorerFolder,
        All.Count
    );
    u32 Count = 0;
    u32 FoldersCount = 0;
    u32 Last = 0;
    u32 Offset = 0;
    u32 Checked = 0;

    for (u64 Index = 0; Index < All.Count; ++Index) {
        Str8 Path = All.V[Index]->Head->String;

        if (Filter.Size) {
            FMRangeList Matches = FuzzyFind(MemPool, Filter, Path);

            if (Matches.Count != Matches.StrPartCount)
                continue;
        }

        Str8 Folder = StrChopLastSlash(Path);
        u32 FolderIndex = Last;

        if (!FoldersCount || !StrMatch(Folders[Last].Path, Folder, 0)) {
            for (FolderIndex = 0; FolderIndex < FoldersCount; ++FolderIndex) {
                if (StrMatch(Folders[FolderIndex].Path, Folder, 0))
                    break;
            }

            if (FolderIndex == FoldersCount)
                Folders[FoldersCount++].Path = Folder;
        }

        Last = FolderIndex;
        ++Folders[FolderIndex].Count;
        Files[Count] = All.V[Index];
        FolderOf[Count] = FolderIndex;
        ++Count;
    }

    for (u32 Index = 0; Index < FoldersCount; ++Index) {
        Folders[Index].First = Offset;
        Offset += Folders[Index].Count;
        Folders[Index].Count = 0;
    }

    for (u32 Index = 0; Index < Count; ++Index) {
        FLAPExplorerFolder* Folder = &Folders[FolderOf[Index]];
        b32 IsChecked = IsFlights && FLAPFlightIsChecked(Files[Index]);

        Sorted[Folder->First + Folder->Count++] = Files[Index];
        Folder->Checked += IsChecked;
        Checked += IsChecked;
    }

    FLAPExplorerRow* Root = &Rows->V[Rows->Count++];

    Root->Files = Sorted;
    Root->Label = Label;
    Root->CollapseKey = ListName;
    Root->FilesCount = Count;
    Root->CheckedCount = Checked;
    Root->Kind = FLAP_EXPLORER_ROW_KIND_ROOT;
    Root->IsFlights = IsFlights;
    Root->IsExpanded = (
        Filter.Size ||
        ConfigNodeChildFromStr(Collapsed, ListName) == EMPTY_CFG_NODE_VALUE
    );

    if (!Root->IsExpanded)
        return;

    for (u32 Index = 0; Index < FoldersCount; ++Index) {
        FLAPExplorerFolder* Folder = &Folders[Index];
        FLAPExplorerRow* Row = &Rows->V[Rows->Count++];

        Row->Files = Sorted + Folder->First;
        Row->Label = StrSkipLastSlash(Folder->Path);
        Row->CollapseKey = ArenaPushStrFmt(
            MemPool,
            "%S:%S",
            PRINT_STR(ListName),
            PRINT_STR(Folder->Path)
        );
        Row->FilesCount = Folder->Count;
        Row->CheckedCount = Folder->Checked;
        Row->Kind = FLAP_EXPLORER_ROW_KIND_FOLDER;
        Row->IsFlights = IsFlights;
        Row->IsExpanded = (
            Filter.Size ||
            ConfigNodeChildFromStr(
                Collapsed,
                Row->CollapseKey
            ) == EMPTY_CFG_NODE_VALUE
        );

        if (!Row->IsExpanded)
            continue;

        for (u32 File = 0; File < Folder->Count; ++File) {
            FLAPExplorerRow* FileRow = &Rows->V[Rows->Count++];

            FileRow->Files = Sorted + Folder->First + File;
            FileRow->Label = StrSkipLastSlash(
                FileRow->Files[0]->Head->String
            );
            FileRow->FilesCount = 1;
            FileRow->CheckedCount = (
                IsFlights && FLAPFlightIsChecked(FileRow->Files[0])
            );
            FileRow->Kind = FLAP_EXPLORER_ROW_KIND_FILE;
            FileRow->Depth = 1;
            FileRow->IsFlights = IsFlights;
        }
    }
}

APP_VIEW_UI_FUNC_DEF(Explorer)
{
    TempArena Scratch = GetScratch(NULL, 0);
    ConfigNode* View = ConfigNodeFromID(Registers()->View);
    AppViewState* VS = AppViewStateFromConfig(View);
    ConfigNode* QueryRoot = ConfigNodeChildFromStr(View, "query"_s8);
    ConfigNode* CmdRoot = ConfigNodeChildFromStr(QueryRoot, "cmd"_s8);
    ConfigNode* Collapsed = ConfigNodeChildFromStr(View, "collapsed"_s8);
    FLAPExplorerViewState* State = GetAppViewState(FLAPExplorerViewState);
    ConfigNodePtrList FlightList = ConfigNodeTopLevelListFromStr(
        Scratch.MemPool,
        "file"_s8
    );
    ConfigNodePtrList ViewList = ConfigNodeTopLevelListFromStr(
        Scratch.MemPool,
        "flv_file"_s8
    );
    ConfigNodePtrArray Flights = ConfigNodePtrArrayFromList(
        Scratch.MemPool,
        &FlightList
    );
    ConfigNodePtrArray Views = ConfigNodePtrArrayFromList(
        Scratch.MemPool,
        &ViewList
    );
    FLAPExplorerRowArray Rows = {};
    FLAPExplorerRow* ToggleExpand = NULL;
    FLAPExplorerRow* ToggleCheck = NULL;
    FLAPExplorerRow* Accepted = NULL;
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

    // NOTE(nc): two headers, one divider, and a list has no more folders
    //         : than files
    Rows.V = ArenaPushArrayZero(
        Scratch.MemPool,
        FLAPExplorerRow,
        2 * (Flights.Count + Views.Count) + 3
    );
    FLAPExplorerPushList(
        Scratch.MemPool,
        &Rows,
        Collapsed,
        Flights,
        "file"_s8,
        "FLIGHTS"_s8,
        Filter,
        TRUE
    );
    Rows.V[Rows.Count++].Kind = FLAP_EXPLORER_ROW_KIND_DIVIDER;
    FLAPExplorerPushList(
        Scratch.MemPool,
        &Rows,
        Collapsed,
        Views,
        "flv_file"_s8,
        "VIEWS"_s8,
        Filter,
        FALSE
    );

    State->Cursor.Y = MIN(State->Cursor.Y, (i64) Rows.Count);
    State->Mark = State->Cursor;

    UITag("menu_bar"_s8) {
        UISetNextFlags(UI_BOX_KIND_DRAW_BACKGROUND);

        UINamedRow("###explorer_toolbar"_s8) {
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

    UIScrollListParameters Params = {};
    UIScrollListSignal ListSig = {};
    r1i64 Visible = {};
    i64 PrevCursorY = State->Cursor.Y;

    Params.Kind = UI_SCROLL_LIST_KIND_ALL;
    Params.DimensionsPX = Length(Rect);
    Params.RowHeightPX = 1.0f;
    Params.CursorRange.Max.Y = (i64) Rows.Count;
    Params.ItemRange = Rng((i64) 0, (i64) Rows.Count);
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
            // NOTE(nc): the cursor does not stop on the divider. the divider
            //         : is not the first row and not the last row.
            if (
                State->Cursor.Y > 0 &&
                Rows.V[State->Cursor.Y - 1].Kind == FLAP_EXPLORER_ROW_KIND_DIVIDER
            ) {
                State->Cursor.Y += (State->Cursor.Y >= PrevCursorY) 
                    ? 1 
                    : -1;
                State->Mark = State->Cursor;
            }

            for (
                i64 Index = Visible.Min;
                Index <= Visible.Max && Index < (i64) Rows.Count;
                ++Index
            ) {
                FLAPExplorerRow* Row = &Rows.V[Index];

                if (Row->Kind == FLAP_EXPLORER_ROW_KIND_DIVIDER) {
                    UIDivider(UI_PX(1.0f, 1.0f));
                    continue;
                }

                b32 IsFile = (Row->Kind == FLAP_EXPLORER_ROW_KIND_FILE);
                b32 IsHeader = (Row->Kind == FLAP_EXPLORER_ROW_KIND_ROOT);
                b32 IsCursor = (State->Cursor.Y == Index + 1);
                UIBoxKind RowFlags = UI_BOX_KIND_MOUSE_CLICKABLE;

                if (IsCursor || IsHeader) {
                    RowFlags |= (
                        UI_BOX_KIND_DRAW_BACKGROUND |
                        UI_BOX_KIND_DRAW_HOT_EFFECTS
                    );
                }

                UITag(
                    IsCursor 
                        ? "pop"_s8 
                        : IsHeader 
                            ? "menu_bar"_s8 
                            : ""_s8
                ) {
                    UITag(
                        (IsHeader && !IsCursor) 
                            ? "alt"_s8 
                            : ""_s8
                    ) {
                        UISetNextChildLayoutAxis(AXIS_2D_X);

                        UIBox* RowBox = UIBuildBoxFromStrFmt(
                            RowFlags,
                            "###explorer_row_%u_%u_%p",
                            Row->Kind,
                            Row->IsFlights,
                            Row->FilesCount ? Row->Files[0] : NULL
                        );

                        UIParent(RowBox) {
                            UITextPadding(0.0f) {
                                UISpacer(
                                    UI_PX(1.0f + 2.0f * (f32) Row->Depth, 1.0f)
                                );

                                if (!IsFile) {
                                    UIPreferredWidth(UI_PX(2.0f, 1.0f)) {
                                        UIFocus(UI_FOCUS_KIND_OFF) {
                                            UISignal ExpanderSig = UIExpander(
                                                Row->IsExpanded,
                                                "###expander"_s8
                                            );

                                            if (UI_PRESSED(ExpanderSig))
                                                ToggleExpand = Row;
                                        }
                                    }
                                } else if (Row->IsFlights) {
                                    UISpacer(UI_PX(2.0f, 1.0f));
                                } else {
                                    b32 IsOpen = FALSE;

                                    for (
                                        Str8Node* N = APP_STATE->TabFilePaths.Head;
                                        N && !IsOpen;
                                        N = N->Next
                                    ) {
                                        IsOpen = PathMatchNormalised(
                                            N->String,
                                            Row->Files[0]->Head->String
                                        );
                                    }

                                    UIPreferredWidth(UI_PX(2.0f, 1.0f)) {
                                        UILabel(
                                            IsOpen
                                                ? APP_ICON_KIND_TEXT_TABLE[APP_ICON_KIND_DOT]
                                                : ""_s8
                                        );
                                    }
                                }

                                if (Row->IsFlights) {
                                    AppIconKind Icon = !Row->CheckedCount
                                        ? APP_ICON_KIND_CHECK_HOLLOW
                                        : (Row->CheckedCount == Row->FilesCount)
                                            ? APP_ICON_KIND_CHECK_FILLED
                                            : APP_ICON_KIND_CHECK_PARTIAL;

                                    UIPreferredWidth(UI_PX(4.0f, 1.0f)) {
                                        UISetNextHoverCursor(CURSOR_KIND_HAND_POINT);

                                        UIBox* CheckBox = UIBuildBoxFromStr(
                                            (
                                                UI_BOX_KIND_MOUSE_CLICKABLE |
                                                UI_BOX_KIND_DRAW_TEXT
                                            ),
                                            "###check"_s8
                                        );

                                        UIBoxEquipDisplayStr(
                                            CheckBox,
                                            APP_ICON_KIND_TEXT_TABLE[Icon]
                                        );

                                        UISignal CheckSig = UISignalFromBox(CheckBox);

                                        if (UI_PRESSED(CheckSig))
                                            ToggleCheck = Row;
                                    }
                                }

                                UIWidthFill() {
                                    UIBox* LabelBox = UILabel(Row->Label).Box;

                                    if (Filter.Size && IsFile) {
                                        FMRangeList Matches = FuzzyFind(
                                            Scratch.MemPool,
                                            Filter,
                                            Row->Label
                                        );

                                        UIBoxEquipFuzzyMatchRanges(
                                            LabelBox,
                                            &Matches
                                        );
                                    }
                                }
                            }
                        }

                        UISignal RowSig = UISignalFromBox(RowBox);

                        if (UI_PRESSED(RowSig)) {
                            State->Cursor.Y = Index + 1;
                            State->Mark = State->Cursor;
                        }

                        if (UI_DOUBLE_CLICKED(RowSig))
                            Accepted = Row;
                    }
                }
            }
        }

        if (
            UIIsFocusActive() &&
            State->Cursor.Y > 0 &&
            UISlotPress(UI_EVENT_ACTION_SLOT_ACCEPT)
        ) {
            Accepted = &Rows.V[State->Cursor.Y - 1];
        }
    }

    if (State->Cursor.Y > 0) {
        FLAPExplorerRow* Row = &Rows.V[State->Cursor.Y - 1];

        if (Row->Kind == FLAP_EXPLORER_ROW_KIND_FILE) {
            Str8 Path = Row->Files[0]->Head->String;

            if (Row->IsFlights) {
                ConfigNode* Transient = ConfigNodeChildFromStr(
                    ConfigNodeRoot(),
                    "transient"_s8
                );
                ConfigNode* Flight = ConfigNodeChildFromStrOrAlloc(
                    APP_STATE->Config,
                    Transient,
                    "flight"_s8
                );

                if (!StrMatch(Flight->Head->String, Path, 0))
                    ConfigNodeNewReplace(APP_STATE->Config, Flight, Path);
            }

            Registers()->FilePath = ArenaPushStrCpy(AppFrameMemPool(), Path);
        }
    }

    if (Accepted) {
        if (Accepted->Kind != FLAP_EXPLORER_ROW_KIND_FILE) {
            ToggleExpand = Accepted;
        } else if (!Accepted->IsFlights) {
            AppCmd(
                APP_COMMAND_KIND_BUILD_TAB,
                __Registers.String = "flv_view"_s8,
                __Registers.FilePath = Accepted->Files[0]->Head->String
            );
        }
    }

    if (ToggleExpand && !Filter.Size)
        FLAPExplorerCollapseToggle(View, ToggleExpand->CollapseKey);

    if (ToggleCheck) {
        b32 IsChecked = (ToggleCheck->CheckedCount != ToggleCheck->FilesCount);

        for (u32 Index = 0; Index < ToggleCheck->FilesCount; ++Index)
            FLAPFlightCheckStore(ToggleCheck->Files[Index], IsChecked);
    }

    ScrollPosition.Y.Offset = 0.0f;
    AppStoreViewScrollPosition(ScrollPosition);
    ReleaseScratch(Scratch);
}
