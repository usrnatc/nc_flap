#include "nc_application.h"
#include "nc_arena.h"
#include "nc_config.h"
#include "nc_draw.h"
#include "nc_memory.h"
#include "nc_ui_components.h"

#include "flv_token.h"
#include "flv_run.h"

#include "flap_widgets.h"

internal Str8
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
 
internal b32
FLAPScalarKindIsChar(FLIScalarKind Kind)
{
    b32 Result = (
        Kind == FLI_SCALAR_KIND_CHAR4 ||
        Kind == FLI_SCALAR_KIND_CHAR16 ||
        Kind == FLI_SCALAR_KIND_CHAR64
    );
 
    return Result;
}

internal u64
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

internal Str8
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
FLAPTable(
    FLAPTableParams* Params, 
    r2f32 Rect
) {
    TempArena Scratch = GetScratch(NULL, 0);
    FLVProgram const* Program = Params->Program;
    FLVRenderItem* Items = Params->Items;
    FLVRenderTable const* Table = &Items[Params->HeadEmit].Table;
    FLAPTableCtx TableCtx = {};
    u32 ColumnCount = Table->ColumnCount + 1;
 
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
        Params->Acc,
        Params->Flight,
        Params->FLI,
        Table->TimeColumn,
        0,
        &TableCtx.Time
    );
 
    for (u32 Index = 0; Index < Table->ColumnCount; ++Index) {
        FLVRenderTableColumn const* Column = &Table->Columns[Index];
 
        if (Column->Column != FLV_COLUMN_NONE) {
            FLVColumnInit(
                Params->Acc,
                Params->Flight,
                Params->FLI,
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
 
    UIScrollListParameters ListParams = {};
    UIScrollListSignal ListSig = {};
    r1i64 Visible = {};
 
    ListParams.Kind = UI_SCROLL_LIST_KIND_ALL;
    ListParams.DimensionsPX = Vec(Length(Rect).X, Length(Rect).Y - 1.0f);
    ListParams.RowHeightPX = 1.0f;
    ListParams.CursorRange.Max.Y = (i64) Table->RowCount;
    ListParams.ItemRange = Rng((i64) 0, (i64) Table->RowCount);
    ListParams.CursorMinIsEmptySelection[AXIS_2D_Y] = TRUE;
 
    UITableFmt(ColumnCount, Params->Percents, "###table_%u", Params->Key) {
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
            &ListParams,
            Params->Scroll,
            Params->Cursor,
            Params->Mark,
            &Visible,
            &ListSig
        ) {
            for (
                i64 Row = Visible.Min;
                Row <= Visible.Max && Row < (i64) Table->RowCount;
                ++Row
            ) {
                b32 IsCursor = (Params->Cursor->Y == Row + 1);
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
                        Params->Cursor->Y = Row + 1;
                        *Params->Mark = *Params->Cursor;
                    }
                }
            }
        }
    }
 
    ReleaseScratch(Scratch);
}

INTERNAL void
FLAPFlagList(
    FLAPFlagListParams* Params,
    r2f32 Rect
) {
    FLVProgram const* Program = Params->Program;
    FLVRenderItem* Items = Params->Items;
    u64 LineCount = 0;
 
    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (Program->Emits[Index].DisplayKind == TOKEN_KIND_FLAG) {
            LineCount += Items[Index].IsMissing
                ? 1
                : 1 + Items[Index].Flag.Count;
        }
    }
 
    UIScrollListParameters ListParams = {};
    UIScrollListSignal ListSig = {};
    r1i64 Visible = {};
 
    ListParams.Kind = UI_SCROLL_LIST_KIND_ALL;
    ListParams.DimensionsPX = Length(Rect);
    ListParams.RowHeightPX = 1.0f;
    ListParams.CursorRange.Max.Y = (i64) LineCount;
    ListParams.ItemRange = Rng((i64) 0, (i64) LineCount);
    ListParams.CursorMinIsEmptySelection[AXIS_2D_Y] = TRUE;
 
    UIScrollList(
        &ListParams,
        Params->Scroll,
        Params->Cursor,
        Params->Mark,
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
