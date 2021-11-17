#ifndef TEST_NORM2_OPENCL_HH
#define TEST_NORM2_OPENCL_HH
#include "test_opencl.hpp"


template <class T, int number_of_tests, int max_dimension>
class bench_norm2
{
public:

  typedef test_opencl<T, ublas::basic_row_major<>> test;


  void run()
  {
	opencl::library lib;
	int passedOperations = 0;
	// get default device and setup context
	compute::device device = compute::system::default_device();
	compute::context context(device);
	compute::command_queue queue(context, device);

	std::srand(time(0));

	ublas::vector<T> v;


	for (int i = 0; i<number_of_tests; i++)
	{
	  int size = std::rand() % max_dimension + 1;

	  v.resize(size);
	  test::init_vector(v, 200);


	  T norm2_cpu = ublas::norm_2(v);
	  T norm2_opencl = opencl::norm_2(v, queue);


	  if ((abs(norm2_cpu - norm2_opencl) / abs(norm2_cpu)) > 1e-6) //precision of float
	  {
		std::cout << "Error in calculations" << std::endl;

		std::cout << "passed: " << passedOperations << std::endl;
		return;
	  }

	  passedOperations++;

	}
	std::cout << "All is well (vector opencl a_sum) of " << typeid(T).name() << std::endl;



  }

};

#endif