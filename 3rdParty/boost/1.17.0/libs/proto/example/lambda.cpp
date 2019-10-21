//[ Lambda
///////////////////////////////////////////////////////////////////////////////
// Copyright 2008 Eric Niebler. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// This example builds a simple but functional lambda library using Proto.

#include <iostream>
#include "./lambda.hpp"

int main()
{
    using namespace boost::lambda;
    int i = (_1 + _1)(42);
    std::cout << i << std::endl;
}
