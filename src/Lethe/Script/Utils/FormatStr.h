#pragma once

#include "../Common.h"

#include <Lethe/Core/String/String.h>
#include <Lethe/Script/TypeInfo/DataTypes.h>

namespace lethe
{

class Stack;
class StringBuilder;

// helper for printf-like functions
String LETHE_API FormatStr(const Stack &stk);
String LETHE_API FormatStr(const Stack &stk, Int &ofs);
StringBuilder LETHE_API FormatStrBuilder(const Stack &stk, Int &ofs);

// analyze format string for errors
void LETHE_API AnalyzeFormatStr(const String &str, Array<DataTypeEnum> &types);

// compatible fmt types?
bool LETHE_API FormatTypeOk(DataTypeEnum src, DataTypeEnum dst);

}
