#ifndef ELEMENT_OPENCL_HH
#define ELEMENT_OPENCL_HH
#include "test_opencl.hpp"

template <class T, class F, int number_of_tests, int max_dimension>
class bench_elementwise
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

	//matrix-matrix operations of cpu
	ublas::matrix<T, F> result_m_add;
	ublas::matrix<T, F> result_m_sub;
	ublas::matrix<T, F> result_m_mul;

	//matrix-matrix operations of gpu
	ublas::matrix<T, F> result_m_add_cl;
	ublas::matrix<T, F> result_m_sub_cl;
	ublas::matrix<T, F> result_m_mul_cl;


	ublas::vector<T> va;
	ublas::vector<T> vb;

	//vector-vector operations of cpu 
	ublas::vector<T> result_v_add;
	ublas::vector<T> result_v_sub;
	ublas::vector<T> result_v_mul;

	//vector-vector operations of gpu 
	ublas::vector<T> result_v_add_cl;
	ublas::vector<T> result_v_sub_cl;
	ublas::vector<T> result_v_mul_cl;


	for (int i = 0; i<number_of_tests; i++)
	{
	  int rows = std::rand() % max_dimension + 1;
	  int cols = std::rand() % max_dimension + 1;

	  a.resize(rows, cols);
	  b.resize(rows, cols);
	  va.resize(rows);
	  vb.resize(rows);


	  test::init_matrix(a, 200);
	  test::init_matrix(b, 200);
	  test::init_vector(va, 200);
	  test::init_vector(vb, 200);

	  result_m_add = a + b;
	  result_m_add_cl = opencl::element_add(a, b, queue);

	  result_m_sub = a - b;
	  result_m_sub_cl = opencl::element_sub(a, b, queue);

	  result_m_mul = ublas::element_prod(a, b);
	  result_m_mul_cl = opencl::element_prod(a, b, queue);


	  result_v_add = va + vb;
	  result_v_add_cl = opencl::element_add(va, vb, queue);

	  result_v_sub = va - vb;
	  result_v_sub_cl = opencl::element_sub(va, vb, queue);

	  result_v_mul = ublas::element_prod(va, vb);
	  result_v_mul_cl = opencl::element_prod(va, vb, queue);






	  if ((!test::compare(result_m_add, result_m_add_cl)) ||
		(!test::compare(result_m_sub, result_m_sub_cl)) ||
		(!test::compare(result_m_mul, result_m_mul_cl)) ||
		(!test::compare(result_v_add, result_v_add_cl)) ||
		(!test::compare(result_v_sub, result_v_sub_cl)) ||
		(!test::compare(result_v_mul, result_v_mul_cl))
		)
	  {
		std::cout << "Error in calculations" << std::endl;

		std::cout << "passed: " << passedOperations << std::endl;
		return;
	  }

	  passedOperations++;

	}
	std::cout << "All is well (matrix opencl element wise operations) of " << typeid(T).name() << std::endl;



  }

};

#endif
