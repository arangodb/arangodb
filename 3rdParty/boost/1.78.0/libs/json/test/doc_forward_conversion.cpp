//
// Copyright (c) 2021 Dmitry Arkhipov (grisumbras@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/json
//

//[doc_forward_conversion_1
namespace boost {
namespace json {

class value;

struct value_from_tag;

template< class T >
struct value_to_tag;

template< class T >
T value_to( const value& v );

template<class T>
void value_from( T&& t, value& jv );

}
}
//]

#include <string>

namespace thirdparty {

struct customer
{
    std::uint64_t id;
    std::string name;
    bool late;

    customer() = default;

    customer( std::uint64_t i, const std::string& n, bool l )
        : id( i ), name( n ), late( l ) { }
};

} // namespace thirdparty

//[doc_forward_conversion_2
namespace thirdparty {

template< class JsonValue >
customer tag_invoke( const boost::json::value_to_tag<customer>&, const JsonValue& jv )
{
    std::uint64_t id = boost::json::value_to<std::uint64_t>( jv.at( "id" ) );
    std::string name = boost::json::value_to< std::string >( jv.at( "name" ) );
    bool late = boost::json::value_to<bool>( jv.at( "late" ) );
    return customer(id, std::move(name), late);
}

template< class JsonValue >
void tag_invoke( const boost::json::value_from_tag&, JsonValue& jv, const customer& c)
{
    auto& obj = jv.emplace_object();
    boost::json::value_from(c.id, obj["id"]);
    boost::json::value_from(c.name, obj["name"]);
    boost::json::value_from(c.late, obj["late"]);
}

}
//]


#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include "test_suite.hpp"


BOOST_JSON_NS_BEGIN

class doc_forward_conversion
{
public:
    void
    run()
    {
        value const jv{ { "id", 1 }, { "name", "Carl" }, { "late", true } };
        auto const c = value_to<thirdparty::customer>( jv );
        BOOST_TEST( c.id == 1 );
        BOOST_TEST( c.name == "Carl" );
        BOOST_TEST( c.late );

        value const jv2 = value_from( c );
        BOOST_TEST( jv == jv2 );
    }
};

TEST_SUITE(doc_forward_conversion, "boost.json.doc_forward_conversion");

BOOST_JSON_NS_END
