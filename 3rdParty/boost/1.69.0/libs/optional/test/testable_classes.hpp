// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/optional for documentation.
//
// You are welcome to contact the author at:
//  akrzemi1@gmail.com

#ifndef BOOST_OPTIONAL_TEST_TESTABKE_CLASSES_AK_07JAN2015_HPP
#define BOOST_OPTIONAL_TEST_TESTABKE_CLASSES_AK_07JAN2015_HPP

#include "boost/optional/optional.hpp"

struct ScopeGuard // no copy/move ctor/assign
{
  int val_;
  explicit ScopeGuard(int v) : val_(v) {}
  int& val() { return val_; }
  const int& val() const { return val_; }
  
private:
  ScopeGuard(ScopeGuard const&);
  void operator=(ScopeGuard const&);
};

struct Abstract
{
  virtual int& val() = 0;
  virtual const int& val() const = 0;
  virtual ~Abstract() {}
  Abstract(){}
  
private:
  Abstract(Abstract const&);
  void operator=(Abstract const&);
};

struct Impl : Abstract
{
  int val_;
  Impl(int v) : val_(v) {}
  int& val() { return val_; }
  const int& val() const { return val_; }
};

template <typename T>
struct concrete_type_of
{
  typedef T type;
};

template <>
struct concrete_type_of<Abstract>
{
  typedef Impl type;
};

template <>
struct concrete_type_of<const Abstract>
{
  typedef const Impl type;
};

template <typename T>
struct has_arrow
{
  static const bool value = true;
};

template <>
struct has_arrow<int>
{
  static const bool value = false;
};

template <>
struct has_arrow< boost::optional<int> >
{
  static const bool value = false;
};

int& val(int& i) { return i; }
int& val(Abstract& a) { return a.val(); }
int& val(Impl& a) { return a.val(); }
int& val(ScopeGuard& g) { return g.val(); }
template <typename T> int& val(T& o) { return *o; }

const int& val(const int& i) { return i; }
const int& val(const Abstract& a) { return a.val(); }
const int& val(const Impl& a) { return a.val(); }
const int& val(const ScopeGuard& g) { return g.val(); }
template <typename T> const int& val(const T& o) { return *o; }

bool operator==(const Abstract& l, const Abstract& r) { return l.val() == r.val(); }
bool operator==(const ScopeGuard& l, const ScopeGuard& r) { return l.val() == r.val(); }

bool operator<(const Abstract& l, const Abstract& r) { return l.val() < r.val(); }
bool operator<(const ScopeGuard& l, const ScopeGuard& r) { return l.val() < r.val(); }

#endif //BOOST_OPTIONAL_TEST_TESTABKE_CLASSES_AK_07JAN2015_HPP
