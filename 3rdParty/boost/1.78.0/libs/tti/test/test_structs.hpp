
//  (C) Copyright Edward Diener 2011,2012,2019
//  Use, modification and distribution are subject to the Boost Software License,
//  Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt).

#if !defined(TEST_STRUCTS_HPP)
#define TEST_STRUCTS_HPP

#include <boost/config.hpp>

struct AType 
  {
  
  // Type
  
  typedef int AnIntType;
  struct AStructType
    {
    template <class> struct MStrMemberTemplate { };
    template<class X,class Y,short AA> static int StatFuncTemplate(X *,Y) { int ret(AA); return ret; }
    };
  typedef int & AnIntTypeReference;
  struct BType
    {
    typedef int AnIntegerType;
    enum BTypeEnum
        {
        BTypeEnum1,
        BTypeEnum2
        };
    struct CType
      {
      typedef int AnotherIntegerType;
      template <class,class,int,short,class,template <class,int> class,class> struct CTManyParameters { };
      template<class X,class Y,class Z,short AA> double SomeFuncTemplate(X,Y *,Z &) { double ret(AA); return ret; }
      union CTypeUnion
        {
        long l;
        bool b;
        void CMemberFunction(int) { }
        };
      };
    };
    
  // Enum
  
  enum AnEnumTtype
    {
    AnEnumTtype1,
    AnEnumTtype2
    };
    
#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)
  
  enum class AnEnumClassType
    {
    AnEnumClassType1,
    AnEnumClassType2
    };
  
#endif
  
  // Union
  
  union AnUnion
    {
    int i;
    double d;
    struct UNStruct
        {
        short sh;
        };
    enum UEnumV
        {
        UEnumV1,
        UEnumV2
        };
    template <int,class,long> struct NestedMemberTemplate { };
    
#if !defined(BOOST_NO_CXX11_UNRESTRICTED_UNION)

    static float USMember;
    
#endif

    };

  // Template
    
  template <class> struct ATPMemberTemplate { };
  template <int> struct AMemberTemplate { };
  template <int,int> struct AnotherMemberTemplate { };
  template <class,class> struct CLMemberTemplate { };
  
  // Data
  
  int AnInt;
  BType IntBT;
  BType::CType NestedCT;
  
  // Function
  
  void VoidFunction() { }
  int IntFunction() { return 0; }
  
  // Const function
  
  double AConstFunction(long, char) const { return 2.57; }
  void WFunction(float, double = 4.3) const { }
  
  // Volatile function
  
  double AVolatileFunction(long, char = 'c') volatile { return 2.58; }
  void VolFunction(float, double) volatile { }
  
  // Const Volatile function
  
  double ACVFunction(long, char) const volatile { return 2.59; }
  void ConstVolFunction(float, double) const volatile { }
  
  // Function Templates
  
  template<class X,int Y> int AFuncTemplate(const X &) { return Y; }
  template<class X,class Y,class Z> void AFuncTemplate(X *,Y,Z &) { }
  template<class X> double FTHasDef(X, long, char = 'a') { return 3.58; }
  
  // Const function template
  
  template<class X,class Y> double AConstFunctionTemplate(X, Y y) const { return (static_cast<double>(y)); }
  template<class X,class Y,class Z> void WFunctionTmp(X **, Y &, Z) const { }
  
  // Volatile function template
  
  template<class X> double AVolatileFT(X, long, char) volatile { return 2.58; }
  template<class X,int Y> void VolFTem(X &) volatile { }
  
  // Const Volatile function template
  
  template<class X,class Y> double ACVFunTemplate(X, Y) const volatile { return 2.59; }
  template<int Y> void ConstVolTTFun(float, double) const volatile { }
  
  // Static Data
  
  static short DSMember;
  
  // Static Function
  
  static int SIntFunction(long,double) { return 2; }
  
  // Static Function Template
  
  template<class X,class Y,class Z> static void AnotherFuncTemplate(X,Y &,const Z &) { }
  
  };
  
struct AnotherType
  {
  
  // Type
  
  typedef AType::AnIntType someOtherType;
    
  // Enum
  
  enum AnotherEnumTtype
    {
    AnotherEnumType1,
    AnotherEnumType2
    };
    
#if !defined(BOOST_NO_CXX11_SCOPED_ENUMS)
  
  enum class AnotherEnumClassType
    {
    AnotherEnumClassType1,
    AnotherEnumClassType2
    };
  
#endif
  
  // Union
  
  union AnotherUnion
    {
    short s;
    char ch;
    template <class,class> struct UnionMemberTemplate { };
    static void UnionStaticMemberFunction() { }
    union InnerUnion
        {
        int i;
        char ch;
        };
        
#if !defined(BOOST_NO_CXX11_UNRESTRICTED_UNION)

    static char ASCData;
    
#endif

    };
    
  // Template
  
  template <class,class,class,class,class,class> struct SomeMemberTemplate { };
  template <class,class,int,class,template <class> class,class,long> struct ManyParameters { };
  template <class,class,class,class> struct SimpleTMP { };
  
  // Data
  
  bool aMember;
  bool cMem;
  long AnInt;
  AType OtherAT;
  AType::AStructType ONestStr;
  
  // Function
  
  AType aFunction(int = 7) { return AType(); }
  int anotherFunction(AType) { return 0; }
  AType::AnIntType sFunction(int,long = 88,double = 1.0) { return 0; }
  double IntFunction(int = 9922) { return 0; }
  
  // Const function
  
  int AnotherConstFunction(AType *, short) const { return 0; }
  AType StillSame(int) const { return OtherAT; }
  
  // Volatile function
  
  int AnotherVolatileFunction(AType *, short) volatile { return 0; }
  bool StillVolatile(int) volatile { return false; }
  
  // Const Volatile function
  
  int AnotherConstVolatileFunction(AType *, short) const volatile { return 0; }
  short StillCV(int = 3) const volatile { return 32; }
  
  // Function Templates
  
  template<class X> long MyFuncTemplate(X &) { return 0; }
  template<int Y> void VWithDefault(float, double d = 2.67) volatile { }
  
  // Static Function
  
  static AType TIntFunction(long,double = 3.0) { return AType(); }
  static AType::AStructType TSFunction(AType::AnIntType,double) { return AType::AStructType(); }
  
  // Static Data
  
  static AType::AStructType AnStat;
  
  // Static Function Template
  
  template<class X,class Y> static void YetAnotherFuncTemplate(const X &,Y &) { }
  template<class X,class Y,class Z> static void StaticFTWithDefault(const X &,Y &,Z*,long = 27) { }
  
  static const int CIntValue = 10;
  
  };
  
struct MarkerType
  {
  };

short AType::DSMember(5622);
AType::AStructType AnotherType::AnStat;

#if !defined(BOOST_NO_CXX11_UNRESTRICTED_UNION)

float AType::AnUnion::USMember(893.53f);
char AnotherType::AnotherUnion::ASCData('e');

#endif

#endif // TEST_STRUCTS_HPP
