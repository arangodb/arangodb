// Copyright 2016 Klemens Morgenstern
// Copyright 2017-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/config.hpp>

#if (__cplusplus >= 201402L) || (defined(_MSVC_LANG) && _MSVC_LANG >= 201402L)

#include <boost/dll/config.hpp>
#include <boost/variant.hpp>

BOOST_SYMBOL_EXPORT extern int unscoped_var;
int unscoped_var = 42;

BOOST_SYMBOL_EXPORT extern const double unscoped_c_var;
const double unscoped_c_var = 1.234;


namespace some_space {

BOOST_SYMBOL_EXPORT extern double variable;
double variable =  0.2;
BOOST_SYMBOL_EXPORT const int & scoped_fun()
{
    static int x = 0xDEADBEEF;
    return x;
}

}


BOOST_SYMBOL_EXPORT void overloaded(const volatile int i)
{
    unscoped_var = i;
}

BOOST_SYMBOL_EXPORT void overloaded(const double d)
{
    some_space::variable = d;
}

BOOST_SYMBOL_EXPORT void use_variant(boost::variant<int, double> & v)
{
    v = 42;
}

BOOST_SYMBOL_EXPORT void use_variant(boost::variant<double, int> & v)
{
    v = 3.124;
}



namespace some_space
{

BOOST_SYMBOL_EXPORT extern int father_value;
int father_value = 12;

struct BOOST_SYMBOL_EXPORT some_father
{
    some_father() {  father_value = 24; };
    ~some_father() { father_value = 112; };
};



struct BOOST_SYMBOL_EXPORT some_class : some_father
{
    static int value ;
    static void set_value(const int &i);

  //  static some_class* dummy();

    virtual double  func(double i, double j);
    virtual int     func(int i,     int j);
    int func(int i,     int j)             volatile;
    double func(double i, double j) const volatile;

    int mem_val;
    int  get() const ;
    void set(int i)  ;

    some_class();
    some_class(some_class &&);
    some_class(int i);

    some_class& operator=(some_class &&);


    virtual ~some_class();
};

some_class::some_class(some_class &&){}


some_class& some_class::operator=(some_class &&ref) {return ref;}


BOOST_SYMBOL_EXPORT extern std::size_t size_of_some_class;
std::size_t size_of_some_class = sizeof(some_space::some_class);


extern "C"  BOOST_SYMBOL_EXPORT const volatile some_class* this_;
const volatile some_class * this_ = nullptr;

int some_class::value = -1;

void some_class::set_value(const int &i) {value = i;}


//some_class* some_class::dummy() {return new some_class();}//so it implements an allocating ctor.

double  some_class::func(double i, double j)                {this_ = this; return i*j;}
int     some_class::func(int i,     int j)                  {this_ = this; return i+j;}
int     some_class::func(int i,     int j)   volatile       {this_ = this; return i-j;;}
double  some_class::func(double i, double j) const volatile {this_ = this; return i/j;}

int  some_class::get() const {this_ = this; return mem_val;}
void some_class::set(int i)  {this_ = this; mem_val = i;}

some_class::some_class()        { this_ = this; value = 23; mem_val = 123;}
some_class::some_class(int i) : mem_val(456) {this_ = this; value = i;}

some_class::~some_class()
{
    value = 0;
    this_ = this;
}

}

namespace space {
  class BOOST_SYMBOL_EXPORT my_plugin;
}

namespace testing { namespace space {
  class BOOST_SYMBOL_EXPORT my_plugin {
  public:
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int Func() const;
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int Func();
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int Func2();
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int AFunc();
  };

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::Func() const { return 30; }

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::Func() { return 32; }

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::Func2() { return 33; }

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::AFunc() { return 31; }

  template BOOST_SYMBOL_EXPORT int my_plugin::Func<::space::my_plugin>();
  template BOOST_SYMBOL_EXPORT int my_plugin::Func2<::space::my_plugin>();
  template BOOST_SYMBOL_EXPORT int my_plugin::AFunc<::space::my_plugin>();
}}

namespace space {
  class BOOST_SYMBOL_EXPORT my_plugin {
  public:
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int Func() const;
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int Func();
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int Func2();
    template<typename Arg>
    BOOST_SYMBOL_EXPORT int AFunc();
  };

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::Func() const { return 40; }

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::Func() { return 42; }

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::Func2() { return 43; }

  template<typename Arg>
  BOOST_SYMBOL_EXPORT int my_plugin::AFunc() { return 41; }

  template BOOST_SYMBOL_EXPORT int my_plugin::Func<::space::my_plugin>();
  template BOOST_SYMBOL_EXPORT int my_plugin::Func2<::space::my_plugin>();
  template BOOST_SYMBOL_EXPORT int my_plugin::AFunc<::space::my_plugin>();
}


#endif
