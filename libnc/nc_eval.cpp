#include "nc_eval.h"
#include "nc_arena.h"
#include "nc_tls.h"
#include "nc_profile.h"
#include "nc_file.h"
#include "nc_log.h"
#include "nc_types.h"
#include "nc_string.h"

#if !defined(XXH_IMPLEMENTATION)
    #define XXH_IMPLEMENTATION
    #define XXH_STATIC_LINKING_ONLY
    #define XXH_INLINE_ALL
    #define XXH_NO_STDLIB
    #include "xxHash/xxhash.h"
#endif

EXTERN_C_LINK_BEGIN
Str8 EVAL_TOKEN_KIND_STRINGS[EVAL_TOKEN_KIND_COUNT] = {
    Str8Lit("NULL"),
    Str8Lit("Identifier"),
    Str8Lit("Numeric"),
    Str8Lit("String Literal"),
    Str8Lit("Char Literal"),
    Str8Lit("Symbol"),
    Str8Lit("Comment")
};

Str8 EVAL_TYPE_KIND_BASIC_STRING_TABLE[35] = {
    Str8Lit(""),
    Str8Lit("char8"),
    Str8Lit("char16"),
    Str8Lit("char32"),
    Str8Lit("uchar8"),
    Str8Lit("uchar16"),
    Str8Lit("uchar32"),
    Str8Lit("uint8"),
    Str8Lit("uint16"),
    Str8Lit("uint32"),
    Str8Lit("uint64"),
    Str8Lit("uint128"),
    Str8Lit("uint256"),
    Str8Lit("uint512"),
    Str8Lit("int8"),
    Str8Lit("int16"),
    Str8Lit("int32"),
    Str8Lit("int64"),
    Str8Lit("int128"),
    Str8Lit("int256"),
    Str8Lit("int512"),
    Str8Lit("bool"),
    Str8Lit("float16"),
    Str8Lit("float32"),
    Str8Lit("float64"),
    Str8Lit("ptr"),
    Str8Lit("array"),
    Str8Lit("enum"),
    Str8Lit("enum"),
    Str8Lit("set"),
    Str8Lit("lens"),
    Str8Lit("lens_spec"),
    Str8Lit("meta_expr"),
    Str8Lit("meta_display_name"),
    Str8Lit("meta_description")
};

u8 EVAL_TYPE_KIND_BASIC_BYTE_SIZE_TABLE[35] = {
     0,
     1,
     2,
     4,
     1,
     2,
     4,
     1,
     2,
     4,
     8,
    16,
    32,
    64,
     1,
     2,
     4,
     8,
    16,
    32,
    64,
     1,
     2,
     4,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0,
     0
};

Str8 EVAL_EXPR_KIND_STRINGS[EVAL_EXPR_KIND_COUNT] = {
    Str8Lit("NULL"),
    Str8Lit("Reference"),
    Str8Lit("Array Index"),
    Str8Lit("Member Access"),
    Str8Lit("Cast"),
    Str8Lit("Positive"),
    Str8Lit("Negative"),
    Str8Lit("Logical Not"),
    Str8Lit("Bitwise Not"),
    Str8Lit("Multiply"),
    Str8Lit("Divide"),
    Str8Lit("Modulo"),
    Str8Lit("Addition"),
    Str8Lit("Subtraction"),
    Str8Lit("Left Shift"),
    Str8Lit("Right Shift"),
    Str8Lit("Less"),
    Str8Lit("Less or Equal"),
    Str8Lit("Greater"),
    Str8Lit("Greater or Equal"),
    Str8Lit("Equal"),
    Str8Lit("Not Equal"),
    Str8Lit("Bitwise And"),
    Str8Lit("Bitwise Xor"),
    Str8Lit("Bitwise Or"),
    Str8Lit("Logical And"),
    Str8Lit("Logical Or"),
    Str8Lit("Ternary"),
    Str8Lit("Call"),
    Str8Lit("Leaf ByteCode"),
    Str8Lit("Leaf String Literal"),
    Str8Lit("Leaf u64"),
    Str8Lit("Leaf f64"),
    Str8Lit("Leaf f32"),
    Str8Lit("Leaf Identifier"),
    Str8Lit("Leaf Offset"),
    Str8Lit("Leaf Value"),
    Str8Lit("Leaf File Path"),
    Str8Lit("Type Identifier"),
    Str8Lit("Ptr"),
    Str8Lit("Array")
};

EvalOpInfo EVAL_EXPR_KIND_OP_INFO_TABLE[EVAL_EXPR_KIND_COUNT] = {
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit("["),
        Str8Lit("]"),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit("."),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        1,
        Str8Lit("cast("),
        Str8Lit(")"),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_UNARY_PREFIX,
        2,
        Str8Lit("+"),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_UNARY_PREFIX,
        2,
        Str8Lit("-"),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_UNARY_PREFIX,
        2,
        Str8Lit("!"),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_UNARY_PREFIX,
        2,
        Str8Lit("~"),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        3,
        Str8Lit(""),
        Str8Lit(" * "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        3,
        Str8Lit(""),
        Str8Lit(" / "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        3,
        Str8Lit(""),
        Str8Lit(" % "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        4,
        Str8Lit(""),
        Str8Lit(" + "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        4,
        Str8Lit(""),
        Str8Lit(" - "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        5,
        Str8Lit(""),
        Str8Lit(" << "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        5,
        Str8Lit(""),
        Str8Lit(" >> "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        6,
        Str8Lit(""),
        Str8Lit(" < "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        6,
        Str8Lit(""),
        Str8Lit(" <= "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        6,
        Str8Lit(""),
        Str8Lit(" > "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        6,
        Str8Lit(""),
        Str8Lit(" >= "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        7,
        Str8Lit(""),
        Str8Lit(" == "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        7,
        Str8Lit(""),
        Str8Lit(" != "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        8,
        Str8Lit(""),
        Str8Lit(" & "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        9,
        Str8Lit(""),
        Str8Lit(" ^ "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        10,
        Str8Lit(""),
        Str8Lit(" | "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        11,
        Str8Lit(""),
        Str8Lit(" && "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_BINARY,
        12,
        Str8Lit(""),
        Str8Lit(" || "),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(" ? "),
        Str8Lit(""),
        Str8Lit(" : ")
    },
    {
        EVAL_OP_KIND_NULL,
        15,
        Str8Lit(""),
        Str8Lit("("),
        Str8Lit(")"),
        Str8Lit(", ")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    },
    {
        EVAL_OP_KIND_NULL,
        0,
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit(""),
        Str8Lit("")
    }
};

Str8 EVAL_INTERP_CODE_DISPLAY_STRINGS[EVAL_INTERP_CODE_COUNT] = {
    Str8Lit(""),
    Str8Lit("Cannot divide by zero (0)"),
    Str8Lit("Invalid operation"),
    Str8Lit("Invalid Operation types"),
    Str8Lit("Insuffiecient Stack space")
};

u16 EVAL_EVAL_OP_CTRL_BITS_TABLE[EVAL_EVAL_OP_COUNT + 1] = {
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 1, 0, 1),
    EVAL_EVAL_CTRL_BITS( 2, 0, 1),
    EVAL_EVAL_CTRL_BITS( 4, 0, 1),
    EVAL_EVAL_CTRL_BITS( 8, 0, 1),
    EVAL_EVAL_CTRL_BITS(16, 0, 1),
    EVAL_EVAL_CTRL_BITS( 1, 0, 1),
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 2, 2, 1),
    EVAL_EVAL_CTRL_BITS( 2, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 2, 1, 1),
    EVAL_EVAL_CTRL_BITS( 1, 0, 1),
    EVAL_EVAL_CTRL_BITS( 0, 1, 0),
    EVAL_EVAL_CTRL_BITS( 1, 0, 0),
    EVAL_EVAL_CTRL_BITS( 1, 2, 1),
    EVAL_EVAL_CTRL_BITS( 1, 1, 1),
    EVAL_EVAL_CTRL_BITS( 4, 0, 0),
    EVAL_EVAL_CTRL_BITS( 8, 0, 0),
    EVAL_EVAL_CTRL_BITS( 0, 2, 2),
    EVAL_EVAL_CTRL_BITS( 0, 0, 0)
};

EvalConversionMatrixNode EVAL_EVAL_TYPE_GROUP_CONV_KIND_MATRIX[8] = {
    {
        {
            EVAL_EVAL_CONV_KIND_OTHER_TO_OTHER,
            EVAL_EVAL_CONV_KIND_FROM_OTHER,
            EVAL_EVAL_CONV_KIND_FROM_OTHER,
            EVAL_EVAL_CONV_KIND_FROM_OTHER,
            EVAL_EVAL_CONV_KIND_FROM_OTHER
        }
    },
    {
        {
            EVAL_EVAL_CONV_KIND_TO_OTHER,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL
        }
    },
    {
        {
            EVAL_EVAL_CONV_KIND_TO_OTHER,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL
        }
    },
    {
        {
            EVAL_EVAL_CONV_KIND_TO_OTHER,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_LEGAL
        }
    },
    {
        {
            EVAL_EVAL_CONV_KIND_TO_OTHER,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_NOOP
        }
    },
    {
        {
            EVAL_EVAL_CONV_KIND_TO_OTHER,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL
        }
    },
    {
        {
            EVAL_EVAL_CONV_KIND_TO_OTHER,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL,
            EVAL_EVAL_CONV_KIND_LEGAL
        }
    },
    {
        {
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_NOOP,
            EVAL_EVAL_CONV_KIND_NOOP
        }
    }
};


EvalIdentifierResolutionPath EVAL_DEFAULT_IDENT_RES_PATHS[EVAL_IDENT_RES_PATH_COUNT] = {
    EVAL_IDENT_RES_PATH_PARENT_EXPR,
    EVAL_IDENT_RES_PATH_PARENT_EXPR_MEMBER,
    EVAL_IDENT_RES_PATH_BUILTIN_CONSTANTS,
    EVAL_IDENT_RES_PATH_BUILTIN_TYPES,
    EVAL_IDENT_RES_PATH_MACROS
};


EvalIdentifierResolutionPath EVAL_CALLABLE_IDENT_RES_PATHS[EVAL_IDENT_RES_PATH_COUNT] = {
    EVAL_IDENT_RES_PATH_MACROS,
    EVAL_IDENT_RES_PATH_PARENT_EXPR,
    EVAL_IDENT_RES_PATH_BUILTIN_CONSTANTS,
    EVAL_IDENT_RES_PATH_BUILTIN_TYPES
};
EXTERN_C_LINK_END

global readonly Str8 EVAL_MULTI_CHAR_SYMBOL_STRINGS[] = {
    Str8Lit("<<"),
    Str8Lit(">>"),
    Str8Lit("->"),
    Str8Lit("<="),
    Str8Lit(">="),
    Str8Lit("=="),
    Str8Lit("!="),
    Str8Lit("&&"),
    Str8Lit("||"),
    Str8Lit("=>")
};

global readonly i64 EVAL_MAX_PRECEDENCE = 15;

EvalStr2ExprMap        __EMPTY_EVAL_STR2EXPR_MAP_VALUE;
EvalStr2ExprMap* const EMPTY_EVAL_STR2EXPR_MAP_VALUE    = &__EMPTY_EVAL_STR2EXPR_MAP_VALUE;
EvalExpr               __EMPTY_EVAL_EXPR_VALUE;
EvalExpr* const        EMPTY_EVAL_EXPR_VALUE            = &__EMPTY_EVAL_EXPR_VALUE;
EvalIRNode             __EMPTY_EVAL_IR_NODE_VALUE;
EvalIRNode* const      EMPTY_EVAL_IR_NODE_VALUE         = &__EMPTY_EVAL_IR_NODE_VALUE;
EvalEval               __EMPTY_EVAL_EVAL_VALUE;
EvalEval* const        EMPTY_EVAL_EVAL_VALUE            = &__EMPTY_EVAL_EVAL_VALUE;
EvalCacheBundle        __EMPTY_EVAL_CACHE_BUNDLE_VALUE;
EvalCacheBundle* const EMPTY_EVAL_CACHE_BUNDLE_VALUE    = &__EMPTY_EVAL_CACHE_BUNDLE_VALUE;
EvalToken              __EMPTY_EVAL_TOKEN_VALUE;
EvalToken* const       EMPTY_EVAL_TOKEN_VALUE           = &__EMPTY_EVAL_TOKEN_VALUE;
EVBlock                __EMPTY_EV_BLOCK_VALUE;
EVBlock* const         EMPTY_EV_BLOCK_VALUE             = &__EMPTY_EV_BLOCK_VALUE;

// EvalInit:
// ----------------------------------------------------------------------------
//
// Initializes all global empty value placeholders used throughout the eval system. 
// Sets up the empty EvalExpr, EvalIRNode, EvalEval, EvalCacheBundle, EvalToken, 
// EvalProjectInfo, and EVBlock structures with their internal linked list pointers 
// pointing to themselves. Also initializes the empty EvalKey, EvalTypeKey, 
// and various map values to their zero states.
void 
EvalInit(void)
{
    EMPTY_EVAL_EXPR_VALUE->Head = EMPTY_EVAL_EXPR_VALUE;
    EMPTY_EVAL_EXPR_VALUE->Tail = EMPTY_EVAL_EXPR_VALUE;
    EMPTY_EVAL_EXPR_VALUE->Next = EMPTY_EVAL_EXPR_VALUE;
    EMPTY_EVAL_EXPR_VALUE->Prev = EMPTY_EVAL_EXPR_VALUE;
    EMPTY_EVAL_EXPR_VALUE->Ref = EMPTY_EVAL_EXPR_VALUE;
    EMPTY_EVAL_EVAL_VALUE->Expr = EMPTY_EVAL_EXPR_VALUE;
    EMPTY_EVAL_EVAL_VALUE->IRTree.Root = EMPTY_EVAL_IR_NODE_VALUE;
    EMPTY_EVAL_CACHE_BUNDLE_VALUE->Parse = {
        {},
        0,
        EMPTY_EVAL_EXPR_VALUE,
        EMPTY_EVAL_EXPR_VALUE
    };
    EMPTY_EVAL_CACHE_BUNDLE_VALUE->IRTree.Root = EMPTY_EVAL_IR_NODE_VALUE;
    EMPTY_EVAL_IR_NODE_VALUE->Head = EMPTY_EVAL_IR_NODE_VALUE;
    EMPTY_EVAL_IR_NODE_VALUE->Tail = EMPTY_EVAL_IR_NODE_VALUE;
    EMPTY_EVAL_IR_NODE_VALUE->Next = EMPTY_EVAL_IR_NODE_VALUE;
    EMPTY_EV_BLOCK_VALUE->Head = EMPTY_EV_BLOCK_VALUE;
    EMPTY_EV_BLOCK_VALUE->Tail = EMPTY_EV_BLOCK_VALUE;
    EMPTY_EV_BLOCK_VALUE->Next = EMPTY_EV_BLOCK_VALUE;
    EMPTY_EV_BLOCK_VALUE->Prev = EMPTY_EV_BLOCK_VALUE;
    EMPTY_EV_BLOCK_VALUE->Parent = EMPTY_EV_BLOCK_VALUE;
    EMPTY_EV_BLOCK_VALUE->Key = {};
    EMPTY_EV_BLOCK_VALUE->SplitRelativeIndex = 0;
    EMPTY_EV_BLOCK_VALUE->String = {};
    EMPTY_EV_BLOCK_VALUE->Eval = {
        {},
        {},
        {},
        EMPTY_EVAL_EXPR_VALUE,
        EMPTY_EVAL_IR_NODE_VALUE
    };
    EMPTY_EV_BLOCK_VALUE->Filter = {};
    EMPTY_EV_BLOCK_VALUE->TypeExpandInfo = {};
    EMPTY_EV_BLOCK_VALUE->TypeExpandRule = (EvalTypeExpandRule*) &EVAL_DEFAULT_TYPE_EXPAND_RULES;
    EMPTY_EV_BLOCK_VALUE->VisualExpandInfo = {};
    EMPTY_EV_BLOCK_VALUE->VisualExpandRule = (EVExpandRule*) &EV_NULL_EXPAND_RULE;
}

// EvalOpCodeFromExprKind:
// ----------------------------------------------------------------------------
//
// Converts an EvalExprKind enum value to its corresponding EvalEvalOp bytecode operation. 
// Uses a switch statement to map expression kinds like NEG to EVAL_EVAL_OP_NEG, ADD 
// to EVAL_EVAL_OP_ADD, and comparison operators to their opcode equivalents. 
// Returns EVAL_EVAL_OP_MEM_READ as the default fallback for unhandled expression kinds.
EvalEvalOp
EvalOpCodeFromExprKind(EvalExprKind Kind)
{
    EvalEvalOp Result = EVAL_EVAL_OP_MEM_READ;

    switch (Kind) {
        case EVAL_EXPR_KIND_NEG: {
            Result = EVAL_EVAL_OP_NEG;
        } break;

        case EVAL_EXPR_KIND_LOG_NOT: {
            Result = EVAL_EVAL_OP_LOG_NOT;
        } break;

        case EVAL_EXPR_KIND_BIT_NOT: {
            Result =  EVAL_EVAL_OP_BIT_NOT;
        } break;

        case EVAL_EXPR_KIND_MUL: {
            Result = EVAL_EVAL_OP_MUL;
        } break;

        case EVAL_EXPR_KIND_DIV: {
            Result = EVAL_EVAL_OP_DIV;
        } break;

        case EVAL_EXPR_KIND_MOD: {
            Result = EVAL_EVAL_OP_MOD;
        } break;

        case EVAL_EXPR_KIND_ADD: {
            Result = EVAL_EVAL_OP_ADD;
        } break;

        case EVAL_EXPR_KIND_SUB: {
            Result = EVAL_EVAL_OP_SUB;
        } break;

        case EVAL_EXPR_KIND_L_SHIFT: {
            Result = EVAL_EVAL_OP_LEFT_SHIFT;
        } break;

        case EVAL_EXPR_KIND_R_SHIFT: {
            Result = EVAL_EVAL_OP_RIGHT_SHIFT;
        } break;

        case EVAL_EXPR_KIND_LESS: {
            Result = EVAL_EVAL_OP_LESS;
        } break;

        case EVAL_EXPR_KIND_LESS_EQ: {
            Result = EVAL_EVAL_OP_LESS_EQ;
        } break;

        case EVAL_EXPR_KIND_GREATER: {
            Result = EVAL_EVAL_OP_GREATER;
        } break;

        case EVAL_EXPR_KIND_GREATER_EQ: {
            Result = EVAL_EVAL_OP_GREATER_EQ;
        } break;

        case EVAL_EXPR_KIND_EQ_EQ: {
            Result = EVAL_EVAL_OP_EQ_EQ;
        } break;

        case EVAL_EXPR_KIND_NOT_EQ: {
            Result = EVAL_EVAL_OP_NOT_EQ;
        } break;

        case EVAL_EXPR_KIND_BIT_AND: {
            Result = EVAL_EVAL_OP_BIT_AND;
        } break;

        case EVAL_EXPR_KIND_BIT_XOR: {
            Result = EVAL_EVAL_OP_BIT_XOR;
        } break;

        case EVAL_EXPR_KIND_BIT_OR: {
            Result = EVAL_EVAL_OP_BIT_OR;
        } break;

        case EVAL_EXPR_KIND_LOG_AND: {
            Result = EVAL_EVAL_OP_LOG_AND;
        } break;

        case EVAL_EXPR_KIND_LOG_OR: {
            Result = EVAL_EVAL_OP_LOG_OR;
        } break;
    }

    return Result;
}

// EvalExprKindIsCmp:
// ----------------------------------------------------------------------------
//
// Determines whether an expression kind represents a comparison operation. 
// Checks against equality (EQ_EQ, NOT_EQ) and relational 
// (LESS, LESS_EQ, GREATER, GREATER_EQ) expression kinds. 
// Returns TRUE if the kind matches any comparison operator, 
// otherwise returns FALSE.
b32 
EvalExprKindIsCmp(EvalExprKind Kind)
{
    b32 Result = FALSE;

    switch (Kind) {
        default: {} break;

        case EVAL_EXPR_KIND_EQ_EQ:
        case EVAL_EXPR_KIND_NOT_EQ:
        case EVAL_EXPR_KIND_LESS:
        case EVAL_EXPR_KIND_LESS_EQ:
        case EVAL_EXPR_KIND_GREATER:
        case EVAL_EXPR_KIND_GREATER_EQ: {
            Result = TRUE;
        } break;
    }

    return Result;
}


// ListPush (EvalTypeKeyList):
// ----------------------------------------------------------------------------
//
// Pushes a new EvalTypeKey onto the end of a EvalTypeKeyList linked list. 
// Allocates a new EvalTypeKeyNode from the memory pool, copies the key into the node, 
// and uses SLL_QUEUE_PUSH to add it to the list's tail. 
// Increments the list's count and returns void.
void 
ListPush(Arena* MemPool, EvalTypeKeyList* List, EvalTypeKey Key)
{
    EvalTypeKeyNode* Node = ArenaPushArrayZero(MemPool, EvalTypeKeyNode, 1);

    Node->V = Key;
    SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
    ++List->Count;
}

// ListPushFront (EvalTypeKeyList):
// ----------------------------------------------------------------------------
//
// Pushes a new EvalTypeKey onto the front of a EvalTypeKeyList linked list. 
// Allocates a new EvalTypeKeyNode from the memory pool, copies the key into the node, 
// and uses SLL_QUEUE_PUSH_FRONT to add it to the list's head. 
// Increments the list's count and returns void.
void 
ListPushFront(Arena* MemPool, EvalTypeKeyList* List, EvalTypeKey Key)
{
    EvalTypeKeyNode* Node = ArenaPushArrayZero(MemPool, EvalTypeKeyNode, 1);

    Node->V = Key;
    SLL_QUEUE_PUSH_FRONT(List->Head, List->Tail, Node);
    ++List->Count;
}


// ListCpy (EvalTypeKeyList):
// ----------------------------------------------------------------------------
//
// Creates a shallow copy of an EvalTypeKeyList into a new list. 
// Iterates through the source list's nodes using a for loop, calling ListPush 
// for each node to copy the keys into the destination list. 
// Returns the newly created EvalTypeKeyList structure with the copied nodes.
EvalTypeKeyList 
ListCpy(Arena* MemPool, EvalTypeKeyList* List)
{
    EvalTypeKeyList Result = {};

    for (EvalTypeKeyNode* Node = List->Head; Node; Node = Node->Next)
        ListPush(MemPool, &Result, Node->V);

    return Result;
}

// EvalMessage (Str8):
// ----------------------------------------------------------------------------
//
// Appends a new EvalMsg to an EvalMsgList with the specified kind, range, 
// and string message. Allocates a new EvalMsg from the memory pool, sets 
// its fields, and links it into the message list using SLL_QUEUE_PUSH. 
// Updates the message list's count and max kind tracking, then returns void.
void 
EvalMessage(
    Arena* MemPool, 
    EvalMsgList* Msgs, 
    EvalMsgKind Kind, 
    r1u64 Range, 
    Str8 String
) {
    EvalMsg* Msg = ArenaPushArrayZero(MemPool, EvalMsg, 1);

    SLL_QUEUE_PUSH(Msgs->Head, Msgs->Tail, Msg);
    ++Msgs->Count;
    Msgs->MaxKind = MAX(Kind, Msgs->MaxKind);
    Msg->Kind = Kind;
    Msg->Range = Range;
    Msg->Text = String;
}


// EvalMessage (format string):
// ----------------------------------------------------------------------------
//
// Appends a new EvalMsg to an EvalMsgList with formatted string content. 
// Uses va_list and va_start to capture variable arguments, then calls 
// ArenaPushStrFmtV to format the message string into a buffer. 
// Ends the va_list and delegates to the Str8 version of EvalMessage to add 
// the formatted message to the list.
void 
EvalMessage(
    Arena* MemPool, 
    EvalMsgList* Msgs, 
    EvalMsgKind Kind, 
    r1u64 Range, 
    char* Fmt, 
    ...
) {
    va_list Args;

    va_start(Args, Fmt);

    Str8 String = ArenaPushStrFmtV(MemPool, Fmt, Args);

    va_end(Args);
    EvalMessage(MemPool, Msgs, Kind, Range, String);
}

// ListCat (EvalMsgList):
// ----------------------------------------------------------------------------
//
// Concatenates one EvalMsgList into another by linking the source list's 
// head to the destination's tail. If the destination already has messages, 
// links the tail to the source head; otherwise copies the entire source list. 
// Zeros out the source list after concatenation and updates the 
// destination's max kind.
void 
ListCat(EvalMsgList* Dst, EvalMsgList* Src)
{
    if (Dst->Tail && Src->Head) {
        Dst->Tail->Next = Src->Head;
        Dst->Tail = Src->Tail;
        Dst->Count += Src->Count;
        Dst->MaxKind = MAX(Dst->MaxKind, Src->MaxKind);
    } else if (Src->Head) {
        MemCpy(Dst, Src, sizeof(*Dst));
    }

    MemZero(Src, sizeof(*Src));
}


// ListCpy (EvalMsgList):
// ----------------------------------------------------------------------------
//
// Creates a deep copy of an EvalMsgList into a new list. 
// Iterates through the source list's messages using a for loop, 
// calling EvalMessage for each message to copy the kind, range, 
// and text into the destination. Returns the newly created EvalMsgList structure.
EvalMsgList 
ListCpy(Arena* MemPool, EvalMsgList* List)
{
    EvalMsgList Result = {};

    for (EvalMsg* Msg = List->Head; Msg; Msg = Msg->Next)
        EvalMessage(MemPool, &Result, Msg->Kind, Msg->Range, Msg->Text);

    return Result;
}

// EvalSpaceAlloc:
// ----------------------------------------------------------------------------
//
// Allocates and initializes an EvalSpace structure with the specified space kind. 
// Sets the space's kind field to the provided EvalSpaceKind and zero-initializes 
// the union fields. Returns the newly created EvalSpace structure ready for 
// use in memory space operations.
EvalSpace 
EvalSpaceAlloc(EvalSpaceKind Kind)
{
    EvalSpace Result = {};

    Result.Kind = Kind;

    return Result;
}

// EvalStr2ExprMapAlloc:
// ----------------------------------------------------------------------------
//
// Creates a new EvalStr2ExprMap for string-to-expression lookups. 
// Allocates an array of slots from the memory pool with the specified slot count, 
// zeros them out, and stores the slot count and slot pointer in the map structure. 
// Returns the initialized EvalStr2ExprMap ready for insertions.
EvalStr2ExprMap 
EvalStr2ExprMapAlloc(Arena* MemPool, u64 SlotsCount)
{
    EvalStr2ExprMap Result = {};

    Result.SlotsCount = SlotsCount;
    Result.Slots = ArenaPushArrayZero(
        MemPool,
        EvalStr2ExprMapSlot,
        Result.SlotsCount
    );

    return Result;
}

// MapInsert (EvalStr2ExprMap):
// ----------------------------------------------------------------------------
//
// Inserts a string-to-expression mapping into a EvalStr2ExprMap. 
// Hashes the string to find the slot index, checks if an existing node 
// matches the string, and if not found, allocates a new node from the memory pool. 
// Copies the string and stores the expression pointer in the node, linking it 
// into the hash chain.
void 
MapInsert(Arena* MemPool, EvalStr2ExprMap* Map, Str8 String, EvalExpr* Expr)
{
    u64 StringHash = Hash(String);
    u64 SlotIndex = StringHash % Map->SlotsCount;
    EvalStr2ExprMapNode* FoundNode = NULL;

    for (
        EvalStr2ExprMapNode* Node = Map->Slots[SlotIndex].Head;
        Node;
        Node = Node->HashNext
    ) {
        if (StrMatch(Node->String, String, 0)) {
            FoundNode = Node;
            break;
        }
    }

    if (!FoundNode) {
        EvalStr2ExprMapNode* Node = ArenaPushArrayZero(
            MemPool,
            EvalStr2ExprMapNode,
            1
        );

        SLL_QUEUE_PUSH_EX(
            Map->Slots[SlotIndex].Head,
            Map->Slots[SlotIndex].Tail,
            Node,
            HashNext
        );
        Node->String = ArenaPushStrCpy(MemPool, String);
        FoundNode = Node;
        FoundNode->Expr = Expr;
    }
}

// EvalStr2ExprMapPoisonIncrement:
// ----------------------------------------------------------------------------
//
// Increments the poison count for a specific string entry in a EvalStr2ExprMap. 
// Hashes the string to find the slot, iterates through the hash chain nodes to 
// find a matching string, and increments its PoisonCount field. 
// Used to temporarily prevent a macro from being resolved recursively.
void 
EvalStr2ExprMapPoisonIncrement(EvalStr2ExprMap* Map, Str8 String)
{
    u64 StringHash = Hash(String);
    u64 SlotIndex = StringHash % Map->SlotsCount;

    for (
        EvalStr2ExprMapNode* Node = Map->Slots[SlotIndex].Head;
        Node;
        Node = Node->HashNext
    ) {
        if (StrMatch(Node->String, String, 0)) {
            ++Node->PoisonCount;
            break;
        }
    }
}

// EvalStr2ExprMapPoisonDecrement:
// ----------------------------------------------------------------------------
//
// Decrements the poison count for a specific string entry in a EvalStr2ExprMap. 
// Hashes the string to find the slot, iterates through the hash chain nodes 
// to find a matching string, and decrements its PoisonCount field. 
// Used to release a recursive poison lock on a macro.
void 
EvalStr2ExprMapPoisonDecrement(EvalStr2ExprMap* Map, Str8 String)
{
    u64 StringHash = Hash(String);
    u64 SlotIndex = StringHash % Map->SlotsCount;

    for (
        EvalStr2ExprMapNode* Node = Map->Slots[SlotIndex].Head;
        Node;
        Node = Node->HashNext
    ) {
        if (StrMatch(Node->String, String, 0)) {
            --Node->PoisonCount;
            break;
        }
    }
}

// EvalStr2ExprMapLookup:
// ----------------------------------------------------------------------------
//
// Looks up an expression from a EvalStr2ExprMap by its string key. 
// Hashes the input string to find the slot, then iterates through the hash 
// chain nodes comparing strings with StrMatch. 
// Returns the found expression pointer if a matching, un-poisoned node exists, 
// otherwise returns EMPTY_EVAL_EXPR_VALUE.
EvalExpr* 
EvalStr2ExprMapLookup(EvalStr2ExprMap* Map, Str8 String)
{
    EvalExpr* Result = EMPTY_EVAL_EXPR_VALUE;

    if (Map->SlotsCount) {
        u64 StringHash = Hash(String);
        u64 SlotIndex = StringHash % Map->SlotsCount;
        EvalStr2ExprMapNode* FoundNode = NULL;

        for (
            EvalStr2ExprMapNode* Node = Map->Slots[SlotIndex].Head;
            Node;
            Node = Node->HashNext
        ) {
            if (StrMatch(Node->String, String, 0) && !Node->PoisonCount) {
                FoundNode = Node;
                break;
            }
        }

        if (FoundNode)
            Result = FoundNode->Expr;
    }

    return Result;
}

// EvalStr2TypeKeyMapAlloc:
// ----------------------------------------------------------------------------
//
// Creates a new EvalStr2TypeKeyMap for string-to-type-key lookups. 
// Allocates an array of slots from the memory pool with the specified slot count, 
// zeros them out, and stores the slot count and slot pointer in the map structure. 
// Returns the initialized EvalStr2TypeKeyMap.
EvalStr2TypeKeyMap 
EvalStr2TypeKeyMapAlloc(Arena* MemPool, u64 SlotsCount)
{
    EvalStr2TypeKeyMap Result = {};

    Result.SlotsCount = SlotsCount;
    Result.Slots = ArenaPushArrayZero(
        MemPool,
        EvalStr2TypeKeySlot,
        Result.SlotsCount
    );

    return Result;
}

// MapInsert (EvalStr2TypeKeyMap):
// ----------------------------------------------------------------------------
//
// Inserts a string-to-type-key mapping into a EvalStr2TypeKeyMap. 
// Hashes the string to find the slot index, allocates a new node from the 
// memory pool, copies the string into the node, sets the type key, and 
// links the node into the slot's linked list. Returns void after insertion.
void 
MapInsert(
    Arena* MemPool, 
    EvalStr2TypeKeyMap* Map, 
    Str8 String, 
    EvalTypeKey TypeKey
) {
    EvalStr2TypeKeyNode* Node = ArenaPushArrayZero(
        MemPool,
        EvalStr2TypeKeyNode,
        1
    );
    u64 StringHash = Hash(String);
    u64 SlotIndex = StringHash % Map->SlotsCount;

    SLL_QUEUE_PUSH(
        Map->Slots[SlotIndex].Head,
        Map->Slots[SlotIndex].Tail,
        Node
    );
    Node->String = ArenaPushStrCpy(MemPool, String);
    Node->Key = TypeKey;
}

// EvalStr2TypeKeyMapLookup:
// ----------------------------------------------------------------------------
//
// Looks up a type key from a EvalStr2TypeKeyMap by its string key. 
// Hashes the input string to find the slot, then iterates through the 
// slot's linked list comparing strings with StrMatch. 
// Returns the found type key if a matching node exists, otherwise 
// returns EMPTY_EVAL_TYPE_KEY_VALUE.
EvalTypeKey 
EvalStr2TypeKeyMapLookup(EvalStr2TypeKeyMap* Map, Str8 String)
{
    EvalTypeKey Result = {};
    u64 StringHash = Hash(String);
    u64 SlotIndex = StringHash % Map->SlotsCount;

    for (
        EvalStr2TypeKeyNode* Node = Map->Slots[SlotIndex].Head;
        Node;
        Node = Node->Next
    ) {
        if (StrMatch(Node->String, String, 0)) {
            Result = Node->Key;
            break;
        }
    }

    return Result;
}

// EvalCacheAlloc:
// ----------------------------------------------------------------------------
//
// Creates a new EvalCache structure with its own dedicated memory pool. 
// Allocates a new arena, then allocates the EvalCache structure from that arena. 
// Stores the memory pool pointer and records the starting position for 
// later reset operations. Returns the newly created cache ready for selection and use.
EvalCache* 
EvalCacheAlloc(void)
{
    Arena* MemPool = ArenaAlloc();
    EvalCache* Result = ArenaPushArrayZero(MemPool, EvalCache, 1);

    Result->MemPool = MemPool;
    Result->EvalMemPoolStartPosition = ArenaGetPosition(MemPool);

    return Result;
}

// EvalCacheRelease:
// ----------------------------------------------------------------------------
//
// Releases all memory associated with an EvalCache by calling 
// ArenaRelease on its memory pool. Frees the entire cache including all cached 
// expressions, types, and evaluation results. Returns void after the 
// cache is destroyed.
void 
EvalCacheRelease(EvalCache* Cache)
{
    ArenaRelease(Cache->MemPool);
}

// EvalCacheSelect:
// ----------------------------------------------------------------------------
//
// Sets the current thread's active evaluation cache to the specified cache. 
// Retrieves the thread-local storage and assigns the EVAL_CACHE pointer to 
// the provided cache. Returns void after the cache is selected for the 
// current thread.
void 
EvalCacheSelect(EvalCache* Cache)
{
    GetTLS()->EVAL_CACHE = Cache;
}

// EvalBaseContextSelect:
// ----------------------------------------------------------------------------
//
// Selects a new base context for evaluation and initializes or resets the 
// thread-local cache. Validates that the context has project info arrays, 
// then sets the thread-local EVAL_BASE_CTX. Resets the cache's memory pool 
// to the start position, zeros the key ID generation, and reinitializes all 
// cache structures including key slots, string slots, constructed type slots, 
// member cache, enum cache, type cache, string ID map, and auto-hook cache 
// with default sizes.
void 
EvalBaseContextSelect(EvalBaseContext* Ctx)
{
    ThreadLocalStorage* const TLS = GetTLS();
    EvalCache* const __EvalCache = TLS->EVAL_CACHE;

    TLS->EVAL_BASE_CTX = Ctx;
    ArenaPopTo(
        __EvalCache->MemPool, 
        __EvalCache->EvalMemPoolStartPosition
    );
    __EvalCache->KeyIDGeneration = 0;
    __EvalCache->KeySlotsCount = 4096;
    __EvalCache->KeySlots = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalCacheSlot,
        __EvalCache->KeySlotsCount
    );
    __EvalCache->StringSlotsCount = 4096;
    __EvalCache->StringSlots = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalCacheSlot,
        __EvalCache->StringSlotsCount
    );
    __EvalCache->FreeParentNode = NULL;
    __EvalCache->HeadParentNode = NULL;
    __EvalCache->ConstructedIDGeneration = 0;
    __EvalCache->ConstructedContentSlotsCount = 256;
    __EvalCache->ConstructedKeySlotsCount = 256;
    __EvalCache->ConstructedContentSlots = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalConstructedTypeSlot,
        __EvalCache->ConstructedContentSlotsCount
    );
    __EvalCache->ConstructedKeySlots = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalConstructedTypeSlot,
        __EvalCache->ConstructedKeySlotsCount
    );

    __EvalCache->TypeCacheSlotsCount = 1024;
    __EvalCache->TypeCacheSlots = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalTypeCacheSlot,
        __EvalCache->TypeCacheSlotsCount
    );
    __EvalCache->FileTypeKey = EvalTypeKeyConstruct(
        __Params.Kind = EVAL_TYPE_KIND_SET,
        __Params.Name = "file"_s8
    );
    __EvalCache->FolderTypeKey = EvalTypeKeyConstruct(
        __Params.Kind = EVAL_TYPE_KIND_SET,
        __Params.Name = "folder"_s8,
        __Params.ExpandRule.ExpandInfoFunc = EVAL_TYPE_EXPAND_INFO_FUNC_NAME(Folder),
        __Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(Folder),
        __Params.ExpandRule.ExpandIDFromNumFunc = EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_NAME(Folder),
        __Params.ExpandRule.ExpandNumFromIDFunc = EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_NAME(Folder)
    );
    __EvalCache->StringIDGeneration = 0;
    __EvalCache->StringIDMap = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalStrIDMap,
        1
    );
    __EvalCache->StringIDMap->IDSlotsCount = 1024;
    __EvalCache->StringIDMap->IDSlots = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalStrIDSlot,
        __EvalCache->StringIDMap->IDSlotsCount
    );
    __EvalCache->StringIDMap->HashSlotsCount = 1024;
    __EvalCache->StringIDMap->HashSlots = ArenaPushArrayZero(
        __EvalCache->MemPool,
        EvalStrIDSlot,
        __EvalCache->StringIDMap->HashSlotsCount
    );
}

// EvalIRContextSelect:
// ----------------------------------------------------------------------------
//
// Selects a new IR context for the current thread. Validates that the context's 
// maps are initialized, creating a new macro map if needed. 
// Sets the thread-local EVAL_IR_CTX to the provided context. 
// Returns void after the IR context is selected.
void 
EvalIRContextSelect(EvalIRContext* Ctx)
{
    ThreadLocalStorage* const TLS = GetTLS();

    if (!Ctx->MacroMap) {
        Ctx->MacroMap = ArenaPushArrayZero(
            TLS->EVAL_CACHE->MemPool,
            EvalStr2ExprMap,
            1
        );

        *Ctx->MacroMap = EvalStr2ExprMapAlloc(TLS->EVAL_CACHE->MemPool, 512);
    }

    TLS->EVAL_IR_CTX = Ctx;
}

// EvalPushParentKey:
// ----------------------------------------------------------------------------
//
// Pushes a new parent key onto the evaluation cache's parent key stack for 
// hierarchical key generation. Saves the current head parent key as the result, 
// then allocates or reuses a cache parent node from the free list. 
// Stores the provided key in the node and pushes it onto the stack. 
// Returns the old parent key that was replaced.
EvalKey 
EvalPushParentKey(EvalKey Key)
{
    EvalKey Result = {};
    ThreadLocalStorage* const TLS = GetTLS();
    EvalCache* __EvalCache = TLS->EVAL_CACHE;

    if (__EvalCache->HeadParentNode)
        Result = __EvalCache->HeadParentNode->Key;

    EvalCacheParentNode* Node = __EvalCache->FreeParentNode;

    if (Node)
        SLL_STACK_POP(__EvalCache->FreeParentNode);
    else
        Node = ArenaPushArrayZero(__EvalCache->MemPool, EvalCacheParentNode, 1);

    SLL_STACK_PUSH(__EvalCache->HeadParentNode, Node);
    Node->Key = Key;

    return Result;
}

// EvalPopParentKey:
// ----------------------------------------------------------------------------
//
// Pops the most recently pushed parent key from the evaluation cache's parent 
// key stack. Retrieves the head node from the cache, saves its key as the 
// result, then pops it from the stack and pushes it onto the free list for 
// reuse. Returns the popped parent key.
EvalKey 
EvalPopParentKey(void)
{
    ThreadLocalStorage* TLS = GetTLS();
    EvalCache* __EvalCache = TLS->EVAL_CACHE;
    EvalCacheParentNode* Node = __EvalCache->HeadParentNode;
    EvalKey Result = Node->Key;

    SLL_STACK_POP(__EvalCache->HeadParentNode);
    SLL_STACK_PUSH(__EvalCache->FreeParentNode, Node);

    return Result;
}

// EvalKeyFromStr (Str8):
// ----------------------------------------------------------------------------
//
// Generates a unique EvalKey from a string with optional parent key context. 
// Retrieves the current parent key from the cache, hashes the parent key 
// combined with the string to find a slot, and searches for an existing bundle. 
// If not found, generates a new key ID, allocates a new cache node, 
// stores the key and string, and links it into both the string hash slot and 
// key hash slot. Returns the newly generated or found key.
EvalKey 
EvalKeyFromStr(Str8 String)
{
    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
    EvalKey ParentKey = {};

    if (__EvalCache->HeadParentNode)
        ParentKey = __EvalCache->HeadParentNode->Key;

    u64 StringHash = Hash(ParentKey.V, String);
    u64 SlotIndex = StringHash % __EvalCache->StringSlotsCount;
    EvalCacheSlot* Slot = &__EvalCache->StringSlots[SlotIndex];
    EvalCacheNode* Node = NULL;

    for (
        EvalCacheNode* N = Slot->Head;
        N;
        N = N->StringNext
    ) {
        if (
            ParentKey == N->Bundle.ParentKey &&
            StrMatch(N->Bundle.String, String, 0) &&
            (
                N->Bundle.Interp.Space.Kind == EVAL_SPACE_KIND_NULL ||
                EvalSpaceGen(N->Bundle.Interp.Space) == N->Bundle.SpaceGeneration
            )
        ) {
            Node = N;
            break;
        }
    }

    if (!Node) {
        ++__EvalCache->KeyIDGeneration;

        EvalKey Key = {
            __EvalCache->KeyIDGeneration
        };
        u64 KeyHash = Hash(Str8Struct(&Key));
        u64 KeySlotIndex = KeyHash % __EvalCache->KeySlotsCount;
        EvalCacheSlot* KeySlot = &__EvalCache->KeySlots[KeySlotIndex];

        Node = ArenaPushArrayZero(
            __EvalCache->MemPool,
            EvalCacheNode,
            1
        );
        SLL_QUEUE_PUSH_EX(Slot->Head, Slot->Tail, Node, StringNext);
        SLL_QUEUE_PUSH_EX(KeySlot->Head, KeySlot->Tail, Node, KeyNext);
        Node->Bundle.Key = Key;
        Node->Bundle.ParentKey = ParentKey;
        Node->Bundle.String = ArenaPushStrCpy(__EvalCache->MemPool, String);
    }

    return Node->Bundle.Key;
}

// EvalKeyFromStr (format):
// ----------------------------------------------------------------------------
//
// Generates a unique EvalKey from a formatted string using printf-style arguments. 
// Uses a scratch arena and va_list to format the string, then delegates 
// to the Str8 version of EvalKeyFromStr to generate the key. 
// Releases the scratch arena and returns the generated key.
EvalKey 
EvalKeyFromStr(char* Fmt, ...)
{
    TempArena Scratch = GetScratch(NULL, 0);
    va_list Args;

    va_start(Args, Fmt);

    Str8 String = ArenaPushStrFmtV(Scratch.MemPool, Fmt, Args);
    EvalKey Result = EvalKeyFromStr(String);

    va_end(Args);
    ReleaseScratch(Scratch);

    return Result;
}

// EvalKeyFromExpr:
// ----------------------------------------------------------------------------
//
// Generates a unique EvalKey from an expression by converting the expression 
// to a string and keying that string. Uses a scratch arena to call 
// EvalStrFromExpr on the expression, then delegates to EvalKeyFromStr to 
// generate the key from the resulting string. 
// Releases the scratch arena and returns the generated key.
EvalKey 
EvalKeyFromExpr(EvalExpr* Expr)
{
    TempArena Scratch = GetScratch(NULL, 0);
    Str8 String = EvalStrFromExpr(Scratch.MemPool, Expr, ""_s8);
    EvalKey Result = EvalKeyFromStr(String);

    ReleaseScratch(Scratch);

    return Result;
}

// EvalCacheBundleFromKey:
// ----------------------------------------------------------------------------
//
// Retrieves the EvalCacheBundle associated with a specific EvalKey. 
// Hashes the key to find the slot, iterates through the key hash chain 
// looking for a matching key, and returns a pointer to the bundle if found. 
// Returns EMPTY_EVAL_CACHE_BUNDLE_VALUE if no matching key exists.
EvalCacheBundle* 
EvalCacheBundleFromKey(EvalKey Key)
{
    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
    u64 KeyHash = Hash(Str8Struct(&Key));
    u64 SlotIndex = KeyHash % __EvalCache->KeySlotsCount;
    EvalCacheSlot* Slot = &__EvalCache->KeySlots[SlotIndex];
    EvalCacheNode* Node = NULL;

    for (
        EvalCacheNode* N = Slot->Head;
        N;
        N = N->KeyNext
    ) {
        if (N->Bundle.Key == Key) {
            Node = N;
            break;
        }
    }

    EvalCacheBundle* Result = EMPTY_EVAL_CACHE_BUNDLE_VALUE;

    if (Node)
        Result = &Node->Bundle;

    return Result;
}

// EvalParseFromBundle:
// ----------------------------------------------------------------------------
//
// Retrieves or generates the parse (AST) result for a cache bundle. 
// If the bundle is valid and the parse flag is not set, sets the flag 
// and calls EvalPushParseFromStr to parse the bundle's string. 
// Copies any messages from the parse into the bundle's message list. 
// Returns the parse result from the bundle.
EvalParse 
EvalParseFromBundle(EvalCacheBundle* Bundle)
{
    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;

    if (
        Bundle != EMPTY_EVAL_CACHE_BUNDLE_VALUE &&
        !(Bundle->Flags & EVAL_CACHE_BUNDLE_FLAG_PARSE)
    ) {
        Bundle->Flags |= EVAL_CACHE_BUNDLE_FLAG_PARSE;
        Bundle->Parse = EvalPushParseFromStr(
            __EvalCache->MemPool,
            Bundle->String
        );

        EvalMsgList MsgsCopy = ListCpy(
            __EvalCache->MemPool, 
            &Bundle->Parse.Msgs
        );

        ListCat(&Bundle->Msgs, &MsgsCopy);
    }

    EvalParse Result = Bundle->Parse;

    return Result;
}

// EvalIRTreeFromBundle:
// ----------------------------------------------------------------------------
//
// Retrieves or generates the IR tree result for a cache bundle. 
// If the bundle is valid and the IR tree flag is not set, sets the 
// flag and retrieves the parent IR tree from the parent key. 
// Calls EvalPushIRTreeAndType to generate the IR tree from the parse result, 
// using the default identifier resolution rule. 
// Copies any messages into the bundle's message list and returns the IR tree result.
EvalIRTreeAndType 
EvalIRTreeFromBundle(EvalCacheBundle* Bundle)
{
    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;

    if (
        Bundle != EMPTY_EVAL_CACHE_BUNDLE_VALUE &&
        !(Bundle->Flags & EVAL_CACHE_BUNDLE_FLAG_IR_TREE)
    ) {
        Bundle->Flags |= EVAL_CACHE_BUNDLE_FLAG_IR_TREE;

        EvalIRTreeAndType Parent = EvalIRTreeFromKey(Bundle->ParentKey);
        EvalParse Parse = EvalParseFromBundle(Bundle);

        Bundle->IRTree = EvalPushIRTreeAndType(
            __EvalCache->MemPool,
            &Parent,
            (EvalIdentifierResolutionRule *) &EVAL_DEFAULT_IDENT_RES_RULE,
            0,
            0,
            Parse.Expr
        );

        EvalMsgList MsgsCopy = ListCpy(
            __EvalCache->MemPool, 
            &Bundle->IRTree.Msgs
        );

        ListCat(&Bundle->Msgs, &MsgsCopy);
    }

    EvalIRTreeAndType Result = Bundle->IRTree;

    return Result;
}

// EvalByteCodeFromBundle:
// ----------------------------------------------------------------------------
//
// Retrieves or generates the bytecode for a cache bundle. If the bundle is 
// valid and the bytecode flag is not set, sets the flag and uses a 
// scratch arena to get the IR tree. Calls EvalOpListFromIRTree to convert 
// the IR to ops, then EvalByteCodeFromOpList to generate the actual bytecode. 
// Stores the bytecode in the bundle and returns it.
Str8 
EvalByteCodeFromBundle(EvalCacheBundle* Bundle)
{
    if (
        Bundle != EMPTY_EVAL_CACHE_BUNDLE_VALUE &&
        !(Bundle->Flags & EVAL_CACHE_BUNDLE_FLAG_BYTECODE)
    ) {
        Bundle->Flags |= EVAL_CACHE_BUNDLE_FLAG_BYTECODE;

        TempArena Scratch = GetScratch(NULL, 0);
        EvalIRTreeAndType IRTree = EvalIRTreeFromBundle(Bundle);
        EvalOpList OpList = EvalOpListFromIRTree(Scratch.MemPool, IRTree.Root);

        Bundle->ByteCode = EvalByteCodeFromOpList(
            GetTLS()->EVAL_CACHE->MemPool, 
            &OpList
        );
        ReleaseScratch(Scratch);
    }

    Str8 Result = Bundle->ByteCode;

    return Result;
}

// EvalInterpFromBundle:
// ----------------------------------------------------------------------------
//
// Retrieves or generates the interpreted result for a cache bundle. 
// If the bundle is valid and the interpret flag is not set, sets the 
// flag and gets the bytecode from the bundle. 
// Calls EvalInterpret to execute the bytecode, and if an error occurs, 
// adds an interpretation error message to the bundle's message list. 
// Stores the interpretation result and space generation, then returns the result.
EvalInterp 
EvalInterpFromBundle(EvalCacheBundle* Bundle)
{
    if (
        Bundle != EMPTY_EVAL_CACHE_BUNDLE_VALUE &&
        !(Bundle->Flags & EVAL_CACHE_BUNDLE_FLAG_INTERPRET)
    ) {
        Bundle->Flags |= EVAL_CACHE_BUNDLE_FLAG_INTERPRET;

        Str8 ByteCode = EvalByteCodeFromBundle(Bundle);
        EvalInterp Interpret = EvalInterpret(ByteCode);

        if (
            Interpret.Code > EVAL_INTERP_CODE_OKAY &&
            Interpret.Code < EVAL_INTERP_CODE_COUNT
        ) {
            EvalMessage(
                GetTLS()->EVAL_CACHE->MemPool,
                &Bundle->Msgs,
                EVAL_MSG_KIND_INTERPRETATION_ERROR,
                Rng((u64) 0, (u64) 0),
                EVAL_INTERP_CODE_DISPLAY_STRINGS[Interpret.Code]
            );
        }

        Bundle->Interp = Interpret;
        Bundle->SpaceGeneration = EvalSpaceGen(Interpret.Space);
    }

    EvalInterp Result = Bundle->Interp;

    return Result;
}

struct ParentResolveTask {
    ParentResolveTask* Next;
    EvalCacheBundle*   Bundle;
};

// EvalFullExprFromKey:
// ----------------------------------------------------------------------------
//
// Reconstructs the full expression string for a key including all parent contexts. 
// Copies the bundle's string as the result, then if there is a parent key, builds 
// a linked list of parent tasks. Iterates through the task chain to get the 
// expression strings for each parent bundle in order, concatenating them to 
// form the full expression. Returns the fully reconstructed expression string.
Str8 
EvalFullExprFromKey(Arena* MemPool, EvalKey Key)
{
    EvalCacheBundle* Bundle = EvalCacheBundleFromKey(Key);
    Str8 Result = ArenaPushStrCpy(MemPool, Bundle->String);

    if (Bundle->ParentKey != EMPTY_EVAL_KEY_VALUE) {
        TempArena Scratch = GetScratch(&MemPool, 1);
        ParentResolveTask StartTask = {
            NULL,
            Bundle
        };
        ParentResolveTask* HeadTask = &StartTask;
        ParentResolveTask* TailTask = HeadTask;

        for (
            ParentResolveTask* Task = HeadTask, *Next = NULL;
            Task;
            (Task = Next, Next = NULL)
        ) {
            if (Task->Bundle->ParentKey != EMPTY_EVAL_KEY_VALUE) {
                ParentResolveTask* T = ArenaPushArrayZero(
                    Scratch.MemPool,
                    ParentResolveTask,
                    1
                );

                SLL_QUEUE_PUSH_FRONT(HeadTask, TailTask, T);
                T->Bundle = EvalCacheBundleFromKey(Task->Bundle->ParentKey);
                Next = T;
            }
        }

        Str8 ParentString = {};

        for (ParentResolveTask* Task = HeadTask; Task; Task = Task->Next) {
            EvalParse Parse = EvalParseFromBundle(Task->Bundle);

            ParentString = EvalStrFromExpr(
                Scratch.MemPool, 
                Parse.Expr, 
                ParentString
            );
        }

        Result = ArenaPushStrCpy(MemPool, ParentString);
        ReleaseScratch(Scratch);
    }

    return Result;
}

EvalEval 
EvalEvalFromBundle(EvalCacheBundle* Bundle)
{
    EvalEval Result = {};

    Result.Key = Bundle->Key;
    Result.ParentKey = Bundle->ParentKey;
    Result.String = Bundle->String;
    Result.Expr = EvalParseFromBundle(Bundle).Expr;
    Result.IRTree = EvalIRTreeFromBundle(Bundle);
    Result.ByteCode = EvalByteCodeFromBundle(Bundle);
    Result.Msgs = Bundle->Msgs;

    EvalInterp Interp = EvalInterpFromBundle(Bundle);

    Result.Code = Interp.Code;
    Result.Space = Interp.Space;
    Result.Value = Interp.Value;

    return Result;
}

EvalEval 
EvalValueEvalFromEval(EvalEval Result)
{
    PROFILE(EvalValueEvalFromEvalTimer) {
        if (Result.IRTree.Mode == EVAL_MODE_OFFSET) {
            EvalTypeKey TypeKey = EvalTypeKeyUnwrap(
                Result.IRTree.TypeKey, 
                EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
            );
            EvalTypeKind TypeKind = EvalTypeKindFromKey(TypeKey);

            if (TypeKind == EVAL_TYPE_KIND_ARRAY) {
                Result.IRTree.Mode = EVAL_MODE_VALUE;
            } else {
                u64 TypeByteSize = EvalByteSizeFromKey(TypeKey);
                r1u64 ValueRange = Rng(
                    Result.Value.U64, 
                    Result.Value.U64 + TypeByteSize
                );

                MemZero(&Result.Value, sizeof(Result.Value));

                if (
                    TypeKey != EMPTY_EVAL_TYPE_KEY_VALUE &&
                    TypeByteSize <= sizeof(EvalValue) &&
                    EvalSpaceRead(Result.Space, &Result.Value, NULL, ValueRange)
                ) {
                    Result.IRTree.Mode = EVAL_MODE_VALUE;

                    switch (TypeKind) {
                        default: {} break;

                        case EVAL_TYPE_KIND_CHAR8:
                        case EVAL_TYPE_KIND_I8: {
                            Result.Value.I64 = (i64) *((i8*) &Result.Value.U64);
                        } break;

                        case EVAL_TYPE_KIND_CHAR16:
                        case EVAL_TYPE_KIND_I16: {
                            Result.Value.I64 = (i64) *((i16*) &Result.Value.U64);
                        } break;

                        case EVAL_TYPE_KIND_CHAR32:
                        case EVAL_TYPE_KIND_I32: {
                            Result.Value.I64 = (i64) *((i32*) &Result.Value.U64);
                        } break;
                    }
                }
            }
        }
    }

    return Result;
}

u64 
EvalIDFromStr(Str8 String)
{
    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
    u64 StringHash = Hash(String);
    u64 HashSlotIndex = StringHash % __EvalCache->StringIDMap->HashSlotsCount;
    EvalStrIDNode* Node = NULL;

    for (
        EvalStrIDNode* N = __EvalCache->StringIDMap->HashSlots[HashSlotIndex].Head;
        N;
        N = N->HashNext
    ) {
        if (StrMatch(N->String, String, 0)) {
            Node = N;
            break;
        }
    }

    if (!Node) {
        ++__EvalCache->StringIDGeneration;

        u64 ID = __EvalCache->StringIDGeneration;
        u64 IDSlotIndex = ID % __EvalCache->StringIDMap->IDSlotsCount;

        Node = ArenaPushArrayZero(
            __EvalCache->MemPool,
            EvalStrIDNode,
            1
        );
        SLL_QUEUE_PUSH_EX(
            __EvalCache->StringIDMap->HashSlots[HashSlotIndex].Head,
            __EvalCache->StringIDMap->HashSlots[HashSlotIndex].Tail,
            Node,
            HashNext
        );
        SLL_QUEUE_PUSH_EX(
            __EvalCache->StringIDMap->IDSlots[IDSlotIndex].Head,
            __EvalCache->StringIDMap->IDSlots[IDSlotIndex].Tail,
            Node,
            IDNext
        );
        Node->ID = ID;
        Node->String = ArenaPushStrCpy(
            __EvalCache->MemPool,
            String
        );
    }

    u64 Result = Node->ID;

    return Result;
}

Str8 
EvalStrFromID(u64 ID)
{
    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
    u64 IDSlotIndex = ID % __EvalCache->StringIDMap->IDSlotsCount;
    EvalStrIDNode* Node = NULL;

    for (
        EvalStrIDNode* N = __EvalCache->StringIDMap->IDSlots[IDSlotIndex].Head;
        N;
        N = N->IDNext
    ) {
        if (N->ID == ID) {
            Node = N;
            break;
        }
    }

    Str8 Result = {};

    if (Node)
        Result = Node->String;

    return Result;
}

EvalKey 
EvalKeyWrap(EvalKey Key, Str8 String)
{
    EvalKey Result = EMPTY_EVAL_KEY_VALUE;

    EvalParentKey(Key) {
        Result = EvalKeyFromStr(String);
    }

    return Result;
}

EvalKey 
EvalKeyWrap(EvalKey Key, char* Fmt, ...)
{
    TempArena Scratch = GetScratch(NULL, 0);
    va_list Args;

    va_start(Args, Fmt);

    Str8 String = ArenaPushStrFmtV(Scratch.MemPool, Fmt, Args);
    EvalKey Result = EvalKeyWrap(Key, String);

    va_end(Args);
    ReleaseScratch(Scratch);

    return Result;
}

r1u64 
EvalRangeFromEval(EvalEval Eval)
{
    u64 BaseOffset = 0;
    u64 Size = KB(16);

    {
        EvalTypeKey TypeKey = EvalTypeKeyUnwrap(
            Eval.IRTree.TypeKey,
            EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
        );
        EvalTypeKind TypeKind = EvalTypeKindFromKey(TypeKey);

        if (Eval.IRTree.Mode == EVAL_MODE_OFFSET)
            Size = EvalByteSizeFromKey(TypeKey);
    }

    r1u64 Result = Rng(BaseOffset, BaseOffset + Size);

    return Result;
}

EvalEvalTypeGroup 
EvalTypeGroupFromKind(EvalTypeKind Kind)
{
    EvalEvalTypeGroup Result = 0;

    switch (Kind) {
        default: {} break;

        case EVAL_TYPE_KIND_NULL: 
        case EVAL_TYPE_KIND_F16:  
        case EVAL_TYPE_KIND_ARRAY:
        case EVAL_TYPE_KIND_STRUCT:     
        case EVAL_TYPE_KIND_ENUM:       
        case EVAL_TYPE_KIND_INCOMPLETE_STRUCT: 
        case EVAL_TYPE_KIND_INCOMPLETE_ENUM: {
            Result = EVAL_EVAL_TYPE_GROUP_OTHER;
        } break;

        case EVAL_TYPE_KIND_UCHAR8: 
        case EVAL_TYPE_KIND_UCHAR16: 
        case EVAL_TYPE_KIND_UCHAR32:
        case EVAL_TYPE_KIND_U8:     
        case EVAL_TYPE_KIND_U16:     
        case EVAL_TYPE_KIND_U32:
        case EVAL_TYPE_KIND_U64:    
        case EVAL_TYPE_KIND_U128:    
        case EVAL_TYPE_KIND_U256:
        case EVAL_TYPE_KIND_U512:
        case EVAL_TYPE_KIND_PTR: {
            Result = EVAL_EVAL_TYPE_GROUP_UNSIGNED;
        } break;

        case EVAL_TYPE_KIND_CHAR8: 
        case EVAL_TYPE_KIND_CHAR16: 
        case EVAL_TYPE_KIND_CHAR32:
        case EVAL_TYPE_KIND_I8:    
        case EVAL_TYPE_KIND_I16:    
        case EVAL_TYPE_KIND_I32:
        case EVAL_TYPE_KIND_I64:   
        case EVAL_TYPE_KIND_I128:   
        case EVAL_TYPE_KIND_I256:
        case EVAL_TYPE_KIND_I512:
        case EVAL_TYPE_KIND_BOOL: {
            Result = EVAL_EVAL_TYPE_GROUP_SIGNED;
        } break;

        case EVAL_TYPE_KIND_F32: {
            Result = EVAL_EVAL_TYPE_GROUP_F32;
        } break;

        case EVAL_TYPE_KIND_F64: {
            Result = EVAL_EVAL_TYPE_GROUP_F64;
        } break;
    }

    return Result;
}

b32 
EvalTypeKindIsInteger(EvalTypeKind Kind)
{
    return (
        Kind >= EVAL_TYPE_KIND_FIRST_INTEGER && 
        Kind <= EVAL_TYPE_KIND_LAST_INTEGER
    );
}

b32 
EvalTypeKindIsSigned(EvalTypeKind Kind)
{
    return (
        Kind >= EVAL_TYPE_KIND_FIRST_SIGNED0 && 
        Kind <= EVAL_TYPE_KIND_LAST_SIGNED0 &&
        Kind >= EVAL_TYPE_KIND_FIRST_SIGNED1 && 
        Kind <= EVAL_TYPE_KIND_LAST_SIGNED1
    );
}

b32 
EvalTypeKindIsPtrOrRef(EvalTypeKind Kind)
{
    return (Kind == EVAL_TYPE_KIND_PTR);
}

EvalTypeKey 
EvalTypeKeyBasic(EvalTypeKind Kind)
{
    EvalTypeKey Result = {
        EVAL_TYPE_KEY_KIND_BASIC
    };

    Result.U64[0] = Kind;

    return Result;
}

u64 
Hash(EvalConstructedTypeParams* Params)
{
    u64 Buffer[] = {
        (u64) Params->Kind,
        (u64) Params->DirectKey.Kind,
        Params->DirectKey.U64[0],
        Params->DirectKey.U64[1],
        Params->DirectKey.U64[2],
        Params->Count,
        Params->Depth
    };
    u64 Result = Hash(Str((u8*) Buffer, sizeof(Buffer)));

    Result = Hash(Result, Params->Name);

    return Result;
}

EvalTypeKey 
__EvalTypeKeyConstruct(EvalConstructedTypeParams* Params)
{
    EvalCache* __EvalCache = GetTLS()->EVAL_CACHE;
    u64 ContentHash = Hash(Params);
    u64 ContentSlotIndex = ContentHash % __EvalCache->ConstructedContentSlotsCount;
    EvalConstructedTypeSlot* ContentSlot = &__EvalCache->ConstructedContentSlots[ContentSlotIndex];
    EvalConstructedTypeNode* Node = NULL;

    for (
        EvalConstructedTypeNode* N = ContentSlot->Head; 
        N; 
        N = N->ContentNext
    ) {
        if (
            N->Params.Kind == Params->Kind &&
            N->Params.Flags == Params->Flags &&
            N->Params.Count == Params->Count &&
            N->Params.Depth == Params->Depth &&
            N->Params.DirectKey == Params->DirectKey &&
            StrMatch(N->Params.Name, Params->Name, 0)
        ) {
            Node = N;
            break;
        }
    }

    EvalTypeKey Result = {};

    if (!Node) {
        EvalTypeKey Key = {
            EVAL_TYPE_KEY_KIND_CONSTRUCTED
        };

        Key.U64[0] = Params->Kind;
        Key.U64[1] = __EvalCache->ConstructedIDGeneration;
        ++__EvalCache->ConstructedIDGeneration;

        u64 KeyHash = Hash(Str8Struct(&Key));
        u64 KeySlotIndex = KeyHash % __EvalCache->ConstructedKeySlotsCount;
        EvalConstructedTypeSlot* KeySlot = &__EvalCache->ConstructedKeySlots[KeySlotIndex];
        EvalConstructedTypeNode* Node = ArenaPushArrayZero(
            __EvalCache->MemPool,
            EvalConstructedTypeNode,
            1
        );

        SLL_QUEUE_PUSH_EX(
            ContentSlot->Head, 
            ContentSlot->Tail, 
            Node, 
            ContentNext
        );
        SLL_QUEUE_PUSH_EX(
            KeySlot->Head,
            KeySlot->Tail,
            Node,
            KeyNext
        );
        Node->Key = Key;
        MemCpy(&Node->Params, Params, sizeof(Node->Params));
        Node->Params.Name = ArenaPushStrCpy(__EvalCache->MemPool, Params->Name);

        if (Node->Params.ExpandRule.ExpandInfoFunc) {
            if (!Node->Params.ExpandRule.ExpandRangeFunc)
                Node->Params.ExpandRule.ExpandRangeFunc = EVAL_TYPE_EXPAND_RANGE_FUNC_NAME(Default);

            if (!Node->Params.ExpandRule.ExpandIDFromNumFunc)
                Node->Params.ExpandRule.ExpandIDFromNumFunc = EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_NAME(Identity);

            if (!Node->Params.ExpandRule.ExpandNumFromIDFunc)
                Node->Params.ExpandRule.ExpandNumFromIDFunc = EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_NAME(Identity);
        }

        if (Params->Args) {
            Node->Params.Args = ArenaPushArrayZero(
                __EvalCache->MemPool,
                EvalExpr*,
                Params->Count
            );

            for (u64 Index = 0; Index < Params->Count; ++Index) {
                Node->Params.Args[Index] = EvalExprCpy(
                    __EvalCache->MemPool, 
                    Params->Args[Index]
                );
            }
        } else {
            switch (Params->Kind) {
                default: {
                    Node->ByteSize = EvalByteSizeFromKey(Node->Params.DirectKey);
                } break;

                case EVAL_TYPE_KIND_ARRAY: {
                    u64 ElementSize = EvalByteSizeFromKey(Node->Params.DirectKey);

                    Node->ByteSize = ElementSize * Node->Params.Count;
                } break;
            }
        }

        Result = Key;
    } else {
        Result = Node->Key;
    }

    return Result;
}

EvalTypeKey 
EvalTypeKeyConstructArray(
    EvalTypeKey ElementTypeKey, 
    u64 Count, 
    EvalTypeFlag Flags
) {
    EvalTypeKey Result = EvalTypeKeyConstruct(
        __Params.Kind = EVAL_TYPE_KIND_ARRAY,
        __Params.DirectKey = ElementTypeKey,
        __Params.Count = Count,
        __Params.Flags = Flags
    );

    return Result;
}

EvalTypeKey 
EvalTypeKeyConstructMetaExpr(EvalTypeKey TypeKey, Str8 Expr)
{
    EvalTypeKey Result = EvalTypeKeyConstruct(
        __Params.Kind = EVAL_TYPE_KIND_META_EXPR,
        __Params.DirectKey = TypeKey,
        __Params.Name = Expr
    );

    return Result;
}

EvalTypeKey 
EvalTypeKeyConstructMetaDisplayName(EvalTypeKey TypeKey, Str8 Name)
{
    EvalTypeKey Result = EvalTypeKeyConstruct(
        __Params.Kind = EVAL_TYPE_KIND_META_DISPLAY_NAME,
        __Params.DirectKey = TypeKey,
        __Params.Name = Name
    );

    return Result;
}

EvalTypeKey 
EvalTypeKeyConstructMetaDescription(EvalTypeKey TypeKey, Str8 Desc)
{
    EvalTypeKey Result = EvalTypeKeyConstruct(
        __Params.Kind = EVAL_TYPE_KIND_META_DESCRIPTION,
        __Params.DirectKey = TypeKey,
        __Params.Name = Desc
    );

    return Result;
}

EvalTypeKind 
EvalTypeKindFromKey(EvalTypeKey Key)
{
    EvalTypeKind Result = EVAL_TYPE_KIND_NULL;

    switch (Key.Kind) {
        default: {} break;

        case EVAL_TYPE_KEY_KIND_BASIC:
        case EVAL_TYPE_KEY_KIND_EXT:
        case EVAL_TYPE_KEY_KIND_CONSTRUCTED: {
            Result = (EvalTypeKind) Key.U64[0];
        } break;
    }

    return Result;
}

u64 
EvalByteSizeFromKey(EvalTypeKey Key)
{
    u64 Result = 0;

    PROFILE(EvalByteSizeFromKeyTimer) {
        switch (Key.Kind) {
            default: {} break;

            case EVAL_TYPE_KEY_KIND_BASIC: {
                EvalTypeKind Kind = (EvalTypeKind) Key.U64[0];

                Result = EVAL_TYPE_KIND_BASIC_BYTE_SIZE_TABLE[Kind];
            } break;

            case EVAL_TYPE_KEY_KIND_EXT: {} break;

            case EVAL_TYPE_KEY_KIND_CONSTRUCTED: {
                EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
                u64 KeyHash = Hash(Str8Struct(&Key));
                u64 KeySlotIndex = KeyHash % __EvalCache->ConstructedKeySlotsCount;
                EvalConstructedTypeSlot* KeySlot = &__EvalCache->ConstructedKeySlots[KeySlotIndex];

                for (
                    EvalConstructedTypeNode* Node = KeySlot->Head;
                    Node;
                    Node = Node->KeyNext
                ) {
                    if (Node->Key == Key) {
                        Result = Node->ByteSize;
                        break;
                    }
                }
            } break;
        }
    }

    return Result;
}

EvalType* 
EvalPushTypeFromKey(Arena* MemPool, EvalTypeKey Key)
{
    EvalType* Result = (EvalType*) &EMPTY_EVAL_TYPE_VALUE;

    PROFILE(EvalPushTypeFromKeyTimer) {
        switch (Key.Kind) {
            default: {} break;

            case EVAL_TYPE_KEY_KIND_BASIC: {
                EvalTypeKind Kind = (EvalTypeKind) Key.U64[0];

                if (
                    Kind >= EVAL_TYPE_KIND_FIRST_BASIC && 
                    Kind <= EVAL_TYPE_KIND_LAST_BASIC
                ) {
                    Result = ArenaPushArrayZero(MemPool, EvalType, 1);
                    Result->Kind = Kind;
                    Result->Name = EVAL_TYPE_KIND_BASIC_STRING_TABLE[Kind];
                    Result->ByteSize = EVAL_TYPE_KIND_BASIC_BYTE_SIZE_TABLE[Kind];
                }
            } break;

            case EVAL_TYPE_KEY_KIND_CONSTRUCTED: {
                EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
                u64 KeyHash = Hash(Str8Struct(&Key));
                u64 KeySlotIndex = KeyHash % __EvalCache->ConstructedKeySlotsCount;
                EvalConstructedTypeSlot* KeySlot = &__EvalCache->ConstructedKeySlots[KeySlotIndex];

                for (
                    EvalConstructedTypeNode* Node = KeySlot->Head;
                    Node;
                    Node = Node->KeyNext
                ) {
                    if (Node->Key == Key) {
                        Result = ArenaPushArrayZero(MemPool, EvalType, 1);
                        Result->Kind = EvalTypeKindFromKey(Node->Key);
                        Result->Flags = Node->Params.Flags;
                        Result->Name = ArenaPushStrCpy(MemPool, Node->Params.Name);
                        Result->DirectTypeKey = Node->Params.DirectKey;
                        Result->Count = Node->Params.Count;
                        Result->Depth = Node->Params.Depth;
                        Result->IRExtFunc = Node->Params.IRExtFunc;
                        Result->AccessFunc = Node->Params.AccessFunc;
                        Result->ExpandRule = Node->Params.ExpandRule;
                        Result->ByteSize = Node->ByteSize;

                        switch (Result->Kind) {
                            default: {} break;

                            case EVAL_TYPE_KIND_LENS: {
                                Result->Args = ArenaPushArrayZero(
                                    MemPool,
                                    EvalExpr*,
                                    Result->Count
                                );
                                MemCpy(
                                    Result->Args, 
                                    Node->Params.Args, 
                                    sizeof(EvalExpr*) * Result->Count
                                );

                                for (u64 Index = 0; Index < Result->Count; ++Index)
                                    Result->Args[Index] = EvalExprCpy(MemPool, Result->Args[Index]);
                            } break;
                        }
                    }
                }
            } break;

            case EVAL_TYPE_KEY_KIND_EXT: {} break;
        }
    }

    return Result;
}

EvalTypeExpandRule* 
EvalExpandRuleFromTypeKey(EvalTypeKey Key)
{
    EvalTypeExpandRule* Result = (EvalTypeExpandRule*) &EVAL_DEFAULT_TYPE_EXPAND_RULES;
    EvalType* _Type = EvalTypeFromKey(
        EvalTypeKeyUnwrap(Key, EVAL_TYPE_UNWRAP_FLAG_META)
    );

    if (_Type->ExpandRule.ExpandInfoFunc)
        Result = &_Type->ExpandRule;

    for (
        EvalType* LensType = _Type;
        LensType->Kind == EVAL_TYPE_KIND_LENS || LensType->Kind == EVAL_TYPE_KIND_SET;
        LensType = EvalTypeFromKey(EvalTypeKeyUnwrap(LensType->DirectTypeKey, EVAL_TYPE_UNWRAP_FLAG_META))
    ) {
        if (LensType->ExpandRule.ExpandInfoFunc) {
            Result = &LensType->ExpandRule;
            break;
        }
    }

    return Result;
}

EvalTypeKey 
EvalTypeKeyDirect(EvalTypeKey Key)
{
    EvalTypeKey Result = {};

    switch (Key.Kind) {
        default: {} break;

        case EVAL_TYPE_KEY_KIND_EXT:
        case EVAL_TYPE_KEY_KIND_CONSTRUCTED: {
            EvalType* _Type = EvalTypeFromKey(Key);

            Result = _Type->DirectTypeKey;
        } break;
    }

    return Result;
}

EvalTypeKey 
EvalTypeKeyPromote(EvalTypeKey Key)
{
    EvalTypeKey Result = Key;
    EvalTypeKind Kind = EvalTypeKindFromKey(Key);

    if (
        Kind == EVAL_TYPE_KIND_BOOL ||
        Kind == EVAL_TYPE_KIND_I8 ||
        Kind == EVAL_TYPE_KIND_I16 ||
        Kind == EVAL_TYPE_KIND_U8 ||
        Kind == EVAL_TYPE_KIND_U16
    ) {
        Result = EvalTypeKeyBasic(EVAL_TYPE_KIND_I32);
    }

    return Result;
}

EvalTypeKey 
EvalTypeKeyUnwrap(EvalTypeKey Key, EvalTypeUnwrapFlag Flags)
{
    EvalTypeKey Result = Key;
    EvalTypeKind Kind = EvalTypeKindFromKey(Result);
    b32 DidPtr = FALSE;

    for (;;) {
        b32 IsDone = FALSE;

        switch (Kind) {
            default: {
                IsDone = TRUE;
            } break;

            case EVAL_TYPE_KIND_LENS: {
                IsDone = !(Flags & EVAL_TYPE_UNWRAP_FLAG_LENSES);
            } break;

            case EVAL_TYPE_KIND_META_DISPLAY_NAME:
            case EVAL_TYPE_KIND_META_DESCRIPTION:
            case EVAL_TYPE_KIND_META_EXPR: {
                IsDone = !(Flags & EVAL_TYPE_UNWRAP_FLAG_META);
            } break;

            case EVAL_TYPE_KIND_ENUM: {
                IsDone = !(Flags & EVAL_TYPE_UNWRAP_FLAG_ENUMS);
            } break;

            case EVAL_TYPE_KIND_ARRAY: {
                IsDone = TRUE;
            } break;
        }

        if (IsDone)
            break;

        Result = EvalTypeKeyDirect(Result);
        Kind = EvalTypeKindFromKey(Result);
    }

    return Result;
}

void 
EvalTypeLHSFromStr(
    Arena* MemPool, 
    EvalTypeKey Key, 
    OUT Str8List* List, 
    u32 Prec, 
    b32 ShouldSkipReturn
) {
    Str8 Keyword = {};
    EvalTypeKind Kind = EvalTypeKindFromKey(Key);

    switch (Kind) {
        default: {
            EvalType* _Type = EvalTypeFromKey(Key);

            ListPushFmt(MemPool, List, "%S ", PRINT_STR(_Type->Name));
        } break;

        case EVAL_TYPE_KIND_STRUCT:
        case EVAL_TYPE_KIND_ENUM: {
            EvalType* _Type = EvalTypeFromKey(Key);

            ListPushFmt(MemPool, List, "%S ", PRINT_STR(_Type->Name));
        } break;

        case EVAL_TYPE_KIND_INCOMPLETE_STRUCT:
        case EVAL_TYPE_KIND_INCOMPLETE_ENUM: {
            if (Kind == EVAL_TYPE_KIND_INCOMPLETE_STRUCT)
                Keyword = Str8Lit("struct");
            else if (Kind == EVAL_TYPE_KIND_INCOMPLETE_ENUM)
                Keyword = Str8Lit("enum");

            EvalType* _Type = EvalTypeFromKey(Key);

            ListPush(MemPool, List, Keyword);
            ListPush(MemPool, List, Str8Lit(" "));
            ListPushFmt(MemPool, List, "%S ", PRINT_STR(_Type->Name));
        } break;

        case EVAL_TYPE_KIND_ARRAY: {
            EvalTypeKey Direct = EvalTypeKeyDirect(Key);

            EvalTypeLHSFromStr(MemPool, Direct, List, 2, ShouldSkipReturn);

            if (Prec == 1)
            ListPush(MemPool, List, "("_s8);
        } break;

        case EVAL_TYPE_KIND_LENS: {
            EvalType* _Type = EvalTypeFromKey(Key);

            ListPushFmt(MemPool, List, "%S(", PRINT_STR(_Type->Name));

            EvalTypeKey Direct = EvalTypeKeyDirect(Key);
            Str8 DirectString = EvalTypeStrFromKey(MemPool, Direct);

            ListPush(MemPool, List, DirectString);

            for (u64 Index = 0; Index < _Type->Count; ++Index) {
                Str8 String = EvalStrFromExpr(MemPool, _Type->Args[Index], ""_s8);

                ListPush(MemPool, List, ", "_s8);
                ListPush(MemPool, List, String);
            }

            ListPush(MemPool, List, ")"_s8);
        } break;

        case EVAL_TYPE_KIND_META_EXPR:
        case EVAL_TYPE_KIND_META_DISPLAY_NAME:
        case EVAL_TYPE_KIND_META_DESCRIPTION: {
            EvalTypeKey Direct = EvalTypeKeyDirect(Key);

            EvalTypeLHSFromStr(MemPool, Direct, List, Prec, ShouldSkipReturn);
        } break;
    }
}

void 
EvalTypeRHSFromStr(
    Arena* MemPool, 
    EvalTypeKey Key, 
    OUT Str8List* List, 
    u32 Prec
) {
    EvalTypeKind Kind = EvalTypeKindFromKey(Key);

    switch (Kind) {
        default: {} break;

        case EVAL_TYPE_KIND_ARRAY: {
            EvalType* _Type = EvalTypeFromKey(Key);

            if (Prec == 1)
                ListPush(MemPool, List, ")"_s8);

            Str8 CountString = ArenaPushStrFmt(MemPool, "%llu", _Type->Count);

            ListPush(MemPool, List, "["_s8);
            ListPush(MemPool, List, CountString);
            ListPush(MemPool, List, "]"_s8);

            EvalTypeKey Direct = EvalTypeKeyDirect(Key);

            EvalTypeRHSFromStr(MemPool, Direct, List, 2);
        } break;

        case EVAL_TYPE_KIND_META_EXPR: {
            EvalTypeKey Direct = EvalTypeKeyDirect(Key);

            EvalTypeRHSFromStr(MemPool, Direct, List, Prec);
        } break;
    }
}

Str8 
EvalTypeStrFromKey(Arena* MemPool, EvalTypeKey Key)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    Str8List List = {};

    EvalTypeLHSFromStr(Scratch.MemPool, Key, &List, 0, FALSE);
    EvalTypeRHSFromStr(Scratch.MemPool, Key, &List, 0);

    Str8 Result = StrListJoin(MemPool, &List, NULL);

    Result = StrSkipChopWhitespace(Result);
    ReleaseScratch(Scratch);

    return Result;
}

EvalTypeKey 
EvalDefaultExpansionTypeFromKey(EvalTypeKey Key)
{
    EvalTypeKey Result = {};
    b32 HitSinglePtr = FALSE;

    for (
        EvalTypeKey K = Key;
        K != EMPTY_EVAL_TYPE_KEY_VALUE;
        K = EvalTypeKeyDirect(K)
    ) {
        b32 IsDone = TRUE;
        EvalTypeKind Kind = EvalTypeKindFromKey(K);

        if (Kind == EVAL_TYPE_KIND_LENS) {
            IsDone = FALSE;
        } else if (
            Kind >= EVAL_TYPE_KIND_FIRST_META && 
            Kind <= EVAL_TYPE_KIND_LAST_META
        ) {
            IsDone = FALSE;
        } else if (
            Kind == EVAL_TYPE_KIND_STRUCT ||
            Kind == EVAL_TYPE_KIND_SET
        ) {
            Result = K;
        } else if (Kind == EVAL_TYPE_KIND_ENUM) {
            Result = K;
        }

        if (IsDone)
            break;
    }

    return Result;
}

EvalType* 
EvalTypeFromKey(EvalTypeKey Key)
{
    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
    EvalType* Result = (EvalType*) &EMPTY_EVAL_TYPE_VALUE;
    u64 KeyHash = Hash(Str8Struct(&Key));
    u64 SlotIndex = KeyHash % __EvalCache->TypeCacheSlotsCount;
    EvalTypeCacheNode* Node = NULL;

    for (
        EvalTypeCacheNode* N = __EvalCache->TypeCacheSlots[SlotIndex].Head;
        N;
        N = N->Next
    ) {
        if (Key == N->Key) {
            Node = N;
            break;
        }
    }

    if (!Node) {
        Node = ArenaPushArrayZero(__EvalCache->MemPool, EvalTypeCacheNode, 1);
        Node->Key = Key;
        Node->Type = EvalPushTypeFromKey(__EvalCache->MemPool, Key);
        SLL_QUEUE_PUSH(
            __EvalCache->TypeCacheSlots[SlotIndex].Head,
            __EvalCache->TypeCacheSlots[SlotIndex].Tail,
            Node
        );
    }

    Result = Node->Type;

    return Result;
}

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Default) 
{
    EvalTypeExpandInfo Result = {};

    {
        EvalTypeKey ExpandTypeKey = EvalDefaultExpansionTypeFromKey(
            Eval.IRTree.TypeKey
        );
        b32 DidExpansion = FALSE;

        if (!DidExpansion) {
            EvalTypeKind ArrayTypeKind = EvalTypeKindFromKey(ExpandTypeKey);

            if (ArrayTypeKind == EVAL_TYPE_KIND_ARRAY) {
                EvalType* ArrayType = EvalTypeFromKey(ExpandTypeKey);

                Result.ExprCount = ArrayType->Count;
                DidExpansion = TRUE;
            }
        }
    }

    return Result;
}

EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Default)
{
    TempArena Scratch = GetScratch(&MemPool, 1);

    {
        EvalTypeKey ExpandTypeKey = EvalDefaultExpansionTypeFromKey(
            Eval.IRTree.TypeKey
        );
        EvalTypeKind ExpandTypeKind = EvalTypeKindFromKey(ExpandTypeKey);

        if (
            ExpandTypeKind == EVAL_TYPE_KIND_ARRAY ||
            ExpandTypeKind == EVAL_TYPE_KIND_SET
        ) {
            u64 ReadRangeLength = Length(IndexRange);

            for (u64 Index = 0; Index < ReadRangeLength; ++Index)
                Evals[Index] = EvalEvalWrapFmt(Eval, "$[%llu]", IndexRange.Min + Index);
        }
    }

    ReleaseScratch(Scratch);
}

EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_DEF(Identity)
{
    return Num;
}

EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_DEF(Identity)
{
    return ID;
}

struct EvalFolderAccel {
    Str8      FolderPath;
    Str8Array Folders;
    Str8Array Files;
};

EVAL_TYPE_EXPAND_INFO_FUNC_DEF(Folder)
{
    EvalTypeExpandInfo Result = {};

    {
        TempArena Scratch = GetScratch(&MemPool, 1);
        u64 LHSStringID = Eval.Value.U64;
        Str8 FolderPath = EvalStrFromID(LHSStringID);
        Str8 LocalFilter = Filter;

        {
            u64 FolderPositionInFilter = StrFindSubStr(
                Filter,
                FolderPath,
                0,
                (
                    STR_MATCH_ALL_CASES |
                    STR_MATCH_SKIP_SLASH
                )
            );

            if (FolderPositionInFilter < Filter.Size) {
                LocalFilter = StrSkip(LocalFilter, FolderPositionInFilter + FolderPath.Size);
                LocalFilter = StrSkipChopSlashes(LocalFilter);
            } else {
                MemZero(&LocalFilter, sizeof(LocalFilter));
            }
        }

        Str8List FolderPaths = {};
        Str8List FilePaths = {};

        {
            FileIter* Iter = SystemFileIterBegin(Scratch.MemPool, FolderPath, 0);

            for (
                FileInfo Info = {};
                SystemFileIterNext(Scratch.MemPool, Iter, &Info);
            ) {
                FMRangeList Matches = FuzzyFind(
                    Scratch.MemPool,
                    LocalFilter,
                    Info.Name
                );

                if (Matches.Count == Matches.StrPartCount) {
                    if (Info.Properties.Kind & SYS_FILE_IS_DIR) {
                        ListPush(
                            Scratch.MemPool,
                            &FolderPaths,
                            ArenaPushStrCpy(MemPool, Info.Name)
                        );
                    } else {
                        ListPush(
                            Scratch.MemPool,
                            &FilePaths,
                            ArenaPushStrCpy(MemPool, Info.Name)
                        );
                    }
                }
            }

            SystemFileIterEnd(Iter);
        }

        EvalFolderAccel* Accel = ArenaPushArrayZero(
            MemPool,
            EvalFolderAccel,
            1
        );

        Accel->FolderPath = ArenaPushStrCpy(MemPool, FolderPath);
        Accel->Folders = StrArrayFromList(MemPool, &FolderPaths);
        Accel->Files = StrArrayFromList(MemPool, &FilePaths);
        Result.UserData = Accel;
        Result.ExprCount = Accel->Folders.Count + Accel->Files.Count;
        ReleaseScratch(Scratch);
    }

    return Result;
}

EVAL_TYPE_EXPAND_RANGE_FUNC_DEF(Folder)
{
    EvalFolderAccel* Accel = (EvalFolderAccel*) UserData;
    u64 ResultIndex = 0;

    for (
        u64 Index = IndexRange.Min;
        Index < IndexRange.Max;
        ++Index, ++ResultIndex
    ) {
        TempArena Scratch = GetScratch(&MemPool, 1);
        Str8 PathExpressionString = {};

        if (Index >= 0 && Index < Accel->Folders.Count) {
            Str8 FolderName = Accel->Folders.Data[Index];
            Str8 FolderPath = ArenaPushStrFmt(
                Scratch.MemPool,
                "%S%s%S",
                PRINT_STR(Accel->FolderPath),
                Accel->FolderPath.Size ? "/" : "",
                PRINT_STR(FolderName)
            );

            PathExpressionString = ArenaPushStrFmt(
                MemPool,
                "folder:\"%S/\"",
                PRINT_STR(EscapedFromRawStr8(Scratch.MemPool, FolderPath))
            );
        } else if (
            Index >= Accel->Folders.Count && 
            Index < Accel->Folders.Count + Accel->Files.Count
        ) {
            Str8 FileName = Accel->Files.Data[Index - Accel->Folders.Count];
            Str8 FilePath = ArenaPushStrFmt(
                Scratch.MemPool,
                "%S%s%S",
                PRINT_STR(Accel->FolderPath),
                Accel->FolderPath.Size ? "/" : "",
                PRINT_STR(FileName)
            );

            PathExpressionString = ArenaPushStrFmt(
                MemPool,
                "file:\"%S\"",
                PRINT_STR(EscapedFromRawStr8(Scratch.MemPool, FilePath))
            );
        }

        Evals[ResultIndex] = EvalEvalFromStr(PathExpressionString);
        ReleaseScratch(Scratch);
    }
}

EVAL_TYPE_EXPAND_ID_FROM_NUM_FUNC_DEF(Folder)
{
    u64 Result = 0;
    EvalFolderAccel* Accel = (EvalFolderAccel*) UserData;
    Str8 Name = {};

    if (Num && Num <= Accel->Folders.Count) {
        Name = Accel->Folders.Data[Num - 1];
    } else if (
        Num > Accel->Folders.Count && 
        Num <= Accel->Folders.Count + Accel->Files.Count
    ) {
        Name = Accel->Files.Data[Num - Accel->Folders.Count - 1];
    }

    Result = Hash(Name);

    return Result;
}

EVAL_TYPE_EXPAND_NUM_FROM_ID_FUNC_DEF(Folder)
{
    u64 Result = 0;
    EvalFolderAccel* Accel = (EvalFolderAccel*) UserData;

    for (
        u64 Index = 0;
        Index < Accel->Folders.Count + Accel->Files.Count;
        ++Index
    ) {
        Str8 Name = {};

        if (Index >= 0 && Index < Accel->Folders.Count) {
            Name = Accel->Folders.Data[Index];
        } else if (
            Index >= Accel->Folders.Count &&
            Index < Accel->Folders.Count + Accel->Files.Count
        ) {
            Name = Accel->Files.Data[Index - Accel->Folders.Count];
        }

        u64 NameHash = Hash(5381, Name);

        if (NameHash == ID) {
            Result = Index + 1;
            break;
        }
    }

    return Result;
}

void 
EvalInterpContextSelect(EvalInterpretContext* Ctx)
{
    GetTLS()->EVAL_INTERP_CTX = Ctx;
}

u64 
EvalSpaceGen(EvalSpace Space)
{
    u64 Result = 0;

    switch (Space.Kind) {
        default: {
            if (GetTLS()->EVAL_BASE_CTX->SpaceGenFunc)
                Result = GetTLS()->EVAL_BASE_CTX->SpaceGenFunc(Space);
        } break;
    }

    return Result;
}

b32 
EvalSpaceRead(EvalSpace Space, OUT void* Data, OUT EvalSpaceRangeInfo* RangeInfo, r1u64 Range)
{
    b32 Result = FALSE;

    PROFILE(EvalSpaceReadTimer) {
        switch (Space.Kind) {
            default: {
                if (GetTLS()->EVAL_BASE_CTX->SpaceReadFunc) {
                    Result = GetTLS()->EVAL_BASE_CTX->SpaceReadFunc(
                        Space, 
                        Data, 
                        NULL,
                        Range
                    );
                }
            } break;
        }
    }

    return Result;
}

b32 
EvalSpaceWrite(EvalSpace Space, IN void* Data, r1u64 Range)
{
    b32 Result = FALSE;

    PROFILE(EvalSpaceWriteTimer) {
        if (GetTLS()->EVAL_BASE_CTX->SpaceWriteFunc) {
            switch(Space.Kind) {
                default: {
                    Result = GetTLS()->EVAL_BASE_CTX->SpaceWriteFunc(
                        Space,
                        Data,
                        Range
                    );
                } break;
            }
        }
    }

    return Result;
}

EvalInterp 
EvalInterpret(Str8 ByteCode)
{
    EvalInterp Result = {};
    TempArena Scratch = GetScratch(NULL, 0);
    u64 StackCapacity = 128;
    EvalValue* Stack = ArenaPushArray(Scratch.MemPool, EvalValue, StackCapacity);
    u64 StackCount = 0;
    EvalSpace SelectedSpace = {};
    EvalInterpretContext* const __EvalInterpCtx = GetTLS()->EVAL_INTERP_CTX;

    if (ByteCode.Size)
        SelectedSpace = __EvalInterpCtx->PrimarySpace;

    u8* ByteCodePtr = ByteCode.Str;
    u8* ByteCodeEnd = ByteCode.Str + ByteCode.Size;

    for ( ; ByteCodePtr < ByteCodeEnd; ) {
        EvalEvalOp Op = (EvalEvalOp) *ByteCodePtr;
        u16 CtrlBits = 0;

        if (Op < EVAL_EVAL_OP_COUNT) {
            CtrlBits = EVAL_EVAL_OP_CTRL_BITS_TABLE[Op];
        } else {
            switch (Op) {
                default: {
                    LogError("got bad eval op [%hu]", Op);
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                } break;

                case EVAL_IR_EXT_KIND_SET_SPACE: {
                    CtrlBits = EVAL_EVAL_CTRL_BITS(32, 0, 0);
                } break;
            }
        }

        ++ByteCodePtr;

        EvalValue IMM = {};

        {
            u32 DecodeSize = EVAL_DECODE_FROM_CTRL_BITS(CtrlBits);
            u8* ByteCodeNextPtr = ByteCodePtr + DecodeSize;

            if (ByteCodeNextPtr > ByteCodeEnd) {
                Result.Code = EVAL_INTERP_CODE_BAD_OP;

                goto DONE;
            }

            MemCpy(&IMM, ByteCodePtr, DecodeSize);
            ByteCodePtr = ByteCodeNextPtr;
        }

        EvalEvalTypeGroup TypeGroup = (EvalEvalTypeGroup) IMM.U512.U8[0];
        u64 OpArithmeticSize = (u64) IMM.U512.U8[1];
        EvalValue* StackValues = NULL;

        {
            u32 PopCount = EVAL_POP_FROM_CTRL_BITS(CtrlBits);

            if (PopCount > StackCount) {
                Result.Code = EVAL_INTERP_CODE_BAD_OP;

                goto DONE;
            }

            if (PopCount <= StackCount) {
                StackCount -= PopCount;
                StackValues = Stack + StackCount;
            }
        }

        EvalValue NValue = {};

        switch (Op) {
            default: {
                LogInfo("got Op %hu", Op);
            } break;

            case EVAL_IR_EXT_KIND_SET_SPACE: {
                MemCpy(&SelectedSpace, &IMM, sizeof(SelectedSpace));
            } break;

            case EVAL_EVAL_OP_CONST_U8:
            case EVAL_EVAL_OP_CONST_U16:
            case EVAL_EVAL_OP_CONST_U32:
            case EVAL_EVAL_OP_CONST_U64:
            case EVAL_EVAL_OP_CONST_U128: {
                NValue = IMM;
            } break;

            case EVAL_EVAL_OP_CONST_STR: {
                MemCpy(&NValue, ByteCodePtr, IMM.U64);
                ByteCodePtr += IMM.U64;
            } break;

            case EVAL_EVAL_OP_NEG: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32)
                    NValue.F32 = -StackValues[0].F32;
                else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64)
                    NValue.F64 = -StackValues[0].F64;
                else
                    NValue.U64 = (~StackValues[0].U64) + 1;
            } break;

            case EVAL_EVAL_OP_ADD: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32)
                    NValue.F32 = StackValues[0].F32 + StackValues[1].F32;
                else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64)
                    NValue.F64 = StackValues[0].F64 + StackValues[1].F64;
                else
                    NValue.U64 = StackValues[0].U64 + StackValues[1].U64;
            } break;

            case EVAL_EVAL_OP_SUB: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32)
                    NValue.F32 = StackValues[0].F32 - StackValues[1].F32;
                else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64)
                    NValue.F64 = StackValues[0].F64 - StackValues[1].F64;
                else
                    NValue.U64 = StackValues[0].U64 - StackValues[1].U64;
            } break;

            case EVAL_EVAL_OP_MUL: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32)
                    NValue.F32 = StackValues[0].F32 * StackValues[1].F32;
                else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64)
                    NValue.F64 = StackValues[0].F64 * StackValues[1].F64;
                else
                    NValue.U64 = StackValues[0].U64 * StackValues[1].U64;
            } break;

            case EVAL_EVAL_OP_DIV: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32) {
                    if (StackValues[1].F32 != 0.0f) {
                        NValue.F32 = StackValues[0].F32 / StackValues[1].F32;
                    } else {
                        Result.Code = EVAL_INTERP_CODE_BAD_OP;

                        goto DONE;
                    }
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64) {
                    if (StackValues[1].F64 != 0.0) {
                        NValue.F64 = StackValues[0].F64 / StackValues[1].F64;
                    } else {
                        Result.Code = EVAL_INTERP_CODE_BAD_OP;

                        goto DONE;
                    }
                } else if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    if (StackValues[1].U64) {
                        NValue.U64 = StackValues[0].U64 / StackValues[1].U64;
                    } else {
                        Result.Code = EVAL_INTERP_CODE_BAD_OP;

                        goto DONE;
                    }
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_MOD: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED || 
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    if (StackValues[1].U64)
                        NValue.U64 = StackValues[0].U64 % StackValues[1].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_LEFT_SHIFT: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED) {
                    switch (OpArithmeticSize) {
                        default: {} break;

                        case 1: {
                            NValue.U8 = StackValues[0].U8 << StackValues[1].U8;
                        } break;

                        case 2: {
                            NValue.U16 = StackValues[0].U16 << StackValues[1].U16;
                        } break;

                        case 4: {
                            NValue.U32 = StackValues[0].U32 << StackValues[1].U32;
                        } break;

                        case 8: {
                            NValue.U64 = StackValues[0].U64 << StackValues[1].U64;
                        } break;
                    }
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED) {
                    switch (OpArithmeticSize) {
                        default: {} break;

                        case 1: {
                            NValue.I8 = StackValues[0].I8 << StackValues[1].I8;
                        } break;

                        case 2: {
                            NValue.I16 = StackValues[0].I16 << StackValues[1].I16;
                        } break;

                        case 4: {
                            NValue.I32 = StackValues[0].I32 << StackValues[1].I32;
                        } break;

                        case 8: {
                            NValue.I64 = StackValues[0].I64 << StackValues[1].I64;
                        } break;
                    }
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_RIGHT_SHIFT: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED) {
                    switch (OpArithmeticSize) {
                        default: {} break;

                        case 1: {
                            NValue.U8 = StackValues[0].U8 >> StackValues[1].U8;
                        } break;

                        case 2: {
                            NValue.U16 = StackValues[0].U16 >> StackValues[1].U16;
                        } break;

                        case 4: {
                            NValue.U32 = StackValues[0].U32 >> StackValues[1].U32;
                        } break;

                        case 8: {
                            NValue.U64 = StackValues[0].U64 >> StackValues[1].U64;
                        } break;
                    }
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED) {
                    switch (OpArithmeticSize) {
                        default: {} break;

                        case 1: {
                            NValue.I8 = StackValues[0].I8 >> StackValues[1].I8;
                        } break;

                        case 2: {
                            NValue.I16 = StackValues[0].I16 >> StackValues[1].I16;
                        } break;

                        case 4: {
                            NValue.I32 = StackValues[0].I32 >> StackValues[1].I32;
                        } break;

                        case 8: {
                            NValue.I64 = StackValues[0].I64 >> StackValues[1].I64;
                        } break;
                    }
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_BIT_AND: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    NValue.U64 = StackValues[0].U64 & StackValues[1].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_BIT_OR: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    NValue.U64 = StackValues[0].U64 | StackValues[1].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_BIT_XOR: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    NValue.U64 = StackValues[0].U64 ^ StackValues[1].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_BIT_NOT: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    NValue.U64 = ~StackValues[0].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_LOG_AND: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    NValue.U64 = StackValues[0].U64 && StackValues[1].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_LOG_OR: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    NValue.U64 = StackValues[0].U64 || StackValues[1].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_LOG_NOT: {
                if (
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED ||
                    TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED
                ) {
                    NValue.U64 = !StackValues[0].U64;
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_EQ_EQ: {
                b32 Res = MemCmp(
                    StackValues[0].U512.U64,
                    StackValues[1].U512.U64,
                    sizeof(StackValues[0].U512.U64)
                );

                NValue.U64 = !!Res;
            } break;

            case EVAL_EVAL_OP_NOT_EQ: {
                b32 Res = MemCmp(
                    StackValues[0].U512.U64,
                    StackValues[1].U512.U64,
                    sizeof(StackValues[0].U512.U64)
                );

                NValue.U64 = !Res;
            } break;

            case EVAL_EVAL_OP_LESS_EQ: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32) {
                    NValue.U64 = (StackValues[0].F32 <= StackValues[1].F32);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64) {
                    NValue.U64 = (StackValues[0].F64 <= StackValues[1].F64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED) {
                    NValue.U64 = (StackValues[0].U64 <= StackValues[1].U64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED) {
                    NValue.U64 = (StackValues[0].I64 <= StackValues[1].I64);
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_GREATER_EQ: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32) {
                    NValue.U64 = (StackValues[0].F32 >= StackValues[1].F32);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64) {
                    NValue.U64 = (StackValues[0].F64 >= StackValues[1].F64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED) {
                    NValue.U64 = (StackValues[0].U64 >= StackValues[1].U64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED) {
                    NValue.U64 = (StackValues[0].I64 >= StackValues[1].I64);
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_LESS: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32) {
                    NValue.U64 = (StackValues[0].F32 < StackValues[1].F32);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64) {
                    NValue.U64 = (StackValues[0].F64 < StackValues[1].F64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED) {
                    NValue.U64 = (StackValues[0].U64 < StackValues[1].U64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED) {
                    NValue.U64 = (StackValues[0].I64 < StackValues[1].I64);
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_GREATER: {
                if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F32) {
                    NValue.U64 = (StackValues[0].F32 > StackValues[1].F32);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_F64) {
                    NValue.U64 = (StackValues[0].F64 > StackValues[1].F64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_UNSIGNED) {
                    NValue.U64 = (StackValues[0].U64 > StackValues[1].U64);
                } else if (TypeGroup == EVAL_EVAL_TYPE_GROUP_SIGNED) {
                    NValue.U64 = (StackValues[0].I64 > StackValues[1].I64);
                } else {
                    Result.Code = EVAL_INTERP_CODE_BAD_OP;

                    goto DONE;
                }
            } break;

            case EVAL_EVAL_OP_TRUNC: {
                if (IMM.U64) {
                    u64 Mask = 0;

                    if (IMM.U64 < 64)
                        Mask = U64_MAX >> (64 - IMM.U64);

                    NValue.U64 = StackValues[0].U64 & Mask;
                }
            } break;

            case EVAL_EVAL_OP_TRUNC_SIGNED: {
                if (IMM.U64) {
                    u64 Mask = 0;

                    if (IMM.U64 < 64)
                        Mask = U64_MAX >> (64 - IMM.U64);

                    u64 High = 0;

                    if (StackValues[0].U64 & (1 << (IMM.U64 - 1)))
                        High = ~Mask;

                    NValue.U64 = High | (StackValues[0].U64 & Mask);
                }
            } break;

            case EVAL_EVAL_OP_CONVERT: {
                u32 In = IMM.U64 & U8_MAX;
                u32 Out = (IMM.U64 >> 8) & U8_MAX;

                if (In != Out) {
                    switch (In + Out * EVAL_EVAL_TYPE_GROUP_COUNT) {
                        case EVAL_EVAL_TYPE_GROUP_F32 + EVAL_EVAL_TYPE_GROUP_UNSIGNED * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.U64 = (u64) StackValues[0].F32;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_F64 + EVAL_EVAL_TYPE_GROUP_UNSIGNED * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.U64 = (u64) StackValues[0].F64;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_F32 + EVAL_EVAL_TYPE_GROUP_SIGNED * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.I64 = (i64) StackValues[0].F32;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_F64 + EVAL_EVAL_TYPE_GROUP_SIGNED * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.I64 = (i64) StackValues[0].F64;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_UNSIGNED + EVAL_EVAL_TYPE_GROUP_F32 * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.F32 = (f32) StackValues[0].U64;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_SIGNED + EVAL_EVAL_TYPE_GROUP_F32 * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.F32 = (f32) StackValues[0].I64;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_F64 + EVAL_EVAL_TYPE_GROUP_F32 * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.F32 = (f32) StackValues[0].F64;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_UNSIGNED + EVAL_EVAL_TYPE_GROUP_F64 * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.F64 = (f64) StackValues[0].U64;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_SIGNED + EVAL_EVAL_TYPE_GROUP_F64 * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.F64 = (f64) StackValues[0].I64;
                        } break;

                        case EVAL_EVAL_TYPE_GROUP_F32 + EVAL_EVAL_TYPE_GROUP_F64 * EVAL_EVAL_TYPE_GROUP_COUNT: {
                            NValue.F64 = (f64) StackValues[0].F32;
                        } break;
                    }
                }
            } break;

            case EVAL_EVAL_OP_VALUE_READ: {
                u64 BytesToRead = IMM.U64;
                u64 Offset = StackValues[0].U64;

                if (Offset + BytesToRead <= sizeof(EvalValue)) {
                    EvalValue SrcValue = StackValues[1];

                    MemCpy(
                        &NValue.U512.U64[0], 
                        (u8*) (&SrcValue.U512.U64[0]) + Offset, 
                        BytesToRead
                    );
                }
            } break;
        }

        {
            u64 PushCount = EVAL_PUSH_FROM_CTRL_BITS(CtrlBits);

            if (PushCount == 1) {
                if (StackCount < StackCapacity) {
                    Stack[StackCount++] = NValue;
                } else {
                    Result.Code = EVAL_INTERP_CODE_INSUFFICIENT_STACK_SPACE;

                    goto DONE;
                }
            }
        }
    }

DONE:;

    if (StackCount)
        Result.Value = Stack[0];

    Result.Space = SelectedSpace;
    ReleaseScratch(Scratch);

    return Result;
}

void 
ListPushOp(
    Arena* MemPool, 
    EvalOpList* List, 
    EvalEvalOp OpCode, 
    EvalValue Value
) {
    u16 CtrlBits = EVAL_EVAL_OP_CTRL_BITS_TABLE[OpCode];
    u32 Size = EVAL_DECODE_FROM_CTRL_BITS(CtrlBits);
    EvalOp* Node = ArenaPushArray(MemPool, EvalOp, 1);

    Node->OpCode = OpCode;
    Node->Value = Value;
    SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
    ++List->OpCount;
    List->EncodedSize += Size + 1;
}

void 
ListPushConstU(Arena* MemPool, EvalOpList* List, u64 X)
{
    EvalValue Value = {};

    Value.U64 = X;

    if (X <= U8_MAX)
        ListPushOp(MemPool, List, EVAL_EVAL_OP_CONST_U8, Value);
    else if (X <= U16_MAX)
        ListPushOp(MemPool, List, EVAL_EVAL_OP_CONST_U16, Value);
    else if (X <= U32_MAX)
        ListPushOp(MemPool, List, EVAL_EVAL_OP_CONST_U32, Value);
    else
        ListPushOp(MemPool, List, EVAL_EVAL_OP_CONST_U64, Value);
}

void 
ListPushByteCode(Arena* MemPool, EvalOpList* List, Str8 ByteCode)
{
    EvalOp* Node = ArenaPushArray(MemPool, EvalOp, 1);

    Node->OpCode = EVAL_IR_EXT_KIND_BYTECODE;
    Node->String = ByteCode;
    SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
    ++List->OpCount;
    List->EncodedSize += ByteCode.Size;
}

void 
ListPushSetSpace(Arena* MemPool, EvalOpList* List, EvalSpace Space)
{
    EvalOp* Node = ArenaPushArray(MemPool, EvalOp, 1);

    Node->OpCode = EVAL_IR_EXT_KIND_SET_SPACE;

    STATIC_ASSERT(sizeof(EvalSpace) <= sizeof(EvalValue), EvalSpaceSizeCheck);

    MemCpy(&Node->Value, &Space, sizeof(Space));
    SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
    ++List->OpCount;
    List->EncodedSize += sizeof(Space) + 1;
}

void 
ListCat(EvalOpList* Dst, EvalOpList* Src)
{
    if (Src->Head && Dst->Head) {
        Dst->Tail->Next = Src->Head;
        Dst->Tail = Src->Tail;
        Dst->OpCount += Src->OpCount;
        Dst->EncodedSize += Src->EncodedSize;
    } else if (!Dst->Head)
        MemCpy(Dst, Src, sizeof(*Dst));

    MemZero(Src, sizeof(*Src));
}

EvalIRNode* 
EvalPushIRNode(Arena* MemPool, EvalEvalOp Op)
{
    EvalIRNode* Node = ArenaPushArrayZero(MemPool, EvalIRNode, 1);

    Node->Head = EMPTY_EVAL_IR_NODE_VALUE;
    Node->Tail = EMPTY_EVAL_IR_NODE_VALUE;
    Node->Next = EMPTY_EVAL_IR_NODE_VALUE;
    Node->Op = Op;

    return Node;
}

void 
EvalIRNodePushChild(EvalIRNode* Parent, EvalIRNode* Child)
{
    if (
        Parent != EMPTY_EVAL_IR_NODE_VALUE && 
        Child != EMPTY_EVAL_IR_NODE_VALUE
    ) {
        SLL_QUEUE_PUSH_EX_EMPTY(
            EMPTY_EVAL_IR_NODE_VALUE, 
            Parent->Head, 
            Parent->Tail, 
            Child, 
            Next
        );
    }
}

EvalIRNode* 
EvalIRTreeConstU(Arena* MemPool, u64 Value)
{
    EvalEvalOp Op = EVAL_EVAL_OP_CONST_U64;

    if (Value < 0x100)
        Op = EVAL_EVAL_OP_CONST_U8;
    else if (Value < 0x10000)
        Op = EVAL_EVAL_OP_CONST_U16;
    else if (Value < 0x100000000)
        Op = EVAL_EVAL_OP_CONST_U32;

    EvalIRNode* Node = EvalPushIRNode(MemPool, Op);

    Node->Value.U64 = Value;

    return Node;
}

EvalIRNode* 
EvalIRTreeUnaryOp(
    Arena* MemPool, 
    EvalEvalOp Op, 
    EvalEvalTypeGroup Group, 
    EvalIRNode* Child
) {
    EvalIRNode* Node = EvalPushIRNode(MemPool, Op);

    Node->Value.U64 = Group;
    EvalIRNodePushChild(Node, Child);

    return Node;
}

EvalIRNode* 
EvalIRTreeBinaryOp(
    Arena* MemPool, 
    EvalEvalOp Op, 
    EvalEvalTypeGroup Group, 
    u64 OperandSize, 
    EvalIRNode* LHS, 
    EvalIRNode* RHS
) {
    EvalIRNode* Node = EvalPushIRNode(MemPool, Op);

    Node->Value.U512.U8[0] = (u8) Group;
    Node->Value.U512.U8[1] = (u8) OperandSize;
    EvalIRNodePushChild(Node, LHS);
    EvalIRNodePushChild(Node, RHS);

    return Node;
}

EvalIRNode* 
EvalIRTreeBinaryOpU(
    Arena* MemPool, 
    EvalEvalOp Op, 
    u64 OperandSize, 
    EvalIRNode* LHS, 
    EvalIRNode* RHS
) {
    return EvalIRTreeBinaryOp(
        MemPool,
        Op,
        EVAL_EVAL_TYPE_GROUP_UNSIGNED,
        OperandSize,
        LHS,
        RHS
    );
}

EvalIRNode* 
EvalIRTreeByteCode(Arena* MemPool, Str8 ByteCode)
{
    EvalIRNode* Node = EvalPushIRNode(MemPool, EVAL_IR_EXT_KIND_BYTECODE);

    Node->String = ByteCode;

    return Node;
}

EvalIRNode* 
EvalIRTreeStrLit(Arena* MemPool, Str8 String)
{
    EvalIRNode* Node = EvalPushIRNode(MemPool, EVAL_EVAL_OP_CONST_STR);

    Node->String = String;

    return Node;
}

EvalIRNode* 
EvalIRTreeSetSpace(Arena* MemPool, EvalSpace Space, EvalIRNode* Child)
{
    EvalIRNode* Node = EvalPushIRNode(MemPool, EVAL_IR_EXT_KIND_SET_SPACE);

    STATIC_ASSERT(sizeof(EvalSpace) <= sizeof(EvalValue), EvalSpaceSizeCheck);

    MemCpy(&Node->Value, &Space, sizeof(Space));
    EvalIRNodePushChild(Node, Child);

    return Node;
}

EvalIRNode* 
EvalIRTreeMemReadType(
    Arena* MemPool, 
    EvalIRNode* Child, 
    EvalTypeKey TypeKey
) {
    EvalIRNode* Result = EMPTY_EVAL_IR_NODE_VALUE;
    EvalIRNode* ReadNode = EvalPushIRNode(MemPool, EVAL_EVAL_OP_MEM_READ);
    u64 ByteSize = EvalByteSizeFromKey(TypeKey);

    ByteSize = MIN(ByteSize, 64);
    ReadNode->Value.U64 = ByteSize;
    EvalIRNodePushChild(ReadNode, Child);
    Result = ReadNode;

    return Result;
}

EvalIRNode* 
EvalIRTreeConvertLo(
    Arena* MemPool, 
    EvalIRNode* Child, 
    OUT EvalEvalTypeGroup To, 
    IN EvalEvalTypeGroup From
) {
    EvalIRNode* Node = EvalPushIRNode(MemPool, EVAL_EVAL_OP_CONVERT);

    Node->Value.U64 = From | (To << 8);
    EvalIRNodePushChild(Node, Child);

    return Node;
}

EvalIRNode* 
EvalIRTreeTrunc(Arena* MemPool, EvalIRNode* Child, EvalTypeKey TypeKey)
{
    EvalIRNode* Result = Child;

    u64 ByteSize = EvalByteSizeFromKey(TypeKey);

    if (ByteSize < 64) {
        EvalEvalOp Op = EVAL_EVAL_OP_TRUNC;
        EvalTypeKind Kind = EvalTypeKindFromKey(TypeKey);

        if (EvalTypeKindIsSigned(Kind))
            Op = EVAL_EVAL_OP_TRUNC_SIGNED;

        u64 BitSize = ByteSize << 3;

        Result = EvalPushIRNode(MemPool, Op);
        Result->Value.U64 = BitSize;
        EvalIRNodePushChild(Result, Child);
    }

    return Result;
}

EvalIRNode* 
EvalIRTreeConvertHi(
    Arena* MemPool, 
    EvalIRNode* Child, 
    OUT EvalTypeKey To, 
    IN EvalTypeKey From
) {
    EvalIRNode* Result = Child;
    EvalTypeKind FromKind = EvalTypeKindFromKey(From);
    EvalTypeKind ToKind = EvalTypeKindFromKey(To);
    u8 FromGroup = EvalTypeGroupFromKind(FromKind);
    u8 ToGroup = EvalTypeGroupFromKind(ToKind);
    u32 ConversionRule = EvalEvalConvKindFromTypeGroups(FromGroup, ToGroup);

    if (ConversionRule == EVAL_EVAL_CONV_KIND_LEGAL)
        Result = EvalIRTreeConvertLo(MemPool, Result, ToGroup, FromGroup);

    u64 FromByteSize = EvalByteSizeFromKey(From);
    u64 ToByteSize = EvalByteSizeFromKey(To);

    if (ToByteSize < FromByteSize && EvalTypeKindIsInteger(ToKind))
        Result = EvalIRTreeTrunc(MemPool, Result, To);

    return Result;
}

EvalIRNode* 
EvalIRTreeResolveToValue(
    Arena* MemPool, 
    EvalMode FromMode, 
    EvalIRNode* Tree, 
    EvalTypeKey TypeKey
) {
    EvalBaseContext* const __EvalBaseCtx = GetTLS()->EVAL_BASE_CTX;
    EvalIRNode* Result = Tree;

    if (FromMode == EVAL_MODE_OFFSET)
        Result = EvalIRTreeMemReadType(MemPool, Tree, TypeKey);

    return Result;
}

EVAL_TYPE_ACCESS_FUNC_DEF(Default)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    EvalIRTreeAndType Result = {
        EMPTY_EVAL_IR_NODE_VALUE
    };

    switch (Expr->Kind) {
        default: {} break;

        case EVAL_EXPR_KIND_MEMBER_ACCESS: {
            EvalExpr* LHSExpr = Expr->Head;
            EvalIRTreeAndType LHS = *LHSIRTree;

            ListCat(&Result.Msgs, &LHS.Msgs);
            EvalMessage(
                MemPool,
                &Result.Msgs,
                EVAL_MSG_KIND_MALFORMED_INPUT,
                LHSExpr->Range,
                "Cannot perform member access on this type."_s8
            );
        } break;

        case EVAL_EXPR_KIND_ARRAY_INDEX: {
            EvalExpr* LHSExpr = Expr->Head;
            EvalExpr* RHSExpr = LHSExpr->Next;
            EvalIRTreeAndType LHS = *LHSIRTree;
            EvalIRTreeAndType RHS = EvalPushIRTreeAndType(
                MemPool,
                Overridden,
                (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                FALSE,
                TRUE,
                RHSExpr
            );

            ListCat(&Result.Msgs, &RHS.Msgs);

            EvalTypeKey LHSResType = EvalTypeKeyUnwrap(
                LHS.TypeKey, 
                EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
            );
            EvalTypeKey RHSResType = EvalTypeKeyUnwrap(
                RHS.TypeKey,
                EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
            );
            EvalTypeKind LHSResTypeKind = EvalTypeKindFromKey(LHSResType);
            EvalTypeKind RHSResTypeKind = EvalTypeKindFromKey(RHSResType);
            EvalTypeKey DirectType = EvalTypeKeyUnwrap(
                LHSResType,
                EVAL_TYPE_UNWRAP_FLAG_ALL & ~EVAL_TYPE_UNWRAP_FLAG_ENUMS
            );
            u64 DirectTypeSize = EvalByteSizeFromKey(DirectType);

            if (!RHS.Root->Op) {
                break;
            } else if (
                LHSResTypeKind != EVAL_TYPE_KIND_PTR &&
                LHSResTypeKind != EVAL_TYPE_KIND_ARRAY
            ) {
                EvalMessage(
                    MemPool,
                    &Result.Msgs,
                    EVAL_MSG_KIND_MALFORMED_INPUT,
                    LHSExpr->Range,
                    "Cannot index into this type."_s8
                );
                break;
            } else if (!EvalTypeKindIsInteger(RHSResTypeKind)) {
                EvalMessage(
                    MemPool,
                    &Result.Msgs,
                    EVAL_MSG_KIND_MALFORMED_INPUT,
                    RHSExpr->Range,
                    "Cannot index with this type."_s8
                );
                break;
            } else if (
                LHSResTypeKind == EVAL_TYPE_KIND_PTR && 
                !DirectTypeSize
            ) {
                EvalMessage(
                    MemPool,
                    &Result.Msgs,
                    EVAL_MSG_KIND_MALFORMED_INPUT,
                    RHSExpr->Range,
                    "Cannot index into pointers of zero-sized types"_s8
                );
                break;
            } else if (
                LHSResTypeKind == EVAL_TYPE_KIND_ARRAY &&
                !DirectTypeSize
            ) {
                EvalMessage(
                    MemPool,
                    &Result.Msgs,
                    EVAL_MSG_KIND_MALFORMED_INPUT,
                    RHSExpr->Range,
                    "Cannot index into arrays of zero-sized types."_s8
                );
                break;
            }

            EvalIRNode* NewTree = EMPTY_EVAL_IR_NODE_VALUE;
            EvalMode Mode = LHS.Mode;

            {
                if (
                    LHS.Mode == EVAL_MODE_VALUE && 
                    LHSResTypeKind == EVAL_TYPE_KIND_ARRAY
                ) {
                    EvalIRNode* OffsetTree = EvalIRTreeResolveToValue(
                        MemPool,
                        RHS.Mode,
                        RHS.Root,
                        RHSResType
                    );

                    if (DirectTypeSize > 1) {
                        EvalIRNode* ConstTree = EvalIRTreeConstU(
                            MemPool,
                            DirectTypeSize
                        );

                        OffsetTree = EvalIRTreeBinaryOpU(
                            MemPool,
                            EVAL_EVAL_OP_MUL,
                            8,
                            OffsetTree,
                            ConstTree
                        );
                    }

                    NewTree = EvalPushIRNode(MemPool, EVAL_EVAL_OP_VALUE_READ);
                    NewTree->Value.U64 = DirectTypeSize;
                    EvalIRNodePushChild(NewTree, OffsetTree);
                    EvalIRNodePushChild(NewTree, LHS.Root);
                } else {
                    EvalIRNode* OffsetTree = EvalIRTreeResolveToValue(
                        MemPool,
                        RHS.Mode,
                        RHS.Root,
                        RHSResType
                    );

                    if (DirectTypeSize > 1) {
                        EvalIRNode* ConstTree = EvalIRTreeConstU(
                            MemPool,
                            DirectTypeSize
                        );

                        OffsetTree = EvalIRTreeBinaryOpU(
                            MemPool,
                            EVAL_EVAL_OP_MUL,
                            8,
                            OffsetTree,
                            ConstTree
                        );
                    }

                    EvalIRNode* BaseTree = LHS.Root;

                    if (
                        EvalTypeKindIsPtrOrRef(LHSResTypeKind) && 
                        LHS.Mode != EVAL_MODE_VALUE
                    ) {
                        BaseTree = EvalIRTreeResolveToValue(
                            MemPool, 
                            LHS.Mode, 
                            BaseTree, 
                            LHSResType
                        );
                    }

                    NewTree = EvalIRTreeBinaryOpU(
                        MemPool,
                        EVAL_EVAL_OP_ADD,
                        8,
                        OffsetTree,
                        BaseTree
                    );

                    if (Mode != EVAL_MODE_NULL)
                        Mode = EVAL_MODE_OFFSET;
                }
            }

            Result.Root = NewTree;
            Result.TypeKey = DirectType;
            Result.Mode = Mode;
        } break;
    }

    ReleaseScratch(Scratch);

    return Result;
}

EvalIRTreeAndType 
EvalPushIRTreeAndType(
    Arena* MemPool, 
    EvalIRTreeAndType* RootParent, 
    EvalIdentifierResolutionRule* IdentResRule, 
    b32 DisallowAutoHooks, 
    b32 DisallowChainedFastPaths, 
    EvalExpr* RootExpr
) {
    EvalIRTreeAndType Result = {
        EMPTY_EVAL_IR_NODE_VALUE
    };

    PROFILE(EvalPushIRTreeAndTypeTimer) {
        EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
        TempArena Scratch = GetScratch(&MemPool, 1);
        EvalTypeKeyList InheritedLenses = {};

        struct Task {
            Task*              Next;
            EvalExpr*          Expr;
            EvalIRTreeAndType* Overridden;
        };

        Task StartTask = {
            NULL,
            RootExpr,
            NULL
        };
        Task* HeadTask = &StartTask;
        Task* TailTask = HeadTask;

        for (Task* T = HeadTask; T; T = T->Next) {
            EvalExpr* Expr = T->Expr;
            EvalIRTreeAndType* Parent = T->Overridden
                ? T->Overridden
                : RootParent;

            if (Expr->Kind == EVAL_EXPR_KIND_REF)
                Expr = Expr->Ref;

            EvalExprKind Kind = Expr->Kind;

            switch (Kind) {
                default: {} break;

                case EVAL_EXPR_KIND_MEMBER_ACCESS:
                case EVAL_EXPR_KIND_ARRAY_INDEX: {
                    EvalExpr* LHS = Expr->Head;

                    for (
                        b32 AutoHooksEnabled = FALSE;
                        AutoHooksEnabled < 1;
                        ++AutoHooksEnabled
                    ) {
                        EvalIRTreeAndType LHSIRTreeTry = EvalPushIRTreeAndType(
                            MemPool,
                            Parent,
                            (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                            !AutoHooksEnabled,
                            TRUE,
                            LHS
                        );

                        for (
                            EvalIRTreeAndType* LHSIRTreeTryChain = &LHSIRTreeTry;
                            LHSIRTreeTryChain;
                            LHSIRTreeTryChain = LHSIRTreeTryChain->Prev
                        ) {
                            EvalType* LHSType = EvalTypeFromKey(LHSIRTreeTryChain->TypeKey);
                            EvalTypeAccessFuncType* LHSAccessFunc = LHSType->AccessFunc;

                            for (
                                EvalType* LensType = LHSType;
                                (
                                    LensType->Kind == EVAL_TYPE_KIND_LENS ||
                                    LensType->Kind == EVAL_TYPE_KIND_SET ||
                                    (
                                        LensType->Kind >= EVAL_TYPE_KIND_FIRST_META &&
                                        LensType->Kind <= EVAL_TYPE_KIND_LAST_META
                                    )
                                );
                                LensType = EvalTypeFromKey(LensType->DirectTypeKey)
                            ) {
                                if (LensType->AccessFunc) {
                                    LHSAccessFunc = LensType->AccessFunc;
                                    break;
                                }
                            }

                            if (!LHSAccessFunc)
                                LHSAccessFunc = EVAL_TYPE_ACCESS_FUNC_NAME(Default);

                            EvalIRTreeAndType NewResultMaybe = LHSAccessFunc(
                                MemPool,
                                Parent,
                                Expr,
                                LHSIRTreeTryChain
                            );

                            if (NewResultMaybe.Root != EMPTY_EVAL_IR_NODE_VALUE) {
                                EvalTypeKey Key = LHSIRTreeTryChain->TypeKey;
                                EvalTypeKind Kind = EvalTypeKindFromKey(Key);

                                for ( ; Kind == EVAL_TYPE_KIND_LENS; ) {
                                    EvalType* LensType = EvalTypeFromKey(Key);

                                    if (
                                        (
                                            LensType->Flags & EVAL_TYPE_FLAG_INHERITED_BY_MEMBERS && 
                                            Expr->Kind == EVAL_EXPR_KIND_MEMBER_ACCESS
                                        ) ||
                                        (
                                            LensType->Flags & EVAL_TYPE_FLAG_INHERITED_BY_ELEMENTS &&
                                            Expr->Kind == EVAL_EXPR_KIND_ARRAY_INDEX
                                        )
                                    ) {
                                        ListPushFront(
                                            Scratch.MemPool, 
                                            &InheritedLenses, 
                                            Key
                                        );
                                    }

                                    Key = EvalTypeKeyDirect(Key);
                                    Kind = EvalTypeKindFromKey(Key);
                                }

                                ListCat(
                                    &Result.Msgs,
                                    &LHSIRTreeTryChain->Msgs
                                );
                            }

                            if (NewResultMaybe.Root != EMPTY_EVAL_IR_NODE_VALUE) {
                                Result = NewResultMaybe;
                                break;
                            } else if (NewResultMaybe.Msgs.Count && !Result.Msgs.Count) {
                                Result = NewResultMaybe;
                            }
                        }

                        if (Result.Root != EMPTY_EVAL_IR_NODE_VALUE)
                            break;
                    }

                    if (Result.Root == EMPTY_EVAL_IR_NODE_VALUE) {
                        b32 IsIdentChain = TRUE;

                        for (
                            EvalExpr* LHSExpr = LHS; 
                            LHSExpr != EMPTY_EVAL_EXPR_VALUE; 
                            LHSExpr = LHSExpr->Next
                        ) {
                            if (
                                LHSExpr->Kind != EVAL_EXPR_KIND_MEMBER_ACCESS && 
                                LHSExpr->Kind != EVAL_EXPR_KIND_LEAF_IDENT
                            ) {
                                IsIdentChain = FALSE;
                                break;
                            }
                        }

                        if (IsIdentChain) {
                            Str8List Parts = {};

                            ListPushFront(Scratch.MemPool, &Parts, LHS->Next->String);

                            for (
                                EvalExpr* LHSExpr = LHS;
                                LHSExpr != EMPTY_EVAL_EXPR_VALUE;
                                LHSExpr = LHSExpr->Head
                            ) {
                                if (LHSExpr->Kind == EVAL_EXPR_KIND_LEAF_IDENT)
                                    ListPushFront(Scratch.MemPool, &Parts, LHSExpr->String);
                                else if (LHSExpr->Kind == EVAL_EXPR_KIND_MEMBER_ACCESS)
                                    ListPushFront(Scratch.MemPool, &Parts, LHSExpr->Head->Next->String);
                            }

                            Str8JoinPart Join = {};

                            Join.Sep = "."_s8;

                            Str8 FullQualifiedName = StrListJoin(
                                Scratch.MemPool,
                                &Parts,
                                &Join
                            );
                            EvalExpr* LeafExprName = EvalPushExpr(
                                Scratch.MemPool,
                                EVAL_EXPR_KIND_LEAF_IDENT,
                                Rng((u64) 0, (u64) 0)
                            );

                            LeafExprName->String = FullQualifiedName;

                            EvalIRTreeAndType NewResultMaybe = EvalPushIRTreeAndType(
                                MemPool,
                                Parent,
                                (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                                DisallowAutoHooks,
                                DisallowAutoHooks,
                                LeafExprName
                            );

                            if (NewResultMaybe.Root != EMPTY_EVAL_IR_NODE_VALUE)
                                Result = NewResultMaybe;
                        }
                    }
                } break;

                case EVAL_EXPR_KIND_CAST: {
                    EvalExpr *CastTypeExpr = Expr->Head;
                    EvalExpr *CastedExpr = CastTypeExpr->Next;
                    EvalIRTreeAndType CastIRTree = EvalPushIRTreeAndType(
                        MemPool, 
                        Parent, 
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE, 
                        DisallowAutoHooks, 
                        TRUE, 
                        CastTypeExpr
                    );

                    ListCat(&Result.Msgs, &CastIRTree.Msgs);

                    EvalTypeKey CastType = CastIRTree.TypeKey;
                    EvalTypeKind CastTypeKind = EvalTypeKindFromKey(CastType);
                    EvalTypeKey CastTypeUnwrapped = EvalTypeKeyUnwrap(
                        CastIRTree.TypeKey, 
                        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
                    );
                    EvalTypeKind CastTypeUnwrappedKind = EvalTypeKindFromKey(
                        CastTypeUnwrapped
                    );
                    u64 CastTypeByteSize = EvalByteSizeFromKey(CastType);
                    EvalIRTreeAndType CastedTree = EvalPushIRTreeAndType(
                        MemPool, 
                        Parent, 
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE, 
                        DisallowAutoHooks, 
                        TRUE, 
                        CastedExpr
                    );

                    ListCat(&Result.Msgs, &CastedTree.Msgs);

                    EvalTypeKey CastedType = EvalTypeKeyUnwrap(
                        CastedTree.TypeKey, 
                        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
                    );
                    EvalTypeKind CastedTypeKind = EvalTypeKindFromKey(CastedType);
                    EvalTypeKey CastedTypeUnwrapped = EvalTypeKeyUnwrap(
                        CastedType,
                        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE | EVAL_TYPE_UNWRAP_FLAG_BITFIELDS
                    );
                    EvalTypeKind CastedTypeUnwrappedKind = EvalTypeKindFromKey(
                        CastedTypeUnwrapped
                    );
                    u64 CastedTypeByteSize = EvalByteSizeFromKey(CastedType);
                    u8 FromGroup  = EvalTypeGroupFromKind(CastedTypeUnwrappedKind);
                    u8 ToGroup = EvalTypeGroupFromKind(CastTypeUnwrappedKind);
                    EvalEvalConversionKind ConversionRule = EvalEvalConvKindFromTypeGroups(
                        FromGroup, 
                        ToGroup
                    );

                    if (!CastedTree.Root->Op) {
                        break;
                    } else if (CastTypeKind == EVAL_TYPE_KIND_NULL) {
                        break;
                    } else if (
                        ConversionRule != EVAL_EVAL_CONV_KIND_NOOP &&
                        ConversionRule != EVAL_EVAL_CONV_KIND_LEGAL 
                    ) {
                        Str8 Text = "Unknown cast conversion rule."_s8;

                        EvalMessage(
                            MemPool, 
                            &Result.Msgs, 
                            EVAL_MSG_KIND_MALFORMED_INPUT, 
                            Expr->Range, 
                            Text
                        );
                        break;
                    }

                    {
                        EvalIRNode *FromTree = EvalIRTreeResolveToValue(
                            MemPool, 
                            CastedTree.Mode, 
                            CastedTree.Root, 
                            CastedType
                        );
                        EvalIRNode *NewTree = FromTree;

                        if (ConversionRule == EVAL_EVAL_CONV_KIND_LEGAL) {
                            NewTree = EvalIRTreeConvertLo(
                                MemPool, 
                                FromTree, 
                                ToGroup, 
                                FromGroup
                            );
                        }

                        if (
                            CastTypeByteSize < CastedTypeByteSize && 
                            EvalTypeKindIsInteger(CastTypeUnwrappedKind)
                        ) {
                            NewTree = EvalIRTreeTrunc(
                                MemPool, 
                                FromTree, 
                                CastType
                            );
                        }

                        Result.Root = NewTree;
                        Result.TypeKey = CastType;
                        Result.Mode = EVAL_MODE_VALUE;
                    }
                } break;

                case EVAL_EXPR_KIND_POS: {
                    Result = EvalPushIRTreeAndType(
                        MemPool,
                        Parent,
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                        DisallowAutoHooks,
                        TRUE,
                        Expr->Head
                    );
                } break;

                case EVAL_EXPR_KIND_NEG:
                case EVAL_EXPR_KIND_BIT_NOT: {
                    EvalExpr* RHSExpr = Expr->Head;
                    EvalIRTreeAndType RHSTree = EvalPushIRTreeAndType(
                        MemPool,
                        Parent,
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                        DisallowAutoHooks,
                        TRUE,
                        RHSExpr
                    );

                    ListCat(&Result.Msgs, &RHSTree.Msgs);

                    EvalTypeKey RHSType = EvalTypeKeyUnwrap(
                        RHSTree.TypeKey,
                        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
                    );
                    EvalTypeKind RHSTypeKind = EvalTypeKindFromKey(RHSType);
                    EvalEvalTypeGroup RHSTypeGroup = EvalTypeGroupFromKind(RHSTypeKind);
                    EvalTypeKey RHSTypePromoted = EvalTypeKeyPromote(RHSType);
                    EvalEvalOp Op = EvalOpCodeFromExprKind(Kind);

                    if (!RHSTree.Root->Op) {
                        break;
                    } else if (
                        !EvalEvalOpTypeGroupAreCompatible(
                            Op, 
                            RHSTypeGroup
                        )
                    ) {
                        EvalMessage(
                            MemPool,
                            &Result.Msgs,
                            EVAL_MSG_KIND_MALFORMED_INPUT,
                            Expr->Range,
                            "cannot use this operator on this type."_s8
                        );
                        break;
                    }

                    {
                        EvalIRNode* InTree = EvalIRTreeResolveToValue(
                            MemPool,
                            RHSTree.Mode,
                            RHSTree.Root,
                            RHSType
                        );

                        InTree = EvalIRTreeConvertHi(
                            MemPool,
                            InTree,
                            RHSTypePromoted,
                            RHSType
                        );

                        EvalIRNode* NewTree = EvalIRTreeUnaryOp(
                            MemPool,
                            Op,
                            RHSTypeGroup,
                            InTree
                        );

                        Result.Root = NewTree;
                        Result.TypeKey = RHSTypePromoted;
                        Result.Mode = EVAL_MODE_VALUE;
                    }
                } break;

                case EVAL_EXPR_KIND_LOG_NOT: {
                    EvalExpr* RHSExpr = Expr->Head;
                    EvalIRTreeAndType RHSTree = EvalPushIRTreeAndType(
                        MemPool,
                        Parent,
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                        DisallowAutoHooks,
                        TRUE,
                        RHSExpr
                    );

                    ListCat(&Result.Msgs, &RHSTree.Msgs);

                    EvalTypeKey RHSType = EvalTypeKeyUnwrap(
                        RHSTree.TypeKey,
                        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
                    );
                    EvalTypeKind RHSTypeKind = EvalTypeKindFromKey(RHSType);
                    EvalEvalTypeGroup RHSTypeGroup = EvalTypeGroupFromKind(RHSTypeKind);
                    EvalTypeKey RHSTypePromoted = EvalTypeKeyBasic(EVAL_TYPE_KIND_BOOL);
                    EvalEvalOp Op = EvalOpCodeFromExprKind(Kind);

                    if (!RHSTree.Root->Op) {
                        break;
                    } else if (
                        !EvalEvalOpTypeGroupAreCompatible(
                            Op, 
                            RHSTypeGroup
                        )
                    ) {
                        EvalMessage(
                            MemPool,
                            &Result.Msgs,
                            EVAL_MSG_KIND_MALFORMED_INPUT,
                            Expr->Range,
                            "cannot use this operator on this type"_s8
                        );
                        break;
                    }

                    {
                        EvalIRNode* InTree = EvalIRTreeResolveToValue(
                            MemPool,
                            RHSTree.Mode,
                            RHSTree.Root,
                            RHSType
                        );

                        InTree = EvalIRTreeConvertHi(
                            MemPool, 
                            InTree, 
                            RHSTypePromoted, 
                            RHSType
                        );

                        EvalIRNode* NewTree = EvalIRTreeUnaryOp(
                            MemPool,
                            Op,
                            RHSTypeGroup,
                            InTree
                        );

                        Result.Root = NewTree;
                        Result.TypeKey = RHSTypePromoted;
                        Result.Mode = EVAL_MODE_VALUE;
                    }
                } break;

                case EVAL_EXPR_KIND_MUL:
                case EVAL_EXPR_KIND_DIV:
                case EVAL_EXPR_KIND_MOD:
                case EVAL_EXPR_KIND_ADD:
                case EVAL_EXPR_KIND_SUB:
                case EVAL_EXPR_KIND_L_SHIFT:
                case EVAL_EXPR_KIND_R_SHIFT:
                case EVAL_EXPR_KIND_LESS:
                case EVAL_EXPR_KIND_LESS_EQ:
                case EVAL_EXPR_KIND_GREATER:
                case EVAL_EXPR_KIND_GREATER_EQ:
                case EVAL_EXPR_KIND_EQ_EQ:
                case EVAL_EXPR_KIND_NOT_EQ:
                case EVAL_EXPR_KIND_BIT_AND:
                case EVAL_EXPR_KIND_BIT_XOR:
                case EVAL_EXPR_KIND_LOG_AND:
                case EVAL_EXPR_KIND_LOG_OR: {
                    EvalEvalOp Op = EvalOpCodeFromExprKind(Kind);
                    b32 IsComparison = EvalExprKindIsCmp(Kind);
                    EvalExpr* LHSExpr = Expr->Head;
                    EvalExpr* RHSExpr= LHSExpr->Next;
                    EvalIRTreeAndType LHSTree = EvalPushIRTreeAndType(
                        MemPool,
                        Parent,
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                        DisallowAutoHooks,
                        TRUE,
                        LHSExpr
                    );
                    EvalIRTreeAndType RHSTree = EvalPushIRTreeAndType(
                        MemPool,
                        Parent,
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                        DisallowAutoHooks,
                        TRUE,
                        RHSExpr
                    );

                    ListCat(&Result.Msgs, &LHSTree.Msgs);
                    ListCat(&Result.Msgs, &RHSTree.Msgs);

                    EvalTypeKey LHSType = EvalTypeKeyUnwrap(
                        LHSTree.TypeKey,
                        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
                    );
                    EvalTypeKey RHSType = EvalTypeKeyUnwrap(
                        RHSTree.TypeKey,
                        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
                    );
                    EvalTypeKind LHSTypeKind = EvalTypeKindFromKey(LHSType);
                    EvalTypeKind RHSTypeKind = EvalTypeKindFromKey(RHSType);
                    u64 LHSTypeSize = EvalByteSizeFromKey(LHSType);
                    u64 RHSTypeSize = EvalByteSizeFromKey(RHSType);
                    u64 OpOperandSize = MAX(LHSTypeSize, RHSTypeSize);
                    b32 LHSIsPtr = (LHSTypeKind == EVAL_TYPE_KIND_PTR);
                    b32 LHSIsDecay = (LHSTypeKind == EVAL_TYPE_KIND_ARRAY && LHSTree.Mode == EVAL_MODE_OFFSET);
                    b32 LHSIsPtrLike = (LHSIsPtr || LHSIsDecay);
                    b32 RHSIsPtr = (RHSTypeKind == EVAL_TYPE_KIND_PTR);
                    b32 RHSIsDecay = (RHSTypeKind == EVAL_TYPE_KIND_ARRAY && RHSTree.Mode == EVAL_MODE_OFFSET);
                    b32 RHSIsPtrLike = (RHSIsPtr || RHSIsDecay);
                    EvalEvalTypeGroup LHSTypeGroup = EvalTypeGroupFromKind(LHSTypeKind);
                    EvalEvalTypeGroup RHSTypeGroup = EvalTypeGroupFromKind(RHSTypeKind);

                    if (!LHSTree.Root->Op || !RHSTree.Root->Op)
                        break;

                #define EVAL_ARITH_PATH_NORMAL        0
                #define EVAL_ARITH_PATH_PTR_ADD       1
                #define EVAL_ARITH_PATH_PTR_SUB       2
                #define EVAL_ARITH_PATH_PTR_ARRAY_CMP 3
                #define EVAL_ARITH_PATH_TYPE_CMP      4

                    b32 PtrArithmeticMulRHSPtr = FALSE;
                    u32 ArithPath = EVAL_ARITH_PATH_NORMAL;

                    if (Kind == EVAL_EXPR_KIND_ADD) {
                        if (LHSIsPtrLike && EvalTypeKindIsInteger(RHSTypeKind)) {
                            ArithPath = EVAL_ARITH_PATH_PTR_ADD;
                        }

                        if (LHSIsPtrLike && EvalTypeKindIsInteger(LHSTypeKind)) {
                            ArithPath = EVAL_ARITH_PATH_PTR_ADD;
                            PtrArithmeticMulRHSPtr = TRUE;
                        }
                    } else if (Kind == EVAL_EXPR_KIND_SUB) {
                        if (LHSIsPtrLike && EvalTypeKindIsInteger(RHSTypeKind)) {
                            ArithPath = EVAL_ARITH_PATH_PTR_ADD;
                        }

                        if (LHSIsPtrLike && RHSIsPtrLike) {
                            EvalTypeKey LHSTypeDirect = EvalTypeKeyUnwrap(
                                LHSType, 
                                EVAL_TYPE_UNWRAP_FLAG_ALL
                            );
                            EvalTypeKey RHSTypeDirect = EvalTypeKeyUnwrap(
                                RHSType,
                                EVAL_TYPE_UNWRAP_FLAG_ALL
                            );
                            u64 LHSTypeDirectByteSize = EvalByteSizeFromKey(LHSTypeDirect);
                            u64 RHSTypeDirectByteSize = EvalByteSizeFromKey(RHSTypeDirect);

                            if (LHSTypeDirectByteSize == RHSTypeDirectByteSize)
                                ArithPath = EVAL_ARITH_PATH_PTR_SUB;
                        }
                    } else if (
                        Kind == EVAL_EXPR_KIND_EQ_EQ ||
                        Kind == EVAL_EXPR_KIND_NOT_EQ
                    ) {
                        if (
                            LHSTypeKind == EVAL_TYPE_KIND_ARRAY &&
                            (
                                RHSTypeKind == EVAL_TYPE_KIND_PTR ||
                                RHSIsDecay
                            )
                        ) {
                            ArithPath = EVAL_ARITH_PATH_PTR_ARRAY_CMP;
                        }

                        if (
                            RHSTypeKind == EVAL_TYPE_KIND_ARRAY &&
                            (
                                LHSTypeKind == EVAL_TYPE_KIND_PTR ||
                                LHSIsDecay
                            )
                        ) {
                            ArithPath = EVAL_ARITH_PATH_PTR_ARRAY_CMP;
                        }

                        if (
                            LHSTree.Mode == EVAL_MODE_NULL &&
                            RHSTree.Mode == EVAL_MODE_NULL
                        ) {
                            ArithPath = EVAL_ARITH_PATH_TYPE_CMP;
                        }
                    }

                    switch (ArithPath) {
                        case EVAL_ARITH_PATH_NORMAL: {
                            if (
                                !EvalEvalOpTypeGroupAreCompatible(Op, LHSTypeGroup) ||
                                !EvalEvalOpTypeGroupAreCompatible(Op, RHSTypeGroup)
                            ) {
                                EvalMessage(
                                    MemPool,
                                    &Result.Msgs,
                                    EVAL_MSG_KIND_MALFORMED_INPUT,
                                    Expr->Range,
                                    "cannot use this operator on this type."_s8
                                );
                                break;
                            }

                            {
                                EvalTypeKey FinalTypeKey = IsComparison
                                    ? EvalTypeKeyBasic(EVAL_TYPE_KIND_BOOL)
                                    : LHSType;
                                EvalIRNode* LHSValueTree = EvalIRTreeResolveToValue(
                                    MemPool,
                                    LHSTree.Mode,
                                    LHSTree.Root,
                                    LHSType
                                );
                                EvalIRNode* RHSValueTree = EvalIRTreeResolveToValue(
                                    MemPool,
                                    RHSTree.Mode,
                                    RHSTree.Root,
                                    RHSType
                                );

                                LHSValueTree = EvalIRTreeConvertHi(
                                    MemPool,
                                    LHSValueTree,
                                    LHSType,
                                    LHSType
                                );
                                RHSValueTree = EvalIRTreeConvertHi(
                                    MemPool,
                                    RHSValueTree,
                                    LHSType,
                                    RHSType
                                );

                                EvalIRNode* NewTree = EvalIRTreeBinaryOpU(
                                    MemPool,
                                    Op,
                                    OpOperandSize,
                                    LHSValueTree,
                                    RHSValueTree
                                );

                                Result.Root = NewTree;
                                Result.TypeKey = FinalTypeKey;
                                Result.Mode = EVAL_MODE_VALUE;
                            }
                        } break;

                        case EVAL_ARITH_PATH_PTR_ADD: {
                            EvalIRTreeAndType* PtrTree = &LHSTree;
                            EvalIRTreeAndType* IntTree = &RHSTree;
                            u64 PtrSize = LHSTypeSize;
                            b32 PtrIsDecay = LHSIsDecay;

                            if (PtrArithmeticMulRHSPtr) {
                                PtrTree = &RHSTree;
                                PtrSize = RHSTypeSize;
                                IntTree = &LHSTree;
                                PtrIsDecay = RHSIsDecay;
                            }

                            EvalTypeKey DirectType = EvalTypeKeyUnwrap(
                                PtrTree->TypeKey,
                                EVAL_TYPE_UNWRAP_FLAG_ALL
                            );
                            u64 DirectTypeSize = EvalByteSizeFromKey(DirectType);

                            {
                                EvalIRNode* PtrRoot = PtrTree->Root;

                                if (!PtrIsDecay) {
                                    PtrRoot = EvalIRTreeResolveToValue(
                                        MemPool,
                                        PtrTree->Mode,
                                        PtrRoot,
                                        PtrTree->TypeKey
                                    );
                                }

                                EvalIRNode* IntRoot = IntTree->Root;

                                IntRoot = EvalIRTreeResolveToValue(
                                    MemPool,
                                    IntTree->Mode,
                                    IntRoot,
                                    IntTree->TypeKey
                                );

                                if (DirectTypeSize > 1) {
                                    EvalIRNode* ConstRoot = EvalIRTreeConstU(
                                        MemPool,
                                        DirectTypeSize
                                    );

                                    IntRoot = EvalIRTreeBinaryOpU(
                                        MemPool,
                                        EVAL_EVAL_OP_MUL,
                                        PtrSize,
                                        IntRoot,
                                        ConstRoot
                                    );
                                }

                                EvalTypeKey PtrType = PtrTree->TypeKey;
                                EvalIRNode* NewRoot = EvalIRTreeBinaryOpU(
                                    MemPool,
                                    Op,
                                    PtrSize,
                                    PtrRoot,
                                    IntRoot
                                );

                                Result.Root = NewRoot;
                                Result.TypeKey = PtrType;
                                Result.Mode = EVAL_MODE_VALUE;
                            }
                        } break;

                        case EVAL_ARITH_PATH_PTR_SUB: {
                            EvalTypeKey DirectType = EvalTypeKeyUnwrap(
                                LHSType,
                                EVAL_TYPE_UNWRAP_FLAG_ALL
                            );
                            u64 DirectTypeSize = EvalByteSizeFromKey(DirectType);
                            EvalIRNode* LHSRoot = LHSTree.Root;
                            EvalIRNode* RHSRoot = RHSTree.Root;

                            if (!LHSIsDecay) {
                                LHSRoot = EvalIRTreeResolveToValue(
                                    MemPool,
                                    LHSTree.Mode,
                                    LHSRoot,
                                    LHSType
                                );
                            }

                            if (!RHSIsDecay) {
                                RHSRoot = EvalIRTreeResolveToValue(
                                    MemPool,
                                    RHSTree.Mode,
                                    RHSRoot,
                                    RHSType
                                );
                            }

                            EvalIRNode* OpTree = EvalIRTreeBinaryOpU(
                                MemPool,
                                Op,
                                LHSTypeSize,
                                LHSRoot,
                                LHSRoot
                            );
                            EvalIRNode* NewTree = OpTree;

                            if (DirectTypeSize > 1) {
                                EvalIRNode* ConstTree = EvalIRTreeConstU(
                                    MemPool,
                                    DirectTypeSize
                                );

                                NewTree = EvalIRTreeBinaryOpU(
                                    MemPool,
                                    EVAL_EVAL_OP_DIV,
                                    LHSTypeSize,
                                    NewTree,
                                    ConstTree
                                );
                            }

                            Result.Root = NewTree;
                            Result.TypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_U64);
                            Result.Mode = EVAL_MODE_VALUE;
                        } break;

                        case EVAL_ARITH_PATH_PTR_ARRAY_CMP: {
                            b32 PtrIsDecay = LHSIsDecay;
                            EvalIRTreeAndType* PtrTree = &LHSTree;
                            EvalIRTreeAndType* ArrTree = &RHSTree;
                            u64 PtrSize = LHSTypeSize;

                            if (
                                LHSTypeKind == EVAL_TYPE_KIND_ARRAY && 
                                LHSTree.Mode == EVAL_MODE_VALUE
                            ) {
                                PtrIsDecay = RHSIsDecay;
                                PtrTree = &RHSTree;
                                ArrTree = &LHSTree;
                                PtrSize = RHSTypeSize;
                            }

                            EvalIRNode* PtrRoot = PtrTree->Root;
                            EvalIRNode* ArrRoot = ArrTree->Root;

                            if (!PtrIsDecay) {
                                PtrRoot = EvalIRTreeResolveToValue(
                                    MemPool,
                                    PtrTree->Mode,
                                    PtrTree->Root,
                                    PtrTree->TypeKey
                                );
                            }

                            EvalIRNode* MemRoot = EvalIRTreeMemReadType(
                                MemPool,
                                PtrRoot,
                                ArrTree->TypeKey
                            );

                            Result.Root = EvalIRTreeBinaryOp(
                                MemPool,
                                Op,
                                EVAL_EVAL_TYPE_GROUP_OTHER,
                                PtrSize,
                                MemRoot,
                                ArrRoot
                            );
                            Result.TypeKey = EvalTypeKeyBasic(
                                EVAL_TYPE_KIND_BOOL
                            );
                            Result.Mode = EVAL_MODE_VALUE;
                        } break;

                        case EVAL_ARITH_PATH_TYPE_CMP: {
                            Result.Root = EvalIRTreeConstU(
                                MemPool,
                                !!(LHSType == RHSType)
                            );
                            Result.TypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_BOOL);
                            Result.Mode = EVAL_MODE_VALUE;
                        } break;
                    }
                } break;

                // TODO:
                case EVAL_EXPR_KIND_TERN: {} break;

                case EVAL_EXPR_KIND_CALL: {
                    b32 StripLenses = FALSE;
                    EvalExpr* LHS = Expr->Head;
                    EvalIRTreeAndType LHSIRTree = EvalPushIRTreeAndType(
                        MemPool,
                        Parent,
                        (EvalIdentifierResolutionRule*) &EVAL_CALLABLE_IDENT_RES_RULE,
                        DisallowAutoHooks,
                        TRUE,
                        LHS
                    );

                    ListCat(&Result.Msgs, &LHSIRTree.Msgs);

                    EvalTypeKey LHSTypeKey = LHSIRTree.TypeKey;
                    EvalType* LHSType = EvalTypeFromKey(LHSTypeKey);

                    if (
                        LHSIRTree.Mode == EVAL_MODE_NULL &&
                        LHSType != &EMPTY_EVAL_TYPE_VALUE &&
                        LHSType->Kind != EVAL_TYPE_KIND_LENS &&
                        LHSType->Kind != EVAL_TYPE_KIND_LENS_SPEC
                    ) {
                        EvalIRTreeAndType CastedTree = EvalPushIRTreeAndType(
                            MemPool,
                            Parent,
                            (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                            DisallowAutoHooks,
                            TRUE,
                            Expr->Head->Next
                        );

                        ListCat(&Result.Msgs, &CastedTree.Msgs);

                        EvalTypeKey CastType = LHSIRTree.TypeKey;
                        EvalTypeKind CastTypeKind = EvalTypeKindFromKey(
                            CastType
                        );
                        EvalTypeKey CastTypeUnwrapped = EvalTypeKeyUnwrap(
                            LHSIRTree.TypeKey,
                            EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE
                        );
                        EvalTypeKind CastTypeUnwrappedKind = EvalTypeKindFromKey(
                            CastTypeUnwrapped
                        );
                        u64 CastTypeByteSize = EvalByteSizeFromKey(CastType);
                        EvalTypeKey CastedType = CastedTree.TypeKey;
                        EvalTypeKey CastedTypeUnwrapped = EvalTypeKeyUnwrap(
                            CastedType,
                            EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE | EVAL_TYPE_UNWRAP_FLAG_BITFIELDS
                        );
                        EvalTypeKind CastedTypeUnwrappedKind = EvalTypeKindFromKey(
                            CastedTypeUnwrapped
                        );
                        u64 CastedTypeByteSize = EvalByteSizeFromKey(CastedType);
                        u8 FromGroup = EvalTypeGroupFromKind(
                            CastedTypeUnwrappedKind
                        );
                        u8 ToGroup = EvalTypeGroupFromKind(
                            CastTypeUnwrappedKind
                        );
                        EvalEvalConversionKind ConversionRule = EvalEvalConvKindFromTypeGroups(
                            FromGroup,
                            ToGroup
                        );

                        if (!CastedTree.Root->Op) {
                            break;
                        } else if (CastTypeKind == EVAL_TYPE_KIND_NULL) {
                            break;
                        } else if (
                            ConversionRule != EVAL_EVAL_CONV_KIND_NOOP ||
                            ConversionRule != EVAL_EVAL_CONV_KIND_LEGAL
                        ) {
                            Str8 Text = "Unknown cast conversion rule."_s8;

                            EvalMessage(
                                MemPool,
                                &Result.Msgs,
                                EVAL_MSG_KIND_MALFORMED_INPUT,
                                Expr->Range,
                                Text
                            );
                            break;
                        }

                        {
                            EvalIRNode* FromTree = EvalIRTreeResolveToValue(
                                MemPool,
                                CastedTree.Mode,
                                CastedTree.Root,
                                CastedType
                            );
                            EvalIRNode* NewTree = FromTree;

                            if (ConversionRule == EVAL_EVAL_CONV_KIND_LEGAL) {
                                NewTree = EvalIRTreeConvertLo(
                                    MemPool,
                                    FromTree,
                                    ToGroup,
                                    FromGroup
                                );
                            }

                            if (
                                CastTypeByteSize < CastedTypeByteSize &&
                                EvalTypeKindIsInteger(CastTypeUnwrappedKind)
                            ) {
                                NewTree = EvalIRTreeTrunc(
                                    MemPool,
                                    FromTree,
                                    CastType
                                );
                            }

                            if (
                                EvalTypeKindIsSigned(CastTypeUnwrappedKind) &&
                                EvalTypeKindIsInteger(CastedTypeUnwrappedKind) &&
                                !EvalTypeKindIsSigned(CastedTypeUnwrappedKind)
                            ) {
                                NewTree = EvalIRTreeTrunc(
                                    MemPool,
                                    FromTree,
                                    CastType
                                );
                            }

                            Result.Root = NewTree;
                            Result.TypeKey = CastType;
                            Result.Mode = EVAL_MODE_VALUE;
                        }
                    } else if (
                        LHS->Kind == EVAL_EXPR_KIND_MEMBER_ACCESS &&
                        LHS->Head->Next != EMPTY_EVAL_EXPR_VALUE
                    ) {
                        EvalExpr* Callee = EvalExprRef(
                            MemPool,
                            LHS->Head->Next
                        );
                        EvalExpr* FirstArg = EvalExprRef(MemPool, LHS->Head);
                        EvalExpr* Call = EvalPushExpr(
                            MemPool,
                            EVAL_EXPR_KIND_CALL,
                            Rng((u64) 0, (u64) 0)
                        );

                        EvalExprPushChild(Call, Callee);
                        EvalExprPushChild(Call, FirstArg);

                        for (
                            EvalExpr* Arg = LHS->Next;
                            Arg != EMPTY_EVAL_EXPR_VALUE;
                            Arg = Arg->Next
                        ) {
                            EvalExprPushChild(
                                Call,
                                EvalExprCpy(MemPool, Arg)
                            );
                        }

                        if (StrMatch(Callee->Ref->String, "raw"_s8, 0)) {
                            StripLenses = TRUE;
                            DisallowAutoHooks = TRUE;
                        }

                        Result = EvalPushIRTreeAndType(
                            MemPool,
                            Parent,
                            (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                            DisallowAutoHooks,
                            TRUE,
                            Call
                        );
                    } else if (LHSType->Kind == EVAL_TYPE_KIND_LENS_SPEC) {
                        if (StrMatch(LHSType->Name, "raw"_s8, 0)) {
                            StripLenses = TRUE;
                            DisallowAutoHooks = TRUE;
                        }

                        Result = EvalPushIRTreeAndType(
                            MemPool,
                            Parent,
                            (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                            DisallowAutoHooks,
                            TRUE,
                            LHS->Next
                        );

                        if (!StripLenses) {
                            TempArena Scratch = GetScratch(&MemPool, 1);
                            u64 ArgCount = 0;

                            for (
                                EvalExpr* Arg = LHS->Next->Next;
                                Arg != EMPTY_EVAL_EXPR_VALUE;
                                Arg = Arg->Next
                            ) {
                                ++ArgCount;
                            }

                            EvalExpr** Args = ArenaPushArrayZero(
                                Scratch.MemPool,
                                EvalExpr*,
                                ArgCount
                            );

                            {
                                u64 Index = 0;

                                for (
                                    EvalExpr* Arg = LHS->Next->Next;
                                    Arg != EMPTY_EVAL_EXPR_VALUE;
                                    Arg = Arg->Next
                                ) {
                                    Args[Index] = Arg;
                                }
                            }

                            {
                                Result.TypeKey = EvalTypeKeyConstruct(
                                    __Params.Kind = EVAL_TYPE_KIND_LENS,
                                    __Params.Flags = LHSType->Flags,
                                    __Params.Count = ArgCount,
                                    __Params.Args = Args,
                                    __Params.DirectKey = Result.TypeKey,
                                    __Params.Name = LHSType->Name,
                                    __Params.IRExtFunc = LHSType->IRExtFunc,
                                    __Params.AccessFunc = LHSType->AccessFunc,
                                    __Params.ExpandRule = LHSType->ExpandRule
                                );
                            }

                            ReleaseScratch(Scratch);
                        }
                    } else {
                        EvalMessage(
                            MemPool,
                            &Result.Msgs,
                            EVAL_MSG_KIND_INTERPRETATION_ERROR,
                            Expr->Range,
                            "Calling this type is not supported."_s8
                        );
                    }

                    if (StripLenses) {
                        if (T->Overridden) {
                            EvalMsgList ExistingMsgs = Result.Msgs;

                            for (
                                EvalIRTreeAndType* Prev = T->Overridden;
                                Prev;
                                Prev = Prev->Prev
                            ) {
                                Result = *Prev;
                            }

                            EvalMsgList OverriddenMsgs = ListCpy(
                                MemPool,
                                &Result.Msgs
                            );

                            Result.Msgs = ExistingMsgs;
                            ListCat(&Result.Msgs, &OverriddenMsgs);
                        }

                        Result.TypeKey = EvalTypeKeyUnwrap(
                            Result.TypeKey,
                            EVAL_TYPE_UNWRAP_FLAG_LENSES | EVAL_TYPE_UNWRAP_FLAG_META
                        );
                    }
                } break;

                case EVAL_EXPR_KIND_LEAF_BYTECODE: {
                    EvalIRNode* NewTree = EvalIRTreeByteCode(
                        MemPool,
                        Expr->ByteCode
                    );

                    NewTree->Space = Expr->Space;

                    EvalTypeKey FinalTypeKey = Expr->TypeKey;

                    Result.Root = NewTree;
                    Result.TypeKey = FinalTypeKey;
                    Result.Mode = Expr->Mode;
                } break;

                case EVAL_EXPR_KIND_LEAF_STR_LIT: {
                    Str8 String = Expr->String;
                    EvalTypeKey TypeKey = EvalTypeKeyConstructArray(
                        EvalTypeKeyBasic(EVAL_TYPE_KIND_UCHAR8),
                        String.Size,
                        0
                    );
                    EvalIRNode* NewTree = EvalIRTreeStrLit(MemPool, String);

                    Result.Root = NewTree;
                    Result.TypeKey = TypeKey;
                    Result.Mode = EVAL_MODE_VALUE;
                } break;

                case EVAL_EXPR_KIND_LEAF_U64: {
                    u64 Value = Expr->Value.U64;
                    EvalIRNode* NewTree = EvalIRTreeConstU(MemPool, Value);
                    EvalTypeKey TypeKey = {};

                    if (Value <= I32_MAX)
                        TypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_I32);
                    else if (Value <= I64_MAX)
                        TypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_I64);
                    else
                        TypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_U64);

                    Result.Root = NewTree;
                    Result.TypeKey = TypeKey;
                    Result.Mode = EVAL_MODE_VALUE;
                } break;

                case EVAL_EXPR_KIND_LEAF_F64: {
                    u64 Value = Expr->Value.U64;
                    EvalIRNode* NewTree = EvalIRTreeConstU(MemPool, Value);

                    Result.Root = NewTree;
                    Result.TypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_F64);
                    Result.Mode = EVAL_MODE_VALUE;
                } break;

                case EVAL_EXPR_KIND_LEAF_F32: {
                    u32 Value = Expr->Value.U32;
                    EvalIRNode* NewTree = EvalIRTreeConstU(MemPool, Value);

                    Result.Root = NewTree;
                    Result.TypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_F32);
                    Result.Mode = EVAL_MODE_VALUE;
                } break;


                case EVAL_EXPR_KIND_LEAF_IDENT: {
                    TempArena Scratch = GetScratch(&MemPool, 1);
                    Str8 Disambiguator = Expr->Disambiguator;
                    Str8 Qualifier = Expr->Qualifier;
                    Str8 String = Expr->String;
                    Str8 StringRedirected = String;
                    b32 StringMapped = FALSE;
                    b32 StringIsImplicitMemberName = FALSE;
                    EvalTypeKey MappedTypeKey = {};
                    EvalMode MappedByteCodeMode = EVAL_MODE_OFFSET;
                    EvalSpace MappedByteCodeSpace = {};
                    Str8 MappedByteCode = {};
                    void* MappedUserData = NULL;
                    b32 Generated = FALSE;
                    EvalCache* const __EvalCache = GetTLS()->EVAL_CACHE;
                    b32 DoLimitResolvePath = FALSE;
                    EvalIdentifierResolutionPath LimitResolvePath = EVAL_IDENT_RES_PATH_BUILTIN_TYPES;

                    for (
                        u64 PathIndex = 0; 
                        !Generated && PathIndex < IdentResRule->PathsCount; 
                        ++PathIndex
                    ) {
                        EvalIdentifierResolutionPath Path = IdentResRule->Paths[PathIndex];

                        PROFILE(IdentifierResolutionTimer) {
                            switch (Path) {
                                default: {} break;

                                case EVAL_IDENT_RES_PATH_PARENT_EXPR: {
                                    if (
                                        !Qualifier.Size &&
                                        !StringMapped &&
                                        StrMatch(String, "$"_s8, 0) &&
                                        Parent &&
                                        (
                                            Parent->Root != EMPTY_EVAL_IR_NODE_VALUE ||
                                            Parent->Msgs.Head
                                        )
                                    ) {
                                        EvalIRTreeAndType* ParentIRTree = Parent;

                                        {
                                            for (
                                                EvalIRTreeAndType* Prev = ParentIRTree; 
                                                Prev; 
                                                Prev = Prev->Prev
                                            ) {
                                                ParentIRTree = Prev;

                                                if (Prev->Root != EMPTY_EVAL_IR_NODE_VALUE)
                                                    break;
                                            }
                                        }

                                        StringMapped = TRUE;
                                        Generated = TRUE;
                                        DisallowAutoHooks = TRUE;

                                        EvalOpList OpList = EvalOpListFromIRTree(
                                            Scratch.MemPool,
                                            ParentIRTree->Root
                                        );
                                        Str8 ByteCode = EvalByteCodeFromOpList(
                                            MemPool,
                                            &OpList
                                        );

                                        Result.Root = EvalIRTreeByteCode(
                                            MemPool,
                                            ByteCode
                                        );
                                        Result.TypeKey = ParentIRTree->TypeKey;
                                        Result.Mode = ParentIRTree->Mode;
                                        Result.AutoHook = ParentIRTree->AutoHook;
                                        
                                        EvalMsgList Msgs = ListCpy(MemPool, &ParentIRTree->Msgs);

                                        ListCat(&Result.Msgs, &Msgs);
                                        Result.Prev = ParentIRTree->Prev;
                                    }
                                } break;

                                case EVAL_IDENT_RES_PATH_PARENT_EXPR_MEMBER: {
                                    if (
                                        !Qualifier.Size &&
                                        !StringMapped &&
                                        Parent &&
                                        Parent->Root != EMPTY_EVAL_IR_NODE_VALUE
                                    ) {
                                        for (
                                            EvalIRTreeAndType* Prev = Parent;
                                            Prev;
                                            Prev = Prev->Prev
                                        ) {
                                            EvalExpr* Acc = EvalExprIRExtMemberAccess(
                                                Scratch.MemPool,
                                                EMPTY_EVAL_EXPR_VALUE,
                                                Prev,
                                                String
                                            );
                                            EvalIRTreeAndType AccParent = {
                                                EMPTY_EVAL_IR_NODE_VALUE
                                            };
                                            EvalIRTreeAndType AccIRTree = EvalPushIRTreeAndType(
                                                Scratch.MemPool,
                                                Prev->Prev ? Prev->Prev : &AccParent,
                                                (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                                                TRUE,
                                                TRUE,
                                                Acc
                                            );

                                            if (AccIRTree.Root != EMPTY_EVAL_IR_NODE_VALUE) {
                                                StringMapped = TRUE;

                                                EvalOpList OpList = EvalOpListFromIRTree(
                                                    Scratch.MemPool,
                                                    AccIRTree.Root
                                                );

                                                MappedTypeKey = AccIRTree.TypeKey;
                                                MappedByteCode = EvalByteCodeFromOpList(
                                                    MemPool,
                                                    &OpList
                                                );
                                                MappedByteCodeMode = AccIRTree.Mode;
                                                ListCat(&Result.Msgs, &AccIRTree.Msgs);

                                                if (!Prev->AutoHook)
                                                    break;
                                            }
                                        }
                                    }
                                } break;

                                case EVAL_IDENT_RES_PATH_BUILTIN_CONSTANTS: {
                                    if (!StringMapped && StrMatch(String, "true"_s8, 0)) {
                                        StringMapped = TRUE;

                                        EvalOpList OpList = {};
                                        
                                        ListPushConstU(MemPool, &OpList, TRUE);
                                        MappedTypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_BOOL);
                                        MappedByteCode = EvalByteCodeFromOpList(MemPool, &OpList);
                                        MappedByteCodeMode = EVAL_MODE_VALUE;
                                    }

                                    if (!StringMapped && StrMatch(String, "false"_s8, 0)) {
                                        StringMapped = TRUE;

                                        EvalOpList OpList = {};

                                        ListPushConstU(MemPool, &OpList, FALSE);
                                        MappedTypeKey = EvalTypeKeyBasic(EVAL_TYPE_KIND_BOOL);
                                        MappedByteCode = EvalByteCodeFromOpList(MemPool, &OpList);
                                        MappedByteCodeMode = EVAL_MODE_VALUE;
                                    }
                                } break;

                                case EVAL_IDENT_RES_PATH_BUILTIN_TYPES: {
                                    MappedTypeKey = EvalLeafBuiltinTypeKeyFromName(String);
                                    StringMapped = MappedTypeKey != EMPTY_EVAL_TYPE_KEY_VALUE;
                                } break;

                                case EVAL_IDENT_RES_PATH_MACROS: {
                                    if (!Generated) {
                                        EvalExpr* MacroExpr = EvalStr2ExprMapLookup(
                                            GetTLS()->EVAL_IR_CTX->MacroMap,
                                            String
                                        );

                                        if (MacroExpr != EMPTY_EVAL_EXPR_VALUE) {
                                            Generated = TRUE;
                                            EvalStr2ExprMapPoisonIncrement(
                                                GetTLS()->EVAL_IR_CTX->MacroMap,
                                                String
                                            );
                                            Result = EvalPushIRTreeAndType(
                                                MemPool,
                                                Parent,
                                                (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                                                DisallowAutoHooks,
                                                TRUE,
                                                MacroExpr
                                            );
                                            EvalStr2ExprMapPoisonDecrement(
                                                GetTLS()->EVAL_IR_CTX->MacroMap,
                                                String
                                            );
                                        }
                                    }
                                } break;
                            }
                        }

                        if (!Generated && MappedByteCode.Size) {
                            Generated = TRUE;

                            EvalIRNode* Root = EvalIRTreeByteCode(
                                MemPool,
                                MappedByteCode
                            );

                            Root->Space = MappedByteCodeSpace;
                            Result.Root = Root;
                            Result.TypeKey = MappedTypeKey;
                            Result.Mode = MappedByteCodeMode;
                        }

                        if (!Generated && MappedTypeKey != EMPTY_EVAL_TYPE_KEY_VALUE) {
                            Generated = TRUE;
                            Result.Root = EvalIRTreeConstU(MemPool, 0);
                            Result.TypeKey = MappedTypeKey;
                            Result.Mode = EVAL_MODE_NULL;
                        }
                    }

                    if (Generated && StringIsImplicitMemberName) {
                        EvalExpr* Acc = EvalExprIRExtMemberAccess(
                            MemPool,
                            EMPTY_EVAL_EXPR_VALUE,
                            &Result,
                            String
                        );

                        Result = EvalPushIRTreeAndType(
                            MemPool,
                            Parent,
                            (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                            DisallowAutoHooks,
                            TRUE,
                            Acc
                        );
                    }

                    if (!Generated && !StrMatch(String, "$"_s8, 0)) {
                        EvalMessage(
                            MemPool,
                            &Result.Msgs,
                            EVAL_MSG_KIND_RESOLUTION_FAILURE,
                            Expr->Range,
                            "`%S` could not be found.",
                            PRINT_STR(String)
                        );
                    }

                    ReleaseScratch(Scratch);
                } break;

                case EVAL_EXPR_KIND_LEAF_OFFSET: {
                    EvalIRNode* NewTree = EvalPushIRNode(
                        MemPool,
                        EVAL_EVAL_OP_CONST_U64
                    );

                    NewTree->Value = Expr->Value;
                    NewTree->Space = Expr->Space;
                    Result.Root = NewTree;
                    Result.TypeKey = Expr->TypeKey;
                    Result.Mode = EVAL_MODE_OFFSET;
                } break;

                case EVAL_EXPR_KIND_LEAF_VALUE: {
                    EvalIRNode* NewTree = EvalPushIRNode(
                        MemPool,
                        EVAL_EVAL_OP_CONST_U128
                    );

                    NewTree->Value = Expr->Value;
                    NewTree->Space = Expr->Space;
                    Result.Root = NewTree;
                    Result.TypeKey = Expr->TypeKey;
                    Result.Mode = EVAL_MODE_VALUE;
                } break;

                case EVAL_EXPR_KIND_LEAF_FILE_PATH: {
                    TempArena Scratch = GetScratch(&MemPool, 1);
                    Str8 FilePath = Expr->String;
                    FileProperties Props = SystemGetFileProperties(FilePath);

                    if (
                        !StrMatch(Expr->Qualifier, "folder"_s8, 0) && 
                        !(Props.Kind & SYS_FILE_IS_DIR) && 
                        FilePath.Size
                    ) {
                        EvalSpace Space = EvalSpaceAlloc(EVAL_SPACE_KIND_FILESYSTEM);

                        Result.Root = EvalIRTreeSetSpace(
                            MemPool,
                            Space,
                            EvalIRTreeConstU(
                                MemPool, 
                                EvalIDFromStr(FilePath)
                            )
                        );
                        Result.TypeKey = __EvalCache->FileTypeKey;
                        Result.Mode = EVAL_MODE_VALUE;
                    } else {
                        Str8 FolderPath = StrChopLastSlash(FilePath);

                        Props = SystemGetFileProperties(FolderPath);

                        if (
                            Props.Kind & SYS_FILE_IS_DIR ||
                            !FolderPath.Size ||
                            StrMatch(FolderPath, "/"_s8, STR_MATCH_SKIP_SLASH)
                        ) {
                            EvalSpace Space = EvalSpaceAlloc(
                                EVAL_SPACE_KIND_FILESYSTEM
                            );

                            Result.Root = EvalIRTreeSetSpace(
                                MemPool,
                                Space,
                                EvalIRTreeConstU(
                                    MemPool,
                                    EvalIDFromStr(FolderPath)
                                )
                            );
                            Result.TypeKey = __EvalCache->FolderTypeKey;
                            Result.Mode = EVAL_MODE_VALUE;
                        }
                    }
                } break;

                case EVAL_EXPR_KIND_TYPE_IDENT: {
                    Result.Root = EvalIRTreeConstU(MemPool, 0);
                    Result.Root->Space = Expr->Space;
                    Result.TypeKey = Expr->TypeKey;
                    Result.Mode = EVAL_MODE_NULL;
                } break;

                // case EVAL_EXPR_KIND_UNSIGNED: {} break;

                // case EVAL_EXPR_KIND_PTR: {} break;

                case EVAL_EXPR_KIND_ARRAY: {
                    EvalIRTreeAndType ElementIRTree = EvalPushIRTreeAndType(
                        MemPool,
                        Parent,
                        (EvalIdentifierResolutionRule*) &EVAL_DEFAULT_IDENT_RES_RULE,
                        DisallowAutoHooks,
                        TRUE,
                        Expr->Head
                    );

                    Result = ElementIRTree;
                    Result.TypeKey = EvalTypeKeyConstructArray(
                        Result.TypeKey,
                        Expr->Value.U64,
                        0
                    );
                } break;
            }

            if (!DisallowChainedFastPaths) {
                local struct {
                    Str8 ShortHand;
                    Str8 FullName;
                } ShortHandLensPairTable[] = {
                    {
                        Str8Lit("x"),
                        Str8Lit("hex")
                    },
                    {
                        Str8Lit("b"),
                        Str8Lit("bin")
                    },
                    {
                        Str8Lit("o"),
                        Str8Lit("oct")
                    },
                    {
                        Str8Lit("d"),
                        Str8Lit("dec")
                    }
                };

                EvalExpr* HeadChained = Expr->Next;

                for (
                    EvalExpr* ChainedExpr = HeadChained;
                    ChainedExpr != EMPTY_EVAL_EXPR_VALUE;
                    ChainedExpr = ChainedExpr->Next
                ) {
                    b32 MatchesShortHand = FALSE;

                    if (ChainedExpr->Kind == EVAL_EXPR_KIND_LEAF_IDENT) {
                        for (
                            u64 ShortHandIndex = 0;
                            ShortHandIndex < ARRAY_COUNT(ShortHandLensPairTable);
                            ++ShortHandIndex
                        ) {
                            if (
                                StrMatch(
                                    ChainedExpr->String, 
                                    ShortHandLensPairTable[ShortHandIndex].ShortHand, 
                                    0
                                )
                            ) {
                                Str8 FullName = ShortHandLensPairTable[ShortHandIndex].FullName;
                                EvalExpr* LensSpecExpr = EvalStr2ExprMapLookup(
                                    GetTLS()->EVAL_IR_CTX->MacroMap,
                                    FullName
                                );
                                EvalTypeKey LensSpecTypeKey = LensSpecExpr->TypeKey;
                                EvalType* LensSpecType = EvalTypeFromKey(LensSpecTypeKey);

                                Result.TypeKey = EvalTypeKeyConstruct(
                                    __Params.Kind = EVAL_TYPE_KIND_LENS,
                                    __Params.DirectKey = Result.TypeKey,
                                    __Params.Name = FullName,
                                    __Params.Flags = LensSpecType->Flags
                                );
                                MatchesShortHand = TRUE;
                                break;
                            }
                        }
                    }

                    if (!MatchesShortHand) {
                        EvalTypeKind TypeKind = EvalTypeKindFromKey(
                            EvalTypeKeyUnwrap(Result.TypeKey, EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE)
                        );

                        if (
                            EvalTypeKindIsPtrOrRef(TypeKind) ||
                            TypeKind == EVAL_TYPE_KIND_ARRAY
                        ) {
                            EvalExpr* LensSpecExpr = EvalStr2ExprMapLookup(
                                GetTLS()->EVAL_IR_CTX->MacroMap,
                                "array"_s8
                            );
                            EvalTypeKey LensSpecTypeKey = LensSpecExpr->TypeKey;
                            EvalType* LensSpecType = EvalTypeFromKey(
                                LensSpecTypeKey
                            );

                            Result.TypeKey = EvalTypeKeyConstruct(
                                __Params.Kind = EVAL_TYPE_KIND_LENS,
                                __Params.Flags = LensSpecType->Flags,
                                __Params.Count = 1,
                                __Params.Args = &ChainedExpr,
                                __Params.DirectKey = Result.TypeKey,
                                __Params.Name = LensSpecType->Name,
                                __Params.IRExtFunc = LensSpecType->IRExtFunc,
                                __Params.AccessFunc = LensSpecType->AccessFunc,
                                __Params.ExpandRule = LensSpecType->ExpandRule
                            );
                        }
                    }
                }
            }

            EvalType* __Type = EvalTypeFromKey(Result.TypeKey);

            {
                EvalTypeIRExtFuncType* IRExt = __Type->IRExtFunc;

                for (
                    EvalType* T = __Type;
                    (
                        T->Kind == EVAL_TYPE_KIND_LENS || 
                        T->Kind == EVAL_TYPE_KIND_SET || 
                        (
                            T->Kind >= EVAL_TYPE_KIND_FIRST_META && 
                            T->Kind <= EVAL_TYPE_KIND_LAST_META
                        )
                    );
                    T = EvalTypeFromKey(T->DirectTypeKey)
                ) {
                    if (T->IRExtFunc) {
                        IRExt = T->IRExtFunc;
                        break;
                    }
                }

                if (IRExt && !Result.UserData) {
                    EvalIRExt Ext = IRExt(MemPool, Expr, &Result);

                    Result.UserData = Ext.UserData;
                }
            }

            if (Parent && Parent->Root != EMPTY_EVAL_IR_NODE_VALUE) {
                EvalIRTreeAndType* NewChain = ArenaPushArrayZero(
                    MemPool,
                    EvalIRTreeAndType,
                    1
                );

                MemCpy(NewChain, Parent, sizeof(*NewChain));

                EvalIRTreeAndType* HeadChain = NULL;
                EvalIRTreeAndType* TailChain = NULL;

                if (Result.Prev) {
                    for (EvalIRTreeAndType* P = Result.Prev; P; P = P->Prev) {
                        EvalIRTreeAndType* PCopy = ArenaPushArrayZero(
                            MemPool,
                            EvalIRTreeAndType,
                            1
                        );

                        MemCpy(PCopy, P, sizeof(*PCopy));
                        SLL_QUEUE_PUSH_EX(HeadChain, TailChain, PCopy, Prev);
                    }

                    SLL_QUEUE_PUSH_FRONT_EX(HeadChain, TailChain, NewChain, Prev);
                    Result.Prev = HeadChain;
                }
            }

            if (T->Overridden)
                Result.AutoHook = TRUE;
        }

        if (InheritedLenses.Count) {
            EvalType* ResultType = EvalTypeFromKey(Result.TypeKey);

            for (
                EvalTypeKeyNode* N = InheritedLenses.Head;
                N;
                N = N->Next
            ) {
                EvalType* SrcType = EvalTypeFromKey(N->V);
                EvalTypeKey DstTypeKey = EvalTypeKeyConstruct(
                    __Params.Kind = SrcType->Kind,
                    __Params.Flags = SrcType->Flags,
                    __Params.Name = SrcType->Name,
                    __Params.Count = SrcType->Count,
                    __Params.Args = SrcType->Args,
                    __Params.IRExtFunc = SrcType->IRExtFunc,
                    __Params.AccessFunc = SrcType->AccessFunc,
                    __Params.ExpandRule = SrcType->ExpandRule,
                    __Params.DirectKey = Result.TypeKey
                );

                Result.TypeKey = DstTypeKey;
            }
        }

        ReleaseScratch(Scratch);
    }

    return Result;
}

void 
EvalAppendOpList(
    Arena* MemPool, 
    EvalIRNode* Root, 
    EvalSpace* CurrentSpace, 
    OUT EvalOpList* List
) {
    u32 Op = Root->Op;

    {
        EvalSpace EmptySpace = {};

        if (Root->Space != EmptySpace && Root->Space != *CurrentSpace) {
            *CurrentSpace = Root->Space;
            ListPushSetSpace(MemPool, List, Root->Space);
        }
    }

    switch (Op) {
        default: {
            if (Op >= EVAL_EVAL_OP_COUNT) {
                // TODO(nathan): this should be an error but needs to be checked against
                //             : the custom op type count that would exceed EVAL_EVAL_OP_COUNT
            } else {
                u16 CtrlBits = EVAL_EVAL_OP_CTRL_BITS_TABLE[Op];
                u64 ChildCount = EVAL_POP_FROM_CTRL_BITS(CtrlBits);
                u64 Index = 0;

                for (
                    EvalIRNode* Child = Root->Head;
                    Child != EMPTY_EVAL_IR_NODE_VALUE && Index < ChildCount;
                    Child = Child->Next, ++Index
                ) {
                    EvalAppendOpList(MemPool, Child, CurrentSpace, List);
                }

                ListPushOp(MemPool, List, (EvalEvalOp) Root->Op, Root->Value);
            }
        } break;

        case EVAL_IR_EXT_KIND_BYTECODE: {
            ListPushByteCode(MemPool, List, Root->String);
        } break;

        case EVAL_IR_EXT_KIND_SET_SPACE: {
            EvalSpace Space = {};

            MemCpy(&Space, &Root->Value, sizeof(Space));
            ListPushSetSpace(MemPool, List, Space);

            for (
                EvalIRNode* Child = Root->Head;
                Child != EMPTY_EVAL_IR_NODE_VALUE;
                Child = Child->Next
            ) {
                EvalAppendOpList(MemPool, Child, CurrentSpace, List);
            }

            ListPushSetSpace(MemPool, List, Space);
        } break;

        case EVAL_EVAL_OP_CONST_STR: {
            ListPushStrLit(MemPool, List, Root->String);
        } break;
    }
}

EvalOpList 
EvalOpListFromIRTree(Arena* MemPool, EvalIRNode* Root)
{
    EvalOpList Ops = {};
    EvalSpace Space = GetTLS()->EVAL_INTERP_CTX->PrimarySpace;

    EvalAppendOpList(MemPool, Root, &Space, &Ops);

    return Ops;
}

Str8 
EvalByteCodeFromOpList(Arena* MemPool, EvalOpList* List)
{
    u64 Size = List->EncodedSize;
    u8* String = ArenaPushArray(MemPool, u8, Size);
    u8* StringPtr = String;
    u8* StringEnd = String + Size;

    for (EvalOp* Op = List->Head; Op; Op = Op->Next) {
        u32 OpCode = Op->OpCode;

        switch (OpCode) {
            default: {
                u16 CtrlBits = EVAL_EVAL_OP_CTRL_BITS_TABLE[OpCode];
                u64 ExtraByteCount = EVAL_DECODE_FROM_CTRL_BITS(CtrlBits);
                u8* NextStringPtr = StringPtr + 1 + ExtraByteCount;

                ASSERT(NextStringPtr <= StringEnd);

                StringPtr[0] = OpCode;
                MemCpy(StringPtr + 1, &Op->Value.U512.U8[0], ExtraByteCount);
                StringPtr = NextStringPtr;
            } break;

            case EVAL_EVAL_OP_CONST_STR: {
                u8* NextStringPtr = StringPtr + 2 + Op->Value.U64;

                ASSERT(NextStringPtr <= StringEnd);

                StringPtr[0] = OpCode;
                StringPtr[1] = (u8) Op->Value.U64;
                MemCpy(StringPtr + 2, Op->String.Str, Op->Value.U64);
                StringPtr = NextStringPtr;
            } break;

            case EVAL_IR_EXT_KIND_BYTECODE: {
                u64 Size = Op->String.Size;
                u8* NextStringPtr = StringPtr + Size;

                ASSERT(NextStringPtr <= StringEnd);

                MemCpy(StringPtr, Op->String.Str, Size);
                StringPtr = NextStringPtr;
            } break;

            case EVAL_IR_EXT_KIND_SET_SPACE: {
                u64 ExtraByteCount = sizeof(EvalSpace);
                u8* NextStringPtr = StringPtr + 1 + ExtraByteCount;

                ASSERT(NextStringPtr <= StringEnd);

                StringPtr[0] = OpCode;
                MemCpy(StringPtr + 1, &Op->Value.U128, ExtraByteCount);
                StringPtr = NextStringPtr;
            } break;
        }
    }

    Str8 Result = {};

    Result.Size = Size;
    Result.Str = String;

    return Result;
}

EvalExpr* 
EvalExprIRExtMemberAccess(
    Arena* MemPool, 
    EvalExpr* LHS, 
    EvalIRTreeAndType* LHSIRTree, 
    Str8 MemberName
) {
    EvalExpr* Root = EvalPushExpr(MemPool, EVAL_EXPR_KIND_MEMBER_ACCESS, Rng((u64) 0, (u64) 0));
    EvalExpr* LHSByteCode = EvalPushExpr(MemPool, EVAL_EXPR_KIND_LEAF_BYTECODE, LHS->Range);
    EvalOpList LHSOpList = EvalOpListFromIRTree(MemPool, LHSIRTree->Root);

    LHSByteCode->String = EvalStrFromExpr(MemPool, LHS, {});
    LHSByteCode->Qualifier = LHS->Qualifier;
    LHSByteCode->Space = LHS->Space;
    LHSByteCode->Mode = LHSIRTree->Mode;
    LHSByteCode->TypeKey = LHSIRTree->TypeKey;
    LHSByteCode->ByteCode = EvalByteCodeFromOpList(MemPool, &LHSOpList);

    EvalExpr* RHS = EvalPushExpr(MemPool, EVAL_EXPR_KIND_LEAF_IDENT, Rng((u64) 0, (u64) 0));

    RHS->String = ArenaPushStrCpy(MemPool, MemberName);
    EvalExprPushChild(Root, LHSByteCode);
    EvalExprPushChild(Root, RHS);

    return Root;
}

void 
ListPush(
    Arena* MemPool, 
    EvalTokenChunkList* List, 
    u64 ChunkSize, 
    EvalToken* Token
) {
    EvalTokenChunkNode* Node = List->Tail;

    if (!Node || Node->Count >= Node->Capacity) {
        Node = ArenaPushArrayZero(MemPool, EvalTokenChunkNode, 1);
        SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
        Node->Capacity = ChunkSize;
        Node->V = ArenaPushArray(MemPool, EvalToken, Node->Capacity);
        ++List->NodeCount;
    }

    MemCpy(&Node->V[Node->Count], Token, sizeof(EvalToken));
    ++Node->Count;
    ++List->TotalCount;
}

EvalTokenArray 
EvalTokenArrayFromChunkList(Arena* MemPool, EvalTokenChunkList* List)
{
    EvalTokenArray Result = {};

    Result.Count = List->TotalCount;
    Result.V = ArenaPushArray(MemPool, EvalToken, Result.Count);

    u64 Index = 0;

    for (
        EvalTokenChunkNode* Node = List->Head; 
        Node; 
        Node = Node->Next
    ) {
        MemCpy(Result.V + Index, Node->V, sizeof(EvalToken) * Node->Count);
        Index += Node->Count;
    }

    return Result;
}

EvalTokenArray 
EvalTokenArrayFromText(Arena* MemPool, Str8 Text)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    EvalTokenChunkList Tokens = {};
    u64 ActiveTokenStartIndex = 0;
    EvalTokenKind ActiveTokenKind = EVAL_TOKEN_KIND_NULL;
    b32 ActiveTokenKindHasTickPrefix = FALSE;
    b32 Escaped = FALSE;
    b32 _Exp = FALSE;
    b32 CommentIsExplicitTerminal = FALSE;

    for (u64 Index = 0, Advance = 0; Index <= Text.Size; Index += Advance) {
        u8 Byte = (Index < Text.Size) ? Text.Str[Index] : 0;
        u8 ByteNext = (Index + 1 < Text.Size) ? Text.Str[Index + 1] : 0;
        u8 ByteNextNext = (Index + 2 < Text.Size) ? Text.Str[Index + 2] : 0;

        Advance = 1;

        b32 TokenFormed = FALSE;
        u64 TokenTerminalIndexPad = 0;

        switch (ActiveTokenKind) {
            default: {
                if (
                    IsAlpha(Byte) ||
                    Byte == '_' ||
                    Byte == '`' ||
                    Byte == '$'
                ) {
                    ActiveTokenKind = EVAL_TOKEN_KIND_IDENT;
                    ActiveTokenStartIndex = Index;
                    ActiveTokenKindHasTickPrefix = (Byte == '`');
                } else if (
                    IsDigit(Byte) ||
                    (
                        Byte == '.' &&
                        IsDigit(ByteNext)
                    )
                ) {
                    ActiveTokenKind = EVAL_TOKEN_KIND_NUMERIC;
                    ActiveTokenStartIndex = Index;
                } else if (Byte == '"') {
                    ActiveTokenKind = EVAL_TOKEN_KIND_STR_LIT;
                    ActiveTokenStartIndex = Index;
                } else if (Byte == '\'') {
                    ActiveTokenKind = EVAL_TOKEN_KIND_CHAR_LIT;
                    ActiveTokenStartIndex = Index;
                } else if (
                    Byte == '~' ||
                    Byte == '!' ||
                    Byte == '%' ||
                    Byte == '^' ||
                    Byte == '&' ||
                    Byte == '*' ||
                    Byte == '(' ||
                    Byte == ')' ||
                    Byte == '-' ||
                    Byte == '=' ||
                    Byte == '+' ||
                    Byte == '[' ||
                    Byte == ']' ||
                    Byte == '{' ||
                    Byte == '}' ||
                    Byte == ':' ||
                    Byte == ';' ||
                    Byte == ',' ||
                    Byte == '.' ||
                    Byte == '<' ||
                    Byte == '>' ||
                    Byte == '/' ||
                    Byte == '?' ||
                    Byte == '|' ||
                    Byte == '#' ||
                    Byte == '@'
                ) {
                    ActiveTokenKind = EVAL_TOKEN_KIND_SYMBOL;
                    ActiveTokenStartIndex = Index;
                } else if (Byte == '/' && ByteNext == '/') {
                    ActiveTokenKind = EVAL_TOKEN_KIND_COMMENT;
                    ActiveTokenStartIndex = Index;
                    CommentIsExplicitTerminal = FALSE;
                } else if (Byte == '/' && ByteNext == '*') {
                    ActiveTokenKind = EVAL_TOKEN_KIND_COMMENT;
                    ActiveTokenStartIndex = Index;
                    CommentIsExplicitTerminal = TRUE;
                }
            } break;

            case EVAL_TOKEN_KIND_IDENT: {
                if (
                    Byte == ':' && 
                    ByteNext == ':' &&
                    (
                        IsAlpha(ByteNextNext) ||
                        ByteNextNext == '_' ||
                        ByteNextNext == '<'
                    )
                ) {
                    Advance = 2;
                } else if (
                    (
                        Byte == '\'' ||
                        Byte == '`'
                    ) &&
                    ActiveTokenKindHasTickPrefix
                ) {
                    ActiveTokenKindHasTickPrefix = FALSE;
                    Advance = 1;
                } else if (Byte == '<') {
                    i64 Nest = 1;

                    for (
                        u64 I = Index + 1;
                        I <= Text.Size;
                        ++I
                    ) {
                        if (I < Text.Size && Text.Str[I] == '<') {
                            ++Nest;
                        } else if (I < Text.Size && Text.Str[I] == '>') {
                            --Nest;

                            if (!Nest) {
                                Advance = (I + 1 - Index);
                                break;
                            }
                        } else if (I == Text.Size && Nest) {
                            TokenFormed = FALSE;
                            Advance = 0;
                            break;
                        }
                    }
                } else if (
                    !IsAlpha(Byte) && 
                    !IsDigit(Byte) && 
                    Byte != '_' && 
                    !ActiveTokenKindHasTickPrefix && 
                    Byte != '@' && 
                    Byte != '$'
                ) {
                    Advance = 0;
                    TokenFormed = TRUE;
                }
            } break;

            case EVAL_TOKEN_KIND_NUMERIC: {
                if (_Exp && (Byte == '+' || Byte == '-')) {
                } else if (
                    !IsAlpha(Byte) && 
                    !IsDigit(Byte) && 
                    Byte != '.' && 
                    Byte != ':'
                ) {
                    Advance = 0;
                    TokenFormed = TRUE;
                } else {
                    _Exp = 0;
                    _Exp = (Byte == 'e');
                }
            } break;

            case EVAL_TOKEN_KIND_STR_LIT: {
                if (!Escaped && Byte == '\\') {
                    Escaped = TRUE;
                } else if (Escaped) {
                    Escaped = FALSE;
                } else if (!Escaped && Byte == '"') {
                    Advance = 1;
                    TokenFormed = TRUE;
                    TokenTerminalIndexPad = 1;
                }
            } break;

            case EVAL_TOKEN_KIND_CHAR_LIT: {
                if (!Escaped && Byte == '\\') {
                    Escaped = TRUE;
                } else if (Escaped) {
                    Escaped = FALSE;
                } else if (!Escaped && Byte == '\'') {
                    Advance = 1;
                    TokenFormed = TRUE;
                    TokenTerminalIndexPad = 1;
                }
            } break;

            case EVAL_TOKEN_KIND_SYMBOL: {
                if (
                    Byte != '~' && 
                    Byte != '!' && 
                    Byte != '%' && 
                    Byte != '^' &&
                    Byte != '&' && 
                    Byte != '*' && 
                    Byte != '(' && 
                    Byte != ')' &&
                    Byte != '-' && 
                    Byte != '=' && 
                    Byte != '+' && 
                    Byte != '[' &&
                    Byte != ']' && 
                    Byte != '{' && 
                    Byte != '}' && 
                    Byte != ':' &&
                    Byte != ';' && 
                    Byte != ',' && 
                    Byte != '.' && 
                    Byte != '<' &&
                    Byte != '>' && 
                    Byte != '/' && 
                    Byte != '?' && 
                    Byte != '|' &&
                    Byte != '#' && 
                    Byte != '@'
                ) {
                    Advance = 0;
                    TokenFormed = TRUE;
                }
            } break;

            case EVAL_TOKEN_KIND_COMMENT: {
                if (!Byte) {
                    TokenFormed = TRUE;
                } else if (
                    CommentIsExplicitTerminal && 
                    Byte == '*' && 
                    ByteNextNext == '/'
                ) {
                    TokenFormed = TRUE;
                    Advance = 2;
                }
            } break;
        }

        if (TokenFormed) {
            if (
                ActiveTokenKind != EVAL_TOKEN_KIND_SYMBOL || 
                Index == ActiveTokenStartIndex + 1
            ) {
                EvalToken Token = {
                    ActiveTokenKind,
                    Rng(ActiveTokenStartIndex, Index + TokenTerminalIndexPad)
                };

                ListPush(Scratch.MemPool, &Tokens, 256, &Token);
            } else if (
                Index == ActiveTokenStartIndex + 2 &&
                Text.Str[ActiveTokenStartIndex] == '-' &&
                Text.Str[ActiveTokenStartIndex + 1] == '-'
            ) {
                break;
            } else {
                u64 Advance2 = 0;

                for (u64 I = ActiveTokenStartIndex; I < Index; I += Advance2) {
                    Advance2 = 1;

                    for (
                        u64 MulticharSymbolIndex = 0;
                        MulticharSymbolIndex < ARRAY_COUNT(EVAL_MULTI_CHAR_SYMBOL_STRINGS);
                        ++MulticharSymbolIndex
                    ) {
                        Str8 MulticharSymbolString = EVAL_MULTI_CHAR_SYMBOL_STRINGS[MulticharSymbolIndex];
                        Str8 PartToken = StrSub(Text, Rng(I, I + MulticharSymbolString.Size));

                        if (StrMatch(PartToken, MulticharSymbolString, 0)) {
                            Advance2 = MulticharSymbolString.Size;
                            break;
                        }
                    }

                    EvalToken Token = {
                        ActiveTokenKind,
                        Rng(I, I + Advance2)
                    };

                    ListPush(Scratch.MemPool, &Tokens, 256, &Token);
                }
            }

            ActiveTokenKind = EVAL_TOKEN_KIND_NULL;
        }
    }

    EvalTokenArray Result = EvalTokenArrayFromChunkList(MemPool, &Tokens);

    ReleaseScratch(Scratch);

    return Result;
}

EvalTokenArray 
EvalTokenArrayAlloc(EvalToken* Head, EvalToken* Tail)
{
    EvalTokenArray Result = {
        Head,
        (u64) (Tail - Head)
    };

    return Result;
}

EvalExpr* 
EvalPushExpr(Arena* MemPool, EvalExprKind Kind, r1u64 Range)
{
    EvalExpr* Result = ArenaPushArrayZero(MemPool, EvalExpr, 1);

    Result->Head = EMPTY_EVAL_EXPR_VALUE;
    Result->Tail = EMPTY_EVAL_EXPR_VALUE;
    Result->Next = EMPTY_EVAL_EXPR_VALUE;
    Result->Prev = EMPTY_EVAL_EXPR_VALUE;
    Result->Ref = EMPTY_EVAL_EXPR_VALUE;
    Result->Range = Range;
    Result->Kind = Kind;

    return Result;
}

void 
EvalExprPushChild(EvalExpr* Parent, EvalExpr* Child)
{
    DLL_PUSH_BACK_EX(
        EMPTY_EVAL_EXPR_VALUE,
        Parent->Head,
        Parent->Tail,
        Child,
        Next,
        Prev
    );
}

EvalExpr* 
EvalExprRef(Arena* MemPool, EvalExpr* Ref)
{
    EvalExpr* Result = EvalPushExpr(MemPool, EVAL_EXPR_KIND_REF, Ref->Range);

    Result->Ref = Ref;

    return Result;
}

EvalExpr* 
EvalExprCpy(Arena* MemPool, EvalExpr* Expr)
{
    EvalExpr* Result = EMPTY_EVAL_EXPR_VALUE;
    TempArena Scratch = GetScratch(&MemPool, 1);

    if (Expr != EMPTY_EVAL_EXPR_VALUE) {
        struct Task {
            Task*     Next;
            EvalExpr* DstParent;
            EvalExpr* Src;
            b32       IsRef;
            b32       IsSib;
        };

        Task StartTask = {
            NULL,
            EMPTY_EVAL_EXPR_VALUE,
            Expr
        };
        Task* HeadTask = &StartTask;
        Task* TailTask = HeadTask;

        for (Task* T = HeadTask; T; T = T->Next) {
            EvalExpr* Dst = EvalPushExpr(MemPool, T->Src->Kind, T->Src->Range);

            Dst->Mode = T->Src->Mode;
            Dst->Space = T->Src->Space;
            Dst->TypeKey = T->Src->TypeKey;
            Dst->Value = T->Src->Value;
            Dst->String = ArenaPushStrCpy(MemPool, T->Src->String);
            Dst->ByteCode = ArenaPushStrCpy(MemPool, T->Src->ByteCode);
            Dst->Disambiguator = ArenaPushStrCpy(MemPool, T->Src->Disambiguator);
            Dst->Qualifier = ArenaPushStrCpy(MemPool, T->Src->Qualifier);

            if (T->DstParent == EMPTY_EVAL_EXPR_VALUE) {
                Result = Dst;
            } else if (T->IsRef) {
                T->DstParent->Ref = Dst;
            } else if (T->IsSib) {
                T->DstParent->Next = Dst;
                Dst->Prev = T->DstParent;
            } else {
                EvalExprPushChild(T->DstParent, Dst);
            }

            if (T->Src->Next != EMPTY_EVAL_EXPR_VALUE) {
                Task* _Task = ArenaPushArrayZero(Scratch.MemPool, Task, 1);

                _Task->DstParent = Dst;
                _Task->Src = T->Src->Next;
                _Task->IsSib = TRUE;
                SLL_QUEUE_PUSH(HeadTask, TailTask, _Task);
            }

            if (T->Src->Ref != EMPTY_EVAL_EXPR_VALUE) {
                Task* _Task = ArenaPushArrayZero(Scratch.MemPool, Task, 1);

                _Task->DstParent = Dst;
                _Task->Src = T->Src->Ref;
                _Task->IsRef = TRUE;
                SLL_QUEUE_PUSH(HeadTask, TailTask, _Task);
            }

            for (
                EvalExpr* SrcChild = T->Src->Head; 
                SrcChild != EMPTY_EVAL_EXPR_VALUE; 
                SrcChild = SrcChild->Next
            ) {
                Task* _Task = ArenaPushArrayZero(Scratch.MemPool, Task, 1);

                _Task->DstParent = Dst;
                _Task->Src = SrcChild;
                SLL_QUEUE_PUSH(HeadTask, TailTask, _Task);
            }
        }
    }

    ReleaseScratch(Scratch);

    return Result;
}

void 
ListPush(Arena* MemPool, EvalExprList* List, EvalExpr* Expr)
{
    EvalExprNode* Node = ArenaPushArrayZero(MemPool, EvalExprNode, 1);

    Node->V = Expr;
    SLL_QUEUE_PUSH(List->Head, List->Tail, Node);
    ++List->Count;
}

void 
EvalAppendStrsFromExpr(
    Arena* MemPool, 
    EvalExpr* Expr, 
    Str8 ParentExprString, 
    OUT Str8List* List
) {
    switch (Expr->Kind) {
        default: {
            EvalOpInfo* OpInfo = &EVAL_EXPR_KIND_OP_INFO_TABLE[Expr->Kind];
            Str8 Seps[] = {
                OpInfo->Pre,
                OpInfo->Sep,
                OpInfo->Post
            };
            u64 SepIndex = 0;

            for (
                EvalExpr* Child = Expr->Head; 
                ; 
                Child = Child->Next
            ) {
                if (SepIndex == ARRAY_COUNT(Seps) - 1 && Child != EMPTY_EVAL_EXPR_VALUE) {
                    ListPush(MemPool, List, OpInfo->Chain);
                } else {
                    ListPush(MemPool, List, Seps[SepIndex]);
                    ++SepIndex;
                }

                if (Child == EMPTY_EVAL_EXPR_VALUE)
                    break;

                EvalOpInfo* ChildOpInfo = &EVAL_EXPR_KIND_OP_INFO_TABLE[Child->Kind];
                b32 NeedParens = (ChildOpInfo->Precedence > OpInfo->Precedence);

                if (NeedParens)
                    ListPush(MemPool, List, "("_s8);
            }
        } break;

        case EVAL_EXPR_KIND_LEAF_BYTECODE:
        case EVAL_EXPR_KIND_LEAF_IDENT: {
            if (Expr->Qualifier.Size)
                ListPushFmt(MemPool, List, "%S:", PRINT_STR(Expr->Qualifier));

            if (StrMatch(Expr->String, "$"_s8, 0) && ParentExprString.Size)
                ListPush(MemPool, List, ParentExprString);
            else
                ListPush(MemPool, List, Expr->String);

            if (Expr->Disambiguator.Size)
                ListPushFmt(MemPool, List, "#%S", PRINT_STR(Expr->Disambiguator));
        } break;

        case EVAL_EXPR_KIND_LEAF_STR_LIT: {
            ListPushFmt(MemPool, List, "\"%S\"", PRINT_STR(Expr->String));
        } break;

        case EVAL_EXPR_KIND_LEAF_U64: {
            ListPushFmt(MemPool, List, "%llu", Expr->Value.U64);
        } break;

        case EVAL_EXPR_KIND_LEAF_FILE_PATH: {
            ListPushFmt(
                MemPool, 
                List, 
                "file:\"%S\"", 
                PRINT_STR(
                    EscapedFromRawStr8(
                        MemPool, 
                        Expr->String
                    )
                )
            );
        } break;

        case EVAL_EXPR_KIND_LEAF_F64: {
            ListPushFmt(MemPool, List, "%lf", Expr->Value.F64);
        } break;

        case EVAL_EXPR_KIND_LEAF_F32: {
            ListPushFmt(MemPool, List, "%f", Expr->Value.F32);
        } break;

        case EVAL_EXPR_KIND_TYPE_IDENT: {
            Str8 TypeString = EvalTypeStrFromKey(MemPool, Expr->TypeKey);

            ListPush(MemPool, List, TypeString);
        } break;

        case EVAL_EXPR_KIND_REF: {
            EvalAppendStrsFromExpr(MemPool, Expr->Ref, ParentExprString, List);
        } break;
    }
}

Str8 
EvalStrFromExpr(Arena* MemPool, EvalExpr* Expr, Str8 ParentExprString)
{
    Str8List Strings = {};

    EvalAppendStrsFromExpr(MemPool, Expr, ParentExprString, &Strings);

    Str8 Result = StrListJoin(MemPool, &Strings, NULL);

    return Result;
}

#define EVAL_BUILTIN_TYPE_X_LIST \
    X("uint8", U8)       \
    X("uint8_t", U8)     \
    X("uchar", UCHAR8)   \
    X("uchar8", UCHAR8)  \
    X("uint16", U16)     \
    X("uint16_t", U16)   \
    X("uchar16", UCHAR16)\
    X("uint32", U32)     \
    X("uint32_t", U32)   \
    X("uchar32", UCHAR32)\
    X("uint64", U64)     \
    X("uint64_t", U64)   \
    X("uint128", U128)   \
    X("uint128_t", U128) \
    X("uint256", U256)   \
    X("uint256_t", U256) \
    X("uint512", U512)   \
    X("uint512_t", U512) \
    X("int8", I8)        \
    X("int8_t", I8)      \
    X("char", CHAR8)     \
    X("char8", CHAR8)    \
    X("int16", I16)      \
    X("int16_t", I16)    \
    X("char16", CHAR16)  \
    X("int32", I32)      \
    X("int32_t", I32)    \
    X("char32", CHAR32)  \
    X("int64", I64)      \
    X("int64_t", I64)    \
    X("int128", I128)    \
    X("int128_t", I128)  \
    X("int256", I256)    \
    X("int256_t", I256)  \
    X("int512", I512)    \
    X("int512_t", I512)  \
    X("bool", BOOL)      \
    X("float", F32)      \
    X("float32", F32)    \
    X("double", F64)     \
    X("float64", F64)

#define X(Y, Z) else if (StrMatch(Name, Str8Lit(Y), 0)) { Result = EvalTypeKeyBasic(EVAL_TYPE_KIND_##Z); }

EvalTypeKey 
EvalLeafBuiltinTypeKeyFromName(Str8 Name)
{
    EvalTypeKey Result = {};

    if (FALSE) {}
    EVAL_BUILTIN_TYPE_X_LIST

    return Result;

}

#undef X

EvalTypeKey 
EvalLeafTypeKeyFromName(Str8 Name)
{
    EvalTypeKey Key = EvalLeafBuiltinTypeKeyFromName(Name);
    EvalBaseContext* const __EvalBaseCtx = GetTLS()->EVAL_BASE_CTX;

    if (Key != EMPTY_EVAL_TYPE_KEY_VALUE) {
        LogError("EvalLeafTypeKeyFromName called with non empty key"_s8);
    }

    return Key;
}

EvalTypeKey 
EvalTypeKeyFromExpr(EvalExpr* Expr)
{
    EvalTypeKey Result = {};
    EvalExprKind Kind = Expr->Kind;

    switch (Kind) {
        default: {} break;

        case EVAL_EXPR_KIND_LEAF_IDENT: {
            Result = EvalLeafTypeKeyFromName(Expr->String);
        } break;

        case EVAL_EXPR_KIND_TYPE_IDENT: {
            Result = Expr->TypeKey;
        } break;

        case EVAL_EXPR_KIND_ARRAY: {
            EvalExpr* ChildExpr = Expr->Head;
            EvalTypeKey DirectTypeKey = EvalTypeKeyFromExpr(ChildExpr);

            Result = EvalTypeKeyConstructArray(DirectTypeKey, Expr->Value.U64, 0);
        } break;
    }

    return Result;
}

EvalParse 
EvalPushParseFromStr(
    Arena* MemPool, 
    Str8 Text, 
    EvalTokenArray Tokens, 
    i64 MaxPrecedence, 
    u64 MaxChainCount
) {
    EvalParse Result = {
        Tokens,
        NULL,
        EMPTY_EVAL_EXPR_VALUE,
        EMPTY_EVAL_EXPR_VALUE
    };

    PROFILE(EvalPushParseFromStrTimer) {
        TempArena Scratch = GetScratch(&MemPool, 1);
        EvalToken* It = Tokens.V;
        EvalToken* ItEnd = Tokens.V + Tokens.Count;

        for (u64 ChainCount = 0; It < ItEnd && ChainCount < MaxChainCount; ) {
            {
                EvalToken Token = EvalTokenAtX(It, &Tokens);
                Str8 TokenString = StrSub(Text, Token.Range);

                if (
                    Token.Kind == EVAL_TOKEN_KIND_SYMBOL &&
                    (
                        StrMatch(TokenString, ")"_s8, 0) ||
                        StrMatch(TokenString, "]"_s8, 0) ||
                        StrMatch(TokenString, ":"_s8, 0) ||
                        StrMatch(TokenString, "?"_s8, 0)
                    )
                ) {
                    break;
                }
            }

            for ( ; It < ItEnd; ) {
                EvalToken Token = EvalTokenAtX(It, &Tokens);
                Str8 TokenString = StrSub(Text, Token.Range);

                if (
                    Token.Kind == EVAL_TOKEN_KIND_SYMBOL &&
                    (
                        StrMatch(TokenString, ","_s8, 0) ||
                        StrMatch(TokenString, ";"_s8, 0)
                    )
                ) {
                    ++It;
                } else {
                    break;
                }
            }

            struct PrefixUnaryTask {
                PrefixUnaryTask* Next;
                EvalExprKind     Kind;
                r1u64            Range;
                EvalExpr*        CastTypeExpr;
            };

            PrefixUnaryTask* HeadPrefixTask = NULL;
            PrefixUnaryTask* TailPrefixTask = NULL;
            EvalExpr* Atom = EMPTY_EVAL_EXPR_VALUE;
            b32 AtomIsMaybeCast = FALSE;

            for (b32 IsDone = FALSE; !IsDone && It < ItEnd; ) {
                EvalToken Token = EvalTokenAtX(It, &Tokens);
                Str8 TokenString = StrSub(Text, Token.Range);
                i64 PrefixUnaryPrecedence = 0;
                EvalExprKind PrefixUnaryKind = EVAL_EXPR_KIND_NULL;
                EvalExpr* PrefixUnaryCastExpr = EMPTY_EVAL_EXPR_VALUE;

                for (
                    EvalExprKind K = 1;
                    K < EVAL_EXPR_KIND_COUNT;
                    ++K
                ) {
                    EvalOpInfo* OpInfo = &EVAL_EXPR_KIND_OP_INFO_TABLE[K];

                    if (
                        OpInfo->Kind == EVAL_OP_KIND_UNARY_PREFIX && 
                        StrMatch(
                            StrSkipChopWhitespace(OpInfo->Pre), 
                            TokenString, 
                            0
                        )
                    ) {
                        PrefixUnaryPrecedence = OpInfo->Precedence;
                        PrefixUnaryKind = K;
                        break;
                    }
                }

                if (PrefixUnaryPrecedence && AtomIsMaybeCast)
                    break;

                if (PrefixUnaryPrecedence) {
                    PrefixUnaryTask* PrefixTask = ArenaPushArrayZero(
                        Scratch.MemPool,
                        PrefixUnaryTask,
                        1
                    );

                    PrefixTask->Kind = PrefixUnaryKind;
                    PrefixTask->Range = Token.Range;
                    PrefixTask->CastTypeExpr = PrefixUnaryCastExpr;
                    SLL_QUEUE_PUSH(HeadPrefixTask, TailPrefixTask, PrefixTask);
                    ++It;
                }

                if (Atom == EMPTY_EVAL_EXPR_VALUE || AtomIsMaybeCast) {
                    b32 GotNewAtom = FALSE;
                    EvalExpr* MaybeCast = AtomIsMaybeCast ? Atom : EMPTY_EVAL_EXPR_VALUE;

                    AtomIsMaybeCast = FALSE;

                    Str8 ResolutionQualifier = {};

                    {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (
                            Token.Kind == EVAL_TOKEN_KIND_IDENT || 
                            Token.Kind == EVAL_TOKEN_KIND_STR_LIT
                        ) {
                            u64 TokenExtCount = 0;
                            EvalToken DotMaybeToken = EvalTokenAtX(It + 1, &Tokens);
                            Str8 DotMaybeTokenString = StrSub(Text, DotMaybeToken.Range);
                            EvalToken ExtMaybeToken = EvalTokenAtX(It + 2, &Tokens);
                            Str8 ExtMaybeTokenString = StrSub(Text, ExtMaybeToken.Range);

                            if (
                                DotMaybeToken.Kind == EVAL_TOKEN_KIND_SYMBOL &&
                                ExtMaybeToken.Kind == EVAL_TOKEN_KIND_IDENT &&
                                StrMatch(DotMaybeTokenString, "."_s8, 0)
                            ) {
                                TokenExtCount = 2;
                            }

                            EvalToken NextToken = EvalTokenAtX(
                                It + 1 + TokenExtCount, 
                                &Tokens
                            );
                            Str8 NextTokenString = StrSub(Text, NextToken.Range);

                            if (
                                NextToken.Kind == EVAL_TOKEN_KIND_SYMBOL &&
                                (
                                    StrMatch(NextTokenString, ":"_s8, 0) ||
                                    StrMatch(NextTokenString, "!"_s8, 0)
                                )
                            ) {
                                It += 2 + TokenExtCount;
                                ResolutionQualifier = StrSub(
                                    Text,
                                    Union(
                                        Token.Range,
                                        Rng(Token.Range.Min, NextToken.Range.Min)
                                    )
                                );
                            }
                        }
                    }

                    if (!GotNewAtom) {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (
                            Token.Kind == EVAL_TOKEN_KIND_SYMBOL && 
                            StrMatch(TokenString, "("_s8, 0)
                        ) {
                            ++It;

                            EvalParse NestedParse = EvalPushParseFromStr(
                                MemPool,
                                Text,
                                EvalTokenArrayAlloc(It, ItEnd),
                                EVAL_MAX_PRECEDENCE,
                                1
                            );

                            ListCat(&Result.Msgs, &NestedParse.Msgs);
                            Atom = NestedParse.Expr;
                            It = NestedParse.PrevToken;
                            AtomIsMaybeCast = TRUE;
                            GotNewAtom = TRUE;

                            EvalToken CloseParenMaybe = EvalTokenAtX(It, &Tokens);
                            Str8 CloseParenMaybeString = StrSub(Text, CloseParenMaybe.Range);

                            if (
                                CloseParenMaybe.Kind != EVAL_TOKEN_KIND_SYMBOL || 
                                !StrMatch(CloseParenMaybeString, ")"_s8, 0)
                            ) {
                                EvalMessage(
                                    MemPool, 
                                    &Result.Msgs, 
                                    EVAL_MSG_KIND_MALFORMED_INPUT, 
                                    Token.Range, 
                                    "Missing `)`."_s8
                                );
                            } else {
                                ++It;
                            }
                        }
                    }

                    if (Atom == EMPTY_EVAL_EXPR_VALUE && !GotNewAtom) {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (
                            Token.Kind == EVAL_TOKEN_KIND_SYMBOL && 
                            StrMatch(TokenString, "["_s8, 0)
                        ) {
                            ++It;

                            EvalParse NestedParse = EvalPushParseFromStr(
                                MemPool,
                                Text,
                                EvalTokenArrayAlloc(It, ItEnd),
                                EVAL_MAX_PRECEDENCE,
                                1
                            );

                            ListCat(&Result.Msgs, &NestedParse.Msgs);
                            Atom = NestedParse.Expr;
                            It = NestedParse.PrevToken;
                            GotNewAtom = TRUE;

                            if (NestedParse.Expr == EMPTY_EVAL_EXPR_VALUE) {
                                EvalMessage(
                                    MemPool,
                                    &Result.Msgs,
                                    EVAL_MSG_KIND_MALFORMED_INPUT,
                                    Token.Range,
                                    "Expected expression following `[`."_s8
                                );
                            }

                            EvalToken CloseParenMaybe = EvalTokenAtX(It, &Tokens);
                            Str8 CloseParenMaybeString = StrSub(Text, CloseParenMaybe.Range);

                            if (
                                CloseParenMaybe.Kind != EVAL_TOKEN_KIND_SYMBOL || 
                                !StrMatch(CloseParenMaybeString, "]"_s8, 0)
                            ) {
                                EvalMessage(
                                    MemPool,
                                    &Result.Msgs,
                                    EVAL_MSG_KIND_MALFORMED_INPUT,
                                    Token.Range,
                                    "Missing `]`."_s8
                                );
                            } else {
                                ++It;
                            }
                        }
                    }

                    if (!GotNewAtom) {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (Token.Kind == EVAL_TOKEN_KIND_IDENT) {
                            Str8 IdentString = TokenString;

                            if (
                                IdentString.Size >= 2 && 
                                IdentString.Str[0] == '`' && 
                                IdentString.Str[IdentString.Size - 1] == '`'
                            ) {
                                IdentString = StrSkip(StrChop(IdentString, 1), 1);
                            }

                            Atom = EvalPushExpr(
                                MemPool, 
                                EVAL_EXPR_KIND_LEAF_IDENT, 
                                Token.Range
                            );
                            Atom->String = IdentString;
                            ++It;
                            GotNewAtom = TRUE;
                        }
                    }

                    if (!GotNewAtom) {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (Token.Kind == EVAL_TOKEN_KIND_NUMERIC) {
                            u64 DotPosition = StrFindSubStr(TokenString, "."_s8, 0, 0);

                            ++It;

                            if (DotPosition == TokenString.Size) {
                                u64 Value = U64FromStr(TokenString);
                                
                                Atom = EvalPushExpr(
                                    MemPool, 
                                    EVAL_EXPR_KIND_LEAF_U64, 
                                    Token.Range
                                );
                                Atom->Value.U64 = Value;
                            }

                            if (DotPosition < TokenString.Size) {
                                f64 Value = F64FromStr(TokenString);
                                u64 FPosition = StrFindSubStr(
                                    TokenString,
                                    "f"_s8,
                                    0,
                                    STR_MATCH_ALL_CASES
                                );

                                if (FPosition < TokenString.Size) {
                                    Atom = EvalPushExpr(
                                        MemPool,
                                        EVAL_EXPR_KIND_LEAF_F32,
                                        Token.Range
                                    );
                                    Atom->Value.F32 = Value;
                                } else {
                                    Atom = EvalPushExpr(
                                        MemPool,
                                        EVAL_EXPR_KIND_LEAF_F64,
                                        Token.Range
                                    );
                                    Atom->Value.F64 = Value;
                                }
                            }

                            GotNewAtom = TRUE;
                        }
                    }

                    if (!GotNewAtom) {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (Token.Kind == EVAL_TOKEN_KIND_CHAR_LIT) {
                            ++It;

                            if (
                                TokenString.Size > 1 &&
                                TokenString.Str[0] == '\'' &&
                                TokenString.Str[1] != '\''
                            ) {
                                Str8 CharLitEscape = StrSkip(StrChop(TokenString, 1), 1);
                                Str8 CharLitRaw = RawFromEscapedStr8(Scratch.MemPool, CharLitEscape);
                                u8 CharValue = CharLitRaw.Size ? CharLitRaw.Str[0] : 0;

                                Atom = EvalPushExpr(
                                    MemPool,
                                    EVAL_EXPR_KIND_LEAF_U64,
                                    Token.Range
                                );
                                GotNewAtom = TRUE;
                            } else {
                                EvalMessage(
                                    MemPool,
                                    &Result.Msgs,
                                    EVAL_MSG_KIND_MALFORMED_INPUT,
                                    Token.Range,
                                    "Malformed character literal."_s8
                                );
                            }
                        }
                    }

                    if (!GotNewAtom) {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (
                            Token.Kind == EVAL_TOKEN_KIND_STR_LIT &&
                            (
                                StrMatch(ResolutionQualifier, "file"_s8, 0) ||
                                StrMatch(ResolutionQualifier, "folder"_s8, 0)
                            )
                        ) {
                            Str8 StringValueEscape = StrChop(StrSkip(TokenString, 1), 1);
                            Str8 StringValueRaw = RawFromEscapedStr8(MemPool, StringValueEscape);

                            Atom = EvalPushExpr(
                                MemPool,
                                EVAL_EXPR_KIND_LEAF_FILE_PATH,
                                Token.Range
                            );
                            Atom->String = StringValueRaw;
                            ++It;
                            GotNewAtom = TRUE;
                        }
                    }

                    if (!GotNewAtom) {
                        EvalToken Token = EvalTokenAtX(It, &Tokens);
                        Str8 TokenString = StrSub(Text, Token.Range);

                        if (Token.Kind == EVAL_TOKEN_KIND_STR_LIT) {
                            Str8 StringValueEscape = StrChop(StrSkip(TokenString, 1), 1);
                            Str8 StringValueRaw = RawFromEscapedStr8(MemPool, StringValueEscape);

                            Atom = EvalPushExpr(
                                MemPool,
                                EVAL_EXPR_KIND_LEAF_STR_LIT,
                                Token.Range
                            );
                            Atom->String = StringValueRaw;
                            ++It;
                            GotNewAtom = TRUE;
                        }
                    }

                    if (Atom != EMPTY_EVAL_EXPR_VALUE && ResolutionQualifier.Size)
                        Atom->Qualifier = ResolutionQualifier;

                    if (GotNewAtom && MaybeCast != EMPTY_EVAL_EXPR_VALUE) {
                        PrefixUnaryTask* PrefixTask = ArenaPushArrayZero(
                            Scratch.MemPool,
                            PrefixUnaryTask,
                            1
                        );

                        PrefixTask->Kind = EVAL_EXPR_KIND_CAST;
                        PrefixTask->Range = MaybeCast->Range;
                        PrefixTask->CastTypeExpr = MaybeCast;
                        SLL_QUEUE_PUSH(
                            HeadPrefixTask,
                            TailPrefixTask,
                            PrefixTask
                        );
                    }
                }

                IsDone = (!AtomIsMaybeCast || Atom == EMPTY_EVAL_EXPR_VALUE);
            }

            if (Atom != EMPTY_EVAL_EXPR_VALUE) {
                for ( ; It < ItEnd; ) {
                    EvalToken Token = EvalTokenAtX(It, &Tokens);
                    Str8 TokenString = StrSub(Text, Token.Range);
                    b32 IsPostfixUnary = FALSE;

                    if (
                        MaxPrecedence >= 1 && 
                        Token.Kind == EVAL_TOKEN_KIND_SYMBOL &&
                        (
                            StrMatch(TokenString, "."_s8, 0) ||
                            StrMatch(TokenString, "->"_s8, 0)
                        )
                    ) {
                        IsPostfixUnary = TRUE;
                        ++It;

                        EvalToken MemberNameMaybe = EvalTokenAtX(It, &Tokens);
                        Str8 MemberNameMaybeString = StrSub(Text, MemberNameMaybe.Range);
                        b32 MemberNameIsGood = (MemberNameMaybe.Kind == EVAL_TOKEN_KIND_IDENT);
                        EvalExpr* LHS = Atom;
                        EvalExpr* RHS = EMPTY_EVAL_EXPR_VALUE;

                        if (MemberNameIsGood) {
                            RHS = EvalPushExpr(
                                MemPool,
                                EVAL_EXPR_KIND_LEAF_IDENT,
                                MemberNameMaybe.Range
                            );
                            RHS->String = MemberNameMaybeString;
                        }

                        Atom = EvalPushExpr(
                            MemPool,
                            EVAL_EXPR_KIND_MEMBER_ACCESS,
                            Token.Range
                        );
                        EvalExprPushChild(Atom, LHS);

                        if (MemberNameIsGood)
                            EvalExprPushChild(Atom, RHS);

                        if (MemberNameIsGood) {
                            ++It;
                        } else {
                            EvalMessage(
                                MemPool,
                                &Result.Msgs,
                                EVAL_MSG_KIND_MALFORMED_INPUT,
                                Token.Range,
                                "Missing member name after `%S`.",
                                PRINT_STR(TokenString)
                            );
                        }
                    }

                    if (
                        Token.Kind == EVAL_TOKEN_KIND_SYMBOL && 
                        StrMatch(TokenString, "["_s8, 0)
                    ) {
                        IsPostfixUnary = TRUE;
                        ++It;

                        EvalParse IndexExprParse = EvalPushParseFromStr(
                            MemPool,
                            Text,
                            EvalTokenArrayAlloc(It, ItEnd),
                            EVAL_MAX_PRECEDENCE,
                            1
                        );

                        ListCat(&Result.Msgs, &IndexExprParse.Msgs);
                        It = IndexExprParse.PrevToken;

                        if (IndexExprParse.Expr != EMPTY_EVAL_EXPR_VALUE) {
                            EvalExpr* ArrayExpr = Atom;
                            EvalExpr* IndexExpr = IndexExprParse.Expr;

                            Atom = EvalPushExpr(
                                MemPool,
                                EVAL_EXPR_KIND_ARRAY_INDEX,
                                Token.Range
                            );
                            EvalExprPushChild(Atom, ArrayExpr);
                            EvalExprPushChild(Atom, IndexExpr);
                        }

                        {
                            EvalToken CloseBraceMaybe = EvalTokenAtX(It, &Tokens);
                            Str8 CloseBraceMaybeString = StrSub(Text, CloseBraceMaybe.Range);

                            if (
                                CloseBraceMaybe.Kind != EVAL_TOKEN_KIND_SYMBOL || 
                                !StrMatch(CloseBraceMaybeString, "]"_s8, 0)
                            ) {
                                EvalMessage(
                                    MemPool,
                                    &Result.Msgs,
                                    EVAL_MSG_KIND_MALFORMED_INPUT,
                                    Token.Range,
                                    "Unclosed `[`."_s8
                                );
                            } else {
                                ++It;
                            }
                        }
                    }

                    if (
                        Token.Kind == EVAL_TOKEN_KIND_SYMBOL &&
                        StrMatch(TokenString, "("_s8, 0)
                    ) {
                        IsPostfixUnary = TRUE;
                        ++It;

                        EvalExpr* CalleeExpr = Atom;
                        EvalExpr* CallExpr = EvalPushExpr(
                            MemPool,
                            EVAL_EXPR_KIND_CALL,
                            Token.Range
                        );

                        CallExpr->String = CalleeExpr->String;
                        EvalExprPushChild(CallExpr, CalleeExpr);

                        EvalParse ArgsParse = EvalPushParseFromStr(
                            MemPool,
                            Text,
                            EvalTokenArrayAlloc(It, ItEnd),
                            EVAL_MAX_PRECEDENCE,
                            U64_MAX
                        );

                        ListCat(&Result.Msgs, &ArgsParse.Msgs);
                        It = ArgsParse.PrevToken;

                        if (ArgsParse.Expr != EMPTY_EVAL_EXPR_VALUE) {
                            CallExpr->Tail->Next = ArgsParse.Expr;
                            ArgsParse.Expr->Prev = CallExpr->Tail;

                            for (
                                EvalExpr* Arg = ArgsParse.Expr;
                                Arg != EMPTY_EVAL_EXPR_VALUE;
                                Arg = Arg->Next
                            ) {
                                CallExpr->Tail = Arg;
                            }
                        }

                        Atom = CallExpr;

                        {
                            EvalToken CloseParenMaybe = EvalTokenAtX(It, &Tokens);
                            Str8 CloseParenMaybeString = StrSub(
                                Text, 
                                CloseParenMaybe.Range
                            );

                            if (
                                CloseParenMaybe.Kind != EVAL_TOKEN_KIND_SYMBOL ||
                                !StrMatch(CloseParenMaybeString, ")"_s8, 0)
                            ) {
                                EvalMessage(
                                    MemPool,
                                    &Result.Msgs,
                                    EVAL_MSG_KIND_MALFORMED_INPUT,
                                    Token.Range,
                                    "Unclosed `(`."_s8
                                );
                                CallExpr->Range.Max = Text.Size;
                            } else {
                                CallExpr->Range = Union(
                                    CallExpr->Range,
                                    CloseParenMaybe.Range
                                );
                                ++It;
                            }
                        }
                    }

                    if (!IsPostfixUnary)
                        break;
                }

                if (Atom != EMPTY_EVAL_EXPR_VALUE) {
                    for (
                        PrefixUnaryTask* PrefixTask = HeadPrefixTask;
                        PrefixTask;
                        PrefixTask = PrefixTask->Next
                    ) {
                        if (PrefixTask->Kind == EVAL_EXPR_KIND_CAST) {
                            EvalExpr* RHS = Atom;

                            Atom = EvalPushExpr(
                                MemPool,
                                PrefixTask->Kind,
                                PrefixTask->Range
                            );
                            EvalExprPushChild(Atom, PrefixTask->CastTypeExpr);
                            EvalExprPushChild(Atom, RHS);
                        } else {
                            EvalExpr* RHS = Atom;

                            Atom = EvalPushExpr(
                                MemPool,
                                PrefixTask->Kind,
                                PrefixTask->Range
                            );
                            EvalExprPushChild(Atom, RHS);
                        }
                    }
                }
            } else if (HeadPrefixTask) {
                EvalMessage(
                    MemPool,
                    &Result.Msgs,
                    EVAL_MSG_KIND_MALFORMED_INPUT,
                    TailPrefixTask->Range,
                    "Missing expression."_s8
                );
            }

            if (Atom != EMPTY_EVAL_EXPR_VALUE) {
                for ( ; It < ItEnd; ) {
                    EvalToken* StartIt = It;
                    EvalToken Token = EvalTokenAtX(It, &Tokens);
                    Str8 TokenString = StrSub(Text, Token.Range);

                    {
                        i64 BinaryPrecedence = 0;
                        EvalExprKind BinaryKind = EVAL_EXPR_KIND_NULL;

                        for (
                            EvalExprKind K = 1;
                            K < EVAL_EXPR_KIND_COUNT;
                            ++K
                        ) {
                            EvalOpInfo* OpInfo = &EVAL_EXPR_KIND_OP_INFO_TABLE[K];

                            if (
                                OpInfo->Kind == EVAL_OP_KIND_BINARY &&
                                StrMatch(
                                    StrSkipChopWhitespace(OpInfo->Sep), 
                                    TokenString, 
                                    0
                                )
                            ) {
                                BinaryPrecedence = OpInfo->Precedence;
                                BinaryKind = K;
                                break;
                            }
                        }

                        if (BinaryPrecedence && BinaryPrecedence <= MaxPrecedence) {
                            EvalParse RHSExprParse = EvalPushParseFromStr(
                                MemPool,
                                Text,
                                EvalTokenArrayAlloc(It + 1, ItEnd),
                                BinaryPrecedence - 1,
                                1
                            );

                            ListCat(&Result.Msgs, &RHSExprParse.Msgs);

                            EvalExpr* RHS = RHSExprParse.Expr;

                            It = RHSExprParse.PrevToken;

                            if (
                                RHS == EMPTY_EVAL_EXPR_VALUE && 
                                BinaryKind == EVAL_EXPR_KIND_MUL
                            ) {
                                EvalExpr* PTee = Atom;

                                Atom = EvalPushExpr(
                                    MemPool,
                                    EVAL_EXPR_KIND_TYPE_PTR,
                                    Token.Range
                                );
                                EvalExprPushChild(Atom, PTee);
                            } else if (RHS == EMPTY_EVAL_EXPR_VALUE) {
                                EvalMessage(
                                    MemPool,
                                    &Result.Msgs,
                                    EVAL_MSG_KIND_MALFORMED_INPUT,
                                    Token.Range,
                                    "Missing right-hand-side of `%S`",
                                    PRINT_STR(TokenString)
                                );
                            } else {
                                EvalExpr* LHS = Atom;

                                Atom = EvalPushExpr(
                                    MemPool,
                                    BinaryKind,
                                    Token.Range
                                );
                                EvalExprPushChild(Atom, LHS);
                                EvalExprPushChild(Atom, RHS);
                            }
                        }
                    }

                    if (It == StartIt)
                        break;
                }
            }

            if (Atom != EMPTY_EVAL_EXPR_VALUE) {
                DLL_PUSH_BACK_EX(
                    EMPTY_EVAL_EXPR_VALUE,
                    Result.Expr,
                    Result.PrevExpr,
                    Atom,
                    Next,
                    Prev
                );
                ++ChainCount;
            } else {
                break;
            }
        }

        Result.PrevToken = It;
        ReleaseScratch(Scratch);
    }

    return Result;
}

EvalParse 
EvalPushParseFromStr(Arena* MemPool, Str8 Text)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    EvalTokenArray Tokens = EvalTokenArrayFromText(MemPool, Text);
    EvalParse Parse = EvalPushParseFromStr(
        MemPool,
        Text,
        Tokens,
        EVAL_MAX_PRECEDENCE,
        U64_MAX
    );

    ReleaseScratch(Scratch);

    return Parse;
}

EvalEvalConversionKind 
EvalEvalConvKindFromTypeGroups(EvalEvalTypeGroup From, EvalEvalTypeGroup To)
{
    EvalEvalConversionKind Result = EVAL_EVAL_TYPE_GROUP_CONV_KIND_MATRIX[From].DstTypeGroups[To];

    return Result;
}

i32 
EvalEvalOpTypeGroupAreCompatible(EvalEvalOp Op, EvalEvalTypeGroup Group)
{
    i32 Result = FALSE;

    switch (Op) {
        case EVAL_EVAL_OP_NEG:
        case EVAL_EVAL_OP_ADD:
        case EVAL_EVAL_OP_SUB:
        case EVAL_EVAL_OP_MUL:
        case EVAL_EVAL_OP_DIV:
        case EVAL_EVAL_OP_EQ_EQ:
        case EVAL_EVAL_OP_NOT_EQ:
        case EVAL_EVAL_OP_LESS_EQ:
        case EVAL_EVAL_OP_GREATER_EQ:
        case EVAL_EVAL_OP_LESS:
        case EVAL_EVAL_OP_GREATER: {
            if (Group != EVAL_EVAL_TYPE_GROUP_OTHER)
                Result = TRUE;
        } break;

        case EVAL_EVAL_OP_MOD:
        case EVAL_EVAL_OP_LEFT_SHIFT:
        case EVAL_EVAL_OP_RIGHT_SHIFT:
        case EVAL_EVAL_OP_BIT_NOT:
        case EVAL_EVAL_OP_BIT_AND:
        case EVAL_EVAL_OP_BIT_XOR:
        case EVAL_EVAL_OP_BIT_OR:
        case EVAL_EVAL_OP_LOG_NOT:
        case EVAL_EVAL_OP_LOG_AND:
        case EVAL_EVAL_OP_LOG_OR: {
            if (
                Group == EVAL_EVAL_TYPE_GROUP_SIGNED || 
                Group == EVAL_EVAL_TYPE_GROUP_UNSIGNED
            ) {
                Result = TRUE;
            }
        } break;
    }

    return Result;
}

EV_EXPAND_RULE_INFO_FUNC_DEF(Null)
{
    EVExpandInfo Result = {};

    return Result;
}

EVKey 
EVKeyAlloc(u64 ParentHash, u64 ChildID)
{
    EVKey Result = {};

    Result.ParentHash = ParentHash;
    Result.ChildID = ChildID;

    return Result;
}

EVKey 
EVKeyRoot(void)
{
    return EVKeyAlloc(5381, 1);
}

u64 
Hash(EVKey Key)
{
    u64 Data[] = {
        Key.ChildID
    };

    u64 Result = Hash(
        Key.ParentHash, 
        Str((u8*) Data, sizeof(Data))
    );

    return Result;
}

EvalTypeKey 
EVExpansionTypeFromKey(EvalTypeKey TypeKey)
{
    EvalTypeKey Result = {};

    for (
        EvalTypeKey Key = TypeKey;
        Key != EMPTY_EVAL_TYPE_KEY_VALUE;
        Key = EvalTypeKeyDirect(Key)
    ) {
        b32 IsDone = TRUE;
        EvalTypeKind Kind = EvalTypeKindFromKey(Key);

        if (Kind == EVAL_TYPE_KIND_LENS) {
            EvalType* __Type = EvalTypeFromKey(Key);

            if (
                __Type->ExpandRule.ExpandInfoFunc ||
                EVExpandRuleFromStr(__Type->Name) != &EV_NULL_EXPAND_RULE
            ) {
                IsDone = TRUE;
                Result = Key;
            } else {
                IsDone = FALSE;
            }
        } else if (
            Kind >= EVAL_TYPE_KIND_FIRST_META && 
            Kind <= EVAL_TYPE_KIND_LAST_META
        ) {
            IsDone = FALSE;
        }

        if (IsDone)
            break;
    }

    return Result;
}

b32 
EVTypeKeyAndModeIsExpandable(EvalTypeKey TypeKey, EvalMode Mode)
{
    b32 Result = FALSE;
    EvalTypeKey EVExpansionTypeKey = EVExpansionTypeFromKey(TypeKey);

    if (EVExpansionTypeKey != EMPTY_EVAL_TYPE_KEY_VALUE) {
        Result = TRUE;
    } else {
        EvalTypeKey DefaultExpansionTypeKey = EvalDefaultExpansionTypeFromKey(
            TypeKey
        );
        EvalTypeKind Kind = EvalTypeKindFromKey(DefaultExpansionTypeKey);

        if (Kind == EVAL_TYPE_KIND_ENUM)
            Result = (Mode == EVAL_MODE_NULL);
        else if (Kind != EVAL_TYPE_KIND_NULL)
            Result = TRUE;
    }

    return Result;
}

b32 
EVTypeKeyIsEditable(EvalTypeKey TypeKey)
{
    b32 Result = FALSE;
    b32 IsDone = FALSE;

    for (
        EvalTypeKey T = TypeKey;
        !Result && !IsDone;
        T = EvalTypeKeyDirect(T)
    ) {
        EvalType* __Type = EvalTypeFromKey(T);
        EvalTypeKind Kind = __Type->Kind;

        if (__Type->Flags & EVAL_TYPE_FLAG_IS_NOT_EDITABLE) {
            Result = FALSE;
            IsDone = TRUE;
        } else {
            switch (Kind) {
                default: {
                    if (
                        (
                            Kind >= EVAL_TYPE_KIND_FIRST_BASIC &&
                            Kind <= EVAL_TYPE_KIND_LAST_BASIC
                        ) ||
                        EvalTypeKindIsPtrOrRef(Kind)
                    ) {
                        Result = TRUE;
                        IsDone = TRUE;
                    }
                } break;

                case EVAL_TYPE_KIND_NULL: {
                    Result = FALSE;
                    IsDone = TRUE;
                } break;

                case EVAL_TYPE_KIND_ARRAY: {
                    if (__Type->Flags & EVAL_TYPE_FLAG_IS_NOT_TEXT) {
                        Result = FALSE;
                        IsDone = TRUE;
                    } else {
                        EvalTypeKind ElementKind = EvalTypeKindFromKey(
                            EvalTypeKeyUnwrap(
                                EvalTypeKeyDirect(T), 
                                EVAL_TYPE_UNWRAP_FLAG_ALL
                            )
                        );

                        Result = (
                            ElementKind == EVAL_TYPE_KIND_U8 ||
                            ElementKind == EVAL_TYPE_KIND_U16 ||
                            ElementKind == EVAL_TYPE_KIND_U32 ||
                            ElementKind == EVAL_TYPE_KIND_I8 ||
                            ElementKind == EVAL_TYPE_KIND_I16 ||
                            ElementKind == EVAL_TYPE_KIND_I32 ||
                            ElementKind == EVAL_TYPE_KIND_UCHAR8 ||
                            ElementKind == EVAL_TYPE_KIND_UCHAR16 ||
                            ElementKind == EVAL_TYPE_KIND_UCHAR32 ||
                            ElementKind == EVAL_TYPE_KIND_CHAR8 ||
                            ElementKind == EVAL_TYPE_KIND_CHAR16 ||
                            ElementKind == EVAL_TYPE_KIND_CHAR32
                        );
                        IsDone = TRUE;
                    }
                } break;
            }
        }
    }

    return Result;
}

EVView* 
EVViewAlloc(void)
{
    Arena* MemPool = ArenaAlloc();
    EVView* Result = ArenaPushArrayZero(
        MemPool,
        EVView,
        1
    );

    Result->MemPool = MemPool;
    Result->ExpandSlotsCount = 256;
    Result->ExpandSlots = ArenaPushArrayZero(
        MemPool,
        EVExpandSlot,
        Result->ExpandSlotsCount
    );

    return Result;
}

void 
EVViewRelease(EVView* View)
{
    ArenaRelease(View->MemPool);
}

EVExpandNode* 
EVExpandNodeFromKey(EVView* View, EVKey Key)
{
    u64 KeyHash = Hash(Key);
    u64 SlotIndex = KeyHash % View->ExpandSlotsCount;
    EVExpandSlot* Slot = &View->ExpandSlots[SlotIndex];
    EVExpandNode* Result = NULL;

    for (
        EVExpandNode* N = Slot->Head;
        N;
        N = N->HashNext
    ) {
        if (N->Key == Key) {
            Result = N;
            break;
        }
    }

    return Result;
}

b32 
EVExpansionFromKey(EVView* View, EVKey Key)
{
    EVExpandNode* Node = EVExpandNodeFromKey(View, Key);

    return (Node && Node->Expanded);
}

void 
EVKeySetExpansion(EVView* View, EVKey ParentKey, EVKey Key, b32 Expanded)
{
    EVExpandNode* ParentNode = EVExpandNodeFromKey(View, ParentKey);
    EVExpandNode* Node = EVExpandNodeFromKey(View, Key);

    if (!Node && Expanded) {
        Node = View->FreeExpandNode;

        if (Node) {
            SLL_STACK_POP(View->FreeExpandNode);
        } else {
            Node = ArenaPushArray(View->MemPool, EVExpandNode, 1);
        }

        MemZero(Node, sizeof(*Node));

        u64 KeyHash = Hash(Key);
        u64 Slot = KeyHash % View->ExpandSlotsCount;
        DLL_PUSH_BACK_EX(
            NULL,
            View->ExpandSlots[Slot].Head,
            View->ExpandSlots[Slot].Tail,
            Node,
            HashNext,
            HashPrev
        );

        if (ParentNode) {
            EVExpandNode* Prev = NULL;

            for (
                EVExpandNode* N = ParentNode->Head;
                N;
                N = N->Next
            ) {
                if (N->Key.ChildID < Key.ChildID)
                    Prev = N;
                else
                    break;
            }

            DLL_INSERT_EX(
                NULL,
                ParentNode->Head,
                ParentNode->Tail,
                Prev,
                Node,
                Next,
                Prev
            );
            Node->Parent = ParentNode;
        }
    }

    if (Node) {
        Node->Key = Key;
        Node->Expanded = Expanded;
    }

    if (!Expanded && Node && !Node->Head) {
        u64 KeyHash = Hash(Key);
        u64 Slot = KeyHash % View->ExpandSlotsCount;

        DLL_REMOVE_EX(
            NULL,
            View->ExpandSlots[Slot].Head,
            View->ExpandSlots[Slot].Tail,
            Node,
            HashNext,
            HashPrev
        );

        if (ParentNode) {
            DLL_REMOVE_EX(
                NULL,
                ParentNode->Head,
                ParentNode->Tail,
                Node,
                Next,
                Prev
            );
        }

        SLL_STACK_PUSH(View->FreeExpandNode, Node);
    }
}

void 
TablePush(Arena* MemPool, EVExpandRuleTable* Table, EVExpandRule* Info)
{
    if (!Table->SlotsCount) {
        Table->SlotsCount = 512;
        Table->Slots = ArenaPushArrayZero(
            MemPool,
            EVExpandRuleSlot,
            Table->SlotsCount
        );
    }

    u64 InfoHash = Hash(Info->String);
    u64 SlotIndex = InfoHash % Table->SlotsCount;
    EVExpandRuleSlot* Slot = &Table->Slots[SlotIndex];
    EVExpandRuleNode* Node = ArenaPushArrayZero(
        MemPool,
        EVExpandRuleNode,
        1
    );

    SLL_QUEUE_PUSH(Slot->Head, Slot->Tail, Node);
    MemCpy(&Node->V, Info, sizeof(*Info));
    Node->V.String = ArenaPushStrCpy(MemPool, Node->V.String);
}

void 
EVSelectExpandRuleTable(EVExpandRuleTable* Table)
{
    GetTLS()->EV_VIEW_RULE_INFO_TABLE = Table;
}

EVExpandRule* 
EVExpandRuleFromStr(Str8 String)
{
    EVExpandRule* Result = (EVExpandRule*) &EV_NULL_EXPAND_RULE;
    EVExpandRuleTable* const __EV_VIEW_RULE_INFO_TABLE = GetTLS()->EV_VIEW_RULE_INFO_TABLE;

    if (__EV_VIEW_RULE_INFO_TABLE && __EV_VIEW_RULE_INFO_TABLE->SlotsCount) {
        u64 StringHash = Hash(String);
        u64 SlotIndex = StringHash % __EV_VIEW_RULE_INFO_TABLE->SlotsCount;
        EVExpandRuleSlot* Slot = &__EV_VIEW_RULE_INFO_TABLE->Slots[SlotIndex];
        EVExpandRuleNode* Node = NULL;

        for (
            EVExpandRuleNode* N = Slot->Head;
            N;
            N = N->Next
        ) {
            if (StrMatch(N->V.String, String, 0)) {
                Node = N;
                break;
            }
        }

        if (Node)
            Result = &Node->V;
    }

    return Result;
}

EVExpandRule* 
EVExpandRuleFromTypeKey(EvalTypeKey TypeKey)
{
    EVExpandRule* Result = (EVExpandRule*) &EV_NULL_EXPAND_RULE;

    {
        EvalTypeKey K = EvalTypeKeyUnwrap(TypeKey, EVAL_TYPE_UNWRAP_FLAG_META);
        EvalTypeKind Kind = EvalTypeKindFromKey(K);

        for (
            ;
            Kind == EVAL_TYPE_KIND_LENS;
            K = EvalTypeKeyDirect(EvalTypeKeyUnwrap(K, EVAL_TYPE_UNWRAP_FLAG_META)), Kind = EvalTypeKindFromKey(K)
        ) {
            EvalType* __Type = EvalTypeFromKey(K);
            EVExpandRule* Candidate = EVExpandRuleFromStr(__Type->Name);

            if (Candidate != &EV_NULL_EXPAND_RULE) {
                Result = Candidate;
                break;
            }
        }
    }

    return Result;
}

EVBlockTree 
EVBlockTreeFromEval(
    Arena* MemPool, 
    EVView* View, 
    Str8 Filter, 
    EvalEval RootEval
) {
    EVBlockTree Result = {
        EMPTY_EV_BLOCK_VALUE
    };

    PROFILE(EVBlockTreeFromEvalTimer) {
        TempArena Scratch = GetScratch(&MemPool, 1);
        EVKey RootKey = EVKeyRoot();
        EVKey RootRowKey = EVKeyAlloc(Hash(RootKey), 1);

        Result.Root = ArenaPushArrayZero(MemPool, EVBlock, 1);
        MemCpy(Result.Root, EMPTY_EV_BLOCK_VALUE, sizeof(EVBlock));
        Result.Root->Key = RootKey;
        Result.Root->String = ""_s8;
        Result.Root->Eval = RootEval;
        Result.Root->TypeExpandRule = (EvalTypeExpandRule*) &EVAL_DEFAULT_TYPE_EXPAND_RULES;
        Result.Root->VisualExpandRule = (EVExpandRule*) &EV_NULL_EXPAND_RULE;
        Result.Root->RowCount = 1;
        ++Result.TotalRowCount;
        ++Result.TotalItemCount;

        struct BlockTreeBuildTask {
            BlockTreeBuildTask* Next;
            EVBlock*            ParentBlock;
            EvalEval            Eval;
            EvalExpr*           NextExpr;
            u64                 ChildID;
            u64                 SplitRelativeIndex;
            b32                 DefaultExpanded;
            b32                 ForceExpanded;
            i32                 Depth;
        };

        BlockTreeBuildTask StartTask = {
            NULL,
            Result.Root,
            Result.Root->Eval,
            Result.Root->Eval.Expr->Next,
            1,
            0
        };
        BlockTreeBuildTask* HeadTask = &StartTask;
        BlockTreeBuildTask* TailTask = HeadTask;

        for (
            BlockTreeBuildTask* T = HeadTask;
            T;
            T = T->Next
        ) {
            EVKey Key = EVKeyAlloc(Hash(T->ParentBlock->Key), T->ChildID);
            EVExpandNode* ExpandNode = EVExpandNodeFromKey(View, Key);
            b32 IsExpanded = (ExpandNode && ExpandNode->Expanded);

            if (T->DefaultExpanded || T->ForceExpanded)
                IsExpanded ^= TRUE;

            if (!IsExpanded)
                continue;

            EvalMode Mode = T->Eval.IRTree.Mode;
            EvalEval Eval = T->Eval;

            {
                EvalTypeKey TypeKey = EvalTypeKeyUnwrap(
                    Eval.IRTree.TypeKey,
                    EVAL_TYPE_UNWRAP_FLAG_MODS | EVAL_TYPE_UNWRAP_FLAG_META
                );
                EvalTypeKind TypeKind = EvalTypeKindFromKey(TypeKey);

                if (EvalTypeKindIsPtrOrRef(TypeKind))
                    Eval = EvalEvalWrap(Eval, "*($)"_s8);

                EvalTypeExpandRule* TypeExpandRule = EvalExpandRuleFromTypeKey(
                    Eval.IRTree.TypeKey
                );
                EVExpandRule* VisualExpandRule = EVExpandRuleFromTypeKey(
                    Eval.IRTree.TypeKey
                );

                if (
                    VisualExpandRule == &EV_NULL_EXPAND_RULE && 
                    !EVTypeKeyAndModeIsExpandable(Eval.IRTree.TypeKey, Mode)
                ) {
                    continue;
                }

                Str8 TaskFilter = T->Depth == 0 ? Filter : ""_s8;
                EvalTypeExpandInfo TypeExpandInfo = TypeExpandRule->ExpandInfoFunc(
                    MemPool, 
                    Eval, 
                    TaskFilter
                );
                EVExpandInfo VisualExpandInfo = VisualExpandRule->InfoHookFunc(
                    MemPool,
                    View,
                    TaskFilter,
                    Eval.Expr
                );
                u64 ExpansionRowCount = TypeExpandInfo.ExprCount;

                if (VisualExpandRule != &EV_NULL_EXPAND_RULE)
                    ExpansionRowCount = VisualExpandInfo.RowCount;

                ExpansionRowCount = MIN(0x0FFFFFFFFFFFFFFFULL, ExpansionRowCount);

                b32 AllowChildExpansions = TRUE;

                if (VisualExpandInfo.SingleItem)
                    AllowChildExpansions = FALSE;

                EVBlock* ExpansionBlock = EMPTY_EV_BLOCK_VALUE;

                if (ExpansionRowCount) {
                    ExpansionBlock = ArenaPushArrayZero(MemPool, EVBlock, 1);
                    MemCpy(ExpansionBlock, EMPTY_EV_BLOCK_VALUE, sizeof(*EMPTY_EV_BLOCK_VALUE));
                    DLL_PUSH_BACK_EX(
                        EMPTY_EV_BLOCK_VALUE,
                        T->ParentBlock->Head,
                        T->ParentBlock->Tail,
                        ExpansionBlock,
                        Next,
                        Prev
                    );
                    ExpansionBlock->Parent = T->ParentBlock;
                    ExpansionBlock->Key = Key;
                    ExpansionBlock->SplitRelativeIndex = T->SplitRelativeIndex;
                    ExpansionBlock->Eval = Eval;
                    ExpansionBlock->Filter = TaskFilter;
                    ExpansionBlock->TypeExpandInfo = TypeExpandInfo;
                    ExpansionBlock->TypeExpandRule = TypeExpandRule;
                    ExpansionBlock->VisualExpandInfo = VisualExpandInfo;
                    ExpansionBlock->VisualExpandRule = VisualExpandRule;
                    ExpansionBlock->RowCount = ExpansionRowCount;
                    Result.TotalRowCount += ExpansionRowCount;
                    Result.TotalItemCount += VisualExpandInfo.SingleItem 
                        ? 1 
                        : ExpansionRowCount;
                }

                u64 ChildCount = 0;
                EVKey* ChildKeys = NULL;
                u64* ChildNums = NULL;

                if (
                    AllowChildExpansions &&
                    !ChildCount &&
                    !VisualExpandInfo.RowsDefaultExpanded &&
                    ExpandNode &&
                    ExpansionRowCount
                ) {
                    for (
                        EVExpandNode* Child = ExpandNode->Head;
                        Child;
                        Child = Child->Next, ++ChildCount
                    );

                    b32 NeedsSort = FALSE;

                    ChildKeys = ArenaPushArrayZero(
                        Scratch.MemPool,
                        EVKey,
                        ChildCount
                    );
                    ChildNums = ArenaPushArrayZero(
                        Scratch.MemPool,
                        u64,
                        ChildCount
                    );

                    {
                        u64 Index = 0;

                        for (
                            EVExpandNode* Child = ExpandNode->Head;
                            Child;
                            Child = Child->Next, ++Index
                        ) {
                            ChildKeys[Index] = Child->Key;
                            ChildNums[Index] = TypeExpandRule->ExpandNumFromIDFunc(
                                TypeExpandInfo.UserData,
                                Child->Key.ChildID
                            );

                            if (ChildNums[Index] != ChildKeys[Index].ChildID)
                                NeedsSort = TRUE;
                        }
                    }

                    if (NeedsSort) {
                        for (u64 I = 0; I < ChildCount; ++I) {
                            u64 MinJ = 0;
                            u64 MinNum = ChildNums[I];

                            for (u64 J = I + 1; J < ChildCount; ++J) {
                                if (ChildNums[J] < MinNum) {
                                    MinJ = J;
                                    MinNum = ChildNums[J];
                                }
                            }

                            if (MinJ) {
                                SWAP(EVKey, ChildKeys[I], ChildKeys[MinJ]);
                                SWAP(u64, ChildNums[I], ChildNums[MinJ]);
                            }
                        }
                    }
                }

                if (
                    AllowChildExpansions &&
                    !ChildCount &&
                    (
                        VisualExpandInfo.RowsDefaultExpanded ||
                        (
                            !ExpandNode &&
                            !VisualExpandInfo.RowsDefaultExpanded
                        )
                    )
                ) {
                    ChildCount = VisualExpandInfo.RowCount;
                    ChildKeys = ArenaPushArrayZero(
                        Scratch.MemPool,
                        EVKey,
                        ChildCount
                    );
                    ChildNums = ArenaPushArrayZero(
                        Scratch.MemPool,
                        u64,
                        ChildCount
                    );

                    for (u64 Index = 0; Index < ChildCount; ++Index) {
                        u64 ChildID = TypeExpandRule->ExpandIDFromNumFunc(
                            TypeExpandInfo.UserData,
                            Index + 1
                        );

                        ChildKeys[Index] = EVKeyAlloc(Hash(Key), ChildID);
                        ChildNums[Index] = Index + 1;
                    }
                }

                for (u64 Index = 0; Index < ChildCount; ++Index) {
                    u64 SplitNum = ChildNums[Index];
                    u64 SplitRelativeIndex = SplitNum - 1;

                    if (SplitRelativeIndex >= ExpansionRowCount)
                        continue;

                    if (
                        VisualExpandInfo.RowsDefaultExpanded || 
                        EVExpansionFromKey(View, ChildKeys[Index])
                    ) {
                        r1u64 ChildRange = Rng(
                            SplitRelativeIndex,
                            SplitRelativeIndex + 1
                        );
                        EvalEval ChildEval = {};

                        TypeExpandRule->ExpandRangeFunc(
                            MemPool,
                            TypeExpandInfo.UserData,
                            Eval,
                            TaskFilter,
                            Rng(SplitRelativeIndex, SplitRelativeIndex + 1),
                            &ChildEval
                        );

                        EVKey ChildKey = ChildKeys[Index];
                        BlockTreeBuildTask* Task = ArenaPushArrayZero(
                            Scratch.MemPool,
                            BlockTreeBuildTask,
                            1
                        );

                        SLL_QUEUE_PUSH(HeadTask, TailTask, Task);
                        Task->ParentBlock = ExpansionBlock;
                        Task->Eval = ChildEval;
                        Task->NextExpr = EMPTY_EVAL_EXPR_VALUE;
                        Task->ChildID = ChildKey.ChildID;
                        Task->SplitRelativeIndex = SplitRelativeIndex;
                        Task->DefaultExpanded = VisualExpandInfo.RowsDefaultExpanded;
                        Task->Depth = T->Depth + 1;
                    }
                }

                if (T->NextExpr != EMPTY_EVAL_EXPR_VALUE) {
                    BlockTreeBuildTask* Task = ArenaPushArrayZero(
                        Scratch.MemPool,
                        BlockTreeBuildTask,
                        1
                    );

                    Task->Next = T->Next;
                    T->Next = Task;
                    Task->ParentBlock = T->ParentBlock;
                    Task->Eval = EvalEvalFromExpr(T->NextExpr);
                    Task->NextExpr = T->NextExpr->Next;
                    Task->ChildID = T->ChildID + 1;
                    Task->SplitRelativeIndex = 0;
                    Task->DefaultExpanded = T->DefaultExpanded;
                    Task->ForceExpanded = TRUE;
                    Task->Depth = T->Depth;
                }
            }

        }

        ReleaseScratch(Scratch);
    }

    return Result;
}

u64 
EVDepthFromBlock(EVBlock* Block)
{
    u64 Result = 0;

    for (
        EVBlock* B = Block->Parent; 
        B != EMPTY_EV_BLOCK_VALUE; 
        B = B->Parent
    ) {
        ++Result;
    }

    return Result;
}

u64 
EVBlockIDFromNum(EVBlock* Block, u64 Num)
{
    u64 Result = Block->TypeExpandRule->ExpandIDFromNumFunc(
        Block->TypeExpandInfo.UserData,
        Num
    );

    return Result;
}

u64 
EVBlockNumFromID(EVBlock* Block, u64 ID)
{
    u64 Result = Block->TypeExpandRule->ExpandNumFromIDFunc(
        Block->TypeExpandInfo.UserData,
        ID
    );

    return Result;
}

EVBlockRangeList 
EVBlockRangeListFromTree(Arena* MemPool, EVBlockTree* BlockTree)
{
    EVBlockRangeList Result = {};

    {
        TempArena Scratch = GetScratch(&MemPool, 1);

        struct BlockTask {
            BlockTask* Next;
            EVBlock*   Block;
            EVBlock*   NextChild;
            r1u64      BlockRelativeRange;
        };

        BlockTask StartTask = {
            NULL,
            BlockTree->Root,
            BlockTree->Root->Head,
            Rng(0ULL, BlockTree->Root->RowCount)
        };

        for (
            BlockTask* T = &StartTask; 
            T; 
            T = T->Next
        ) {
            r1u64 BlockRelativeRange = T->BlockRelativeRange;

            if (T->NextChild != EMPTY_EV_BLOCK_VALUE)
                BlockRelativeRange.Max = T->NextChild->SplitRelativeIndex + 1;

            u64 BlockNumVisualRows = Length(BlockRelativeRange);

            if (BlockNumVisualRows) {
                EVBlockRangeNode* N = ArenaPushArrayZero(
                    MemPool,
                    EVBlockRangeNode,
                    1
                );

                N->V.Block = T->Block;
                N->V.Range = BlockRelativeRange;
                SLL_QUEUE_PUSH(Result.Head, Result.Tail, N);
                ++Result.Count;
            }

            if (T->NextChild != EMPTY_EV_BLOCK_VALUE) {
                BlockTask* ChildTask = ArenaPushArrayZero(
                    Scratch.MemPool, 
                    BlockTask, 
                    1
                );

                ChildTask->Next = T->Next;
                T->Next = ChildTask;
                ChildTask->Block = T->NextChild;
                ChildTask->NextChild = T->NextChild->Head;
                ChildTask->BlockRelativeRange = Rng(0ULL, T->NextChild->RowCount);

                r1u64 RemainderRange = Rng(
                    T->NextChild->SplitRelativeIndex + 1,
                    T->BlockRelativeRange.Max
                );

                if (RemainderRange.Max >= RemainderRange.Min) {
                    BlockTask* RemainderTask = ArenaPushArrayZero(
                        Scratch.MemPool,
                        BlockTask,
                        1
                    );

                    RemainderTask->Next = ChildTask->Next;
                    ChildTask->Next = RemainderTask;
                    RemainderTask->Block = T->Block;
                    RemainderTask->NextChild = T->NextChild->Next;
                    RemainderTask->BlockRelativeRange = RemainderRange;
                }
            }
        }

        ReleaseScratch(Scratch);
    }

    return Result;
}

EVBlockRange 
EVBlockRangeFromNum(EVBlockRangeList* BlockRanges, u64 Num)
{
    EVBlockRange Result = {
        EMPTY_EV_BLOCK_VALUE
    };
    u64 BaseNum = 1;

    for (EVBlockRangeNode* Node = BlockRanges->Head; Node; Node = Node->Next) {
        u64 RangeSize = Node->V.Block->VisualExpandInfo.SingleItem 
            ? 1 
            : Length(Node->V.Range);
        r1u64 GlobalRange = Rng(BaseNum, BaseNum + RangeSize);

        if (InRange(GlobalRange, Num)) {
            Result = Node->V;
            break;
        }

        BaseNum += RangeSize;
    }

    return Result;
}

EVKey 
EVKeyFromNum(EVBlockRangeList* BlockRanges, u64 Num)
{
    EVKey Result = {};

    if (BlockRanges->Head)
        Result = EVKeyAlloc(Hash(EVKeyRoot()), 1);

    u64 BaseNum = 1;

    for (
        EVBlockRangeNode* Node = BlockRanges->Head;
        Node;
        Node = Node->Next
    ) {
        u64 RangeSize = Node->V.Block->VisualExpandInfo.SingleItem
            ? 1
            : Length(Node->V.Range);
        r1u64 GlobalRange = Rng(BaseNum, BaseNum + RangeSize);

        if (InRange(GlobalRange, Num)) {
            u64 RelativeNum = (Num - BaseNum) + Node->V.Range.Min + 1;
            u64 ChildID = EVBlockIDFromNum(Node->V.Block, RelativeNum);
            EVKey BlockKey = Node->V.Block->Key;

            Result = EVKeyAlloc(Hash(BlockKey), ChildID);
            break;
        }

        BaseNum += RangeSize;
    }

    return Result;
}

u64 
EVNumFromKey(EVBlockRangeList* BlockRanges, EVKey Key)
{
    u64 Result = 0;
    u64 BaseNum = 1;

    for (
        EVBlockRangeNode* Node = BlockRanges->Head;
        Node;
        Node = Node->Next
    ) {
        u64 KeyHash = Hash(Node->V.Block->Key);

        if (KeyHash == Key.ParentHash) {
            u64 RelativeNum = EVBlockNumFromID(Node->V.Block, Key.ChildID);
            r1u64 NumRange = Rng(
                Node->V.Range.Min,
                Node->V.Block->VisualExpandInfo.SingleItem
                    ? (Node->V.Range.Min + 1)
                    : Node->V.Range.Max
            );

            if (InRange(NumRange, RelativeNum - 1)) {
                Result = BaseNum + (RelativeNum - 1 - Node->V.Range.Min);
                break;
            }
        }

        BaseNum += Node->V.Block->VisualExpandInfo.SingleItem
            ? 1
            : Length(Node->V.Range);
    }

    return Result;
}

u64 
EVVNumFromNum(EVBlockRangeList* BlockRanges, u64 Num)
{
    u64 Result = 0;

    {
        u64 BaseVNum = 1;
        u64 BaseNum = 1;

        for (
            EVBlockRangeNode* Node = BlockRanges->Head;
            Node;
            Node = Node->Next
        ) {
            u64 NextBaseNum = BaseNum + (
                Node->V.Block->VisualExpandInfo.SingleItem 
                    ? 1 
                    : Length(Node->V.Range)
            );

            if (Num >= BaseNum && Num < NextBaseNum) {
                u64 RelativeVNum = Node->V.Block->VisualExpandInfo.SingleItem
                    ? 0
                    : (Num - BaseNum);

                Result = BaseVNum + RelativeVNum;
                break;
            }

            BaseNum = NextBaseNum;
            BaseVNum += Length(Node->V.Range);
        }

        if (!Result)
            Result = BaseVNum;
    }

    return Result;
}

u64 
EVNumFromVNum(EVBlockRangeList* BlockRanges, u64 VNum)
{
    u64 Result = 0;

    {
        u64 BaseVNum = 1;
        u64 BaseNum = 1;

        for (
            EVBlockRangeNode* Node = BlockRanges->Head;
            Node;
            Node = Node->Next
        ) {
            u64 NextBaseVNum = BaseVNum + Length(Node->V.Range);

            if (VNum >= BaseVNum && VNum < NextBaseVNum) {
                u64 RelativeNum = Node->V.Block->VisualExpandInfo.SingleItem
                    ? 0
                    : (VNum - BaseVNum);

                Result = BaseNum + RelativeNum;
                break;
            }

            BaseVNum = NextBaseVNum;
            BaseNum += Node->V.Block->VisualExpandInfo.SingleItem
                ? 1
                : Length(Node->V.Range);
        }
    }

    return Result;
}

EVWindowedRowList 
EVWindowedRowListFromBlockRangeList(
    Arena* MemPool, 
    EVView* View, 
    EVBlockRangeList* BlockRanges, 
    r1u64 VNumRange
) {
    EVWindowedRowList Result = {};

    {
        u64 BaseVNum = 1;

        for (
            EVBlockRangeNode* Node = BlockRanges->Head;
            Node;
            Node = Node->Next
        ) {
            r1u64 BlockRelativeRange = Node->V.Range;
            u64 BlockNumVisualRows = Length(BlockRelativeRange);
            r1u64 BlockGlobalRange = Rng(
                BaseVNum, 
                BaseVNum + BlockNumVisualRows
            );
            Str8 BlockFilter = Node->V.Block->Filter;
            u64 NumSkipped = 0;
            u64 NumChopped = 0;

            {
                if (VNumRange.Min > BlockGlobalRange.Min) {
                    NumSkipped = VNumRange.Min - BlockGlobalRange.Min;
                    NumSkipped = MIN(NumSkipped, BlockNumVisualRows);
                }

                if (VNumRange.Max < BlockGlobalRange.Max) {
                    NumChopped = BlockGlobalRange.Max - VNumRange.Max;
                    NumChopped = MIN(NumChopped, BlockNumVisualRows);
                }
            }

            r1u64 BlockRelativeRangeWindowed = Rng(
                BlockRelativeRange.Min + NumSkipped,
                BlockRelativeRange.Max - NumChopped
            );

            BaseVNum += BlockNumVisualRows;
            Result.CountBeforeVisual += NumSkipped;

            if (BlockNumVisualRows && NumSkipped) {
                if (Node->V.Block->VisualExpandInfo.SingleItem) {
                    if (NumSkipped >= BlockNumVisualRows)
                        Result.CountBeforeSemantic += 1;
                } else {
                    Result.CountBeforeSemantic += NumSkipped;
                }
            }

            if (BlockRelativeRangeWindowed.Max > BlockRelativeRangeWindowed.Min) {
                b32 IsStandaloneRow = FALSE;
                u64 RangeExprsCount = Length(BlockRelativeRangeWindowed);
                EvalEval* RangeEvals = ArenaPushArrayZero(
                    MemPool,
                    EvalEval,
                    RangeExprsCount
                );

                for (u64 Index = 0; Index < RangeExprsCount; ++Index)
                    RangeEvals[Index] = *EMPTY_EVAL_EVAL_VALUE;

                if (
                    Node->V.Block->VisualExpandInfo.SingleItem ||
                    Node->V.Block->Parent == EMPTY_EV_BLOCK_VALUE
                ) {
                    IsStandaloneRow = TRUE;
                } else {
                    Node->V.Block->TypeExpandRule->ExpandRangeFunc(
                        MemPool,
                        Node->V.Block->TypeExpandInfo.UserData,
                        Node->V.Block->Eval,
                        BlockFilter,
                        BlockRelativeRangeWindowed,
                        RangeEvals
                    );
                }

                if (IsStandaloneRow) {
                    EVWindowedRowNode* RowNode = ArenaPushArrayZero(
                        MemPool,
                        EVWindowedRowNode,
                        1
                    );

                    SLL_QUEUE_PUSH(Result.Head, Result.Tail, RowNode);
                    ++Result.Count;
                    RowNode->VisualSizeSkipped = NumSkipped;
                    RowNode->VisualSizeChopped = NumChopped;

                    EVRow* Row = &RowNode->Row;

                    Row->Block = Node->V.Block;
                    Row->Key = EVKeyAlloc(Hash(Row->Block->Key), 1);
                    Row->VisualSize = Node->V.Block->VisualExpandInfo.SingleItem
                        ? (Node->V.Block->RowCount - (NumSkipped + NumChopped))
                        : 1;
                    Row->EditString = Node->V.Block->String;
                    Row->Eval = Node->V.Block->Eval;
                } else {
                    for (u64 Index = 0; Index < RangeExprsCount; ++Index) {
                        u64 ChildNum = BlockRelativeRange.Min + NumSkipped + Index + 1;
                        u64 ChildID = EVBlockIDFromNum(Node->V.Block, ChildNum);
                        EVKey RowKey = EVKeyAlloc(Hash(Node->V.Block->Key), ChildID);
                        EvalEval RowEval = RangeEvals[Index];
                        EVWindowedRowNode* RowNode = ArenaPushArrayZero(
                            MemPool,
                            EVWindowedRowNode,
                            1
                        );

                        SLL_QUEUE_PUSH(Result.Head, Result.Tail, RowNode);
                        ++Result.Count;

                        EVRow* Row = &RowNode->Row;

                        Row->Block = Node->V.Block;
                        Row->Key = RowKey;
                        Row->VisualSize = 1;
                        Row->EditString = RowEval.String;
                        Row->Eval = RowEval;
                    }
                }
            }
        }
    }

    return Result;
}

EVRow* 
EVRowFromNum(
    Arena* MemPool, 
    EVView* View, 
    EVBlockRangeList* BlockRanges, 
    u64 Num
) {
    u64 VIndex = EVVNumFromNum(BlockRanges, Num);
    EVWindowedRowList Rows = EVWindowedRowListFromBlockRangeList(
        MemPool,
        View,
        BlockRanges,
        Rng(VIndex, VIndex + 1)
    );
    EVRow* Result = NULL;

    if (Rows.Head) {
        Result = &Rows.Head->Row;
    } else {
        Result = ArenaPushArrayZero(MemPool, EVRow, 1);
        Result->Block = EMPTY_EV_BLOCK_VALUE;
        Result->Eval = *EMPTY_EVAL_EVAL_VALUE;
    }

    return Result;
}

EVWindowedRowList 
EVRowsFromNumRange(
    Arena* MemPool, 
    EVView* View, 
    EVBlockRangeList* BlockRanges, 
    r1u64 NumRange
) {
    r1u64 VNumRange = Rng(
        EVVNumFromNum(BlockRanges, NumRange.Min),
        EVVNumFromNum(BlockRanges, NumRange.Max) + 1
    );
    EVWindowedRowList Result = EVWindowedRowListFromBlockRangeList(
        MemPool,
        View,
        BlockRanges,
        VNumRange
    );

    return Result;
}

b32 
EVEvalIsExpandable(EvalEval Eval)
{
    b32 Result = FALSE;
    EvalIRTreeAndType IRTree = Eval.IRTree;

    if (!Result) {
        EVExpandRule* ExpandRule = EVExpandRuleFromTypeKey(IRTree.TypeKey);

        if (ExpandRule != &EV_NULL_EXPAND_RULE)
            Result = TRUE;
    }

    if (!Result)
        Result = EVTypeKeyAndModeIsExpandable(IRTree.TypeKey, IRTree.Mode);

    return Result;
}

b32 
EVRowIsExpandable(EVRow* Row)
{
    b32 Result = FALSE;

    if (Row->Block->Key != EVKeyRoot())
        Result = EVEvalIsExpandable(Row->Eval);

    return Result;
}

b32 
EVRowIsEditable(EVRow* Row)
{
    b32 Result = FALSE;
    EvalIRTreeAndType IRTree = Row->Eval.IRTree;

    Result = EVTypeKeyIsEditable(IRTree.TypeKey);

    return Result;
}

Str8 
EVStrFromASCII(Arena* MemPool, u8 Char)
{
    Str8 Result = {};

    switch (Char) {
        case 0x00: {
            Result = "\\0"_s8;
        } break;

        case 0x07: {
            Result = "\\a"_s8;
        } break;

        case 0x08: {
            Result = "\\b"_s8;
        } break;

        case 0x0C: {
            Result = "\\f"_s8;
        } break;

        case 0x0A: {
            Result = "\\n"_s8;
        } break;

        case 0x0D: {
            Result = "\\r"_s8;
        } break;

        case 0x09: {
            Result = "\\t"_s8;
        } break;

        case 0x0B: {
            Result = "\\v"_s8;
        } break;

        case 0x3F: {
            Result = "\\?"_s8;
        } break;

        case '"': {
            Result = "\\\""_s8;
        } break;

        case '\'': {
            Result = "\\'"_s8;
        } break;

        case '\\': {
            Result = "\\\\"_s8;
        } break;

        default: {
            if (Char >= 32 && Char < 255)
                Result = ArenaPushStrFmt(MemPool, "%c", Char);
        } break;
    }

    return Result;
}

Str8 
EVStrFromSimpleTypedEval(Arena* MemPool, EVStrParams* Params, EvalEval Eval)
{
    Str8 Result = {};
    EvalTypeKey TypeKey = EvalTypeKeyUnwrap(
        Eval.IRTree.TypeKey,
        EVAL_TYPE_UNWRAP_FLAG_ALL_DECORATIVE & ~EVAL_TYPE_UNWRAP_FLAG_ENUMS
    );
    EvalTypeKind TypeKind = EvalTypeKindFromKey(TypeKey);
    u64 TypeByteSize = EvalByteSizeFromKey(TypeKey);
    u8 DigitGroupSeparator = 0;

    if (!(Params->Flags & EV_STR_FLAG_READ_ONLY_DISPLAY_RULES))
        DigitGroupSeparator = 0;

    i64 I64 = 0;
    u64 U64 = 0;
    f64 F64 = 0.0;

    switch (TypeKind) {
        default: {} break;

        case EVAL_TYPE_KIND_CHAR8:
        case EVAL_TYPE_KIND_CHAR16:
        case EVAL_TYPE_KIND_CHAR32:
        case EVAL_TYPE_KIND_UCHAR8:
        case EVAL_TYPE_KIND_UCHAR16:
        case EVAL_TYPE_KIND_UCHAR32: {
            b32 TypeIsSigned = (
                TypeKind >= EVAL_TYPE_KIND_UCHAR8 && 
                TypeKind <= EVAL_TYPE_KIND_UCHAR32
            );
            Str8 CharString = {};

            if (!(Params->Flags & EV_STR_FLAG_DISABLE_CHARS))
                CharString = EVStrFromASCII(MemPool, Eval.Value.I64);

            if (CharString.Size) {
                if (Params->Flags & EV_STR_FLAG_READ_ONLY_DISPLAY_RULES) {
                    Str8 IMMString = TypeIsSigned
                        ? StrFromU64(
                            MemPool,
                            Eval.Value.U64,
                            Params->Radix,
                            Params->MinDigits,
                            DigitGroupSeparator
                        )
                        : StrFromI64(
                            MemPool,
                            Eval.Value.I64,
                            Params->Radix,
                            Params->MinDigits,
                            DigitGroupSeparator
                        );

                    Result = ArenaPushStrFmt(
                        MemPool,
                        "'%S' (%S)",
                        PRINT_STR(CharString),
                        PRINT_STR(IMMString)
                    );
                } else {
                    Result = ArenaPushStrFmt(MemPool, "'%S'", PRINT_STR(CharString));
                }
            } else {
                Result = TypeIsSigned
                    ? StrFromU64(
                        MemPool,
                        Eval.Value.U64,
                        Params->Radix,
                        Params->MinDigits,
                        DigitGroupSeparator
                    )
                    : StrFromI64(
                        MemPool,
                        Eval.Value.I64,
                        Params->Radix,
                        Params->MinDigits,
                        DigitGroupSeparator
                    );
            }
        } break;

        case EVAL_TYPE_KIND_I8:
        case EVAL_TYPE_KIND_I16:
        case EVAL_TYPE_KIND_I32:
        case EVAL_TYPE_KIND_I64: {
            if (TypeKind == EVAL_TYPE_KIND_I8)
                I64 = (i64) Eval.Value.I8;
            else if (TypeKind == EVAL_TYPE_KIND_I16)
                I64 = (i64) Eval.Value.I16;
            else if (TypeKind == EVAL_TYPE_KIND_I32)
                I64 = (i64) Eval.Value.I32;
            else
                I64 = Eval.Value.I64;

            Result = StrFromI64(
                MemPool,
                I64,
                Params->Radix,
                Params->MinDigits,
                DigitGroupSeparator
            );
        } break;

        case EVAL_TYPE_KIND_U8:
        case EVAL_TYPE_KIND_U16:
        case EVAL_TYPE_KIND_U32:
        case EVAL_TYPE_KIND_U64: {
            if (TypeKind == EVAL_TYPE_KIND_U8)
                U64 = (u64) Eval.Value.U8;
            else if (TypeKind == EVAL_TYPE_KIND_U16)
                U64 = (u64) Eval.Value.U16;
            else if (TypeKind == EVAL_TYPE_KIND_U32)
                U64 = (u64) Eval.Value.U32;
            else
                U64 = Eval.Value.U64;

            Result = StrFromU64(
                MemPool,
                U64,
                Params->Radix,
                Params->MinDigits,
                DigitGroupSeparator
            );
        } break;

        case EVAL_TYPE_KIND_U128: {
            TempArena Scratch = GetScratch(&MemPool, 1);
            Str8 UpperBits = StrFromU64(
                Scratch.MemPool,
                Eval.Value.U128.U64[0],
                Params->Radix,
                Params->MinDigits,
                DigitGroupSeparator
            );
            Str8 LowerBits = StrFromU64(
                Scratch.MemPool,
                Eval.Value.U128.U64[1],
                Params->Radix,
                Params->MinDigits,
                DigitGroupSeparator
            );

            Result = ArenaPushStrFmt(
                MemPool, 
                "%S:%S", 
                PRINT_STR(UpperBits), 
                PRINT_STR(LowerBits)
            );
            ReleaseScratch(Scratch);
        } break;

        case EVAL_TYPE_KIND_F32:
        case EVAL_TYPE_KIND_F64: {
            if (TypeKind == EVAL_TYPE_KIND_F32)
                F64 = (f64) Eval.Value.F32;
            else
                F64 = (f64) Eval.Value.F64;

            Result = ArenaPushStrFmt(
                MemPool,
                "%.*f",
                Params->MinDigits ? Params->MinDigits : 16,
                F64
            );

            u64 NumToChop = 0;

            for (u64 NumTrimmed = 0; NumTrimmed < Result.Size; ++NumTrimmed) {
                if (Result.Str[Result.Size - 1 - NumTrimmed] != '0') {
                    if (Result.Str[Result.Size - 1 - NumTrimmed] == '.' && NumToChop)
                        --NumToChop;

                    break;
                }

                ++NumToChop;
            }

            Result = StrChop(Result, NumToChop);
        } break;

        case EVAL_TYPE_KIND_BOOL: {
            Result = ArenaPushStrFmt(
                MemPool,
                "%s",
                Eval.Value.U64 ? "true" : "false"
            );
        } break;

        case EVAL_TYPE_KIND_PTR: {
            Result = ArenaPushStrFmt(
                MemPool,
                "0x%lX",
                Eval.Value.U64
            );
        } break;
    }

    return Result;
}

Str8 
EVEscapedFromRawStr(Arena* MemPool, Str8 Raw)
{
    TempArena Scratch = GetScratch(&MemPool, 1);
    Str8List Parts = {};
    u64 StartSplitIndex = 0;

    for (u64 Index = 0; Index <= Raw.Size; ++Index) {
        u8 Byte = (Index < Raw.Size) ? Raw.Str[Index] : 0;
        b32 Split = TRUE;
        Str8 SeparatorReplace = {};

        switch (Byte) {
            default: {
                Split = FALSE;
            } break;

            case 0: {} break;

            case '\a': {
                SeparatorReplace = "\\a"_s8;
            } break;

            case '\b': {
                SeparatorReplace = "\\b"_s8;
            } break;

            case '\f': {
                SeparatorReplace = "\\f"_s8;
            } break;

            case '\n': {
                SeparatorReplace = "\\n"_s8;
            } break;

            case '\r': {
                SeparatorReplace = "\\r"_s8;
            } break;

            case '\t': {
                SeparatorReplace = "\\t"_s8;
            } break;

            case '\v': {
                SeparatorReplace = "\\v"_s8;
            } break;

            case '\\': {
                SeparatorReplace = "\\\\"_s8;
            } break;

            case '"': {
                SeparatorReplace = "\\\""_s8;
            } break;
        }

        if (Split) {
            Str8 SubStr = StrSub(Raw, Rng(StartSplitIndex, Index));

            StartSplitIndex = Index + 1;
            ListPush(Scratch.MemPool, &Parts, SubStr);

            if (SeparatorReplace.Size)
                ListPush(Scratch.MemPool, &Parts, SeparatorReplace);
        }
    }

    Str8JoinPart Join = {};
    Str8 Result = StrListJoin(MemPool, &Parts, &Join);

    ReleaseScratch(Scratch);

    return Result;
}

EVStrIter* 
EVStrIterBegin(Arena* MemPool, EvalEval Eval, EVStrParams* Params)
{
    EVStrIter* Result = ArenaPushArrayZero(
        MemPool,
        EVStrIter,
        1
    );

    Result->HeadTask = ArenaPushArrayZero(MemPool, EVStrIterTask, 1);
    Result->HeadTask->Eval = Eval;
    MemCpy(&Result->HeadTask->Params, Params, sizeof(*Params));

    return Result;
}

b32 
EVStrIterNext(Arena* MemPool, EVStrIter* Iter, OUT Str8* String)
{
    b32 Result = FALSE;

    MemZero(String, sizeof(*String));

    b32 PopRequired = TRUE;
    b32 NewTaskRequired = FALSE;
    EVStrIterTask NewTask = {};
    i32 HeadTaskDepth = 0;

    if (Iter->HeadTask) {
        Result = TRUE;

        u64 TaskIndex = Iter->HeadTask->Index;
        i32 Depth = Iter->HeadTask->Depth;

        HeadTaskDepth = Iter->HeadTask->Depth;

        EVStrParams* Params = &Iter->HeadTask->Params;
        EvalEval Eval = Iter->HeadTask->Eval;
        EvalTypeKey TypeKey = Eval.IRTree.TypeKey;
        EvalTypeKind TypeKind = EvalTypeKindFromKey(TypeKey);
        Str8 ExpansionOpenerSymbol = "{"_s8;
        Str8 ExpansionCloserSymbol = "}"_s8;

        if (
            Eval.IRTree.Mode == EVAL_MODE_NULL && 
            Eval.IRTree.TypeKey != EMPTY_EVAL_TYPE_KEY_VALUE
        ) {
            *String = EvalTypeStrFromKey(MemPool, TypeKey);
        } else {
            switch (TypeKind) {
                default: {
                    EvalEval ValueEval = EvalValueEvalFromEval(Eval);

                    *String = EVStrFromSimpleTypedEval(MemPool, Params, ValueEval);
                } break;

                case EVAL_TYPE_KIND_LENS: {
                    if (Iter->HeadTask->RedirectToSetsAndStructs) {
                        EvalType* __Type = EvalTypeFromKey(TypeKey);

                        if (__Type->Flags & EVAL_TYPE_FLAG_ARRAY_LIKE_EXPANSION) {
                            ExpansionOpenerSymbol = "["_s8;
                            ExpansionCloserSymbol = "]"_s8;
                        }

                        goto ARRAYS_AND_SETS_AND_STRUCTS;
                    }

                    EvalType* __Type = EvalTypeFromKey(TypeKey);
                    EvalTypeKind ElementTypeKind = EvalTypeKindFromKey(
                        EvalTypeKeyUnwrap(
                            __Type->DirectTypeKey, 
                            EVAL_TYPE_UNWRAP_FLAG_ALL
                        )
                    );
                    b32 LensApplied = TRUE;
                    EVStrParams LensParams = *Params;

                    if (StrMatch(__Type->Name, "bin"_s8, 0)) {
                        LensParams.Radix = 2;
                    } else if (StrMatch(__Type->Name, "oct"_s8, 0)) {
                        LensParams.Radix = 8;
                    } else if (StrMatch(__Type->Name, "dec"_s8, 0)) {
                        LensParams.Radix = 10;
                    } else if (StrMatch(__Type->Name, "hex"_s8, 0)) {
                        LensParams.Radix = 16;
                    } else if (StrMatch(__Type->Name, "digits"_s8, 0) && __Type->Count) {
                        EvalParentKey(Eval.Key) {
                            EvalValue Value = EvalValueFromExpr(__Type->Args[0]);

                            LensParams.MinDigits = Value.U64;
                        }
                    } else if (StrMatch(__Type->Name, "no_string"_s8, 0)) {
                        LensParams.Flags |= EV_STR_FLAG_DISABLE_STRS;
                    } else if (StrMatch(__Type->Name, "no_char"_s8, 0)) {
                        LensParams.Flags |= EV_STR_FLAG_DISABLE_CHARS;
                    } else if (
                        StrMatch(__Type->Name, "array"_s8, 0) &&
                        __Type->Count &&
                        (
                            (
                                ElementTypeKind >= EVAL_TYPE_KIND_CHAR8 &&
                                ElementTypeKind <= EVAL_TYPE_KIND_UCHAR32
                            ) ||
                            ElementTypeKind == EVAL_TYPE_KIND_I8 ||
                            ElementTypeKind == EVAL_TYPE_KIND_U8
                        )
                    ) {
                        EvalParentKey(Eval.Key) {
                            LensParams.LimitStrings = TRUE;
                            LensParams.LimitStringsSize = EvalValueFromExpr(
                                __Type->Args[0]
                            ).U64;
                        }
                    } else {
                        LensApplied = FALSE;
                    }

                    if (LensApplied) {
                        NewTaskRequired = TRUE;
                        PopRequired = TRUE;
                        NewTask.Params = LensParams;
                        NewTask.Eval = Eval;
                        NewTask.Eval.IRTree.TypeKey = EvalTypeKeyDirect(
                            Eval.IRTree.TypeKey
                        );
                    } else if (
                        __Type->Kind != EVAL_TYPE_KIND_LENS || 
                        __Type->ExpandRule.ExpandInfoFunc
                    ) {
                        NewTaskRequired = TRUE;
                        PopRequired = TRUE;
                        NewTask.Params = *Params;
                        NewTask.Eval = Eval;
                        NewTask.RedirectToSetsAndStructs = TRUE;
                    } else {
                        NewTaskRequired = TRUE;
                        PopRequired = TRUE;
                        NewTask.Params = LensParams;
                        NewTask.Eval = Eval;
                        NewTask.Eval.IRTree.TypeKey = EvalTypeKeyDirect(
                            Eval.IRTree.TypeKey
                        );
                    }
                } break;

                case EVAL_TYPE_KIND_META_EXPR: {
                    if (Params->Flags & EV_STR_FLAG_READ_ONLY_DISPLAY_RULES) {
                        switch (TaskIndex) {
                            default: {} break;

                            case 0: {
                                EvalType* __Type = EvalTypeFromKey(TypeKey);
                                *String = ArenaPushStrFmt(
                                    MemPool, 
                                    "%S (", 
                                    __Type->Name
                                );
                                PopRequired = FALSE;
                                NewTaskRequired = TRUE;
                                NewTask.Params = *Params;
                                NewTask.Eval = Eval;
                                NewTask.Eval.IRTree.TypeKey = EvalTypeKeyDirect(
                                    Eval.IRTree.TypeKey
                                );
                            } break;

                            case 1: {
                                *String = ")"_s8;
                            } break;
                        }
                    } else {
                        EvalType* __Type = EvalTypeFromKey(TypeKey);

                        *String = __Type->Name;
                    }
                } break;

                case EVAL_TYPE_KIND_META_DESCRIPTION:
                case EVAL_TYPE_KIND_META_DISPLAY_NAME: {
                    PopRequired = TRUE;
                    NewTaskRequired = TRUE;
                    NewTask.Params = *Params;
                    NewTask.Eval = Eval;
                    NewTask.Eval.IRTree.TypeKey = EvalTypeKeyDirect(
                        NewTask.Eval.IRTree.TypeKey
                    );
                } break;

                case EVAL_TYPE_KIND_PTR:
                case EVAL_TYPE_KIND_ARRAY: {
                    if (
                        TypeKind == EVAL_TYPE_KIND_ARRAY &&
                        Iter->HeadTask->RedirectToSetsAndStructs
                    ) {
                        ExpansionOpenerSymbol = "["_s8;
                        ExpansionCloserSymbol = "]"_s8;

                        goto ARRAYS_AND_SETS_AND_STRUCTS;
                    }

                    struct EVStrPtrData {
                        EvalEval  ValueEval;
                        EvalType* Type;
                        EvalType* DirectType;
                        b32       PTeeHasContent;
                        b32       PTeeHasString;
                        b32       DidPrefixContent;
                        b32       DidPrefixString;
                        b32       DidRedirect;
                        b32       DidPrePrefixPtr;
                        b32       AddrIsGood;
                    };
                    EVStrPtrData* PtrData = (EVStrPtrData*) Iter->HeadTask->UserData;

                    if (!PtrData) {
                        Iter->HeadTask->UserData = ArenaPushArrayZero(
                            MemPool,
                            EVStrPtrData,
                            1
                        );
                        PtrData = (EVStrPtrData*) Iter->HeadTask->UserData;
                        PtrData->ValueEval = EvalValueEvalFromEval(Eval);
                        PtrData->Type = EvalTypeFromKey(TypeKey);
                        PtrData->DirectType = EvalTypeFromKey(
                            EvalTypeKeyUnwrap(
                                EvalTypeKeyDirect(
                                    EvalTypeKeyUnwrap(
                                        TypeKey, 
                                        EVAL_TYPE_UNWRAP_FLAG_ALL
                                    )
                                ),
                                EVAL_TYPE_UNWRAP_FLAG_ALL
                            )
                        );
                        PtrData->PTeeHasContent = (
                            PtrData->ValueEval.Value.U64 &&
                            PtrData->DirectType->Kind != EVAL_TYPE_KIND_NULL
                        );
                        PtrData->PTeeHasString = (
                            (
                                PtrData->DirectType->Kind >= EVAL_TYPE_KIND_CHAR8 &&
                                PtrData->DirectType->Kind <= EVAL_TYPE_KIND_UCHAR32
                            ) ||
                            PtrData->DirectType->Kind == EVAL_TYPE_KIND_I8 ||
                            PtrData->DirectType->Kind == EVAL_TYPE_KIND_U8
                        );

                        u8 Byte = 0;
                        u64 ByteBadFlags = 0;
                        EvalSpaceRangeInfo RangeInfo = {};

                        RangeInfo.ByteBadFlags = &ByteBadFlags;

                        if (PtrData->ValueEval.Space.Kind == EVAL_SPACE_KIND_NULL) {
                            PtrData->AddrIsGood = TRUE;
                        } else if (
                            EvalSpaceRead(
                                PtrData->ValueEval.Space, 
                                &Byte, 
                                &RangeInfo, 
                                Rng(
                                    PtrData->ValueEval.Value.U64, 
                                    PtrData->ValueEval.Value.U64 + 1
                                )
                            )
                        ) {
                            PtrData->AddrIsGood = !(ByteBadFlags & 1);
                        }
                    }

                    if (PtrData->DidRedirect) {
                        PopRequired = TRUE;
                    } else {
                        switch (TaskIndex) {
                            default: {} break;

                            case 0: {
                                PopRequired = FALSE;
                            } break;

                            case 1: {
                                Str8 PrePrefix = {};

                                if (PtrData->DidPrePrefixPtr)
                                    PrePrefix = " -> "_s8;

                                if (
                                    !(PtrData->Type->Flags & EVAL_TYPE_FLAG_IS_NOT_TEXT) &&
                                    !PtrData->DidPrefixContent &&
                                    PtrData->PTeeHasString &&
                                    !(Params->Flags & EV_STR_FLAG_DISABLE_STRS) &&
                                    (
                                        TypeKind == EVAL_TYPE_KIND_ARRAY ||
                                        Params->Flags & EV_STR_FLAG_READ_ONLY_DISPLAY_RULES
                                    )
                                ) {
                                    TempArena Scratch = GetScratch(&MemPool, 1);

#define EV_STR_ITER_STR_BUFFER_CAP 4096

                                    u64 StringBufferSize = EV_STR_ITER_STR_BUFFER_CAP;
                                    u8* StringBuffer = ArenaPushArrayZero(
                                        Scratch.MemPool,
                                        u8,
                                        StringBufferSize
                                    );

                                    if (
                                        TypeKind == EVAL_TYPE_KIND_ARRAY && 
                                        Eval.IRTree.Mode == EVAL_MODE_VALUE
                                    ) {
                                        STATIC_ASSERT(
                                            sizeof(Eval.Value.U512.U8) <= EV_STR_ITER_STR_BUFFER_CAP, 
                                            EvStrIterValueStrBufferSizeCheck
                                        );

                                        MemCpy(
                                            StringBuffer,
                                            Eval.Value.U512.U8,
                                            sizeof(Eval.Value.U512.U8)
                                        );
                                    } else {
                                        u64 StringAddress = PtrData->ValueEval.Value.U64;

                                        for (
                                            u64 Size = StringBufferSize;
                                            Size >= 16;
                                            Size /= 2
                                        ) {
                                            b32 ReadOkay = EvalSpaceRead(
                                                Eval.Space,
                                                StringBuffer,
                                                NULL,
                                                Rng(
                                                    StringAddress,
                                                    StringAddress + Size
                                                )
                                            );

                                            if (ReadOkay)
                                                break;
                                        }

                                        StringBuffer[StringBufferSize - 1] = 0;
                                    }

                                    u64 ElementSize = PtrData->DirectType->ByteSize;
                                    Str8 RawString = {};

                                    switch (ElementSize) {
                                        default: {
                                            RawString = CStr8((char*) StringBuffer);
                                        } break;

                                        case 2: {
                                            RawString = Str8FromStr16(
                                                Scratch.MemPool, 
                                                CStr16((u16*) StringBuffer)
                                            );
                                        } break;

                                        case 4: {
                                            RawString = Str8FromStr32(
                                                Scratch.MemPool,
                                                CStr32((u32*) StringBuffer)
                                            );
                                        } break;
                                    }

                                    if (Params->LimitStrings) {
                                        RawString = StrPrefix(
                                            RawString, 
                                            Params->LimitStringsSize
                                        );
                                    } else if (
                                        TypeKind == EVAL_TYPE_KIND_ARRAY && 
                                        PtrData->Type->Count
                                    ) {
                                        RawString = StrPrefix(
                                            RawString, 
                                            PtrData->Type->Count
                                        );
                                    }

                                    b32 StringIsEscapedAndQuoted = (
                                        !(Params->Flags & EV_STR_FLAG_DISABLE_STR_QUOTES) ||
                                        Depth > 0
                                    );
                                    Str8 StringEscapedAndQuoted = RawString;

                                    if (StringIsEscapedAndQuoted) {
                                        Str8 StringEscaped = EVEscapedFromRawStr(
                                            Scratch.MemPool,
                                            RawString
                                        );

                                        StringEscapedAndQuoted = ArenaPushStrFmt(
                                            Scratch.MemPool,
                                            "\"%S\"",
                                            PRINT_STR(StringEscaped)
                                        );
                                    }

                                    *String = ArenaPushStrFmt(
                                        MemPool,
                                        "%S%S",
                                        PRINT_STR(PrePrefix),
                                        PRINT_STR(StringEscapedAndQuoted)
                                    );
                                    PtrData->DidPrefixContent = TRUE;
                                    PtrData->DidPrefixString = TRUE;
                                    ReleaseScratch(Scratch);
                                }

                                if (
                                    TypeKind == EVAL_TYPE_KIND_ARRAY && 
                                    !PtrData->DidPrefixContent
                                ) {
                                    NewTaskRequired = TRUE;
                                    PopRequired = FALSE;
                                    NewTask.Params = *Params;
                                    NewTask.Eval = Eval;
                                    NewTask.RedirectToSetsAndStructs = TRUE;
                                    PtrData->DidRedirect = TRUE;
                                } else if (
                                    TypeKind == EVAL_TYPE_KIND_ARRAY &&
                                    PtrData->DidPrefixContent
                                ) {
                                } else {
                                    PopRequired = FALSE;
                                }
                            } break;

                            case 2: {
                                TempArena Scratch = GetScratch(&MemPool, 1);
                                Str8 PtrValueString = StrFromU64(
                                    Scratch.MemPool,
                                    PtrData->ValueEval.Value.U64,
                                    PtrData->ValueEval.Value.U64
                                        ? 16
                                        : 10,
                                    0,
                                    0
                                );

                                if (
                                    !PtrData->DidPrePrefixPtr &&
                                    !(Params->Flags & EV_STR_FLAG_READ_ONLY_DISPLAY_RULES) &&
                                    !PtrData->DidPrefixContent &&
                                    PtrData->PTeeHasContent
                                ) {
                                    if (PtrData->Type->Count == 1) {
                                        EvalEval DerefEval = EvalEvalWrapFmt(
                                            Eval,
                                            "*$"
                                        );

                                        NewTaskRequired = TRUE;
                                        PopRequired = FALSE;
                                        NewTask.Params = *Params;
                                        NewTask.Eval = DerefEval;
                                    } else {}
                                } else if (
                                    !PtrData->DidPrePrefixPtr &&
                                    !PtrData->DidPrefixContent
                                ) {
                                    *String = ArenaPushStrCpy(
                                        MemPool,
                                        PtrValueString
                                    );
                                }

                                ReleaseScratch(Scratch);
                            } break;
                        }
                    }
                } break;

                case EVAL_TYPE_KIND_STRUCT:
                case EVAL_TYPE_KIND_INCOMPLETE_STRUCT:
                case EVAL_TYPE_KIND_SET:
                ARRAYS_AND_SETS_AND_STRUCTS: {
                    struct EVExpandedTypeData {
                            EvalType*           Type;
                            EvalTypeExpandRule* ExpandRule;
                            EvalTypeExpandInfo  ExpandInfo;
                        };
                        EVExpandedTypeData* ExpandData = (EVExpandedTypeData*) Iter->HeadTask->UserData;

                        if (!ExpandData) {
                            Iter->HeadTask->UserData = ArenaPushArrayZero(
                                MemPool,
                                EVExpandedTypeData,
                                1
                            );
                            ExpandData = (EVExpandedTypeData*) Iter->HeadTask->UserData;
                            ExpandData->Type = EvalTypeFromKey(TypeKey);
                        }

                        switch (TaskIndex) {
                            default: {
                                if (TaskIndex == ExpandData->ExpandInfo.ExprCount + 1) {
                                    *String = ExpansionCloserSymbol;
                                } else {
                                    EvalEval NextEval = *EMPTY_EVAL_EVAL_VALUE;

                                    ExpandData->ExpandRule->ExpandRangeFunc(
                                        MemPool,
                                        ExpandData->ExpandInfo.UserData,
                                        Eval,
                                        Params->Filter,
                                        Rng(TaskIndex - 1, TaskIndex),
                                        &NextEval
                                    );

                                    if (NextEval.Expr != EMPTY_EVAL_EXPR_VALUE) {
                                        NewTaskRequired = TRUE;
                                        PopRequired = FALSE;
                                        NewTask.Params = *Params;
                                        NewTask.Eval = NextEval;

                                        if (TaskIndex > 1)
                                            *String = ", "_s8;
                                    } else {
                                        PopRequired = FALSE;
                                    }
                                }
                            } break;

                            case 0: {
                                if (ExpandData->Type->Flags & EVAL_TYPE_FLAG_STUB_SINGLE_LINE_EXPANSION) {
                                    *String = ArenaPushStrFmt(
                                        MemPool,
                                        "%S...%S",
                                        PRINT_STR(ExpansionOpenerSymbol),
                                        PRINT_STR(ExpansionCloserSymbol)
                                    );
                                } else {
                                    PopRequired = FALSE;
                                    ExpandData->ExpandRule = EvalExpandRuleFromTypeKey(
                                        TypeKey
                                    );
                                    ExpandData->ExpandInfo = ExpandData->ExpandRule->ExpandInfoFunc(
                                        MemPool,
                                        Eval,
                                        Params->Filter
                                    );
                                    *String = ExpansionOpenerSymbol;
                                }
                            } break;
                        }
                } break;
            }
        }
    }

    if (Iter->HeadTask)
        ++Iter->HeadTask->Index;

    if (Result && PopRequired) {
        EVStrIterTask* Task = Iter->HeadTask;

        SLL_STACK_POP(Iter->HeadTask);
        SLL_STACK_PUSH(Iter->FreeTask, Task);
    }

    if (Result && NewTaskRequired) {
        EVStrIterTask* NewT = Iter->FreeTask;

        if (NewT)
            SLL_STACK_POP(Iter->FreeTask);
        else
            NewT = ArenaPushArrayZero(MemPool, EVStrIterTask, 1);

        MemCpy(NewT, &NewTask, sizeof(NewTask));
        NewT->Depth = HeadTaskDepth + !PopRequired;
        SLL_STACK_PUSH(Iter->HeadTask, NewT);
        NewT->Index = 0;
    }

    return Result;
}
