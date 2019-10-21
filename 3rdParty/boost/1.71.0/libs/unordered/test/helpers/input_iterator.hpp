
// Copyright 2005-2010 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(BOOST_UNORDERED_TEST_HELPERS_INPUT_ITERATOR_HEADER)
#define BOOST_UNORDERED_TEST_HELPERS_INPUT_ITERATOR_HEADER

#include <boost/config.hpp>
#include <iterator>

namespace test {
  template <class Iterator> struct proxy
  {
    typedef typename Iterator::value_type value_type;

    explicit proxy(value_type const& v) : v_(v) {}
    proxy(proxy const& x) : v_(x.v_) {}
    operator value_type const&() const { return v_; }

    value_type v_;

  private:
    proxy& operator=(proxy const&);
  };

  template <class Iterator> struct input_iterator_adaptor
  {
    typedef typename std::iterator_traits<Iterator>::value_type value_type;
    typedef typename std::iterator_traits<Iterator>::pointer pointer;
    typedef proxy<Iterator> reference;
    typedef std::ptrdiff_t difference_type;
    typedef std::input_iterator_tag iterator_category;

    input_iterator_adaptor() : base_() {}
    explicit input_iterator_adaptor(Iterator& it) : base_(&it) {}
    proxy<Iterator> operator*() const { return proxy<Iterator>(**base_); }
    value_type* operator->() const { return &**base_; }
    input_iterator_adaptor& operator++()
    {
      ++*base_;
      return *this;
    }
    // input_iterator_adaptor operator++(int) {
    //}
    bool operator==(input_iterator_adaptor const& x) const
    {
      return *base_ == *x.base_;
    }
    bool operator!=(input_iterator_adaptor const& x) const
    {
      return *base_ != *x.base_;
    }

  private:
    Iterator* base_;
  };

  template <class Iterator>
  input_iterator_adaptor<Iterator> input_iterator(Iterator& it)
  {
    return input_iterator_adaptor<Iterator>(it);
  }

  template <class Iterator> struct copy_iterator_adaptor
  {
    typedef typename std::iterator_traits<Iterator>::value_type value_type;
    typedef
      typename std::iterator_traits<Iterator>::difference_type difference_type;
    typedef typename std::iterator_traits<Iterator>::iterator_category
      iterator_category;
    typedef typename std::iterator_traits<Iterator>::pointer pointer;
    typedef proxy<Iterator> reference;

    copy_iterator_adaptor() : base_() {}
    explicit copy_iterator_adaptor(Iterator const& it) : base_(it) {}
    value_type operator*() const { return *base_; }
    value_type* operator->() const { return &*base_; }
    value_type operator[](difference_type d) { return base_[d]; }
    copy_iterator_adaptor& operator++()
    {
      ++base_;
      return *this;
    }
    copy_iterator_adaptor operator++(int)
    {
      copy_iterator_adaptor tmp(*this);
      ++base_;
      return tmp;
    }
    copy_iterator_adaptor& operator--()
    {
      --base_;
      return *this;
    }
    copy_iterator_adaptor operator--(int)
    {
      copy_iterator_adaptor tmp(*this);
      --base_;
      return tmp;
    }
    copy_iterator_adaptor operator+=(difference_type x)
    {
      base_ += x;
      return *this;
    }
    copy_iterator_adaptor operator-=(difference_type x)
    {
      base_ -= x;
      return *this;
    }
    copy_iterator_adaptor operator+(difference_type n)
    {
      return copy_iterator_adaptor(base_ + n);
    }
    copy_iterator_adaptor operator-(difference_type n)
    {
      return copy_iterator_adaptor(base_ - n);
    }
    friend copy_iterator_adaptor operator+(
      difference_type n, copy_iterator_adaptor x)
    {
      return x + n;
    }
    difference_type operator-(copy_iterator_adaptor const& other)
    {
      return base_ - other.base_;
    }
    bool operator==(copy_iterator_adaptor const& x) const
    {
      return base_ == x.base_;
    }
    bool operator!=(copy_iterator_adaptor const& x) const
    {
      return base_ != x.base_;
    }
    bool operator<(copy_iterator_adaptor const& x) const
    {
      return base_ < x.base_;
    }
    bool operator>(copy_iterator_adaptor const& x) const
    {
      return base_ > x.base_;
    }
    bool operator<=(copy_iterator_adaptor const& x) const
    {
      return base_ <= x.base_;
    }
    bool operator>=(copy_iterator_adaptor const& x) const
    {
      return base_ >= x.base_;
    }

  private:
    Iterator base_;
  };

  template <class Iterator>
  copy_iterator_adaptor<Iterator> copy_iterator(Iterator const& it)
  {
    return copy_iterator_adaptor<Iterator>(it);
  }
}

#endif
