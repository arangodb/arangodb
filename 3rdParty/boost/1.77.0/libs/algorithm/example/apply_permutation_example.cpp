/*
  Copyright (c) Alexander Zaitsev <zamazan4ik@gmail.com>, 2017

  Distributed under the Boost Software License, Version 1.0. (See
  accompanying file LICENSE_1_0.txt or copy at
  http://www.boost.org/LICENSE_1_0.txt)

  See http://www.boost.org/ for latest version.
*/

#include <vector>
#include <iostream>

#include <boost/algorithm/apply_permutation.hpp>


namespace ba = boost::algorithm;

int main ( int /*argc*/, char * /*argv*/ [] )
{
    // WARNING: Example require C++11 or newer compiler
    {
        std::cout << "apply_permutation with iterators:\n";
        std::vector<int> vec{1, 2, 3, 4, 5}, order{4, 2, 3, 1, 0};

        ba::apply_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        for (const auto& x : vec)
        {
            std::cout << x << ", ";
        }
        std::cout << std::endl;
    }
    {
        std::cout << "apply_reverse_permutation with iterators:\n";
        std::vector<int> vec{1, 2, 3, 4, 5}, order{4, 2, 3, 1, 0};

        ba::apply_reverse_permutation(vec.begin(), vec.end(), order.begin(), order.end());
        for (const auto& x : vec)
        {
            std::cout << x << ", ";
        }
        std::cout << std::endl;
    }
    {
        std::cout << "apply_reverse_permutation with ranges:\n";
        std::vector<int> vec{1, 2, 3, 4, 5}, order{4, 2, 3, 1, 0};

        ba::apply_reverse_permutation(vec, order);
        for (const auto& x : vec)
        {
            std::cout << x << ", ";
        }
        std::cout << std::endl;
    }
    {
        std::cout << "apply_permutation with ranges:\n";
        std::vector<int> vec{1, 2, 3, 4, 5}, order{4, 2, 3, 1, 0};

        ba::apply_permutation(vec, order);
        for (const auto& x : vec)
        {
            std::cout << x << ", ";
        }
        std::cout << std::endl;
    }

    return 0;
}

