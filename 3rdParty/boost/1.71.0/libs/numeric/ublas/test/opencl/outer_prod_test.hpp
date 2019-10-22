#ifndef TEST_PROD_OPENCL_HH
#define TEST_PROD_OPENCL_HH
#include "test_opencl.hpp"


template <class T, int number_of_tests, int max_dimension>
class bench_outer_prod
{
public:

  typedef test_opencl<T> test;

  void run()
  {
	opencl::library lib;
	int passedOperations = 0;
	// get default device and setup context
	compute::device device = compute::system::default_device();
	compute::context context(device);
	compute::command_queue queue(context, device);

	std::srand(time(0));

	ublas::vector<T> va;
	ublas::vector<T> vb;
	ublas::matrix<T> resultUBLAS;
	ublas::matrix<T> resultOPENCL;


	for (int i = 0; i<number_of_tests; i++)
	{
	  int rows = std::rand() % max_dimension + 1;
	  int cols = std::rand() % max_dimension + 1;

	  va.resize(rows);
	  vb.resize(cols);
	  
	  test::init_vector(va, 200);
	  test::init_vector(vb, 200);

	  //matrix_matrix
	  resultUBLAS = ublas::outer_prod(va, vb);
	  resultOPENCL = opencl::outer_prod(va, vb, queue);


	  if (!test::compare(resultUBLAS, resultOPENCL))
	  {
		std::cout << "Error in calculations" << std::endl;

		std::cout << "passed: " << passedOperations << std::endl;
		return;
	  }

	  passedOperations++;

	}
	std::cout << "All is well (matrix opencl outer prod) of " << typeid(T).name() << std::endl;



  }

};

#endif