#include "prod_test.hpp"
#include <boost/numeric/ublas/matrix.hpp>

int main()
{

	///testing row major flaot prod
	bench_prod<float, ublas::basic_row_major<>, 10, 10> b1;

	///testing row major complex float prod
	bench_prod<std::complex<float>, ublas::basic_row_major<>, 10, 10> b2;


	///testing row major double prod
	bench_prod<double, ublas::basic_row_major<>, 10, 10> b3;

	///testing row major complex float prod
	bench_prod<std::complex<double>, ublas::basic_row_major<>, 10, 10> b4;


	///testing column major flaot prod
	bench_prod<float, ublas::basic_column_major<>, 10, 10> b5;

	///testing column major complex float prod
	bench_prod<std::complex<float>, ublas::basic_column_major<>, 10, 10> b6;

	///testing column major double prod
	bench_prod<double, ublas::basic_column_major<>, 10, 10> b7;

	///testing column major complex double prod
	bench_prod<std::complex<double>, ublas::basic_column_major<>, 10, 10> b8;


	std::cout << "Row major:" << std::endl;
	b1.run();
	b2.run();
	b3.run();
	b4.run();

	std::cout << "Column major:" << std::endl;
	b5.run();
	b6.run();
	b7.run();
	b8.run();

		return 0;

}