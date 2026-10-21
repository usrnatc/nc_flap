#include "fli_main.h"
#include "nc_memory.h"
#include "nc_log.h"
#include "nc_print.h"
#include "fli_bin.h"

EXTERN_C_LINK_BEGIN
Str8 FLI_FILE_FORMAT_DISPLAY_NAME_TABLE[FLI_FILE_FORMAT_KIND_COUNT] = {
    Str8Lit(""),
    Str8Lit("Ardupilot DataFlash Log (BIN)"),
    Str8Lit("Flight Log Index (FLI)")
};

FLIOutputKindInfo FLI_OUTPUT_KIND_INFO_TABLE[FLI_OUTPUT_KIND_COUNT] = {
    {
        Str8Lit(""),
        Str8Lit("")
    },
    {
        Str8Lit("fli"),
        Str8Lit("Flight Log Index (.fli) Conversion")
    },
    {
        Str8Lit("dump"),
        Str8Lit("Textual Dumping")
    }
};
EXTERN_C_LINK_END

FLIFile __EMPTY_FLI_FILE_VALUE;
FLIFile* const EMPTY_FLI_FILE_VALUE = &__EMPTY_FLI_FILE_VALUE;
FLIMainShared* FLI_MAIN_SHARED = NULL;

INTERNAL FLIFileFormatKind
FLIFileFormatFromData(Str8 Data)
{
    FLIFileFormatKind Result = FLI_FILE_FORMAT_KIND_NULL;

    if (Result == FLI_FILE_FORMAT_KIND_NULL && Data.Size >= 8) {
        u64 Magic = 0;

        MemCpy(&Magic, Data.Str, sizeof(Magic));

        if (Magic == FLI_MAGIC_NUMBER)
            Result = FLI_FILE_FORMAT_KIND_FLI;
    }

    if (Result == FLI_FILE_FORMAT_KIND_NULL && Data.Size >= 3) {
        u8 BINMagic[] = {
            BIN_HEAD_BYTE_0,
            BIN_HEAD_BYTE_1,
            BIN_FMT_TYPE
        };

        if (MemCmp(Data.Str, BINMagic, sizeof(BINMagic)))
            Result = FLI_FILE_FORMAT_KIND_BIN;
    }

    return Result;
}

INTERNAL void
FLIFileListPush(Arena* MemPool, FLIFileList* List, FLIFile* File)
{
    FLIFileNode* Node = ArenaPushArrayZero(MemPool, FLIFileNode, 1);

    Node->V = File;
    SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
    ++List->Count;
}

void
FLIEntryPoint(CommandLine *CLI)
{
    TempArena Scratch = GetScratch(NULL, 0);
    u64 ThreadsCount = GetSystemProperties()->LogicalProcessorCount;
    Str8 ThreadsCountFromCLIRaw = CommandLineString(CLI, "thread_count"_s8);

    if (ThreadsCountFromCLIRaw.Size) {
        u64 ThreadsCountFromCLI = U64FromStr(ThreadsCountFromCLIRaw);

        if (ThreadsCountFromCLI)
            ThreadsCount = ThreadsCountFromCLI;
    }

    LogInfo("%llu fli threads launched", ThreadsCount);

    Handle* Threads = ArenaPushArrayZero(
        Scratch.MemPool, 
        Handle, 
        ThreadsCount
    );
    FLIThreadParams* ThreadsParams = ArenaPushArrayZero(
        Scratch.MemPool,
        FLIThreadParams,
        ThreadsCount
    );
    Handle Barrier = BarrierAlloc(ThreadsCount);
    u64 BroadcastrValue = 0;

    for (u64 Index = 0; Index < ThreadsCount; ++Index) {
        ThreadsParams[Index].CLI = CLI;
        ThreadsParams[Index].LaneCtx.Index = Index;
        ThreadsParams[Index].LaneCtx.Count = ThreadsCount;
        ThreadsParams[Index].LaneCtx.Barrier = Barrier;
        ThreadsParams[Index].LaneCtx.BroadcastMemory = &BroadcastrValue;
        Threads[Index] = ThreadLaunch(
            FLIThreadEntryPoint, 
            &ThreadsParams[Index]
        );
    }

    for (u64 Index = 0; Index < ThreadsCount; ++Index)
        ThreadJoin(Threads[Index], U64_MAX);

    ReleaseScratch(Scratch);
}

void 
FLIThreadEntryPoint(void* __Params)
{
    FLIThreadParams* Params = (FLIThreadParams*) __Params;
    CommandLine* CLI = Params->CLI;
    LaneContext LaneCtx = Params->LaneCtx;
 
    ThreadSetName("[FLI_%llu]", LaneCtx.Index);
    SetLaneContext(LaneCtx);
 
    Arena* MemPool = ArenaAlloc();
 
    if (!LaneIndex())
        FLI_MAIN_SHARED = ArenaPushArrayZero(MemPool, FLIMainShared, 1);
 
    LaneSync();
 
    if (!LaneIndex()) {
        Str8List InputFilePathTasks = StrListCopy(MemPool, &CLI->Inputs);
 
        for (
            Str8Node* Node = InputFilePathTasks.Head; 
            Node; 
            Node = Node->Next
        ) {
            TempArena Scratch = GetScratch(&MemPool, 1);
            FLIFileFormatKind FileFormat = FLI_FILE_FORMAT_KIND_NULL;
            Handle File = SystemOpenFile(
                SYS_ACCESS_READ | SYS_ACCESS_SHARE_READ, 
                Node->String
            );
            FileProperties FileProps = SystemGetFileProperties(File);
 
            FileFormat = FLIFileFormatFromData(Node->String);
 
            if (FileFormat == FLI_FILE_FORMAT_KIND_NULL && FileProps.Size) {
                u64 ProbeSize = MIN(FileProps.Size, 512);
                u8* ProbeData = ArenaPushArrayZero(
                    Scratch.MemPool, 
                    u8, 
                    ProbeSize
                );
 
                SystemReadFile(File, Rng(0ULL, ProbeSize), ProbeData);
 
                Str8 ProbeStr = Str(ProbeData, ProbeSize);
 
                FileFormat = FLIFileFormatFromData(ProbeStr);
            }
 
            SystemCloseFile(File);
 
            if (FileFormat != FLI_FILE_FORMAT_KIND_NULL) {
                LogInfo(
                    "%S recognized as %S",
                    PRINT_STR(Node->String),
                    PRINT_STR(FLI_FILE_FORMAT_DISPLAY_NAME_TABLE[FileFormat])
                );
            } else {
                LogInfo(
                    "%S was not recognized as a supported format",
                    PRINT_STR(Node->String)
                );
                ReleaseScratch(Scratch);
                continue;
            }
 
            Str8 FileData = {};
            Handle FileHandle = {};
            Handle MapHandle = {};
            b32 IsMapped = FALSE;
 
            if (FileProps.Size > MB(100)) {
                Handle File = SystemOpenFile(
                    SYS_ACCESS_READ | SYS_ACCESS_SHARE_READ,
                    Node->String
                );
 
                if (File != EMPTY_HANDLE_VALUE) {
                    Handle Map = SystemOpenFileMap(SYS_ACCESS_READ, File);
 
                    if (Map != EMPTY_HANDLE_VALUE) {
                        void* View = SystemOpenFileMapView(
                            Map,
                            SYS_ACCESS_READ,
                            Rng(0ULL, FileProps.Size)
                        );
 
                        if (View) {
                            FileData = Str((u8*)View, FileProps.Size);
                            FileHandle = File;
                            MapHandle = Map;
                            IsMapped = TRUE;
 
                            LogInfo(
                                "memory mapped %S (%llu MB)",
                                PRINT_STR(Node->String),
                                FileProps.Size / MB(1)
                            );
                        } else {
                            SystemCloseFileMap(Map);
                            SystemCloseFile(File);
                        }
                    } else {
                        SystemCloseFile(File);
                    }
                }
            }
 
            if (!IsMapped)
                FileData = ReadFileContents(MemPool, Node->String);
 
            FLIFile* InputFile = ArenaPushArrayZero(MemPool, FLIFile, 1);
 
            InputFile->Format = FileFormat;
            InputFile->Path = ArenaPushStrCpy(MemPool, Node->String);
            InputFile->Data = FileData;
            InputFile->FileHandle = FileHandle;
            InputFile->MapHandle = MapHandle;
            FLIFileListPush(MemPool, &FLI_MAIN_SHARED->InputFiles, InputFile);
            ReleaseScratch(Scratch);
        }
    }
 
    LaneSync();
 
    if (!LaneIndex()) {
        for (
            FLIFileNode* N = FLI_MAIN_SHARED->InputFiles.Head;
            N;
            N = N->Next
        ) {
            FLIFileNode* FileNode = ArenaPushArrayZero(
                MemPool, 
                FLIFileNode, 
                1
            );
 
            FileNode->V = N->V;
            SLL_QUEUE_PUSH(
                FLI_MAIN_SHARED->InputFilesFromFormatTable[N->V->Format].Head,
                FLI_MAIN_SHARED->InputFilesFromFormatTable[N->V->Format].Tail,
                FileNode
            );
            ++FLI_MAIN_SHARED->InputFilesFromFormatTable[N->V->Format].Count;
        }
    }
 
    LaneSync();
 
    FLIFileList* InputFilesFromFormatTable = FLI_MAIN_SHARED->InputFilesFromFormatTable;
    FLIOutputKind OutputKind = FLI_OUTPUT_KIND_NULL;
    Str8 OutputPath = CommandLineString(CLI, "out"_s8);
 
    {
        if (OutputKind == FLI_OUTPUT_KIND_NULL) {
            for (u32 K = 1; K < FLI_OUTPUT_KIND_COUNT; ++K) {
                if (
                    CommandLineHasFlag(
                        CLI, 
                        FLI_OUTPUT_KIND_INFO_TABLE[K].Arg
                    )
                ) {
                    OutputKind = (FLIOutputKind) K;
                    break;
                }
            }
        }
 
        if (OutputKind == FLI_OUTPUT_KIND_NULL && OutputPath.Size) {
            Str8 Ext = StrSkipLastDot(OutputPath);
 
            if (StrMatch(Ext, "fli"_s8, STR_MATCH_ALL_CASES)) {
                OutputKind = FLI_OUTPUT_KIND_FLI;
                LogInfo(
                    "Output path has .fli extension; performing `%S`",
                    PRINT_STR(FLI_OUTPUT_KIND_INFO_TABLE[OutputKind].Title)
                );
            } else if (StrMatch(Ext, "dump"_s8, STR_MATCH_ALL_CASES) ||
                       StrMatch(Ext, "txt"_s8, STR_MATCH_ALL_CASES)) {
                OutputKind = FLI_OUTPUT_KIND_DUMP;
                LogInfo(
                    "Output path has .dump or .txt extension; performing `%S`",
                    PRINT_STR(FLI_OUTPUT_KIND_INFO_TABLE[OutputKind].Title)
                );
            }
        }
 
        if (
            OutputKind == FLI_OUTPUT_KIND_NULL && 
            FLI_MAIN_SHARED->InputFiles.Count > 0
        ) {
            FLIFile* FirstFile = TopFLIFile(&FLI_MAIN_SHARED->InputFiles);
 
            if (FirstFile->Format != FLI_FILE_FORMAT_KIND_FLI)
                OutputKind = FLI_OUTPUT_KIND_FLI;
            else
                OutputKind = FLI_OUTPUT_KIND_DUMP;
        }
    }
 
    if (!LaneIndex()) {
        if (
            OutputKind == FLI_OUTPUT_KIND_NULL || 
            !CLI->Inputs.Count
        ) {
            PrintErr("FLI Converter\n\n"_s8);
 
            if (OutputKind != FLI_OUTPUT_KIND_NULL) {
                PrintErr(
                    "%S Help\n", 
                    PRINT_STR(
                        FLI_OUTPUT_KIND_INFO_TABLE[OutputKind].Title
                    )
                );
                PrintErr(
                    "To see top-level options, run the binary with no arguments.\n\n"_s8
                );
            }
 
            PrintErr("-------------------------------------------------------------------------------\n\n"_s8);
        }
    }
 
    LaneSync();
 
    Str8List OutputBlobs = {};
 
    switch (OutputKind) {
        default:
        case FLI_OUTPUT_KIND_NULL: {
            if (!LaneIndex()) {
                PrintErr(
                    "USAGE EXAMPLES\n\n"
                    "fli_convert flight.bin\n"
                    "Converts `flight.bin` to FLI format.\n\n"
                    "fli_convert flight.bin --out:flight.fli\n"
                    "Converts `flight.bin` to FLI, and stores it in `flight.fli`.\n\n"
                    "fli_convert --dump flight.fli\n"
                    "Outputs the textual dump of the data stored in `flight.fli`.\n\n"
                    "-------------------------------------------------------------------------------\n\n"
                    "DESCRIPTION\n\n"
                    "This utility converts ArduPilot DataFlash logs to the Flight Log Index format (.fli).\n"
                    "Supported input formats: BIN.\n\n"
                    "-------------------------------------------------------------------------------\n\n"
                    "ARGUMENTS\n\n"
                    "--fli            Specifies that the utility should convert input files\n"
                    "                 to the Flight Log Index format (.fli).\n\n"
                    "--dump           Specifies that the utility should dump textualized contents of\n"
                    "                 all input files.\n\n"
                    "--out:<path>     Specifies the path to which output data should be written. If\n"
                    "                 not specified, the utility will choose a fallback based on\n"
                    "                 input file names.\n\n"
                    "--deterministic  Turns off all sources of non-determinism in generated output.\n\n"
                    "--no_compress    Disables compression of the output FLI file.\n\n"
                    "--verbose        Outputs all log information collected during execution.\n\n"
                    "--thread_count:<n>  Specifies the number of worker threads to use.\n\n"
                );
            }
        } break;
 
        case FLI_OUTPUT_KIND_FLI: {
            if (
                !LaneIndex() && 
                !CLI->Inputs.Count
            ) {
                PrintErr(
                    "ARGUMENTS\n\n"
                    "--no_compress                    Disables compression of the output.\n"
                    "\n"
                );
            }
 
            FLIFileList* BINFiles = &InputFilesFromFormatTable[FLI_FILE_FORMAT_KIND_BIN];
 
            for (
                FLIFileNode* FileNode = BINFiles->Head;
                FileNode;
                FileNode = FileNode->Next
            ) {
                FLIFile* BINFile = FileNode->V;
                TempArena Scratch = GetScratch(&MemPool, 1);
 
                if (!LaneIndex()) {
                    LogInfo("converting %S", PRINT_STR(BINFile->Path));
                }
 
                FLIBakeParams BakeParams = {};
                B2FConvertParams ConvertParams = {};
 
                ConvertParams.InputBINName = BINFile->Path;
                ConvertParams.InputBINData = BINFile->Data;
                BakeParams = B2FConvert(MemPool, &ConvertParams);
                LaneSync();
 
                FLIBakeResults BakeResults = {};
 
                BakeResults = FLIBake(MemPool, &BakeParams);
                LaneSync();
 
                FLISerialisedSectionBundle Bundle = {};
 
                if (!LaneIndex()) {
                    Bundle = FLISerialisedSectionBundleFromBakeResults(
                        MemPool, 
                        &BakeResults
                    );
                }
 
                LaneSync();
 
                b32 DoCompress = !CommandLineHasFlag(CLI, "no_compress"_s8);
 
                if (DoCompress)
                    Bundle = FLICompress(MemPool, &Bundle);
 
                LaneSync();
 
                if (!LaneIndex()) {
                    Str8 ThisOutputPath = OutputPath;
 
                    if (!ThisOutputPath.Size) {
                        ThisOutputPath = PathReplaceFileExtension(
                            Scratch.MemPool, 
                            BINFile->Path, 
                            "fli"_s8
                        );
                    }
 
                    Str8List FileBlobs = FLIFileBlobsFromSectionBundle(
                        Scratch.MemPool,
                        &Bundle
                    );
 
                    b32 WriteOkay = WriteFileContents(
                        ThisOutputPath,
                        FileBlobs
                    );
 
                    if (WriteOkay)
                        LogInfo("output written to %S", PRINT_STR(ThisOutputPath));
                    else
                        LogError("failed to write output %S", PRINT_STR(ThisOutputPath));
                }
 
                LaneSync();
 
                if (BINFile->MapHandle != EMPTY_HANDLE_VALUE) {
                    SystemCloseFileMapView(
                        BINFile->MapHandle, 
                        BINFile->Data.Str, 
                        Rng(0ULL, BINFile->Data.Size)
                    );
                    SystemCloseFileMap(BINFile->MapHandle);
                    SystemCloseFile(BINFile->FileHandle);
                }
 
                LaneSync();
                ReleaseScratch(Scratch);
            }
 
            if (
                !LaneIndex() && 
                CLI->Inputs.Count != 0 && 
                !BINFiles->Count
            ) {
                LogError(
                    "Could not load log data from the specified inputs. "
                    "Supported formats: BIN."_s8
                );
            }
        } break;
 
        case FLI_OUTPUT_KIND_DUMP: {
            if (
                !LaneIndex() && 
                !CLI->Inputs.Count
            ) {
                PrintErr(
                    "All input files specified on the command line will be dumped.\n"
                    "Currently only FLI files are supported for dumping.\n\n"
                );
            }
 
            FLIFileList* FLIFiles = &InputFilesFromFormatTable[FLI_FILE_FORMAT_KIND_FLI];
 
            for (
                FLIFileNode* FileNode = FLIFiles->Head;
                FileNode;
                FileNode = FileNode->Next
            ) {
                FLIFile* File = FileNode->V;
                b32 Deterministic = CommandLineHasFlag(CLI, "deterministic"_s8);
 
                if (!LaneIndex()) {
                    Str8 DisplayPath = Deterministic 
                        ? StrSkipLastSlash(File->Path) 
                        : File->Path;
 
                    ListPushFmt(
                        MemPool,
                        &OutputBlobs,
                        "// %S (%S)",
                        PRINT_STR(DisplayPath),
                        PRINT_STR(FLI_FILE_FORMAT_DISPLAY_NAME_TABLE[File->Format])
                    );
 
                    FLIParsed Parsed = {};
                    FLIParseStatus Stat = FLIParse(
                        File->Data.Str,
                        File->Data.Size,
                        &Parsed
                    );
 
                    switch (Stat) {
                        default: break;
 
                        case FLI_PARSE_STATUS_HEADER_MISMATCH: {
                            LogError("FLI parse failure: header does not match"_s8);
                        } break;
 
                        case FLI_PARSE_STATUS_VERSION_MISMATCH: {
                            LogError("FLI parse failure: unsupported version"_s8);
                        } break;
 
                        case FLI_PARSE_STATUS_DATA_SECTION_LAYOUT_MISMATCH: {
                            LogError("FLI parse failure: invalid data section layout"_s8);
                        } break;
 
                        case FLI_PARSE_STATUS_SUCCESS: {
                            // TODO: Implement FLI dump
                            ListPushFmt(
                                MemPool,
                                &OutputBlobs,
                                "FLI dump not yet implemented\n"
                            );
                        } break;
                    }
                }
 
                LaneSync();
            }
        } break;
    }
 
    if (!LaneIndex()) {
        if (OutputKind == FLI_OUTPUT_KIND_DUMP) {
            if (OutputPath.Size) {
                b32 IsWritten = WriteFileContents(OutputPath, OutputBlobs);
 
                if (IsWritten)
                    LogInfo("Results written to %S", PRINT_STR(OutputPath));
                else
                    LogError("Failed to write file %S", PRINT_STR(OutputPath));
            } else {
                for (Str8Node* N = OutputBlobs.Head; N; N = N->Next) {
                    for (u64 Off = 0; Off < N->String.Size;) {
                        u64 SizeToWrite = MIN(N->String.Size - Off, GB(2));
 
                        PrintOut("%S", SizeToWrite, N->String.Str + Off);
                        Off += SizeToWrite;
                    }
                }
 
                LogInfo("Results written to stdout"_s8);
            }
        }
    }
 
    LaneSync();
 
    if (!LaneIndex()) {
        b32 Verbose = CommandLineHasFlag(CLI, "verbose"_s8);
 
        if (Verbose) {}
    }
}
