// Copyright 2019 Glen Joseph Fernandes
// (glenjofe@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>
#include <boost/multi_array.hpp>
#include <algorithm>

template<class T>
class creator {
public:
  typedef T value_type;
  typedef T* pointer;
  typedef std::size_t size_type;
  typedef std::ptrdiff_t difference_type;

  template<class U>
  struct rebind {
    typedef creator<U> other;
  };

  creator(int state)
    : state_(state) { }

  template<class U>
  creator(const creator<U>& other)
    : state_(other.state()) { }

  T* allocate(std::size_t size) {
    return static_cast<T*>(::operator new(sizeof(T) * size));
  }

  void deallocate(T* ptr, std::size_t) {
    ::operator delete(ptr);
  }

  int state() const {
    return state_;
  }

private:
  int state_;
};

template<class T, class U>
inline bool
operator==(const creator<T>& a, const creator<U>& b)
{
  return a.state() == b.state();
}

template<class T, class U>
inline bool
operator!=(const creator<T>& a, const creator<U>& b)
{
  return !(a == b);
}

void test(const double&, std::size_t*, int*, unsigned)
{
}

template<class Array>
void test(const Array& array, std::size_t* sizes, int* strides,
    unsigned elements)
{
  BOOST_TEST(array.num_elements() == elements);
  BOOST_TEST(array.size() == *sizes);
  BOOST_TEST(std::equal(sizes, sizes + array.num_dimensions(), array.shape()));
  BOOST_TEST(std::equal(strides, strides + array.num_dimensions(),
    array.strides()));
  test(array[0], ++sizes, ++strides, elements / array.size());
}

bool test(const double& a, const double& b)
{
  return a == b;
}

template<class A1, class A2>
bool test(const A1& a1, const A2& a2)
{
  typename A1::const_iterator i1 = a1.begin();
  typename A2::const_iterator i2 = a2.begin();
  for (; i1 != a1.end(); ++i1, ++i2) {
    if (!test(*i1, *i2)) {
      return false;
    }
  }
  return true;
}

int main()
{
  typedef boost::multi_array<double, 3, creator<double> > type;
  creator<double> state(1);
  {
    type array(state);
  }
  boost::array<type::size_type, 3> sizes = { { 3, 3, 3 } };
  type::size_type elements = 27;
  {
    int strides[] = { 9, 3, 1 };
    type array(sizes, state);
    test(array, &sizes[0], strides, elements);
  }
  {
    int strides[] = { 1, 3, 9 };
    type array(sizes, boost::fortran_storage_order(), state);
    test(array, &sizes[0], strides, elements);
  }
  {
    int strides[] = { 9, 3, 1 };
    type::extent_gen extents;
    type array(extents[3][3][3], state);
    test(array, &sizes[0], strides, elements);
  }
  {
    type array1(sizes, state);
    std::vector<double> values(elements, 4.5);
    array1.assign(values.begin(), values.end());
    type array2(array1);
    int strides[] = { 9, 3, 1 };
    test(array2, &sizes[0], strides, elements);
    BOOST_TEST(test(array1, array2));
  }
  {
    type array1(sizes, state);
    type array2(sizes, state);
    std::vector<double> values(elements, 4.5);
    array1.assign(values.begin(), values.end());
    array2 = array1;
    int strides[] = { 9, 3, 1 };
    test(array2, &sizes[0], strides, elements);
    BOOST_TEST(test(array1, array2));
  }
  {
    type array1(sizes, state);
    std::vector<double> values(elements, 4.5);
    array1.assign(values.begin(), values.end());
    typedef type::subarray<2>::type other;
    other array2 = array1[1];
    other::value_type value = array2[0];
    BOOST_TEST(test(array1[1][0], value));
    BOOST_TEST(test(array2[0], value));
  }
  return boost::report_errors();
}
