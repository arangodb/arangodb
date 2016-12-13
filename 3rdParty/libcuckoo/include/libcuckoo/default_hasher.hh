#ifndef _DEFAULT_HASHER_HH
#define _DEFAULT_HASHER_HH

#include <string>
#include <type_traits>

/*! DefaultHasher is the default hash class used in the table. It overloads a
 *  few types that std::hash does badly on (namely integers), and falls back to
 *  std::hash for anything else. */
template <class Key>
class DefaultHasher {
    std::hash<Key> fallback;

public:
    template <class T = Key>
    typename std::enable_if<std::is_integral<T>::value, size_t>::type
    operator()(const Key& k) const {
        // This constant is found in the CityHash code
        return k * 0x9ddfea08eb382d69ULL;
    }

    template <class T = Key>
    typename std::enable_if<!std::is_integral<T>::value, size_t>::type
    operator()(const Key& k) const {
        return fallback(k);
    }
};

#endif // _DEFAULT_HASHER_HH
