/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman
    Copyright (c) 2005 Eric Niebler

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/adapted/mpl.hpp>
#include <boost/fusion/algorithm/query/count_if.hpp>
#include <boost/mpl/vector_c.hpp>
#include <functional>

template <typename F> struct bind1st;
template <template <typename> class F, typename T>
struct bind1st<F<T> > : public F<T>
{
    T n;
    bind1st(T n) : n(n) { }
    bool operator()(T v) const { return F<T>::operator()(n, v); }
};

int
main()
{
    {
        boost::fusion::vector<int, short, double> t(1, 2, 3.3);
        BOOST_TEST(boost::fusion::count_if(t, bind1st<std::equal_to<double> >(2)) == 1);
    }

    {
        boost::fusion::vector<int, short, double> t(1, 2, 3.3);
        BOOST_TEST(boost::fusion::count_if(t, bind1st<std::equal_to<double> >(3)) == 0);
    }

    {
        typedef boost::mpl::vector_c<int, 1, 2, 3> mpl_vec;
        // Cannot use lambda here as mpl iterators return rvalues and lambda needs lvalues
        BOOST_TEST(boost::fusion::count_if(mpl_vec(), bind1st<std::greater_equal<int> >(2)) == 2);
        BOOST_TEST(boost::fusion::count_if(mpl_vec(), bind1st<std::less<int> >(2)) == 1);
    }

    return boost::report_errors();
}

