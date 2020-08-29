#pragma once

#include "Types.h"
#include "Assert.h"

// We require 2's complement representation of signed integers!!!

namespace lethe
{

extern const Float FLOAT_INFINITY;
extern const Float FLOAT_NAN;
extern const Double DOUBLE_INFINITY;
extern const Double DOUBLE_NAN;

static inline void Limits_Dummy_Twos_Complement_Validator___()
{
	LETHE_COMPILE_ASSERT((Int)((SByte)-128) == -128);	// need 2's complement integers!
}

// because we have clashes with macro definitions (must resolve later), will use templates

template< typename T > struct Limits
{
};

template<> struct Limits<Byte>
{
	static const Byte MIN = 0;
	static const Byte MAX = 255;
	static inline Byte Min()
	{
		return MIN;
	}
	static inline Byte Max()
	{
		return MAX;
	}
};

template<> struct Limits<SByte>
{
	static const SByte MIN = -128;
	static const SByte MAX = 127;
	static inline SByte Min()
	{
		return MIN;
	}
	static inline SByte Max()
	{
		return MAX;
	}
};

template<> struct Limits<Short>
{
	static const Short MIN = -32768;
	static const Short MAX = 32767;
	static inline Short Min()
	{
		return MIN;
	}
	static inline Short Max()
	{
		return MAX;
	}
};

template<> struct Limits<UShort>
{
	static const UShort MIN = 0;
	static const UShort MAX = 65535;
	static inline UShort Min()
	{
		return MIN;
	}
	static inline UShort Max()
	{
		return MAX;
	}
};

template<> struct Limits<UInt>
{
	static const UInt MIN = 0;
	static const UInt MAX = (UInt)0xffffffffu;
	static inline UInt Min()
	{
		return MIN;
	}
	static inline UInt Max()
	{
		return MAX;
	}
};

template<> struct Limits<Int>
{
	static const Int MIN = -(Int)(Limits<UInt>::MAX/2)-1;
	static const Int MAX = (Int)(Limits<UInt>::MAX/2);
	static inline Int Min()
	{
		return MIN;
	}
	static inline Int Max()
	{
		return MAX;
	}
};

template<> struct Limits<ULong>
{
	static const ULong MIN = 0;
	static const ULong MAX = (ULong)0-1;
	static inline ULong Min()
	{
		return MIN;
	}
	static inline ULong Max()
	{
		return MAX;
	}
};

template<> struct Limits<Long>
{
	static const Long MIN = -(Long)(Limits<ULong>::MAX/2)-1;
	static const Long MAX = (Long)(Limits<ULong>::MAX/2);
	static inline Long Min()
	{
		return MIN;
	}
	static inline Long Max()
	{
		return MAX;
	}
};

template<> struct Limits<Float>
{
	// almost zero (above denormal)
	static inline Float EpsilonDenormal()
	{
		return 1.0e-37f;
	}
	// smallest epsilon such that 1 + eps != 1
	static inline Float Epsilon()
	{
		return 1.192092896e-07f;
	}
	static inline Float Max()
	{
		return 1.0e+37f;
	}
	static inline Float Infinity()
	{
		return FLOAT_INFINITY;
	}
	static inline Float NaN()
	{
		return FLOAT_NAN;
	}
	static inline bool IsNan(Float n)
	{
		return n != n;
	}
};

template<> struct Limits<Double>
{
	// almost zero (above denormal)
	static inline Double EpsilonDenormal()
	{
		return 1.0e-307;
	}
	// smallest epsilon such that 1 + eps != 1
	static inline Double Epsilon()
	{
		return 2.2204460492503131e-016;
	}
	static inline Double Max()
	{
		return 1.0e+307;
	}
	static inline Double Infinity()
	{
		return DOUBLE_INFINITY;
	}
	static inline Double NaN()
	{
		return DOUBLE_NAN;
	}
	static inline bool IsNan(Double n)
	{
		return n != n;
	}
};

}
