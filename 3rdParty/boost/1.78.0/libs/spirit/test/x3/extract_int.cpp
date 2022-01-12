/*=============================================================================
  Copyright (c) 2018 Nikita Kniazev

  Distributed under the Boost Software License, Version 1.0. (See accompanying
  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <boost/spirit/home/x3/support/numeric_utils/extract_int.hpp>

#include <boost/core/lightweight_test.hpp>
#include <cmath> // for std::pow
#include <cstdio>
#include <iosfwd>
#include <limits>

#ifdef _MSC_VER
# pragma warning(disable: 4127)   // conditional expression is constant
#endif

template <int Min, int Max>
struct custom_int
{
    custom_int() = default;
    constexpr custom_int(int value) : value_{value} {}

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

    static constexpr int max = Max;
    static constexpr int min = Min;

private:
    int value_;
};

namespace utils {

template <int Min, int Max> struct digits;
template <> struct digits<-9,  9>  { static constexpr int r2 = 3, r10 = 1; };
template <> struct digits<-10, 10> { static constexpr int r2 = 3, r10 = 1; };
template <> struct digits<-15, 15> { static constexpr int r2 = 3, r10 = 1; };

}

namespace std {

template <int Min, int Max>
class numeric_limits<custom_int<Min, Max>> : public numeric_limits<int>
{
public:
    static constexpr custom_int<Min, Max> max() noexcept { return Max; }
    static constexpr custom_int<Min, Max> min() noexcept { return Min; }
    static constexpr custom_int<Min, Max> lowest() noexcept { return min(); }
    static_assert(numeric_limits<int>::radix == 2, "hardcoded for digits of radix 2");
    static constexpr int digits = utils::digits<Min, Max>::r2;
    static constexpr int digits10 = utils::digits<Min, Max>::r10;
};

}

namespace x3 = boost::spirit::x3;

template <typename T, int Base, int MaxDigits>
void test_overflow_handling(char const* begin, char const* end, int i)
{
    // Check that parser fails on overflow
    static_assert(std::numeric_limits<T>::is_bounded, "tests prerequest");
    BOOST_ASSERT_MSG(MaxDigits == -1 || static_cast<int>(std::pow(float(Base), MaxDigits)) > T::max,
                     "test prerequest");
    int initial = Base - i % Base; // just a 'random' non-equal to i number
    T x { initial };
    char const* it = begin;
    bool r = x3::extract_int<T, Base, 1, MaxDigits>::call(it, end, x);
    if (T::min <= i && i <= T::max) {
        BOOST_TEST(r);
        BOOST_TEST(it == end);
        BOOST_TEST_EQ(x, i);
    }
    else
        if (MaxDigits == -1) // TODO: Looks like a regression. See #430
    {
        BOOST_TEST(!r);
        BOOST_TEST(it == begin);
    }
}

template <typename T, int Base>
void test_unparsed_digits_are_not_consumed(char const* it, char const* end, int i)
{
    // Check that unparsed digits are not consumed
    static_assert(T::min <= -Base+1, "test prerequest");
    static_assert(T::max >=  Base-1, "test prerequest");
    bool has_sign = *it == '+' || *it == '-';
    auto len = end - it;
    int initial = Base - i % Base; // just a 'random' non-equal to i number
    T x { initial };
    bool r = x3::extract_int<T, Base, 1, 1>::call(it, end, x);
    BOOST_TEST(r);
    if (-Base < i && i < Base) {
        BOOST_TEST(it == end);
        BOOST_TEST_EQ(x, i);
    }
    else {
        BOOST_TEST_EQ(end - it, len - 1 - has_sign);
        BOOST_TEST_EQ(x, i / Base);
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
}

int main()
{
    for (int i = -30; i <= 30; ++i) {
        char s[4];
        std::snprintf(s, 4, "%d", i);
        auto begin = &s[0], end = begin + std::strlen(begin);

        // log(Base, abs(MinOrMax) + 1) == digits
        run_tests<custom_int<-9, 9>, 10>(begin, end, i);
        // (MinOrMax % Base) == 0
        run_tests<custom_int<-10, 10>, 10>(begin, end, i);
        // (MinOrMax % Base) != 0
        run_tests<custom_int<-15, 15>, 10>(begin, end, i);
    }

    return boost::report_errors();
}
