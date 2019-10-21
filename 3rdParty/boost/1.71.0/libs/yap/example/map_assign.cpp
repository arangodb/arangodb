// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ map_assign
#include <boost/yap/algorithm.hpp>

#include <map>
#include <iostream>


// This transform applies all the call-subexpressions in a map_list_of
// expression (a nested chain of call operations) as a side effect; the
// expression returned by the transform is ignored.
template <typename Key, typename Value, typename Allocator>
struct map_list_of_transform
{
    template <typename Fn, typename Key2, typename Value2>
    auto operator() (boost::yap::expr_tag<boost::yap::expr_kind::call>,
                     Fn const & fn, Key2 && key, Value2 && value)
    {
        // Recurse into the function subexpression.  Remember, transform()
        // walks the nodes in an expression tree looking for matches.  Once it
        // finds a match, it is finished with that matching subtree.  So
        // without this recursive call, only the top-level call expression is
        // matched by transform().
        boost::yap::transform(
            boost::yap::as_expr<boost::yap::minimal_expr>(fn), *this);
        map.emplace(
            std::forward<Key2 &&>(key),
            std::forward<Value2 &&>(value)
        );
        // All we care about are the side effects of this transform, so we can
        // return any old thing here.
        return 0;
    }

    std::map<Key, Value, Allocator> & map;
};


// A custom expression template type for map_list_of expressions.  We only
// need support for the call operator and an implicit conversion to a
// std::map.
template <boost::yap::expr_kind Kind, typename Tuple>
struct map_list_of_expr
{
    static boost::yap::expr_kind const kind = Kind;

    Tuple elements;

    template <typename Key, typename Value, typename Allocator>
    operator std::map<Key, Value, Allocator> () const
    {
        std::map<Key, Value, Allocator> retval;
        map_list_of_transform<Key, Value, Allocator> transform{retval};
        boost::yap::transform(*this, transform);
        return retval;
    }

    BOOST_YAP_USER_CALL_OPERATOR_N(::map_list_of_expr, 2)
};

// A tag type for creating the map_list_of function terminal.
struct map_list_of_tag {};

auto map_list_of = boost::yap::make_terminal<map_list_of_expr>(map_list_of_tag{});


int main()
{
    // Initialize a map:
    std::map<std::string, int> op =
        map_list_of
            ("<", 1)
            ("<=",2)
            (">", 3)
            (">=",4)
            ("=", 5)
            ("<>",6)
        ;

    std::cout << "\"<\"  --> " << op["<"] << std::endl;
    std::cout << "\"<=\" --> " << op["<="] << std::endl;
    std::cout << "\">\"  --> " << op[">"] << std::endl;
    std::cout << "\">=\" --> " << op[">="] << std::endl;
    std::cout << "\"=\"  --> " << op["="] << std::endl;
    std::cout << "\"<>\" --> " << op["<>"] << std::endl;

    return 0;
}
//]
