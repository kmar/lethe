#include <limits>
#include "Limits.h"

namespace lethe
{

const Float FLOAT_INFINITY = std::numeric_limits<Float>::infinity();
const Float FLOAT_NAN = std::numeric_limits<Float>::quiet_NaN();
const Double DOUBLE_INFINITY = std::numeric_limits<Double>::infinity();
const Double DOUBLE_NAN = std::numeric_limits<Double>::quiet_NaN();

}
