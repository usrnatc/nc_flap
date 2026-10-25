#include "libnc.h"

#include "fli.h"
#include "fli_bin.h"
#include "fli_main.h"

#include "fli.cpp"
#include "fli_bin.cpp"
#include "fli_main.cpp"

internal void
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
                        MILLISECONDS(100)
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

void 
EntryPoint(CommandLine* CLI)
{
#if defined(NC_FLI)
    FLIEntryPoint(CLI);
#else
    TempArena Scratch = GetScratch(NULL, 0);
    u64 LaneBroadcastValue = 0;
    u64 AsyncThreadsCount = GetSystemProperties()->LogicalProcessorCount - 1;
    Str8 AsyncThreadsCountString = CommandLineString(CLI, "threads"_s8);

    if (AsyncThreadsCountString.Size)
        AsyncThreadsCount = U64FromStr(AsyncThreadsCountString);

    AsyncThreadsCount = MAX(1, AsyncThreadsCount);

    Handle Barrier = BarrierAlloc(AsyncThreadsCount);
    LaneContext* LaneCtxs = ArenaPushArrayZero(
        Scratch.MemPool,
        LaneContext,
        AsyncThreadsCount
    );

    ASYNC_THREADS_COUNT = AsyncThreadsCount;

    Handle* AsyncThreads = ArenaPushArrayZero(
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

    local char* FLIPaths[] = {
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000010.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000034.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000038.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000047.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000048.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000050.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000058.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000061.BIN",
        "H:\\rfdesign\\nc_flap\\tools\\logs\\00000064.BIN"
    };
    FLIKey Keys[ARRAY_COUNT(FLIPaths)] = {};

    for (u64 Index = 0; Index < ARRAY_COUNT(FLIPaths); ++Index) {
        Str8 Path = Str(FLIPaths[Index]);

        Keys[Index] = FLIKeyFromPathTimeStamp(Path, 0);
        FLIOpen(Keys[Index]);
    }

    for (;;) {
        Access* Acc = AccessOpen();
        b32 GotAllFLIs = TRUE;
        u64 NumFLIsLoaded = 0;

        for (u64 Index = 0; Index < ARRAY_COUNT(FLIPaths); ++Index) {
            FLIParsed* FLI = FLIParsedFromKey(
                Acc,
                Keys[Index],
                TRUE,
                0
            );

            if (FLI == EMPTY_FLI_PARSED_VALUE)
                GotAllFLIs = FALSE;
            else
                ++NumFLIsLoaded;
        }

        PrintOut(
            "\rloaded [%llu/%llu], %llu active threads, %llu active processes",
            NumFLIsLoaded,
            ARRAY_COUNT(FLIPaths),
            FLI_SHARED->ConversionThreadCount,
            FLI_SHARED->ConversionProcessCount
        );
        AccessClose(Acc);

        if (!GotAllFLIs)
            AsyncWaitTick(TimeNow() + MILLISECONDS(50));

        if (GotAllFLIs) {
            PrintOut("\n"_s8);

            for (u64 Index = 0; Index < ARRAY_COUNT(FLIPaths); ++Index) {
                Access* Acc = AccessOpen();
                FLIParsed* FLI = FLIParsedFromKey(Acc, Keys[Index], TRUE, 0);
                FLIFileInfo* Info = FLIElementFromNameIndex(
                    FLI, 
                    FileInfo, 
                    FILE_INFO, 
                    0
                );
                u64 VehicleLength = 0;
                u8* Vehicle = FLIStrFromIndex(
                    FLI, 
                    Info->VehicleStringIndex, 
                    &VehicleLength
                );
                u64 FirmwareLength = 0;
                u8* Firmware = FLIStrFromIndex(
                    FLI, 
                    Info->FirmwareStringIndex, 
                    &FirmwareLength
                );
                u64 MessageTypeCount = 0;
                u64 FieldCount = 0;
                u64 ColumnCount = 0;
                u64 ParamCount = 0;
                u64 MessageCount = 0;
                u64 ModeCount = 0;
                u64 EventCount = 0;
                u64 ErrorCount = 0;

                FLITableFromName(
                    FLI, 
                    MessageTypes, 
                    MESSAGE_TYPES, 
                    &MessageTypeCount
                );
                FLITableFromName(FLI, Fields, FIELDS, &FieldCount);
                FLITableFromName(FLI, Columns, COLUMNS, &ColumnCount);
                FLITableFromName(FLI, Params, PARAMS, &ParamCount);
                FLITableFromName(FLI, Messages, MESSAGES, &MessageCount);
                FLITableFromName(FLI, Modes, MODES, &ModeCount);
                FLITableFromName(FLI, Events, EVENTS, &EventCount);
                FLITableFromName(FLI, Errors, ERRORS, &ErrorCount);

                PrintOut("---- %s\n", FLIPaths[Index]);
                PrintOut(
                    "  sections: %llu (fixed %u + columns %llu)\n", 
                    FLI->SectionsCount, 
                    FLI_SECTION_KIND_COUNT, 
                    FLI->SectionsCount - FLI_SECTION_KIND_COUNT
                );
                PrintOut(
                    "  vehicle: %S\n", 
                    VehicleLength, 
                    Vehicle
                );
                PrintOut(
                    "  firmware: %S\n", 
                    FirmwareLength, 
                    Firmware
                );
                PrintOut(
                    "  records: %llu (skipped %llu)\n", 
                    Info->RecordCount, 
                    Info->SkippedRecordCount
                );
                PrintOut(
                    "  time: start %llu us, end %llu us, duration %.1f s\n", 
                    Info->StartTimeUS, 
                    Info->EndTimeUS, 
                    (f64) (Info->EndTimeUS - Info->StartTimeUS) / 1e6
                );
                PrintOut(
                    "  md5: %016llX%016llX\n", 
                    Info->SourceMD5.U64[0], 
                    Info->SourceMD5.U64[1]
                );
                PrintOut(
                    "  message types %llu, fields %llu, columns %llu\n", 
                    MessageTypeCount - 1, 
                    FieldCount - 1, 
                    ColumnCount - 1
                );
                PrintOut(
                    "  params %llu, messages %llu, modes %llu, events %llu, errors %llu\n", 
                    ParamCount - 1, 
                    MessageCount - 1, 
                    ModeCount - 1, 
                    EventCount - 1, 
                    ErrorCount - 1
                );

                u32 ColumnIndex = FLIColumnIndexFromNames(
                    FLI, 
                    "GPS"_s8, 
                    "Alt"_s8
                );
                FLIColumn* Column = FLIElementFromNameIndex(
                    FLI, 
                    Columns, 
                    COLUMNS, 
                    ColumnIndex
                );
                FLIField* Field = FLIElementFromNameIndex(
                    FLI, 
                    Fields, 
                    FIELDS, 
                    Column->FieldIndex
                );
                u64 UnitLength = 0;
                u8* Unit = FLIStrFromIndex(
                    FLI, 
                    Field->UnitStringIndex, 
                    &UnitLength
                );

                PrintOut(
                    "  GPS.Alt: column %u, kind %u, count %llu, unit '%S', mult %g\n", 
                    ColumnIndex, 
                    Column->Kind, 
                    Column->Count, 
                    UnitLength, 
                    Unit, 
                    Field->Multiplier
                );
                PrintOut(
                    "  GPS.Alt: min %f, max %f, mean %f\n", 
                    Column->Min, 
                    Column->Max, 
                    Column->Mean
                );

                FLIMessageType* MessageType = FLIElementFromNameIndex(
                    FLI, 
                    MessageTypes, 
                    MESSAGE_TYPES, 
                    Column->MessageTypeIndex
                );
                u64 AltCount = 0;
                f64* Alt = (f64*) FLIColumnRawDataFromIndex(
                    FLI, 
                    ColumnIndex, 
                    &AltCount
                );
                u64 TimeCount = 0;
                u64* Time = (u64*) FLIColumnRawDataFromIndex(
                    FLI, 
                    MessageType->TimeColumn, 
                    &TimeCount
                );

                if (AltCount && TimeCount == AltCount) {
                    PrintOut(
                        "  GPS.Alt: first %f @ %llu us, last %f @ %llu us\n", 
                        Alt[0], 
                        Time[0], 
                        Alt[AltCount - 1], 
                        Time[AltCount - 1]
                    );
                } else {
                    PrintOut(
                        "  GPS.Alt: column read failed (alt %llu, time %llu)\n", 
                        AltCount, 
                        TimeCount
                    );
                }

                {
                    FLINameMap* NameMap = FLIElementFromNameIndex(
                        FLI, 
                        NameMaps, 
                        NAME_MAPS, 
                        FLI_NAME_MAP_KIND_PARAMS
                    );
                    FLIParsedNameMap ParsedNameMap = {};

                    FLIParsedFromNameMap(FLI, NameMap, &ParsedNameMap);

                    Str8 ParamName = "ARMING_CHECK"_s8;
                    FLINameMapNode* MapNode = FLINameMapLookup(
                        FLI, 
                        &ParsedNameMap, 
                        ParamName.Str, 
                        ParamName.Size
                    );
                    u32 Num = 0;
                    u32* Run = FLIMatchesFromMapNode(FLI, MapNode, &Num);

                    for (u32 RunIndex = 0; RunIndex < Num; ++RunIndex) {
                        FLIParam* Param = FLIElementFromNameIndex(
                            FLI, 
                            Params, 
                            PARAMS, 
                            Run[RunIndex]
                        );

                        PrintOut(
                            "  %S: %f (default %f) @ %llu us\n", 
                            PRINT_STR(ParamName), 
                            Param->Value, 
                            Param->Default, 
                            Param->TimeUS
                        );
                    }

                    if (!Num)
                        PrintOut("  %S: not found\n", PRINT_STR(ParamName));
                }

                AccessClose(Acc);
            }

            {
                Access* Acc = AccessOpen();
                Str8 SearchQuery = "gyr"_s8;
                b32 Stale = FALSE;
                FLISearchItemArray Items = FLISearchItemArrayFromTargetQuery(
                    Acc,
                    FLI_SECTION_KIND_FIELDS,
                    SearchQuery,
                    U64_MAX,
                    &Stale
                );

                PrintOut(
                    "searched fields for %S, found %llu items\n",
                    PRINT_STR(SearchQuery),
                    Items.Count
                );
                AccessClose(Acc);

                Str8 MatchQuery = "IMU"_s8;
                FLIMatch Match = FLIMatchFromStr(
                    MatchQuery, 
                    0, 
                    Keys[0], 
                    U64_MAX
                );

                PrintOut(
                    "matched %S, found %u in section %u of [%llX:%llX]\n",
                    PRINT_STR(MatchQuery),
                    Match.Index,
                    Match.Kind,
                    Match.Key.U64[0],
                    Match.Key.U64[1]
                );
            }

            break;
        }
    }

    AtomicExchangeU32(&GLOBAL_ASYNC_EXIT, TRUE, MEM_ORDER_SEQ_CST);
    AsyncWake(FALSE);

    for (u64 Index = 0; Index < ASYNC_THREADS_COUNT; ++Index)
        ThreadJoin(AsyncThreads[Index], U64_MAX);

    BarrierRelease(Barrier);
    ReleaseScratch(Scratch);
#endif
}
