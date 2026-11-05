#if !defined(__FLV_COMPILER_H__)
#define __FLV_COMPILER_H__

#include "nc_types.h"
#include "nc_string.h"
#include "nc_tls.h"
#include "nc_system.h"

#include "flv_base_types.h"

struct Arena;
struct NamePool;
struct TypeTable;
struct SymTab;
struct ParserState;
struct FLVBuiltin;
struct FLVSchema;

// @defines____________________________________________________________________
#define TYPE_TABLE        (GetTLS()->FLV_COMPILER->Types)
#define KEYWORD_NAME_BASE (GetTLS()->FLV_COMPILER->KeywordNameBase)
#define OP_NAMES          (GetTLS()->FLV_COMPILER->OpNames)
#define FLV_STDLIB_TABLE  (GetTLS()->FLV_COMPILER->StdLib)
#define FLV_STDLIB_FLIGHT (GetTLS()->FLV_COMPILER->StdLibFlight)
#define FLV_STDLIB_ARMED  (GetTLS()->FLV_COMPILER->StdLibArmed)

#define FLV_LOCK_SCOPE_COMPILER() DEFER(FLVCompilerLock(), FLVCompilerUnlock())

// @types______________________________________________________________________
struct FLVCompiler {
    Arena*       MemPool;
    Arena*       SchemaMemPool;
    Handle       Mtx;
    NamePool*    Names;
    TypeTable*   Types;
    SymTab*      SymbolTable;
    ParserState* Parser;
    FLVSchema*   Schema;
    FLVBuiltin*  StdLib;
    u64          SchemaKeyHash;
    u32          KeywordNameBase;
    u32          __Padding;
    NameIndex    OpNames[256];
    TypeIndex    StdLibFlight;
    BindIndex    StdLibArmed;
};

struct FLVSlotName {
    Str8 Message;
    Str8 Field;
    Str8 Mode;
};

// @runtime____________________________________________________________________
extern FLVCompiler* FLV_COMPILER_SHARED;

// @functions__________________________________________________________________
void FLVCompilerInit(void);
FLVCompiler* FLVCompilerLock(void);
void FLVCompilerUnlock(void);

#endif // __FLV_COMPILER_H__
