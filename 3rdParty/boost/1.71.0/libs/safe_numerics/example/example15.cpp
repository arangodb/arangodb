#include <iostream>
#include <limits>

#include <boost/rational.hpp>
#include <boost/safe_numerics/safe_integer.hpp>

int main(int, const char *[]){
    // simple demo of rational library
    const boost::rational<int> r {1, 2};
    std::cout << "r = " << r << std::endl;
    const boost::rational<int> q {-2, 4};
    std::cout << "q = " << q << std::endl;
    // display the product
    std::cout << "r * q = " << r * q << std::endl;

    // problem: rational doesn't handle integer overflow well
    const boost::rational<int> c {1, INT_MAX};
    std::cout << "c = " << c << std::endl;
    const boost::rational<int> d {1, 2};
    std::cout << "d = " << d << std::endl;
    // display the product - wrong answer
    std::cout << "c * d = " << c * d << std::endl;

    // solution: use safe integer in rational definition
    using safe_rational = boost::rational<
        boost::safe_numerics::safe<int>
    >;

    // use rationals created with safe_t
    const safe_rational sc {1, INT_MAX};
    std::cout << "c = " << sc << std::endl;
    const safe_rational sd {1, 2};
    std::cout << "d = " << sd << std::endl;
    std::cout << "c * d = ";
    try {
        // multiply them. This will overflow
        std::cout << sc * sd << std::endl;
    }
    catch (std::exception const& e) {
        // catch exception due to multiplication overflow
        std::cout << e.what() << std::endl;
    }

    return 0;
}
