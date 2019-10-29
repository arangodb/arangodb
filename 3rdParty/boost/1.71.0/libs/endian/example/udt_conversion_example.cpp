//  udt_conversion_example.cpp  --------------------------------------------------------//

//  Copyright Beman Dawes 2013

//  Distributed under the Boost Software License, Version 1.0.
//  http://www.boost.org/LICENSE_1_0.txt

//--------------------------------------------------------------------------------------//

#include <boost/endian/detail/disable_warnings.hpp>

#include <boost/endian/conversion.hpp>
#include <iostream>
#include <cstring>

using namespace boost::endian;
using std::cout;
using std::endl;
using boost::int32_t;
using boost::int64_t;

namespace user
{
  class UDT
  {
  public:
    UDT() : id_(0), value_(0) {desc_[0] = '\0';}
    UDT(int32_t id, int64_t value, const char* desc) : id_(id), value_(value)
    {
      std::strncpy(desc_, desc, sizeof(desc_)-1);
      desc_[sizeof(desc_)-1] = '\0';
    }
    int32_t     id() const          {return id_;}
    int64_t     value() const       {return value_;}
    const char* desc() const        {return desc_;}
    void        id(int32_t x)       {id_ = x;}
    void        value(int64_t v)    {value_ = v;}
    void        desc(const char* s)
    {
      std::strncpy(desc_, s, sizeof(desc_)-1);
      desc_[sizeof(desc_-1)] = '\0';
    }

    friend void endian_reverse_inplace(UDT&);

  private:
    int32_t id_;
    int64_t value_;
    char    desc_[56];  // '/0'
  };

  void endian_reverse_inplace(UDT& x)
  {
    boost::endian::endian_reverse_inplace(x.id_);
    boost::endian::endian_reverse_inplace(x.value_);
  }
}

int main(int, char* [])
{
  user::UDT x(1, 123456789012345LL, "Bingo!");

  //cout << std::hex;
  cout << "(1) " << x.id() << ' ' << x.value() << ' ' << x.desc() << endl;

  user::endian_reverse_inplace(x);
  cout << "(2) " << x.id() << ' ' << x.value() << ' ' << x.desc() << endl;

  endian_reverse_inplace(x);
  cout << "(3) " << x.id() << ' ' << x.value() << ' ' << x.desc() << endl;

  conditional_reverse_inplace<order::little, order::big>(x);
  cout << "(4) " << x.id() << ' ' << x.value() << ' ' << x.desc() << endl;

  conditional_reverse_inplace(x, order::big, order::little);
  cout << "(5) " << x.id() << ' ' << x.value() << ' ' << x.desc() << endl;
}

#include <boost/endian/detail/disable_warnings_pop.hpp>
