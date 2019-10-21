//  benchmark.cpp  ---------------------------------------------------------------------//

//  Copyright Beman Dawes 2011

//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

#ifndef _SCL_SECURE_NO_WARNINGS
# define _SCL_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif

#include <cstdlib>
#include <boost/endian/conversion.hpp>
#include <boost/random.hpp>
#include <boost/cstdint.hpp>
#include <boost/timer/timer.hpp>
#include <iostream>
#include <string>

using namespace boost;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;

namespace
{
  std::string command_args;
  long long n_cases;
  int places = 2;
  bool verbose (false);

#ifndef BOOST_TWO_ARG
  typedef int32_t (*timee_func)(int32_t);
#else
  typedef void (*timee_func)(int32_t, int32_t&);
#endif

  typedef  boost::timer::nanosecond_type nanosecond_t;

//--------------------------------------------------------------------------------------//

  nanosecond_t benchmark(timee_func timee, const char* msg,
    nanosecond_t overhead = 0)
  //  Returns: total cpu time (i.e. system time + user time)
  {
    if (verbose)
      cout << "\nRunning benchmark..." << endl;
    int64_t sum = 0;
    boost::timer::cpu_times times;
    nanosecond_t cpu_time;
    boost::timer::auto_cpu_timer t(places);

    for (long long i = n_cases; i; --i)
    {
#   ifndef BOOST_TWO_ARG
      sum += timee(static_cast<int32_t>(i)) ;
#   else
      int32_t y;
      timee(static_cast<int32_t>(i), y);
      sum += y;
#   endif
    }
    t.stop();
    times = t.elapsed();
    cpu_time = (times.system + times.user) - overhead;
    const long double sec = 1000000000.0L;
    cout.setf(std::ios_base::fixed, std::ios_base::floatfield);
    cout.precision(places);
    cout << msg << " " << cpu_time / sec << endl;

    if (verbose)
    {
      t.report();
      cout << "  Benchmark complete\n"
              "    sum is " << sum << endl;
    }
    return cpu_time;
  }

  void process_command_line(int argc, char * argv[])
  {
    for (int a = 0; a < argc; ++a)
    {
      command_args += argv[a];
      if (a != argc-1)
        command_args += ' ';
    }

    cout << command_args << '\n';;

    if (argc >=2)
#ifndef _MSC_VER
      n_cases = std::atoll(argv[1]);
#else
      n_cases = _atoi64(argv[1]);
#endif

    for (; argc > 2; ++argv, --argc)
    {
      if ( *(argv[2]+1) == 'p' )
        places = atoi( argv[2]+2 );
      else if ( *(argv[2]+1) == 'v' )
        verbose = true;
      else
      {
        cout << "Error - unknown option: " << argv[2] << "\n\n";
        argc = -1;
        break;
      }
    }

    if (argc < 2)
    {
      cout << "Usage: benchmark n [Options]\n"
              "  The argument n specifies the number of test cases to run\n"
              "  Options:\n"
              "   -v       Verbose messages\n"
              "   -p#      Decimal places for times; default -p" << places << "\n";
      return std::exit(1);
    }
  }

  inline void inplace(int32_t& x)
  {
    x =  (static_cast<uint32_t>(x) << 24)
      | ((static_cast<uint32_t>(x) << 8) & 0x00ff0000)
      | ((static_cast<uint32_t>(x) >> 8) & 0x0000ff00)
      | (static_cast<uint32_t>(x) >> 24);
  }

  inline int32_t by_return(int32_t x)
  {
    return (static_cast<uint32_t>(x) << 24)
      | ((static_cast<uint32_t>(x) << 8) & 0x00ff0000)
      | ((static_cast<uint32_t>(x) >> 8) & 0x0000ff00)
      | (static_cast<uint32_t>(x) >> 24);
  }

  inline int32_t by_return_intrinsic(int32_t x)
  {
    return BOOST_ENDIAN_INTRINSIC_BYTE_SWAP_4(static_cast<uint32_t>(x));
  }

  inline int32_t by_return_pyry(int32_t x)
  {
    uint32_t step16;
    step16 = static_cast<uint32_t>(x) << 16 | static_cast<uint32_t>(x) >> 16;
    return
        ((static_cast<uint32_t>(step16) << 8) & 0xff00ff00)
      | ((static_cast<uint32_t>(step16) >> 8) & 0x00ff00ff);
  }

  inline int32_t two_operand(int32_t x, int32_t& y)
  {
    return y = ((x << 24) & 0xff000000) | ((x << 8) & 0x00ff0000) | ((x >> 24) & 0x000000ff)
      | ((x >> 8) & 0x0000ff00);
  }

  inline int32_t modify_noop(int32_t x)
  {
    int32_t v(x);
    return v;
  }

  inline int32_t modify_inplace(int32_t x)
  {
    int32_t v(x);
    inplace(v);
    return v;
  }

  inline int32_t modify_by_return(int32_t x)
  {
    int32_t v(x);
    return by_return(v);
  }

  inline int32_t modify_by_return_pyry(int32_t x)
  {
    int32_t v(x);
    return by_return_pyry(v);
  }

  inline int32_t modify_by_return_intrinsic(int32_t x)
  {
    int32_t v(x);
    return by_return_intrinsic(v);
  }

  inline void non_modify_assign(int32_t x, int32_t& y)
  {
    y = x;
  }

  inline void non_modify_two_operand(int32_t x, int32_t& y)
  {
    two_operand(x, y);
  }

  inline void non_modify_by_return(int32_t x, int32_t& y)
  {
    y = by_return(x);
  }

} // unnamed namespace

//-------------------------------------- main()  ---------------------------------------//

int main(int argc, char * argv[])
{
  process_command_line(argc, argv);

  nanosecond_t overhead;

#ifndef BOOST_TWO_ARG
  overhead = benchmark(modify_noop, "modify no-op");
  benchmark(modify_inplace, "modify in place"/*, overhead*/);
  benchmark(modify_by_return, "modify by return"/*, overhead*/);
  benchmark(modify_by_return_pyry, "modify by return_pyry"/*, overhead*/);
  benchmark(modify_by_return_intrinsic, "modify by return_intrinsic"/*, overhead*/);
#else
  overhead = benchmark(non_modify_assign, "non_modify_assign     ");
  benchmark(non_modify_two_operand,       "non_modify_two_operand", overhead);
  benchmark(non_modify_by_return,         "non_modify_by_return  ", overhead);
#endif

  return 0;
}
