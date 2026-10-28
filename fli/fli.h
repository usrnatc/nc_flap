#if !defined(__FLI_H__)
#define __FLI_H__

#include "nc_types.h"
#include "nc_string.h"
#include "nc_thread.h"
#include "nc_sync.h"
#include "nc_file.h"
#include "nc_object_cache.h"

// @defines____________________________________________________________________
#define FLI_MAGIC_NUMBER     0x494C462E
#define FLI_ENCODING_VERSION 1

#if !defined(FLI_DISABLE_TABLE_INDEX_TYPECHECKING)
    #define FLI_DISABLE_TABLE_INDEX_TYPECHECKING FALSE
#endif

enum FLILoadStatus {
    FLI_LOAD_STATUS_NULL,
    FLI_LOAD_STATUS_ACTIVE,
    FLI_LOAD_STATUS_COMPLETE
};

enum FLIEventKind {
    FLI_EVENT_KIND_NULL,
    FLI_EVENT_KIND_CONVERSION_STARTED,
    FLI_EVENT_KIND_CONVERSION_ENDED,
    FLI_EVENT_KIND_COUNT
};

enum FLISectionKind {
    FLI_SECTION_KIND_NULL,
    FLI_SECTION_KIND_FILE_INFO,
    FLI_SECTION_KIND_MESSAGE_TYPES,
    FLI_SECTION_KIND_FIELDS,
    FLI_SECTION_KIND_COLUMNS,
    FLI_SECTION_KIND_PARAMS,
    FLI_SECTION_KIND_MESSAGES,
    FLI_SECTION_KIND_MODES,
    FLI_SECTION_KIND_EVENTS,
    FLI_SECTION_KIND_ERRORS,
    FLI_SECTION_KIND_STRING_DATA,
    FLI_SECTION_KIND_STRING_TABLE,
    FLI_SECTION_KIND_INDEX_RUNS,
    FLI_SECTION_KIND_NAME_MAPS,
    FLI_SECTION_KIND_NAME_MAP_BUCKETS,
    FLI_SECTION_KIND_NAME_MAP_NODES,
    FLI_SECTION_KIND_COUNT
};

typedef u32 FLISectionEncoding;
enum : u32 {
    FLI_SECTION_ENCODING_UNPACKED,
    FLI_SECTION_ENCODING_LZB
};

typedef u32 FLINameMapKind;
enum : u32 {
    FLI_NAME_MAP_KIND_NULL,
    FLI_NAME_MAP_KIND_MESSAGE_TYPES,
    FLI_NAME_MAP_KIND_FIELDS,
    FLI_NAME_MAP_KIND_PARAMS,
    FLI_NAME_MAP_KIND_STRING_DATA,
    FLI_NAME_MAP_KIND_STRING_TABLE,
    FLI_NAME_MAP_KIND_COUNT
};

typedef u8 FLIScalarKind;
enum : u8 {
    FLI_SCALAR_KIND_NULL,
    FLI_SCALAR_KIND_U8,
    FLI_SCALAR_KIND_I8,
    FLI_SCALAR_KIND_U16,
    FLI_SCALAR_KIND_I16,
    FLI_SCALAR_KIND_U32,
    FLI_SCALAR_KIND_I32,
    FLI_SCALAR_KIND_U64,
    FLI_SCALAR_KIND_I64,
    FLI_SCALAR_KIND_F32,
    FLI_SCALAR_KIND_F64,
    FLI_SCALAR_KIND_CHAR4,
    FLI_SCALAR_KIND_CHAR16,
    FLI_SCALAR_KIND_CHAR64,
    FLI_SCALAR_KIND_COUNT
};

enum FLIParseStatus {
    FLI_PARSE_STATUS_SUCCESS,
    FLI_PARSE_STATUS_HEADER_MISMATCH,
    FLI_PARSE_STATUS_VERSION_MISMATCH,
    FLI_PARSE_STATUS_DATA_SECTION_LAYOUT_MISMATCH,
};

#define FLI_HEADER_XLIST      \
    X(u32, Magic)             \
    X(u32, EncodingVersion)   \
    X(u32, DataSectionOffset) \
    X(u32, DataSectionCount)  \
 

#define FLI_SECTION_XLIST               \
    X(FLISectionEncoding, Encoding)     \
    X(u32,                Pad)          \
    X(u64,                Offset)       \
    X(u64,                EncodedSize)  \
    X(u64,                UnpackedSize) \
 

#define FLI_SECTIONKIND_XLIST                             \
    X(Null,           null,             u8)               \
    X(FileInfo,       file_info,        FLIFileInfo)      \
    X(MessageTypes,   message_types,    FLIMessageType)   \
    X(Fields,         fields,           FLIField)         \
    X(Columns,        columns,          FLIColumn)        \
    X(Params,         params,           FLIParam)         \
    X(Messages,       messages,         FLIMessage)       \
    X(Modes,          modes,            FLIMode)          \
    X(Events,         events,           FLIEvt)           \
    X(Errors,         errors,           FLIError)         \
    X(StringData,     string_data,      u8)               \
    X(StringTable,    string_table,     u32)              \
    X(IndexRuns,      index_runs,       u32)              \
    X(NameMaps,       name_maps,        FLINameMap)       \
    X(NameMapBuckets, name_map_buckets, FLINameMapBucket) \
    X(NameMapNodes,   name_map_nodes,   FLINameMapNode)   \
 

#define FLI_FILEINFO_XLIST           \
    X(u32,    SourcePathStringIndex) \
    X(u32,    VehicleStringIndex)    \
    X(u32,    FirmwareStringIndex)   \
    X(u32,    __Padding)             \
    X(FLIMD5, SourceMD5)             \
    X(u64,    StartTimeUSecs)           \
    X(u64,    EndTimeUSecs)             \
    X(u64,    RecordCount)           \
    X(u64,    SkippedRecordCount)    \
 

#define FLI_MESSAGETYPE_XLIST   \
    X(u8,  ID)                  \
    X(u8,  Length)              \
    X(u16, __Padding)           \
    X(u32, NameStringIndex)     \
    X(u32, FormatStringIndex)   \
    X(u32, HeadField)           \
    X(u32, FieldCount)          \
    X(u32, InstanceField)       \
    X(u32, TimeColumn)          \
    X(u32, HeadColumn)          \
    X(u32, ColumnCount)         \
    X(u64, RecordCount)         \
 

#define FLI_FIELD_XLIST               \
    X(u32,           NameStringIndex) \
    X(u32,           UnitStringIndex) \
    X(u8,            FormatChar)      \
    X(FLIScalarKind, LoggedKind)      \
    X(FLIScalarKind, StoredKind)      \
    X(u8,            LoggedSize)      \
    X(u32,           ColumnIndex)     \
    X(f64,           Multiplier)      \
 

#define FLI_COLUMN_XLIST               \
    X(u32,           MessageTypeIndex) \
    X(u32,           FieldIndex)       \
    X(u32,           SectionIndex)     \
    X(FLIScalarKind, Kind)             \
    X(u8,            __Padding[3])     \
    X(u64,           Count)            \
    X(f64,           Min)              \
    X(f64,           Max)              \
    X(f64,           Mean)             \
 

#define FLI_PARAM_XLIST      \
    X(u32, NameStringIndex)  \
    X(f32, Value)            \
    X(f32, Default)          \
    X(u32, __Padding)        \
    X(u64, TimeUSecs)           \
 

#define FLI_MESSAGE_XLIST    \
    X(u64, TimeUSecs)           \
    X(u32, TextStringIndex)  \
    X(u32, __Padding)        \
 

#define FLI_MODE_XLIST       \
    X(u64, TimeUSecs)           \
    X(u8,  Mode)             \
    X(u8,  ModeNum)          \
    X(u8,  Reason)           \
    X(u8,  __Padding[5])     \
 

#define FLI_EVENT_XLIST      \
    X(u64, TimeUSecs)           \
    X(u8,  ID)               \
    X(u8,  __Padding[7])     \
 

#define FLI_ERROR_XLIST      \
    X(u64, TimeUSecs)           \
    X(u8,  Subsystem)        \
    X(u8,  Code)             \
    X(u8,  __Padding[6])     \
 

#define FLI_NAMEMAP_XLIST    \
    X(u32, BucketBaseIndex)  \
    X(u32, NodeBaseIndex)    \
    X(u32, BucketCount)      \
    X(u32, NodeCount)        \
 

#define FLI_NAMEMAPBUCKET_XLIST \
    X(u32, FirstNode)           \
    X(u32, NodeCount)           \
 

#define FLI_NAMEMAPNODE_XLIST         \
    X(u32, StringIndex)               \
    X(u32, MatchCount)                \
    X(u32, MatchIndexOrIndexRunFirst) \
 
#define FLITableFromName(X, Y, Z, W)        ((FLISectionElementType##Y*) FLISectionRawTableFromKind((X), FLI_SECTION_KIND_##Z, (W)))
#define FLIElementFromNameIndex(X, Y, Z, W) ((FLISectionElementType##Y*) FLISectionRawElementFromKindIndex((X), FLI_SECTION_KIND_##Z, (W)))
#define FLIMessageTypeFromMatch(M, Match)   FLIElementFromNameIndex((M), MessageTypes, MESSAGE_TYPES, (Match).Index)
#define FLIFieldFromMatch(M, Match)         FLIElementFromNameIndex((M), Fields, FIELDS, (Match).Index)
#define FLIParamFromMatch(M, Match)         FLIElementFromNameIndex((M), Params, PARAMS, (Match).Index)
#define FLINameFromMessageType(M, P, L)     FLIStrFromIndex((M), (P)->NameStringIndex, (L))
#define FLINameFromField(M, P, L)           FLIStrFromIndex((M), (P)->NameStringIndex, (L))
#define FLINameFromParam(M, P, L)           FLIStrFromIndex((M), (P)->NameStringIndex, (L))
 
#define FLI_CHUNK_LIST_TYPES(X)        \
    struct FLI##X##ChunkNode {         \
        FLI##X##ChunkNode* Next;       \
        FLI##X*            V;          \
        u64                Count;      \
        u64                Capacity;   \
        u64                BaseIndex;  \
    };                                 \
    struct FLI##X##ChunkList {         \
        FLI##X##ChunkNode* Head;       \
        FLI##X##ChunkNode* Tail;       \
        u64                ChunkCount; \
        u64                TotalCount; \
    };
 
struct FLISection;
 
#define FLIBakeIndexedChunkListPush(MemPool, List, ChunkType, ElementType, CapacityValue, Result, ...) \
    ElementType* Result = NULL;                                                                        \
    do {                                                                                               \
        ChunkType* N = List->Tail;                                                                     \
        if (!N || N->Count >= N->Capacity) {                                                           \
            N = ArenaPushArrayZero(MemPool, ChunkType, 1);                                             \
            N->Capacity = CapacityValue;                                                               \
            N->BaseIndex = List->TotalCount;                                                           \
            __VA_ARGS__;                                                                               \
            N->V = ArenaPushArray(MemPool, ElementType, N->Capacity);                                  \
            SLL_QUEUE_PUSH(List->Head, List->Tail, N);                                                 \
            ++List->ChunkCount;                                                                        \
        }                                                                                              \
        Result = &N->V[N->Count];                                                                      \
        Result->Chunk = N;                                                                             \
        ++N->Count;                                                                                    \
        ++List->TotalCount;                                                                            \
    } while (FALSE);
 
#define FLIBakeIndexedChunkListElementGetIndex(Ptr, Result)        \
    u64 Index = 0;                                                 \
    if (Ptr && Ptr->Chunk)                                         \
        Index = Ptr->Chunk->BaseIndex + (Ptr - Ptr->Chunk->V) + 1;
 
#define FLIBakeIndexedChunkListCat(ChunkType, Dst, ToPush, ...) \
    for (ChunkType* N = ToPush->Head; N; N = N->Next) {         \
         N->BaseIndex += Dst->TotalCount;                       \
    }                                                           \
    if (Dst->Tail && ToPush->Head) {                            \
        Dst->Tail->Next = ToPush->Head;                         \
        Dst->Tail = ToPush->Tail;                               \
        Dst->ChunkCount += ToPush->ChunkCount;                  \
        Dst->TotalCount += ToPush->TotalCount;                  \
        __VA_ARGS__;                                            \
    } else if (!Dst->Head) {                                    \
        MemCpy(Dst, ToPush, sizeof(*Dst));                      \
    }                                                           \
    MemSet(ToPush, 0, sizeof(*ToPush));
 
#define FLISerialisedSectionMakeUnpackedStruct(Ptr)       FLISerialisedSectionMakeUnpacked((Ptr), sizeof(*(Ptr)))
#define FLISerialisedSectionMakeUnpackedArray(Ptr, Count) FLISerialisedSectionMakeUnpacked((Ptr), sizeof(*(Ptr)) * (Count))

// @types______________________________________________________________________
struct FLIKey {
    u64 U64[2];
};

struct FLIKeyNode {
    FLIKeyNode* Next;
    FLIKey      V;
};

struct FLIKeyList {
    FLIKeyNode* Head;
    FLIKeyNode* Tail;
    u64         Count;
};

struct FLIKeyArray {
    FLIKey* V;
    u64     Count;
};

struct FLIKeyPathNode {
    FLIKeyPathNode* Next;
    FLIKeyPathNode* Prev;
    Str8            Path;
    u64             MinTimeStamp;
    FLIKey          Key;
};

struct FLIKeySlot {
    FLIKeyPathNode* Head;
    FLIKeyPathNode* Tail;
};

struct FLIParsed {
    u8*         RawData;
    u64         RawDataSize;
    FLISection* Sections;
    u64         SectionsCount;
};

struct FLINode {
    FLINode*       Next;
    FLINode*       Prev;
    FLIKey         Key;
    Handle         File;
    Handle         FileMap;
    void*          FileBase;
    FileProperties FileProps;
    Arena*         MemPool;
    FLIParsed      FLI;
    AccessPoint    Acc;
    u64            RefCount;
    u64            BatchRequestsCounts[2];
    u64            WorkingCount;
    u64            CompletionCount;
};

struct FLISlot {
    FLINode* Head;
    FLINode* Tail;
};

struct FLIRequest {
    FLIKey Key;
};

struct FLIRequestNode {
    FLIRequestNode* Next;
    FLIRequest      V;
};

struct FLIRequestBatch {
    Handle          Mtx;
    Arena*          MemPool;
    FLIRequestNode* Head;
    FLIRequestNode* Tail;
    u64             Count;
};

struct FLILoadTask {
    FLILoadTask*  Next;
    FLILoadTask*  Prev;
    FLIKey        Key;
    FLILoadStatus Status;
    b32           OriginalAnalysed;
    b32           OriginalIsFLI;
    u64           OriginalSize;
    b32           FLIAnalysed;
    b32           IsFLIStale;
    u64           ThreadCount;
    Handle        Process;
};

struct FLILoadCompletion {
    FLILoadCompletion* Next;
    u64                Code;
};

struct FLISearchItem {
    u64         Index;
    FLIKey      Key;
    u64         MissedSize;
    FMRangeList MatchRanges;
};

struct FLISearchItemChunk {
    FLISearchItemChunk* Next;
    u64                 BaseIndex;
    FLISearchItem*      V;
    u64                 Count;
    u64                 Capacity;
};

struct FLISearchItemChunkList {
    FLISearchItemChunk* Head;
    FLISearchItemChunk* Tail;
    u64                 ChunkCount;
    u64                 TotalCount;
};

struct FLISearchItemArray {
    FLISearchItem* V;
    u64            Count;
};

struct FLIMatch {
    FLIKey         Key;
    FLISectionKind Kind;
    u32            Index;
};

struct FLIEvent {
    FLIEventKind Kind;
    Str8         String;
};

struct FLIEventNode {
    FLIEventNode* Next;
    FLIEvent      V;
};

struct FLIEventList {
    FLIEventNode* Head;
    FLIEventNode* Tail;
    u64           Count;
};

struct FLIShared {
    Arena*             MemPool;
    u64                LoadGeneration;
    u64                LoadCount;
    u64                Key2PathSlotsCount;
    FLIKeySlot*        Key2PathSlots;
    StripeArray        Key2PathStripes;
    u64                Path2KeySlotsCount;
    FLIKeySlot*        Path2KeySlots;
    StripeArray        Path2KeyStripes;
    u64                SlotsCount;
    FLISlot*           Slots;
    StripeArray        Stripes;
    FLIRequestBatch    RequestBatches[2];
    FLILoadTask*       HeadLoadTask[2];
    FLILoadTask*       TailLoadTask[2];
    FLILoadTask*       FreeLoadTask;
    u64                ConversionProcessCount;
    u64                ConversionThreadCount;
    u64                ConversionCompletionCode;
    Str8               ConversionCompletionLockSemaphoreName;
    Str8               ConversionCompletionSignalSemaphoreName;
    Str8               ConversionCompletionSharedMemoryName;
    Handle             ConversionCompletionLockSemaphore;
    Handle             ConversionCompletionSignalSemaphore;
    Handle             ConversionCompletionSharedMemory;
    u64*               ConversionCompletionSharedMemoryBase;
    Handle             ConversionCompletionSignalReceiverThread;
    Handle             CompletionMtx;
    Arena*             CompletionMemPool;
    FLILoadCompletion* HeadCompletion;
    FLILoadCompletion* TailCompletion;
    Handle             EventMtx;
    Arena*             EventMemPool;
    FLIEventList       Events;
};

#if !FLI_DISABLE_TABLE_INDEX_TYPECHECKING
    struct FLIU32MessageTypes { u32 V; };
    struct FLIU32Fields       { u32 V; };
    struct FLIU32Columns      { u32 V; };
    struct FLIU32Params       { u32 V; };
    struct FLIU32Messages     { u32 V; };
    struct FLIU32Modes        { u32 V; };
    struct FLIU32Events       { u32 V; };
    struct FLIU32Errors       { u32 V; };
    struct FLIU32StringTable  { u32 V; };
    struct FLIU32IndexRuns    { u32 V; };
#else
    struct FLIU32Table { u32 V; };
    typedef FLIU32Table FLIU32MessageTypes;
    typedef FLIU32Table FLIU32Fields;
    typedef FLIU32Table FLIU32Columns;
    typedef FLIU32Table FLIU32Params;
    typedef FLIU32Table FLIU32Messages;
    typedef FLIU32Table FLIU32Modes;
    typedef FLIU32Table FLIU32Events;
    typedef FLIU32Table FLIU32Errors;
    typedef FLIU32Table FLIU32StringTable;
    typedef FLIU32Table FLIU32IndexRuns;
#endif

union FLIMD5 {
    u8  U8[16];
    u64 U64[2];
};

struct FLIHeader {
    u64 Magic;
    u32 EncodingVersion;
    u32 DataSectionOffset;
    u32 DataSectionCount;
};

struct FLISection {
    FLISectionEncoding Encoding;
    u32                Pad;
    u64                Offset;
    u64                EncodedSize;
    u64                UnpackedSize;
};
 
struct FLIFileInfo {
    u32    SourcePathStringIndex;
    u32    VehicleStringIndex;
    u32    FirmwareStringIndex;
    u32    __Padding;
    FLIMD5 SourceMD5;
    u64    StartTimeUSecs;
    u64    EndTimeUSecs;
    u64    RecordCount;
    u64    SkippedRecordCount;
};
 
struct FLIMessageType {
    u8  ID;
    u8  Length;
    u16 __Padding;
    u32 NameStringIndex;
    u32 FormatStringIndex;
    u32 HeadField;
    u32 FieldCount;
    u32 InstanceField;
    u32 TimeColumn;
    u32 HeadColumn;
    u32 ColumnCount;
    u64 RecordCount;
};
 
struct FLIField {
    u32           NameStringIndex;
    u32           UnitStringIndex;
    u8            FormatChar;
    FLIScalarKind LoggedKind;
    FLIScalarKind StoredKind;
    u8            LoggedSize;
    u32           ColumnIndex;
    f64           Multiplier;
};
 
struct FLIColumn {
    u32           MessageTypeIndex;
    u32           FieldIndex;
    u32           SectionIndex;
    FLIScalarKind Kind;
    u8            __Padding[3];
    u64           Count;
    f64           Min;
    f64           Max;
    f64           Mean;
};
 
struct FLIParam {
    u32 NameStringIndex;
    f32 Value;
    f32 Default;
    u32 __Padding;
    u64 TimeUSecs;
};
 
struct FLIMessage {
    u64 TimeUSecs;
    u32 TextStringIndex;
    u32 __Padding;
};
 
struct FLIMode {
    u64 TimeUSecs;
    u8  Mode;
    u8  ModeNum;
    u8  Reason;
    u8  __Padding[5];
};
 
struct FLIEvt {
    u64 TimeUSecs;
    u8  ID;
    u8  __Padding[7];
};
 
struct FLIError {
    u64 TimeUSecs;
    u8  Subsystem;
    u8  Code;
    u8  __Padding[6];
};
 
struct FLINameMap {
    u32 BucketBaseIndex;
    u32 NodeBaseIndex;
    u32 BucketCount;
    u32 NodeCount;
};
 
struct FLINameMapBucket {
    u32 FirstNode;
    u32 NodeCount;
};
 
struct FLINameMapNode {
    u32 StringIndex;
    u32 MatchCount;
    u32 MatchIndexOrIndexRunFirst;
};
 
struct FLIParsedNameMap {
    FLINameMapBucket* Buckets;
    FLINameMapNode*   Nodes;
    u64               BucketCount;
    u64               NodeCount;
};
 
typedef FLIFileInfo      FLISectionElementTypeFileInfo;
typedef FLIMessageType   FLISectionElementTypeMessageTypes;
typedef FLIField         FLISectionElementTypeFields;
typedef FLIColumn        FLISectionElementTypeColumns;
typedef FLIParam         FLISectionElementTypeParams;
typedef FLIMessage       FLISectionElementTypeMessages;
typedef FLIMode          FLISectionElementTypeModes;
typedef FLIEvt           FLISectionElementTypeEvents;
typedef FLIError         FLISectionElementTypeErrors;
typedef u8               FLISectionElementTypeStringData;
typedef u32              FLISectionElementTypeStringTable;
typedef u32              FLISectionElementTypeIndexRuns;
typedef FLINameMap       FLISectionElementTypeNameMaps;
typedef FLINameMapBucket FLISectionElementTypeNameMapBuckets;
typedef FLINameMapNode   FLISectionElementTypeNameMapNodes;
 
union FLIEmptyUnion {
    FLIFileInfo      FileInfo;
    FLIMessageType   MessageType;
    FLIField         Field;
    FLIColumn        Column;
    FLIParam         Param;
    FLIMessage       Message;
    FLIMode          Mode;
    FLIEvt           Event;
    FLIError         Error;
    u8               StringData;
    u32              StringTable;
    FLINameMap       NameMap;
    FLINameMapBucket NameMapBucket;
    FLINameMapNode   NameMapNode;
    u8               ColumnElement[64];
};
 
#define X(Y, Z, W)          \
    struct FLI##Y##Result { \
        W* V;               \
        u64 Count;          \
        u64 TotalSize;      \
    };
 
FLI_SECTIONKIND_XLIST;
 
#undef X
 
struct FLISerialisedSection {
    void*              Data;
    u64                EncodedSize;
    u64                UnpackedSize;
    FLISectionEncoding Encoding;
};
 
struct FLISerialisedSectionBundle {
    FLISerialisedSection  Sections[FLI_SECTION_KIND_COUNT];
    FLISerialisedSection* ColumnSections;
    u64                   ColumnSectionsCount;
};
 
struct FLIBakeField;
struct FLIBakeColumn;
struct FLIBakeMessageTypeChunkNode;
 
struct FLIBakeMessageType {
    FLIBakeMessageTypeChunkNode* Chunk;
    u8                           ID;
    u8                           Length;
    Str8                         Name;
    Str8                         Format;
    FLIBakeField*                HeadField;
    u32                          FieldCount;
    u32                          InstanceField;
    FLIBakeColumn*               TimeColumn;
    FLIBakeColumn*               HeadColumn;
    u32                          ColumnCount;
    u64                          RecordCount;
};
 
struct FLIBakeFieldChunkNode;
 
struct FLIBakeField {
    FLIBakeFieldChunkNode* Chunk;
    FLIBakeMessageType*    MessageType;
    Str8                   Name;
    Str8                   Unit;
    u8                     FormatChar;
    FLIScalarKind          LoggedKind;
    FLIScalarKind          StoredKind;
    u8                     LoggedSize;
    f64                    Multiplier;
    FLIBakeColumn*         Column;
};
 
struct FLIBakeColumnChunkNode;
 
struct FLIBakeColumn {
    FLIBakeColumnChunkNode* Chunk;
    FLIBakeMessageType*     MessageType;
    FLIBakeField*           Field;
    FLIScalarKind           Kind;
    void*                   Data;
    u64                     Count;
    f64                     Min;
    f64                     Max;
    f64                     Mean;
};
 
struct FLIBakeParamChunkNode;
 
struct FLIBakeParam {
    FLIBakeParamChunkNode* Chunk;
    Str8                   Name;
    f32                    Value;
    f32                    Default;
    u64                    TimeUSecs;
};
 
struct FLIBakeMessageChunkNode;
 
struct FLIBakeMessage {
    FLIBakeMessageChunkNode* Chunk;
    Str8                     Text;
    u64                      TimeUSecs;
};
 
struct FLIBakeModeChunkNode;
 
struct FLIBakeMode {
    FLIBakeModeChunkNode* Chunk;
    u64                   TimeUSecs;
    u8                    Mode;
    u8                    ModeNum;
    u8                    Reason;
};
 
struct FLIBakeEventChunkNode;
 
struct FLIBakeEvent {
    FLIBakeEventChunkNode* Chunk;
    u64                    TimeUSecs;
    u8                     ID;
};
 
struct FLIBakeErrorChunkNode;
 
struct FLIBakeError {
    FLIBakeErrorChunkNode* Chunk;
    u64                    TimeUSecs;
    u8                     Subsystem;
    u8                     Code;
};
 
struct FLIBakeFileInfo {
    Str8   SourcePath;
    Str8   Vehicle;
    Str8   Firmware;
    FLIMD5 SourceMD5;
    u64    StartTimeUSecs;
    u64    EndTimeUSecs;
    u64    RecordCount;
    u64    SkippedRecordCount;
};
 
FLI_CHUNK_LIST_TYPES(BakeMessageType);
FLI_CHUNK_LIST_TYPES(BakeField);
FLI_CHUNK_LIST_TYPES(BakeColumn);
FLI_CHUNK_LIST_TYPES(BakeParam);
FLI_CHUNK_LIST_TYPES(BakeMessage);
FLI_CHUNK_LIST_TYPES(BakeMode);
FLI_CHUNK_LIST_TYPES(BakeEvent);
FLI_CHUNK_LIST_TYPES(BakeError);
 
struct FLISortKey {
    u64   Key;
    void* Val;
};
 
struct FLIOrderedRange {
    FLIOrderedRange* Next;
    u64              Start;
    u64              End;
};
 
struct FLIBakeParams {
    FLIBakeFileInfo             FileInfo;
    FLIBakeMessageTypeChunkList MessageTypes;
    FLIBakeFieldChunkList       Fields;
    FLIBakeColumnChunkList      Columns;
    FLIBakeParamChunkList       Params;
    FLIBakeMessageChunkList     Messages;
    FLIBakeModeChunkList        Modes;
    FLIBakeEventChunkList       Events;
    FLIBakeErrorChunkList       Errors;
};
 
struct FLIBakeSection {
    void*              Data;
    FLISectionEncoding Encoding;
    u64                EncodingSize;
    u64                UnpackedSize;
    FLISectionKind     Tag;
    u64                TagIndex;
};
 
struct FLIBakeSectionNode {
    FLIBakeSectionNode* Next;
    FLIBakeSection      V;
};
 
struct FLIBakeSectionList {
    FLIBakeSectionNode* Head;
    FLIBakeSectionNode* Tail;
    u64                 Count;
};
 
struct FLIBakeStr {
    u64  StringHash;
    Str8 String;
};
 
struct FLIBakeStrChunkNode {
    FLIBakeStrChunkNode* Next;
    FLIBakeStr*          V;
    u64                  Count;
    u64                  Capacity;
    u64                  BaseIndex;
};
 
struct FLIBakeStrChunkList {
    FLIBakeStrChunkNode* Head;
    FLIBakeStrChunkNode* Tail;
    u64                  ChunkCount;
    u64                  TotalCount;
};
 
struct FLIBakeStrMapTopology {
    u64 SlotsCount;
};
 
struct FLIBakeStrMapBaseIndices {
    u64* SlotsBaseIndices;
};
 
struct FLIBakeStrMapLoose {
    FLIBakeStrChunkList** Slots;
};
 
struct FLIBakeStrMapTight {
    FLIBakeStrChunkList* Slots;
    u64*                 SlotsBaseIndices;
    u64                  SlotsCount;
    u64                  TotalCount;
};
 
struct FLIBakeIndexRun {
    u64  RunHash;
    u64  Count;
    u32* Indices;
};
 
struct FLIBakeIndexRunChunkNode {
    FLIBakeIndexRunChunkNode* Next;
    FLIBakeIndexRun*          V;
    u64                       Count;
    u64                       Capacity;
};
 
struct FLIBakeIndexRunChunkList {
    FLIBakeIndexRunChunkNode* Head;
    FLIBakeIndexRunChunkNode* Tail;
    u64                       ChunkCount;
    u64                       TotalCount;
};
 
struct FLIBakeIndexRunMapTopology {
    u64 SlotsCount;
};
 
struct FLIBakeIndexRunMapLoose {
    FLIBakeIndexRunChunkList** Slots;
    u64*                       SlotsIndicesCount;
};
 
struct FLIBakeIndexRunMap {
    FLIBakeIndexRunChunkList* Slots;
    u64*                      SlotsBaseIndices;
    u64                       SlotsCount;
};
 
struct FLIBakeName {
    Str8 String;
    u64  NameHash;
    u64  Index;
};
 
struct FLIBakeNameChunkNode {
    FLIBakeNameChunkNode* Next;
    FLIBakeName*          V;
    u64                   Count;
    u64                   Capacity;
    u64                   BaseIndex;
};
 
struct FLIBakeNameChunkList {
    FLIBakeNameChunkNode* Head;
    FLIBakeNameChunkNode* Tail;
    u64                   ChunkCount;
    u64                   TotalCount;
};
 
struct FLIBakeNameMapTopology {
    u64  SlotsCount;
};
 
struct FLIBakeNameMap {
    FLIBakeNameChunkList** Slots;
};
 
struct FLIStringBakeResult {
    u32* StringOffsets;
    u64  StringOffsetsCount;
    u8*  StringData;
    u64  StringDataSize;
};
 
struct FLIFileInfoBakeResult {
    FLIFileInfo* FileInfo;
    u64          FileInfoCount;
};
 
struct FLIMessageTypeBakeResult {
    FLIMessageType* MessageTypes;
    u64             MessageTypesCount;
};
 
struct FLIFieldBakeResult {
    FLIField* Fields;
    u64       FieldsCount;
};
 
struct FLIColumnBakeResult {
    FLIColumn*            Columns;
    u64                   ColumnsCount;
    FLISerialisedSection* ColumnData;
};
 
struct FLIParamBakeResult {
    FLIParam* Params;
    u64       ParamsCount;
};
 
struct FLIMessageBakeResult {
    FLIMessage* Messages;
    u64         MessagesCount;
};
 
struct FLIModeBakeResult {
    FLIMode* Modes;
    u64      ModesCount;
};
 
struct FLIEvtBakeResult {
    FLIEvt* Events;
    u64     EventsCount;
};
 
struct FLIErrorBakeResult {
    FLIError* Errors;
    u64       ErrorsCount;
};
 
struct FLINameMapBakeResult {
    FLINameMap*       NameMaps;
    u64               NameMapsCount;
    FLINameMapBucket* Buckets;
    u64               BucketsCount;
    FLINameMapNode*   Nodes;
    u64               NodesCount;
};
 
struct FLIIndexRunBakeResult {
    u32* IndexRuns;
    u64  IndexRunsCount;
};
 
struct FLIBakeResults {
    FLIFileInfoBakeResult    FileInfo;
    FLIStringBakeResult      Strings;
    FLIMessageTypeBakeResult MessageTypes;
    FLIFieldBakeResult       Fields;
    FLIColumnBakeResult      Columns;
    FLIParamBakeResult       Params;
    FLIMessageBakeResult     Messages;
    FLIModeBakeResult        Modes;
    FLIEvtBakeResult         Events;
    FLIErrorBakeResult       Errors;
    FLINameMapBakeResult     NameMaps;
    FLIIndexRunBakeResult    IndexRuns;
};
 
struct FLIBakeShared {
    FLIBakeStrMapTight       BakeStrings;
    FLIBakeIndexRunMap       BakeIndexRuns;
    FLIStringBakeResult      BakedStrings;
    FLIIndexRunBakeResult    BakedIndexRuns;
    u64*                     LaneNameMapNodeCounts[FLI_NAME_MAP_KIND_COUNT];
    u64*                     LaneNameMapNodeOffs[FLI_NAME_MAP_KIND_COUNT];
    u64                      NameMapNodeCounts[FLI_NAME_MAP_KIND_COUNT];
    u64                      TotalNameMapNodeCount;
    FLINameMapBakeResult     BakedNameMaps;
    FLIFileInfoBakeResult    BakedFileInfo;
    FLIMessageTypeBakeResult BakedMessageTypes;
    FLIFieldBakeResult       BakedFields;
    FLIColumnBakeResult      BakedColumns;
    FLIParamBakeResult       BakedParams;
    FLIMessageBakeResult     BakedMessages;
    FLIModeBakeResult        BakedModes;
    FLIEvtBakeResult         BakedEvents;
    FLIErrorBakeResult       BakedErrors;
};

// @runtime____________________________________________________________________
extern FLIShared* FLI_SHARED;
extern FLIParsed* const EMPTY_FLI_PARSED_VALUE;
extern u64 FLI_SECTION_ELEMENT_SIZE_TABLE[FLI_SECTION_KIND_COUNT];
extern u64 FLI_SCALAR_KIND_SIZE_TABLE[FLI_SCALAR_KIND_COUNT];
extern FLIEmptyUnion* const EMPTY_FLI_ELEMENT_UNION_VALUE;
extern FLIBakeShared* FLI_BAKE_SHARED;

// @functions__________________________________________________________________
void ListPush(Arena* MemPool, FLIKeyList* List, FLIKey Key);
FLIKeyArray FLIKeyArrayFromList(Arena* MemPool, FLIKeyList* List);
void FLIInit(CommandLine* CLI);
FLIKey FLIKeyFromPathTimeStamp(Str8 Path, u64 MinTimeStamp);
void FLIOpen(FLIKey Key);
void FLIClose(FLIKey Key, b32 ForceClosed);
u64 FLILoadGeneration(void);
u64 FLILoadCount(void);
FLIKeyArray FLIPushAllLoadedKeys(Arena* MemPool);
FLIParsed* FLIParsedFromKey(Access* Acc, FLIKey Key, b32 HighPriority, u64 EndTimeUSecs);
FLIEventList FLIGetEvents(Arena* MemPool);
void FLIAsyncTick(void);
void FLISignalCompletion(void);
void FLIConversionCompletionSignalReceiverThreadEntryPoint(void* Params);
OCObject FLISearchObjectCreate(Str8 Key, b32* CancelSignal, OUT b32* Retry, OUT u64* Generation);
void FLISearchObjectDestroy(OCObject Object);
FLISearchItemArray FLISearchItemArrayFromTargetQuery(Access* Acc, FLISectionKind Section, Str8 Query, u64 EndTimeUSecs, OUT b32* Stale);
OCObject FLIMatchObjectCreate(Str8 Key, b32* CancelSignal, OUT b32* Retry, OUT u64* Generation);
FLIMatch FLIMatchFromStr(Str8 String, u64 Index, FLIKey PreferredFLIKey, u64 EndTimeUSecs);
FLIParseStatus FLIParse(u8* Data, u64 Size, OUT FLIParsed* Out);
u64 FLIDecompressedSizeFromParsed(FLIParsed* Parsed);
void FLIDecompressParsed(u8* OutBuffer, u64 OutSize, FLIParsed* Parsed);
void* FLISectionRawTableFromKind(FLIParsed* FLI, FLISectionKind Kind, OUT u64* ElementCount);
void* FLISectionRawDataFromKind(FLIParsed* FLI, FLISectionKind Kind, OUT FLISectionEncoding* Encoding, OUT u64* Size);
void* FLISectionRawElementFromKindIndex(FLIParsed* FLI, FLISectionKind Kind, u64 Index);
void* FLIColumnRawDataFromIndex(FLIParsed* FLI, u32 ColumnIndex, OUT u64* ElementCount);
u32 FLIColumnIndexFromNames(FLIParsed* FLI, Str8 MessageTypeName, Str8 FieldName);
void FLIParsedFromNameMap(FLIParsed* FLI, FLINameMap* Map, FLIParsedNameMap* OutParsedMap);
FLINameMapNode* FLINameMapLookup(FLIParsed* FLI, FLIParsedNameMap* Map, u8* NameStr, u64 NameLen);
u32* FLIMatchesFromMapNode(FLIParsed* FLI, FLINameMapNode* Node, OUT u32* Count);
u8* FLIStrFromIndex(FLIParsed* FLI, u32 Index, OUT u64* StringLength);
u64 Hash(u8* Ptr, u64 Size);
FLISortKey* FLISortKeyArray(Arena* MemPool, FLISortKey* Keys, u64 Count);
FLIBakeMessageType* FLIBakeMessageTypeChunkListPush(Arena* MemPool, FLIBakeMessageTypeChunkList* List, u64 Cap);
u64 FLIIndexFromBakeMessageType(FLIBakeMessageType* MessageType);
void FLIBakeMessageTypeChunkListCat(FLIBakeMessageTypeChunkList* Dst, FLIBakeMessageTypeChunkList* ToPush);
FLIBakeField* FLIBakeFieldChunkListPush(Arena* MemPool, FLIBakeFieldChunkList* List, u64 Cap);
u64 FLIIndexFromBakeField(FLIBakeField* Field);
void FLIBakeFieldChunkListCat(FLIBakeFieldChunkList* Dst, FLIBakeFieldChunkList* ToPush);
FLIBakeColumn* FLIBakeColumnChunkListPush(Arena* MemPool, FLIBakeColumnChunkList* List, u64 Cap);
u64 FLIIndexFromBakeColumn(FLIBakeColumn* Column);
void FLIBakeColumnChunkListCat(FLIBakeColumnChunkList* Dst, FLIBakeColumnChunkList* ToPush);
FLIBakeParam* FLIBakeParamChunkListPush(Arena* MemPool, FLIBakeParamChunkList* List, u64 Cap);
u64 FLIIndexFromBakeParam(FLIBakeParam* Param);
void FLIBakeParamChunkListCat(FLIBakeParamChunkList* Dst, FLIBakeParamChunkList* ToPush);
FLIBakeMessage* FLIBakeMessageChunkListPush(Arena* MemPool, FLIBakeMessageChunkList* List, u64 Cap);
u64 FLIIndexFromBakeMessage(FLIBakeMessage* Message);
void FLIBakeMessageChunkListCat(FLIBakeMessageChunkList* Dst, FLIBakeMessageChunkList* ToPush);
FLIBakeMode* FLIBakeModeChunkListPush(Arena* MemPool, FLIBakeModeChunkList* List, u64 Cap);
u64 FLIIndexFromBakeMode(FLIBakeMode* Mode);
void FLIBakeModeChunkListCat(FLIBakeModeChunkList* Dst, FLIBakeModeChunkList* ToPush);
FLIBakeEvent* FLIBakeEventChunkListPush(Arena* MemPool, FLIBakeEventChunkList* List, u64 Cap);
u64 FLIIndexFromBakeEvent(FLIBakeEvent* Event);
void FLIBakeEventChunkListCat(FLIBakeEventChunkList* Dst, FLIBakeEventChunkList* ToPush);
FLIBakeError* FLIBakeErrorChunkListPush(Arena* MemPool, FLIBakeErrorChunkList* List, u64 Cap);
u64 FLIIndexFromBakeError(FLIBakeError* Error);
void FLIBakeErrorChunkListCat(FLIBakeErrorChunkList* Dst, FLIBakeErrorChunkList* ToPush);
void FLIBakeParamsCat(FLIBakeParams* Dst, FLIBakeParams* Src);
FLIBakeStr* FLIBakeStrChunkListPush(Arena* MemPool, FLIBakeStrChunkList* List, u64 Cap);
void FLIBakeStrChunkListCat(FLIBakeStrChunkList* Dst, FLIBakeStrChunkList* ToPush);
FLIBakeStrChunkList FLIBakeStrChunkListSortedFromUnsorted(Arena* MemPool, FLIBakeStrChunkList* Src);
FLIBakeStrMapLoose* FLIBakeStrMapLooseAlloc(Arena* MemPool, FLIBakeStrMapTopology* Top);
void FLIBakeStrMapLooseInsert(Arena* MemPool, FLIBakeStrMapTopology* MapTopology, FLIBakeStrMapLoose* Map, u64 ChunkCap, Str8 String);
FLIBakeStrMapBaseIndices FLIBakeStrMapBaseIndicesFromMapLoose(Arena* MemPool, FLIBakeStrMapTopology* MapTopology, FLIBakeStrMapLoose* Map);
u32 FLIBakeIndexFromStr(FLIBakeStrMapTight* Map, Str8 String);
u64 FLIHashFromIndexRun(u32* IndexRun, u32 Count);
FLIBakeIndexRun* FLIBakeIndexRunChunkListPush(Arena* MemPool, FLIBakeIndexRunChunkList* List, u64 Cap);
void FLIBakeIndexRunChunkListCat(FLIBakeIndexRunChunkList* Dst, FLIBakeIndexRunChunkList* ToPush);
FLIBakeIndexRunChunkList FLIBakeIndexRunChunkListSortedFromUnsorted(Arena* MemPool, FLIBakeIndexRunChunkList* Src);
FLIBakeIndexRunMapLoose* FLIBakeIndexRunMapLooseAlloc(Arena* MemPool, FLIBakeIndexRunMapTopology* Top);
void FLIBakeIndexRunMapLooseInsert(Arena* MemPool, FLIBakeIndexRunMapTopology* MapTopology, FLIBakeIndexRunMapLoose* Map, u64 ChunkCap, u32* Indices, u32 Count);
u32 FLIBakeIndexFromIndexRun(FLIBakeIndexRunMap* Map, u32* Indices, u32 Count);
FLIBakeName* FLIBakeNameChunkListPush(Arena* MemPool, FLIBakeNameChunkList* List, u64 Cap);
void FLIBakeNameChunkListCat(FLIBakeNameChunkList* Dst, FLIBakeNameChunkList* ToPush);
FLIBakeNameChunkList FLIBakeNameChunkListSortedFromUnsorted(Arena* MemPool, FLIBakeNameChunkList* Src);
FLIBakeNameMap* FLIBakeNameMapAlloc(Arena* MemPool, FLIBakeNameMapTopology* Top);
void FLIBakeNameMapInsert(Arena* MemPool, FLIBakeNameMapTopology* MapTopology, FLIBakeNameMap* Map, u64 ChunkCap, Str8 String, u64 Index);
FLIBakeSection* FLIBakeSectionListPush(Arena* MemPool, FLIBakeSectionList* List);
FLIBakeSection* FLIBakeSectionListPushNewUnpacked(Arena* MemPool, FLIBakeSectionList* List, void* Data, u64 Size, FLISectionKind Tag, u64 TagIndex);
void FLIBakeSectionListCat(FLIBakeSectionList* Dst, FLIBakeSectionList* ToPush);
FLISerialisedSection FLISerialisedSectionMakeUnpacked(void* Data, u64 Size);
FLISerialisedSectionBundle FLISerialisedSectionBundleFromBakeResults(Arena* MemPool, FLIBakeResults* Results);
Str8List FLIFileBlobsFromSectionBundle(Arena* MemPool, FLISerialisedSectionBundle* Bundle);
FLIBakeResults FLIBake(Arena* MemPool, FLIBakeParams* Params);
FLISerialisedSectionBundle FLICompress(Arena* MemPool, IN FLISerialisedSectionBundle* Bundle);
 
#if defined(NC_LANG_CPP)
    b32 operator==(FLIKey KeyA, FLIKey KeyB);
    b32 operator!=(FLIKey KeyA, FLIKey KeyB);
#endif

#endif // __FLI_H__
