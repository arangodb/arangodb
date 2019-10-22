#ifndef TEST_INNER_PROD_HH
#define TEST_INNER_PROD_HH
#include "test_opencl.hpp"


template <class T, int number_of_tests, int max_dimension>
class bench_inner_prod
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
	T result_inner_prod_ublas;
	T result_inner_prod_opencl;


	for (int i = 0; i<number_of_tests; i++)
	{
	  int size = std::rand() % max_dimension + 1;

	  va.resize(size);
	  vb.resize(size);

	  test::init_vector(va, 200);
	  test::init_vector(vb, 200);

	  result_inner_prod_ublas = ublas::inner_prod(va, vb);

	  result_inner_prod_opencl = opencl::inner_prod(va, vb, queue);


	  if ((  result_inner_prod_ublas != result_inner_prod_opencl  ))
	  {
		std::cout << "Error in calculations" << std::endl;

		std::cout << "passed: " << passedOperations << std::endl;
		return;
	  }

	  passedOperations++;

	}
	std::cout << "All is well (matrix opencl inner prod) of " << typeid(T).name() << std::endl;



  }

};

#endif
