#if !defined(__FLV_COMPILER_H__)
#define __FLV_COMPILER_H__

#include "nc_types.h"
#include "nc_string.h"
#include "nc_tls.h"

#include "flv_base_types.h"

struct Arena;
struct NamePool;
struct TypeTable;
struct FLVBuiltin;
struct FLVSchema;

// @defines____________________________________________________________________
#define TYPE_TABLE        (GetTLS()->FLV_COMPILER->Types)
#define KEYWORD_NAME_BASE (GetTLS()->FLV_COMPILER->KeywordNameBase)
#define OP_NAMES          (GetTLS()->FLV_COMPILER->OpNames)
#define FLV_STDLIB_TABLE  (GetTLS()->FLV_COMPILER->StdLib)
#define FLV_STDLIB_FLIGHT (GetTLS()->FLV_COMPILER->StdLibFlight)
#define FLV_STDLIB_ARMED  (GetTLS()->FLV_COMPILER->StdLibArmed)

// @types______________________________________________________________________
struct FLVCompiler {
    Arena*      MemPool;
    Arena*      SchemaMemPool;
    NamePool*   Names;
    TypeTable*  Types;
    FLVSchema*  Schema;
    FLVBuiltin* StdLib;
    u64         SchemaKeyHash;
    u32         KeywordNameBase;
    u32         __Padding;
    NameIndex   OpNames[256];
    TypeIndex   StdLibFlight;
    BindIndex   StdLibArmed;
};

struct FLVSlotName {
    Str8 Message;
    Str8 Field;
    Str8 Mode;
};

// @runtime____________________________________________________________________


// @functions__________________________________________________________________
FLVCompiler* FLVCompilerSelected(void);

#endif // __FLV_COMPILER_H__
