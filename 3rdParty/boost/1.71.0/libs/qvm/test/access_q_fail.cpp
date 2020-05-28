//Copyright (c) 2008-2016 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/qvm/quat_access.hpp>
#include <boost/qvm/vec_access.hpp>

struct my_quat { };

namespace
boost
    {
    namespace
    qvm
        {
        template <>
        struct
        quat_traits<my_quat>
            {
            typedef int scalar_type;
            template <int I> static int read_element( my_quat const & );
            template <int I> static int & write_element( my_quat & );
            };
        }
    }

int
main()
    {
    using namespace boost::qvm;
    my_quat const q=my_quat();
    A<3>(V(q));
    return 1;
    }
