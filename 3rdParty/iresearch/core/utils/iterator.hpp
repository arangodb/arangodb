////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ITERATOR_H
#define IRESEARCH_ITERATOR_H

#include "noncopyable.hpp"
#include "ebo.hpp"
#include "std.hpp"

#include <memory>
#include <cassert>
#include <boost/iterator/iterator_facade.hpp>

namespace iresearch {

// ----------------------------------------------------------------------------
// --SECTION--                                             Java style iterators 
// ----------------------------------------------------------------------------

template<typename T>
struct IRESEARCH_API_TEMPLATE iterator {
  virtual ~iterator() = default;
  virtual T value() const = 0;
  virtual bool next() = 0;
};

template<
  typename Key,
  typename Value,
  typename Base,
  typename Less = std::less<Key>
> class iterator_adaptor
    : public Base,
      private irs::compact<0, Less> {
 private:
  typedef irs::compact<0, Less> comparer_t;

 public:
  typedef Value value_type;
  typedef Key key_type;
  typedef const value_type* const_pointer;
  typedef const value_type& const_reference;

  iterator_adaptor(
      const_pointer begin,
      const_pointer end,
      const Less& less = Less())
    : comparer_t(less),
      begin_(begin),
      cur_(begin),
      end_(end) {
  }

  const_reference value() const noexcept override {
    return *cur_;
  }

  bool seek(const key_type& key) noexcept override {
    begin_ = std::lower_bound(cur_, end_, key, comparer_t::get());
    return next();
  }

  bool next() noexcept override {
    if (begin_ == end_) {
      cur_ = begin_; // seal iterator
      return false;
    }

    cur_ = begin_++;
    return true;
  }

 private:
  const_pointer begin_;
  const_pointer cur_;
  const_pointer end_;
}; // iterator_adaptor

// ----------------------------------------------------------------------------
// --SECTION--                                              C++ style iterators 
// ----------------------------------------------------------------------------

namespace detail {

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

}

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
    : irstd::adjust_const<typename element_type::value_type, T> { };
  
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

}

#endif
