/*==============================================================================
    Copyright (c) 2005-2010 Joel de Guzman
    Copyright (c) 2010 Thomas Heller

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/

#include <boost/phoenix.hpp>
#include <vector>
#include <algorithm>
#include <sstream>
 
int main()
{
    std::vector<unsigned char> data;
    using boost::phoenix::arg_names::_1;
    using boost::phoenix::static_cast_;
    std::ostringstream oss;
    oss << std::hex;
    std::for_each(data.begin(),data.end(), static_cast_<unsigned int>(_1) );
}
