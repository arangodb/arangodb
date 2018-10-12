////////////////////////////////////////////////////////////////////////////
// lazy_make_pair_tests.cpp
//
// lazy make_pair test solving the optimizer problem.
//
////////////////////////////////////////////////////////////////////////////
/*=============================================================================
    Copyright (c) 2001-2007 Joel de Guzman
    Copyright (c) 2015 John Fletcher

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix/core/limits.hpp>

#include <boost/detail/lightweight_test.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/function.hpp>

namespace boost {

  namespace phoenix {

#ifdef BOOST_RESULT_OF_USE_TR1

    namespace result_of {

      template <
          typename Arg1
        , typename Arg2
      >
      class make_pair
      {
      public:
        typedef typename boost::remove_reference<Arg1>::type Arg1Type;
        typedef typename boost::remove_reference<Arg2>::type Arg2Type;
        typedef std::pair<Arg1Type,Arg2Type> type;
      };
    }
#endif

  namespace impl
  {

    struct make_pair {


#ifdef BOOST_RESULT_OF_USE_TR1
       template <typename Sig>
       struct result;
       // This fails with -O2 unless refs are removed from A1 and A2.
       template <typename This, typename A0, typename A1>
       struct result<This(A0, A1)>
       {
         typedef typename result_of::make_pair<A0,A1>::type type;
       };
#else
       template <typename Sig>
       struct result;

       template <typename This, typename A0, typename A1>
       struct result<This(A0, A1)>
         : boost::remove_reference<std::pair<A0, A1> >
       {};
      
#endif


       template <typename A0, typename A1>
#ifdef BOOST_RESULT_OF_USE_TR1
       typename result<make_pair(A0,A1)>::type
#else
       std::pair<A0, A1>
#endif
       operator()(A0 const & a0, A1 const & a1) const
       {
          return std::make_pair(a0,a1);
       }

    };
  }

BOOST_PHOENIX_ADAPT_CALLABLE(make_pair, impl::make_pair, 2)

  }

}

int main()
{
    namespace phx = boost::phoenix;
    using boost::phoenix::arg_names::arg1;
    using boost::phoenix::arg_names::arg2;
    int a = 99; int b = 256;

    std::pair<int,int> ab1 = phx::make_pair(a,b)();
    //std::cout << ab1.first << "," << ab1.second << std::endl;
    BOOST_TEST(ab1.first == 99 && ab1.second == 256);
    std::pair<int,int> ab2 = phx::make_pair(arg1,b)(a);
    //std::cout << ab2.first << "," << ab2.second << std::endl;
    BOOST_TEST(ab2.first == 99 && ab2.second == 256);
    std::pair<int,int> ab3 = phx::make_pair(arg1,arg2)(a,b);
    //std::cout << ab3.first << "," << ab3.second << std::endl;
    BOOST_TEST(ab3.first == 99 && ab3.second == 256);


    return boost::report_errors();
}
