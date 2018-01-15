// Boost.Convert test and usage example
// Copyright (c) 2009-2016 Vladimir Batov.
// Use, modification and distribution are subject to the Boost Software License,
// Version 1.0. See http://www.boost.org/LICENSE_1_0.txt.

#include "./test.hpp"

#if defined(BOOST_CONVERT_IS_NOT_SUPPORTED)
int main(int, char const* []) { return 0; }
#else

#include "./prepare.hpp"
#include <boost/convert.hpp>
#include <boost/convert/stream.hpp>
#include <boost/convert/printf.hpp>
#include <boost/convert/strtol.hpp>
#include <boost/convert/spirit.hpp>
#include <boost/convert/lexical_cast.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/timer/timer.hpp>
#include <boost/array.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <cstdlib>
#include <cstdio>

using std::string;
using boost::convert;

namespace cnv = boost::cnv;
namespace arg = boost::cnv::parameter;

namespace { namespace local
{
    template<typename Type>
    struct array
    {
        typedef boost::array<Type, 20> type;
    };
    template<typename T> static typename array<T>::type const& get();

    static int const num_cycles = 1000000;
    int                     sum = 0;

    struct timer : public boost::timer::cpu_timer
    {
        typedef timer                   this_type;
        typedef boost::timer::cpu_timer base_type;

        double value() const
        {
            boost::timer::cpu_times times = base_type::elapsed();
            int const             use_sum = (sum % 2) ? 0 : (sum % 2); BOOST_TEST(use_sum == 0);

            return double(times.user + times.system) / 1000000000 + use_sum;
        }
    };
    template<            typename Type, typename Cnv> static double str_to (Cnv const&);
    template<typename S, typename Type, typename Cnv> static double to_str (Cnv const&);

    template<>
    local::array<int>::type const&
    get<int>()
    {
        static array<int>::type ints;
        static bool           filled;

        if (!filled)
        {
            boost::random::mt19937                     gen (::time(0));
            boost::random::uniform_int_distribution<> dist (INT_MIN, INT_MAX); // INT_MAX(32) = 2,147,483,647

            for (size_t k = 0; k < ints.size(); ++k)
                ints[k] = dist(gen);

            filled = true;
        }
        return ints;
    }
    template<>
    array<long int>::type const&
    get<long int>()
    {
        static array<long int>::type ints;
        static bool                filled;

        if (!filled)
        {
            boost::random::mt19937                     gen (::time(0));
            boost::random::uniform_int_distribution<> dist (INT_MIN, INT_MAX); // INT_MAX(32) = 2147483647

            for (size_t k = 0; k < ints.size(); ++k)
                ints[k] = dist(gen);

            filled = true;
        }
        return ints;
    }
    template<>
    array<double>::type const&
    get<double>()
    {
        static array<double>::type dbls;
        static bool              filled;

        if (!filled)
        {
            boost::random::mt19937                     gen (::time(0));
            boost::random::uniform_int_distribution<> dist (INT_MIN, INT_MAX); // INT_MAX(32) = 2147483647

            for (size_t k = 0; k < dbls.size(); ++k)
                dbls[k] = double(dist(gen)) + 0.7654321;

            filled = true;
        }
        return dbls;
    }
}}

struct raw_str_to_int_spirit
{
    int operator()(char const* str) const
    {
        char const* beg = str;
        char const* end = beg + strlen(str);
        int      result;

        if (boost::spirit::qi::parse(beg, end, boost::spirit::int_, result))
            if (beg == end) // ensure the whole string was parsed
                return result;

        return (BOOST_ASSERT(0), result);
    }
};

struct raw_str_to_int_lxcast
{
    int operator()(char const* str) const
    {
        return boost::lexical_cast<int>(str);
    }
};

template<typename Type, typename Converter>
double
raw_str_to(Converter const& cnv)
{
    local::strings strings = local::get_strs(); // Create strings on the stack
    int const         size = strings.size();
    local::timer     timer;

    for (int t = 0; t < local::num_cycles; ++t)
        for (int k = 0; k < size; ++k)
            local::sum += cnv(strings[k].c_str());

    return timer.value();
}

template<typename Type, typename Converter>
double
local::str_to(Converter const& try_converter)
{
    local::strings strings = local::get_strs(); // Create strings on the stack
    int const         size = strings.size();
    local::timer     timer;

    for (int t = 0; t < local::num_cycles; ++t)
        for (int k = 0; k < size; ++k)
            local::sum += boost::convert<Type>(strings[k].c_str(), try_converter).value();

    return timer.value();
}

template<typename string_type, typename Type, typename Converter>
double
local::to_str(Converter const& try_converter)
{
    typedef typename local::array<Type>::type collection;

    collection  values = local::get<Type>();
    int const     size = values.size();
    local::timer timer;

    for (int t = 0; t < local::num_cycles; ++t)
        for (int k = 0; k < size; ++k)
            local::sum += *boost::convert<string_type>(Type(values[k]), try_converter).value().begin();

    return timer.value();
}

template<typename Converter>
double
performance_str_to_type(Converter const& try_converter)
{
    char const* input[] = { "no", "up", "dn" };
    local::timer  timer;

    for (int k = 0; k < local::num_cycles; ++k)
    {
        change chg = boost::convert<change>(input[k % 3], try_converter).value();
        int    res = chg.value();

        BOOST_TEST(res == k % 3);

        local::sum += res; // Make sure chg is not optimized out
    }
    return timer.value();
}

template<typename Converter>
double
performance_type_to_str(Converter const& try_converter)
{
    boost::array<change, 3>   input = {{ change::no, change::up, change::dn }};
    boost::array<string, 3> results = {{ "no", "up", "dn" }};
    local::timer              timer;

    for (int k = 0; k < local::num_cycles; ++k)
    {
        string res = boost::convert<string>(input[k % 3], try_converter).value();

        BOOST_TEST(res == results[k % 3]);

        local::sum += res[0]; // Make sure res is not optimized out
    }
    return timer.value();
}

template<typename Raw, typename Cnv>
void
performance_comparative(Raw const& raw, Cnv const& cnv, char const* txt)
{
    int const num_tries = 5;
    double     cnv_time = 0;
    double     raw_time = 0;

    for (int k = 0; k < num_tries; ++k) cnv_time += local::str_to<int>(cnv);
    for (int k = 0; k < num_tries; ++k) raw_time += raw_str_to<int>(raw);

    cnv_time /= num_tries;
    raw_time /= num_tries;

    double change = 100 * (1 - cnv_time / raw_time);

    printf("str-to-int: %s raw/cnv=%.2f/%.2f seconds (%.2f%%).\n", txt, raw_time, cnv_time, change);
}

int
main(int, char const* [])
{
    printf("Started performance tests...\n");

    printf("str-to-int: spirit/strtol/lcast/scanf/stream=%7.2f/%7.2f/%7.2f/%7.2f/%7.2f seconds.\n",
           local::str_to<int>(boost::cnv::spirit()),
           local::str_to<int>(boost::cnv::strtol()),
           local::str_to<int>(boost::cnv::lexical_cast()),
           local::str_to<int>(boost::cnv::printf()),
           local::str_to<int>(boost::cnv::cstream()));
    printf("str-to-lng: spirit/strtol/lcast/scanf/stream=%7.2f/%7.2f/%7.2f/%7.2f/%7.2f seconds.\n",
           local::str_to<long int>(boost::cnv::spirit()),
           local::str_to<long int>(boost::cnv::strtol()),
           local::str_to<long int>(boost::cnv::lexical_cast()),
           local::str_to<long int>(boost::cnv::printf()),
           local::str_to<long int>(boost::cnv::cstream()));
    printf("str-to-dbl: spirit/strtol/lcast/scanf/stream=%7.2f/%7.2f/%7.2f/%7.2f/%7.2f seconds.\n",
           local::str_to<double>(boost::cnv::spirit()),
           local::str_to<double>(boost::cnv::strtol()),
           local::str_to<double>(boost::cnv::lexical_cast()),
           local::str_to<double>(boost::cnv::printf()),
           local::str_to<double>(boost::cnv::cstream()));

    printf("int-to-str: spirit/strtol/lcast/prntf/stream=%7.2f/%7.2f/%7.2f/%7.2f/%7.2f seconds.\n",
           local::to_str<std::string, int>(boost::cnv::spirit()),
           local::to_str<std::string, int>(boost::cnv::strtol()),
           local::to_str<std::string, int>(boost::cnv::lexical_cast()),
           local::to_str<std::string, int>(boost::cnv::printf()),
           local::to_str<std::string, int>(boost::cnv::cstream()));
    printf("lng-to-str: spirit/strtol/lcast/prntf/stream=%7.2f/%7.2f/%7.2f/%7.2f/%7.2f seconds.\n",
           local::to_str<std::string, long int>(boost::cnv::spirit()),
           local::to_str<std::string, long int>(boost::cnv::strtol()),
           local::to_str<std::string, long int>(boost::cnv::lexical_cast()),
           local::to_str<std::string, long int>(boost::cnv::printf()),
           local::to_str<std::string, long int>(boost::cnv::cstream()));
    printf("dbl-to-str: spirit/strtol/lcast/prntf/stream=%7.2f/%7.2f/%7.2f/%7.2f/%7.2f seconds.\n",
           local::to_str<std::string, double>(boost::cnv::spirit()),
           local::to_str<std::string, double>(boost::cnv::strtol()(arg::precision = 6)),
           local::to_str<std::string, double>(boost::cnv::lexical_cast()),
           local::to_str<std::string, double>(boost::cnv::printf()(arg::precision = 6)),
           local::to_str<std::string, double>(boost::cnv::cstream()(arg::precision = 6)));

    printf("str-to-user-type: lcast/stream/strtol=%.2f/%.2f/%.2f seconds.\n",
           performance_str_to_type(boost::cnv::lexical_cast()),
           performance_str_to_type(boost::cnv::cstream()),
           performance_str_to_type(boost::cnv::strtol()));
    printf("user-type-to-str: lcast/stream/strtol=%.2f/%.2f/%.2f seconds.\n",
           performance_type_to_str(boost::cnv::lexical_cast()),
           performance_type_to_str(boost::cnv::cstream()),
           performance_type_to_str(boost::cnv::strtol()));

    //[small_string_results
    printf("strtol int-to std::string/small-string: %.2f/%.2f seconds.\n",
           local::to_str<std::string, int>(boost::cnv::strtol()),
           local::to_str<  my_string, int>(boost::cnv::strtol()));
    printf("spirit int-to std::string/small-string: %.2f/%.2f seconds.\n",
           local::to_str<std::string, int>(boost::cnv::spirit()),
           local::to_str<  my_string, int>(boost::cnv::spirit()));
    printf("stream int-to std::string/small-string: %.2f/%.2f seconds.\n",
           local::to_str<std::string, int>(boost::cnv::cstream()),
           local::to_str<  my_string, int>(boost::cnv::cstream()));
    //]
    performance_comparative(raw_str_to_int_spirit(), boost::cnv::spirit(),       "spirit");
    performance_comparative(raw_str_to_int_lxcast(), boost::cnv::lexical_cast(), "lxcast");

    return boost::report_errors();
}

#endif
