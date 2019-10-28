//  Boost Sort library test_pdqsort.cpp file  ----------------------------//

//  Copyright Orson Peters 2017. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/sort for library home page.


#include <vector>
#include <string>
#include <random>

#include <boost/sort/pdqsort/pdqsort.hpp>
// Include unit test framework
#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/test_tools.hpp>


// Gives a vector containing strings with the same order.
std::string u32_to_str(uint32_t n) {
    std::string r;
    for (int i = 0; i < 32; ++i) {
        r = char('0' + (n & 1)) + r;
        n >>= 1;
    }
    return r;
}

std::vector<std::string> vec_u32_to_str(const std::vector<uint32_t>& v) {
    std::vector<std::string> r; r.reserve(v.size());
    for (size_t i = 0; i < v.size(); ++i) {
        r.push_back(u32_to_str(v[i]));
    }
    return r;
}


// Distributions.
std::vector<uint32_t> shuffled(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = 0; i < size; ++i) v.push_back(i);
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

std::vector<uint32_t> shuffled_16_values(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = 0; i < size; ++i) v.push_back(i % 16);
    std::shuffle(v.begin(), v.end(), rng);
    return v;
}

std::vector<uint32_t> all_equal(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = 0; i < size; ++i) v.push_back(0);
    return v;
}

std::vector<uint32_t> ascending(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = 0; i < size; ++i) v.push_back(i);
    return v;
}

std::vector<uint32_t> descending(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = size - 1; ; --i) {
        v.push_back(i);
        if (i == 0) break;
    }
    return v;
}

std::vector<uint32_t> pipe_organ(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = 0; i < size/2; ++i) v.push_back(i);
    for (uint32_t i = size/2; i < size; ++i) v.push_back(size - i);
    return v;
}

std::vector<uint32_t> push_front(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = 1; i < size; ++i) v.push_back(i);
    v.push_back(0);
    return v;
}

std::vector<uint32_t> push_middle(size_t size, std::mt19937_64& rng) {
    std::vector<uint32_t> v; v.reserve(size);
    for (uint32_t i = 0; i < size; ++i) {
        if (i != size/2) v.push_back(i);
    }
    v.push_back(size/2);
    return v;
}


// Tests.
typedef std::vector<uint32_t> (*DistrF)(size_t, std::mt19937_64&); 
void execute_test(DistrF distr, std::string distr_name, int repeats) {
    // In C++14 we'd just use std::less<>().
    std::less<uint32_t> u32less;
    std::greater<uint32_t> u32greater;
    std::less<std::string> sless;
    std::greater<std::string> sgreater;

    for (size_t sz = 1; sz <= 1000; sz *= 10) {
        // Consistent but pseudorandom tests.
        std::mt19937_64 rng; rng.seed(0);

        for (int i = 0; i < repeats; ++i) {
            std::vector<uint32_t> u32l = distr(sz, rng);
            std::vector<uint32_t> u32g = u32l;
            std::vector<std::string> sl = vec_u32_to_str(u32l);
            std::vector<std::string> sg = sl;

            boost::sort::pdqsort(u32l.begin(), u32l.end(), u32less);
            boost::sort::pdqsort(u32g.begin(), u32g.end(), u32greater);
            boost::sort::pdqsort(sl.begin(), sl.end(), sless);
            boost::sort::pdqsort(sg.begin(), sg.end(), sgreater);

            BOOST_CHECK_MESSAGE(
                std::is_sorted(u32l.begin(), u32l.end(), u32less),
                "pdqsort<uint32_t, std::less> " + distr_name + " failed with size " + std::to_string(sz)
            ); 
            
            BOOST_CHECK_MESSAGE(
                std::is_sorted(u32g.begin(), u32g.end(), u32greater),
                "pdqsort<uint32_t, std::greater> " + distr_name + " failed with size " + std::to_string(sz)
            ); 
            
            BOOST_CHECK_MESSAGE(
                std::is_sorted(sl.begin(), sl.end(), sless),
                "pdqsort<std::string, std::less> " + distr_name + " failed with size " + std::to_string(sz)
            ); 
            
            BOOST_CHECK_MESSAGE(
                std::is_sorted(sg.begin(), sg.end(), sgreater),
                "pdqsort<std::string, std::greater> " + distr_name + " failed with size " + std::to_string(sz)
            ); 
        }
    }
}


// test main 
int test_main(int argc, char** argv) {
    // No unused warning.
    (void) argc; (void) argv;

    execute_test(shuffled, "shuffled", 100);
    execute_test(shuffled_16_values, "shuffled_16_values", 100);
    execute_test(all_equal, "all_equal", 1);
    execute_test(ascending, "ascending", 1);
    execute_test(descending, "descending", 1);
    execute_test(pipe_organ, "pipe_organ", 1);
    execute_test(push_front, "push_front", 1);
    execute_test(push_middle, "push_middle", 1);

    return 0;
}
