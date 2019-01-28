/*==============================================================================
    Copyright (c) 2005-2010 Joel de Guzman
    Copyright (c) 2010 Thomas Heller

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix.hpp>
#include <boost/signals2.hpp>
 
struct s
{
        bool f(int, bool) { return true; }
};
 
int main()
{
        s s_obj;
        boost::signals2::signal<bool (int, bool)> sig;
        sig.connect(
                boost::phoenix::bind(
                        &s::f, &s_obj,
                        boost::phoenix::placeholders::arg1,
                        boost::phoenix::placeholders::arg2));
}

