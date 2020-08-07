#pragma once

#include "Templates.h"
#include "../Sys/Limits.h"
#include "../Sys/Likely.h"
#include "../Sys/Endian.h"
#include <cmath>
#if LETHE_COMPILER_MSC && LETHE_CPU_X86
#	include <intrin.h>
#endif

#if LETHE_CPU_X86
#	include <xmmintrin.h>
#endif

namespace lethe
{

// Constants

// Float constants
static const Float PI = (Float)3.1415926535897932384626433832795;
static const Float RAD_TO_DEG = (Float)(180.0 / 3.1415926535897932384626433832795);
static const Float DEG_TO_RAD = (Float)(3.1415926535897932384626433832795 / 180.0);

// Double constants
static const Double D_PI = 3.1415926535897932384626433832795;
static const Double D_RAD_TO_DEG = 180.0 / 3.1415926535897932384626433832795;
static const Double D_DEG_TO_RAD = 3.1415926535897932384626433832795 / 180.0;

// FIXME: breaks strict aliasing BUT is very fast
static inline UInt FloatToBinary(const Float &u)
{
	return *(const UInt *)(const char *)&u;
}

static inline ULong DoubleToBinary(const Double &u)
{
	return *(const ULong *)(const char *)&u;
}

static inline Float BinaryToFloat(const UInt &u)
{
	return *(const Float *)(const char *)&u;
}

static inline Double BinaryToDouble(const ULong &u)
{
	return *(const Double *)(const char *)&u;
}

// Fast Abs() for Float/Double
static inline Float Abs(const Float &x)
{
	return std::fabs(x);
}

static inline Double Abs(const Double &x)
{
	return std::fabs(x);
}

template< typename T > static inline T Atan(T x)
{
	return std::atan(x) * (T)D_RAD_TO_DEG;
}

// arctangent of 2d vector in degrees
template< typename T > static inline T Atan2(T y, T x)
{
	return std::atan2(y, x) * (T)D_RAD_TO_DEG;
}

// cosine of angle in degrees
template< typename T > static inline T Cos(T x)
{
	return std::cos(x * (T)D_DEG_TO_RAD);
}

// sine of angle in degrees
template< typename T > static inline T Sin(T x)
{
	return std::sin(x * (T)D_DEG_TO_RAD);
}

// tangent of angle in degrees
template< typename T > static inline T Tan(T x)
{
	return std::tan(x * (T)D_DEG_TO_RAD);
}

// acos
template< typename T > static inline T ACos(T x)
{
	return std::acos(Clamp(x, (T)-1, (T)1)) * (T)D_RAD_TO_DEG;
}

// asin
template< typename T > static inline T ASin(T x)
{
	return std::asin(Clamp(x, (T)-1, (T)1)) * (T)D_RAD_TO_DEG;
}

// arctangent of 2d vector in radians
template< typename T > static inline Float Atan2Rad(T y, T x)
{
	return std::atan2(y, x);
}

// cosine of angle in radians
template< typename T > static inline T CosRad(T x)
{
	return std::cos(x);
}

// sine of angle in radians
template< typename T> static inline T SinRad(T x)
{
	return std::sin(x);
}

// tangent of angle in radians
template< typename T > static inline T TanRad(T x)
{
	return std::tan(x);
}

// acos
template< typename T > static inline T ACosRad(T x)
{
	return std::acos(Clamp(x, (T)-1, (T)1));
}

// asin
template< typename T > static inline T ASinRad(T x)
{
	return std::asin(Clamp(x, (T)-1, (T)1));
}

template< typename T > static inline T Sqrt(T x)
{
	return (T)std::sqrt(x);
}

#if LETHE_CPU_X86
// this is to avoid stupid errno handling on negative x
inline Float Sqrt(Float x)
{
	Float res;
	__m128 tmp = _mm_set_ss(x);
	tmp = _mm_sqrt_ss(tmp);
	_mm_store_ss(&res, tmp);
	return res;
}
#endif

template< typename T > static inline T FMod(T f, T m)
{
	return std::fmod(f, m);
}

template< typename T > static inline T Exp(T x)
{
	return std::exp(x);
}

template< typename T > static inline T Pow(T x, T y)
{
	return std::pow(x, y);
}

// natural logarithm (base e)
template< typename T > static inline T Ln(T x)
{
	return std::log(x);
}

// base-2 logarithm
template< typename T > static inline T Log2(T x)
{
	return Ln(x) * (T)1.442695040888963407359924681001892137426645954152985934135449;
}

// base-10 logarithm
template< typename T > static inline T Log10(T x)
{
	return Ln(x) * (T)0.434294481903251827651128918916605082294397005803666566114453;
}

// very slow cube root using Pow but allows negative numbers
template< typename T > static inline T Cbrt(T x)
{
	return Pow(Abs(x), (T)(1.0/3.0)) * Sign(x);
}

// Special: float conversions

// floor (rounds down, floor(-2.1) = -3
template< typename T > static inline T Floor(T f)
{
	return (T)floor(f);
}

// ceil (rounds up, ceil(-2.1) = -2
template< typename T > static inline T Ceil(T f)
{
	return (T)ceil(f);
}

// return integral part (=truncate)
template< typename T > static inline T Trunc(T f)
{
	return f < (T)0 ? (T)ceil(f) : (T)floor(f);
}

// round
template< typename T > static inline T Round(T f)
{
	return Floor(f + (T)0.5);
}

template< typename T > static inline T Frac(T f)
{
	// return fractional part
	return f - Floor(f);
}

// modulo
template< typename T > static inline T Mod(T x, T y)
{
	return (T)std::fmod(x, y);
}

// NOTE: truncation/rounding may not handle 0.5 as expected (performance reasons)
// I've seen Intel compiler in the past round towards nearest integer!
// this truncates float to integer; i.e. rounds towards zero
static inline Int FloatToInt(Float f)
{
	return (Int)f;
}

// half float support
Float LETHE_API HalfToFloat(UShort h);
UShort LETHE_API FloatToHalf(Float f);

static inline Int RoundFloatToInt(Float f)
{
	static const Float ofs[2] = {0.5f, -0.5f};
	return FloatToInt(f + ofs[FloatToBinary(f) >> 31]);
}

static inline Float IntToFloat(Int i)
{
	return (Float)i;
}

// this truncates double to integer
static inline Int DoubleToInt(Double d)
{
	return (Int)d;
}

static inline Int RoundDoubleToInt(Double d)
{
	static const Double ofs[2] = {0.5, -0.5};
	return DoubleToInt(d + ofs[d<0]);
}

static inline Double IntToDouble(Int i)
{
	return (Double)i;
}

static inline bool IsAlmostZero(Float f, Float eps = Limits<Float>::EpsilonDenormal())
{
	return Abs(f) <= eps;
}

static inline bool IsAlmostZero(Double d, Double eps = Limits<Double>::EpsilonDenormal())
{
	return Abs(d) <= eps;
}

static inline bool IsAlmostEqual(Float f, Float g, Float eps = Limits<Float>::EpsilonDenormal())
{
	return Abs(f-g) <= eps;
}

static inline bool IsAlmostEqual(Double d, Double e, Float eps = Limits<Float>::EpsilonDenormal())
{
	return Abs(d-e) <= eps;
}

static inline bool IsMax(Float f)
{
	return f != f || Abs(f) >= Limits<Float>::Max();
}

static inline bool IsMax(Double d)
{
	return d != d || Abs(d) >= Limits<Double>::Max();
}

// solve quadratic equation
// must use Float or Double
// x1 is always <= x2 on success
template< typename T >
static bool SolveQuadraticEquation(T a, T b, T c, T &x1, T &x2)
{
	// FIXME: use IsAlmostZero?
	T a2 = (T)2*a;
	LETHE_RET_FALSE_UNLIKELY(a2);
	T D = Sqr(b) - (T)4*a*c;
	LETHE_RET_FALSE_UNLIKELY(D >= (T)0);
	T sqD = Sqrt(D);
	// dividing here instead of multiplying by reciprocal, we want more precision
	x1 = (-b + sqD) / a2;
	x2 = (-b - sqD) / a2;

	// make sure x1 is smaller
	if (x1 > x2)
		Swap(x1, x2);

	return 1;
}

template< typename T >
static bool SolveCubicEquation(T a, T b, T c, T d, T &x1, T &x2, T &x3)
{
	// FIXME: use IsAlmostZero?
	// reference: http://www.1728.org/cubic2.htm
	LETHE_RET_FALSE_UNLIKELY(a);
	T a2 = Sqr(a);
	T b2 = Sqr(b);
	T a3 = a2*a;
	T b3 = b2*b;
	T f = ((T)3*c/a - b2/a2) / (T)3;
	T g = ((T)2*b3/a3 - (T)9*b*c/a2 + (T)27*d/a) / (T)27;
	T h = Sqr(g)/(T)4 + Sqr(f)*f / (T)27;
	T i = Sqrt(Sqr(g)/(T)4 - h);
	LETHE_RET_FALSE_UNLIKELY(i);
	T j = Cbrt(i);
	T k0 = -g / ((T)2*i);
	LETHE_RET_FALSE_UNLIKELY(k0 >= (T)-1 && k0 <= (T)1);
	T k = ACosRad(k0);
	T l = -j;
	T m = CosRad(k / (T)3);
	T n = Sqrt((T)3) * SinRad(k / (T)3);
	T p = -b / ((T)3 * a);

	x1 = (T)2*j*CosRad(k/(T)3) - (b / ((T)3*a));
	x2 = l * (m + n) + p;
	x3 = l * (m - n) + p;

	// and finally sort them
	if (x1 > x2)
		Swap(x1, x2);

	if (x1 > x3)
		Swap(x1, x3);

	if (x2 > x3)
		Swap(x2, x3);

	return 1;
}

}
