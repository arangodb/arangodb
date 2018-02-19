//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/mat_access.hpp>

template <int R,int C> struct my_mat { };

namespace
boost
    {
    namespace
    qvm
        {
        template <int R,int C>
        struct
        mat_traits< my_mat<R,C> >
            {
            typedef int scalar_type;
            static int const rows=R;
            static int const cols=C;
            template <int Row,int Col> static int read_element( my_mat<R,C> const & );
            template <int Row,int Col> static int & write_element( my_mat<R,C> & );
            };
        }
    }

int
main()
    {
    using namespace boost::qvm;
    my_mat<1,1> const m=my_mat<1,1>();
    A11(m);
    return 1;
    }
