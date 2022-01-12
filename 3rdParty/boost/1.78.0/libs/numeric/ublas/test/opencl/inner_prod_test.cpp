#include "inner_prod_test.hpp"
#include <boost/numeric/ublas/matrix.hpp>

int main()
{
  ///testing row major int inner prod
  bench_inner_prod<int, 10, 10> b1;

  ///testing row major float inner prod
  bench_inner_prod<float, 10, 10> b2;


  ///testing row major double inner prod
  bench_inner_prod<double, 10, 10> b3;


  b1.run();
  b2.run();
  b3.run();

  return 0;

}