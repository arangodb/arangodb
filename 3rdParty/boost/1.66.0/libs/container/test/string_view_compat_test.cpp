//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2007-2017. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/container/string.hpp>
#include <boost/utility/string_view.hpp>

#include <boost/core/lightweight_test.hpp>

void conversion_test()
{
   #ifndef BOOST_CONTAINER_TEMPLATED_CONVERSION_OPERATOR_BROKEN
   {
      const boost::container::string s = "some text";
      boost::string_view sv(s);
      BOOST_TEST(s.data() == sv.data() && s.size() == sv.size());
      boost::string_view sv2;
      sv2 = s;
      BOOST_TEST(s.data() == sv2.data() && s.size() == sv2.size());
      const boost::string_view csv(s);
      BOOST_TEST(s.data() == sv.data() && s.size() == csv.size());
   }
   #endif
}

void to_view_test()
{
   const boost::container::string s = "some text";
   boost::string_view sv(s.to_view<boost::string_view>());
   BOOST_TEST(s.data() == sv.data() && s.size() == sv.size());
   boost::string_view sv2;
   sv2 = s.to_view<boost::string_view>();
   BOOST_TEST(s.data() == sv2.data() && s.size() == sv2.size());
   const boost::string_view csv(s.to_view<boost::string_view>());
   BOOST_TEST(s.data() == csv.data() && s.size() == csv.size());
}

void equal_test()
{
   const boost::string_view sv = "same text";
   const boost::string_view svd = "different text";
   const boost::container::string s = "same text";
   BOOST_TEST(sv == s);
   BOOST_TEST(s == sv);
   BOOST_TEST(!(svd == s));
   BOOST_TEST(!(s == svd));
}

void unequal_test()
{
   const boost::string_view sv = "same text";
   const boost::string_view svd = "different text";
   const boost::container::string s = "same text";
   BOOST_TEST(!(sv != s));
   BOOST_TEST(!(s != sv));
   BOOST_TEST(svd != s);
   BOOST_TEST(s != svd);
}

void less_test()
{
   boost::string_view sv  = "0123456";
   boost::container::string s = "0123459";
   BOOST_TEST(sv < s);
   BOOST_TEST(!(s < sv));

   sv = "0123459";
   s  = "0123456";
   BOOST_TEST(!(sv < s));
   BOOST_TEST(s < sv);

   sv = "0123456";
   BOOST_TEST(!(sv < s));
   BOOST_TEST(!(s < sv));
}

void greater_test()
{
   boost::string_view sv  = "0123459";
   boost::container::string s = "0123456";
   BOOST_TEST(sv > s);
   BOOST_TEST(!(s > sv));

   sv = "0123456";
   s  = "0123459";
   BOOST_TEST(!(sv > s));
   BOOST_TEST(s > sv);

   sv = "0123459";
   BOOST_TEST(!(sv > s));
   BOOST_TEST(!(s > sv));
}

void less_equal_test()
{
   boost::string_view sv  = "0123456";
   boost::container::string s = "0123459";
   BOOST_TEST(sv <= s);
   BOOST_TEST(!(s <= sv));

   sv = "0123459";
   s  = "0123456";
   BOOST_TEST(!(sv <= s));
   BOOST_TEST(s <= sv);

   sv = "0123456";
   BOOST_TEST(sv <= s);
   BOOST_TEST(s <= sv);
}

void greater_equal_test()
{
   boost::string_view sv  = "0123459";
   boost::container::string s = "0123456";
   BOOST_TEST(sv >= s);
   BOOST_TEST(!(s >= sv));

   sv = "0123456";
   s  = "0123459";
   BOOST_TEST(!(sv >= s));
   BOOST_TEST(s >= sv);

   sv = "0123459";
   BOOST_TEST(sv >= s);
   BOOST_TEST(s >= sv);
}

void constructor_test()
{
   boost::string_view sv  = "0123459";
   boost::container::string s(sv);
   BOOST_TEST(sv == s);
   boost::container::string s2(sv, s.get_allocator());
   BOOST_TEST(sv == s);
}

void assignment_test()
{
   boost::string_view sv  = "0123459";
   boost::container::string s;
   s = sv;
   BOOST_TEST(sv == s);
}

void assign_test()
{
   boost::string_view sv  = "0123459";
   boost::container::string s;
   s.assign(sv);
   BOOST_TEST(sv == s);
}

void plus_equal_test()
{
   boost::string_view sv  = "23459";
   boost::container::string s("01");
   s += sv;
   BOOST_TEST(s == "0123459");
}

void append_test()
{
   boost::string_view sv  = "23459";
   boost::container::string s("01");
   s.append(sv);
   BOOST_TEST(s == "0123459");
}

void insert_test()
{
   boost::string_view sv  = "34";
   boost::container::string s("01259");
   s.insert(3u, sv);
   BOOST_TEST(s == "0123459");
}

void replace_test()
{
   boost::string_view sv  = "5678";
   boost::container::string s("01259");
   s.replace(2u, 2u, sv);
   BOOST_TEST(s == "0156789");
   s.replace(s.begin()+3, s.begin()+6, sv);
   BOOST_TEST(s == "01556789");
   s.replace(5u, 3u, sv, 2u, 2u);
   BOOST_TEST(s == "0155678");
}

void find_test()
{
   const boost::string_view sv  = "25";
   boost::container::string s("0125925123");
   BOOST_TEST(s.find(sv,4) == 5);
}

void rfind_test()
{
   const boost::string_view sv  = "25";
   boost::container::string s("0125925123");
   BOOST_TEST(s.rfind(sv,4) == 2);
}

void find_first_of_test()
{
   const boost::string_view sv  = "52";
   boost::container::string s("0125925123");
   BOOST_TEST(s.find_first_of(sv,4) == 5);
}

void find_last_of_test()
{
   const boost::string_view sv  = "52";
   boost::container::string s("520125925123");
   BOOST_TEST(s.find_last_of(sv,6) == 5);
}

void find_first_not_of_test()
{
   const boost::string_view sv  = "52";
   boost::container::string s("0125925123");
   BOOST_TEST(s.find_first_not_of(sv,2) == 4);
}

void find_last_not_of_test()
{
   const boost::string_view sv  = "52";
   boost::container::string s("0125925123");
   BOOST_TEST(s.find_last_not_of(sv,6) == 4);
}

void compare_test()
{
   const boost::string_view sv  = "52";
   boost::container::string s("0125925123");
   BOOST_TEST(s.compare(sv) < 0);
   BOOST_TEST(s.compare(boost::string_view("0125925123")) == 0);
   BOOST_TEST(s.compare(2u, s.size() - 2u, boost::string_view("25925123")) == 0);
   boost::string_view sv2("5212592512389");
   BOOST_TEST(s.compare(2u, s.size() - 2u, sv2, 3, sv2.size()-5u) == 0);
}

int main()
{
   conversion_test();
   to_view_test();
   equal_test();
   unequal_test();
   less_test();
   greater_test();
   less_equal_test();
   greater_equal_test();
   constructor_test();
   assignment_test();
   assign_test();
   plus_equal_test();
   append_test();
   insert_test();
   replace_test();
   find_test();
   rfind_test();
   find_first_of_test();
   find_last_of_test();
   find_first_not_of_test();
   find_last_not_of_test();
   compare_test();

   return boost::report_errors();
}

