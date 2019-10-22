#ifndef TEST_ELEMENT_CONSTANT_OPENCL_HH
#define TEST_ELEMENT_CONSTANT_OPENCL_HH
#include "test_opencl.hpp"


template <class T, class F, int number_of_tests, int max_dimension>
class bench_elementwise_constant
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

	ublas::matrix<T, F> m;
	ublas::matrix<T, F> m_result_add_ublas;
	ublas::matrix<T, F> m_result_sub_ublas;
	ublas::matrix<T, F> m_result_add_opencl;
	ublas::matrix<T, F> m_result_sub_opencl;
	ublas::vector<T> v;
	ublas::vector<T> v_result_add_ublas;
	ublas::vector<T> v_result_sub_ublas;
	ublas::vector<T> v_result_add_opencl;
	ublas::vector<T> v_result_sub_opencl;



	for (int i = 0; i<number_of_tests; i++)
	{
	  int rows = std::rand() % max_dimension + 1;
	  int cols = std::rand() % max_dimension + 1;

	  m.resize(rows, cols);
	  v.resize(rows);

	  test::init_matrix(m, 200);
	  test::init_vector(v, 200);

	  T constant = rand() % max_dimension;
	  ublas::matrix<T, F> m_constant_holder(rows, cols, constant);
	  ublas::vector<T> v_constant_holder(rows, constant);

	  m_result_add_ublas = m + m_constant_holder;
	  m_result_sub_ublas = m - m_constant_holder;
	  m_result_add_opencl = opencl::element_add(m, constant, queue);
	  m_result_sub_opencl = opencl::element_sub(m, constant, queue);

	  v_result_add_ublas = v + v_constant_holder;
	  v_result_sub_ublas = v - v_constant_holder;
	  v_result_add_opencl = opencl::element_add(v, constant, queue);
	  v_result_sub_opencl = opencl::element_sub(v, constant, queue);



	  if ((!test::compare(m_result_add_ublas, m_result_add_opencl))
		|| (!test::compare(m_result_sub_ublas, m_result_sub_opencl)) ||
		(!test::compare(v_result_add_ublas, v_result_add_opencl))
		|| (!test::compare(v_result_sub_ublas, v_result_sub_opencl)))
	  {
		std::cout << "Error in calculations" << std::endl;

		std::cout << "passed: " << passedOperations << std::endl;
		return;
	  }

	  passedOperations++;

	}
	std::cout << "All is well (matrix opencl elementwise operations with constants) of " << typeid(T).name() << std::endl;



  }

};

#endif