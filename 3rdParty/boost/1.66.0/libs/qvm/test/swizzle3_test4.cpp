//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/vec_traits.hpp>
#include <boost/qvm/swizzle3.hpp>
#include <boost/detail/lightweight_test.hpp>

template <int D> struct my_vec { };
int called=0;

namespace
boost
    {
    namespace
    qvm
        {
        void
        ZZZ(...)
            {
            BOOST_TEST(0);
            }
        void
        XXW(...)
            {
            ++called;
            }
        template <int D>
        struct
        vec_traits< my_vec<D> >
            {
            typedef int scalar_type;
            static int const dim=D;
            template <int I> static int read_element( my_vec<D> const & );
            template <int I> static int & write_element( my_vec<D> & );
            };
        }
    }

int
main()
    {
    using namespace boost::qvm;
    ZZZ(my_vec<3>());
    XXW(my_vec<3>());
    BOOST_TEST(called==1);
    return boost::report_errors();
    }
