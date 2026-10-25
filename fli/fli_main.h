#if !defined(__FLI_MAIN_H__)
#define __FLI_MAIN_H__

#include "nc_types.h"
#include "fli.h"

// @defines____________________________________________________________________
enum FLIFileFormatKind {
    FLI_FILE_FORMAT_KIND_NULL,
    FLI_FILE_FORMAT_KIND_BIN,
    FLI_FILE_FORMAT_KIND_FLI,
    FLI_FILE_FORMAT_KIND_COUNT
};

enum FLIOutputKind {
    FLI_OUTPUT_KIND_NULL,
    FLI_OUTPUT_KIND_FLI,
    FLI_OUTPUT_KIND_DUMP,
    FLI_OUTPUT_KIND_COUNT
};

#define TopFLIFile(X) ((X)->Head ? (X)->Head->V : EMPTY_FLI_FILE_VALUE)

// @types______________________________________________________________________
struct FLIThreadParams {
    CommandLine* CLI;
    LaneContext  LaneCtx;
};

struct FLIFile {
    FLIFileFormatKind Format;
    Str8              Path;
    Str8              Data;
    Handle            FileHandle;
    Handle            MapHandle;
};

struct FLIFileNode {
    FLIFileNode* Next;
    FLIFile*     V;
};

struct FLIFileList {
    FLIFileNode* Head;
    FLIFileNode* Tail;
    u64          Count;
};

struct FLIOutputKindInfo {
    Str8 Arg;
    Str8 Title;
};

struct FLIMainShared {
    FLIFileList InputFiles;
    FLIFileList InputFilesFromFormatTable[FLI_FILE_FORMAT_KIND_COUNT];
};

// @runtime____________________________________________________________________
EXTERN_C_LINK_BEGIN
extern Str8 FLI_FILE_FORMAT_DISPLAY_NAME_TABLE[FLI_FILE_FORMAT_KIND_COUNT];
extern FLIOutputKindInfo FLI_OUTPUT_KIND_INFO_TABLE[FLI_OUTPUT_KIND_COUNT];
EXTERN_C_LINK_END

extern FLIFile* const EMPTY_FLI_FILE_VALUE;
extern FLIMainShared* FLI_MAIN_SHARED;

// @functions__________________________________________________________________
void FLIEntryPoint(CommandLine* CLI);
void FLIThreadEntryPoint(void* __Params);

#endif // __FLI_MAIN_H__
