/*=============================================================================
    Copyright (c) 2007 Tobias Schwinger

    Use modification and distribution are subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
==============================================================================*/

#include <boost/functional/factory.hpp>
#include <boost/core/lightweight_test.hpp>

#include <memory>

class sum
{
    int val_sum;
  public:
    sum(int a, int b) : val_sum(a + b) { }

    operator int() const { return this->val_sum; }
};

// Suppress warnings about std::auto_ptr.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

int main()
{
    int one = 1, two = 2;
    {
      sum* instance( boost::factory< sum* >()(one,two) );
      BOOST_TEST(*instance == 3);
    }
#if !defined(BOOST_NO_AUTO_PTR)
    {
      std::auto_ptr<sum> instance( boost::factory< std::auto_ptr<sum> >()(one,two) );
      BOOST_TEST(*instance == 3);
    }
#endif
#if !defined(BOOST_NO_CXX11_SMART_PTR)
    {
      std::unique_ptr<sum> instance( boost::factory< std::unique_ptr<sum> >()(one,two) );
      BOOST_TEST(*instance == 3);
    }
#endif
    return boost::report_errors();
}

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
