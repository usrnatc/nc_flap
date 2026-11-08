#include "nc_application.h"
#include "nc_arena.h"
#include "nc_config.h"
#include "nc_draw.h"
#include "nc_memory.h"
#include "nc_ui_components.h"

#include "flv_token.h"
#include "flv_run.h"

#include "flap_views.h"

INTERNAL FLAPFlightSet
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

INTERNAL Str8
FLAPStrFromTimeOffset(Arena* MemPool, u64 USecs)
{
    Str8 Result = "0s"_s8;
 
    if (USecs) {
        Result = ArenaPushStrFmt(
            MemPool,
            "+ %llu.%03llus",
            USecs / MILLION(1),
            (USecs % MILLION(1)) / THOUSAND(1)
        );
    }
 
    return Result;
}
 
INTERNAL b32
FLAPScalarKindIsChar(FLIScalarKind Kind)
{
    b32 Result = (
        Kind == FLI_SCALAR_KIND_CHAR4 ||
        Kind == FLI_SCALAR_KIND_CHAR16 ||
        Kind == FLI_SCALAR_KIND_CHAR64
    );
 
    return Result;
}

INTERNAL u64
FLAPTablePositionFromRow(FLVRenderTable const* Table, u64 Row)
{
    u64 Result = Row;
 
    if (Table->Mask) {
        u64 Low = 0;
        u64 High = FLVSeriesWordCount(Table->SampleCount);
 
        while (High - Low > 1) {
            u64 Mid = Low + (High - Low) / 2;
 
            if (Table->Rank[Mid] <= Row)
                Low = Mid;
            else
                High = Mid;
        }
 
        u64 Word = Table->Mask[Low];
 
        for (u64 Skip = Row - Table->Rank[Low]; Skip; --Skip)
            Word &= Word - 1;
 
        Result = (Low << 6) + Ctz64(Word);
    }
 
    return Result;
}

INTERNAL Str8
FLAPStrFromCell(
    Arena* MemPool,
    FLAPTableCtx* Data,
    u32 ColumnIndex,
    u64 Row,
    u64 Sample
) {
    FLVRenderTableColumn const* Column = &Data->Table->Columns[ColumnIndex];
    FLVColumn const* Ref = &Data->Columns[ColumnIndex];
    Str8 Result = "-"_s8;
 
    if (Column->IsMissing)
        return Result;
 
    if (Column->Column != FLV_COLUMN_NONE) {
        if (Ref->IsMissing)
            return Result;
 
        if (FLAPScalarKindIsChar(Ref->Kind)) {
            u8 const* Chars = (u8 const*) Ref->Data + Sample * Ref->ElementSize;
            u64 Size = 0;
 
            for ( ; Size < Ref->ElementSize && Chars[Size]; ++Size);
 
            Result = Str((u8*) Chars, Size);
        } else {
            f64 Value = FLVColumnF64At(Ref, Sample);
            b32 IsInteger = (
                Ref->Scale == 1.0 &&
                Ref->Kind != FLI_SCALAR_KIND_F32 &&
                Ref->Kind != FLI_SCALAR_KIND_F64
            );
 
            if (Value != Value) {
                // NOTE(nathan): no-op
            } else if (IsInteger) {
                Result = ArenaPushStrFmt(MemPool, "%lld", (i64) Value);
            } else {
                Result = ArenaPushStrFmt(MemPool, "%.3f", Value);
            }
        }
    } else {
        u8 const* Bytes = (u8 const*) Column->Values + Row * Column->Width;
 
        if (FLAPScalarKindIsChar(Column->Kind)) {
            u64 Size = 0;
 
            for ( ; Size < Column->Width && Bytes[Size]; ++Size);
 
            Result = Str((u8*) Bytes, Size);
        } else if (Column->Kind == FLI_SCALAR_KIND_F64) {
            f64 Value = 0.0;
 
            MemCpy(&Value, Bytes, sizeof(Value));
 
            if (Value == Value)
                Result = ArenaPushStrFmt(MemPool, "%.3f", Value);
        } else {
            i64 Value = 0;
 
            MemCpy(&Value, Bytes, sizeof(Value));
 
            if (Value != FLV_I64_HOLE)
                Result = ArenaPushStrFmt(MemPool, "%lld", Value);
        }
    }
 
    return Result;
}

INTERNAL
UI_BOX_CUSTOM_DRAW(FLAPFlaggedRowDraw)
{
    v4f32 Colour = UIColourFromTagsKeyName(Box->TagsKey, "hover"_s8);
 
    UNUSED(UserData);
    Colour.W *= 0.5f;
    DrawRect(Box->Rect, Colour, 0.0f);
}

INTERNAL b32
FLAPTimeIsFlagged(FLAPTableCtx* Ctx, u64 TimeUSecs)
{
    b32 Result = FALSE;
 
    for (
        u32 FlagIndex = 0; 
        FlagIndex < Ctx->FlagsCount && !Result; 
        ++FlagIndex
    ) {
        FLVRenderFlag const* Flag = Ctx->Flags[FlagIndex];
 
        for (u64 Index = 0; Index < Flag->Count && !Result; ++Index)
            Result = InRange(Flag->Ranges[Index], TimeUSecs);
    }
 
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
FLAPTableUI(
    FLAPViewTabCtx* Ctx,
    u32 NodeIndex,
    FLVProgram const* Program,
    FLVRenderItem* Items,
    u32 HeadEmit,
    r2f32 Rect
) {
    TempArena Scratch = GetScratch(NULL, 0);
    FLVRenderTable const* Table = &Items[HeadEmit].Table;
    FLAPTableCtx TableCtx = {};
    u32 ColumnCount = Table->ColumnCount + 1;
    f32** Percents = FLAPTablePercents(Ctx, NodeIndex, ColumnCount);
 
    TableCtx.Table = Table;
    TableCtx.Program = Program;
    TableCtx.Columns = ArenaPushArrayZero(
        Scratch.MemPool,
        FLVColumn,
        Table->ColumnCount
    );
    TableCtx.Flags = ArenaPushArrayZero(
        Scratch.MemPool,
        FLVRenderFlag const*,
        Program->EmitCount
    );
 
    FLVColumnInit(
        Ctx->Acc,
        Ctx->Keys[Ctx->Flight],
        Ctx->FLI,
        Table->TimeColumn,
        0,
        &TableCtx.Time
    );
 
    for (u32 Index = 0; Index < Table->ColumnCount; ++Index) {
        FLVRenderTableColumn const* Column = &Table->Columns[Index];
 
        if (Column->Column != FLV_COLUMN_NONE) {
            FLVColumnInit(
                Ctx->Acc,
                Ctx->Keys[Ctx->Flight],
                Ctx->FLI,
                Column->Column,
                0,
                &TableCtx.Columns[Index]
            );
        }
    }
 
    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (
            Program->Emits[Index].DisplayKind == TOKEN_KIND_FLAG &&
            !Items[Index].IsMissing
        ) {
            TableCtx.Flags[TableCtx.FlagsCount++] = &Items[Index].Flag;
        }
    }
 
    if (Table->RowCount && !TableCtx.Time.IsMissing) {
        u64 StartSample = FLVSeriesSampleIndex(
            Table->Ranges,
            Table->RangesCount,
            FLAPTablePositionFromRow(Table, 0)
        );
 
        TableCtx.StartTimeUSecs = FLVTimeAt(&TableCtx.Time, StartSample);
    }
 
    UIScrollListParameters Params = {};
    UIScrollListSignal ListSig = {};
    r1i64 Visible = {};
 
    Params.Kind = UI_SCROLL_LIST_KIND_ALL;
    Params.DimensionsPX = Vec(Length(Rect).X, Length(Rect).Y - 1.0f);
    Params.RowHeightPX = 1.0f;
    Params.CursorRange.Max.Y = (i64) Table->RowCount;
    Params.ItemRange = Rng((i64) 0, (i64) Table->RowCount);
    Params.CursorMinIsEmptySelection[AXIS_2D_Y] = TRUE;
 
    UITableFmt(ColumnCount, Percents, "###table_%u", NodeIndex) {
        UITag("menu_bar"_s8) {
            UISetNextFlags(UI_BOX_KIND_DRAW_BACKGROUND);
 
            UITableVector() {
                UITableCell() {
                    UILabel("time"_s8);
                }
 
                for (u32 Index = 0; Index < Table->ColumnCount; ++Index) {
                    UITableCell() {
                        UILabel(
                            Program->Strings[
                                Table->Columns[Index].LabelStringIndex
                            ]
                        );
                    }
                }
            }
        }
 
        UIScrollList(
            &Params,
            &Ctx->State->Scroll[NodeIndex],
            &Ctx->State->Cursor[NodeIndex],
            &Ctx->State->Mark[NodeIndex],
            &Visible,
            &ListSig
        ) {
            for (
                i64 Row = Visible.Min;
                Row <= Visible.Max && Row < (i64) Table->RowCount;
                ++Row
            ) {
                b32 IsCursor = (Ctx->State->Cursor[NodeIndex].Y == Row + 1);
                u64 Sample = FLVSeriesSampleIndex(
                    Table->Ranges,
                    Table->RangesCount,
                    FLAPTablePositionFromRow(Table, (u64) Row)
                );
                u64 TimeUSecs = FLVTimeAt(&TableCtx.Time, Sample);
 
                UITag(
                    IsCursor 
                        ? "pop"_s8 
                        : ""_s8
                ) {
                    UISetNextFlags(
                        UI_BOX_KIND_MOUSE_CLICKABLE |
                        UI_BOX_KIND_DRAW_BACKGROUND |
                        UI_BOX_KIND_DRAW_HOT_EFFECTS
                    );
 
                    UIBox* RowBox = UITableVectorBegin();
 
                    if (FLAPTimeIsFlagged(&TableCtx, TimeUSecs)) {
                        UIBoxEquipCustomDraw(
                            RowBox, 
                            FLAPFlaggedRowDraw, 
                            NULL
                        );
                    }

                    UITextAlignment(UI_TEXT_ALIGN_RIGHT) {
                        UITableCell() {
                            UILabel(
                                "%S###c0",
                                PRINT_STR(
                                    FLAPStrFromTimeOffset(
                                        UIBuildMemPool(),
                                        TimeUSecs - TableCtx.StartTimeUSecs
                                    )
                                )
                            );
                        }
 
                        for (
                            u32 Index = 0; 
                            Index < Table->ColumnCount; 
                            ++Index
                        ) {
                            b32 IsChar = FLAPScalarKindIsChar(
                                Table->Columns[Index].Kind
                            );
 
                            UITextAlignment(
                                IsChar
                                    ? UI_TEXT_ALIGN_LEFT
                                    : UI_TEXT_ALIGN_RIGHT
                            ) {
                                UITableCell() {
                                    UILabel(
                                        "%S###c%u",
                                        PRINT_STR(
                                            FLAPStrFromCell(
                                                UIBuildMemPool(),
                                                &TableCtx,
                                                Index,
                                                (u64) Row,
                                                Sample
                                            )
                                        ),
                                        Index + 1
                                    );
                                }
                            }
                        }
                    }
 
                    UISignal RowSig = UITableVectorEnd();
 
                    if (UI_PRESSED(RowSig)) {
                        Ctx->State->Cursor[NodeIndex].Y = Row + 1;
                        Ctx->State->Mark[NodeIndex] = Ctx->State->Cursor[
                            NodeIndex
                        ];
                    }
                }
            }
        }
    }
 
    ReleaseScratch(Scratch);
}

INTERNAL Str8
FLAPFlagLine(
    Arena* MemPool,
    FLVProgram const* Program,
    FLVRenderItem* Items,
    u64 Row
) {
    Str8 Result = ""_s8;
 
    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (Program->Emits[Index].DisplayKind != TOKEN_KIND_FLAG)
            continue;
 
        FLVRenderItem const* Item = &Items[Index];
        Str8 Label = Program->Strings[Program->Emits[Index].LabelStringIndex];
        u64 Lines = Item->IsMissing ? 1 : 1 + Item->Flag.Count;
 
        if (Row >= Lines) {
            Row -= Lines;
            continue;
        }
 
        if (!Row) {
            Result = Item->IsMissing
                ? ArenaPushStrFmt(MemPool, "%S: missing", PRINT_STR(Label))
                : ArenaPushStrFmt(
                    MemPool,
                    "%S: %llu, total %S",
                    PRINT_STR(Label),
                    Item->Flag.Count,
                    PRINT_STR(
                        FLAPStrFromTimeOffset(
                            MemPool,
                            Item->Flag.TotalDurationUSecs
                        )
                    )
                );
        } else {
            r1u64 Range = Item->Flag.Ranges[Row - 1];
 
            Result = ArenaPushStrFmt(
                MemPool,
                "  %S for %S",
                PRINT_STR(FLAPStrFromTimeOffset(MemPool, Range.Min)),
                PRINT_STR(FLAPStrFromTimeOffset(MemPool, Length(Range)))
            );
        }
 
        break;
    }
 
    return Result;
}

INTERNAL void
FLAPFlagListUI(
    FLAPViewTabCtx* Ctx,
    u32 NodeIndex,
    FLVProgram const* Program,
    FLVRenderItem* Items,
    r2f32 Rect
) {
    u64 LineCount = 0;
 
    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (Program->Emits[Index].DisplayKind == TOKEN_KIND_FLAG) {
            LineCount += Items[Index].IsMissing
                ? 1
                : 1 + Items[Index].Flag.Count;
        }
    }
 
    UIScrollListParameters Params = {};
    UIScrollListSignal ListSig = {};
    r1i64 Visible = {};
 
    Params.Kind = UI_SCROLL_LIST_KIND_ALL;
    Params.DimensionsPX = Length(Rect);
    Params.RowHeightPX = 1.0f;
    Params.CursorRange.Max.Y = (i64) LineCount;
    Params.ItemRange = Rng((i64) 0, (i64) LineCount);
    Params.CursorMinIsEmptySelection[AXIS_2D_Y] = TRUE;
 
    UIScrollList(
        &Params,
        &Ctx->State->Scroll[NodeIndex],
        &Ctx->State->Cursor[NodeIndex],
        &Ctx->State->Mark[NodeIndex],
        &Visible,
        &ListSig
    ) {
        for (
            i64 Row = Visible.Min;
            Row <= Visible.Max && Row < (i64) LineCount;
            ++Row
        ) {
            UIWidthFill() {
                UILabel(
                    FLAPFlagLine(
                        UIBuildMemPool(),
                        Program,
                        Items,
                        (u64) Row
                    )
                );
            }
        }
    }
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
 
            if (TableEmit != U32_MAX)
                FLAPTableUI(Ctx, NodeIndex, Program, Items, TableEmit, Rect);
            else if (FlagCount)
                FLAPFlagListUI(Ctx, NodeIndex, Program, Items, Rect);
        }
    }
}

INTERNAL void
FLAPLayoutUI(FLAPViewTabCtx* Ctx, u32 NodeIndex, r2f32 Rect)
{
    FLVLayoutNode const* Node = &Ctx->Unit->Lowered.Layout[NodeIndex];
 
    if (Node->AnalysisIndex != U32_MAX) {
        FLAPAnalysisUI(Ctx, NodeIndex, Rect);
 
        return;
    }
 
    Axis2D Axis = (Node->Depth & 1) ? AXIS_2D_X : AXIS_2D_Y;
    u32 Count = MAX(Node->ChildCount, 1);
    f32 Share = 1.0f / (f32) Count;
    f32 Step = Length(Rect).V[Axis] * Share;
 
    UISetNextChildLayoutAxis(Axis);
 
    UIBox* Box = UIBuildBoxFromStrFmt(0, "###layout_%u", NodeIndex);
 
    UIParent(Box) {
        for (u32 Child = 0; Child < Node->ChildCount; ++Child) {
            r2f32 ChildRect = Rect;
 
            ChildRect.Point0.V[Axis] = Rect.Point0.V[Axis] + Step * (f32) Child;
            ChildRect.Point1.V[Axis] = ChildRect.Point0.V[Axis] + Step;
 
            UISetNextPreferredSize(Axis, UI_PERCENT(Share, 0.0f));
            UISetNextPreferredSize(FLIP_AXIS(Axis), UI_PERCENT(1.0f, 0.0f));
            FLAPLayoutUI(Ctx, Node->HeadChild + Child, ChildRect);
        }
    }
}

void
FLAPViewTabUI(r2f32 Rect)
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
 
            UISetNextPreferredSize(AXIS_2D_X, UI_PERCENT(1.0f, 0.0f));
            UISetNextPreferredSize(AXIS_2D_Y, UI_PERCENT(1.0f, 0.0f));
            FLAPLayoutUI(&Ctx, 0, Rect);
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
