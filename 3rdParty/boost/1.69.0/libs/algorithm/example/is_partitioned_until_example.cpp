/*
   Copyright (c) Alexander Zaitsev <zamazan4ik@gmail.by>, 2017

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/

#include <vector>
#include <functional>
#include <iostream>

#include <boost/algorithm/is_partitioned_until.hpp>


namespace ba = boost::algorithm;

bool isOdd(const int v1)
{
    return v1 % 2 != 0;
}

struct isOddComp
{
    bool operator()(const int v1) const
    {
        return v1 % 2 != 0;
    }
};


int main ( int /*argc*/, char * /*argv*/ [] )
{
    std::vector<int> good({1, 2, 4});
    std::vector<int> bad({1, 2, 3});

    //Use custom function
    auto it1 = ba::is_partitioned_until(good.begin(), good.end(), isOdd);
    if(it1 == good.end())
    {
        std::cout << "The sequence is partitioned\n";
    }
    else
    {
        std::cout << "is_partitioned_until check failed here: " << *it1 << std::endl;
    }

    //Use custom comparator
    auto it2 = ba::is_partitioned_until(good.begin(), good.end(), isOddComp());
    if(it2 == good.end())
    {
        std::cout << "The sequence is partitioned\n";
    }
    else
    {
        std::cout << "is_partitioned_until check failed here: " << *it2 << std::endl;
    }

    auto it3 = ba::is_partitioned_until(bad, isOdd);
    if(it3 == bad.end())
    {
        std::cout << "The sequence is partitioned\n";
    }
    else
    {
        std::cout << "is_partitioned_until check failed here: " << *it3 << std::endl;
    }
    return 0;
}
