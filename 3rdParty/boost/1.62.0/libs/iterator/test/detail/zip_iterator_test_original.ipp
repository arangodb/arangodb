// (C) Copyright Dave Abrahams and Thomas Becker 2003. Distributed
// under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

// File:
// =====
// zip_iterator_test_main.cpp

// Author:
// =======
// Thomas Becker

// Created:
// ========
// Jul 15, 2003

// Purpose:
// ========
// Test driver for zip_iterator.hpp

// Compilers Tested:
// =================
// Metrowerks Codewarrior Pro 7.2, 8.3
// gcc 2.95.3
// gcc 3.2
// Microsoft VC 6sp5 (test fails due to some compiler bug)
// Microsoft VC 7 (works)
// Microsoft VC 7.1
// Intel 5
// Intel 6
// Intel 7.1
// Intel 8
// Borland 5.5.1 (broken due to lack of support from Boost.Tuples)

/////////////////////////////////////////////////////////////////////////////
//
// Includes
//
/////////////////////////////////////////////////////////////////////////////

#include <boost/iterator/zip_iterator.hpp>
#include <boost/iterator/zip_iterator.hpp> // 2nd #include tests #include guard.
#include <iostream>
#include <vector>
#include <list>
#include <set>
#include <string>
#include <functional>
#include <boost/iterator/transform_iterator.hpp>
#include <boost/iterator/is_readable_iterator.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/detail/workaround.hpp>
#include <stddef.h>


/// Tests for https://svn.boost.org/trac/boost/ticket/1517
int to_value(int const &v)
{
    return v;
}

void category_test()
{
    std::list<int> rng1;
    std::string rng2;

    boost::make_zip_iterator(
        ZI_MAKE_TUPLE(
            boost::make_transform_iterator(rng1.begin(), &to_value), // BidirectionalInput
            rng2.begin() // RandomAccess
        )
    );
}
///

/////////////////////////////////////////////////////////////////////////////
//
// Das Main Funktion
//
/////////////////////////////////////////////////////////////////////////////

int main( void )
{

  category_test();

  std::cout << "\n"
            << "***********************************************\n"
            << "*                                             *\n"
            << "* Test driver for boost::zip_iterator         *\n"
            << "* Copyright Thomas Becker 2003                *\n"
            << "*                                             *\n"
            << "***********************************************\n\n"
            << std::flush;

  size_t num_successful_tests = 0;
  size_t num_failed_tests = 0;

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator construction and dereferencing
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator construction and dereferencing: "
            << std::flush;

  std::vector<double> vect1(3);
  vect1[0] = 42.;
  vect1[1] = 43.;
  vect1[2] = 44.;

  std::set<int> intset;
  intset.insert(52);
  intset.insert(53);
  intset.insert(54);
  //

  typedef
  boost::zip_iterator<
      ZI_TUPLE<
          std::set<int>::iterator
        , std::vector<double>::iterator
      >
  > zit_mixed;

  zit_mixed zip_it_mixed = zit_mixed(
    ZI_MAKE_TUPLE(
        intset.begin()
      , vect1.begin()
    )
  );

  ZI_TUPLE<int, double> val_tuple(
      *zip_it_mixed);

  ZI_TUPLE<const int&, double&> ref_tuple(
      *zip_it_mixed);

  double dblOldVal = ZI_TUPLE_GET(1)(ref_tuple);
  ZI_TUPLE_GET(1)(ref_tuple) -= 41.;

  if( 52 == ZI_TUPLE_GET(0)(val_tuple) &&
      42. == ZI_TUPLE_GET(1)(val_tuple) &&
      52 == ZI_TUPLE_GET(0)(ref_tuple)  &&
      1. == ZI_TUPLE_GET(1)(ref_tuple)  &&
      1. == *vect1.begin()
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  // Undo change to vect1
  ZI_TUPLE_GET(1)(ref_tuple) = dblOldVal;
  
#if defined(ZI_USE_BOOST_TUPLE)

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator with 12 components
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterators with 12 components:            "
            << std::flush;

  // Declare 12 containers
  //
  std::list<int> li1;
  li1.push_back(1);
  std::set<int> se1;
  se1.insert(2);
  std::vector<int> ve1;
  ve1.push_back(3);
  //
  std::list<int> li2;
  li2.push_back(4);
  std::set<int> se2;
  se2.insert(5);
  std::vector<int> ve2;
  ve2.push_back(6);
  //
  std::list<int> li3;
  li3.push_back(7);
  std::set<int> se3;
  se3.insert(8);
  std::vector<int> ve3;
  ve3.push_back(9);
  //
  std::list<int> li4;
  li4.push_back(10);
  std::set<int> se4;
  se4.insert(11);
  std::vector<int> ve4;
  ve4.push_back(12);

  // typedefs for cons lists of iterators.
  typedef boost::tuples::cons<
    std::set<int>::iterator,
    ZI_TUPLE<
      std::vector<int>::iterator,
      std::list<int>::iterator,
      std::set<int>::iterator,
      std::vector<int>::iterator,
      std::list<int>::iterator,
      std::set<int>::iterator,
      std::vector<int>::iterator,
      std::list<int>::iterator,
      std::set<int>::iterator,
      std::vector<int>::const_iterator
      >::inherited
    > cons_11_its_type;
  //
  typedef boost::tuples::cons<
    std::list<int>::const_iterator,
    cons_11_its_type
    > cons_12_its_type;

  // typedefs for cons lists for dereferencing the zip iterator
  // made from the cons list above.
  typedef boost::tuples::cons<
    const int&,
    ZI_TUPLE<
      int&,
      int&,
      const int&,
      int&,
      int&,
      const int&,
      int&,
      int&,
      const int&,
      const int&
      >::inherited
    > cons_11_refs_type;
  //
  typedef boost::tuples::cons<
    const int&,
    cons_11_refs_type
    > cons_12_refs_type;

  // typedef for zip iterator with 12 elements
  typedef boost::zip_iterator<cons_12_its_type> zip_it_12_type;

  // Declare a 12-element zip iterator.
  zip_it_12_type zip_it_12(
    cons_12_its_type(
      li1.begin(),
      cons_11_its_type(
        se1.begin(),
        ZI_MAKE_TUPLE(
          ve1.begin(),
          li2.begin(),
          se2.begin(),
          ve2.begin(),
          li3.begin(),
          se3.begin(),
          ve3.begin(),
          li4.begin(),
          se4.begin(),
          ve4.begin()
          )
        )
      )
    );

  // Dereference, mess with the result a little.
  cons_12_refs_type zip_it_12_dereferenced(*zip_it_12);
  ZI_TUPLE_GET(9)(zip_it_12_dereferenced) = 42;

  // Make a copy and move it a little to force some instantiations.
  zip_it_12_type zip_it_12_copy(zip_it_12);
  ++zip_it_12_copy;

  if( ZI_TUPLE_GET(11)(zip_it_12.get_iterator_tuple()) == ve4.begin() &&
      ZI_TUPLE_GET(11)(zip_it_12_copy.get_iterator_tuple()) == ve4.end() &&
      1 == ZI_TUPLE_GET(0)(zip_it_12_dereferenced) &&
      12 == ZI_TUPLE_GET(11)(zip_it_12_dereferenced) &&
      42 == *(li4.begin())
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }
  
#endif

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator incrementing and dereferencing
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator ++ and *:                       "
            << std::flush;

  std::vector<double> vect2(3);
  vect2[0] = 2.2;
  vect2[1] = 3.3;
  vect2[2] = 4.4;

  boost::zip_iterator<
    ZI_TUPLE<
      std::vector<double>::const_iterator,
      std::vector<double>::const_iterator
      >
    >
  zip_it_begin(
    ZI_MAKE_TUPLE(
     vect1.begin(),
     vect2.begin()
     )
  );

  boost::zip_iterator<
    ZI_TUPLE<
      std::vector<double>::const_iterator,
      std::vector<double>::const_iterator
      >
    >
  zip_it_run(
    ZI_MAKE_TUPLE(
     vect1.begin(),
     vect2.begin()
     )
  );

  boost::zip_iterator<
    ZI_TUPLE<
      std::vector<double>::const_iterator,
      std::vector<double>::const_iterator
      >
    >
  zip_it_end(
    ZI_MAKE_TUPLE(
     vect1.end(),
     vect2.end()
     )
  );

  if( zip_it_run == zip_it_begin &&
      42. == ZI_TUPLE_GET(0)(*zip_it_run) &&
      2.2 == ZI_TUPLE_GET(1)(*zip_it_run) &&
      43. == ZI_TUPLE_GET(0)(*(++zip_it_run)) &&
      3.3 == ZI_TUPLE_GET(1)(*zip_it_run) &&
      44. == ZI_TUPLE_GET(0)(*(++zip_it_run)) &&
      4.4 == ZI_TUPLE_GET(1)(*zip_it_run) &&
      zip_it_end == ++zip_it_run
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator decrementing and dereferencing
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator -- and *:                       "
            << std::flush;

  if( zip_it_run == zip_it_end &&
      zip_it_end == zip_it_run-- &&
      44. == ZI_TUPLE_GET(0)(*zip_it_run) &&
      4.4 == ZI_TUPLE_GET(1)(*zip_it_run) &&
      43. == ZI_TUPLE_GET(0)(*(--zip_it_run)) &&
      3.3 == ZI_TUPLE_GET(1)(*zip_it_run) &&
      42. == ZI_TUPLE_GET(0)(*(--zip_it_run)) &&
      2.2 == ZI_TUPLE_GET(1)(*zip_it_run) &&
      zip_it_begin == zip_it_run
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator copy construction and equality
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator copy construction and equality: "
            << std::flush;

  boost::zip_iterator<
    ZI_TUPLE<
      std::vector<double>::const_iterator,
      std::vector<double>::const_iterator
      >
    > zip_it_run_copy(zip_it_run);

  if(zip_it_run == zip_it_run && zip_it_run == zip_it_run_copy)
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator inequality
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator inequality:                     "
            << std::flush;

  if(!(zip_it_run != zip_it_run_copy) && zip_it_run != ++zip_it_run_copy)
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator less than
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator less than:                      "
            << std::flush;

  // Note: zip_it_run_copy == zip_it_run + 1
  //
  if( zip_it_run < zip_it_run_copy  &&
      !( zip_it_run < --zip_it_run_copy) &&
      zip_it_run == zip_it_run_copy
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator less than or equal
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "zip iterator less than or equal:             "
            << std::flush;

  // Note: zip_it_run_copy == zip_it_run
  //
  ++zip_it_run;
  zip_it_run_copy += 2;

  if( zip_it_run <= zip_it_run_copy &&
      zip_it_run <= --zip_it_run_copy &&
      !( zip_it_run <= --zip_it_run_copy) &&
      zip_it_run <= zip_it_run
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator greater than
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator greater than:                   "
            << std::flush;

  // Note: zip_it_run_copy == zip_it_run - 1
  //
  if( zip_it_run > zip_it_run_copy &&
      !( zip_it_run > ++zip_it_run_copy) &&
      zip_it_run == zip_it_run_copy
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator greater than or equal
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator greater than or equal:          "
            << std::flush;

  ++zip_it_run;

  // Note: zip_it_run == zip_it_run_copy + 1
  //
  if( zip_it_run >= zip_it_run_copy &&
      --zip_it_run >= zip_it_run_copy &&
      ! (zip_it_run >= ++zip_it_run_copy)
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator + int
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator + int:                          "
            << std::flush;

  // Note: zip_it_run == zip_it_run_copy - 1
  //
  zip_it_run = zip_it_run + 2;
  ++zip_it_run_copy;

  if( zip_it_run == zip_it_run_copy && zip_it_run == zip_it_begin + 3 )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator - int
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator - int:                          "
            << std::flush;

  // Note: zip_it_run == zip_it_run_copy, and both are at end position
  //
  zip_it_run = zip_it_run - 2;
  --zip_it_run_copy;
  --zip_it_run_copy;

  if( zip_it_run == zip_it_run_copy && (zip_it_run - 1) == zip_it_begin )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator +=
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator +=:                             "
            << std::flush;

  // Note: zip_it_run == zip_it_run_copy, and both are at begin + 1
  //
  zip_it_run += 2;
  if( zip_it_run == zip_it_begin + 3 )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator -=
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator -=:                             "
            << std::flush;

  // Note: zip_it_run is at end position, zip_it_run_copy is at
  // begin plus one.
  //
  zip_it_run -= 2;
  if( zip_it_run == zip_it_run_copy )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator getting member iterators
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator member iterators:               "
            << std::flush;

  // Note: zip_it_run and zip_it_run_copy are both at
  // begin plus one.
  //
  if( ZI_TUPLE_GET(0)(zip_it_run.get_iterator_tuple()) == vect1.begin() + 1 &&
      ZI_TUPLE_GET(1)(zip_it_run.get_iterator_tuple()) == vect2.begin() + 1
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Making zip iterators
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Making zip iterators:                        "
            << std::flush;

  std::vector<ZI_TUPLE<double, double> >
    vect_of_tuples(3);

  std::copy(
    boost::make_zip_iterator(
    ZI_MAKE_TUPLE(
      vect1.begin(),
      vect2.begin()
      )
    ),
    boost::make_zip_iterator(
    ZI_MAKE_TUPLE(
      vect1.end(),
      vect2.end()
      )
    ),
    vect_of_tuples.begin()
  );

  if( 42. == ZI_TUPLE_GET(0)(*vect_of_tuples.begin()) &&
      2.2 == ZI_TUPLE_GET(1)(*vect_of_tuples.begin()) &&
      43. == ZI_TUPLE_GET(0)(*(vect_of_tuples.begin() + 1)) &&
      3.3 == ZI_TUPLE_GET(1)(*(vect_of_tuples.begin() + 1)) &&
      44. == ZI_TUPLE_GET(0)(*(vect_of_tuples.begin() + 2)) &&
      4.4 == ZI_TUPLE_GET(1)(*(vect_of_tuples.begin() + 2))
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator non-const --> const conversion
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator non-const to const conversion:  "
            << std::flush;

  boost::zip_iterator<
    ZI_TUPLE<
      std::set<int>::const_iterator,
      std::vector<double>::const_iterator
      >
    >
  zip_it_const(
    ZI_MAKE_TUPLE(
      intset.begin(),
      vect2.begin()
    )
  );
  //
  boost::zip_iterator<
    ZI_TUPLE<
      std::set<int>::iterator,
      std::vector<double>::const_iterator
      >
    >
  zip_it_half_const(
    ZI_MAKE_TUPLE(
      intset.begin(),
      vect2.begin()
    )
  );
  //
  boost::zip_iterator<
    ZI_TUPLE<
      std::set<int>::iterator,
      std::vector<double>::iterator
      >
    >
  zip_it_non_const(
    ZI_MAKE_TUPLE(
      intset.begin(),
      vect2.begin()
    )
  );

  zip_it_half_const = ++zip_it_non_const;
  zip_it_const = zip_it_half_const;
  ++zip_it_const;
//  zip_it_non_const = ++zip_it_const;  // Error: can't convert from const to non-const

  if( 54 == ZI_TUPLE_GET(0)(*zip_it_const) &&
      4.4 == ZI_TUPLE_GET(1)(*zip_it_const)  &&
      53 == ZI_TUPLE_GET(0)(*zip_it_half_const)  &&
      3.3 == ZI_TUPLE_GET(1)(*zip_it_half_const)
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }


#if defined(ZI_USE_BOOST_TUPLE)

  /////////////////////////////////////////////////////////////////////////////
  //
  // Zip iterator categories
  //
  /////////////////////////////////////////////////////////////////////////////

  std::cout << "Zip iterator categories:                     "
            << std::flush;

  // The big iterator of the previous test has vector, list, and set iterators.
  // Therefore, it must be bidirectional, but not random access.
  bool bBigItIsBidirectionalIterator = boost::is_convertible<
    boost::iterator_traversal<zip_it_12_type>::type
        , boost::bidirectional_traversal_tag
        >::value;

  bool bBigItIsRandomAccessIterator = boost::is_convertible<
    boost::iterator_traversal<zip_it_12_type>::type
        , boost::random_access_traversal_tag
        >::value;

  // A combining iterator with all vector iterators must have random access
  // traversal.
  //
  typedef boost::zip_iterator<
    ZI_TUPLE<
      std::vector<double>::const_iterator,
      std::vector<double>::const_iterator
      >
    > all_vects_type;

  bool bAllVectsIsRandomAccessIterator = boost::is_convertible<
    boost::iterator_traversal<all_vects_type>::type
        , boost::random_access_traversal_tag
    >::value;

  // The big test.
  if( bBigItIsBidirectionalIterator &&
      ! bBigItIsRandomAccessIterator &&
      bAllVectsIsRandomAccessIterator
    )
  {
    ++num_successful_tests;
    std::cout << "OK" << std::endl;
  }
  else
  {
    ++num_failed_tests;
    std::cout << "not OK" << std::endl;
  }
  
#endif

  // Done
  //
  std::cout << "\nTest Result:"
            << "\n============"
            << "\nNumber of successful tests: " << static_cast<unsigned int>(num_successful_tests)
            << "\nNumber of failed tests: " << static_cast<unsigned int>(num_failed_tests)
            << std::endl;

  return num_failed_tests;
}

