//  endian/example/uses_cases.cpp  -----------------------------------------------------//

//  Copyright Beman Dawes 2014

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//--------------------------------------------------------------------------------------//

#ifndef _SCL_SECURE_NO_WARNINGS
# define _SCL_SECURE_NO_WARNINGS
#endif

#ifndef _CRT_SECURE_NO_WARNINGS
# define _CRT_SECURE_NO_WARNINGS
#endif


#include <boost/endian/conversion.hpp>
#include <boost/endian/buffers.hpp>
#include <boost/endian/arithmetic.hpp>
#include <iostream>

using namespace boost::endian;

using std::cout;
using std::endl;




  { // Use case 2 - Endian buffer types

    struct Record
    {
      big_ubuf32_t count;  // big endian
      big_buf32_t  value;  // big endian
    };

    Record rec;

    read(&rec, sizeof(Record));

    uint32_t count = rec.count.value();
    int32_t value = rec.value.value();

    ++count;
    value += fee;

    rec.count = count;
    rec.value = value;

    write(&rec, sizeof(Record));
  }

  { // Use case 3a - Endian arithmetic types

    struct Record
    {
      big_uint32_t count;  // big endian
      big_int32_t  value;  // big endian
    };

    Record rec;

    read(&rec, sizeof(Record));

    ++rec.count;
    rec.value += fee;

    write(&rec, sizeof(Record));
  }

  { // Use case 3b - Endian arithmetic types

    struct Record
    {
      big_uint32_t count;  // big endian
      big_int32_t  value;  // big endian
    };

    Record rec;

    read(&rec, sizeof(Record));

    uint32_t count = rec.count;
    int32_t value = rec.value;

    ++count;
    value += fee;

    rec.count = count;
    rec.value = value;

    write(&rec, sizeof(Record));
  }

  //  Recommended approach when conversion time is not a concern
  //
  //  Conversion time is not a concert with this application because the minimum
  //  possible number of conversions is performed and because I/O time will be
  //  much greater than conversion time.

  {
    struct Record
    {
      big_uint32_t count;  // big endian
      big_int32_t  value;  // big endian
    };

    Record rec;

    read(&rec, sizeof(Record));

    ++rec.count;
    rec.value += fee;

    write(&rec, sizeof(Record));
  }

//  Recommended approach when conversion time is a concern
  //
  //  Conversion time is a concert with this application because (1) any conversions
  //  performed in the loop will consume a great deal of time and because (2) 
  //  computation time will be much greater than I/O time.

  {
    struct Record
    {
      big_uint32_t count;  // big endian
      big_int32_t  value;  // big endian
    };

    Record rec;

    read(&rec, sizeof(Record));

    uint32_t count = rec.count;
    int32_t value = rec.value;

    for (long long i = 0; i < several_gazillion; ++i)  // (1)
    {
      ... immensely complex computation using rec variables many times // (2)
    }

    rec.count = count;
    rec.value = value;

    write(&rec, sizeof(Record));
  }

}
