#if !defined(__FLV_VISUALISATION_H__)
#define __FLV_VISUALISATION_H__

#include "nc_types.h"
#include "nc_math.h"
#include "nc_string.h"
#include "nc_simd.h"

#include "fli.h"

#include "flv_bind.h"

// @defines____________________________________________________________________
#define FLV_COLUMN_MODE U32_MAX

typedef u8 FLVRenderKind;
enum : u8 {
    FLV_RENDER_KIND_NULL,
    FLV_RENDER_KIND_CURVE,
    FLV_RENDER_KIND_TABLE,
    FLV_RENDER_KIND_TEXT,
    FLV_RENDER_KIND_FLAG,
    FLV_RENDER_KIND_COUNT
};

// @types______________________________________________________________________
struct FLVRenderCurve {
    r1u64* Ranges;
    u64    RangesCount;
    u64*   Mask;
    u32*   Rank;
    f32*   Values;
    u64    SampleCount;
    u64    Count;
    u32    TimeColumn;
    u32    ValueColumn;
    u32    LabelStringIndex;
    u8     Axis;
    u8     __Padding[3];
};

struct FLVRenderFlag {
    r1u64* Ranges;
    u64    Count;
    u64    TotalDurationUSecs;
    u32    LabelStringIndex;
    u32    __Padding;
};

struct FLVRenderTableColumn {
    void*         Values;
    u32           Column;
    u32           LabelStringIndex;
    FLIScalarKind Kind;
    u8            Width;
    u8            IsMissing;
    u8            __Padding;
};

struct FLVRenderTable {
    r1u64*                Ranges;
    u64                   RangesCount;
    u64*                  Mask;
    u32*                  Rank;
    FLVRenderTableColumn* Columns;
    u64                   SampleCount;
    u64                   RowCount;
    u32                   TimeColumn;
    u32                   ColumnCount;
};

struct FLVRenderItem {
    FLVRenderKind Kind;
    u8            IsMissing;
    u16           __Padding;
    u32           AnalysisIndex;
    u32           FlightIndex;
    u32           EmitIndex;
    FLIKey        Flight;

    union {
        FLVRenderCurve Curve;
        FLVRenderFlag  Flag;
        FLVRenderTable Table;
        Str8           Text;
    };
};

// @runtime____________________________________________________________________


// @funcions___________________________________________________________________
INTERNAL u64
FLVSeriesWordCount(u64 SampleCount)
{
    u64 Result = (SampleCount + 63) >> 6;

    return Result;
}

INTERNAL b32
FLVSeriesBit(u64 const* Mask, u64 Position)
{
    b32 Result = !Mask || ((Mask[Position >> 6] >> (Position & 63)) & 1);

    return Result;
}

INTERNAL u64
FLVSeriesRank(u64 const* Mask, u32 const* Rank, u64 Position)
{
    u64 Result = Position;

    if (Mask) {
        u64 Word = Position >> 6;
        u64 Below = Mask[Word] & ((1ULL << (Position & 63)) - 1);

        Result = Rank[Word] + PopCount64(Below);
    }

    return Result;
}

INTERNAL u64
FLVSeriesSampleIndex(r1u64 const* Ranges, u64 RangesCount, u64 Position)
{
    u64 Result = U64_MAX;

    for (u64 Index = 0; Index < RangesCount; ++Index) {
        u64 RangeLength = Length(Ranges[Index]);

        if (Position < RangeLength) {
            Result = Ranges[Index].Min + Position;
            break;
        }

        Position -= RangeLength;
    }

    return Result;
}

INTERNAL u64
FLVSeriesFirstSet(u64 const* Mask, u64 SampleCount)
{
    u64 Result = SampleCount;

    if (!Mask) {
        Result = 0;
    } else {
        u64 Words = FLVSeriesWordCount(SampleCount);

        for (u64 WordIndex = 0; WordIndex < Words; ++WordIndex) {
            if (Mask[WordIndex]) {
                Result = (WordIndex << 6) + Ctz64(Mask[WordIndex]);
                break;
            }
        }
    }

    return Result;
}

INTERNAL u64
FLVSeriesLastSet(u64 const* Mask, u64 SampleCount)
{
    u64 Result = SampleCount;

    if (!Mask) {
        Result = SampleCount - 1;
    } else {
        u64 Words = FLVSeriesWordCount(SampleCount);

        for (u64 WordIndex = 0; WordIndex < Words; ++WordIndex) {
            if (Mask[WordIndex]) {
                Result = (WordIndex << 6) + 63 - Clz64(Mask[WordIndex]);
                break;
            }
        }
    }

    return Result;
}

INTERNAL f64
FLVColumnF64At(FLIParsed* FLI, u32 Column, u64 Index)
{
    FLIColumn* Record = FLIElementFromNameIndex(
        FLI,
        Columns,
        COLUMNS,
        Column
    );
    u64 Count = 0;
    void* Data = FLIColumnRawDataFromIndex(
        FLI, 
        Column, 
        &Count
    );
    f64 Result = NC_NAN;

    if (Data && Index < Count) {
        switch (Record->Kind) {
            default: {} break;

            case FLI_SCALAR_KIND_U8: {
                Result = (f64) ((u8*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_I8: {
                Result = (f64) ((i8*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_U16: {
                Result = (f64) ((u16*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_I16: {
                Result = (f64) ((i16*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_U32: {
                Result = (f64) ((u32*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_I32: {
                Result = (f64) ((i32*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_U64: {
                Result = (f64) ((u64*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_I64: {
                Result = (f64) ((i64*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_F32: {
                Result = (f64) ((f32*) Data)[Index];
            } break;

            case FLI_SCALAR_KIND_F64: {
                Result = ((f64*) Data)[Index];
            } break;
        }
    }

    return Result;
}

INTERNAL u64
FLVColumnTimeAt(FLIParsed* FLI, u32 Column, u64 Index)
{
    u64 Count = 0;
    u64* Data = (u64*) FLIColumnRawDataFromIndex(FLI, Column, &Count);
    u64 Result = 0;

    if (Data && Index < Count)
        Result = Data[Index];

    return Result;
}

INTERNAL f64
FLVCurveValueAt(FLIParsed* FLI, FLVRenderCurve const* Curve, u64 Position)
{
    f64 Result = NC_NAN;

    if (Curve->ValueColumn != FLV_COLUMN_NONE) {
        u64 Sample = FLVSeriesSampleIndex(
            Curve->Ranges, 
            Curve->RangesCount, 
            Position
        );

        Result = FLVColumnF64At(FLI, Curve->ValueColumn, Sample);
    } else {
        Result = Curve->Values[
            FLVSeriesRank(Curve->Mask, Curve->Rank, Position)
        ];
    }

    return Result;
}

#endif // __FLV_VISUALISATION_H__
