//  phoenix_style_control_structures.cpp  -- The Boost Lambda Library ------
//
// Copyright (C) 2000-2003 Jaakko Jarvi (jaakko.jarvi@cs.utu.fi)
// Copyright (C) 2000-2003 Gary Powell (powellg@amazon.com)
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see www.boost.org

// -----------------------------------------------------------------------


#include <boost/core/lightweight_test.hpp>

#include "boost/lambda/lambda.hpp"
#include "boost/lambda/if.hpp"
#include "boost/lambda/loops.hpp"

#include <iostream>
#include <vector>
#include <list>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <functional>



using namespace boost::lambda;
using namespace std;



//  If-else, while, do-while, for statements


int main()
{
    vector<int> v;
    v.clear();
    v.push_back(1);
    v.push_back(2);
    v.push_back(3);
    v.push_back(4);
    v.push_back(5);
    v.push_back(6);
    v.push_back(7);
    v.push_back(8);
    v.push_back(9);
    v.push_back(10);

    int sum = 0; 
    //////////////////////////////////
    for_each(v.begin(), v.end(),
        if_(_1 > 3 && _1 <= 8)
        [
            sum += _1
        ]
    );

    BOOST_TEST_EQ(sum, 4+5+6+7+8);

    int gt = 0, eq = 0, lt = 0;
    //////////////////////////////////
    for_each(v.begin(), v.end(),
        if_(_1 > 5)
        [
            ++var(gt)
        ]
        .else_
        [
            if_(_1 == 5)
            [
                ++var(eq)
            ]
            .else_
            [
                ++var(lt)
            ]
        ]
    );

    BOOST_TEST_EQ(lt, 4);
    BOOST_TEST_EQ(eq, 1);
    BOOST_TEST_EQ(gt, 5);

    vector<int> t = v;

    int counta = 0;
    int countb = 0;
    //////////////////////////////////
    for_each(v.begin(), v.end(),
        (
            while_(_1--)
            [
                ++var(counta)
            ],
            ++var(countb)
        )
    );
    
    BOOST_TEST_EQ(counta, 55);
    BOOST_TEST_EQ(countb, 10);


    v = t;

    counta = 0; countb = 0;
    //////////////////////////////////
    for_each(v.begin(), v.end(),
        (
            do_
            [
             ++var(counta)
            ]
            .while_(_1--),
            ++var(countb)
        )
    );

    BOOST_TEST_EQ(counta, (2+11)*10/2);
    BOOST_TEST_EQ(countb, 10);


    v = t;
    counta = 0; countb = 0;
    //////////////////////////////////
    int iii;
    for_each(v.begin(), v.end(),
        (
            for_(var(iii) = 0, var(iii) < _1, ++var(iii))
            [
              ++var(counta)
            ],
            ++var(countb)
        )
    );

    BOOST_TEST_EQ(counta, (1+10)*10/2);
    BOOST_TEST_EQ(countb, 10);

    v = t;

    return boost::report_errors();
}

