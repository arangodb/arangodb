/*=============================================================================
    Copyright (c) 2012 Joel falcou

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <iostream>
#include <boost/mpl/transform.hpp>
#include <boost/fusion/include/mpl.hpp>
#include <boost/fusion/adapted/mpl.hpp>
#include <boost/fusion/include/at.hpp>
#include <boost/fusion/include/as_vector.hpp>
#include <boost/type_traits/add_reference.hpp>
#include <boost/fusion/include/adapt_struct.hpp>

struct foo
{
  double d; float f; short c;
};

BOOST_FUSION_ADAPT_STRUCT(foo,(double,d)(float,f)(short,c))

template<class T>
class composite_reference
    : public  boost::mpl::
              transform < typename  boost::fusion::result_of::
                                    as_vector<T>::type
                        , boost::add_reference<boost::mpl::_>
                        >::type
{
  public:
  typedef typename  boost::mpl::
          transform < typename  boost::fusion::result_of::
                                    as_vector<T>::type
                        , boost::add_reference<boost::mpl::_>
                        >::type                                 parent;

  composite_reference(T& src)       : parent( src ) {}
  composite_reference(parent& src)  : parent(src) {}

  composite_reference& operator=(T& src)
  {
    static_cast<parent&>(*this) = static_cast<parent&>(src);
    return *this;
  }

  composite_reference& operator=(parent const& src)
  {
    static_cast<parent&>(*this) = src;
    return *this;
  }
};

int main(int,char**)
{
  foo f;
  composite_reference<foo> ref_f(f);

  boost::fusion::at_c<0>(ref_f) = 1.2;
  boost::fusion::at_c<1>(ref_f) = 1.2f;
  boost::fusion::at_c<2>(ref_f) = 12;

  std::cout << f.d << " " << f.f << " " << f.c << "\n";
}
