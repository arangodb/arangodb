//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_ITERATOR_H
#define IRESEARCH_ITERATOR_H

#include "noncopyable.hpp"

#include <memory>
#include <cassert>
#include <boost/iterator/iterator_facade.hpp>

NS_ROOT

// ----------------------------------------------------------------------------
// --SECTION--                                             Java style iterators 
// ----------------------------------------------------------------------------

template< typename T >
struct IRESEARCH_API_TEMPLATE iterator {
  DECLARE_PTR(iterator<T>);
  DECLARE_FACTORY(iterator<T>);

  virtual ~iterator() {}
  virtual T value() const = 0;
  virtual bool next() = 0;
};

template<typename Iterator, typename Base>
class iterator_adapter : public Base {
 public:
  typedef decltype(*Iterator()) value_type;

  iterator_adapter(Iterator begin, Iterator end)
    : begin_(begin), end_(end) {
  }

  virtual const value_type& value() const {
    return *begin_;
  }

  virtual bool next() {
    if (begin_ == end_) {
      return false;
    }

    ++begin_;
    return true;
  }

 private:
  Iterator begin_;
  Iterator end_;
}; // field_iterator

// ----------------------------------------------------------------------------
// --SECTION--                                              C++ style iterators 
// ----------------------------------------------------------------------------

template< typename _T >
struct forward_iterator_impl {
  typedef _T value_type;
  typedef _T& reference;
  typedef const _T& const_reference;

  virtual ~forward_iterator_impl() {}
  virtual void operator++( ) = 0;
  virtual reference operator*() = 0;
  virtual const_reference operator*() const = 0;
  virtual bool operator==( const forward_iterator_impl& ) = 0;
};

template< typename _IteratorImpl >
class forward_iterator 
  : private util::noncopyable,
    public ::boost::iterator_facade<forward_iterator<_IteratorImpl>,
            typename _IteratorImpl::value_type, 
            ::boost::forward_traversal_tag> {
 public:
  typedef _IteratorImpl iterator_impl;
  typedef typename iterator_impl::value_type value_type;
  typedef typename iterator_impl::reference reference;

  explicit forward_iterator(iterator_impl* impl = nullptr) : impl_(impl) {}

  forward_iterator(forward_iterator&& rhs) NOEXCEPT
    : impl_(rhs.impl_) {
    rhs.impl_ = nullptr;
  }

  ~forward_iterator() { delete impl_; }

  forward_iterator& operator=(forward_iterator&& rhs) NOEXCEPT {
    if (this != &rhs) {
      impl_ = rhs.impl_;
      rhs.impl_ = nullptr;
    }

    return *this;
  }

private:
  friend class ::boost::iterator_core_access;

  reference dereference() const { return **impl_; }
  void increment() { ++(*impl_); }
  bool equal(const forward_iterator& rhs) const {
    return (!impl_ && !rhs.impl_) || (impl_ && rhs.impl_ && *impl_ == *rhs.impl_);
  }

  iterator_impl* impl_;
};

NS_BEGIN(detail)

template<typename SmartPtr>
struct extract_element_type {
  typedef typename SmartPtr::element_type value_type;
  typedef typename SmartPtr::element_type& reference;
  typedef typename SmartPtr::element_type* pointer;
};

template<typename SmartPtr>
struct extract_element_type<const SmartPtr> {
  typedef const typename SmartPtr::element_type value_type;
  typedef const typename SmartPtr::element_type& reference;
  typedef const typename SmartPtr::element_type* pointer;
};

template<typename In, typename Out>
struct adjust_const {
  typedef Out value_type;
  typedef Out& reference;
  typedef Out* pointer;
};

template<typename In, typename Out>
struct adjust_const<const In, Out> {
  typedef const Out value_type;
  typedef const Out& reference;
  typedef const Out* pointer;
};

NS_END

//////////////////////////////////////////////////////////////////////////////
/// @class const_ptr_iterator
/// @brief iterator adapter for containers with the smart pointers
//////////////////////////////////////////////////////////////////////////////
template<typename IteratorImpl>
class ptr_iterator 
  : public ::boost::iterator_facade<
     ptr_iterator<IteratorImpl>,
     typename detail::extract_element_type<typename std::remove_reference<
       typename IteratorImpl::reference>::type
     >::value_type,
     ::boost::random_access_traversal_tag > {
 private:
  typedef detail::extract_element_type<
    typename std::remove_reference<typename IteratorImpl::reference>::type
  > element_type;

  typedef ::boost::iterator_facade<
     ptr_iterator<IteratorImpl>,
     typename detail::extract_element_type<typename std::remove_reference<
       typename IteratorImpl::reference>::type
     >::value_type,
     ::boost::random_access_traversal_tag > base;

  typedef typename base::iterator_facade_ iterator_facade;

  typedef typename element_type::value_type base_element_type;
  
  template<typename T>
  struct adjust_const 
    : detail::adjust_const<typename element_type::value_type, T> { };
  
 public:
  typedef typename iterator_facade::reference reference;
  typedef typename iterator_facade::difference_type difference_type;

  // -----------------------------------------------------------------------------
  // --SECTION--                                      constructors and destructors
  // -----------------------------------------------------------------------------

  ptr_iterator( const IteratorImpl& it ) : it_( it ) {}

  // -----------------------------------------------------------------------------
  // --SECTION--                                                   cast operations
  // -----------------------------------------------------------------------------

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns downcasted reference to the iterator's value
  //////////////////////////////////////////////////////////////////////////////
  template<typename T>
  typename adjust_const<T>::reference as() const {
    typedef typename std::enable_if<
      std::is_base_of< base_element_type, T >::value, T
    >::type type;

#ifdef IRESEARCH_DEBUG
    return dynamic_cast<typename adjust_const<type>::reference>(dereference());
#else
    return static_cast<typename adjust_const<type>::reference>(dereference());
#endif // IRESEARCH_DEBUG
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns downcasted pointer to the iterator's value
  ///        or nullptr if there is no available conversion
  //////////////////////////////////////////////////////////////////////////////
  template<typename T>
  typename adjust_const<T>::pointer safe_as() const {
    typedef typename std::enable_if<
      std::is_base_of< base_element_type, T >::value, T
    >::type type;

    reference it = dereference();
    return dynamic_cast<typename adjust_const<type>::pointer>(&it);
  }

 private:
  friend class ::boost::iterator_core_access;

  reference dereference() const { 
    assert( *it_ );
    return **it_; 
  }
  void advance( difference_type n ) { it_ += n; }
  difference_type distance_to( const ptr_iterator& rhs ) const {
    return rhs.it_ - it_;
  }
  void increment() { ++it_; }
  void decrement() { --it_; }
  bool equal( const ptr_iterator& rhs ) const {
    return it_ == rhs.it_;
  }

  IteratorImpl it_;
};

NS_END

#endif
