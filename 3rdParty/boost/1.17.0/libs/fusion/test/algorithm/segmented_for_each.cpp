/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman
    Copyright (c) 2011 Eric Niebler
    Copyright (c) 2018 Kohei Takahashi

    Distributed under the Boost Software License, Version 1.0. (See accompanying 
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/core/lightweight_test.hpp>
#include <boost/fusion/container/vector/vector.hpp>
#include <boost/fusion/container/generation/make_vector.hpp>
#include <boost/fusion/algorithm/iteration/for_each.hpp>
#include <boost/fusion/sequence/comparison/equal_to.hpp>
#include <boost/fusion/sequence/io/out.hpp>
#include "../sequence/tree.hpp"

struct print
{
    template <typename T>
    void operator()(T const& v) const
    {
        std::cout << "[ " << v << " ] ";
    }
};

struct increment
{
    template <typename T>
    void operator()(T& v) const
    {
        ++v;
    }
};

struct mutable_increment : increment
{
    template <typename T>
    void operator()(T& v)
    {
        return increment::operator()(v);
    }
};

template <typename F, typename Tree>
void test(Tree tree, Tree const& expected)
{
    boost::fusion::for_each(tree, F());
    BOOST_TEST_EQ(tree, expected);
}

int
main()
{
    using namespace boost::fusion;

    {
        for_each(
            make_tree(
                make_vector(double(0),'B')
              , make_tree(
                    make_vector(1,2,long(3))
                  , make_tree(make_vector('a','b','c'))
                  , make_tree(make_vector(short('d'),'e','f'))
                )
              , make_tree(
                    make_vector(4,5,6)
                  , make_tree(make_vector(float(1),'h','i'))
                  , make_tree(make_vector('j','k','l'))
                )
            )
          , print()
        );
        std::cout << std::endl;
    }

    {
        test<increment>(
            make_tree(
                make_vector(double(0),'B')
              , make_tree(
                    make_vector(1,2,long(3))
                  , make_tree(make_vector('a','b','c'))
                  , make_tree(make_vector(short('d'),'e','f'))
                )
              , make_tree(
                    make_vector(4,5,6)
                  , make_tree(make_vector(float(1),'h','i'))
                  , make_tree(make_vector('j','k','l'))
                )
            )
          , make_tree(
                make_vector(double(1),'C')
              , make_tree(
                    make_vector(2,3,long(4))
                  , make_tree(make_vector('b','c','d'))
                  , make_tree(make_vector(short('e'),'f','g'))
                )
              , make_tree(
                    make_vector(5,6,7)
                  , make_tree(make_vector(float(2),'i','j'))
                  , make_tree(make_vector('k','l','m'))
                )
            )
        );
    }

    {
        test<mutable_increment>(
            make_tree(
                make_vector(double(0),'B')
              , make_tree(
                    make_vector(1,2,long(3))
                  , make_tree(make_vector('a','b','c'))
                  , make_tree(make_vector(short('d'),'e','f'))
                )
              , make_tree(
                    make_vector(4,5,6)
                  , make_tree(make_vector(float(1),'h','i'))
                  , make_tree(make_vector('j','k','l'))
                )
            )
          , make_tree(
                make_vector(double(1),'C')
              , make_tree(
                    make_vector(2,3,long(4))
                  , make_tree(make_vector('b','c','d'))
                  , make_tree(make_vector(short('e'),'f','g'))
                )
              , make_tree(
                    make_vector(5,6,7)
                  , make_tree(make_vector(float(2),'i','j'))
                  , make_tree(make_vector('k','l','m'))
                )
            )
        );
    }

    return boost::report_errors();
}

