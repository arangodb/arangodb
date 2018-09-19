// Copyright 2008 Gunter Winkler <guwi17@gmx.de>
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#ifndef _HPP_TESTHELPER_
#define _HPP_TESTHELPER_

#include <utility>
#include <iostream>
#include <boost/numeric/ublas/vector_expression.hpp>
#include <boost/numeric/ublas/matrix_expression.hpp>

static unsigned _success_counter = 0;
static unsigned _fail_counter    = 0;

static inline
void assertTrue(const char* message, bool condition) {
#ifndef NOMESSAGES
  std::cout << message;
#endif
  if ( condition ) {
    ++ _success_counter;
    std::cout << "1\n"; // success
  } else {
    ++ _fail_counter;
    std::cout << "0\n"; // failed
  }
}

template < class T >
void assertEquals(const char* message, T expected, T actual) {
#ifndef NOMESSAGES
  std::cout << message;
#endif
  if ( expected == actual ) {
    ++ _success_counter;
    std::cout << "1\n"; // success
  } else {
    #ifndef NOMESSAGES
      std::cout << " expected " << expected << " actual " << actual << " ";
    #endif
    ++ _fail_counter;
    std::cout << "0\n"; // failed
  }
}

inline static
std::pair<unsigned, unsigned> getResults() {
  return std::make_pair(_success_counter, _fail_counter);
}

template < class M1, class M2 >
bool compare( const boost::numeric::ublas::matrix_expression<M1> & m1, 
              const boost::numeric::ublas::matrix_expression<M2> & m2 ) {
  if ((m1().size1() != m2().size1()) ||
      (m1().size2() != m2().size2())) {
    return false;
  }

  size_t size1 = m1().size1();
  size_t size2 = m1().size2();
  for (size_t i=0; i < size1; ++i) {
    for (size_t j=0; j < size2; ++j) {
      if ( m1()(i,j) != m2()(i,j) ) return false;
    }
  }
  return true;
}

template < class M1, class M2 >
bool compare( const boost::numeric::ublas::vector_expression<M1> & m1, 
              const boost::numeric::ublas::vector_expression<M2> & m2 ) {
  if (m1().size() != m2().size()) {
    return false;
  }

  size_t size = m1().size();
  for (size_t i=0; i < size; ++i) {
    if ( m1()(i) != m2()(i) ) return false;
  }
  return true;
}

// Compare if two matrices or vectors are equals based on distance.

template <class AE>
typename AE::value_type mean_square(const boost::numeric::ublas::matrix_expression<AE> &me) {
    typename AE::value_type s(0);
    typename AE::size_type i, j;
    for (i=0; i!= me().size1(); i++) {
        for (j=0; j!= me().size2(); j++) {
            s += boost::numeric::ublas::scalar_traits<typename AE::value_type>::type_abs(me()(i,j));
        }
    }
    return s / (me().size1() * me().size2());
}

template <class AE>
typename AE::value_type mean_square(const boost::numeric::ublas::vector_expression<AE> &ve) {
    // We could have use norm2 here, but ublas' ABS does not support unsigned types.
    typename AE::value_type s(0);
    typename AE::size_type i;
    for (i=0; i!= ve().size(); i++) {
        s += boost::numeric::ublas::scalar_traits<typename AE::value_type>::type_abs(ve()(i));
    }
    return s / ve().size();
}

template < class M1, class M2 >
bool compare_to( const boost::numeric::ublas::matrix_expression<M1> & m1,
               const boost::numeric::ublas::matrix_expression<M2> & m2,
               double tolerance = 0.0 ) {
    if ((m1().size1() != m2().size1()) ||
        (m1().size2() != m2().size2())) {
        return false;
    }

    return mean_square(m2() - m1()) <= tolerance;
}

template < class M1, class M2 >
bool compare_to( const boost::numeric::ublas::vector_expression<M1> & m1,
               const boost::numeric::ublas::vector_expression<M2> & m2,
               double tolerance = 0.0 ) {
    if (m1().size() != m2().size()) {
        return false;
    }

    return mean_square(m2() - m1()) <= tolerance;
}


#endif
