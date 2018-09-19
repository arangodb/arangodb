#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/math/special_functions.hpp>
#include <boost/chrono.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>

template <class Clock>
struct stopwatch
{
   typedef typename Clock::duration duration;
   stopwatch()
   {
      m_start = Clock::now();
   }
   duration elapsed()
   {
      return Clock::now() - m_start;
   }
   void reset()
   {
      m_start = Clock::now();
   }

private:
   typename Clock::time_point m_start;
};

template <class T>
T generate_random()
{
   typedef int e_type;
   static boost::random::mt19937 gen;
   T val = gen();
   T prev_val = -1;
   while(val != prev_val)
   {
      val *= (gen.max)();
      prev_val = val;
      val += gen();
   }
   e_type e;
   val = frexp(val, &e);

   static boost::random::uniform_int_distribution<e_type> ui(-20, 20);
   return ldexp(val, ui(gen));
}


template <typename T>
double my_convert_to_double(const T& x)
{
   double ret = 0;
   if(isfinite(x)) {
      if(x.backend().exponent() >= -1023 - 52 && x != 0) {
         if(x.backend().exponent() <= 1023) {
            int e = x.backend().exponent();
            T y = ldexp(abs(x), 55 - e);
            T t = trunc(y);
            int64_t ti = t.template convert_to<int64_t>();
            if((ti & 1) == 0) {
               if(t < y)
                  ti |= 1;
            }
            if(e >= -1023 + 1) {
               ret = ldexp(double(ti), e - 55);
            }
            else {
               // subnormal
               typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<128, boost::multiprecision::backends::digit_base_2> > cpp_bin_float128_t;
               cpp_bin_float128_t sx = ldexp(cpp_bin_float128_t(ti), e - 55);
               sx += DBL_MIN;
               e = -1023 + 1;
               cpp_bin_float128_t sy = ldexp(sx, 55 - e);
               cpp_bin_float128_t st = trunc(sy);
               ti = st.convert_to<int64_t>();
               if((ti & 1) == 0) {
                  if(st < sy)
                     ti |= 1;
               }
               ret = ldexp(double(ti), e - 55) - DBL_MIN;
            }
         }
         else {
            // overflow
            ret = HUGE_VAL;
         }
      }
   }
   else {
      if(isnan(x))
         return nan("");
      // inf
      ret = HUGE_VAL;
   }
   return x.backend().sign() ? -ret : ret;
}


template <class T>
void test_conversion_time(const char* name)
{
   std::cout << "Testing times for type: " << name << "\n";
   std::vector<T> values;

   for(unsigned i = 0; i < 10000000; ++i)
   {
      values.push_back(generate_random<T>());
   }

   boost::chrono::duration<double> time;
   stopwatch<boost::chrono::high_resolution_clock> c;

   double total = 0;

   for(typename std::vector<T>::const_iterator i = values.begin(); i != values.end(); ++i)
   {
      total += my_convert_to_double(*i);
   }

   time = c.elapsed();
   std::cout << std::setprecision(3) << std::fixed;
   std::cout << "Reference time: " << std::setw(7) << std::right << time << " (total sum = " << total << ")" << std::endl;

   c.reset();

   total = 0;

   for(typename std::vector<T>::const_iterator i = values.begin(); i != values.end(); ++i)
   {
      total += i->template convert_to<double>();
   }

   time = c.elapsed();
   std::cout << "Boost time:     " << std::setw(7) << std::right << time << " (total sum = " << total << ")" << std::endl;


}


int main()
{
   using namespace boost::multiprecision;

   test_conversion_time<cpp_bin_float_double>("cpp_bin_float_double");
   test_conversion_time<cpp_bin_float_quad>("cpp_bin_float_quad");
   test_conversion_time<cpp_bin_float_50>("cpp_bin_float_50");
   test_conversion_time<cpp_bin_float_100>("cpp_bin_float_100");

   return 0;
}

