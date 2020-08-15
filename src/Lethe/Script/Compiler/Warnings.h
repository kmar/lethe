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
	WARN_NRVO_PREVENTED
};

}
