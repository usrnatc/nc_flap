#include "nc_arena.h"
#include "nc_application.h"
#include "nc_draw.h"
#include "nc_memory.h"
#include "nc_ui_components.h"
#include "nc_render.h"

#include "flv_token.h"
#include "flv_run.h"

#include "flap_widgets.h"

global u8 const FLAP_BRAILLE_BITS[4][2] = {
    { 0x01, 0x08 },
    { 0x02, 0x10 },
    { 0x04, 0x20 },
    { 0x40, 0x80 }
};

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

INTERNAL u64
FLAPGraphSampleFromTime(FLVColumn const* Time, r1u64 Samples, u64 TimeUSecs)
{
    u64 Low = Samples.Min;
    u64 High = Samples.Max;

    while (Low < High) {
        u64 Mid = Low + (High - Low) / 2;

        if (FLVTimeAt(Time, Mid) < TimeUSecs)
            Low = Mid + 1;
        else
            High = Mid;
    }

    return Low;
}

INTERNAL void
FLAPGraphReduce(
    FLAPGraphColumns* Columns,
    FLVColumn const* Time,
    FLVColumn const* Value
) {
    FLVRenderCurve const* Curve = Columns->Key.Curve;
    r1u64 TimeRange = Columns->Key.TimeRangeUSecs;
    u64 Count = Columns->Key.Count;
    u64 Span = Length(TimeRange);
    u64 PositionBase = 0;

    for (
        u64 RangeIndex = 0;
        RangeIndex < Curve->RangesCount;
        PositionBase += Length(Curve->Ranges[RangeIndex]), ++RangeIndex
    ) {
        r1u64 Range = Curve->Ranges[RangeIndex];

        if (Range.Min == Range.Max)
            continue;

        if (
            FLVTimeAt(Time, Range.Max - 1) < TimeRange.Min ||
            FLVTimeAt(Time, Range.Min) >= TimeRange.Max
        ) {
            continue;
        }

        u64 Sample = FLAPGraphSampleFromTime(Time, Range, TimeRange.Min);
        b32 PrevHasValue = FALSE;

        while (Sample < Range.Max) {
            u64 Column = (
                (FLVTimeAt(Time, Sample) - TimeRange.Min) * Count
            ) / Span;

            if (Column >= Count)
                break;

            // NOTE(nc): the first time that is not in this column
            u64 EndTimeUSecs = TimeRange.Min + (
                (Column + 1) * Span + Count - 1
            ) / Count;
            u64 End = FLAPGraphSampleFromTime(
                Time,
                Rng(Sample + 1, Range.Max),
                EndTimeUSecs
            );
            u64 Position = PositionBase + (Sample - Range.Min);
            f32 Low = 0.0f;
            f32 High = 0.0f;
            b32 HasValue = FALSE;
            b32 IsJoined = (
                PrevHasValue &&
                FLVSeriesBit(Curve->Mask, Position - 1) &&
                FLVSeriesBit(Curve->Mask, Position)
            );

            if (Curve->ValueColumn == FLV_COLUMN_NONE) {
                u64 ValueIndex = FLVSeriesRank(
                    Curve->Mask,
                    Curve->Rank,
                    Position
                );
                u64 ValueEnd = FLVSeriesRank(
                    Curve->Mask,
                    Curve->Rank,
                    Position + (End - Sample)
                );

                if (ValueIndex < ValueEnd) {
                    Low = Curve->Values[ValueIndex];
                    High = Low;
                    HasValue = TRUE;

                    for (
                        ++ValueIndex;
                        ValueIndex < ValueEnd;
                        ++ValueIndex
                    ) {
                        f32 V = Curve->Values[ValueIndex];

                        Low = MIN(Low, V);
                        High = MAX(High, V);
                    }
                }
            } else {
                for (u64 Index = Sample; Index < End; ++Index) {
                    if (!FLVSeriesBit(Curve->Mask, Position + (Index - Sample)))
                        continue;

                    f32 V = (f32) FLVColumnF64At(Value, Index);

                    if (V != V)
                        continue;

                    if (!HasValue) {
                        Low = V;
                        High = V;
                        HasValue = TRUE;
                    } else {
                        Low = MIN(Low, V);
                        High = MAX(High, V);
                    }
                }
            }

            if (HasValue) {
                Columns->Samples += End - Sample;

                u64 Bit = 1ULL << (Column & 63);

                if (Columns->Has[Column >> 6] & Bit) {
                    Columns->Min[Column] = MIN(Columns->Min[Column], Low);
                    Columns->Max[Column] = MAX(Columns->Max[Column], High);
                } else {
                    Columns->Min[Column] = Low;
                    Columns->Max[Column] = High;
                    Columns->Has[Column >> 6] |= Bit;

                    if (IsJoined)
                        Columns->Join[Column >> 6] |= Bit;
                }
            }

            PrevHasValue = HasValue;
            Sample = End;
        }
    }
}

INTERNAL b32
FLAPGraphColumnsFromKey(
    Arena *MemPool, 
    FLAPGraphColumns *Columns, 
    FLAPGraphKey *Key, 
    Access *Acc, 
    FLIParsed *FLI
) {
    b32 Result = TRUE;
    // FIXME(nc): should this be a MemCmp over two byte arrays?
    b32 IsMatch = (
        Columns->Key.Curve == Key->Curve &&
        Columns->Key.UnitHash == Key->UnitHash &&
        Columns->Key.Flight.U64[0] == Key->Flight.U64[0] &&
        Columns->Key.Flight.U64[1] == Key->Flight.U64[1] &&
        Columns->Key.TimeRangeUSecs.Min == Key->TimeRangeUSecs.Min &&
        Columns->Key.TimeRangeUSecs.Max == Key->TimeRangeUSecs.Max &&
        Columns->Key.Count == Key->Count
    );

    if (!IsMatch) {
        FLVColumn Time = {};
        FLVColumn Value = {};

        Result = FLVColumnInit(
            Acc,
            Key->Flight,
            FLI,
            Key->Curve->TimeColumn,
            0,
            &Time
        );

        if (Key->Curve->ValueColumn != FLV_COLUMN_NONE) {
            Result &= FLVColumnInit(
                Acc,
                Key->Flight,
                FLI,
                Key->Curve->ValueColumn,
                0,
                &Value
            );
        }

        if (Result) {
            if (Columns->Capacity < Key->Count) {
                Columns->Capacity = SafeTruncateU64(
                    Round64NextPow2(Key->Count)
                );
                Columns->Min = ArenaPushArray(
                    MemPool,
                    f32,
                    Columns->Capacity
                );
                Columns->Max = ArenaPushArray(
                    MemPool,
                    f32,
                    Columns->Capacity
                );
                Columns->Has = ArenaPushArray(
                    MemPool,
                    u64,
                    FLVSeriesWordCount(Columns->Capacity)
                );
                Columns->Join = ArenaPushArray(
                    MemPool,
                    u64,
                    FLVSeriesWordCount(Columns->Capacity)
                );
            }

            Columns->Key = *Key;
            Columns->Samples = 0;
            MemZero(
                Columns->Has,
                FLVSeriesWordCount(Key->Count) * sizeof(u64)
            );
            MemZero(
                Columns->Join,
                FLVSeriesWordCount(Key->Count) * sizeof(u64)
            );

            if (
                Key->Count &&
                Length(Key->TimeRangeUSecs) &&
                !Time.IsMissing &&
                !Value.IsMissing
            ) {
                FLAPGraphReduce(Columns, &Time, &Value);
            }
        }
    }

    return Result;
}

INTERNAL b32
FLAPGraphEmitIsCurve(
    FLVProgram const* Program, 
    FLVRenderItem* Items, 
    u32 Index
) {
    b32 Result = (
        Program->Emits[Index].DisplayKind == TOKEN_KIND_GRAPH &&
        !Items[Index].IsMissing &&
        Items[Index].Kind == FLV_RENDER_KIND_CURVE
    );

    return Result;
}

INTERNAL b32
FLAPGraphFitTimeRange(FLAPGraphParams* Params, OUT r1u64* TimeRange)
{
    b32 Result = TRUE;
    r1u64 Fit = {};

    Fit.Min = U64_MAX;

    for (u32 Index = 0; Index < Params->Program->EmitCount; ++Index) {
        if (!FLAPGraphEmitIsCurve(Params->Program, Params->Items, Index))
            continue;

        FLVRenderCurve const* Curve = &Params->Items[Index].Curve;
        FLVColumn Time = {};

        Result &= FLVColumnInit(
            Params->Acc,
            Params->Flight,
            Params->FLI,
            Curve->TimeColumn,
            0,
            &Time
        );

        if (Time.IsMissing)
            continue;

        for (
            u64 RangeIndex = 0; 
            RangeIndex < Curve->RangesCount; 
            ++RangeIndex
        ) {
            r1u64 Range = Curve->Ranges[RangeIndex];

            if (Range.Min == Range.Max)
                continue;

            Fit.Min = MIN(Fit.Min, FLVTimeAt(&Time, Range.Min));
            Fit.Max = MAX(Fit.Max, FLVTimeAt(&Time, Range.Max - 1) + 1);
        }
    }

    if (Result && Fit.Min < Fit.Max)
        *TimeRange = Fit;

    return Result;
}

INTERNAL void
FLAPGraphDot(
    u8* Dots,
    u8* Owners,
    u32 Width,
    u32 Height,
    i32 X,
    i32 Y,
    u8 Owner
) {
    if (
        X >= 0 && 
        X < (i32) (Width * 2) && 
        Y >= 0 && 
        Y < (i32) (Height * 4)
    ) {
        u32 Cell = (u32) (Y >> 2) * Width + (u32) (X >> 1);

        Dots[Cell] |= FLAP_BRAILLE_BITS[Y & 3][X & 1];
        Owners[Cell] = Owner;
    }
}

INTERNAL f32
FLAPGraphTickStep(f32 Range, u32 MaxTicks)
{
    f32 Raw = Range / (f32) MaxTicks;
    f32 Magnitude = PowF32(10.0f, FloorF32(Log10F32(Raw)));
    f32 Norm = Raw / Magnitude;
    f32 Result = Magnitude * (
        (Norm <= 1.0f) 
            ? 1.0f 
            : (Norm <= 2.0f) 
                ? 2.0f 
                : (Norm <= 5.0f) 
                    ? 5.0f 
                    : 10.0f
    );

    return Result;
}

INTERNAL void
FLAPGraphText(
    Arena* MemPool,
    v2f32 Position,
    f32 MaxX,
    Str8 String,
    v4f32 Colour
) {
    FancyStrParams Params = {};
    FancyStrList List = {};

    Params.Colour = Colour;
    ListPush(MemPool, &List, &Params, NULL, String);
    DrawFancyStrList(Position, 1.0f, &List, MaxX, ""_s8, NULL, Colour);
}

INTERNAL Str8
FLAPGraphCursorValue(
    Arena* MemPool,
    FLAPGraphColumns const* Columns,
    u64 TimeUSecs
) {
    Str8 Result = " -"_s8;
    r1u64 TimeRange = Columns->Key.TimeRangeUSecs;
    u64 Span = Length(TimeRange);

    if (
        Span &&
        Columns->Key.Count >= 2 &&
        TimeRange.Min <= TimeUSecs &&
        TimeUSecs < TimeRange.Max
    ) {
        // NOTE(nc): the two dot columns of the cell
        u64 First = (
            ((TimeUSecs - TimeRange.Min) * Columns->Key.Count) / Span
        ) & ~1ULL;
        f32 Low = 0.0f;
        f32 High = 0.0f;
        b32 HasValue = FALSE;

        for (u64 Column = First; Column < First + 2; ++Column) {
            if (!(Columns->Has[Column >> 6] & (1ULL << (Column & 63))))
                continue;

            if (!HasValue) {
                Low = Columns->Min[Column];
                High = Columns->Max[Column];
                HasValue = TRUE;
            } else {
                Low = MIN(Low, Columns->Min[Column]);
                High = MAX(High, Columns->Max[Column]);
            }
        }

        if (HasValue) {
            Result = (Low == High)
                ? ArenaPushStrFmt(MemPool, "=%3.3f", (f64) Low)
                : ArenaPushStrFmt(
                    MemPool,
                    "=[%3.3f..%3.3f]",
                    (f64) Low,
                    (f64) High
                );
        }
    }

    return Result;
}

INTERNAL
UI_BOX_CUSTOM_DRAW(FLAPGraphDraw)
{
    FLAPGraphDrawData* Data = (FLAPGraphDrawData*) UserData;
    u32 Width = Data->Width;
    u32 Height = Data->Height;

    if (!Width || !Height)
        return;

    TempArena Scratch = GetScratch(NULL, 0);
    u64 CellCount = (u64) Width * (u64) Height;
    u8* Dots = ArenaPushArrayZero(Scratch.MemPool, u8, CellCount);
    u8* Owners = ArenaPushArrayZero(Scratch.MemPool, u8, CellCount);
    i32 X0 = (i32) FloorF32(Box->Rect.X0) + (i32) Data->PlotX;
    i32 Y0 = (i32) FloorF32(Box->Rect.Y0) + (i32) Data->PlotY;

    // NOTE(nc): flag bands. one mark for each plot column, then one rect
    //         : for each run, so that the colour is applied one time.
    r1u64 ViewRange = Data->TimeRangeUSecs;
    u64 ViewSpan = Length(ViewRange);

    if (ViewSpan) {
        u8* Bands = ArenaPushArrayZero(Scratch.MemPool, u8, Width);

        for (u32 FlagIndex = 0; FlagIndex < Data->FlagsCount; ++FlagIndex) {
            FLVRenderFlag const* Flag = Data->Flags[FlagIndex];

            for (u64 Index = 0; Index < Flag->Count; ++Index) {
                r1u64 Range = Flag->Ranges[Index];

                if (Range.Max <= ViewRange.Min || Range.Min >= ViewRange.Max)
                    continue;

                u64 First = (
                    (MAX(Range.Min, ViewRange.Min) - ViewRange.Min) * Width
                ) / ViewSpan;
                u64 Last = (
                    (MIN(Range.Max, ViewRange.Max) - 1 - ViewRange.Min) * Width
                ) / ViewSpan;

                for (u64 Column = First; Column <= Last; ++Column)
                    Bands[Column] = 1;
            }
        }

        for (u32 X = 0; X < Width; ) {
            if (!Bands[X]) {
                ++X;
                continue;
            }

            u32 Start = X;

            for ( ; X < Width && Bands[X]; ++X);

            DrawRect(
                Rng(
                    (f32) (X0 + (i32) Start),
                    (f32) Y0,
                    (f32) (X0 + (i32) X),
                    (f32) (Y0 + (i32) Height)
                ),
                Data->BandColour,
                0.0f
            );
        }

        if (
            Data->HasCursor &&
            ViewRange.Min <= Data->CursorTimeUSecs &&
            Data->CursorTimeUSecs < ViewRange.Max
        ) {
            i32 X = X0 + (i32) (
                ((Data->CursorTimeUSecs - ViewRange.Min) * Width) / ViewSpan
            );

            DrawRect(
                Rng(
                    (f32) X,
                    (f32) Y0,
                    (f32) (X + 1),
                    (f32) (Y0 + (i32) Height)
                ),
                Data->CursorColour,
                0.0f
            );
        }
    }

    for (u32 CurveIndex = 0; CurveIndex < Data->CurvesCount; ++CurveIndex) {
        FLAPGraphDrawCurve* Curve = &Data->Curves[CurveIndex];
        FLAPGraphColumns const* Columns = Curve->Columns;
        r1f32 Range = Data->ValueRanges[Curve->Axis];
        f32 Scale = (f32) (Height * 4 - 1) / (Range.Max - Range.Min);
        i32 PrevX = 0;
        i32 PrevTop = 0;
        i32 PrevBottom = 0;

        for (u64 Column = 0; Column < Columns->Key.Count; ++Column) {
            u64 Bit = 1ULL << (Column & 63);

            if (!(Columns->Has[Column >> 6] & Bit))
                continue;

            i32 X = (i32) Column;
            i32 Top = (i32) ((Range.Max - Columns->Max[Column]) * Scale + 0.5f);
            i32 Bottom = (i32) ((Range.Max - Columns->Min[Column]) * Scale + 0.5f);

            if (Columns->Join[Column >> 6] & Bit) {
                i32 FromY = CLAMP(PrevTop, (Top + Bottom) / 2, PrevBottom);
                i32 ToY = CLAMP(Top, FromY, Bottom);
                i32 DeltaX = X - PrevX;
                i32 DeltaY = ToY - FromY;
                i32 Steps = MAX(DeltaX, (DeltaY < 0) ? -DeltaY : DeltaY);

                for (i32 Step = 1; Step < Steps; ++Step) {
                    FLAPGraphDot(
                        Dots,
                        Owners,
                        Width,
                        Height,
                        PrevX + (DeltaX * Step) / Steps,
                        FromY + (DeltaY * Step) / Steps,
                        (u8) CurveIndex
                    );
                }
            }

            for (i32 Y = Top; Y <= Bottom; ++Y) {
                FLAPGraphDot(
                    Dots, 
                    Owners, 
                    Width, 
                    Height, 
                    X, 
                    Y, 
                    (u8) CurveIndex
                );
            }

            PrevX = X;
            PrevTop = Top;
            PrevBottom = Bottom;
        }
    }

    for (u32 Y = 0; Y < Height; ++Y) {
        for (u32 X = 0; X < Width; ++X) {
            u32 Cell = Y * Width + X;

            if (Dots[Cell]) {
                RendPutCell(
                    X0 + (i32) X,
                    Y0 + (i32) Y,
                    0x2800 + Dots[Cell],
                    Data->Curves[Owners[Cell]].Colour,
                    (RendAttr) 0
                );
            }
        }
    }

    // NOTE(nc): markers, only when there is one sample or less for each cell
    for (u32 CurveIndex = 0; CurveIndex < Data->CurvesCount; ++CurveIndex) {
        FLAPGraphDrawCurve* Curve = &Data->Curves[CurveIndex];
        FLAPGraphColumns const* Columns = Curve->Columns;

        if (!Columns->Samples || Columns->Samples > Width)
            continue;

        r1f32 Range = Data->ValueRanges[Curve->Axis];
        f32 Scale = (f32) (Height * 4 - 1) / (Range.Max - Range.Min);

        for (u64 Column = 0; Column < Columns->Key.Count; ++Column) {
            if (!(Columns->Has[Column >> 6] & (1ULL << (Column & 63))))
                continue;

            i32 Top = (i32) ((Range.Max - Columns->Max[Column]) * Scale + 0.5f);

            if (Top >= 0 && Top < (i32) (Height * 4)) {
                RendPutCell(
                    X0 + (i32) (Column >> 1),
                    Y0 + (Top >> 2),
                    '+',
                    Curve->Colour,
                    (RendAttr) 0
                );
            }
        }
    }

    // NOTE(nc): legend
    f32 LegendX = Box->Rect.X0 + 1.0f;

    for (
        u32 Index = 0;
        Index < Data->LabelsCount && LegendX < Box->Rect.X1;
        ++Index
    ) {
        FLAPGraphDrawLabel* Label = &Data->Labels[Index];

        FLAPGraphText(
            Scratch.MemPool,
            Vec(LegendX, Box->Rect.Y0),
            Box->Rect.X1,
            Label->String,
            Label->Colour
        );
        LegendX += DDimensionsFromStr(1.0f, Label->String).X + 2.0f;
    }

    // NOTE(nc): value labels, left axis then right axis
    for (u32 Axis = 0; Axis < 2; ++Axis) {
        if (!Data->HasRange[Axis])
            continue;

        r1f32 Range = Data->ValueRanges[Axis];
        f32 Scale = (f32) (Height * 4 - 1) / (Range.Max - Range.Min);
        f32 Step = FLAPGraphTickStep(
            Range.Max - Range.Min,
            MAX(Height / 3, (u32) 2)
        );
        f32 First = CeilF32(Range.Min / Step);
        char* Fmt = (char*) (
            (Step >= 0.95f) 
                ? "%.0f" 
                : (Step >= 0.095f) 
                    ? "%.1f" 
                    : (Step >= 0.0095f) 
                        ? "%.2f" 
                        : "%.3f"
        );

        for (u32 Tick = 0; ; ++Tick) {
            f32 Value = (First + (f32) Tick) * Step;

            if (Value > Range.Max)
                break;

            i32 Row = ((i32) ((Range.Max - Value) * Scale + 0.5f)) >> 2;
            Str8 String = ArenaPushStrFmt(Scratch.MemPool, Fmt, (f64) Value);
            f32 X = Axis
                ? (f32) (X0 + (i32) Width + 1)
                : (f32) (X0 - 1 - (i32) String.Size);

            FLAPGraphText(
                Scratch.MemPool,
                Vec(X, (f32) (Y0 + Row)),
                Box->Rect.X1,
                String,
                Data->LabelColour
            );
        }
    }

    // NOTE(nc): time labels
    r1u64 TimeRange = Data->TimeRangeUSecs;
    u64 Span = Length(TimeRange);

    if (Span) {
        f32 Step = FLAPGraphTickStep(
            (f32) Span / 1000000.0f,
            MAX(Width / 12, (u32) 1)
        );
        u64 StepUSecs = MAX((u64) (Step * 1000000.0f + 0.5f), (u64) 1);

        for (
            u64 TimeUSecs = (
                (TimeRange.Min + StepUSecs - 1) / StepUSecs
            ) * StepUSecs;
            TimeUSecs < TimeRange.Max;
            TimeUSecs += StepUSecs
        ) {
            i32 X = X0 + (i32) (((TimeUSecs - TimeRange.Min) * Width) / Span);
            Str8 String = (StepUSecs >= MILLION(1))
                ? ArenaPushStrFmt(
                    Scratch.MemPool,
                    "%llus",
                    TimeUSecs / MILLION(1)
                )
                : ArenaPushStrFmt(
                    Scratch.MemPool,
                    "%llu.%03llus",
                    TimeUSecs / MILLION(1),
                    (TimeUSecs % MILLION(1)) / THOUSAND(1)
                );

            FLAPGraphText(
                Scratch.MemPool,
                Vec((f32) X, (f32) (Y0 + (i32) Height)),
                Box->Rect.X1,
                String,
                Data->LabelColour
            );
        }
    }

    ReleaseScratch(Scratch);
}

INTERNAL void
FLAPGraph(FLAPGraphParams* Params, r2f32 Rect)
{
    FLVProgram const* Program = Params->Program;
    FLVRenderItem* Items = Params->Items;
    FLAPGraphState* State = Params->State;
    FLAPGraphDrawData* Data = ArenaPushArrayZero(
        UIBuildMemPool(),
        FLAPGraphDrawData,
        1
    );
    u32 Width = (u32) Length(Rect).X;
    u32 Height = (u32) Length(Rect).Y;
    u32 Gutters = FLAP_GRAPH_GUTTER;
    b32 IsReady = TRUE;

    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (
            FLAPGraphEmitIsCurve(Program, Items, Index) &&
            Items[Index].Curve.Axis == 2
        ) {
            Gutters = 2 * FLAP_GRAPH_GUTTER;
            break;
        }
    }

    Data->Curves = ArenaPushArrayZero(
        UIBuildMemPool(),
        FLAPGraphDrawCurve,
        Program->EmitCount
    );
    Data->Labels = ArenaPushArrayZero(
        UIBuildMemPool(),
        FLAPGraphDrawLabel,
        Program->EmitCount + 1
    );
    Data->PlotX = FLAP_GRAPH_GUTTER;
    Data->PlotY = 1;
    Data->Width = (Width > Gutters) ? Width - Gutters : 0;
    Data->Height = (Height > 2) ? Height - 2 : 0;

    UITag("weak"_s8) {
        Data->LabelColour = UIColourFromName("text"_s8);
    }

    Data->BandColour = UIColourFromName("hover"_s8);
    Data->BandColour.W *= 0.2f;
    Data->CursorColour = UIColourFromName("selection"_s8);
    Data->HasCursor = State->HasCursor;
    Data->CursorTimeUSecs = State->CursorTimeUSecs;
    Data->Flags = ArenaPushArrayZero(
        UIBuildMemPool(),
        FLVRenderFlag const*,
        Program->EmitCount
    );

    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (
            Program->Emits[Index].DisplayKind == TOKEN_KIND_FLAG &&
            !Items[Index].IsMissing
        ) {
            Data->Flags[Data->FlagsCount++] = &Items[Index].Flag;
        }
    }

    if (State->HasCursor) {
        FLAPGraphDrawLabel* Label = &Data->Labels[Data->LabelsCount++];

        Label->String = ArenaPushStrFmt(
            UIBuildMemPool(),
            "%llu.%03llus",
            State->CursorTimeUSecs / MILLION(1),
            (State->CursorTimeUSecs % MILLION(1)) / THOUSAND(1)
        );
        Label->Colour = Data->LabelColour;
    }

    if (!State->IsFitted) {
        r1u64 Fit = {};
        b32 IsFitReady = FLAPGraphFitTimeRange(Params, &Fit);

        IsReady &= IsFitReady;
        State->IsFitReady &= IsFitReady;

        if (Fit.Min < Fit.Max) {
            if (State->FitRangeUSecs.Min < State->FitRangeUSecs.Max) {
                State->FitRangeUSecs.Min = MIN(
                    State->FitRangeUSecs.Min, 
                    Fit.Min
                );
                State->FitRangeUSecs.Max = MAX(
                    State->FitRangeUSecs.Max, 
                    Fit.Max
                );
            } else {
                State->FitRangeUSecs = Fit;
            }
        }
    }

    Data->TimeRangeUSecs = State->TimeRangeUSecs;

    for (u32 Index = 0; Index < Program->EmitCount; ++Index) {
        if (Program->Emits[Index].DisplayKind != TOKEN_KIND_GRAPH)
            continue;

        FLAPGraphDrawLabel* Label = &Data->Labels[Data->LabelsCount++];
        Str8 Name = Program->Strings[Program->Emits[Index].LabelStringIndex];

        if (!FLAPGraphEmitIsCurve(Program, Items, Index)) {
            Label->String = ArenaPushStrFmt(
                UIBuildMemPool(),
                "%S (missing)",
                PRINT_STR(Name)
            );
            Label->Colour = Data->LabelColour;
            continue;
        }

        FLAPGraphColumns* Columns = &Params->Columns[Index];
        FLAPGraphDrawCurve* Curve = &Data->Curves[Data->CurvesCount];
        FLAPGraphKey Key = {};

        Key.Curve = &Items[Index].Curve;
        Key.UnitHash = Params->UnitHash;
        Key.Flight = Params->Flight;
        Key.TimeRangeUSecs = State->TimeRangeUSecs;
        Key.Count = Data->Width * 2;
        IsReady &= FLAPGraphColumnsFromKey(
            Params->MemPool,
            Columns,
            &Key,
            Params->Acc,
            Params->FLI
        );

        Curve->Columns = Columns;
        Curve->Axis = (Key.Curve->Axis == 2);
        Curve->Colour = UIColourFromName(
            ArenaPushStrFmt(
                UIBuildMemPool(),
                "curve_%u",
                Data->CurvesCount % 8
            )
        );
        Label->String = ArenaPushStrFmt(
            UIBuildMemPool(),
            "%S%S%S",
            PRINT_STR(Name),
            PRINT_STR(Curve->Axis ? ":2"_s8 : ""_s8),
            PRINT_STR(
                State->HasCursor
                    ? FLAPGraphCursorValue(
                        UIBuildMemPool(),
                        Columns,
                        State->CursorTimeUSecs
                    )
                    : ""_s8
            )
        );
        Label->Colour = Curve->Colour;

        r1f32* Range = &Data->ValueRanges[Curve->Axis];

        for (u64 Column = 0; Column < Columns->Key.Count; ++Column) {
            if (!(Columns->Has[Column >> 6] & (1ULL << (Column & 63))))
                continue;

            if (!Data->HasRange[Curve->Axis]) {
                Range->Min = Columns->Min[Column];
                Range->Max = Columns->Max[Column];
                Data->HasRange[Curve->Axis] = TRUE;
            } else {
                Range->Min = MIN(Range->Min, Columns->Min[Column]);
                Range->Max = MAX(Range->Max, Columns->Max[Column]);
            }
        }

        ++Data->CurvesCount;
    }

    for (u32 Axis = 0; Axis < 2; ++Axis) {
        if (Data->ValueRanges[Axis].Max <= Data->ValueRanges[Axis].Min) {
            Data->ValueRanges[Axis].Min -= 1.0f;
            Data->ValueRanges[Axis].Max += 1.0f;
        }
    }

    if (!IsReady)
        AppRequestFrame();

    UISetNextPreferredSize(AXIS_2D_X, UI_PERCENT(1.0f, 0.0f));
    UISetNextPreferredSize(AXIS_2D_Y, UI_PX((f32) Height, 1.0f));

    UIBox* Box = UIBuildBoxFromStrFmt(
        UI_BOX_KIND_MOUSE_CLICKABLE | UI_BOX_KIND_SCROLL,
        "###graph_%u",
        Params->Key
    );

    UIBoxEquipCustomDraw(Box, FLAPGraphDraw, Data);

    UISignal Sig = UISignalFromBox(Box);

    if (UI_PRESSED(Sig))
        AppCmd(APP_COMMAND_KIND_FOCUS_PANEL);

    r1u64 TimeRange = State->TimeRangeUSecs;
    f64 Span = (f64) Length(TimeRange);
    f64 PlotWidth = (f64) Data->Width;

    if (Data->Width && Span > 0.0) {
        v2f32 Mouse = UIMouse();
        f32 PlotX0 = Box->Rect.X0 + (f32) Data->PlotX;
        f32 PlotY0 = Box->Rect.Y0 + (f32) Data->PlotY;

        if (
            UI_HOVERING(Sig) &&
            Mouse.X >= PlotX0 &&
            Mouse.X < PlotX0 + (f32) Data->Width &&
            Mouse.Y >= PlotY0 &&
            Mouse.Y < PlotY0 + (f32) Data->Height
        ) {
            State->NextCursorTimeUSecs = TimeRange.Min + (u64) (
                ((f64) FloorF32(Mouse.X - PlotX0) + 0.5) * (Span / PlotWidth)
            );
            State->HasNextCursor = TRUE;
        }

        if (Sig.Scroll.Y) {
            f64 Middle = (f64) TimeRange.Min + 0.5 * Span;
            f64 Factor = 1.0;

            for (
                i32 Notch = Sig.Scroll.Y;
                Notch;
                Notch += (Notch < 0) 
                    ? 1 
                    : -1
            ) {
                Factor *= (Notch < 0) 
                    ? 0.8 
                    : 1.25;
            }

            f64 NewSpan = CLAMP(
                (f64) (Data->Width * 2),
                Span * Factor,
                (f64) FLAP_GRAPH_SPAN_MAX_USECS
            );
            f64 NewMin = MAX(Middle - 0.5 * NewSpan, 0.0);

            State->TimeRangeUSecs.Min = (u64) NewMin;
            State->TimeRangeUSecs.Max = (u64) NewMin + (u64) NewSpan;
            AppRequestFrame();
        }

        if (UI_PRESSED(Sig)) {
            UIStoreDragStruct(&State->TimeRangeUSecs);
        } else if (UI_DRAGGING(Sig)) {
            r1u64 Start = *UIGetDragStruct(r1u64);
            f64 StartSpan = (f64) Length(Start);
            f64 NewMin = MAX(
                (f64) Start.Min - (f64) UIDragDelta().X * (StartSpan / PlotWidth),
                0.0
            );

            State->TimeRangeUSecs.Min = (u64) NewMin;
            State->TimeRangeUSecs.Max = (u64) NewMin + Length(Start);
            AppRequestFrame();
        }
    }
}
