/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Matei David 2014
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////

#ifndef BOUNDED_POINTER_HPP
#define BOUNDED_POINTER_HPP

#include <iostream>
#include <cstdlib>
#include <cassert>
#include <boost/container/vector.hpp>
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/core/no_exceptions_support.hpp>
#include <boost/move/adl_move_swap.hpp>

template < typename T >
class bounded_pointer;

template < typename T >
class bounded_reference;

template < typename T >
class bounded_allocator;


template < typename T >
class bounded_pointer
{
   private:
   void unspecified_bool_type_func() const {}
   typedef void (bounded_pointer::*unspecified_bool_type)() const;

   public:
   typedef typename boost::intrusive::detail::remove_const< T >::type mut_val_t;
   typedef const mut_val_t const_val_t;

   typedef bounded_reference<T>  reference;

   static const unsigned char max_offset = (unsigned char)-1;

   bounded_pointer() : m_offset(max_offset) {}

   bounded_pointer(const bounded_pointer& other)
      : m_offset(other.m_offset)
   {}

   template<class T2>
   bounded_pointer( const bounded_pointer<T2> &other
                  , typename boost::intrusive::detail::enable_if_convertible<T2*, T*>::type* = 0)
      :  m_offset(other.m_offset)
   {}

   bounded_pointer& operator = (const bounded_pointer& other)
   { m_offset = other.m_offset; return *this; }

   template <class T2>
   typename boost::intrusive::detail::enable_if_convertible<T2*, T*, bounded_pointer&>::type
      operator= (const bounded_pointer<T2> & other)
   {  m_offset = other.m_offset;  return *this;  }

   const bounded_pointer< typename boost::intrusive::detail::remove_const< T >::type >& unconst() const
   { return *reinterpret_cast< const bounded_pointer< typename boost::intrusive::detail::remove_const< T >::type >* >(this); }

   bounded_pointer< typename boost::intrusive::detail::remove_const< T >::type >& unconst()
   { return *reinterpret_cast< bounded_pointer< typename boost::intrusive::detail::remove_const< T >::type >* >(this); }

   static mut_val_t* base()
   {
      assert(bounded_allocator< mut_val_t >::inited());
      return &bounded_allocator< mut_val_t >::m_base[0];
   }

   static bounded_pointer pointer_to(bounded_reference< T > r) { return &r; }

   template<class U>
   static bounded_pointer const_cast_from(const bounded_pointer<U> &uptr)
   {  return uptr.unconst();  }

   operator unspecified_bool_type() const
   {
      return m_offset != max_offset? &bounded_pointer::unspecified_bool_type_func : 0;
   }

   T* raw() const
   { return base() + m_offset; }

   bounded_reference< T > operator * () const
   { return bounded_reference< T >(*this); }

   T* operator -> () const
      { return raw(); }

   bounded_pointer& operator ++ ()
   { ++m_offset; return *this; }

   bounded_pointer operator ++ (int)
   { bounded_pointer res(*this); ++(*this); return res; }

   friend bool operator == (const bounded_pointer& lhs, const bounded_pointer& rhs)
   { return lhs.m_offset == rhs.m_offset;   }

   friend bool operator != (const bounded_pointer& lhs, const bounded_pointer& rhs)
   { return lhs.m_offset != rhs.m_offset;   }

   friend bool operator < (const bounded_pointer& lhs, const bounded_pointer& rhs)
   { return lhs.m_offset < rhs.m_offset;   }

   friend bool operator <= (const bounded_pointer& lhs, const bounded_pointer& rhs)
   { return lhs.m_offset <= rhs.m_offset;   }

   friend bool operator >= (const bounded_pointer& lhs, const bounded_pointer& rhs)
   { return lhs.m_offset >= rhs.m_offset;   }

   friend bool operator > (const bounded_pointer& lhs, const bounded_pointer& rhs)
   { return lhs.m_offset > rhs.m_offset;   }

   friend std::ostream& operator << (std::ostream& os, const bounded_pointer& ptr)
   {
      os << static_cast< int >(ptr.m_offset);
      return os;
   }
   private:

   template <typename> friend class bounded_pointer;
   friend class bounded_reference< T >;
   friend class bounded_allocator< T >;

   unsigned char m_offset;
}; // class bounded_pointer

template < typename T >
class bounded_reference
{
   public:
   typedef typename boost::intrusive::detail::remove_const< T >::type mut_val_t;
   typedef const mut_val_t const_val_t;
   typedef bounded_pointer< T > pointer;
   static const unsigned char max_offset = pointer::max_offset;


   bounded_reference()
      : m_offset(max_offset)
   {}

   bounded_reference(const bounded_reference& other)
      : m_offset(other.m_offset)
   {}

   T& raw() const
   { assert(m_offset != max_offset); return *(bounded_pointer< T >::base() + m_offset); }

   operator T& () const
   { assert(m_offset != max_offset); return raw(); }

   bounded_pointer< T > operator & () const
   { assert(m_offset != max_offset); bounded_pointer< T > res; res.m_offset = m_offset; return res; }

   bounded_reference& operator = (const T& rhs)
   { assert(m_offset != max_offset); raw() = rhs; return *this; }

   bounded_reference& operator = (const bounded_reference& rhs)
   { assert(m_offset != max_offset); raw() = rhs.raw(); return *this; }

   template<class T2>
   bounded_reference( const bounded_reference<T2> &other
                    , typename boost::intrusive::detail::enable_if_convertible<T2*, T*>::type* = 0)
      :  m_offset(other.m_offset)
   {}

   template <class T2>
   typename boost::intrusive::detail::enable_if_convertible<T2*, T*, bounded_reference&>::type
      operator= (const bounded_reference<T2> & other)
   {  m_offset = other.m_offset;  return *this;  }

   friend std::ostream& operator << (std::ostream& os, const bounded_reference< T >& ref)
   {
      os << "[bptr=" << static_cast< int >(ref.m_offset) << ",deref=" << ref.raw() << "]";
      return os;
   }

   // the copy asop is shallow; we need swap overload to shuffle a vector of references
   friend void swap(bounded_reference& lhs, bounded_reference& rhs)
   {  ::boost::adl_move_swap(lhs.m_offset, rhs.m_offset); }

   private:
   template <typename> friend class bounded_reference;
   friend class bounded_pointer< T >;
   bounded_reference(bounded_pointer< T > bptr) : m_offset(bptr.m_offset) { assert(m_offset != max_offset); }

   unsigned char m_offset;
}; // class bounded_reference

template < typename T >
class bounded_allocator
{
   public:
   typedef T value_type;
   typedef bounded_pointer< T > pointer;

   static const unsigned char max_offset = pointer::max_offset;

   pointer allocate(size_t n)
   {
      assert(inited());
      assert(n == 1);(void)n;
      pointer p;
      unsigned char i;
      for (i = 0; i < max_offset && m_in_use[i]; ++i);
      assert(i < max_offset);
      p.m_offset = i;
      m_in_use[p.m_offset] = true;
      ++m_in_use_count;
      return p;
   }

   void deallocate(pointer p, size_t n)
   {
      assert(inited());
      assert(n == 1);(void)n;
      assert(m_in_use[p.m_offset]);
      m_in_use[p.m_offset] = false;
      --m_in_use_count;
   }

   // static methods
   static void init()
   {
      assert(m_in_use.empty());
      m_in_use = boost::container::vector< bool >(max_offset, false);
      // allocate non-constructed storage
      m_base = static_cast< T* >(::operator new [] (max_offset * sizeof(T)));
   }

   static bool inited()
   {
      return m_in_use.size() == max_offset;
   }

   static bool is_clear()
   {
      assert(inited());
      for (unsigned char i = 0; i < max_offset; ++i)
      {
         if (m_in_use[i])
         {
               return false;
         }
      }
      return true;
   }

   static void destroy()
   {
      // deallocate storage without destructors
      ::operator delete [] (m_base);
      m_in_use.clear();
   }

   private:
   friend class bounded_pointer< T >;
   friend class bounded_pointer< const T >;
   static T* m_base;
   static boost::container::vector< bool > m_in_use;
   static std::size_t m_in_use_count;
}; // class bounded_allocator

template <class BoundedAllocator>
class bounded_allocator_scope
{
   public:
   bounded_allocator_scope()
   {  BoundedAllocator::init();  }

   ~bounded_allocator_scope()
   {
      assert(BoundedAllocator::is_clear());
      BoundedAllocator::destroy();
   }
};

template < typename T >
T* bounded_allocator< T >::m_base = 0;

template < typename T >
boost::container::vector< bool > bounded_allocator< T >::m_in_use;

template < typename T >
std::size_t bounded_allocator< T >::m_in_use_count;

template < typename T >
class bounded_reference_cont
    : private boost::container::vector< bounded_reference< T > >
{
   private:
   typedef T val_type;
   typedef boost::container::vector< bounded_reference< T > > Base;
   typedef bounded_allocator< T > allocator_type;
   typedef bounded_pointer< T > pointer;

   public:
   typedef typename Base::value_type value_type;
   typedef typename Base::iterator iterator;
   typedef typename Base::const_iterator const_iterator;
   typedef typename Base::reference reference;
   typedef typename Base::const_reference const_reference;
   typedef typename Base::reverse_iterator reverse_iterator;
   typedef typename Base::const_reverse_iterator const_reverse_iterator;

   using Base::begin;
   using Base::rbegin;
   using Base::end;
   using Base::rend;
   using Base::front;
   using Base::back;
   using Base::size;
   using Base::operator[];
   using Base::push_back;

   explicit bounded_reference_cont(size_t n = 0)
      : Base(), m_allocator()
   {
      for (size_t i = 0; i < n; ++i){
         pointer p = m_allocator.allocate(1);
         BOOST_TRY{
            new (p.raw()) val_type();
         }
         BOOST_CATCH(...){
            m_allocator.deallocate(p, 1);
            BOOST_RETHROW
         }
         BOOST_CATCH_END
         Base::push_back(*p);
      }
   }

   bounded_reference_cont(const bounded_reference_cont& other)
      : Base(), m_allocator(other.m_allocator)
   {  *this = other;  }

   template < typename InputIterator >
   bounded_reference_cont(InputIterator it_start, InputIterator it_end)
      : Base(), m_allocator()
   {
      for (InputIterator it = it_start; it != it_end; ++it){
         pointer p = m_allocator.allocate(1);
         new (p.raw()) val_type(*it);
         Base::push_back(*p);
      }
   }

   template <typename InputIterator>   
   void assign(InputIterator it_start, InputIterator it_end)
   {
      this->clear();
      for (InputIterator it = it_start; it != it_end;){
         pointer p = m_allocator.allocate(1);
         new (p.raw()) val_type(*it++);
         Base::push_back(*p);
      }
   }

   ~bounded_reference_cont()
   {  clear();  }

   void clear()
   {
      while (!Base::empty()){
         pointer p = &Base::back();
         p->~val_type();
         m_allocator.deallocate(p, 1);
         Base::pop_back();
      }
   }

   bounded_reference_cont& operator = (const bounded_reference_cont& other)
   {
      if (&other != this){
         clear();
         for (typename Base::const_iterator it = other.begin(); it != other.end(); ++it){
               pointer p = m_allocator.allocate(1);
               new (p.raw()) val_type(*it);
               Base::push_back(*p);
         }
      }
      return *this;
   }

   private:
   allocator_type m_allocator;
}; // class bounded_reference_cont

template < typename T >
class bounded_pointer_holder
{
   public:
   typedef T value_type;
   typedef bounded_pointer< value_type > pointer;
   typedef bounded_pointer< const value_type > const_pointer;
   typedef bounded_allocator< value_type > allocator_type;

   bounded_pointer_holder() : _ptr(allocator_type().allocate(1))
   {  new (_ptr.raw()) value_type();   }

   ~bounded_pointer_holder()
   {
      _ptr->~value_type();
      allocator_type().deallocate(_ptr, 1);
   }

   const_pointer get_node () const
   { return _ptr; }

   pointer get_node ()
   { return _ptr; }

   private:
   const pointer _ptr;
}; // class bounded_pointer_holder

#endif
