/////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga  2015-2015.
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/intrusive for documentation.
//
/////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_INTRUSIVE_DETAIL_INT_HOLDER_HPP
#define BOOST_INTRUSIVE_DETAIL_INT_HOLDER_HPP

namespace boost{
namespace intrusive{

struct int_holder
{
   explicit int_holder(int value = 0)
      : int_(value)
   {}

   int_holder &operator=(int value)
   {  int_ = value;  return *this;  }

   int int_value() const
   {  return int_;  }

   friend bool operator==(const int_holder &l, const int_holder &r)
   {  return l.int_ == r.int_;  }

   friend bool operator!=(const int_holder &l, const int_holder &r)
   {  return l.int_ != r.int_;  }

   friend bool operator<(const int_holder &l, const int_holder &r)
   {  return l.int_ < r.int_;  }

   friend bool operator>(const int_holder &l, const int_holder &r)
   {  return l.int_ > r.int_;  }

   friend bool operator<=(const int_holder &l, const int_holder &r)
   {  return l.int_ <= r.int_;  }

   friend bool operator>=(const int_holder &l, const int_holder &r)
   {  return l.int_ >= r.int_;  }

///
   friend bool operator==(int l, const int_holder &r)
   {  return l == r.int_;  }

   friend bool operator!=(int l, const int_holder &r)
   {  return l != r.int_;  }

   friend bool operator<(int l, const int_holder &r)
   {  return l < r.int_;  }

   friend bool operator>(int l, const int_holder &r)
   {  return l > r.int_;  }

   friend bool operator<=(int l, const int_holder &r)
   {  return l <= r.int_;  }

   friend bool operator>=(int l, const int_holder &r)
   {  return l >= r.int_;  }

   bool operator< (int i) const
   {  return int_ < i;   }

   bool operator> (int i) const
   {  return int_ > i;   }

   bool operator<= (int i) const
   {  return int_ <= i;   }

   bool operator>= (int i) const
   {  return int_ >= i;   }

   bool operator== (int i) const
   {  return int_ == i;   }

   bool operator!= (int i) const
   {  return int_ != i;   }

   friend std::size_t hash_value(const int_holder &t)
   {
      boost::hash<int> hasher;
      return hasher((&t)->int_value());
   }

   int int_;
};

template<class ValueType>
struct int_holder_key_of_value
{
   typedef int_holder type;

   const type &operator()(const ValueType &tv)
   {  return tv.get_int_holder();  }
};

}  //namespace boost{
}  //namespace intrusive{

#endif   //BOOST_INTRUSIVE_DETAIL_INT_HOLDER_HPP
