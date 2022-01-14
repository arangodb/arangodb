// test performance.cpp : Defines the entry point for the console application.
//

#include <cstdio>
#include <cstdint>
#include <iostream>
#include <chrono>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/integer.hpp>

#include <boost/safe_numerics/safe_integer.hpp>

typedef boost::safe_numerics::safe<unsigned> safe_type;

namespace boost {
namespace multiprecision {

    template <class Integer, class I2>
    typename enable_if_c<boost::safe_numerics::is_safe<Integer>::value, Integer&>::type
    multiply(Integer& result, const I2& a, const I2& b){
        return result = static_cast<Integer>(a) * static_cast<Integer>(b);
    }

    template <class Integer>
    typename enable_if_c<boost::safe_numerics::is_safe<Integer>::value, bool>::type
    bit_test(const Integer& val, unsigned index){
        Integer mask = 1;
        if (index >= sizeof(Integer) * CHAR_BIT)
            return 0;
        if (index)
            mask <<= index;
        return val & mask ? true : false;
    }

    template <class I1, class I2>
    typename enable_if_c<boost::safe_numerics::is_safe<I1>::value, I2>::type
    integer_modulus(const I1& x, I2 val){
        return x % val;
    }

    namespace detail {
        template <class T> struct double_integer;

        template <>
        struct double_integer<safe_type>{
            using type = boost::safe_numerics::safe<std::uint64_t>;
        };
    }

    template <class I1, class I2, class I3>
    typename enable_if_c<boost::safe_numerics::is_safe<I1>::value, I1>::type
    powm(const I1& a, I2 b, I3 c){
        typedef typename detail::double_integer<I1>::type double_type;

        I1 x(1), y(a);
        double_type result;

        while (b > 0){
            if (b & 1){
                multiply(result, x, y);
                x = integer_modulus(result, c);
            }
            multiply(result, y, y);
            y = integer_modulus(result, c);
            b >>= 1;
        }
        return x % c;
    }

    template <class T, class PP, class EP>
    inline unsigned
    lsb(const boost::safe_numerics::safe<T, PP, EP>& x){
        return lsb(static_cast<T>(x));
    }

} }

#include <boost/multiprecision/miller_rabin.hpp>

template <class Clock>
class stopwatch
{
    const typename Clock::time_point m_start;
public:
    stopwatch() :
        m_start(Clock::now())
    {}
    typename Clock::duration elapsed() const {
        return Clock::now() - m_start;
    }
};

template<typename T>
void test(const char * msg){
    const stopwatch<std::chrono::high_resolution_clock> c;

    unsigned count = 0;
    for (T i = 3; i < 30000000; ++i)
        if (boost::multiprecision::miller_rabin_test(i, 25)) ++count;

    std::chrono::duration<double> time = c.elapsed();
    std::cout<< msg << ":\ntime = " << time.count();
    std::cout << "\ncount = " << count << std::endl;
}

int main()
{
    test<unsigned>("Testing type unsigned");
    test<boost::safe_numerics::safe<unsigned>>("Testing type safe<unsigned>");
    return 0;
}

