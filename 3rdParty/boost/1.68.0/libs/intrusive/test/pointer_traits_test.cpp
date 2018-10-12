//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2011-2013. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
//////////////////////////////////////////////////////////////////////////////
#include <boost/intrusive/detail/mpl.hpp>
#include <boost/static_assert.hpp>
#include <boost/intrusive/pointer_traits.hpp>
#include <boost/core/lightweight_test.hpp>

struct CompleteSmartPtrStats
{
   static unsigned static_cast_called;
   static unsigned dynamic_cast_called;
   static unsigned const_cast_called;
   static unsigned pointer_to_called;

   static void reset_stats()
   {
      static_cast_called = 0;
      dynamic_cast_called = 0;
      const_cast_called = 0;
      pointer_to_called = 0;
   }
};

unsigned CompleteSmartPtrStats::static_cast_called= 0;
unsigned CompleteSmartPtrStats::dynamic_cast_called = 0;
unsigned CompleteSmartPtrStats::const_cast_called = 0;
unsigned CompleteSmartPtrStats::pointer_to_called = 0;

template<class T>
class CompleteSmartPtr
{
   template <class U>
   friend class CompleteSmartPtr;
   void unspecified_bool_type_func() const {}
   typedef void (CompleteSmartPtr::*unspecified_bool_type)() const;

   public:
   #if !defined(BOOST_NO_CXX11_TEMPLATE_ALIASES)
   template <class U> using rebind = CompleteSmartPtr<U>;
   #else
   template <class U> struct rebind
   {  typedef CompleteSmartPtr<U> other;  };
   #endif

   typedef char          difference_type;
   typedef T             element_type;

   CompleteSmartPtr()
    : ptr_(0)
   {}

   explicit CompleteSmartPtr(T &p)
    : ptr_(&p)
   {}

   CompleteSmartPtr(const CompleteSmartPtr &c)
   {  this->ptr_ = c.ptr_; }

   CompleteSmartPtr & operator=(const CompleteSmartPtr &c)
   {  this->ptr_ = c.ptr_; }

   static CompleteSmartPtr pointer_to(T &r)
   {  ++CompleteSmartPtrStats::pointer_to_called; return CompleteSmartPtr(r);  }

   T * operator->() const
   {  return ptr_;  }

   T & operator *() const
   {  return *ptr_;  }

   operator unspecified_bool_type() const
   {  return ptr_? &CompleteSmartPtr::unspecified_bool_type_func : 0;   }

   template<class U>
   static CompleteSmartPtr static_cast_from(const CompleteSmartPtr<U> &uptr)
   {
      ++CompleteSmartPtrStats::static_cast_called;
      element_type* const p = static_cast<element_type*>(uptr.ptr_);
      return p ? CompleteSmartPtr(*p) : CompleteSmartPtr();
   }

   template<class U>
   static CompleteSmartPtr const_cast_from(const CompleteSmartPtr<U> &uptr)
   {
      ++CompleteSmartPtrStats::const_cast_called; 
      element_type* const p = const_cast<element_type*>(uptr.ptr_);
      return p ? CompleteSmartPtr(*p) : CompleteSmartPtr();
   }
  
   template<class U>
   static CompleteSmartPtr dynamic_cast_from(const CompleteSmartPtr<U> &uptr)
   {
      ++CompleteSmartPtrStats::dynamic_cast_called; 
      element_type* const p = dynamic_cast<element_type*>(uptr.ptr_);
      return p ? CompleteSmartPtr(*p) : CompleteSmartPtr();
   }

   friend bool operator ==(const CompleteSmartPtr &l, const CompleteSmartPtr &r)
   {  return l.ptr_ == r.ptr_; }

   friend bool operator !=(const CompleteSmartPtr &l, const CompleteSmartPtr &r)
   {  return l.ptr_ != r.ptr_; }

   private:
   T *ptr_;
};


template<class T>
class SimpleSmartPtr
{
   void unspecified_bool_type_func() const {}
   typedef void (SimpleSmartPtr::*unspecified_bool_type)() const;
   public:

   SimpleSmartPtr()
    : ptr_(0)
   {}

   explicit SimpleSmartPtr(T *p)
    : ptr_(p)
   {}

   SimpleSmartPtr(const SimpleSmartPtr &c)
   {  this->ptr_ = c.ptr_; }

   SimpleSmartPtr & operator=(const SimpleSmartPtr &c)
   {  this->ptr_ = c.ptr_; }

   friend bool operator ==(const SimpleSmartPtr &l, const SimpleSmartPtr &r)
   {  return l.ptr_ == r.ptr_; }

   friend bool operator !=(const SimpleSmartPtr &l, const SimpleSmartPtr &r)
   {  return l.ptr_ != r.ptr_; }

   T* operator->() const
   {  return ptr_;  }

   T & operator *() const
   {  return *ptr_;  }

   operator unspecified_bool_type() const
   {  return ptr_? &SimpleSmartPtr::unspecified_bool_type_func : 0;   }

   private:
   T *ptr_;
};

class B{ public: virtual ~B(){} };
class D : public B {};
class DD : public virtual B {};

int main()
{
   int dummy;

   //Raw pointer
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       <int*>::element_type, int>::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       <int*>::pointer, int*>::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       <int*>::difference_type, std::ptrdiff_t>::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       <int*>::rebind_pointer<double>::type
                       , double*>::value ));
   BOOST_TEST(boost::intrusive::pointer_traits<int*>::pointer_to(dummy) == &dummy);
   BOOST_TEST(boost::intrusive::pointer_traits<D*>::  static_cast_from((B*)0)     == 0);
   BOOST_TEST(boost::intrusive::pointer_traits<D*>:: const_cast_from((const D*)0) == 0);
   BOOST_TEST(boost::intrusive::pointer_traits<DD*>:: dynamic_cast_from((B*)0)    == 0);

   //Complete smart pointer
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < CompleteSmartPtr<int> >::element_type, int>::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < CompleteSmartPtr<int> >::pointer, CompleteSmartPtr<int> >::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < CompleteSmartPtr<int> >::difference_type, char>::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < CompleteSmartPtr<int> >::rebind_pointer<double>::type
                       , CompleteSmartPtr<double> >::value ));
   //pointer_to
   CompleteSmartPtrStats::reset_stats();
   BOOST_TEST(boost::intrusive::pointer_traits< CompleteSmartPtr<int> >::pointer_to(dummy) == CompleteSmartPtr<int>(dummy));
   BOOST_TEST(CompleteSmartPtrStats::pointer_to_called == 1);
   //static_cast_from
   CompleteSmartPtrStats::reset_stats();
   BOOST_TEST(boost::intrusive::pointer_traits< CompleteSmartPtr<D> >::static_cast_from(CompleteSmartPtr<B>()) == CompleteSmartPtr<D>());
   BOOST_TEST(CompleteSmartPtrStats::static_cast_called == 1);
   //const_cast_from
   CompleteSmartPtrStats::reset_stats();
   BOOST_TEST(boost::intrusive::pointer_traits< CompleteSmartPtr<D> >::const_cast_from(CompleteSmartPtr<const D>()) == CompleteSmartPtr<D>());
   BOOST_TEST(CompleteSmartPtrStats::const_cast_called == 1);
   //dynamic_cast_from
   CompleteSmartPtrStats::reset_stats();
   BOOST_TEST(boost::intrusive::pointer_traits< CompleteSmartPtr<DD> >::dynamic_cast_from(CompleteSmartPtr<B>()) == CompleteSmartPtr<DD>());
   BOOST_TEST(CompleteSmartPtrStats::dynamic_cast_called == 1);

   //Simple smart pointer
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < SimpleSmartPtr<int> >::element_type, int>::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < SimpleSmartPtr<int> >::pointer, SimpleSmartPtr<int> >::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < SimpleSmartPtr<int> >::difference_type, std::ptrdiff_t>::value ));
   BOOST_STATIC_ASSERT(( boost::intrusive::detail::is_same<boost::intrusive::pointer_traits
                       < SimpleSmartPtr<int> >::rebind_pointer<double>::type
                       , SimpleSmartPtr<double> >::value ));

   BOOST_TEST(boost::intrusive::pointer_traits< SimpleSmartPtr<int> >::pointer_to(dummy) == SimpleSmartPtr<int>(&dummy));
   BOOST_TEST(boost::intrusive::pointer_traits< SimpleSmartPtr<D> >  ::static_cast_from(SimpleSmartPtr<B>()) == SimpleSmartPtr<D>());
   BOOST_TEST(boost::intrusive::pointer_traits< SimpleSmartPtr<D> >  ::const_cast_from(SimpleSmartPtr<const D>()) == SimpleSmartPtr<D>());
   BOOST_TEST(boost::intrusive::pointer_traits< SimpleSmartPtr<DD> >::dynamic_cast_from(SimpleSmartPtr<B>()) == SimpleSmartPtr<DD>());

   return boost::report_errors();
}
