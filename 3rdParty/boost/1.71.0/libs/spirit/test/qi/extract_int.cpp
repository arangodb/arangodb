/*=============================================================================
  Copyright (c) 2018 Nikita Kniazev

  Distributed under the Boost Software License, Version 1.0. (See accompanying
  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/detail/lightweight_test.hpp>
#include <boost/spirit/home/qi/numeric/numeric_utils.hpp>
#include <boost/static_assert.hpp>
#include <cmath> // for std::pow
#include <iosfwd>
#include <limits>
#include <sstream>

template <int Min, int Max>
struct custom_int
{
    BOOST_DEFAULTED_FUNCTION(custom_int(), {})
    BOOST_CONSTEXPR custom_int(int value) : value_(value) {}

    custom_int operator+(custom_int x) const { return value_ + x.value_; }
    custom_int operator-(custom_int x) const { return value_ - x.value_; }
    custom_int operator*(custom_int x) const { return value_ * x.value_; }
    custom_int operator/(custom_int x) const { return value_ / x.value_; }

    custom_int& operator+=(custom_int x) { value_ += x.value_; return *this; }
    custom_int& operator-=(custom_int x) { value_ -= x.value_; return *this; }
    custom_int& operator*=(custom_int x) { value_ *= x.value_; return *this; }
    custom_int& operator/=(custom_int x) { value_ /= x.value_; return *this; }
    custom_int& operator++() { ++value_; return *this; }
    custom_int& operator--() { --value_; return *this; }
    custom_int operator++(int) { return value_++; }
    custom_int operator--(int) { return value_--; }

    custom_int operator+() { return +value_; }
    custom_int operator-() { return -value_; }

    bool operator< (custom_int x) const { return value_ <  x.value_; }
    bool operator> (custom_int x) const { return value_ >  x.value_; }
    bool operator<=(custom_int x) const { return value_ <= x.value_; }
    bool operator>=(custom_int x) const { return value_ >= x.value_; }
    bool operator==(custom_int x) const { return value_ == x.value_; }
    bool operator!=(custom_int x) const { return value_ != x.value_; }

    template <typename Char, typename Traits>
    friend std::basic_ostream<Char, Traits>&
    operator<<(std::basic_ostream<Char, Traits>& os, custom_int x) {
        return os << x.value_;
    }

    BOOST_STATIC_CONSTEXPR int max = Max;
    BOOST_STATIC_CONSTEXPR int min = Min;

private:
    int value_;
};

namespace utils {

template <int Min, int Max> struct digits;
template <> struct digits<-9,  9>  { BOOST_STATIC_CONSTEXPR int r2 = 3, r10 = 1; };
template <> struct digits<-10, 10> { BOOST_STATIC_CONSTEXPR int r2 = 3, r10 = 1; };
template <> struct digits<-15, 15> { BOOST_STATIC_CONSTEXPR int r2 = 3, r10 = 1; };

}

namespace std {

template <int Min, int Max>
class numeric_limits<custom_int<Min, Max> > : public numeric_limits<int>
{
public:
    static BOOST_CONSTEXPR custom_int<Min, Max> max() BOOST_NOEXCEPT_OR_NOTHROW { return Max; }
    static BOOST_CONSTEXPR custom_int<Min, Max> min() BOOST_NOEXCEPT_OR_NOTHROW { return Min; }
    static BOOST_CONSTEXPR custom_int<Min, Max> lowest() BOOST_NOEXCEPT_OR_NOTHROW { return min(); }
    BOOST_STATIC_ASSERT_MSG(numeric_limits<int>::radix == 2, "hardcoded for digits of radix 2");
    BOOST_STATIC_CONSTEXPR int digits = utils::digits<Min, Max>::r2;
    BOOST_STATIC_CONSTEXPR int digits10 = utils::digits<Min, Max>::r10;
};

}

namespace qi = boost::spirit::qi;

template <typename T, int Base, int MaxDigits>
void test_overflow_handling(char const* begin, char const* end, int i)
{
    // Check that parser fails on overflow
    BOOST_STATIC_ASSERT_MSG(std::numeric_limits<T>::is_bounded, "tests prerequest");
    BOOST_ASSERT_MSG(MaxDigits == -1 || static_cast<int>(std::pow(float(Base), MaxDigits)) > T::max,
                     "test prerequest");
    int initial = Base - i % Base; // just a 'random' non-equal to i number
    T x(initial);
    char const* it = begin;
    bool r = qi::extract_int<T, Base, 1, MaxDigits>::call(it, end, x);
    if (T::min <= i && i <= T::max) {
        BOOST_TEST(r);
        BOOST_TEST(it == end);
        BOOST_TEST_EQ(x, i);
    }
    else {
        BOOST_TEST(!r);
        BOOST_TEST(it == begin);
    }
}

template <typename T, int Base>
void test_unparsed_digits_are_not_consumed(char const* it, char const* end, int i)
{
    // Check that unparsed digits are not consumed
    BOOST_STATIC_ASSERT_MSG(T::min <= -Base+1, "test prerequest");
    BOOST_STATIC_ASSERT_MSG(T::max >=  Base-1, "test prerequest");
    bool has_sign = *it == '+' || *it == '-';
    char const* begin = it;
    int initial = Base - i % Base; // just a 'random' non-equal to i number
    T x(initial);
    bool r = qi::extract_int<T, Base, 1, 1>::call(it, end, x);
    BOOST_TEST(r);
    if (-Base < i && i < Base) {
        BOOST_TEST(it == end);
        BOOST_TEST_EQ(x, i);
    }
    else {
        BOOST_TEST_EQ(end - it, (end - begin) - 1 - has_sign);
        BOOST_TEST_EQ(x, i / Base);
    }
}

template <typename T, int Base>
void test_ignore_overflow_digits(char const* it, char const* end, int i)
{
    // TODO: Check accumulating too?
    if (i < 0) return; // extract_int does not support IgnoreOverflowDigits

    bool has_sign = *it == '+' || *it == '-';
    char const* begin = it;
    int initial = Base - i % Base; // just a 'random' non-equal to i number
    T x(initial);
    BOOST_TEST((qi::extract_uint<T, Base, 1, -1, false, true>::call(it, end, x)));
    if (T::min <= i && i <= T::max) {
        BOOST_TEST(it == end);
        BOOST_TEST_EQ(x, i);
    }
    else {
        BOOST_TEST_EQ(it - begin, (qi::detail::digits_traits<T, Base>::value) + has_sign);
        if (Base == std::numeric_limits<T>::radix)
            BOOST_TEST_EQ(it - begin, std::numeric_limits<T>::digits + has_sign);
        if (Base == 10)
            BOOST_TEST_EQ(it - begin, std::numeric_limits<T>::digits10 + has_sign);
        int expected = i;
        for (char const* p = it; p < end; ++p) expected /= Base;
        BOOST_TEST_EQ(x, expected);
    }
}

template <typename T, int Base>
void run_tests(char const* begin, char const* end, int i)
{
    // Check that parser fails on overflow
    test_overflow_handling<T, Base, -1>(begin, end, i);
    // Check that MaxDigits > digits10 behave like MaxDigits=-1
    test_overflow_handling<T, Base, 2>(begin, end, i);
    // Check that unparsed digits are not consumed
    test_unparsed_digits_are_not_consumed<T, Base>(begin, end, i);
    // Check that IgnoreOverflowDigits does what we expect
    test_ignore_overflow_digits<T, Base>(begin, end, i);
}

int main()
{
    for (int i = -30; i <= 30; ++i) {
        std::ostringstream oss;
        oss << i;
        std::string s = oss.str();
        char const* begin = s.data(), *const end = begin + s.size();

        // log(Base, abs(MinOrMax) + 1) == digits
        run_tests<custom_int<-9, 9>, 10>(begin, end, i);
        // (MinOrMax % Base) == 0
        run_tests<custom_int<-10, 10>, 10>(begin, end, i);
        // (MinOrMax % Base) != 0
        run_tests<custom_int<-15, 15>, 10>(begin, end, i);
    }

    return boost::report_errors();
}
