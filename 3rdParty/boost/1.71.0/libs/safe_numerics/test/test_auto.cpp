#include <iostream>
#include <cassert>
#include <boost/core/demangle.hpp>

#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_range.hpp>
#include <boost/safe_numerics/automatic.hpp>
#include <boost/safe_numerics/utility.hpp>

int test_log(){
    using namespace boost::safe_numerics::utility;
    assert(ilog2(127u) == 6);
    assert(ilog2(128u) == 7);
    assert(ilog2(129u) == 7);
    assert(ilog2(255u) == 7);
    assert(ilog2(256u) == 8);

    assert(ilog2(127) == 6);
    assert(ilog2(128) == 7);
    assert(ilog2(129) == 7);
    assert(ilog2(255) == 7);
    assert(ilog2(256) == 8);
    return 0;
}

template<class T>
void print_argument_type(const T & t){
    const std::type_info & ti = typeid(T);
    std::cout
        << boost::core::demangle(ti.name()) << ' ' << t << std::endl;
}

template<typename T, typename U>
int test_auto(const T & t, const U & u){
    using namespace boost::safe_numerics;

    try{
        safe<T, automatic>(t) + u;
    }
    catch(const std::exception &){
        safe<T, automatic>(t) + u;
    }

    try{
        t + safe<U, automatic>(u);
    }
    catch(const std::exception &){
        t + safe<U, automatic>(u);
    }
    return 0;
}

int test_auto_result(){
    using namespace boost::safe_numerics;
    automatic::addition_result<
        safe<std::int8_t, automatic>,
        safe<std::uint16_t, automatic>
    >::type r1;
    print_argument_type(r1);

    automatic::addition_result<
        safe_signed_range<-3, 8, automatic>,
        safe_signed_range<-4, 9, automatic>
    >::type r2;
    print_argument_type(r2);
    return 0;
}

int test_compare_result(){
    using namespace boost::safe_numerics;

    automatic::comparison_result<
        safe<std::int16_t, automatic>,
        safe<std::int16_t, automatic>
    >::type r1;
    print_argument_type(r1);

    return 0;
}

int main(){
    using namespace boost::safe_numerics;

    test_log();
    test_auto<std::int8_t, std::int8_t>(1, -128);
    test_auto_result();
    test_compare_result();
    return 0;
}
