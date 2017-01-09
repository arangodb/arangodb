//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//User define
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME func0to3
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 0
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 3
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace func0to3ns {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END }
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME func1to2
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 1
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 2
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace func1to2ns {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END }
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME func3to3
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 3
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 3
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace func3to3ns {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END }
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_FUNCNAME func0to0
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MIN 0
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_MAX 0
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_BEG namespace func0to0ns {
#define BOOST_INTRUSIVE_HAS_MEMBER_FUNCTION_CALLABLE_WITH_NS_END }
#include <boost/intrusive/detail/has_member_function_callable_with.hpp>

///////////////////
///////////////////
// TEST CODE
///////////////////
///////////////////

struct functor
{
   //func0to3
   void func0to3();
   void func0to3(const int&);
   void func0to3(const int&, const int&);
   void func0to3(const int&, const int&, const int&);
   //func1to2
   void func1to2(const int&);
   void func1to2(const int&, const int&);
   //func3to3
   void func3to3(const int&, const int&, const int&);
   //func0to0
   void func0to0();
};

struct functor2
{
   void func0to3(char*);
   void func0to3(int, char*);
   void func0to3(int, char*, double);
   void func0to3(const int&, char*, void *);
   //func1to2
   void func1to2(char*);
   void func1to2(int, char*);
   void func1to2(int, char*, double);
   //func3to3
   void func3to3(const int&, char*, void *);
};

struct functor3
{

};

struct functor4
{
   //func0to3
   void func0to3(...);
   template<class T>          void func0to3(int, const T &);
   template<class T>          void func0to3(const T &);
   template<class T, class U> void func0to3(int, const T &, const U &);
   //func1to2
   template<class T>          void func1to2(int, const T &);
   template<class T>          void func1to2(const T &);
   template<class T, class U> void func1to2(int, const T &, const U &);
   //func3to3
   void func3to3(...);
   template<class T, class U> void func3to3(int, const T &, const U &);
};

int main()
{
   #if !defined(BOOST_INTRUSIVE_DETAIL_HAS_MEMBER_FUNCTION_CALLABLE_WITH_0_ARGS_UNSUPPORTED)
   {  //func0to3 0 arg
   using func0to3ns::has_member_function_callable_with_func0to3;
   int check1[ has_member_function_callable_with_func0to3<functor>::value  ? 1 : -1];
   int check2[!has_member_function_callable_with_func0to3<functor2>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func0to3<functor3>::value ? 1 : -1];
   int check4[ has_member_function_callable_with_func0to3<functor4>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   (void)check4;
   }
   {  //func0to0 0 arg
   using func0to0ns::has_member_function_callable_with_func0to0;
   int check1[ has_member_function_callable_with_func0to0<functor>::value  ? 1 : -1];
   int check2[!has_member_function_callable_with_func0to0<functor2>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func0to0<functor3>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   }
   #endif

   {  //func0to3 1arg
   using func0to3ns::has_member_function_callable_with_func0to3;
   int check1[ has_member_function_callable_with_func0to3<functor,  int>::value   ? 1 : -1];
   int check2[!has_member_function_callable_with_func0to3<functor,  char*>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func0to3<functor2, int>::value   ? 1 : -1];
   int check4[ has_member_function_callable_with_func0to3<functor2, char*>::value ? 1 : -1];
   int check5[!has_member_function_callable_with_func0to3<functor3, int>::value   ? 1 : -1];
   int check6[!has_member_function_callable_with_func0to3<functor3, char*>::value ? 1 : -1];
   int check7[ has_member_function_callable_with_func0to3<functor4, int>::value ? 1 : -1];
   int check8[ has_member_function_callable_with_func0to3<functor4, char*>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   (void)check4;
   (void)check5;
   (void)check6;
   (void)check7;
   (void)check8;
   }

   {  //func1to2 1arg
   using func1to2ns::has_member_function_callable_with_func1to2;
   int check1[ has_member_function_callable_with_func1to2<functor,  int>::value   ? 1 : -1];
   int check2[!has_member_function_callable_with_func1to2<functor,  char*>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func1to2<functor2, int>::value   ? 1 : -1];
   int check4[ has_member_function_callable_with_func1to2<functor2, char*>::value ? 1 : -1];
   int check5[!has_member_function_callable_with_func1to2<functor3, int>::value   ? 1 : -1];
   int check6[!has_member_function_callable_with_func1to2<functor3, char*>::value ? 1 : -1];
   int check7[ has_member_function_callable_with_func1to2<functor4, int>::value ? 1 : -1];
   int check8[ has_member_function_callable_with_func1to2<functor4, char*>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   (void)check4;
   (void)check5;
   (void)check6;
   (void)check7;
   (void)check8;
   }

   {  //func0to3 2arg
   using func0to3ns::has_member_function_callable_with_func0to3;
   int check1[ has_member_function_callable_with_func0to3<functor,  int, int>::value   ? 1 : -1];
   int check2[!has_member_function_callable_with_func0to3<functor,  int, char*>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func0to3<functor2, int, int>::value   ? 1 : -1];
   int check4[ has_member_function_callable_with_func0to3<functor2, int, char*>::value ? 1 : -1];
   int check5[!has_member_function_callable_with_func0to3<functor3, int, int>::value   ? 1 : -1];
   int check6[!has_member_function_callable_with_func0to3<functor3, int, char*>::value ? 1 : -1];
   int check7[ has_member_function_callable_with_func0to3<functor4, int, char*>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   (void)check4;
   (void)check5;
   (void)check6;
   (void)check7;
   }

   {  //func1to2 2arg
   using func1to2ns::has_member_function_callable_with_func1to2;
   int check1[ has_member_function_callable_with_func1to2<functor,  int, int>::value   ? 1 : -1];
   int check2[!has_member_function_callable_with_func1to2<functor,  int, char*>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func1to2<functor2, int, int>::value   ? 1 : -1];
   int check4[ has_member_function_callable_with_func1to2<functor2, int, char*>::value ? 1 : -1];
   int check5[!has_member_function_callable_with_func1to2<functor3, int, int>::value   ? 1 : -1];
   int check6[!has_member_function_callable_with_func1to2<functor3, int, char*>::value ? 1 : -1];
   int check7[ has_member_function_callable_with_func1to2<functor4, int, char*>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   (void)check4;
   (void)check5;
   (void)check6;
   (void)check7;
   }

   {  //func0to3 3arg
   using func0to3ns::has_member_function_callable_with_func0to3;
   int check1[ has_member_function_callable_with_func0to3<functor,  int, int, int>::value   ? 1 : -1];
   int check2[!has_member_function_callable_with_func0to3<functor,  int, char*, int>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func0to3<functor2, int, int, int>::value   ? 1 : -1];
   int check4[ has_member_function_callable_with_func0to3<functor2, int, char*, void*>::value ? 1 : -1];
   int check5[!has_member_function_callable_with_func0to3<functor3, int, int, int>::value   ? 1 : -1];
   int check6[!has_member_function_callable_with_func0to3<functor3, int, char*, void*>::value ? 1 : -1];
   int check7[ has_member_function_callable_with_func0to3<functor4, int, char*, int>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   (void)check4;
   (void)check5;
   (void)check6;
   (void)check7;
   }

   {  //func3to3 3arg
   using func3to3ns::has_member_function_callable_with_func3to3;
   int check1[ has_member_function_callable_with_func3to3<functor,  int, int, int>::value   ? 1 : -1];
   int check2[!has_member_function_callable_with_func3to3<functor,  int, char*, int>::value ? 1 : -1];
   int check3[!has_member_function_callable_with_func3to3<functor2, int, int, int>::value   ? 1 : -1];
   int check4[ has_member_function_callable_with_func3to3<functor2, int, char*, void*>::value ? 1 : -1];
   int check5[!has_member_function_callable_with_func3to3<functor3, int, int, int>::value   ? 1 : -1];
   int check6[!has_member_function_callable_with_func3to3<functor3, int, char*, void*>::value ? 1 : -1];
   int check7[ has_member_function_callable_with_func3to3<functor4, int, char*, int>::value ? 1 : -1];
   (void)check1;
   (void)check2;
   (void)check3;
   (void)check4;
   (void)check5;
   (void)check6;
   (void)check7;
   }

   return 0;
}
