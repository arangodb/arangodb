#include "norm2_test.hpp"
#include <boost/numeric/ublas/matrix.hpp>

int main()
{

  ///testing float norm2 
  bench_norm2<float, 10, 10> b1;


  ///testing double norm2 
  bench_norm2<double, 10, 10> b2;


  ///testing float norm2 
  bench_norm2<std::complex<float>, 10, 10> b3;


  ///testing double norm2 
  bench_norm2<std::complex<double>, 10, 10> b4;



  b1.run();
  b2.run();
  b3.run();
  b4.run();


  return 0;

}