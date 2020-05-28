///////////////////////////////////////////////////////////////
//  Copyright 2018 Nick Thompson. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

/*`This example demonstrates the usage of the MPC backend for multiprecision complex numbers.
In the following, we will show how using MPC backend allows for the same operations as the C++ standard library complex numbers.
*/

//[mpc_eg
#include <iostream>
#include <complex>
#include <boost/multiprecision/mpc.hpp>

template<class Complex>
void complex_number_examples()
{
    Complex z1{0, 1};
    std::cout << std::setprecision(std::numeric_limits<typename Complex::value_type>::digits10);
    std::cout << std::scientific << std::fixed;
    std::cout << "Print a complex number: " << z1 << std::endl;
    std::cout << "Square it             : " << z1*z1 << std::endl;
    std::cout << "Real part             : " << z1.real() << " = " << real(z1) << std::endl;
    std::cout << "Imaginary part        : " << z1.imag() << " = " << imag(z1) << std::endl;
    using std::abs;
    std::cout << "Absolute value        : " << abs(z1) << std::endl;
    std::cout << "Argument              : " << arg(z1) << std::endl;
    std::cout << "Norm                  : " << norm(z1) << std::endl;
    std::cout << "Complex conjugate     : " << conj(z1) << std::endl;
    std::cout << "Projection onto Riemann sphere: " <<  proj(z1) << std::endl;
    typename Complex::value_type r = 1;
    typename Complex::value_type theta = 0.8;
    using std::polar;
    std::cout << "Polar coordinates (phase = 0)    : " << polar(r) << std::endl;
    std::cout << "Polar coordinates (phase !=0)    : " << polar(r, theta) << std::endl;

    std::cout << "\nElementary special functions:\n";
    using std::exp;
    std::cout << "exp(z1) = " << exp(z1) << std::endl;
    using std::log;
    std::cout << "log(z1) = " << log(z1) << std::endl;
    using std::log10;
    std::cout << "log10(z1) = " << log10(z1) << std::endl;
    using std::pow;
    std::cout << "pow(z1, z1) = " << pow(z1, z1) << std::endl;
    using std::sqrt;
    std::cout << "Take its square root  : " << sqrt(z1) << std::endl;
    using std::sin;
    std::cout << "sin(z1) = " << sin(z1) << std::endl;
    using std::cos;
    std::cout << "cos(z1) = " << cos(z1) << std::endl;
    using std::tan;
    std::cout << "tan(z1) = " << tan(z1) << std::endl;
    using std::asin;
    std::cout << "asin(z1) = " << asin(z1) << std::endl;
    using std::acos;
    std::cout << "acos(z1) = " << acos(z1) << std::endl;
    using std::atan;
    std::cout << "atan(z1) = " << atan(z1) << std::endl;
    using std::sinh;
    std::cout << "sinh(z1) = " << sinh(z1) << std::endl;
    using std::cosh;
    std::cout << "cosh(z1) = " << cosh(z1) << std::endl;
    using std::tanh;
    std::cout << "tanh(z1) = " << tanh(z1) << std::endl;
    using std::asinh;
    std::cout << "asinh(z1) = " << asinh(z1) << std::endl;
    using std::acosh;
    std::cout << "acosh(z1) = " << acosh(z1) << std::endl;
    using std::atanh;
    std::cout << "atanh(z1) = " << atanh(z1) << std::endl;
}

int main()
{
    std::cout << "First, some operations we usually perform with std::complex:\n";
    complex_number_examples<std::complex<double>>();
    std::cout << "\nNow the same operations performed using the MPC backend:\n";
    complex_number_examples<boost::multiprecision::mpc_complex_50>();

    return 0;
}
//]

/*

//[mpc_out

Print a complex number: (0.00000000000000000000000000000000000000000000000000,1.00000000000000000000000000000000000000000000000000)
Square it             : -1.00000000000000000000000000000000000000000000000000
Real part             : 0.00000000000000000000000000000000000000000000000000 = 0.00000000000000000000000000000000000000000000000000
Imaginary part        : 1.00000000000000000000000000000000000000000000000000 = 1.00000000000000000000000000000000000000000000000000
Absolute value        : 1.00000000000000000000000000000000000000000000000000
Argument              : 1.57079632679489661923132169163975144209858469968755
Norm                  : 1.00000000000000000000000000000000000000000000000000
Complex conjugate     : (0.00000000000000000000000000000000000000000000000000,-1.00000000000000000000000000000000000000000000000000)
Projection onto Riemann sphere: (0.00000000000000000000000000000000000000000000000000,1.00000000000000000000000000000000000000000000000000)
Polar coordinates (phase = 0)    : 1.00000000000000000000000000000000000000000000000000
Polar coordinates (phase !=0)    : (0.69670670934716538906374002277244853473117519431538,0.71735609089952279256716781570337728075604730751255)

Elementary special functions:
exp(z1) = (0.54030230586813971740093660744297660373231042061792,0.84147098480789650665250232163029899962256306079837)
log(z1) = (0.00000000000000000000000000000000000000000000000000,1.57079632679489661923132169163975144209858469968755)
log10(z1) = (0.00000000000000000000000000000000000000000000000000,0.68218817692092067374289181271567788510506374186196)
pow(z1, z1) = 0.20787957635076190854695561983497877003387784163177
Take its square root  : (0.70710678118654752440084436210484903928483593768847,0.70710678118654752440084436210484903928483593768847)
sin(z1) = (0.00000000000000000000000000000000000000000000000000,1.17520119364380145688238185059560081515571798133410)
cos(z1) = 1.54308063481524377847790562075706168260152911236587
tan(z1) = (0.00000000000000000000000000000000000000000000000000,0.76159415595576488811945828260479359041276859725794)
asin(z1) = (0.00000000000000000000000000000000000000000000000000,0.88137358701954302523260932497979230902816032826163)
acos(z1) = (1.57079632679489661923132169163975144209858469968755,-0.88137358701954302523260932497979230902816032826163)
atan(z1) = (0.00000000000000000000000000000000000000000000000000,inf)
sinh(z1) = (0.00000000000000000000000000000000000000000000000000,0.84147098480789650665250232163029899962256306079837)
cosh(z1) = 0.54030230586813971740093660744297660373231042061792
tanh(z1) = (0.00000000000000000000000000000000000000000000000000,1.55740772465490223050697480745836017308725077238152)
asinh(z1) = (0.00000000000000000000000000000000000000000000000000,1.57079632679489661923132169163975144209858469968755)
acosh(z1) = (0.88137358701954302523260932497979230902816032826163,1.57079632679489661923132169163975144209858469968755)
atanh(z1) = (0.00000000000000000000000000000000000000000000000000,0.78539816339744830961566084581987572104929234984378)

//]
*/
