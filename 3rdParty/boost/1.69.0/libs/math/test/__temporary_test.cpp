//
// This test file exists to output diagnostic info for tests failing in the online matrix
// for perplexing reasons, it's contents are subject to constant change!!
//
#define BOOST_MATH_INSTRUMENT

#include <boost/math/special_functions/math_fwd.hpp>
#include <iostream>

int main()
{
   std::cout << "EllintD(1, -1) = " << std::setprecision(20) << boost::math::ellint_d(1.0, -1.0) << std::endl;
   std::cout << "EllintD(6.4851474761962890625e-01L , -7.6733188629150390625e+00L) = " << std::setprecision(20) << boost::math::ellint_d(6.4851474761962890625e-01, -7.6733188629150390625e+00) << std::endl;

   std::cout << "EllintD(1, -1) = " << std::setprecision(20) << boost::math::ellint_d(1.0L, -1.0L) << std::endl;
   std::cout << "EllintD(6.4851474761962890625e-01L , -7.6733188629150390625e+00L) = " << std::setprecision(20) << boost::math::ellint_d(6.4851474761962890625e-01L , -7.6733188629150390625e+00L) << std::endl;
}

