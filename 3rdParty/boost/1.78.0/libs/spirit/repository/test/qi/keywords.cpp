/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman
    Copyright (c) 2011 Thomas Bernard

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/
#include <boost/spirit/repository/include/qi_keywords.hpp>
#include <boost/spirit/repository/include/qi_kwd.hpp>

#include <boost/spirit/include/qi_operator.hpp>
#include <boost/spirit/include/qi_char.hpp>
#include <boost/spirit/include/qi_string.hpp>
#include <boost/spirit/include/qi_numeric.hpp>
#include <boost/spirit/include/qi_directive.hpp>
#include <boost/spirit/include/qi_action.hpp>
#include <boost/spirit/include/qi_nonterminal.hpp>
#include <boost/spirit/include/support_argument.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>
#include <boost/phoenix/stl/container.hpp>

#include <vector>
#include <string>
#include <iostream>
#include "test.hpp"

struct x_attr
{

};

namespace boost { namespace spirit { namespace traits
{


    template <>
    struct container_value<x_attr>
    {
        typedef char type; // value type of container
    };


    template <>
    struct push_back_container<x_attr, char>
    {
        static bool call(x_attr& /*c*/, char /*val*/)
        {
            // push back value type into container
            return true;
        }
    };
}}}

int
main()
{
    using spirit_test::test_attr;
    using spirit_test::test;
    using namespace boost::spirit;
    using namespace boost::spirit::ascii;
    using boost::spirit::repository::kwd;
    using boost::spirit::repository::ikwd;
    using boost::spirit::repository::dkwd;
    using boost::spirit::qi::inf;
    using boost::spirit::qi::omit;
    using boost::spirit::qi::int_;
    using boost::spirit::qi::lit;
    using boost::spirit::qi::_1;
    using boost::spirit::qi::lexeme;


    {

        // no constraints
        boost::fusion::vector<char,char,int> data;
        BOOST_TEST( test_attr("c=1 a=a", kwd("a")[ '=' > char_] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], data, space));
        BOOST_TEST( boost::fusion::at_c<0>(data) == 'a' );
        BOOST_TEST( boost::fusion::at_c<1>(data) == 0 );
        BOOST_TEST( boost::fusion::at_c<2>(data) == 1 );

        BOOST_TEST( test("a=a c=1", kwd("a")[ '=' > char_] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));

        BOOST_TEST( test("", kwd("a")[ '=' > char_] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));

        // Exact
        BOOST_TEST(test("a=a b=b c=1", kwd("a",1)[ '=' > char_] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));
        BOOST_TEST(test("a=a b=c b=e c=1", kwd("a",1)[ '=' > char_] / kwd("b",2)[ '=' > char_] / kwd("c")['=' > int_], space));
        BOOST_TEST(!test("b=c b=e c=1", kwd("a",1)[ '=' > char_] / kwd("b",2)[ '=' > char_] / kwd("c")['=' > int_], space));

        // Min - Max
        BOOST_TEST(test("a=f b=c b=e c=1", kwd("a",1,2)[ '=' > char_] / kwd("b",0,2)[ '=' > char_] / kwd("c",1,2)['=' > int_], space));
        BOOST_TEST(!test("b=c b=e c=1", kwd("a",1,2)[ '=' > char_] / kwd("b",0,1)[ '=' > char_] / kwd("c",1,2)['=' > int_], space));
        BOOST_TEST(test("a=g a=f b=c b=e c=1", kwd("a",1,2)[ '=' > char_] / kwd("b",0,2)[ '=' > char_] / kwd("c",1,2)['=' > int_], space));
        BOOST_TEST(!test("a=f a=e b=c b=e a=p c=1", kwd("a",1,2)[ '=' > char_] / kwd("b",0,1)[ '=' > char_] / kwd("c",1,2)['=' > int_], space));

        // Min - inf
        BOOST_TEST(test("a=f b=c b=e c=1", kwd("a",1,inf)[ '=' > char_] / kwd("b",0,inf)[ '=' > char_] / kwd("c",1,inf)['=' > int_], space ));
        BOOST_TEST(!test("b=c b=e c=1", kwd("a",1,inf)[ '=' > char_] / kwd("b",0,inf)[ '=' > char_] / kwd("c",1,inf)['=' > int_], space ));
        BOOST_TEST(test("a=f a=f a=g b=c b=e c=1 a=e", kwd("a",1,inf)[ '=' > char_] / kwd("b",0,inf)[ '=' > char_] / kwd("c",1,inf)['=' > int_], space ));
    }

    {   // Single keyword, empty string
        BOOST_TEST(test(" ", kwd("aad")[char_],space));
        // Single keyword
        BOOST_TEST(test("aad E ", kwd("aad")[char_],space));
        // Single no case keyword
        BOOST_TEST(test("AaD E ", ikwd("aad")[char_],space));

    }

    {
        // Vector container
        boost::fusion::vector<std::vector<int>,std::vector<int>,std::vector<int> > data;
        BOOST_TEST(test_attr(" a=1 b=2 b=5 c=3",kwd("a")[ '=' > int_] / kwd("b")[ '=' > int_] / kwd("c")['=' > int_] , data, space)
                    && (boost::fusion::at_c<0>(data).size()==1)
                    && (boost::fusion::at_c<0>(data)[0]==1)

                    &&(boost::fusion::at_c<1>(data).size()==2)
                    &&(boost::fusion::at_c<1>(data)[0]==2)
                    &&(boost::fusion::at_c<1>(data)[1]==5)

                    &&(boost::fusion::at_c<2>(data).size()==1)
                    &&(boost::fusion::at_c<2>(data)[0]==3)
            );
    }

    {
        // no_case test
        BOOST_TEST( test("B=a c=1 a=E", no_case[kwd("a")[ "=E" ] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_]], space));
        BOOST_TEST( test("B=a c=1 a=e", no_case[kwd("a")[ "=E" ] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_]], space));
        BOOST_TEST( !test("B=a c=1 A=E", no_case[kwd("a")[ '=' > char_]] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));
        BOOST_TEST( test("b=a c=1 A=E", no_case[kwd("a")[ '=' > char_]] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));
        BOOST_TEST( !test("A=a c=1 a=E", kwd("a")[ '=' > char_] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));
        BOOST_TEST( test("A=a c=1 a=E", ikwd("a")[ '=' > char_] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));
        BOOST_TEST( !test("A=a C=1 a=E", ikwd("a")[ '=' > char_] / kwd("b")[ '=' > char_] / kwd("c")['=' > int_], space));
    }

    {
        // iterator restoration
        BOOST_TEST( test("a=a c=1 ba=d", (kwd("a")[ '=' > char_] / kwd("b")[ '=' > int_] / kwd("c")['=' > int_] ) > lit("ba=") > char_, space));
        BOOST_TEST( test("A=a c=1 ba=d", (ikwd("a")[ '=' > char_] / kwd("b")[ '=' > int_] / kwd("c")['=' > int_] ) > lit("ba=") > char_, space));
    }

    { // actions
        namespace phx = boost::phoenix;

        std::vector<int> v;
        BOOST_TEST(test("b=2 c=4", kwd("b")['=' > int_][phx::ref(v)=_1] / kwd("c")[ '=' > int_ ],space) &&
            v[0] == 2 );
    }


    {
        // no constraints
        boost::fusion::vector<char,char,int> data;
        BOOST_TEST( test_attr("c,1,2=1 2,b=a", kwd( char_ >> lit(',') >> int_ )[ '=' >> char_] / kwd(int_ >> lit(',') >> char_)[ '=' >> char_] / kwd(char_ >> lit(',') >> int_ >> lit(',') >> int_)['=' >> int_], data, space));
        BOOST_TEST( boost::fusion::at_c<0>(data) == 0 );
        BOOST_TEST( boost::fusion::at_c<1>(data) == 'a');
        BOOST_TEST( boost::fusion::at_c<2>(data) == 1 );

        BOOST_TEST( test("2,b=a c,1,2=1", kwd( char_ >> ',' >> int_ )[ '=' >> char_] / kwd(int_ >> ',' >> char_)[ '=' >> char_] / kwd(char_ >> ',' >> int_ >> ',' >> int_)['=' >> int_], space));

        BOOST_TEST( test("", kwd( char_ >> ',' >> int_ )[ '=' >> char_] / kwd(int_ >> ',' >> char_)[ '=' >> char_] / kwd(char_ >> ',' >> int_ >> ',' >> int_)['=' >> int_], space));

        // Exact
        BOOST_TEST(test("7a=a 5b=b 2c=1", kwd(int_ >> lit('a'),1)[ '=' >> char_] / kwd(int_ >> lit('b'))[ '=' >> char_] / kwd(int_ >> lit('c'))['=' >> int_], space));
        BOOST_TEST(test("7a=a 3b=d 5b=b 2c=1", kwd(int_ >> lit('a'),1)[ '=' >> char_] / kwd(int_ >> lit('b'),2)[ '=' >> char_] / kwd(int_ >>'c')['=' >> int_], space));
        BOOST_TEST(!test("7a=a 5b=b 2c=1", kwd(int_ >> lit('a'),1)[ '=' >> char_] / kwd(int_ >> lit('b'),2)[ '=' >> char_] / kwd(int_ >>'c')['=' >> int_], space));

        // Min - Max
        BOOST_TEST(test("6a=f 2b=c 3b=e 1c=1", kwd(int_ >> "a",1,2)[ '=' >> char_] / kwd(int_ >> "b",0,2)[ '=' >> char_] / kwd(int_ >> "c",1,2)['=' >> int_], space));
        BOOST_TEST(!test("1b=c 6b=e 2c=1", kwd(int_ >> "a",1,2)[ '=' >> char_] / kwd(int_ >> "b",0,1)[ '=' >> char_] / kwd(int_ >> "c",1,2)['=' >> int_], space));
        BOOST_TEST(test("4a=g 7a=f 2b=c 1b=e 4c=1", kwd(int_ >> "a",1,2)[ '=' >> char_] / kwd(int_ >> "b",0,2)[ '=' >> char_] / kwd(int_ >> "c",1,2)['=' >> int_], space));
        BOOST_TEST(!test("1a=f a=e 2b=c 5b=e 6a=p 67c=1", kwd(int_ >> "a",1,2)[ '=' >> char_] / kwd(int_ >> "b",0,1)[ '=' >> char_] / kwd(int_ >> "c",1,2)['=' >> int_], space));

        // Min - inf
        BOOST_TEST(test("41a=f 44b=c 12b=e 45c=1", kwd(int_ >> "a",1,inf)[ '=' >> char_] / kwd(int_ >> "b",0,inf)[ '=' >> char_] / kwd(int_ >> "c",1,inf)['=' >> int_], space ));
        BOOST_TEST(!test("31b=c 55b=e 2c=1", kwd("a",1,inf)[ '=' >> char_] / kwd("b",0,inf)[ '=' >> char_] / kwd("c",1,inf)['=' >> int_], space ));
        BOOST_TEST(test("12a=f 45a=f 12a=g 1b=c 7b=e 12c=1 6a=e", kwd(int_ >> "a",1,inf)[ '=' >> char_] / kwd(int_ >> "b",0,inf)[ '=' >> char_] / kwd(int_ >> "c",1,inf)['=' >> int_], space ));

    }

    {

        // Vector container
        boost::fusion::vector<std::vector<int>,std::vector<int>,std::vector<int> > data;
        BOOST_TEST(test_attr(" 41a=1 4b=2 12b=5 5c=3",kwd(int_ >> "a")[ '=' >> int_] / kwd(int_ >> "b")[ '=' >> int_] / kwd(int_ >> "c")['=' >> int_] , data, space)
            && (boost::fusion::at_c<0>(data).size()==1)
            && (boost::fusion::at_c<0>(data)[0]==1)

            &&(boost::fusion::at_c<1>(data).size()==2)
            &&(boost::fusion::at_c<1>(data)[0]==2)
            &&(boost::fusion::at_c<1>(data)[1]==5)

            &&(boost::fusion::at_c<2>(data).size()==1)
            &&(boost::fusion::at_c<2>(data)[0]==3)
         );



    }

    {
        // no_case test
        BOOST_TEST( test("12B=a 5c=1 1a=E", no_case[kwd(int_ >> "a")[ "=E" ] / kwd(int_ >> "b")[ '=' >> char_] / kwd(int_ >> "c")['=' >> int_]], space));
        BOOST_TEST( test("5B=a 2c=1 5a=e", no_case[kwd(int_ >> "a")[ "=E" ] / kwd(int_ >> "b")[ '=' >> char_] / kwd(int_ >> "c")['=' >> int_]], space));
        BOOST_TEST( !test("1B=a 8c=1 1A=E", no_case[kwd(int_ >> "a")[ '=' >> char_]] / kwd(int_ >> "b")[ '=' >> char_] / kwd(int_ >> "c")['=' >> int_], space));
        BOOST_TEST( test("2b=a 6c=1 5A=E", no_case[kwd(int_ >> "a")[ '=' >> char_]] / kwd(int_ >> "b")[ '=' >> char_] / kwd(int_ >> "c")['=' >> int_], space));
        BOOST_TEST( !test("1A=a 5c=1 1a=E", kwd(int_ >> "a")[ '=' >> char_] / kwd(int_ >> "b")[ '=' >> char_] / kwd(int_ >> "c")['=' >> int_], space));
        BOOST_TEST( test("A=a 23c=1 a=E", ikwd("a")[ '=' >> char_] / kwd(int_ >> "b")[ '=' >> char_] / kwd(int_ >> "c")['=' >> int_], space));
        BOOST_TEST( !test("A=a 21C=1 a=E", ikwd("a")[ '=' >> char_] / kwd(int_ >> "b")[ '=' >> char_] / kwd(int_ >> "c")['=' >> int_], space));
    }

    {
        // iterator restoration
        BOOST_TEST( test("4a=a c4=1 ba=d", (kwd(int_ >> "a")[ '=' >> char_] / kwd("b" >> int_)[ '=' >> int_] / kwd("c" >> int_ )['=' >> int_] ) >> lit("ba=") >> char_, space));
    }

    { // actions
        namespace phx = boost::phoenix;

        std::vector<int> v;
        BOOST_TEST(test("b4=2 c1=4", kwd("b" >> int_)['=' >> int_][phx::ref(v)=_1] / kwd("c" >> int_)[ '=' >> int_ ],space) &&
            v[0] == 2 );
    }

    {
        // complex keyword single test
        int result=0;

        BOOST_TEST( test_attr("(a,1) = 3214", kwd( '(' >> char_ >> ',' >> int_ >> ')' )['=' >> int_], result, space) );
        BOOST_TEST(result==3214);
    }
    {
        // Mixed complex keyword loop
        boost::fusion::vector<int,int,int> data;

        BOOST_TEST( test_attr("(a,1) = 3214 b += 2 hello 10 (a,2)=31", kwd( '(' >> char_ >> ',' >> int_ >> ')' )['=' >> int_] / kwd("hello")[int_] / kwd("b")["+=" >> int_], data, space) );
        BOOST_TEST( boost::fusion::at_c<0>(data) == 31);
        BOOST_TEST( boost::fusion::at_c<1>(data) == 10);
        BOOST_TEST( boost::fusion::at_c<2>(data) == 2);
    }

    // dkwd and idkwd
    {
      BOOST_TEST( test("a =a", dkwd("a")[ '=' > char_] , space));
      BOOST_TEST( !test("a=a", dkwd("a")[ '=' > char_] , space));
      BOOST_TEST(test("a =a b =b c=1", dkwd("a",1)[ '=' > char_] / dkwd("b",1,2)[ '=' > char_] / kwd("c")['=' > int_], space));
      BOOST_TEST(!test("a=a b=b c =1", dkwd("a",1)[ '=' > char_] / dkwd("b",1,2)[ '=' > char_] / kwd("c")['=' > int_], space));

      BOOST_TEST(test("a =a b =b b =d c=1", dkwd("a",1,inf)[ '=' > char_] / dkwd("b",2,inf)[ '=' > char_] / kwd("c")['=' > int_], space));

    }

    { // attribute customization

//        x_attr x;
//        test_attr("a = b c = d", kwd("a")['=' > char_] / kwd("c")['=' > char_], x);
    }


   return boost::report_errors();
}

