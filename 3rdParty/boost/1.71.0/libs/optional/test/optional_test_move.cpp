// Copyright (C) 2014 - 2015 Andrzej Krzemienski.
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

using boost::optional;
using boost::none;

//#ifndef BOOST_OPTIONAL_NO_CONVERTING_ASSIGNMENT
//#ifndef BOOST_OPTIONAL_NO_CONVERTING_COPY_CTOR

#ifndef BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES

enum State
{
    sDefaultConstructed,
    sValueCopyConstructed,
    sValueMoveConstructed,
    sCopyConstructed,
    sMoveConstructed,
    sMoveAssigned,
    sCopyAssigned,
    sValueCopyAssigned,
    sValueMoveAssigned,
    sMovedFrom,
    sIntConstructed
};

struct OracleVal
{
    State s;
    int i;
    OracleVal(int i = 0) : s(sIntConstructed), i(i) {}
};


struct Oracle
{
    State s;
    OracleVal val;

    Oracle() : s(sDefaultConstructed) {}
    Oracle(const OracleVal& v) : s(sValueCopyConstructed), val(v) {}
    Oracle(OracleVal&& v) : s(sValueMoveConstructed), val(std::move(v)) {v.s = sMovedFrom;}
    Oracle(const Oracle& o) : s(sCopyConstructed), val(o.val) {}
    Oracle(Oracle&& o) : s(sMoveConstructed), val(std::move(o.val)) {o.s = sMovedFrom;}

    Oracle& operator=(const OracleVal& v) { s = sValueCopyAssigned; val = v; return *this; }
    Oracle& operator=(OracleVal&& v) { s = sValueMoveAssigned; val = std::move(v); v.s = sMovedFrom; return *this; }
    Oracle& operator=(const Oracle& o) { s = sCopyAssigned; val = o.val; return *this; }
    Oracle& operator=(Oracle&& o) { s = sMoveAssigned; val = std::move(o.val); o.s = sMovedFrom; return *this; }
};

bool operator==( Oracle const& a, Oracle const& b ) { return a.val.i == b.val.i; }
bool operator!=( Oracle const& a, Oracle const& b ) { return a.val.i != b.val.i; }


void test_move_ctor_from_U()
{
  optional<Oracle> o1 ((OracleVal()));
  BOOST_TEST(o1);
  BOOST_TEST(o1->s == sValueMoveConstructed || o1->s == sMoveConstructed);
    
  OracleVal v1;
  optional<Oracle> o2 (v1);
  BOOST_TEST(o2);
  BOOST_TEST(o2->s == sValueCopyConstructed || o2->s == sCopyConstructed || o2->s == sMoveConstructed );
  BOOST_TEST(v1.s == sIntConstructed);
    
  optional<Oracle> o3 (boost::move(v1));
  BOOST_TEST(o3);
  BOOST_TEST(o3->s == sValueMoveConstructed || o3->s == sMoveConstructed);
  BOOST_TEST(v1.s == sMovedFrom);
}

void test_move_ctor_form_T()
{
  optional<Oracle> o1 ((Oracle()));
  BOOST_TEST(o1);
  BOOST_TEST(o1->s == sMoveConstructed);
  
  Oracle v1;
  optional<Oracle> o2 (v1);
  BOOST_TEST(o2);
  BOOST_TEST(o2->s == sCopyConstructed);
  BOOST_TEST(v1.s == sDefaultConstructed);
    
  optional<Oracle> o3 (boost::move(v1));
  BOOST_TEST(o3);
  BOOST_TEST(o3->s == sMoveConstructed);
  BOOST_TEST(v1.s == sMovedFrom);
}

void test_move_ctor_from_optional_T()
{
  optional<Oracle> o1;
  optional<Oracle> o2(boost::move(o1));
  
  BOOST_TEST(!o1);
  BOOST_TEST(!o2);
  
  optional<Oracle> o3((Oracle()));
  optional<Oracle> o4(boost::move(o3));
  BOOST_TEST(o3);
  BOOST_TEST(o4);
  BOOST_TEST(o3->s == sMovedFrom);
  BOOST_TEST(o4->s == sMoveConstructed);
  
  optional<Oracle> o5((optional<Oracle>()));
  BOOST_TEST(!o5);
  
  optional<Oracle> o6((optional<Oracle>(Oracle())));
  BOOST_TEST(o6);
  BOOST_TEST(o6->s == sMoveConstructed);
  
  optional<Oracle> o7(o6); // does copy ctor from non-const lvalue compile?
}

void test_move_assign_from_U()
{
  optional<Oracle> o1 = boost::none; // test if additional ctors didn't break it
  o1 = boost::none;                  // test if additional assignments didn't break it
  o1 = OracleVal();
  BOOST_TEST(o1);
  
  BOOST_TEST(o1->s == sValueMoveConstructed);  
  
  o1 = OracleVal();
  BOOST_TEST(o1);
  BOOST_TEST(o1->s == sMoveAssigned); 
    
  OracleVal v1;
  optional<Oracle> o2;
  o2 = v1;
  BOOST_TEST(o2);
  BOOST_TEST(o2->s == sValueCopyConstructed);
  BOOST_TEST(v1.s == sIntConstructed);
  o2 = v1;
  BOOST_TEST(o2);
  BOOST_TEST(o2->s == sCopyAssigned || o2->s == sMoveAssigned);
  BOOST_TEST(v1.s == sIntConstructed);
    
  optional<Oracle> o3;
  o3 = boost::move(v1);
  BOOST_TEST(o3);
  BOOST_TEST(o3->s == sValueMoveConstructed);
  BOOST_TEST(v1.s == sMovedFrom);
}

void test_move_assign_from_T()
{
  optional<Oracle> o1;
  o1 = Oracle();
  BOOST_TEST(o1);
  BOOST_TEST(o1->s == sMoveConstructed);  
  
  o1 = Oracle();
  BOOST_TEST(o1);
  BOOST_TEST(o1->s == sMoveAssigned); 
    
  Oracle v1;
  optional<Oracle> o2;
  o2 = v1;
  BOOST_TEST(o2);
  BOOST_TEST(o2->s == sCopyConstructed);
  BOOST_TEST(v1.s == sDefaultConstructed);
  o2 = v1;
  BOOST_TEST(o2);
  BOOST_TEST(o2->s == sCopyAssigned);
  BOOST_TEST(v1.s == sDefaultConstructed);
    
  optional<Oracle> o3;
  o3 = boost::move(v1);
  BOOST_TEST(o3);
  BOOST_TEST(o3->s == sMoveConstructed);
  BOOST_TEST(v1.s == sMovedFrom);
}

void test_move_assign_from_optional_T()
{
    optional<Oracle> o1;
    optional<Oracle> o2;
    o1 = optional<Oracle>();
    BOOST_TEST(!o1);
    optional<Oracle> o3((Oracle()));
    o1 = o3;
    BOOST_TEST(o3);
    BOOST_TEST(o3->s == sMoveConstructed);
    BOOST_TEST(o1);
    BOOST_TEST(o1->s == sCopyConstructed);
    
    o2 = boost::move(o3);
    BOOST_TEST(o3);
    BOOST_TEST(o3->s == sMovedFrom);
    BOOST_TEST(o2);
    BOOST_TEST(o2->s == sMoveConstructed);
    
    o2 = optional<Oracle>((Oracle()));
    BOOST_TEST(o2);
    BOOST_TEST(o2->s == sMoveAssigned);
}

class MoveOnly
{
public:
  int val;
  MoveOnly(int v) : val(v) {}
  MoveOnly(MoveOnly&& rhs) : val(rhs.val) { rhs.val = 0; }
  void operator=(MoveOnly&& rhs) {val = rhs.val; rhs.val = 0; }
  
private:
  MoveOnly(MoveOnly const&);
  void operator=(MoveOnly const&);
  
  friend class MoveOnlyB;
};

void test_with_move_only()
{
    optional<MoveOnly> o1;
    optional<MoveOnly> o2((MoveOnly(1)));
    BOOST_TEST(o2);
    BOOST_TEST(o2->val == 1);
    optional<MoveOnly> o3 (boost::move(o1));
    BOOST_TEST(!o3);
    optional<MoveOnly> o4 (boost::move(o2));
    BOOST_TEST(o4);
    BOOST_TEST(o4->val == 1);
    BOOST_TEST(o2);
    BOOST_TEST(o2->val == 0);
    
    o3 = boost::move(o4);
    BOOST_TEST(o3);
    BOOST_TEST(o3->val == 1);
    BOOST_TEST(o4);
    BOOST_TEST(o4->val == 0);
}

class MoveOnlyB
{
public:
  int val;
  MoveOnlyB(int v) : val(v) {}
  MoveOnlyB(MoveOnlyB&& rhs) : val(rhs.val) { rhs.val = 0; }
  void operator=(MoveOnlyB&& rhs) {val = rhs.val; rhs.val = 0; }
  MoveOnlyB(MoveOnly&& rhs) : val(rhs.val) { rhs.val = 0; }
  void operator=(MoveOnly&& rhs) {val = rhs.val; rhs.val = 0; }
  
private:
  MoveOnlyB(MoveOnlyB const&);
  void operator=(MoveOnlyB const&);
  MoveOnlyB(MoveOnly const&);
  void operator=(MoveOnly const&);
};

void test_move_assign_from_optional_U()
{
    optional<MoveOnly> a((MoveOnly(2)));
    optional<MoveOnlyB> b1;
    b1 = boost::move(a);
    
    BOOST_TEST(b1);
    BOOST_TEST(b1->val == 2);
    BOOST_TEST(a);
    BOOST_TEST(a->val == 0);
    
    b1 = MoveOnly(4);
    
    BOOST_TEST(b1);
    BOOST_TEST(b1->val == 4);
}

void test_move_ctor_from_optional_U()
{
    optional<MoveOnly> a((MoveOnly(2)));
    optional<MoveOnlyB> b1(boost::move(a));
    
    BOOST_TEST(b1);
    BOOST_TEST(b1->val == 2);
    BOOST_TEST(a);
    BOOST_TEST(a->val == 0);
    
    optional<MoveOnlyB> b2(( optional<MoveOnly>(( MoveOnly(4) )) ));
    
    BOOST_TEST(b2);
    BOOST_TEST(b2->val == 4);
}

void test_swap()
{
    optional<MoveOnly> a((MoveOnly(2)));
    optional<MoveOnly> b((MoveOnly(3)));
    swap(a, b);
    
    BOOST_TEST(a->val == 3);
    BOOST_TEST(b->val == 2);
}

void test_optional_ref_to_movables()
{
    MoveOnly m(3);
    optional<MoveOnly&> orm = m;
    orm->val = 2;
    BOOST_TEST(m.val == 2);
    
    optional<MoveOnly&> orm2 = orm;
    orm2->val = 1;
    BOOST_TEST(m.val == 1);
    BOOST_TEST(orm->val == 1);
    
    optional<MoveOnly&> orm3 = boost::move(orm);
    orm3->val = 4;
    BOOST_TEST(m.val == 4);
    BOOST_TEST(orm->val == 4);
    BOOST_TEST(orm2->val == 4);
}

#endif

int main()
{
#ifndef BOOST_OPTIONAL_DETAIL_NO_RVALUE_REFERENCES
    test_move_ctor_from_U();
    test_move_ctor_form_T();
    test_move_ctor_from_optional_T();
    test_move_ctor_from_optional_U();
    test_move_assign_from_U();
    test_move_assign_from_T();
    test_move_assign_from_optional_T();
    test_move_assign_from_optional_U();
    test_with_move_only();
    test_optional_ref_to_movables();
    test_swap();
#endif

  return boost::report_errors();
}
