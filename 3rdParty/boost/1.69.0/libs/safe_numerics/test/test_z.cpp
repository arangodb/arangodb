
#if 0
auto val()
{
  return -0xFFFFFFFF;
}

#include <stdexcept>
#include <iostream>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_literal.hpp>

void val0(){
    const boost::safe_numerics::safe<unsigned int> x{0};
    std::cout << x << std::endl;
    std::cout << -x << std::endl;
    auto y = -x;
    std::cout << y << std::endl;
}

constexpr boost::safe_numerics::safe<unsigned int> val1()
{
    constexpr boost::safe_numerics::safe<unsigned int> x = 0xFFFFFFFF;
    return -x;
}
constexpr boost::safe_numerics::safe<unsigned int> val2()
{
    boost::safe_numerics::safe<unsigned int> x = - boost::safe_numerics::safe_unsigned_literal<0xFFFFFFFF>();
    return x;
}

constexpr boost::safe_numerics::safe<unsigned int> val3()
{
    return - boost::safe_numerics::safe_unsigned_literal<0xFFFFFFFF>();
}

int main(){
    val0();
    std::cout << val1() << std::endl;
    std::cout << val2() << std::endl;
    std::cout << val3() << std::endl;
    return 0;
}

// test utility
#include <boost/safe_numerics/utility.hpp>

int main(){
    using namespace boost::safe_numerics;
    using x = unsigned_stored_type<0, 42>;
    print_type<x> p1;

    return 0;
}

// test automatic type promotion
#include <boost/safe_numerics/automatic.hpp>
#include <boost/safe_numerics/safe_integer.hpp>
#include <type_traits>
#include <cstdint>
#include <iostream>

int main(){
    using namespace boost::safe_numerics;
    using ar = automatic::addition_result<std::uint8_t, std::uint8_t>;
    static_assert(
        std::is_same<ar::type, std::uint16_t>::value,
        "sum of two 8 bit unsigned integers should fit in on 16 bit unsigned integer"
    );
    return 0;
}


// test automatic type promotion
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_range.hpp>
#include <boost/safe_numerics/safe_integer_literal.hpp>
#include <boost/safe_numerics/automatic.hpp>
#include <type_traits>
#include <cstdint>
#include <iostream>

int main(){
    using namespace boost::safe_numerics;
    unsigned char t1 = 1;
    constexpr const safe_unsigned_literal<42, automatic, default_exception_policy> v2;
    using result_type = decltype(t1 + v2);

    static_assert(
        std::is_same<
            result_type,
            safe_unsigned_range<42, 297, automatic, default_exception_policy>
        >::value,
        "result type should have a range 42-297"
    );
    return 0;
}
void f1(){
    using namespace boost::safe_numerics;
    constexpr safe<int> j = 0;
    constexpr safe<int> k = 3;
    constexpr safe<int> l = j + k; // compile error
}

void f2(){
    using namespace boost::safe_numerics;
    constexpr safe<int> j = boost::safe_numerics::safe_signed_literal<0>();
    constexpr safe<int> k = boost::safe_numerics::safe_signed_literal<3>();
    constexpr safe<int> l = j + k; // compile error
}

void f3(){
    using namespace boost::safe_numerics;
    constexpr auto j = safe_signed_literal<0, native, loose_trap_policy>();
    constexpr auto k = safe_signed_literal<3>();
    constexpr const safe<int> l = j + k;
}

void f4(){
    using namespace boost::safe_numerics;
    safe_signed_literal<0, native, loose_trap_policy> j;
    safe_signed_literal<3> k;
    constexpr auto l = safe_signed_literal<3>();
    constexpr const safe<int> l2 = j + k;
}

#include <boost/safe_numerics/interval.hpp>

int main(){
    return 0;
}

#include <boost/safe_numerics/utility.hpp>
#include <boost/safe_numerics/cpp.hpp>
#include <boost/safe_numerics/safe_common.hpp>

using pic16_promotion = boost::safe_numerics::cpp<
    8,  // char
    8,  // short
    8,  // int
    16, // long
    32  // long long
>;

#include <type_traits>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/range_value.hpp>
#include <iostream>

int main(){
    using namespace boost::safe_numerics;
    static_assert(
        std::is_literal_type<safe<int>>::value,
        "safe type is a literal type"
    );
    static_assert(
        std::is_literal_type<interval<int>>::value,
        "interval type is a literal type"
    );
    static_assert(
        std::is_literal_type<interval<
            safe<int>
        >>::value,
        "interval of safe types is a literal type"
    );
    static_assert(
        std::is_literal_type<range_value<
            safe<int>
        >>::value,
        "range_value of safe types is a literal type"
    );
    safe<int> x = 42;
    std::cout << make_range_value(x);
    return 0;
}

auto val()
{
  return -0xFFFFFFFF;
}

#include <stdexcept>
#include <iostream>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/safe_integer_literal.hpp>

void val0(){
    const boost::safe_numerics::safe<unsigned int> x{0};
    std::cout << x << std::endl;
    std::cout << -x << std::endl;
    auto y = -x;
    std::cout << y << std::endl;
}

constexpr boost::safe_numerics::safe<unsigned int> val1(){
    constexpr boost::safe_numerics::safe<unsigned int> x = 0xFFFFFFFF;
    return -x;
}
constexpr boost::safe_numerics::safe<unsigned int> val2(){
    const boost::safe_numerics::safe<unsigned int> x
        = -boost::safe_numerics::safe_unsigned_literal<0xFFFFFFFF>();
    return x;
}
constexpr boost::safe_numerics::safe<unsigned int> val3(){
    return - boost::safe_numerics::safe_unsigned_literal<0xFFFFFFFF>();
}

int main(){
    val0();
    std::cout << val1() << std::endl;
    std::cout << val2() << std::endl;
    std::cout << val3() << std::endl;
    return 0;
}

#include <boost/logic/tribool.hpp>
#include <boost/safe_integer/checked_integer.hpp>
#include <boost/safe_integer/checked_result.hpp>
#include <boost/safe_integer/checked_result_operations.hpp>
#include <boost/safe_integer/interval.hpp>

namespace boost {
namespace safe_numerics {

template<class EP, typename R>
constexpr void
dispatch(const checked_result<R> & cr){
}

template<class T>
constexpr T base_value(const T & t){
    return t;
}

template<typename R, R Min, R Max, typename T, typename E>
struct validate_detail {

    constexpr static const interval<checked_result<R>> t_interval{
        checked::cast<R>(base_value(std::numeric_limits<T>::min())),
        checked::cast<R>(base_value(std::numeric_limits<T>::max()))
    };
    constexpr static const interval<checked_result<R>> r_interval{Min, Max};

/*
    static_assert(
        ! static_cast<bool>(r_interval.excludes(t_interval)),
        "ranges don't overlap: can't cast"
    );
*/

    struct exception_possible {
        constexpr static R return_value(
            const T & t
        ){
            static_assert(
                ! static_cast<bool>(r_interval.includes(t_interval)),
                "exeption not possible"
            );
            // INT08-C
            const checked_result<R> r = checked::cast<R>(t);
            dispatch<E>(r);
            return base_value(r);
        }
    };
    struct exception_not_possible {
        constexpr static R return_value(
            const T & t
        ){
            static_assert(
                static_cast<bool>(r_interval.includes(t_interval)),
                "exeption not possible"
            );
            return static_cast<R>(t);
        }
    };

    static R return_value(const T & t){
        return std::conditional<
            static_cast<bool>(r_interval.includes(t_interval)),
            exception_not_possible,
            exception_possible
        >::type::return_value(t);
    }
};

template<typename R, R Min, R Max, typename T>
bool test1(const T & t){
    const interval<checked_result<R>> t_interval{
        checked::cast<R>(base_value(std::numeric_limits<T>::min())),
        checked::cast<R>(base_value(std::numeric_limits<T>::max()))
    };
    const interval<checked_result<R>> r_interval{Min, Max};

/*
    static_assert(
        ! static_cast<bool>(r_interval.excludes(t_interval)),
        "ranges don't overlap: can't cast"
    );
*/
    const boost::logic::tribool tb1 = r_interval.includes(t_interval);
    const bool x1 = tb1;

    const boost::logic::tribool tb2 = r_interval.excludes(t_interval);
    const bool x2 = tb2;
    return x2;
}


} // safe_numerics
} // boost

int main(){
    unsigned int x1 = boost::safe_numerics::test1<
        unsigned int, 0, 100, signed char
    >(-1);
    bool x2 = boost::safe_numerics::validate_detail<
        unsigned int, 0, 100, signed char, void
    >::return_value(-1);
    return 0;
}

using uint8_t = unsigned char;

enum class safe_numerics_error : uint8_t {
    success = 0,
    failure,    // result is above representational maximum
    error_count
};

template<typename R>
struct checked_result {
    const safe_numerics_error m_e;
    const union {
        const R m_r;
        char const * const m_msg;
    };
    constexpr /*explicit*/ checked_result(const R & r) :
        m_e(safe_numerics_error::success),
        m_r(r)
    {}
    constexpr /*explicit*/ checked_result(const safe_numerics_error & e) :
        m_e(e),
        m_msg("")
    {}
};

// integers addition
template<class T>
constexpr inline checked_result<T> operator+(
    const checked_result<T> & t,
    const checked_result<T> & u
){
#if 1  // compile fails
    constexpr const safe_numerics_error x[2][2]{
        // t == success
        {
            // u == ...
            safe_numerics_error::success,
            safe_numerics_error::failure
        },
        // t == positive_overflow_error,
        {
            // u == ...
            safe_numerics_error::success,
            safe_numerics_error::failure
        }
    };

    // "Constexpr variable 'e' must be initialized by a constant expression"
    constexpr const safe_numerics_error e = x
        [static_cast<uint8_t>(t.m_e)]
        [static_cast<uint8_t>(u.m_e)]
    ;

    return
        safe_numerics_error::success == e
        ? t.m_r + u.m_r
        : checked_result<T>(e)
    ;
#else  // works as expected
    constexpr const safe_numerics_error x[2][2]{
        // t == success
        {
            // u == ...
            safe_numerics_error::success,
            safe_numerics_error::failure
        },
        // t == failure,
        {
            // u == ...
            safe_numerics_error::failure,
            safe_numerics_error::failure
        }
    };

    return
        safe_numerics_error::success == x
        [static_cast<uint8_t>(t.m_e)]
        [static_cast<uint8_t>(u.m_e)]
        ? t.m_r + u.m_r
        : checked_result<T>(x
        [static_cast<uint8_t>(t.m_e)]
        [static_cast<uint8_t>(u.m_e)]
        )
    ;
#endif
}

int main(){
    constexpr const checked_result<unsigned> i = 0;
    constexpr const checked_result<unsigned> j = 0;

    constexpr const checked_result<unsigned> k = i + j;

    // return k.m_r;

    constexpr const checked_result<unsigned> i2 = safe_numerics_error::failure;
    constexpr const checked_result<unsigned> j2 = 0;

    constexpr const checked_result<unsigned> k2 = i2 + j2;
    return k2.m_r;
}
#endif

#if 0
using uint8_t = unsigned char;


#if 1
enum class safe_numerics_error : uint8_t {
    success = 0,
    failure,    // result is above representational maximum
    error_count
};
#else
// avoiding enum class fails to solve problem
struct safe_numerics_error {
    const uint8_t m_t;
    constexpr const static uint8_t success = 0;
    constexpr const static uint8_t failure = 1;
    constexpr safe_numerics_error(uint8_t t) :
        m_t(t)
    {}
    constexpr operator uint8_t () const {
        return m_t;
    }
};
#endif

template<typename R>
struct checked_result {
    const safe_numerics_error m_e;
    const union {
        const R m_r;
        char const * const m_msg;
    };
    constexpr /*explicit*/ checked_result(const R & r) :
        m_e(safe_numerics_error::success),
        m_r(r)
    {}
    constexpr /*explicit*/ checked_result(const safe_numerics_error & e) :
        m_e(e),
        m_msg("")
    {}
};

// integers addition
template<class T>
constexpr inline checked_result<T> operator+(
    const checked_result<T> & t,
    const checked_result<T> & u
){
    // "Constexpr variable 'e' must be initialized by a constant expression"
    constexpr const safe_numerics_error x[2][2]{
        // t == success
        {
            // u == ...
            safe_numerics_error::success,
            safe_numerics_error::failure
        },
        // t == positive_overflow_error,
        {
            // u == ...
            safe_numerics_error::failure,
            safe_numerics_error::failure
        }
    };

#if 1  // compile fails
    const safe_numerics_error e = x
        [static_cast<uint8_t>(t.m_e)]
        [static_cast<uint8_t>(u.m_e)]
    ;

    return
        (safe_numerics_error::success == e)
        ? t.m_r + u.m_r
        : checked_result<T>(e)
    ;
#else  // works as expected
    return
        safe_numerics_error::success == x
            [static_cast<uint8_t>(t.m_e)]
            [static_cast<uint8_t>(u.m_e)]
        ? t.m_r + u.m_r
        : checked_result<T>(x
            [static_cast<uint8_t>(t.m_e)]
            [static_cast<uint8_t>(u.m_e)]
        )
    ;
#endif
}

int main(){
    constexpr const checked_result<unsigned> i = 0;
    constexpr const checked_result<unsigned> j = 0;

    //constexpr const checked_result<unsigned> k = i + j;
    // return k.m_r;

    constexpr const checked_result<unsigned> i2 = safe_numerics_error::failure;
    constexpr const checked_result<unsigned> j2 = 0;

    constexpr const checked_result<unsigned> k2 = i2 + j2;
    return 0;
}

#endif

#if 0
//#include "safe_common.hpp>
//#include "checked_result.hpp>
//#include "checked_default.hpp>
#include <cassert>
#include <boost/logic/tribool.hpp>

#include <iostream>

// note: Don't reorder these.  Code in the file checked_result_operations.hpp
// depends upon this order !!!
enum class safe_numerics_error : std::uint8_t {
    success = 0,
    positive_overflow_error,    // result is above representational maximum
    negative_overflow_error,    // result is below representational minimum
    domain_error,               // one operand is out of valid range
    range_error,                // result cannot be produced for this operation
    precision_overflow_error,   // result lost precision
    underflow_error,            // result is too small to be represented
    negative_value_shift,       // negative value in shift operator
    negative_shift,             // shift a negative value
    shift_too_large,            // l/r shift exceeds variable size
    uninitialized_value         // l/r shift exceeds variable size
};

// checked result is an "extended version" of the type R.  That is it's domain is
// the domain of R U possible other values which might result from arithmetic
// operations.  An example of such a value would be safe_error::positive_overflow_error.
template<typename R>
struct checked_result {
    const safe_numerics_error m_e;
    const union {
        R m_r;
        char const * m_msg;
    };
    
    constexpr /*explicit*/ checked_result(const R & r) :
        m_e(safe_numerics_error::success),
        m_r(r)
    {}
    constexpr /*explicit*/ checked_result(
        safe_numerics_error e,
        const char * msg = ""
    ) :
        m_e(e),
        m_msg(msg)
    {
        assert(m_e != safe_numerics_error::success);
    }
    constexpr bool exception() const {
        return m_e != safe_numerics_error::success;
    }

    // don't permit construction without initial value;
    checked_result() = delete;

    // disallow assignment
    checked_result & operator=(const checked_result &) = delete;
};

// all arithmetic operations of type T are supported on checked_result<T>.
// but the results might surprising.  For example


constexpr signed int test_constexpr(
    const checked_result<signed int> & t,
    const checked_result<signed int> & u
){
    const boost::logic::tribool tb2 = t < u;
    const signed int x = (tb2) ? 2 : 3;
    return x;
}

using namespace boost::safe_numerics;

int main()
{
    constexpr const checked_result<signed int> po = safe_numerics_error::positive_overflow_error;
    constexpr const checked_result<signed int> no = safe_numerics_error::negative_overflow_error;
    constexpr const boost::logic::tribool tb = no < po;
    const boost::logic::tribool tb1 = no > po;
    constexpr const checked_result<signed int> re = safe_numerics_error::range_error;
    const boost::logic::tribool tb2 = no < re;
    const checked_result<signed int> x = no < re ? no : re;

    static_assert(test_constexpr(no, re) == 3, "test_constexpr(no, re)");


    static_assert(tb, "no < po");

    signed int result;
    if(tb)
        result = 0;
    else
        result = 1;
    std::cout << result;
    return result;
}

#endif

#if 0

#include <boost/logic/tribool.hpp>
#include <cassert>
int main(){
    constexpr const boost::tribool tb_t{true};
    static_assert(tb_t, "tb_t");
    assert(static_cast<bool>(tb_t));
    constexpr boost::tribool tb_f{false};
    static_assert(! tb_f, "tb_f");
    assert(! static_cast<bool>(tb_f));
    return 0;
}
#endif

#if 0
#include <boost/integer.hpp>
#include <boost/safe_numerics/utility.hpp>

// include headers to support safe integers
#include <boost/safe_numerics/cpp.hpp>
//#include <boost/safe_numerics/exception.hpp>

using promotion_policy = boost::safe_numerics::cpp<
    8,  // char      8 bits
    16, // short     16 bits
    16, // int       16 bits
    16, // long      32 bits
    32  // long long 32 bits
>;

template<typename R, typename T, typename U>
struct test {
    using ResultType = promotion_policy::result_type<T,U>;
    //boost::safe_numerics::utility::print_type<ResultType> pt;
    static_assert(
        std::is_same<R, ResultType>::value,
        "is_same<R, ResultType>"
    );
};

test<std::uint16_t, std::uint8_t, std::uint8_t> t1;

int main(){
    return 0;
}

#endif

#if 0
#include <string>
#include <unordered_map>
#include <boost/safe_numerics/safe_integer.hpp>

#include <functional> // hash

template<typename T>
struct safe_hash {
    size_t operator()(boost::safe_numerics::safe<T> const& t) const  {
        return std::hash<T>()(t);
    }
};

int main(){
    auto foo = std::unordered_map<
        boost::safe_numerics::safe<int>,
        std::string,
        safe_hash<int>
    >{};
    foo[boost::safe_numerics::safe<int>(42)] = "hello, world!";
    foo[42] = "hello, world!";
}

#endif

#if 0

#include <string>
#include <unordered_map>
#include <boost/safe_numerics/safe_integer.hpp>

#include <functional> // hash

template<typename T>
struct safe_hash {
    size_t operator()(boost::safe_numerics::safe<T> const& t) const  {
        return std::hash<T>()(t);
    }
};

int main(){
    auto foo = std::unordered_map<int, std::string>{};
    foo[boost::safe_numerics::safe<int>(42)] = "hello, world!";
}

#endif

#if 0

#include <iostream>
#include <boost/safe_numerics/safe_integer.hpp>
#include <boost/safe_numerics/automatic.hpp>

using namespace boost::safe_numerics;

int main(){
    using safe_int = safe<
        int,
        automatic,
        loose_trap_policy
    >; 
    safe_int i;
    std::cin >> i; // might throw exception
    auto j = i * i;
        // won't ever trap
        // result type can hold the maximum value of i * i
    static_assert(is_safe<decltype(j)>::value, "result is a safe type");
    static_assert(
        std::numeric_limits<decltype(i * i)>::max() >=
        std::numeric_limits<safe_int>::max() * std::numeric_limits<safe_int>::max(),
        "result can never overflow"
    ); // always true

    return 0;
}
#endif

int main(){}
