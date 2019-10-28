#include "outer_prod_test.hpp"

int main()
{


  ///testing float outer prod
  bench_outer_prod<float, 10, 10> b1;


  ///testing double outer prod
  bench_outer_prod<double, 10, 10> b2;


  ///testing complex of float outer prod
  bench_outer_prod<std::complex<float>, 10, 10> b3;


  ///testing complex of double outer prod
  bench_outer_prod<std::complex<double>, 10, 10> b4;



  b1.run();
  b2.run();
  b3.run();
  b4.run();

  return 0;

}