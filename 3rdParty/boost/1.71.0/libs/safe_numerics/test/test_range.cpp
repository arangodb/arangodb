#include <iostream>
#include <cassert>
#include <limits>

#include <boost/safe_numerics/utility.hpp>
#include <boost/safe_numerics/safe_integer_range.hpp>

template<typename T>
void display_log(T Max){
    std::cout
        << "ilog2(" << Max << ") = "
        << boost::safe_numerics::utility::ilog2(Max) << std::endl;
}

bool test_significant_bits(){
    using namespace boost::safe_numerics;
    assert(utility::significant_bits(127u) == 7); // 7 bits
    assert(utility::significant_bits(127u) == 7); // 7 bits
    assert(utility::significant_bits(128u) == 8); // 8 bits
    assert(utility::significant_bits(129u) == 8); // 8 bits
    assert(utility::significant_bits(255u) == 8); // 8 bits
    assert(utility::significant_bits(256u) == 9); // 9 bits
    return true;
}

bool test1(){
    using namespace boost::safe_numerics;
    using t2 = safe_unsigned_range<0u, 1000u>;
    static_assert(
        std::numeric_limits<t2>::is_signed == false,
        "this range should be unsigned"
    );
    return true;
}

#include <boost/safe_numerics/automatic.hpp>

template <
    std::intmax_t Min,
    std::intmax_t Max
>
using safe_t = boost::safe_numerics::safe_signed_range<
    Min,
    Max,
    boost::safe_numerics::automatic,
    boost::safe_numerics::default_exception_policy
>;

bool test2(){
    std::cout << "test1" << std::endl;
    try{
        const safe_t<-64, 63> x(1);
        safe_t<-64, 63> y;
        y = 2;
        std::cout << "x = " << x << std::endl;
        std::cout << "y = " << y << std::endl;
        auto z = x + y;
        std::cout << "x + y = ["
            << std::numeric_limits<decltype(z)>::min() << ","
            << std::numeric_limits<decltype(z)>::max() << "] = "
            << z << std::endl;

        auto z2 = x - y;
        std::cout << "x - y = ["
            << std::numeric_limits<decltype(z2)>::min() << ","
            << std::numeric_limits<decltype(z2)>::max() << "] = "
            << z2 << std::endl;

            short int yi, zi;
            yi = y;
            zi = x + yi;
    }
    catch(const std::exception & e){
        // none of the above should trap. Mark failure if they do
        std::cout << e.what() << std::endl;
        return false;
    }
    return true;
}

int main(){
    //using namespace boost::safe_numerics;
    //safe_signed_literal2<100> one_hundred;
    //one_hundred = 200;

    bool rval = 
        test_significant_bits() &&
        test1() &&
        test2() /* &&
        test3() &&
        test4()
        */
    ;
    std::cout << (rval ? "success!" : "failure") << std::endl;
    return rval ? EXIT_SUCCESS : EXIT_FAILURE;
}
