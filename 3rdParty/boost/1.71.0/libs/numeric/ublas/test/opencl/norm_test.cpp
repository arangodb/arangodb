#include "norm_test.hpp"
#include <boost/numeric/ublas/matrix.hpp>

int main()
{

  ///testing float norm1
  bench_norm<float, 10, 10> b1;


  ///testing double norm1
  bench_norm<double, 10, 10> b2;



  b1.run();
  b2.run();


  return 0;

}