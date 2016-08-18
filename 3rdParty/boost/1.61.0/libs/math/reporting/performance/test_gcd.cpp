//  Copyright Jeremy Murphy 2016.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#  pragma warning (disable : 4224)
#endif

#include "../../test/table_type.hpp"
#include "table_helper.hpp"
#include "performance.hpp"

#include <boost/math/common_factor_rt.hpp>
#include <boost/math/special_functions/prime.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random.hpp>
#include <boost/array.hpp>
#include <iostream>
#include <algorithm>
#include <numeric>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
#include <functional>

using namespace std;

boost::multiprecision::cpp_int total_sum(0);

template <typename Func, class Table>
double exec_timed_test_foo(Func f, const Table& data, double min_elapsed = 0.5)
{
    double t = 0;
    unsigned repeats = 1;
    typename Table::value_type::first_type sum{0};
    stopwatch<boost::chrono::high_resolution_clock> w;
    do
    {
       for(unsigned count = 0; count < repeats; ++count)
       {
          for(typename Table::size_type n = 0; n < data.size(); ++n)
            sum += f(data[n].first, data[n].second);
       }

        t = boost::chrono::duration_cast<boost::chrono::duration<double>>(w.elapsed()).count();
        if(t < min_elapsed)
            repeats *= 2;
    }
    while(t < min_elapsed);
    total_sum += sum;
    return t / repeats;
}


template <typename T>
struct test_function_template
{
    vector<pair<T, T> > const & data;
    const char* data_name;
    
    test_function_template(vector<pair<T, T> > const &data, const char* name) : data(data), data_name(name) {}
    
    template <typename Function>
    void operator()(pair<Function, string> const &f) const
    {
        auto result = exec_timed_test_foo(f.first, data);
        report_execution_time(result, 
                            string("gcd method comparison with ") + compiler_name() + string(" on ") + platform_name(), 
                            string("gcd<") + data_name + string(">"), 
                            string(f.second) + "\n" + boost_name());
    }
};

boost::random::mt19937 rng;
boost::random::uniform_int_distribution<> d_0_6(0, 6);
boost::random::uniform_int_distribution<> d_1_20(1, 20);

template <class T>
T get_random_arg()
{
   int n_primes = d_0_6(rng);
   switch(n_primes)
   {
   case 0:
      // Generate a power of 2:
      return static_cast<T>(1u) << d_1_20(rng);
   case 1:
      // prime number:
      return boost::math::prime(d_1_20(rng) + 3);
   }
   T result = 1;
   for(int i = 0; i < n_primes; ++i)
      result *= boost::math::prime(d_1_20(rng) + 3) * boost::math::prime(d_1_20(rng) + 3) * boost::math::prime(d_1_20(rng) + 3) * boost::math::prime(d_1_20(rng) + 3) * boost::math::prime(d_1_20(rng) + 3);
   return result;
}

template <class T>
void test_type(const char* name)
{
   using namespace boost::math::detail;
   typedef T int_type;
   std::vector<pair<int_type, int_type> > data, data2;

   for(unsigned i = 0; i < 1000; ++i)
   {
      data.push_back(std::make_pair(get_random_arg<T>(), get_random_arg<T>()));
   }
   // uncomment and change data to data2 below to test one specific value:
   //data2.assign(data.size(), data[1]);


   //pair<int_type, int_type> test_data{ 1836311903, 2971215073 }; // 46th and 47th Fibonacci numbers. 47th is prime.
   
   typedef pair< function<int_type(int_type, int_type)>, string> f_test;
   array<f_test, 2> test_functions{ { { gcd_binary<int_type>, "gcd_binary" } ,{ gcd_euclidean<int_type>, "gcd_euclidean" } } };
   for_each(begin(test_functions), end(test_functions), test_function_template<int_type>(data, name));
}


int main()
{
    test_type<unsigned short>("unsigned short");
    test_type<unsigned>("unsigned");
    test_type<unsigned long>("unsigned long");
    test_type<unsigned long long>("unsigned long long");

    test_type<boost::multiprecision::uint256_t>("boost::multiprecision::uint256_t");
    test_type<boost::multiprecision::uint512_t>("boost::multiprecision::uint512_t");
    test_type<boost::multiprecision::uint1024_t>("boost::multiprecision::uint1024_t");
}
