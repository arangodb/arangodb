//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Howard Hinnant 2014.
// (C) Copyright Ion Gaztanaga 2014-2014. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/container for documentation.
//
//////////////////////////////////////////////////////////////////////////////
//
// This testcase is based on H. Hinnant's article "Insert vs. Emplace",
// (http://github.com/HowardHinnant/papers/blob/master/insert_vs_emplace.html)
//
//////////////////////////////////////////////////////////////////////////////
#include <iostream>
#include <boost/move/utility_core.hpp>
#include <boost/container/vector.hpp>
#include <boost/core/lightweight_test.hpp>

class X
{
   int i_;
   int* p_;

   BOOST_COPYABLE_AND_MOVABLE(X)

public:
   struct special
   {
      unsigned c;
      unsigned dt;
      unsigned cc;
      unsigned ca;
      unsigned mc;
      unsigned ma;

      friend bool operator==(const special &l, const special &r)
      {  return l.c == r.c && l.dt == r.dt && l.cc == r.cc && l.ca == r.ca && l.mc == r.mc && l.ma == r.ma;  }
   };
   static special sp;

   X(int i, int* p)
      : i_(i)
      , p_(p)
   {
//         std::cout << "X(int i, int* p)\n";
      sp.c++;
   }

   ~X()
   {
//         std::cout << "~X()\n";
      sp.dt++;
   }

   X(const X& x)
      : i_(x.i_)
      , p_(x.p_)
   {
//         std::cout << "X(const X& x)\n";
      sp.cc++;
   }

   X& operator=(BOOST_COPY_ASSIGN_REF(X) x)
   {

      i_ = x.i_;
      p_ = x.p_;
//         std::cout << "X& operator=(const X& x)\n";
      sp.ca++;
      return *this;
   }

   X(BOOST_RV_REF(X) x) BOOST_NOEXCEPT_OR_NOTHROW
      : i_(x.i_)
      , p_(x.p_)
   {
//         std::cout << "X(X&& x)\n";
      sp.mc++;
   }

   X& operator=(BOOST_RV_REF(X) x) BOOST_NOEXCEPT_OR_NOTHROW
   {

      i_ = x.i_;
      p_ = x.p_;
//         std::cout << "X& operator=(X&& x)\n";
      sp.ma++;
      return *this;
   }

};

std::ostream&
operator<<(std::ostream& os, X::special const& sp)
{
   os << "Const: " << sp.c << '\n';
   os << "Destr: " << sp.dt << '\n';
   os << "CopyC: " << sp.cc << '\n';
   os << "CopyA: " << sp.ca << '\n';
   os << "MoveC: " << sp.mc << '\n';
   os << "MoveA: " << sp.ma << '\n';
   os << "Total: " << (sp.c + sp.dt + sp.cc + sp.ca + sp.mc + sp.ma) << '\n';
   return os;
}

X::special X::sp = X::special();

const X produce_const_prvalue()
{   return X(0, 0);   }

int main()
{
   X::special insert_results;
   X::special emplace_results;
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--insert lvalue no reallocation--\n";
      X::sp = X::special();
      v.insert(v.begin(), x);
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace lvalue no reallocation--\n";
      X::sp = X::special();
      v.emplace(v.begin(), x);
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--insert xvalue no reallocation--\n";
      X::sp = X::special();
      v.insert(v.begin(), boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace xvalue no reallocation--\n";
      X::sp = X::special();
      v.emplace(v.begin(), boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace const prvalue no reallocation--\n";
      X::sp = X::special();
      v.emplace(v.begin(), produce_const_prvalue());
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace const prvalue no reallocation--\n";
      X::sp = X::special();
      v.insert(v.begin(), produce_const_prvalue());
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--insert rvalue no reallocation--\n";
      X::sp = X::special();
      v.insert(v.begin(), X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--emplace rvalue no reallocation--\n";
      X::sp = X::special();
      v.emplace(v.begin(), X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   //With emulated move semantics an extra copy is unavoidable
   #if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   #endif
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--insert lvalue reallocation--\n";
      X::sp = X::special();
      v.insert(v.begin(), x);
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace lvalue reallocation--\n";
      X::sp = X::special();
      v.emplace(v.begin(), x);
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--insert xvalue reallocation--\n";
      X::sp = X::special();
      v.insert(v.begin(), boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace xvalue reallocation--\n";
      X::sp = X::special();
      v.emplace(v.begin(), boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--insert rvalue reallocation--\n";
      X::sp = X::special();
      v.insert(v.begin(), X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--emplace rvalue reallocation--\n";
      X::sp = X::special();
      v.emplace(v.begin(), X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   //With emulated move semantics an extra copy is unavoidable
   #if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   #endif
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--push_back lvalue no reallocation--\n";
      X::sp = X::special();
      v.push_back(x);
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace_back lvalue no reallocation--\n";
      X::sp = X::special();
      v.emplace_back(x);
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--push_back xvalue no reallocation--\n";
      X::sp = X::special();
      v.push_back(boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace_back xvalue no reallocation--\n";
      X::sp = X::special();
      v.emplace_back(boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--push_back rvalue no reallocation--\n";
      X::sp = X::special();
      v.push_back(X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(4);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--emplace_back rvalue no reallocation--\n";
      X::sp = X::special();
      v.emplace_back(X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   //With emulated move semantics an extra copy is unavoidable
   #if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   #endif
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--push_back lvalue reallocation--\n";
      X::sp = X::special();
      v.push_back(x);
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace_back lvalue reallocation--\n";
      X::sp = X::special();
      v.emplace_back(x);
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--push_back xvalue reallocation--\n";
      X::sp = X::special();
      v.push_back(boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      X x(0,0);
      std::cout << "--emplace_back xvalue reallocation--\n";
      X::sp = X::special();
      v.emplace_back(boost::move(x));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--push_back rvalue reallocation--\n";
      X::sp = X::special();
      v.push_back(X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      insert_results = X::sp;
   }
   {
      boost::container::vector<X> v;
      v.reserve(3);
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      v.push_back(X(0,0));
      std::cout << "--emplace_back rvalue reallocation--\n";
      X::sp = X::special();
      v.emplace_back(X(0,0));
      std::cout << X::sp;
      std::cout << "----\n";
      emplace_results = X::sp;
   }
   //With emulated move semantics an extra copy is unavoidable
   #if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
   BOOST_TEST_EQ(insert_results == emplace_results, true);
   #endif
   return boost::report_errors();
}
