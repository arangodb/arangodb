//  Copyright (c) 2020 Andrey Semashev
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  https://www.boost.org/LICENSE_1_0.txt)

#include <boost/filesystem.hpp>

int main()
{
    boost::filesystem::path p(".");
    boost::filesystem::is_directory(p);
}
