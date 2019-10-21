#include "elementwise_operations_test.hpp"

int main()
{

  ///testing row major flaot elementwise operations
  bench_elementwise <float, ublas::basic_row_major<>, 10, 10> b1;

  ///testing row major complex float elementwise operations
  bench_elementwise <std::complex<float>, ublas::basic_row_major<>, 10, 10> b2;

  ///testing row major double elementwise operations
  bench_elementwise <double, ublas::basic_row_major<>, 10, 10> b5;

  ///testing row major complex double elementwise operations
  bench_elementwise <std::complex<double>, ublas::basic_row_major<>, 10, 10> b6;

  ///testing column major flaot elementwise operations
  bench_elementwise <float, ublas::basic_column_major<>, 10, 10> b3;
 
  ///testing column major complex float elementwise operations
  bench_elementwise <std::complex<float>, ublas::basic_column_major<>, 10, 10> b4;

  ///testing column major double elementwise operations
  bench_elementwise <double, ublas::basic_column_major<>, 10, 10> b7;

  ///testing column major complex double elementwise operations
  bench_elementwise <std::complex<double>, ublas::basic_column_major<>, 10, 10> b8;


  std::cout << "row major:" << std::endl;
  b1.run();
  b2.run();
  b5.run();
  b6.run();


  std::cout << "column major:" << std::endl;
  b3.run();
  b4.run();
  b7.run();
  b8.run();
 
}
