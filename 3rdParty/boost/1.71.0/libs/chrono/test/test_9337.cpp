//#define BOOST_CHRONO_PROVIDES_DEPRECATED_IO_SINCE_V2_0_0
#include <iostream>
#include <unistd.h>
#include <boost/chrono.hpp>
#include <boost/chrono/chrono_io.hpp>

using namespace std;
using namespace boost::chrono;

template <typename Clock>
void test()
{
  typename Clock::time_point start=Clock::now();
  sleep(1);
  typename Clock::time_point stop=Clock::now();
  cout<<stop-start<<endl;
  cout << start <<endl;
  cout << stop <<endl;
}

int main() {
  test<process_cpu_clock>();
  test<process_real_cpu_clock>();
  test<process_user_cpu_clock>();
  test<process_system_cpu_clock>();
  return 1;
}
