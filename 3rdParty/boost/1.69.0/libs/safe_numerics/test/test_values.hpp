//
//  test_values.hpp
//
//  Created by Robert Ramey on 3/27/14.
//
//

#ifndef test_values_hpp
#define test_values_hpp

#include <cstdint>

#define VALUES (33, (                 \
    (std::int8_t)0x01,                \
    (std::int8_t)0x7f,                \
    (std::int8_t)0x80,                \
    (std::int8_t)0xff,                \
    (std::int16_t)0x0001,             \
    (std::int16_t)0x7fff,             \
    (std::int16_t)0x8000,             \
    (std::int16_t)0xffff,             \
    (std::int32_t)0x00000001,         \
    (std::int32_t)0x7fffffff,         \
    (std::int32_t)0x80000000,         \
    (std::int32_t)0xffffffff,         \
    (std::int64_t)0x0000000000000001, \
    (std::int64_t)0x7fffffffffffffff, \
    (std::int64_t)0x8000000000000000, \
    (std::int64_t)0xffffffffffffffff, \
    (std::uint8_t)0x01,               \
    (std::uint8_t)0x7f,               \
    (std::uint8_t)0x80,               \
    (std::uint8_t)0xff,               \
    (std::uint16_t)0x0001,            \
    (std::uint16_t)0x7fff,            \
    (std::uint16_t)0x8000,            \
    (std::uint16_t)0xffff,            \
    (std::uint32_t)0x00000001,        \
    (std::uint32_t)0x7fffffff,        \
    (std::uint32_t)0x80000000,        \
    (std::uint32_t)0xffffffff,        \
    (std::uint64_t)0x0000000000000001,\
    (std::uint64_t)0x7fffffffffffffff,\
    (std::uint64_t)0x8000000000000000,\
    (std::uint64_t)0xffffffffffffffff,\
    (std::int8_t)0x0                  \
))

#endif
