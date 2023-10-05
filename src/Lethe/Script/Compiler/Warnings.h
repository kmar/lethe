#pragma once

namespace lethe
{

enum Warnings
{
	WARN_GENERIC,
	// unref_variable
	WARN_UNREFERENCED,
	// conversion may lose precision
	WARN_CONV_PRECISION,
	// missing override
	WARN_MISSING_OVERRIDE,
	// private/protected inheritance not supported
	WARN_PRIV_PROT_INHERIT,
	// noinit used with small elementary type
	WARN_NOINIT_SMALL,
	// variable shadows another variable in outer scope
	WARN_SHADOW,
	// integer constant overflow
	WARN_OVERFLOW,
	// NRVO optimization prevented
	WARN_NRVO_PREVENTED,
	// generic performance warning
	WARN_PERF,
	// discarding result of a nodiscard function call
	WARN_DISCARD,
	// division by zero
	WARN_DIV_BY_ZERO,
	// signed-unsigned comparison
	WARN_SIGNED_UNSIGNED_COMPARISON,
	// out-of-order designated initializer
	WARN_OUT_OF_ORDER_DESIGNATED_INITIALIZER,
	// comparing boolean and number
	WARN_COMPARE_BOOL_AND_NUMBER,
	// calling a deprecated function
	WARN_DEPRECATED
};

}
