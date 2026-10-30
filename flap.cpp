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
#include "flv_run.h"

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
#include "flv_run.cpp"

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

INTERNAL void
FLVTest(
    Arena* MemPool,
    FLIParsed** Flights,
    u32 FlightsCount,
    char const** ViewPaths,
    u32 ViewCount
) {
    NamePool* Names = NamePoolInit();

    PROFILE(SymTabInitTimer) {
        SymTabInit(Names);
    }

    PROFILE(BuiltinTypesInitTimer) {
        BuiltinTypesInit();
    }

    PROFILE(TypeTableInitTimer) {
        TypeTableInit(Names);
    }

    PROFILE(LexerInternKeywordsTimer) {
        LexerInternKeywords(Names);
    }

    PROFILE(ParserInternNamesTimer) {
        ParserInternNames(Names);
    }

    PROFILE(FLVStdLibInitTimer) {
        FLVStdLibInit();
    }

    PROFILE(FLVKernelInitTimer) {
        FLVKernelInit();
    }

    FLVSchema Schema = {};

    PROFILE(FLVSchemaBuildTimer) {
        FLVSchemaBuild(MemPool, Flights, FlightsCount, &Schema);
    }

    PrintOut(
        "\nschema: %u messages, %u fields, %u flights\n",
        Schema.MessageCount,
        Schema.FieldCount,
        FlightsCount
    );

    for (u32 Index = 0; Index < ViewCount; ++Index) {
        Arena* ViewMemPool = ArenaAlloc();
        Str8 Path = Str((char*) ViewPaths[Index]);
        Str8 Source = ReadFileContentsEx(ViewMemPool, Path, NC_SIMD_ALIGN256);

        PrintOut("\n=== %S ===\n", PRINT_STR(Path));

        if (!Source.Size) {
            PrintOut("could not read file\n"_s8);
            ArenaRelease(ViewMemPool);
            continue;
        }

        ParsedFile Root = {};

        Root.Path = Path;
        Root.Source = Source;

        PROFILE(LexStringTimer) {
            Root.Tokens = LexString(ViewMemPool, Source, Names);
        }

        PROFILE(LineOffsetsFromSourceTimer) {
            Root.LineOffsets = LineOffsetsFromSource(
                ViewMemPool, 
                Source, 
                &Root.LineCount
            );
        }

        AST* Tree = NULL;

        PROFILE(ParseTokensTimer) {
            Tree = ParseTokens(ViewMemPool, &Root, NULL, 0);
        }

        FLVSlot* Slots = NULL;
        u32 SlotsCount = 0;

        PROFILE(CheckASTTimer) {
            CheckAST(ViewMemPool, Tree, &Schema, &Slots, &SlotsCount);
        }

        FLVLowered Lowered = {};

        PROFILE(FLVLowerTimer) {
            Lowered = FLVLower(
                ViewMemPool,
                Tree,
                &Schema,
                Slots,
                SlotsCount
            );
        }

        PrintOut(
            "%u slots, %u analyses, %u layout nodes, errors: %s\n",
            SlotsCount,
            Lowered.ProgramCount,
            Lowered.LayoutCount,
            Lowered.HasError ? "YES" : "NO"
        );

        for (u32 Program = 0; Program < Lowered.ProgramCount; ++Program) {
            Str8List Lines = FLVProgramPrint(
                ViewMemPool, 
                &Lowered.Program[Program], 
                Slots
            );

            for (Str8Node* Node = Lines.Head; Node; Node = Node->Next) {
                PrintOut("%S\n", PRINT_STR(Node->String));
            }
        }

        if (FlightsCount) {
            FLVBoundSlot* Bound = NULL;

            PROFILE(FLVBindSlotsTimer) {
                Bound = FLVBindSlots(
                    ViewMemPool, 
                    Flights[0], 
                    Slots, 
                    SlotsCount
                );
            }

            Arena* LaneMemPool = ArenaAlloc();
            Arena* ResultMemPool = ArenaAlloc();

            for (u32 Program = 0; Program < Lowered.ProgramCount; ++Program) {
                FLVLane Lane = {};

                PROFILE(FLVLaneInitTimer) {
                    FLVLaneInit(
                        &Lane, 
                        LaneMemPool, 
                        ResultMemPool, 
                        &Lowered.Program[Program], 
                        Flights[0], 
                        Bound, 
                        SlotsCount, 
                        0
                    );
                }

                PROFILE(FLVLaneRunTimer) {
                    FLVLaneRun(&Lane);
                }

                PrintOut("analysis %u on flight 0:\n", Program);

                Str8List Lines = FLVRenderItemsPrint(ViewMemPool, &Lane);

                for (Str8Node* Node = Lines.Head; Node; Node = Node->Next)
                    PrintOut("%S\n", PRINT_STR(Node->String));

                ArenaClear(LaneMemPool);
            }

            ArenaRelease(LaneMemPool);
            ArenaRelease(ResultMemPool);
        }

        ArenaRelease(ViewMemPool);
    }

    PROFILE(SymTabResetTimer) {
        SymTabReset();
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
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000010.BIN",
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000034.BIN",
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000038.BIN",
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000047.BIN",
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000048.BIN",
        "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000050.BIN",
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000058.BIN",
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000061.BIN",
        // "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\logs\\00000064.BIN"
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
                MILLISECONDS(50)
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
            Access* Acc = AccessOpen();
            FLIParsed* Flights[ARRAY_COUNT(FLIPaths)] = {};

            for (u64 Index = 0; Index < ARRAY_COUNT(FLIPaths); ++Index) {
                Flights[Index] = FLIParsedFromKey(Acc, Keys[Index], TRUE, 0);
            }

            local char const* ViewPaths[] = {
                "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\flv_examples\\power.flv",
                "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\flv_examples\\vibration.flv",
                "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\flv_examples\\ekf.flv",
                "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\flv_examples\\tuning.flv",
                "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\flv_examples\\overview.flv",
                "C:\\Users\\NathanCorcoran\\Documents\\sandbox\\nc_flap\\tools\\flv_examples\\tester.flv"
            };

            FLVTest(
                Scratch.MemPool,
                Flights,
                ARRAY_COUNT(FLIPaths),
                ViewPaths,
                ARRAY_COUNT(ViewPaths)
            );

            AccessClose(Acc);
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
