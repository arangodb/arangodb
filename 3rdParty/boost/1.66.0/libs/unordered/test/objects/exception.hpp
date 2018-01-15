
// Copyright 2006-2009 Daniel James.
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#if !defined(BOOST_UNORDERED_EXCEPTION_TEST_OBJECTS_HEADER)
#define BOOST_UNORDERED_EXCEPTION_TEST_OBJECTS_HEADER

#include "../helpers/exception_test.hpp"

#include "../helpers/count.hpp"
#include "../helpers/fwd.hpp"
#include "../helpers/generators.hpp"
#include "../helpers/memory.hpp"
#include "./fwd.hpp"
#include <boost/limits.hpp>
#include <cstddef>
#include <new>

namespace test {
  namespace exception {
    class object;
    class hash;
    class equal_to;
    template <class T> class allocator;
    object generate(object const*, random_generator);
    std::pair<object, object> generate(
      std::pair<object, object> const*, random_generator);

    struct true_type
    {
      enum
      {
        value = true
      };
    };

    struct false_type
    {
      enum
      {
        value = false
      };
    };

    class object : private counted_object
    {
    public:
      int tag1_, tag2_;

      explicit object() : tag1_(0), tag2_(0)
      {
        UNORDERED_SCOPE(object::object())
        {
          UNORDERED_EPOINT("Mock object default constructor.");
        }
      }

      explicit object(int t1, int t2 = 0) : tag1_(t1), tag2_(t2)
      {
        UNORDERED_SCOPE(object::object(int))
        {
          UNORDERED_EPOINT("Mock object constructor by value.");
        }
      }

      object(object const& x)
          : counted_object(x), tag1_(x.tag1_), tag2_(x.tag2_)
      {
        UNORDERED_SCOPE(object::object(object))
        {
          UNORDERED_EPOINT("Mock object copy constructor.");
        }
      }

      ~object()
      {
        tag1_ = -1;
        tag2_ = -1;
      }

      object& operator=(object const& x)
      {
        UNORDERED_SCOPE(object::operator=(object))
        {
          tag1_ = x.tag1_;
          UNORDERED_EPOINT("Mock object assign operator 1.");
          tag2_ = x.tag2_;
          // UNORDERED_EPOINT("Mock object assign operator 2.");
        }
        return *this;
      }

      friend bool operator==(object const& x1, object const& x2)
      {
        UNORDERED_SCOPE(operator==(object, object))
        {
          UNORDERED_EPOINT("Mock object equality operator.");
        }

        return x1.tag1_ == x2.tag1_ && x1.tag2_ == x2.tag2_;
      }

      friend bool operator!=(object const& x1, object const& x2)
      {
        UNORDERED_SCOPE(operator!=(object, object))
        {
          UNORDERED_EPOINT("Mock object inequality operator.");
        }

        return !(x1.tag1_ == x2.tag1_ && x1.tag2_ == x2.tag2_);
      }

      // None of the last few functions are used by the unordered associative
      // containers - so there aren't any exception points.
      friend bool operator<(object const& x1, object const& x2)
      {
        return x1.tag1_ < x2.tag1_ ||
               (x1.tag1_ == x2.tag1_ && x1.tag2_ < x2.tag2_);
      }

      friend object generate(object const*, random_generator g)
      {
        int* x = 0;
        return object(::test::generate(x, g), ::test::generate(x, g));
      }

      friend std::ostream& operator<<(std::ostream& out, object const& o)
      {
        return out << "(" << o.tag1_ << "," << o.tag2_ << ")";
      }
    };

    std::pair<object, object> generate(
      std::pair<object, object> const*, random_generator g)
    {
      int* x = 0;
      return std::make_pair(
        object(::test::generate(x, g), ::test::generate(x, g)),
        object(::test::generate(x, g), ::test::generate(x, g)));
    }

    class hash
    {
      int tag_;

    public:
      hash(int t = 0) : tag_(t)
      {
        UNORDERED_SCOPE(hash::object())
        {
          UNORDERED_EPOINT("Mock hash default constructor.");
        }
      }

      hash(hash const& x) : tag_(x.tag_)
      {
        UNORDERED_SCOPE(hash::hash(hash))
        {
          UNORDERED_EPOINT("Mock hash copy constructor.");
        }
      }

      hash& operator=(hash const& x)
      {
        UNORDERED_SCOPE(hash::operator=(hash))
        {
          UNORDERED_EPOINT("Mock hash assign operator 1.");
          tag_ = x.tag_;
          UNORDERED_EPOINT("Mock hash assign operator 2.");
        }
        return *this;
      }

      std::size_t operator()(object const& x) const
      {
        UNORDERED_SCOPE(hash::operator()(object))
        {
          UNORDERED_EPOINT("Mock hash function.");
        }

        return hash_impl(x);
      }

      std::size_t operator()(std::pair<object, object> const& x) const
      {
        UNORDERED_SCOPE(hash::operator()(std::pair<object, object>))
        {
          UNORDERED_EPOINT("Mock hash pair function.");
        }

        return hash_impl(x.first) * 193ul + hash_impl(x.second) * 97ul + 29ul;
      }

      std::size_t hash_impl(object const& x) const
      {
        int result;
        switch (tag_) {
        case 1:
          result = x.tag1_;
          break;
        case 2:
          result = x.tag2_;
          break;
        default:
          result = x.tag1_ + x.tag2_;
        }
        return static_cast<std::size_t>(result);
      }

      friend bool operator==(hash const& x1, hash const& x2)
      {
        UNORDERED_SCOPE(operator==(hash, hash))
        {
          UNORDERED_EPOINT("Mock hash equality function.");
        }
        return x1.tag_ == x2.tag_;
      }

      friend bool operator!=(hash const& x1, hash const& x2)
      {
        UNORDERED_SCOPE(hash::operator!=(hash, hash))
        {
          UNORDERED_EPOINT("Mock hash inequality function.");
        }
        return x1.tag_ != x2.tag_;
      }
    };

    class less
    {
      int tag_;

    public:
      less(int t = 0) : tag_(t) {}

      less(less const& x) : tag_(x.tag_) {}

      bool operator()(object const& x1, object const& x2) const
      {
        return less_impl(x1, x2);
      }

      bool operator()(std::pair<object, object> const& x1,
        std::pair<object, object> const& x2) const
      {
        if (less_impl(x1.first, x2.first)) {
          return true;
        }
        if (!less_impl(x1.first, x2.first)) {
          return false;
        }
        return less_impl(x1.second, x2.second);
      }

      bool less_impl(object const& x1, object const& x2) const
      {
        switch (tag_) {
        case 1:
          return x1.tag1_ < x2.tag1_;
        case 2:
          return x1.tag2_ < x2.tag2_;
        default:
          return x1 < x2;
        }
      }

      friend bool operator==(less const& x1, less const& x2)
      {
        return x1.tag_ == x2.tag_;
      }

      friend bool operator!=(less const& x1, less const& x2)
      {
        return x1.tag_ != x2.tag_;
      }
    };

    class equal_to
    {
      int tag_;

    public:
      equal_to(int t = 0) : tag_(t)
      {
        UNORDERED_SCOPE(equal_to::equal_to())
        {
          UNORDERED_EPOINT("Mock equal_to default constructor.");
        }
      }

      equal_to(equal_to const& x) : tag_(x.tag_)
      {
        UNORDERED_SCOPE(equal_to::equal_to(equal_to))
        {
          UNORDERED_EPOINT("Mock equal_to copy constructor.");
        }
      }

      equal_to& operator=(equal_to const& x)
      {
        UNORDERED_SCOPE(equal_to::operator=(equal_to))
        {
          UNORDERED_EPOINT("Mock equal_to assign operator 1.");
          tag_ = x.tag_;
          UNORDERED_EPOINT("Mock equal_to assign operator 2.");
        }
        return *this;
      }

      bool operator()(object const& x1, object const& x2) const
      {
        UNORDERED_SCOPE(equal_to::operator()(object, object))
        {
          UNORDERED_EPOINT("Mock equal_to function.");
        }

        return equal_impl(x1, x2);
      }

      bool operator()(std::pair<object, object> const& x1,
        std::pair<object, object> const& x2) const
      {
        UNORDERED_SCOPE(equal_to::operator()(
          std::pair<object, object>, std::pair<object, object>))
        {
          UNORDERED_EPOINT("Mock equal_to function.");
        }

        return equal_impl(x1.first, x2.first) &&
               equal_impl(x1.second, x2.second);
      }

      bool equal_impl(object const& x1, object const& x2) const
      {
        switch (tag_) {
        case 1:
          return x1.tag1_ == x2.tag1_;
        case 2:
          return x1.tag2_ == x2.tag2_;
        default:
          return x1 == x2;
        }
      }

      friend bool operator==(equal_to const& x1, equal_to const& x2)
      {
        UNORDERED_SCOPE(operator==(equal_to, equal_to))
        {
          UNORDERED_EPOINT("Mock equal_to equality function.");
        }
        return x1.tag_ == x2.tag_;
      }

      friend bool operator!=(equal_to const& x1, equal_to const& x2)
      {
        UNORDERED_SCOPE(operator!=(equal_to, equal_to))
        {
          UNORDERED_EPOINT("Mock equal_to inequality function.");
        }
        return x1.tag_ != x2.tag_;
      }

      friend less create_compare(equal_to x) { return less(x.tag_); }
    };

    template <class T> class allocator
    {
    public:
      int tag_;
      typedef std::size_t size_type;
      typedef std::ptrdiff_t difference_type;
      typedef T* pointer;
      typedef T const* const_pointer;
      typedef T& reference;
      typedef T const& const_reference;
      typedef T value_type;

      template <class U> struct rebind
      {
        typedef allocator<U> other;
      };

      explicit allocator(int t = 0) : tag_(t)
      {
        UNORDERED_SCOPE(allocator::allocator())
        {
          UNORDERED_EPOINT("Mock allocator default constructor.");
        }
        test::detail::tracker.allocator_ref();
      }

      template <class Y> allocator(allocator<Y> const& x) : tag_(x.tag_)
      {
        test::detail::tracker.allocator_ref();
      }

      allocator(allocator const& x) : tag_(x.tag_)
      {
        test::detail::tracker.allocator_ref();
      }

      ~allocator() { test::detail::tracker.allocator_unref(); }

      allocator& operator=(allocator const& x)
      {
        tag_ = x.tag_;
        return *this;
      }

      // If address throws, then it can't be used in erase or the
      // destructor, which is very limiting. I need to check up on
      // this.

      pointer address(reference r)
      {
        // UNORDERED_SCOPE(allocator::address(reference)) {
        //    UNORDERED_EPOINT("Mock allocator address function.");
        //}
        return pointer(&r);
      }

      const_pointer address(const_reference r)
      {
        // UNORDERED_SCOPE(allocator::address(const_reference)) {
        //    UNORDERED_EPOINT("Mock allocator const address function.");
        //}
        return const_pointer(&r);
      }

      pointer allocate(size_type n)
      {
        T* ptr = 0;
        UNORDERED_SCOPE(allocator::allocate(size_type))
        {
          UNORDERED_EPOINT("Mock allocator allocate function.");

          using namespace std;
          ptr = (T*)malloc(n * sizeof(T));
          if (!ptr)
            throw std::bad_alloc();
        }
        test::detail::tracker.track_allocate((void*)ptr, n, sizeof(T), tag_);
        return pointer(ptr);

        // return pointer(static_cast<T*>(::operator new(n * sizeof(T))));
      }

      pointer allocate(size_type n, void const*)
      {
        T* ptr = 0;
        UNORDERED_SCOPE(allocator::allocate(size_type, const_pointer))
        {
          UNORDERED_EPOINT("Mock allocator allocate function.");

          using namespace std;
          ptr = (T*)malloc(n * sizeof(T));
          if (!ptr)
            throw std::bad_alloc();
        }
        test::detail::tracker.track_allocate((void*)ptr, n, sizeof(T), tag_);
        return pointer(ptr);

        // return pointer(static_cast<T*>(::operator new(n * sizeof(T))));
      }

      void deallocate(pointer p, size_type n)
      {
        //::operator delete((void*) p);
        if (p) {
          test::detail::tracker.track_deallocate((void*)p, n, sizeof(T), tag_);
          using namespace std;
          free(p);
        }
      }

      void construct(pointer p, T const& t)
      {
        UNORDERED_SCOPE(allocator::construct(T*, T))
        {
          UNORDERED_EPOINT("Mock allocator construct function.");
          new (p) T(t);
        }
        test::detail::tracker.track_construct((void*)p, sizeof(T), tag_);
      }

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
      template <class... Args> void construct(T* p, BOOST_FWD_REF(Args)... args)
      {
        UNORDERED_SCOPE(allocator::construct(pointer, BOOST_FWD_REF(Args)...))
        {
          UNORDERED_EPOINT("Mock allocator construct function.");
          new (p) T(boost::forward<Args>(args)...);
        }
        test::detail::tracker.track_construct((void*)p, sizeof(T), tag_);
      }
#endif

      void destroy(T* p)
      {
        test::detail::tracker.track_destroy((void*)p, sizeof(T), tag_);
        p->~T();
      }

      size_type max_size() const
      {
        UNORDERED_SCOPE(allocator::construct(pointer, T))
        {
          UNORDERED_EPOINT("Mock allocator max_size function.");
        }
        return (std::numeric_limits<std::size_t>::max)();
      }

      typedef true_type propagate_on_container_copy_assignment;
      typedef true_type propagate_on_container_move_assignment;
      typedef true_type propagate_on_container_swap;
    };

    template <class T> void swap(allocator<T>& x, allocator<T>& y)
    {
      std::swap(x.tag_, y.tag_);
    }

    // It's pretty much impossible to write a compliant swap when these
    // two can throw. So they don't.

    template <class T>
    inline bool operator==(allocator<T> const& x, allocator<T> const& y)
    {
      // UNORDERED_SCOPE(operator==(allocator, allocator)) {
      //    UNORDERED_EPOINT("Mock allocator equality operator.");
      //}
      return x.tag_ == y.tag_;
    }

    template <class T>
    inline bool operator!=(allocator<T> const& x, allocator<T> const& y)
    {
      // UNORDERED_SCOPE(operator!=(allocator, allocator)) {
      //    UNORDERED_EPOINT("Mock allocator inequality operator.");
      //}
      return x.tag_ != y.tag_;
    }

    template <class T> class allocator2
    {
    public:
      int tag_;
      typedef std::size_t size_type;
      typedef std::ptrdiff_t difference_type;
      typedef T* pointer;
      typedef T const* const_pointer;
      typedef T& reference;
      typedef T const& const_reference;
      typedef T value_type;

      template <class U> struct rebind
      {
        typedef allocator2<U> other;
      };

      explicit allocator2(int t = 0) : tag_(t)
      {
        UNORDERED_SCOPE(allocator2::allocator2())
        {
          UNORDERED_EPOINT("Mock allocator2 default constructor.");
        }
        test::detail::tracker.allocator_ref();
      }

      allocator2(allocator<T> const& x) : tag_(x.tag_)
      {
        test::detail::tracker.allocator_ref();
      }

      template <class Y> allocator2(allocator2<Y> const& x) : tag_(x.tag_)
      {
        test::detail::tracker.allocator_ref();
      }

      allocator2(allocator2 const& x) : tag_(x.tag_)
      {
        test::detail::tracker.allocator_ref();
      }

      ~allocator2() { test::detail::tracker.allocator_unref(); }

      allocator2& operator=(allocator2 const&) { return *this; }

      // If address throws, then it can't be used in erase or the
      // destructor, which is very limiting. I need to check up on
      // this.

      pointer address(reference r)
      {
        // UNORDERED_SCOPE(allocator2::address(reference)) {
        //    UNORDERED_EPOINT("Mock allocator2 address function.");
        //}
        return pointer(&r);
      }

      const_pointer address(const_reference r)
      {
        // UNORDERED_SCOPE(allocator2::address(const_reference)) {
        //    UNORDERED_EPOINT("Mock allocator2 const address function.");
        //}
        return const_pointer(&r);
      }

      pointer allocate(size_type n)
      {
        T* ptr = 0;
        UNORDERED_SCOPE(allocator2::allocate(size_type))
        {
          UNORDERED_EPOINT("Mock allocator2 allocate function.");

          using namespace std;
          ptr = (T*)malloc(n * sizeof(T));
          if (!ptr)
            throw std::bad_alloc();
        }
        test::detail::tracker.track_allocate((void*)ptr, n, sizeof(T), tag_);
        return pointer(ptr);

        // return pointer(static_cast<T*>(::operator new(n * sizeof(T))));
      }

      pointer allocate(size_type n, void const*)
      {
        T* ptr = 0;
        UNORDERED_SCOPE(allocator2::allocate(size_type, const_pointer))
        {
          UNORDERED_EPOINT("Mock allocator2 allocate function.");

          using namespace std;
          ptr = (T*)malloc(n * sizeof(T));
          if (!ptr)
            throw std::bad_alloc();
        }
        test::detail::tracker.track_allocate((void*)ptr, n, sizeof(T), tag_);
        return pointer(ptr);

        // return pointer(static_cast<T*>(::operator new(n * sizeof(T))));
      }

      void deallocate(pointer p, size_type n)
      {
        //::operator delete((void*) p);
        if (p) {
          test::detail::tracker.track_deallocate((void*)p, n, sizeof(T), tag_);
          using namespace std;
          free(p);
        }
      }

      void construct(pointer p, T const& t)
      {
        UNORDERED_SCOPE(allocator2::construct(T*, T))
        {
          UNORDERED_EPOINT("Mock allocator2 construct function.");
          new (p) T(t);
        }
        test::detail::tracker.track_construct((void*)p, sizeof(T), tag_);
      }

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
      template <class... Args> void construct(T* p, BOOST_FWD_REF(Args)... args)
      {
        UNORDERED_SCOPE(allocator2::construct(pointer, BOOST_FWD_REF(Args)...))
        {
          UNORDERED_EPOINT("Mock allocator2 construct function.");
          new (p) T(boost::forward<Args>(args)...);
        }
        test::detail::tracker.track_construct((void*)p, sizeof(T), tag_);
      }
#endif

      void destroy(T* p)
      {
        test::detail::tracker.track_destroy((void*)p, sizeof(T), tag_);
        p->~T();
      }

      size_type max_size() const
      {
        UNORDERED_SCOPE(allocator2::construct(pointer, T))
        {
          UNORDERED_EPOINT("Mock allocator2 max_size function.");
        }
        return (std::numeric_limits<std::size_t>::max)();
      }

      typedef false_type propagate_on_container_copy_assignment;
      typedef false_type propagate_on_container_move_assignment;
      typedef false_type propagate_on_container_swap;
    };

    template <class T> void swap(allocator2<T>& x, allocator2<T>& y)
    {
      std::swap(x.tag_, y.tag_);
    }

    // It's pretty much impossible to write a compliant swap when these
    // two can throw. So they don't.

    template <class T>
    inline bool operator==(allocator2<T> const& x, allocator2<T> const& y)
    {
      // UNORDERED_SCOPE(operator==(allocator2, allocator2)) {
      //    UNORDERED_EPOINT("Mock allocator2 equality operator.");
      //}
      return x.tag_ == y.tag_;
    }

    template <class T>
    inline bool operator!=(allocator2<T> const& x, allocator2<T> const& y)
    {
      // UNORDERED_SCOPE(operator!=(allocator2, allocator2)) {
      //    UNORDERED_EPOINT("Mock allocator2 inequality operator.");
      //}
      return x.tag_ != y.tag_;
    }
  }
}

namespace test {
  template <typename X> struct equals_to_compare;
  template <> struct equals_to_compare<test::exception::equal_to>
  {
    typedef test::exception::less type;
  };
}

// Workaround for ADL deficient compilers
#if defined(BOOST_NO_ARGUMENT_DEPENDENT_LOOKUP)
namespace test {
  test::exception::object generate(
    test::exception::object const* x, random_generator g)
  {
    return test::exception::generate(x, g);
  }

  std::pair<test::exception::object, test::exception::object> generate(
    std::pair<test::exception::object, test::exception::object> const* x,
    random_generator g)
  {
    return test::exception::generate(x, g);
  }
}
#endif

#endif
