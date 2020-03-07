#pragma once

namespace lethe
{

template<typename T>
void CustomDeleter_(const T *ptr)
{
	delete reinterpret_cast<const unsigned char *>(ptr);
}

}