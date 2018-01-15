// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#include "boost/optional/optional.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"
#include "boost/none.hpp"

//#ifndef BOOST_OPTIONAL_NO_CONVERTING_ASSIGNMENT
//#ifndef BOOST_OPTIONAL_NO_CONVERTING_COPY_CTOR

using boost::optional;
using boost::none;
using boost::in_place_init;
using boost::in_place_init_if;

#if (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES) && (!defined BOOST_NO_CXX11_VARIADIC_TEMPLATES)

class Guard
{
public:
    int which_ctor;
    Guard () : which_ctor(0) { }
    Guard (int&, double&&) : which_ctor(1) { }
    Guard (int&&, double&) : which_ctor(2) { }
    Guard (int&&, double&&) : which_ctor(3) { }
    Guard (int&, double&) : which_ctor(4) { }
    Guard (std::string const&) : which_ctor(5) { }
    Guard (std::string &) : which_ctor(6) { }
    Guard (std::string &&) : which_ctor(7) { }
private:
    Guard(Guard&&);
    Guard(Guard const&);
    void operator=(Guard &&);
    void operator=(Guard const&);
};


void test_emplace()
{
    int i = 0;
    double d = 0.0;
    const std::string cs;
    std::string ms;
    optional<Guard> o;
    
    o.emplace();
    BOOST_TEST(o);
    BOOST_TEST(0 == o->which_ctor);
    
    o.emplace(i, 2.0);
    BOOST_TEST(o);
    BOOST_TEST(1 == o->which_ctor);
    
    o.emplace(1, d);
    BOOST_TEST(o);
    BOOST_TEST(2 == o->which_ctor);
    
    o.emplace(1, 2.0);
    BOOST_TEST(o);
    BOOST_TEST(3 == o->which_ctor);
    
    o.emplace(i, d);
    BOOST_TEST(o);
    BOOST_TEST(4 == o->which_ctor);
    
    o.emplace(cs);
    BOOST_TEST(o);
    BOOST_TEST(5 == o->which_ctor);
    
    o.emplace(ms);
    BOOST_TEST(o);
    BOOST_TEST(6 == o->which_ctor);
    
    o.emplace(std::string());
    BOOST_TEST(o);
    BOOST_TEST(7 == o->which_ctor);
}

void test_in_place_ctor()
{
    int i = 0;
    double d = 0.0;
    const std::string cs;
    std::string ms;
        
  {
    optional<Guard> o (in_place_init);
    BOOST_TEST(o);
    BOOST_TEST(0 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init, i, 2.0);
    BOOST_TEST(o);
    BOOST_TEST(1 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init, 1, d);
    BOOST_TEST(o);
    BOOST_TEST(2 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init, 1, 2.0);
    BOOST_TEST(o);
    BOOST_TEST(3 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init, i, d);
    BOOST_TEST(o);
    BOOST_TEST(4 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init, cs);
    BOOST_TEST(o);
    BOOST_TEST(5 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init, ms);
    BOOST_TEST(o);
    BOOST_TEST(6 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init, std::string());
    BOOST_TEST(o);
    BOOST_TEST(7 == o->which_ctor);
  }
}

void test_in_place_if_ctor()
{
    int i = 0;
    double d = 0.0;
    const std::string cs;
    std::string ms;
        
  {
    optional<Guard> o (in_place_init_if, true);
    BOOST_TEST(o);
    BOOST_TEST(0 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init_if, true, i, 2.0);
    BOOST_TEST(o);
    BOOST_TEST(1 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init_if, true, 1, d);
    BOOST_TEST(o);
    BOOST_TEST(2 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init_if, true, 1, 2.0);
    BOOST_TEST(o);
    BOOST_TEST(3 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init_if, true, i, d);
    BOOST_TEST(o);
    BOOST_TEST(4 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init_if, true, cs);
    BOOST_TEST(o);
    BOOST_TEST(5 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init_if, true, ms);
    BOOST_TEST(o);
    BOOST_TEST(6 == o->which_ctor);
  }
  {  
    optional<Guard> o (in_place_init_if, true, std::string());
    BOOST_TEST(o);
    BOOST_TEST(7 == o->which_ctor);
  }
  
  {
    optional<Guard> o (in_place_init_if, false, 1, 2.0);
    BOOST_TEST(!o);
  }
}


#endif

#if (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES)


struct ThrowOnMove
{
    ThrowOnMove(ThrowOnMove&&) { throw int(); }
    ThrowOnMove(ThrowOnMove const&) { throw int(); }
    ThrowOnMove(int){}
};


void test_no_moves_on_emplacement()
{
    try {
        optional<ThrowOnMove> o;
        o.emplace(1);
        BOOST_TEST(o);
    } 
    catch (...) {
        BOOST_TEST(false);
    }
}

void test_no_moves_on_in_place_ctor()
{
    try {
        optional<ThrowOnMove> o (in_place_init, 1);
        BOOST_TEST(o);
        
        optional<ThrowOnMove> p (in_place_init_if, true, 1);
        BOOST_TEST(p);
        
        optional<ThrowOnMove> q (in_place_init_if, false, 1);
        BOOST_TEST(!q);
    } 
    catch (...) {
        BOOST_TEST(false);
    }
}

#endif

struct Thrower
{
    Thrower(bool throw_) { if (throw_) throw int(); }
    
private:
    Thrower(Thrower const&);
#if (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES)
    Thrower(Thrower&&);
#endif
};

void test_clear_on_throw()
{
    optional<Thrower> ot;
    try {
        ot.emplace(false);
        BOOST_TEST(ot);
    } catch(...) {
        BOOST_TEST(false);
    }
    
    try {
        ot.emplace(true);
        BOOST_TEST(false);
    } catch(...) {
        BOOST_TEST(!ot);
    }
}

void test_no_assignment_on_emplacement()
{
    optional<const std::string> os, ot;
    BOOST_TEST(!os);
    os.emplace("wow");
    BOOST_TEST(os);
    BOOST_TEST_EQ(*os, "wow");
    
    BOOST_TEST(!ot);
    ot.emplace();
    BOOST_TEST(ot);
    BOOST_TEST_EQ(*ot, "");
}

namespace no_rvalue_refs {

class Guard
{
public:
    int which_ctor;
    Guard () : which_ctor(0) { }
    Guard (std::string const&) : which_ctor(5) { }
    Guard (std::string &) : which_ctor(6) { }
private:
    Guard(Guard const&);
    void operator=(Guard const&);
};

void test_emplace()
{
    const std::string cs;
    std::string ms;
    optional<Guard> o;
    
    o.emplace();
    BOOST_TEST(o);
    BOOST_TEST(0 == o->which_ctor);
    
    o.emplace(cs);
    BOOST_TEST(o);
    BOOST_TEST(5 == o->which_ctor);
    
    o.emplace(ms);
    BOOST_TEST(o);
    BOOST_TEST(6 == o->which_ctor);
}

void test_in_place_ctor()
{
  const std::string cs;
  std::string ms;
  
  {
    optional<Guard> o (in_place_init);
    BOOST_TEST(o);
    BOOST_TEST(0 == o->which_ctor);
  }
  {
    optional<Guard> o (in_place_init, cs);
    BOOST_TEST(o);
    BOOST_TEST(5 == o->which_ctor);
  }
  {
    optional<Guard> o (in_place_init, ms);
    BOOST_TEST(o);
    BOOST_TEST(6 == o->which_ctor);
  }
}

void test_in_place_if_ctor()
{
  const std::string cs;
  std::string ms;
  
  {
    optional<Guard> n (in_place_init_if, false);
    BOOST_TEST(!n);
    
    optional<Guard> o (in_place_init_if, true);
    BOOST_TEST(o);
    BOOST_TEST(0 == o->which_ctor);
  }
  {
    optional<Guard> n (in_place_init_if, false, cs);
    BOOST_TEST(!n);
    
    optional<Guard> o (in_place_init_if, true, cs);
    BOOST_TEST(o);
    BOOST_TEST(5 == o->which_ctor);
  }
  {
    optional<Guard> n (in_place_init_if, false, ms);
    BOOST_TEST(!n);
    
    optional<Guard> o (in_place_init_if, true, ms);
    BOOST_TEST(o);
    BOOST_TEST(6 == o->which_ctor);
  }
}

} // namespace no_rvalue_ref

int main()
{
#if (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES) && (!defined BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    test_emplace();
    test_in_place_ctor();
    test_in_place_if_ctor();
#endif
#if (!defined BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES)
    test_no_moves_on_emplacement();
    test_no_moves_on_in_place_ctor();
#endif
    test_clear_on_throw();
    test_no_assignment_on_emplacement();
    no_rvalue_refs::test_emplace();
    no_rvalue_refs::test_in_place_ctor();
    no_rvalue_refs::test_in_place_if_ctor();
  
    return boost::report_errors();
}
