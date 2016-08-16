// Copyright 2016 Klemens Morgenstern
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org


#include <boost/config.hpp>
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
    some_class(int i);

    virtual ~some_class();

};

int some_class::value = -1;

void some_class::set_value(const int &i) {value = i;}


//some_class* some_class::dummy() {return new some_class();}//so it implements an allocating ctor.

double  some_class::func(double i, double j) {return i*j;}
int     some_class::func(int i,     int j)      {return i+j;}
int     some_class::func(int i,     int j)             volatile {return i-j;;}
double  some_class::func(double i, double j) const volatile {return i/j;}

int  some_class::get() const {return mem_val;}
void some_class::set(int i)  {mem_val = i;}

some_class::some_class()        {value = 23; mem_val = 123;}
some_class::some_class(int i) : mem_val(456) {value = i;}

some_class::~some_class()
{
    value = 0;
}

}
