#pragma once

#include "../Math/Templates.h"

namespace lethe
{

template< typename T >
void ReverseArray(T buf, T bufe)
{
	--bufe;

	while (buf < bufe)
		Swap(*buf++, *bufe--);
}

// left rotation by mid
template< typename T >
void RotateArray(T buf, T mid, T end)
{
	ReverseArray(buf, mid);
	ReverseArray(mid, end);
	ReverseArray(buf, end);
	// alternatives: Juggling algorithm, Gries-Mills
}

}
