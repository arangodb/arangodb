#include "transposition_test.hpp"

int main()
{

  //Row-major
  bench_trans<float, ublas::basic_row_major<>, 10, 10> b1;
  bench_trans<double, ublas::basic_row_major<>, 10, 10> b2;
  bench_trans<std::complex<float>, ublas::basic_row_major<>, 10, 10> b3;
  bench_trans<std::complex<double>, ublas::basic_row_major<>, 10, 10> b4;

  //Column-major
  bench_trans<float, ublas::basic_column_major<>, 10, 10> b5;
  bench_trans<double, ublas::basic_column_major<>, 10, 10> b6;
  bench_trans<std::complex<float>, ublas::basic_column_major<>, 10, 10> b7;
  bench_trans<std::complex<double>, ublas::basic_column_major<>, 10, 10> b8;

  std::cout << "Row-major:" << std::endl;
  b1.run();
  b2.run();
  b3.run();
  b4.run();

  std::cout << std::endl << "Column-major:" << std::endl;

  b5.run();
  b6.run();
  b7.run();
  b8.run();

  return 0;

}