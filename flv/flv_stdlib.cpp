#include "nc_defines.h"
#include "nc_types.h"
#include "nc_string.h"
#include "nc_tls.h"
#include "flv_stdlib.h"
#include "flv_symbol_table.h"
#include "flv_intern.h"
#include "flv_type.h"
#include "flv_parser.h"

FLVBuiltin FLV_STDLIB_TABLE[FLV_STDLIB_ID_COUNT] = {
    {
        Str8Lit("mean"),
        FLV_SIGNATURE_KIND_SERIES_TO_F64,
        TRUE
    },
    {
        Str8Lit("min"),
        FLV_SIGNATURE_KIND_SERIES_TO_T,
        TRUE
    },
    {
        Str8Lit("max"),
        FLV_SIGNATURE_KIND_SERIES_TO_T,
        TRUE
    },
    {
        Str8Lit("sum"),
        FLV_SIGNATURE_KIND_SERIES_TO_T,
        TRUE
    },
    {
        Str8Lit("stddev"),
        FLV_SIGNATURE_KIND_SERIES_TO_F64,
        TRUE
    },
    {
        Str8Lit("length"),
        FLV_SIGNATURE_KIND_SERIES_TO_I64,
        FALSE
    },
    {
        Str8Lit("head"),
        FLV_SIGNATURE_KIND_SERIES_TO_T,
        FALSE
    },
    {
        Str8Lit("tail"),
        FLV_SIGNATURE_KIND_SERIES_TO_T,
        FALSE
    },
    {
        Str8Lit("abs"),
        FLV_SIGNATURE_KIND_SERIES_TO_SERIES,
        TRUE
    },
    {
        Str8Lit("abs"),
        FLV_SIGNATURE_KIND_T_TO_T,
        TRUE
    },
    {
        Str8Lit("diff"),
        FLV_SIGNATURE_KIND_SERIES_TO_SERIES,
        TRUE
    },
    {
        Str8Lit("rate"),
        FLV_SIGNATURE_KIND_SERIES_TO_SERIES_F64,
        TRUE
    },
    {
        Str8Lit("rolling_mean"),
        FLV_SIGNATURE_KIND_SERIES_DURATION_TO_SERIES,
        TRUE
    },
    {
        Str8Lit("rolling_min"),
        FLV_SIGNATURE_KIND_SERIES_DURATION_TO_SERIES,
        TRUE
    },
    {
        Str8Lit("rolling_max"),
        FLV_SIGNATURE_KIND_SERIES_DURATION_TO_SERIES,
        TRUE
    },
    {
        Str8Lit("mode"),
        FLV_SIGNATURE_KIND_MODE_TO_SELECTION,
        FALSE
    },
    {
        Str8Lit("event"),
        FLV_SIGNATURE_KIND_I64_TO_SELECTION,
        FALSE
    },
    {
        Str8Lit("duration"),
        FLV_SIGNATURE_KIND_SELECTION_TO_DURATION,
        FALSE
    }
};

TypeIndex FLV_STDLIB_FLIGHT = {};
BindIndex FLV_STDLIB_ARMED = {};

#define X(N, V) { Str8Lit(#N), V },

global const FLVModeName FLV_PLANE_MODE_TABLE[] = {
    FLV_PLANE_MODES(X)
};

global const FLVModeName FLV_COPTER_MODE_TABLE[] = {
    FLV_COPTER_MODES(X)
};

#undef X

FLVModeName const* const FLV_MODE_TABLE[FLV_VEHICLE_KIND_COUNT] = {
    FLV_PLANE_MODE_TABLE,
    FLV_COPTER_MODE_TABLE
};

u32 const FLV_MODE_TABLE_COUNT[FLV_VEHICLE_KIND_COUNT] = {
    ARRAY_COUNT(FLV_PLANE_MODE_TABLE),
    ARRAY_COUNT(FLV_COPTER_MODE_TABLE)
};

FLVVehicleKind
FLVVehicleKindFromStr(Str8 KindString)
{
    FLVVehicleKind Result = FLV_VEHICLE_KIND_UNKNOWN;

    if (StrMatch(StrPrefix(KindString, 9), "ArduPlane"_s8, 0))
        Result = FLV_VEHICLE_KIND_PLANE;
    else if (StrMatch(StrPrefix(KindString, 10), "ArduCopter"_s8, 0))
        Result = FLV_VEHICLE_KIND_COPTER;

    return Result;
}

u32
FLVModeNumberFromName(FLVVehicleKind Kind, Str8 Name)
{
    u32 Result = FLV_MODE_NUMBER_NONE;
 
    if (Kind < FLV_VEHICLE_KIND_COUNT) {
        FLVModeName const* Table = FLV_MODE_TABLE[Kind];
        u32 Count = FLV_MODE_TABLE_COUNT[Kind];
 
        for (u32 Index = 0; Index < Count; ++Index) {
            if (StrMatch(Table[Index].Name, Name, 0)) {
                Result = Table[Index].Number;
                break;
            }
        }
    }
 
    return Result;
}

void
FLVStdLibBindModes(FLVVehicleKind Kind)
{
    if (Kind >= FLV_VEHICLE_KIND_COUNT)
        return;

    NamePool* Names = GetTLS()->SYMBOL_TABLE->Names;
    FLVModeName const* Table = FLV_MODE_TABLE[Kind];
    u32 Count = FLV_MODE_TABLE_COUNT[Kind];
    ScopeIndex Root = {};
    DeclIndex NoDecl = {
        AST_NODE_INDEX_INVALID
    };

    for (u32 Index = 0; Index < Count; ++Index)
        InternName(Names, Table[Index].Name);

    SymTabPrepare();

    for (u32 Index = 0; Index < Count; ++Index) {
        NameIndex Name = InternName(Names, Table[Index].Name);

        if (FindBinding(Name).V == BINDING_INVALID_VALUE)
            AddBinding(Root, Name, NoDecl, TypeMode(), BINDING_KIND_VALUE);
    }
}

INTERNAL TypeIndex
SignatureFromKind(FLVSignatureKind Kind)
{
    BindIndex NoBind = {
        BINDING_INVALID_VALUE
    };
    TypeIndex T = TypeParam(NoBind, FLV_STDLIB_PARAM_T);
    TypeIndex M = TypeParam(NoBind, FLV_STDLIB_PARAM_M);
    TypeIndex SeriesTM = TypeSeries(T, M);
    TypeIndex SelectionFlight = TypeSelection(FLV_STDLIB_FLIGHT);
    TypeIndex Signature[3] = {};                                                // NOTE(nc): [ReturnType, ParamType_1, ParamType_2]
    u32 ParamsCount = 1;

    switch (Kind) {
        default: {
            UNREACHABLE();
        } break;

        case FLV_SIGNATURE_KIND_SERIES_TO_T: {
            Signature[0] = T;
            Signature[1] = SeriesTM;
        } break;

        case FLV_SIGNATURE_KIND_SERIES_TO_I64: {
            Signature[0].V = BUILTIN_i64;
            Signature[1] = SeriesTM;
        } break;

        case FLV_SIGNATURE_KIND_SERIES_TO_F64: {
            Signature[0].V = BUILTIN_f64;
            Signature[1] = SeriesTM;
        } break;

        case FLV_SIGNATURE_KIND_SERIES_TO_SERIES: {
            Signature[0] = SeriesTM;
            Signature[1] = SeriesTM;
        } break;

        case FLV_SIGNATURE_KIND_SERIES_TO_SERIES_F64: {
            TypeIndex F64 = {
                BUILTIN_f64
            };

            Signature[0] = TypeSeries(F64, M);
            Signature[1] = SeriesTM;
        } break;

        case FLV_SIGNATURE_KIND_T_TO_T: {
            Signature[0] = T;
            Signature[1] = T;
        } break;

        case FLV_SIGNATURE_KIND_SERIES_DURATION_TO_SERIES: {
            Signature[0] = SeriesTM;
            Signature[1] = SeriesTM;
            Signature[2] = TypeDuration();
            ParamsCount = 2;
        } break;

        case FLV_SIGNATURE_KIND_MODE_TO_SELECTION: {
            Signature[0] = SelectionFlight;
            Signature[1] = TypeMode();
        } break;

        case FLV_SIGNATURE_KIND_I64_TO_SELECTION: {
            Signature[0] = SelectionFlight;
            Signature[1].V = BUILTIN_i64;
        } break;

        case FLV_SIGNATURE_KIND_SELECTION_TO_DURATION: {
            Signature[0] = TypeDuration();
            Signature[1] = TypeSelection(M);
        } break;
    }

    return TypeFunc(Signature, ParamsCount);
}

void 
FLVStdLibInit(void)
{
    NamePool* Names = GetTLS()->SYMBOL_TABLE->Names;
    ScopeIndex Root = {};
    DeclIndex NoDecl = {
        AST_NODE_INDEX_INVALID
    };
    NameIndex FlightName = InternName(Names, "flight"_s8);
    NameIndex ArmedName = InternName(Names, "armed"_s8);
    NameIndex FuncNames[FLV_STDLIB_ID_COUNT] = {};

    for (u32 Index = 0; Index < FLV_STDLIB_ID_COUNT; ++Index)
        FuncNames[Index] = InternName(Names, FLV_STDLIB_TABLE[Index].Name);

    SymTabPrepare();
    FLV_STDLIB_FLIGHT = TypeMessage(FlightName, 0);

    for (u32 Index = 0; Index < FLV_STDLIB_ID_COUNT; ++Index) {
        FLVBuiltin* Entry = &FLV_STDLIB_TABLE[Index];
        DeclIndex Row = {
            Index
        };

        Entry->Signature = SignatureFromKind(Entry->SignatureKind);
        Entry->Bind = AddFuncBinding(
            Root, 
            FuncNames[Index], 
            Row, 
            Entry->Signature
        );

        ASSERT_ALWAYS(Entry->Bind.V != BINDING_INVALID_VALUE);
    }

    FLV_STDLIB_ARMED = AddBinding(
        Root,
        ArmedName,
        NoDecl,
        TypeSelection(FLV_STDLIB_FLIGHT),
        BINDING_KIND_VALUE
    );

    ASSERT_ALWAYS(FLV_STDLIB_ARMED.V != BINDING_INVALID_VALUE);
}
