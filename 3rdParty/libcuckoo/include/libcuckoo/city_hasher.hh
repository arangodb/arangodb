#ifndef _CITY_HASHER_HH
#define _CITY_HASHER_HH

#include <cityhash/city.h>
#include <string>

/*! CityHasher is a std::hash-style wrapper around CityHash. We
 *  encourage using CityHasher instead of the default std::hash if
 *  possible. */
template <class Key>
class CityHasher {
public:
    size_t operator()(const Key& k) const {
        if (sizeof(size_t) < 8) {
            return CityHash32((const char*) &k, sizeof(k));
        }
        /* Although the following line should be optimized away on 32-bit
         * builds, the cast is still necessary to stop MSVC emitting a
         * truncation warning. */
        return static_cast<size_t>(CityHash64((const char*) &k, sizeof(k)));
    }
};

/*! This is a template specialization of CityHasher for
 *  std::string. */
template <>
class CityHasher<std::string> {
public:
    size_t operator()(const std::string& k) const {
        if (sizeof(size_t) < 8) {
            return CityHash32(k.c_str(), k.size());
        }
        /* Although the following line should be optimized away on 32-bit
         * builds, the cast is still necessary to stop MSVC emitting a
         * truncation warning. */
        return static_cast<size_t>(CityHash64(k.c_str(), k.size()));
    }
};

#endif // _CITY_HASHER_HH
