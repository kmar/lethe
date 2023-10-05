#pragma once

#include "../Common.h"

namespace lethe
{

enum AstQualifier : ULong
{
	AST_Q_CONST		=	1 << 0,
	AST_Q_STATIC	=	1 << 1,
	AST_Q_NATIVE	=	1 << 2,
	AST_Q_VIRTUAL	=	1 << 3,
	AST_Q_WEAK		=	1 << 4,
	AST_Q_FINAL		=	1 << 5,
	AST_Q_PUBLIC	=	1 << 6,
	AST_Q_PROTECTED	=	1 << 7,
	AST_Q_PRIVATE	=	1 << 8,
	AST_Q_METHOD    =   1 << 9,
	AST_Q_OVERRIDE  =   1 << 10,
	AST_Q_TRANSIENT	=	1 << 11,
	AST_Q_REFERENCE	=	1 << 12,
	// special method qualifiers
	AST_Q_CTOR      =   1 << 13,
	AST_Q_DTOR      =   1 << 14,
	// scope-resolved; no virtual call here
	AST_Q_NON_VIRT  =   1 << 15,
	AST_Q_FORMAT	=	1 << 16,
	// can store full int here (local var)
	AST_Q_LOCAL_INT	=	1 << 17,
	AST_Q_INLINE	=	1 << 18,
	// used for null type to skip dtor
	AST_Q_SKIP_DTOR	=	1 << 19,
	// used to mark noncopyable structs
	AST_Q_NOCOPY	=	1 << 20,
	AST_Q_INTRINSIC	=	1 << 21,
	AST_Q_NOINIT	=	1 << 22,
	// TypeGen helper flag
	AST_Q_REBUILD_MEMBER_TYPES = 1 << 23,
	AST_Q_ASSERT	=	1 << 24,
	AST_Q_EDITABLE	=	1 << 25,
	AST_Q_PLACEABLE	=	1 << 26,
	AST_Q_LATENT	=	1 << 27,
	AST_Q_CONSTEXPR =   1 << 28,
	AST_Q_STATE     =   1 << 29,
	AST_Q_STATEBREAK =  (ULong)1 << 30,
	AST_Q_TEMPLATE  =   (ULong)1 << 31,
	// local variable symbol aliased via reference (because of JIT)
	AST_Q_REF_ALIASED = (ULong)1 << 32,
	// necessary so that return succeeds (virtual assignment to result)
	AST_Q_CAN_MODIFY_CONSTANT = (ULong)1 << 33,
	// tagged instantiated templates
	AST_Q_TEMPLATE_INSTANTIATED = (ULong)1 << 34,
	AST_Q_TYPEDEF_LOCK = (ULong)1 << 35,
	// used to mark enum class
	AST_Q_ENUM_CLASS = (ULong)1 << 36,
	// raw pointer
	AST_Q_RAW = (ULong)1 << 37,
	// marking end_state deferred statement
	AST_Q_NOSTATEBREAK =  (ULong)1 << 38,
	// auto vardecl in range based for, need to infer from comparison instead
	AST_Q_AUTO_RANGE_FOR =  (ULong)1 << 39,
	// nontrivial struct will force to call a ctor before copy (=can't be zero-init constructed)
	AST_Q_NONTRIVIAL = (ULong)1 << 40,
	// don't generate bounds check for this array type
	AST_Q_NOBOUNDS = (ULong)1 << 41,
	// virtual property
	AST_Q_PROPERTY = (ULong)1 << 42,
	// for constant folding to avoid warnings on cast
	AST_Q_NO_WARNINGS = (ULong)1 << 43,
	// referenced function
	AST_Q_FUNC_REFERENCED = (ULong)1 << 44,
	// don't discard result
	AST_Q_NODISCARD = (ULong)1 << 45,
	// for typedefs: symbol alias
	AST_Q_SYMBOL_ALIAS = (ULong)1 << 46,
	// for structs: has gaps due to alignment?
	// if so, we force zero-init for safety reasons
	AST_Q_HAS_GAPS = (ULong)1 << 47,
	// context-resolved symbol, used for enum class values, like if (val == .RED) ...
	AST_Q_CONTEXT_SYMBOL = (ULong)1 << 48,
	// special bitfield type
	AST_Q_BITFIELD = (ULong)1 << 49,
	// arguments tagged with this cannot accept temporary args
	AST_Q_NOTEMP = (ULong)1 << 50,
	// need to tag operators for func overload name gen
	AST_Q_OPERATOR = (ULong)1 << 51,
	// deprecated function
	AST_Q_DEPRECATED = (ULong)1 << 52,
	// note: 11 left

	// func decl inherit mask
	AST_Q_FUNC_MASK	= AST_Q_STATIC | AST_Q_NATIVE | AST_Q_VIRTUAL | AST_Q_FINAL | AST_Q_CTOR | AST_Q_DTOR |
					  AST_Q_PUBLIC | AST_Q_PROTECTED | AST_Q_PRIVATE | AST_Q_METHOD | AST_Q_OVERRIDE | AST_Q_FORMAT | AST_Q_INLINE |
					  AST_Q_INTRINSIC | AST_Q_ASSERT | AST_Q_LATENT | AST_Q_STATE | AST_Q_STATEBREAK | AST_Q_DEPRECATED,
	// include mask for type comparison
	AST_Q_TYPE_CMP_MASK = AST_Q_REFERENCE | AST_Q_CONST
};

}
