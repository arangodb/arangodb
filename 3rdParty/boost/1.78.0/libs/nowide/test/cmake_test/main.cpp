//
//  Copyright (c) 2019-2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0. (See
//  accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//

#include <boost/nowide/args.hpp>
#include <boost/nowide/convert.hpp>

int main(int argc, char** argv, char** env)
{
    boost::nowide::args _(argc, argv, env);
    if(argc < 1)
        return 1;
    if(boost::nowide::narrow(boost::nowide::widen(argv[0])) != argv[0])
        return 1;
    return 0;
}
