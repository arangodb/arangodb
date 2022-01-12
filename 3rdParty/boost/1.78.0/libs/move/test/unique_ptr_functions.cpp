//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2014-2014.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/make_unique.hpp>
#include <boost/core/lightweight_test.hpp>
#include <cstring>

struct A
{
   int a, b, c;
   static int count;
   A() : a (999), b(1000), c(1001) {++count;}
   A(int a) : a (a), b(1000), c(1001) {++count;}
   A(int a, int b) : a (a), b(b), c(1001) {++count;}
   A(int a, int b, int c) : a (a), b(b), c(c) {++count;}
   A(const A&) {++count;}
   virtual ~A() {--count;}
};

int A::count = 0;

struct B
   : public A
{
   static int count;
   B() : A() {++count;}
   B(const B&) : A() {++count;}
   virtual ~B() {--count;}
};

int B::count = 0;

void reset_counters()
{  A::count = B::count = 0;  }

static const unsigned PatternSize = 8;
static const unsigned char ff_patternbuf[PatternSize] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static const unsigned char ee_patternbuf[PatternSize] = { 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE, 0xEE };
static const unsigned char dd_patternbuf[PatternSize] = { 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD, 0xDD };
static const unsigned char cc_patternbuf[PatternSize] = { 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC };

void volatile_memset(volatile void *p, int ch, std::size_t len)
{
   volatile unsigned char *puch = static_cast<volatile unsigned char *>(p);
   for(std::size_t i = 0; i != len; ++i){
      *puch = (unsigned char)ch;
      ++puch;
   }
}

int volatile_memcmp(const volatile void *p1, const volatile void *p2, std::size_t len)
{
   const volatile unsigned char *s1 = static_cast<const volatile unsigned char *>(p1);
   const volatile unsigned char *s2 = static_cast<const volatile unsigned char *>(p2);
   unsigned char u1, u2;

   for ( ; len-- ; s1++, s2++) {
      u1 = *s1;
      u2 = *s2;
      if (u1 != u2) {
         return (u1-u2);
      }
   }
   return 0;
}

void volatile_memcmp(volatile void *p, int ch, std::size_t len)
{
   volatile unsigned char *puch = static_cast<volatile unsigned char *>(p);
   for(std::size_t i = 0; i != len; ++i){
      *puch = (unsigned char)ch;
      ++puch;
   }
}

#include <iostream>

struct default_init
{
   static void* operator new(std::size_t sz)
   {
      void *const p = ::operator new(sz);
      //Make sure they are not optimized out
      volatile_memset(p, 0xFF, sz);
      std::cout << "0xFF" << '\n';
      return p;
   }
   static void* operator new[](std::size_t sz)
   {
      void *const p = ::operator new[](sz);
      //Make sure they are not optimized out
      volatile_memset(p, 0xEE, sz);
      std::cout << "0xEE" << '\n';
      return p;
   }
   static void* operator new(std::size_t sz, const std::nothrow_t &)
   {
      void *const p = ::operator new(sz);
      //Make sure they are not optimized out
      volatile_memset(p, 0xDD, sz);
      std::cout << "0xDD" << '\n';
      return p;
   }
   static void* operator new[](std::size_t sz, const std::nothrow_t &)
   {
      void *const p = ::operator new[](sz);
      //Make sure they are not optimized out
      volatile_memset(p, 0xCC, sz);
      std::cout << "0xCC" << '\n';
      return p;
   }
   unsigned char buf[PatternSize];
};

namespace bml = ::boost::movelib;

////////////////////////////////
//   make_unique_single
////////////////////////////////

namespace make_unique_single{

void test()
{
   //Single element deleter
   reset_counters();
   {
   bml::unique_ptr<default_init> p(bml::make_unique_definit<default_init>());
   BOOST_TEST(0 == volatile_memcmp(p.get(), ff_patternbuf, sizeof(ff_patternbuf)));
   }
   {
   bml::unique_ptr<default_init> p(bml::make_unique_nothrow_definit<default_init>());
   
   BOOST_TEST(0 == volatile_memcmp(p.get(), dd_patternbuf, sizeof(dd_patternbuf)));
   }

   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A> p(bml::make_unique<A>());
   BOOST_TEST(A::count == 1);
   BOOST_TEST(p->a == 999);
   BOOST_TEST(p->b == 1000);
   BOOST_TEST(p->c == 1001);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A> p(bml::make_unique_nothrow<A>(0));
   BOOST_TEST(A::count == 1);
   BOOST_TEST(p->a == 0);
   BOOST_TEST(p->b == 1000);
   BOOST_TEST(p->c == 1001);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A> p(bml::make_unique<A>(0, 1));
   BOOST_TEST(A::count == 1);
   BOOST_TEST(p->a == 0);
   BOOST_TEST(p->b == 1);
   BOOST_TEST(p->c == 1001);
   }
   BOOST_TEST(A::count == 0);
   {
   bml::unique_ptr<A> p(bml::make_unique_nothrow<A>(0, 1, 2));
   BOOST_TEST(A::count == 1);
   BOOST_TEST(p->a == 0);
   BOOST_TEST(p->b == 1);
   BOOST_TEST(p->c == 2);
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace make_unique_single{


////////////////////////////////
//    make_unique_array
////////////////////////////////

namespace make_unique_array{

void test()
{
   //Array element
   reset_counters();
   {
      bml::unique_ptr<A[]> p(bml::make_unique<A[]>(10));
      BOOST_TEST(A::count == 10u);
      for(std::size_t i = 0; i != 10u; ++i){
         BOOST_TEST(p[i].a == 999);
         BOOST_TEST(p[i].b == 1000);
         BOOST_TEST(p[i].c == 1001);
      }
   }
   BOOST_TEST(A::count == 0);
   {
      bml::unique_ptr<A[]> p(bml::make_unique_nothrow<A[]>(10));
      BOOST_TEST(A::count == 10u);
      for(std::size_t i = 0; i != 10u; ++i){
         BOOST_TEST(p[i].a == 999);
         BOOST_TEST(p[i].b == 1000);
         BOOST_TEST(p[i].c == 1001);
      }
   }
   BOOST_TEST(A::count == 0);
   reset_counters();
   {
      bml::unique_ptr<default_init[]> p(bml::make_unique_definit<default_init[]>(10));
      for(std::size_t i = 0; i != 10u; ++i){
         BOOST_TEST(0 == volatile_memcmp(&p[i], ee_patternbuf, sizeof(ee_patternbuf)));
      }
   }
   reset_counters();
   {
      bml::unique_ptr<default_init[]> p(bml::make_unique_nothrow_definit<default_init[]>(10));
      for(std::size_t i = 0; i != 10u; ++i){
         BOOST_TEST(0 == volatile_memcmp(&p[i], cc_patternbuf, sizeof(cc_patternbuf)));
      }
   }
}

}  //namespace make_unique_array{

////////////////////////////////
//       unique_compare
////////////////////////////////

namespace unique_compare{

void test()
{
   //Single element deleter
   reset_counters();
   {
      bml::unique_ptr<A> pa(bml::make_unique<A>());
      bml::unique_ptr<A> pb(bml::make_unique<A>());
      BOOST_TEST(A::count == 2);

      //Take references to less and greater
      bml::unique_ptr<A> &rpl = pa < pb ? pa : pb;
      bml::unique_ptr<A> &rpg = pa < pb ? pb : pa;

      //Now test operations with .get()

      //Equal
      BOOST_TEST(rpl == rpl && rpl.get() == rpl.get());
      BOOST_TEST(!(rpl == rpg) && !(rpl.get() == rpg.get()));
      //Unequal
      BOOST_TEST(rpl != rpg && rpl.get() != rpg.get());
      BOOST_TEST(!(rpl != rpl) && !(rpl.get() != rpl.get()));
      //Less
      BOOST_TEST(rpl < rpg && rpl.get() < rpg.get());
      BOOST_TEST(!(rpg < rpl) && !(rpg.get() < rpl.get()));
      //Greater
      BOOST_TEST(rpg > rpl && rpg.get() > rpl.get());
      BOOST_TEST(!(rpg > rpg) && !(rpg.get() > rpg.get()));
      //Less or equal
      BOOST_TEST(rpl <= rpg && rpl.get() <= rpg.get());
      BOOST_TEST(rpl <= rpl && rpl.get() <= rpl.get());
      BOOST_TEST(!(rpg <= rpl) && !(rpg.get() <= rpl.get()));
      //Greater or equal
      BOOST_TEST(rpg >= rpl && rpg.get() >= rpl.get());
      BOOST_TEST(rpg >= rpg && rpg.get() >= rpg.get());
      BOOST_TEST(!(rpl >= rpg) && !(rpl.get() >= rpg.get()));
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_compare{

////////////////////////////////
//       unique_compare_zero
////////////////////////////////
namespace unique_compare_zero{

void test()
{
   //Single element deleter
   reset_counters();
   {
      bml::unique_ptr<A> pa(bml::make_unique<A>());
      bml::unique_ptr<A> pb;
      BOOST_TEST(A::count == 1);

      //Equal
      BOOST_TEST(!(pa == 0));
      BOOST_TEST(!(0  == pa));
      BOOST_TEST((pb == 0));
      BOOST_TEST((0  == pb));
      //Unequal
      BOOST_TEST((pa != 0));
      BOOST_TEST((0  != pa));
      BOOST_TEST(!(pb != 0));
      BOOST_TEST(!(0  != pb));
      //Less
      BOOST_TEST((pa < 0) == (pa.get() < (A*)0));
      BOOST_TEST((0 < pa) == ((A*)0 < pa.get()));
      BOOST_TEST((pb < 0) == (pb.get() < (A*)0));
      BOOST_TEST((0 < pb) == ((A*)0 < pb.get()));
      //Greater
      BOOST_TEST((pa > 0) == (pa.get() > (A*)0));
      BOOST_TEST((0 > pa) == ((A*)0 > pa.get()));
      BOOST_TEST((pb > 0) == (pb.get() > (A*)0));
      BOOST_TEST((0 > pb) == ((A*)0 > pb.get()));
      //Less or equal
      BOOST_TEST((pa <= 0) == (pa.get() <= (A*)0));
      BOOST_TEST((0 <= pa) == ((A*)0 <= pa.get()));
      BOOST_TEST((pb <= 0) == (pb.get() <= (A*)0));
      BOOST_TEST((0 <= pb) == ((A*)0 <= pb.get()));
      //Greater or equal
      BOOST_TEST((pa >= 0) == (pa.get() >= (A*)0));
      BOOST_TEST((0 >= pa) == ((A*)0 >= pa.get()));
      BOOST_TEST((pb >= 0) == (pb.get() >= (A*)0));
      BOOST_TEST((0 >= pb) == ((A*)0 >= pb.get()));
   }
   BOOST_TEST(A::count == 0);
}

}  //namespace unique_compare_zero{

////////////////////////////////
//       unique_compare_nullptr
////////////////////////////////

namespace unique_compare_nullptr{

void test()
{
   #if !defined(BOOST_NO_CXX11_NULLPTR)
   //Single element deleter
   reset_counters();
   {
      bml::unique_ptr<A> pa(bml::make_unique<A>());
      bml::unique_ptr<A> pb;
      BOOST_TEST(A::count == 1);

      //Equal
      BOOST_TEST(!(pa == nullptr));
      BOOST_TEST(!(nullptr  == pa));
      BOOST_TEST((pb == nullptr));
      BOOST_TEST((nullptr  == pb));
      //Unequal
      BOOST_TEST((pa != nullptr));
      BOOST_TEST((nullptr  != pa));
      BOOST_TEST(!(pb != nullptr));
      BOOST_TEST(!(nullptr  != pb));
      //Less
      BOOST_TEST((pa < nullptr) == (pa.get() < (A*)nullptr));
      BOOST_TEST((nullptr < pa) == ((A*)nullptr < pa.get()));
      BOOST_TEST((pb < nullptr) == (pb.get() < (A*)nullptr));
      BOOST_TEST((nullptr < pb) == ((A*)nullptr < pb.get()));
      //Greater
      BOOST_TEST((pa > nullptr) == (pa.get() > (A*)nullptr));
      BOOST_TEST((nullptr > pa) == ((A*)nullptr > pa.get()));
      BOOST_TEST((pb > nullptr) == (pb.get() > (A*)nullptr));
      BOOST_TEST((nullptr > pb) == ((A*)nullptr > pb.get()));
      //Less or equal
      BOOST_TEST((pa <= nullptr) == (pa.get() <= (A*)nullptr));
      BOOST_TEST((nullptr <= pa) == ((A*)nullptr <= pa.get()));
      BOOST_TEST((pb <= nullptr) == (pb.get() <= (A*)nullptr));
      BOOST_TEST((nullptr <= pb) == ((A*)nullptr <= pb.get()));
      //Greater or equal
      BOOST_TEST((pa >= nullptr) == (pa.get() >= (A*)nullptr));
      BOOST_TEST((nullptr >= pa) == ((A*)nullptr >= pa.get()));
      BOOST_TEST((pb >= nullptr) == (pb.get() >= (A*)nullptr));
      BOOST_TEST((nullptr >= pb) == ((A*)nullptr >= pb.get()));
   }
   BOOST_TEST(A::count == 0);
   #endif   //#if !defined(BOOST_NO_CXX11_NULLPTR)
}

}  //namespace unique_compare_nullptr{


////////////////////////////////
//             main
////////////////////////////////
int main()
{
   make_unique_single::test();
   make_unique_array::test();
   unique_compare::test();
   unique_compare_zero::test();
   unique_compare_nullptr::test();

   //Test results
   return boost::report_errors();
}
