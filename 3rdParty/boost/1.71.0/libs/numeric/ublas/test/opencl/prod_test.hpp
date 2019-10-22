#ifndef TEST_PROD_OPENCL_HH
#define TEST_PROD_OPENCL_HH
#include "test_opencl.hpp"


template <class T, class F, int number_of_tests, int max_dimension>
class bench_prod
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
	ublas::matrix<T, F> b;
	ublas::matrix<T, F> resultUBLAS;
	ublas::matrix<T, F> resultOPENCL;
	ublas::vector<T> va;
	ublas::vector<T> vb;
	ublas::vector<T> result_vector_ublas_mv;
	ublas::vector<T> result_vector_ublas_vm;
	ublas::vector<T> result_vector_opencl_mv;
	ublas::vector<T> result_vector_opencl_vm;



	for (int i = 0; i<number_of_tests; i++)
	{
	  int rowsA = std::rand() % max_dimension + 1;
	  int colsA = std::rand() % max_dimension + 1;
	  int colsB = std::rand() % max_dimension + 1;

	  a.resize(rowsA, colsA);
	  b.resize(colsA, colsB);
	  va.resize(colsA);
	  vb.resize(rowsA);


	  test::init_matrix(a, 200);
	  test::init_matrix(b, 200);
	  test::init_vector(va, 200);
	  test::init_vector(vb, 200);

	  //matrix_matrix
	  resultUBLAS = prod(a, b);
	  resultOPENCL = opencl::prod(a, b, queue);


	  //matrix_vector
	  result_vector_ublas_mv = ublas::prod(a, va);
	  result_vector_opencl_mv = opencl::prod(a, va, queue);


	  //vector-matrix
	  result_vector_ublas_vm = ublas::prod(vb, a);
	  result_vector_opencl_vm = opencl::prod(vb, a, queue);


	  if ((!test::compare(resultUBLAS, resultOPENCL)) || (!test::compare(result_vector_opencl_mv, result_vector_ublas_mv)) || (!test::compare(result_vector_opencl_vm, result_vector_ublas_vm)))
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