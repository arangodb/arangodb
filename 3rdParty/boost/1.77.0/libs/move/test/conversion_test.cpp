//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright David Abrahams, Vicente Botet, Ion Gaztanaga 2010-2012.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/move for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/move/utility_core.hpp>
#include <boost/move/detail/meta_utils.hpp>
#include <cassert>
#include <new>
#include <boost/move/detail/move_helpers.hpp>


enum ConstructionType { Copied, Moved, Other };

class conversion_source
{
   public:
   conversion_source(){}
   operator int() const { return 0; }
};

class conversion_target
{
   ConstructionType c_type_;
   public:
   conversion_target(conversion_source)
      { c_type_ = Other; }
   conversion_target()
      { c_type_ = Other; }
   conversion_target(const conversion_target &)
      { c_type_ = Copied; }
   ConstructionType construction_type() const
      { return c_type_; }
};


class conversion_target_copymovable
{
   ConstructionType c_type_;
   BOOST_COPYABLE_AND_MOVABLE(conversion_target_copymovable)
   public:
   conversion_target_copymovable()
      { c_type_ = Other; }
   conversion_target_copymovable(conversion_source)
      { c_type_ = Other; }
   conversion_target_copymovable(const conversion_target_copymovable &)
      { c_type_ = Copied; }
   conversion_target_copymovable(BOOST_RV_REF(conversion_target_copymovable) )
      { c_type_ = Moved; }
   conversion_target_copymovable &operator=(BOOST_RV_REF(conversion_target_copymovable) )
      { c_type_ = Moved; return *this; }
   conversion_target_copymovable &operator=(BOOST_COPY_ASSIGN_REF(conversion_target_copymovable) )
      { c_type_ = Copied; return *this; }
   ConstructionType construction_type() const
      {  return c_type_; }
};

class conversion_target_movable
{
   ConstructionType c_type_;
   BOOST_MOVABLE_BUT_NOT_COPYABLE(conversion_target_movable)
   public:
   conversion_target_movable()
      { c_type_ = Other; }
   conversion_target_movable(conversion_source)
      { c_type_ = Other; }
   conversion_target_movable(BOOST_RV_REF(conversion_target_movable) )
      { c_type_ = Moved; }
   conversion_target_movable &operator=(BOOST_RV_REF(conversion_target_movable) )
      { c_type_ = Moved; return *this; }
   ConstructionType construction_type() const
      {  return c_type_; }
};


template<class T>
class container
{
   T *storage_;
   public:
   struct const_iterator{};
   struct iterator : const_iterator{};
   container()
      : storage_(0)
   {}

   ~container()
   {  delete storage_; }

   container(const container &c)
      : storage_(c.storage_ ? new T(*c.storage_) : 0)
   {}

   container &operator=(const container &c)
   {
      if(storage_){
         delete storage_;
         storage_ = 0;
      }
      storage_ = c.storage_ ? new T(*c.storage_) : 0;
      return *this;
   }

   BOOST_MOVE_CONVERSION_AWARE_CATCH(push_back, T, void, priv_push_back)

   BOOST_MOVE_CONVERSION_AWARE_CATCH_1ARG(insert, T, iterator, priv_insert, const_iterator, const_iterator)

   template <class Iterator>
   iterator insert(Iterator, Iterator){ return iterator(); }

   ConstructionType construction_type() const
   {  return construction_type_impl
         (typename ::boost::move_detail::integral_constant<bool, ::boost::move_detail::is_class_or_union<T>::value>());
   }

   ConstructionType construction_type_impl(::boost::move_detail::true_type) const
     {  return storage_->construction_type(); }

   ConstructionType construction_type_impl(::boost::move_detail::false_type) const
     {  return Copied; }

   iterator begin() const { return iterator(); }
   
   private:
   template<class U>
    void priv_construct(BOOST_MOVE_CATCH_FWD(U) x)
   {
      if(storage_){
         delete storage_;
         storage_ = 0;
      }
      storage_ = new T(::boost::forward<U>(x));
   }

   template<class U>
   void priv_push_back(BOOST_MOVE_CATCH_FWD(U) x)
   {  priv_construct(::boost::forward<U>(x));   }

   template<class U>
   iterator priv_insert(const_iterator, BOOST_MOVE_CATCH_FWD(U) x)
   {  priv_construct(::boost::forward<U>(x));   return iterator();   }
};

class recursive_container
{
   BOOST_COPYABLE_AND_MOVABLE(recursive_container)
   public:
   recursive_container()
   {}

   recursive_container(const recursive_container &c)
      : container_(c.container_)
   {}

   recursive_container(BOOST_RV_REF(recursive_container) c)
      : container_(::boost::move(c.container_))
   {}

   recursive_container & operator =(BOOST_COPY_ASSIGN_REF(recursive_container) c)
   {
      container_= c.container_;
      return *this;
   }

   recursive_container & operator =(BOOST_RV_REF(recursive_container) c)
   {
      container_= ::boost::move(c.container_);
      return *this;
   }

   container<recursive_container> container_;
   friend bool operator< (const recursive_container &a, const recursive_container &b)
   {  return &a < &b;   }
};


int main()
{
   conversion_target_movable a;
   conversion_target_movable b(::boost::move(a));
   {
      container<conversion_target> c;
      {
         conversion_target x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         const conversion_target x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         c.push_back(conversion_target());
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), conversion_target());
         assert(c.construction_type() == Copied);
      }
      {
         conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         const conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         c.push_back(conversion_source());
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), conversion_source());
         assert(c.construction_type() == Copied);
      }
   }

   {
      container<conversion_target_copymovable> c;
      {
         conversion_target_copymovable x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         const conversion_target_copymovable x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         c.push_back(conversion_target_copymovable());
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), conversion_target_copymovable());
         assert(c.construction_type() == Moved);
      }
      {
         conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Moved);
      }
      {
         const conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Moved);
      }
      {
         c.push_back(conversion_source());
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), conversion_source());
         assert(c.construction_type() == Moved);
      }
   }
   {
      container<conversion_target_movable> c;
      //This should not compile
      //{
      //   conversion_target_movable x;
      //   c.push_back(x);
      //   assert(c.construction_type() == Copied);
      //}
      //{
      //   const conversion_target_movable x;
      //   c.push_back(x);
      //   assert(c.construction_type() == Copied);
      //}
      {
         c.push_back(conversion_target_movable());
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), conversion_target_movable());
         assert(c.construction_type() == Moved);
      }
      {
         conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Moved);
      }
      {
         const conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Moved);
      }
      {
         c.push_back(conversion_source());
         assert(c.construction_type() == Moved);
         c.insert(c.begin(), conversion_source());
         assert(c.construction_type() == Moved);
      }
   }
   {
      container<int> c;
      {
         int x = 0;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), c.construction_type());
         assert(c.construction_type() == Copied);
      }
      {
         const int x = 0;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         c.push_back(int(0));
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), int(0));
         assert(c.construction_type() == Copied);
      }
      {
         conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }

      {
         const conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         c.push_back(conversion_source());
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), conversion_source());
         assert(c.construction_type() == Copied);
      }
      //c.insert(c.begin(), c.begin());
   }

   {
      container<int> c;
      {
         int x = 0;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), c.construction_type());
         assert(c.construction_type() == Copied);
      }
      {
         const int x = 0;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         c.push_back(int(0));
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), int(0));
         assert(c.construction_type() == Copied);
      }
      {
         conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }

      {
         const conversion_source x;
         c.push_back(x);
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), x);
         assert(c.construction_type() == Copied);
      }
      {
         c.push_back(conversion_source());
         assert(c.construction_type() == Copied);
         c.insert(c.begin(), conversion_source());
         assert(c.construction_type() == Copied);
      }
      c.insert(c.begin(), c.begin());
   }

   {
      recursive_container c;
      recursive_container internal;
      c.container_.insert(c.container_.begin(), recursive_container());
      c.container_.insert(c.container_.begin(), internal);
      c.container_.insert(c.container_.begin(), c.container_.begin());
   }

   return 0;
}
