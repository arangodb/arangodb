/*=============================================================================
    Copyright (c) 2007 Tobias Schwinger
    Copyright (c) 2017 Daniel James

    Use modification and distribution are subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt).
==============================================================================*/

#include <boost/functional/factory.hpp>
#include <boost/detail/lightweight_test.hpp>

#include <cstddef>
#include <memory>
#include <boost/shared_ptr.hpp>

class sum
{
    int val_sum;
  public:
    sum(int a, int b) : val_sum(a + b) { }

    operator int() const { return this->val_sum; }
};

int main()
{
    int one = 1, two = 2;
    {
      boost::shared_ptr<sum> instance(
          boost::factory< boost::shared_ptr<sum>, std::allocator<int>,
              boost::factory_alloc_for_pointee_and_deleter >()(one,two) );
      BOOST_TEST(*instance == 3);
    }

    {
      boost::shared_ptr<sum> instance(
          boost::factory< boost::shared_ptr<sum>, std::allocator<int>,
              boost::factory_passes_alloc_to_smart_pointer >()(one,two) );
      BOOST_TEST(*instance == 3);
    }

    return boost::report_errors();
}

