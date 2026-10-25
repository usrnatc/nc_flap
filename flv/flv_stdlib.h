#if !defined(__FLV_STDLIB_H__)
#define __FLV_STDLIB_H__

#include "nc_types.h"
#include "nc_string.h"
#include "flv_base_types.h"

// @defines____________________________________________________________________
typedef u16 FLVStdLibID;
enum : u16 {
    FLV_STDLIB_ID_MEAN,
    FLV_STDLIB_ID_MIN,
    FLV_STDLIB_ID_MAX,
    FLV_STDLIB_ID_SUM,
    FLV_STDLIB_ID_STDDEV,
    FLV_STDLIB_ID_LENGTH,
    FLV_STDLIB_ID_HEAD,
    FLV_STDLIB_ID_TAIL,
    FLV_STDLIB_ID_ABS_SERIES,
    FLV_STDLIB_ID_ABS_SCALAR,
    FLV_STDLIB_ID_DIFF,
    FLV_STDLIB_ID_RATE,
    FLV_STDLIB_ID_ROLLING_MEAN,
    FLV_STDLIB_ID_ROLLING_MIN,
    FLV_STDLIB_ID_ROLLING_MAX,
    FLV_STDLIB_ID_MODE,
    FLV_STDLIB_ID_EVENT,
    FLV_STDLIB_ID_DURATION,
    FLV_STDLIB_ID_COUNT
};

typedef u8 FLVSignatureKind;
enum : u8 {
    FLV_SIGNATURE_KIND_SERIES_TO_T,                                             // (Series<T, M>) -> T
    FLV_SIGNATURE_KIND_SERIES_TO_I64,                                           // (Series<T, M>) -> i64
    FLV_SIGNATURE_KIND_SERIES_TO_SERIES,                                        // (Series<T, M>) -> Series<T, M>
    FLV_SIGNATURE_KIND_SERIES_TO_SERIES_F64,                                    // (Series<T, M>) -> Series<f64, M>
    FLV_SIGNATURE_KIND_T_TO_T,                                                  // (T) -> T
    FLV_SIGNATURE_KIND_SERIES_DURATION_TO_SERIES,                               // (Series<T, M>, Duration) -> Series<T, M>
    FLV_SIGNATURE_KIND_MODE_TO_SELECTION,                                       // (Mode) -> Selection<flight>
    FLV_SIGNATURE_KIND_I64_TO_SELECTION,                                        // (i64) -> Selection<flight>
    FLV_SIGNATURE_KIND_SELECTION_TO_DURATION,                                   // (Selection<M>) -> Duration
};

#define FLV_STDLIB_PARAM_T 0
#define FLV_STDLIB_PARAM_M 1

#define FLV_MODE_NUMBER_NONE U32_MAX

#define FLV_PLANE_MODES(X)  \
    X(MANUAL, 0)            \
    X(CIRCLE, 1)            \
    X(STABILIZE, 2)         \
    X(TRAINING, 3)          \
    X(ACRO, 4)              \
    X(FBWA, 5)              \
    X(FBWB, 6)              \
    X(CRUISE, 7)            \
    X(AUTOTUNE, 8)          \
    X(AUTO, 10)             \
    X(RTL, 11)              \
    X(LOITER, 12)           \
    X(TAKEOFF, 13)          \
    X(AVOID_ADSB, 14)       \
    X(GUIDED, 15)           \
    X(INITIALISING, 16)     \
    X(QSTABILIZE, 17)       \
    X(QHOVER, 18)           \
    X(QLOITER, 19)          \
    X(QLAND, 20)            \
    X(QRTL, 21)             \
    X(QAUTOTUNE, 22)        \
    X(QACRO, 23)            \
    X(THERMAL, 24)          \
    X(LOITER_ALT_QLAND, 25)

#define FLV_COPTER_MODES(X) \
    X(STABILIZE, 0)         \
    X(ACRO, 1)              \
    X(ALT_HOLD, 2)          \
    X(AUTO, 3)              \
    X(GUIDED, 4)            \
    X(LOITER, 5)            \
    X(RTL, 6)               \
    X(CIRCLE, 7)            \
    X(LAND, 9)              \
    X(DRIFT, 11)            \
    X(SPORT, 13)            \
    X(FLIP, 14)             \
    X(AUTOTUNE, 15)         \
    X(POSHOLD, 16)          \
    X(BRAKE, 17)            \
    X(THROW, 18)            \
    X(AVOID_ADSB, 19)       \
    X(GUIDED_NOGPS, 20)     \
    X(SMART_RTL, 21)        \
    X(FLOWHOLD, 22)         \
    X(FOLLOW, 23)           \
    X(ZIGZAG, 24)           \
    X(SYSTEMID, 25)         \
    X(AUTOROTATE, 26)       \
    X(AUTO_RTL, 27)         \
    X(TURTLE, 28)

typedef u8 FLVVehicleKind;
enum : u8 {
    FLV_VEHICLE_KIND_PLANE,
    FLV_VEHICLE_KIND_COPTER,
    FLV_VEHICLE_KIND_COUNT,
    FLV_VEHICLE_KIND_UNKNOWN = FLV_VEHICLE_KIND_COUNT
};

// @types______________________________________________________________________
struct FLVModeName {
    Str8 Name;
    u32  Number;
};

struct FLVBuiltin {
    Str8             Name;
    FLVSignatureKind SignatureKind;
    u8               AcceptsNumericOnly;
    u16              __Padding;
    TypeIndex        Signature;
    BindIndex        Bind;
};

// @runtime____________________________________________________________________
extern FLVBuiltin FLV_STDLIB_TABLE[FLV_STDLIB_ID_COUNT];
extern TypeIndex FLV_STDLIB_FLIGHT;
extern BindIndex FLV_STDLIB_ARMED;
extern FLVModeName const* const FLV_MODE_TABLE[FLV_VEHICLE_KIND_COUNT];
extern u32 const FLV_MODE_TABLE_COUNT[FLV_VEHICLE_KIND_COUNT];

// @functions__________________________________________________________________
void FLVStdLibInit(void);
void FLVStdLibBindModes(FLVVehicleKind Kind);
FLVVehicleKind FLVVehicleKindFromStr(Str8 KindString);
u32 FLVModeNumberFromName(FLVVehicleKind Kind, Str8 Name);

#endif // __FLV_STDLIB_H__
