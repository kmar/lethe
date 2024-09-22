#pragma once

#include "../Common.h"

#include <Lethe/Core/Ptr/RefCounted.h>
#include <Lethe/Core/Collect/Array.h>
#include <Lethe/Core/Lexer/Token.h>

namespace lethe
{

LETHE_API_BEGIN

struct LETHE_API Attributes : RefCounted
{
	// attributes are pre-parsed as lexer tokens
	Array<Token> tokens;
};

LETHE_API_END

}
