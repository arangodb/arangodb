//-----------------------------------------------------------------------------
// boost-libs variant/test/variant_visit_internal_linkage.cpp header file
// See http://www.boost.org for updates, documentation, and revision history.
//-----------------------------------------------------------------------------
//
// Copyright (c) 2018 Louis Dionne, Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// This test checks that we can visit a variant containing a type that has
// internal linkage (anonymous namespace).

#include "boost/variant/variant.hpp"

#ifdef BOOST_NO_CXX14_DECLTYPE_AUTO

void run() {}

#else

namespace {
   struct Foo { };

   struct Visitor {
      void operator()(Foo const&) const { }
   };
}

void run() {
   boost::variant<Foo> v = Foo();
   boost::apply_visitor(Visitor(), v);
}

#endif

int main() {
   run();
}

