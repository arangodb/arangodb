#ifndef TEST_TRANS_OPENCL_HH
#define TEST_TRANS_OPENCL_HH
#include "test_opencl.hpp"


template <class T, class F, int number_of_tests, int max_dimension>
class bench_trans
{
public:

  typedef test_opencl<T, F> test;

  void run()
  {
	opencl::library lib;
	int passedOperations = 0;
	// get default device and setup context
	compute::device device = compute::system::default_device();
	compute::context context(device);
	compute::command_queue queue(context, device);

	std::srand(time(0));

	ublas::matrix<T, F> a;
	ublas::matrix<T, F> resultUBLAS;
	ublas::matrix<T, F> resultOPENCL;


	for (int i = 0; i<number_of_tests; i++)
	{
	  int rowsA = std::rand() % max_dimension + 1;
	  int colsA = std::rand() % max_dimension + 1;

	  a.resize(rowsA, colsA);

	  test::init_matrix(a, 200);

	  resultUBLAS = ublas::trans(a);
	  resultOPENCL = opencl::trans(a, queue);


	  if (!test::compare(resultUBLAS, resultOPENCL))
	  {
		std::cout << "Error in calculations" << std::endl;

		std::cout << "passed: " << passedOperations << std::endl;
		return;
	  }

	  passedOperations++;

	}
	std::cout << "All is well (matrix opencl prod) of " << typeid(T).name() << std::endl;



  }

};

#endif