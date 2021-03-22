#pragma once

// Compilation environment control macros

#ifdef __APPLE__
    #ifndef _DARWIN_C_SOURCE
        #define _DARWIN_C_SOURCE 1
    #endif
#endif

#if defined(__CYGWIN__) || ! defined(_WIN32)
    #ifndef _XOPEN_SOURCE
        #define _XOPEN_SOURCE 700 // Posix 2008
    #endif
    #ifndef _REENTRANT
        #define _REENTRANT 1
    #endif
#endif

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX 1
    #endif
    #ifndef UNICODE
        #define UNICODE 1
    #endif
    #ifndef _UNICODE
        #define _UNICODE 1
    #endif
    #ifndef WINVER
        #define WINVER 0x601 // Windows 7
    #endif
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x601
    #endif
#endif

// Includes go here so anything that needs the macros above will see them

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <exception>
#include <functional>
#include <iostream>
#include <istream>
#include <iterator>
#include <limits>
#include <mutex>
#include <new>
#include <optional>
#include <ostream>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <vector>

#ifdef __GNUC__
    #include <cxxabi.h>
#endif

// GNU brain damage

#ifdef major
    #undef major
#endif
#ifdef minor
    #undef minor
#endif

// Microsoft brain damage

#ifdef pascal
    #undef pascal
#endif

#ifdef _MSC_VER
    #pragma warning(disable: 4127) // Conditional expression is constant
    #pragma warning(disable: 4310) // Cast truncates constant value
    #pragma warning(disable: 4459) // Declaration hides global declaration
#endif

// Preprocessor macros

#ifndef RS_ATTR_UNUSED
    #ifdef __GNUC__
        #define RS_ATTR_UNUSED __attribute__((__unused__))
    #else
        #define RS_ATTR_UNUSED
    #endif
#endif

#define RS_BITMASK_OPERATORS(E) \
    constexpr RS_ATTR_UNUSED bool operator!(E x) noexcept { return std::underlying_type_t<E>(x) == 0; } \
    constexpr RS_ATTR_UNUSED E operator~(E x) noexcept { return E(~ std::underlying_type_t<E>(x)); } \
    constexpr RS_ATTR_UNUSED E operator&(E lhs, E rhs) noexcept { using U = std::underlying_type_t<E>; return E(U(lhs) & U(rhs)); } \
    constexpr RS_ATTR_UNUSED E operator|(E lhs, E rhs) noexcept { using U = std::underlying_type_t<E>; return E(U(lhs) | U(rhs)); } \
    constexpr RS_ATTR_UNUSED E operator^(E lhs, E rhs) noexcept { using U = std::underlying_type_t<E>; return E(U(lhs) ^ U(rhs)); } \
    constexpr RS_ATTR_UNUSED E& operator&=(E& lhs, E rhs) noexcept { return lhs = lhs & rhs; } \
    constexpr RS_ATTR_UNUSED E& operator|=(E& lhs, E rhs) noexcept { return lhs = lhs | rhs; } \
    constexpr RS_ATTR_UNUSED E& operator^=(E& lhs, E rhs) noexcept { return lhs = lhs ^ rhs; }

#ifndef RS_ENUM
    #define RS_ENUM_IMPLEMENTATION(EnumType, IntType, class_tag, name_prefix, first_value, first_name, ...) \
        enum class_tag EnumType: IntType { \
            RS_##EnumType##_begin_ = first_value, \
            first_name = first_value, \
            __VA_ARGS__, \
            RS_##EnumType##_end_ \
        }; \
        constexpr RS_ATTR_UNUSED bool enum_is_valid(EnumType t) noexcept { \
            return t >= EnumType::RS_##EnumType##_begin_ && t < EnumType::RS_##EnumType##_end_; \
        } \
        inline RS_ATTR_UNUSED std::vector<EnumType> make_enum_values(EnumType) { \
            static constexpr size_t n = size_t(IntType(EnumType::RS_##EnumType##_end_) - IntType(first_value)); \
            std::vector<EnumType> v; \
            v.reserve(n); \
            for (IntType i = first_value; i < IntType(EnumType::RS_##EnumType##_end_); ++i) \
                v.push_back(EnumType(i)); \
            return v; \
        } \
        inline RS_ATTR_UNUSED bool str_to_enum(std::string_view s, EnumType& t) noexcept { \
            return ::RS::RS_Detail::enum_from_str(s, t, EnumType::RS_##EnumType##_begin_, #EnumType "::", #first_name "," #__VA_ARGS__); \
        } \
        inline RS_ATTR_UNUSED std::string to_str(EnumType t) { \
            return ::RS::RS_Detail::enum_to_str(t, EnumType::RS_##EnumType##_begin_, EnumType::RS_##EnumType##_end_, name_prefix, #first_name "," #__VA_ARGS__); \
        } \
        inline RS_ATTR_UNUSED std::ostream& operator<<(std::ostream& out, EnumType t) { return out << to_str(t); }
    #define RS_ENUM(EnumType, IntType, first_value, first_name, ...) \
        RS_ENUM_IMPLEMENTATION(EnumType, IntType,, "", first_value, first_name, __VA_ARGS__)
    #define RS_ENUM_CLASS(EnumType, IntType, first_value, first_name, ...) \
        RS_ENUM_IMPLEMENTATION(EnumType, IntType, class, #EnumType "::", first_value, first_name, __VA_ARGS__)
#endif

#ifndef RS_MOVE_ONLY
    #define RS_MOVE_ONLY(T) \
        T(const T&) = delete; \
        T(T&&) = default; \
        T& operator=(const T&) = delete; \
        T& operator=(T&&) = default;
#endif

#ifndef RS_NO_COPY_MOVE
    #define RS_NO_COPY_MOVE(T) \
        T(const T&) = delete; \
        T(T&&) = delete; \
        T& operator=(const T&) = delete; \
        T& operator=(T&&) = delete;
#endif

#ifndef RS_NO_INSTANCE
    #if defined(__GNUC__) && __GNUC__ < 9
        #define RS_NO_INSTANCE(T) \
            T() = delete; \
            T(const T&) = delete; \
            T(T&&) = delete; \
            T& operator=(const T&) = delete; \
            T& operator=(T&&) = delete;
    #else
        #define RS_NO_INSTANCE(T) \
            T() = delete; \
            T(const T&) = delete; \
            T(T&&) = delete; \
            ~T() = delete; \
            T& operator=(const T&) = delete; \
            T& operator=(T&&) = delete;
    #endif
#endif

#define RS_OVERLOAD(f) [] (auto&&... args) { return f(std::forward<decltype(args)>(args)...); }

// Must be used in the global namespace

#ifndef RS_DEFINE_STD_HASH
    #define RS_DEFINE_STD_HASH(T) \
        namespace std { \
            template <> struct hash<T> { \
                using argument_type = T; \
                using result_type = size_t; \
                size_t operator()(const T& t) const noexcept { return t.hash(); } \
            }; \
        }
#endif

// Link control

#define RS_LDLIB(libs)

// This instructs the makefile to link with one or more static libraries.
// Specify library names without the -l prefix (e.g. RS_LDLIB(foo) will link
// with -lfoo). If link order is important for a particular set of libraries,
// supply them in a space delimited list in a single RS_LDLIB() line.

// Libraries that are needed only on specific targets can be prefixed with one
// of the target identifiers listed below, e.g. RS_LDLIB(apple:foo) will link
// with -lfoo for Apple targets only. Only one target can be specified per
// invocation; if the same libraries are needed on multiple targets, but not
// on all targets, you will need a separate RS_LDLIB() line for each target.

// * apple:
// * cygwin:
// * linux:
// * msvc:

// RS_LDLIB() lines are picked up at the "make dep" stage; if you change a
// link library, the change will not be detected until dependencies are
// regenerated.

namespace RS {

    // Basic types

    #ifdef _XOPEN_SOURCE
        using NativeCharacter = char;
    #else
        #define RS_NATIVE_WCHAR 1
        using NativeCharacter = wchar_t;
    #endif

    #if WCHAR_MAX < 0x7fffffff
        #define RS_WCHAR_UTF16 1
        using WcharEquivalent = char16_t;
    #else
        #define RS_WCHAR_UTF32 1
        using WcharEquivalent = char32_t;
    #endif

    using Ustring = std::string;
    using Uview = std::string_view;
    using Strings = std::vector<std::string>;
    using NativeString = std::basic_string<NativeCharacter>;
    using WstringEquivalent = std::basic_string<WcharEquivalent>;

    template <auto> class IncompleteTemplate;
    class IncompleteType;
    template <auto> class CompleteTemplate { RS_NO_INSTANCE(CompleteTemplate); };
    class CompleteType { RS_NO_INSTANCE(CompleteType); };

    // Constants

    constexpr const char* ascii_whitespace = "\t\n\v\f\r ";
    constexpr size_t npos = size_t(-1);

    #if defined(_WIN32) || (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
        constexpr bool big_endian_target = false;
        constexpr bool little_endian_target = true;
    #else
        constexpr bool big_endian_target = true;
        constexpr bool little_endian_target = false;
    #endif

    template <typename T> constexpr bool dependent_false = false;

    namespace RS_Detail {

        template <int N> using SetBitType =
            std::conditional_t<(N < 8), uint8_t,
            std::conditional_t<(N < 16), uint16_t,
            std::conditional_t<(N < 32), uint32_t,
            std::conditional_t<(N < 64), uint64_t, void>>>>;

    }

    template <int N> constexpr RS_Detail::SetBitType<N> setbit = RS_Detail::SetBitType<N>(1) << N;

    // Forward declarations

    template <typename T> bool from_str(std::string_view view, T& t) noexcept;
    template <typename T> T from_str(std::string_view view);
    template <typename T> std::string to_str(const T& t);

    // Enumeration macro implementation

    namespace RS_Detail {

        template <typename EnumType>
        const std::vector<std::string> enum_str_list(const char* names) {
            static const std::vector<std::string> names_vec = [=] {
                auto ptr = names;
                std::vector<std::string> vec;
                for (;;) {
                    if (*ptr == ' ')
                        ++ptr;
                    auto next = std::strchr(ptr, ',');
                    if (! next)
                        break;
                    vec.push_back(std::string(ptr, next - ptr));
                    ptr = next + 1;
                }
                vec.push_back(ptr);
                return vec;
            }();
            return names_vec;
        }

        template <typename EnumType>
        bool enum_from_str(std::string_view s, EnumType& t, EnumType begin, const char* prefix, const char* names) {
            using U = std::underlying_type_t<EnumType>;
            size_t psize = std::strlen(prefix);
            if (psize < s.size() && std::memcmp(s.data(), prefix, psize) == 0)
                s = std::string_view(s.data() + psize, s.size() - psize);
            auto& names_vec = enum_str_list<EnumType>(names);
            for (auto& name: names_vec) {
                if (s == name) {
                    t = EnumType(U(begin) + U(&name - names_vec.data()));
                    return true;
                }
            }
            return false;
        }

        template <typename EnumType>
        std::string enum_to_str(EnumType t, EnumType begin, EnumType end, const char* prefix, const char* names) {
            using U = std::underlying_type_t<EnumType>;
            if (t >= begin && t < end)
                return prefix + enum_str_list<EnumType>(names)[U(t) - U(begin)];
            else
                return std::to_string(U(t));
        }

    }

    template <typename EnumType>
    std::vector<EnumType> enum_values() {
        static const auto enum_vec = make_enum_values(EnumType());
        return enum_vec;
    }

    // Error handling

    inline void runtime_assert(bool condition, std::string_view message) noexcept {
        if (! condition) {
            std::fwrite(message.data(), 1, message.size(), stderr);
            std::fputc('\n', stderr);
            std::abort();
        }
    }

    // Metaprogramming

    namespace Meta {

        // Walter E. Brown, N4502 Proposing Standard Library Support for the C++ Detection Idiom V2 (2015)
        // http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2015/n4502.pdf

        template <typename...> using VoidType = void;

        template <typename Default, typename, template <typename...> typename Archetype, typename... Args>
        struct Detector {
            using value_t = std::false_type;
            using type = Default;
        };
        template <typename Default, template <typename...> typename Archetype, typename... Args>
        struct Detector<Default, VoidType<Archetype<Args...>>, Archetype, Args...> {
            using value_t = std::true_type;
            using type = Archetype<Args...>;
        };

        template <template <typename...> typename Archetype, typename... Args>
            using IsDetected = typename Detector<CompleteType, void, Archetype, Args...>::value_t;
        template <template <typename...> typename Archetype, typename... Args>
            constexpr bool is_detected = IsDetected<Archetype, Args...>::value;
        template <template <typename...> typename Archetype, typename... Args>
            using DetectedType = typename Detector<CompleteType, void, Archetype, Args...>::type;

        // Return nested type if detected, otherwise default
        template <typename Default, template <typename...> typename Archetype, typename... Args>
            using DetectedOr = typename Detector<Default, void, Archetype, Args...>::type;

        // Detect only if it yields a specific return type
        template <typename Result, template <typename...> typename Archetype, typename... Args>
            using IsDetectedExact = std::is_same<Result, DetectedType<Archetype, Args...>>;
        template <typename Result, template <typename...> typename Archetype, typename... Args>
            constexpr bool is_detected_exact = IsDetectedExact<Result, Archetype, Args...>::value;

        // Detect only if it yields a return type convertible to the given type
        template <typename Result, template <typename...> typename Archetype, typename... Args>
            using IsDetectedConvertible = std::is_convertible<DetectedType<Archetype, Args...>, Result>;
        template <typename Result, template <typename...> typename Archetype, typename... Args>
            constexpr bool is_detected_convertible = IsDetectedConvertible<Result, Archetype, Args...>::value;

        namespace MetaDetail {

            template <typename T> using IteratorCategory = typename std::iterator_traits<T>::iterator_category;

            template <typename T> using HasAdlBeginArchetype = decltype(begin(std::declval<T>()));
            template <typename T> using HasAdlEndArchetype = decltype(end(std::declval<T>()));
            template <typename T> using HasStdBeginArchetype = decltype(std::begin(std::declval<T>()));
            template <typename T> using HasStdEndArchetype = decltype(std::end(std::declval<T>()));

        }

        template <typename T> using IsIterator = IsDetected<MetaDetail::IteratorCategory, T>;
        template <typename T> constexpr bool is_iterator = IsIterator<T>::value;
        template <typename T> struct IsRange {
            static constexpr bool adl_begin = is_detected<MetaDetail::HasAdlBeginArchetype, T>;
            static constexpr bool adl_end = is_detected<MetaDetail::HasAdlEndArchetype, T>;
            static constexpr bool std_begin = is_detected<MetaDetail::HasStdBeginArchetype, T>;
            static constexpr bool std_end = is_detected<MetaDetail::HasStdEndArchetype, T>;
            static constexpr bool value = (adl_begin && adl_end) || (std_begin && std_end);
        };
        template <typename T> constexpr bool is_range = IsRange<T>::value;

        namespace MetaDetail {

            template <typename T, bool Iter = Meta::is_iterator<T>> struct IteratorValueType { using type = void; };
            template <typename T> struct IteratorValueType<T, true> { using type = std::decay_t<decltype(*std::declval<T>())>; };

            template <typename T, bool Std = IsDetected<HasStdBeginArchetype, T>::value, bool Adl = IsDetected<HasAdlBeginArchetype, T>::value> struct RangeIteratorType { using type = void; };
            template <typename T, bool Adl> struct RangeIteratorType<T, true, Adl> { using type = decltype(std::begin(std::declval<T&>())); };
            template <typename T> struct RangeIteratorType<T, false, true> { using type = decltype(begin(std::declval<T&>())); };

            template <typename T> using HasValueTypeArchetype = typename T::value_type;
            template <typename T, bool Val = IsDetected<HasValueTypeArchetype, T>::value> struct RangeValueType { using type = typename T::value_type; };
            template <typename T> struct RangeValueType<T, false> { using type = typename IteratorValueType<typename RangeIteratorType<T>::type>::type; };

        }

        template <typename T> using IteratorValue = typename MetaDetail::IteratorValueType<T>::type;
        template <typename T> using RangeIterator = typename MetaDetail::RangeIteratorType<T>::type;
        template <typename T> using RangeValue = typename MetaDetail::RangeValueType<T>::type;

    }

    // Mixin types

    template <typename T>
    struct EqualityComparable {
        friend bool operator!=(const T& lhs, const T& rhs) noexcept { return ! (lhs == rhs); }
    };

    template <typename T>
    struct LessThanComparable:
    EqualityComparable<T> {
        friend bool operator>(const T& lhs, const T& rhs) noexcept { return rhs < lhs; }
        friend bool operator<=(const T& lhs, const T& rhs) noexcept { return ! (rhs < lhs); }
        friend bool operator>=(const T& lhs, const T& rhs) noexcept { return ! (lhs < rhs); }
    };

    template <typename T, typename CV>
    struct InputIterator:
    EqualityComparable<T> {
        using difference_type = ptrdiff_t;
        using iterator_category = std::input_iterator_tag;
        using pointer = CV*;
        using reference = CV&;
        using value_type = std::remove_const_t<CV>;
        CV* operator->() const noexcept { return &*static_cast<const T&>(*this); }
        friend T operator++(T& t, int) { T rc = t; ++t; return rc; }
    };

    template <typename T>
    struct OutputIterator {
        using difference_type = void;
        using iterator_category = std::output_iterator_tag;
        using pointer = void;
        using reference = void;
        using value_type = void;
        T& operator*() noexcept { return static_cast<T&>(*this); }
        friend T& operator++(T& t) noexcept { return t; }
        friend T operator++(T& t, int) noexcept { return t; }
    };

    template <typename T, typename CV>
    struct ForwardIterator:
    InputIterator<T, CV> {
        using iterator_category = std::forward_iterator_tag;
    };

    template <typename T, typename CV>
    struct BidirectionalIterator:
    ForwardIterator<T, CV> {
        using iterator_category = std::bidirectional_iterator_tag;
        friend T operator--(T& t, int) { T rc = t; --t; return rc; }
    };

    template <typename T, typename CV>
    struct RandomAccessIterator:
    BidirectionalIterator<T, CV>,
    LessThanComparable<T> {
        using iterator_category = std::random_access_iterator_tag;
        CV& operator[](ptrdiff_t i) const noexcept { T t = static_cast<const T&>(*this); t += i; return *t; }
        friend T& operator++(T& t) { t += 1; return t; }
        friend T& operator--(T& t) { t += -1; return t; }
        friend T& operator-=(T& lhs, ptrdiff_t rhs) { return lhs += - rhs; }
        friend T operator+(const T& lhs, ptrdiff_t rhs) { T t = lhs; t += rhs; return t; }
        friend T operator+(ptrdiff_t lhs, const T& rhs) { T t = rhs; t += lhs; return t; }
        friend T operator-(const T& lhs, ptrdiff_t rhs) { T t = lhs; t -= rhs; return t; }
        friend bool operator==(const T& lhs, const T& rhs) noexcept { return lhs - rhs == 0; }
        friend bool operator<(const T& lhs, const T& rhs) noexcept { return lhs - rhs < 0; }
    };

    template <typename T, typename CV>
    struct FlexibleRandomAccessIterator:
    BidirectionalIterator<T, CV>,
    LessThanComparable<T> {
        using iterator_category = std::random_access_iterator_tag;
        CV& operator[](ptrdiff_t i) const noexcept { T t = static_cast<const T&>(*this); t += i; return *t; }
        friend T& operator-=(T& lhs, ptrdiff_t rhs) { return lhs += - rhs; }
        friend T operator+(const T& lhs, ptrdiff_t rhs) { T t = lhs; t += rhs; return t; }
        friend T operator+(ptrdiff_t lhs, const T& rhs) { T t = rhs; t += lhs; return t; }
        friend T operator-(const T& lhs, ptrdiff_t rhs) { T t = lhs; t -= rhs; return t; }
        friend bool operator<(const T& lhs, const T& rhs) noexcept { return lhs - rhs < 0; }
    };

    // Algorithms

    namespace RS_Detail {

        template <typename CT, typename VT> using HasPushBackArchetype = decltype(std::declval<CT>().push_back(std::declval<VT>()));

    }

    template <typename Container, typename T>
    void append_to(Container& con, const T& t) {
        static constexpr bool has_push_back = Meta::is_detected<RS_Detail::HasPushBackArchetype, Container, T>;
        if constexpr (has_push_back)
            con.push_back(t);
        else
            con.insert(con.end(), t);
    }

    template <typename Container>
    class AppendIterator:
    public OutputIterator<AppendIterator<Container>> {
    public:
        using value_type = Meta::RangeValue<Container>;
        AppendIterator() = default;
        explicit AppendIterator(Container& c): con(&c) {}
        AppendIterator& operator=(const value_type& v) { append_to(*con, v); return *this; }
    private:
        Container* con;
    };

    template <typename Container> AppendIterator<Container> append(Container& con) { return AppendIterator<Container>(con); }
    template <typename Container> AppendIterator<Container> overwrite(Container& con) { con.clear(); return AppendIterator<Container>(con); }

    template <typename Range, typename Container>
    const Range& operator>>(const Range& lhs, AppendIterator<Container> rhs) {
        using std::begin;
        using std::end;
        std::copy(begin(lhs), end(lhs), rhs);
        return lhs;
    }

    template <typename Range1, typename Range2, typename Compare>
    int compare_3way(const Range1& r1, const Range2& r2, Compare cmp) {
        using std::begin;
        using std::end;
        auto i = begin(r1), e1 = end(r1);
        auto j = begin(r2), e2 = end(r2);
        for (; i != e1 && j != e2; ++i, ++j) {
            if (cmp(*i, *j))
                return -1;
            else if (cmp(*j, *i))
                return 1;
        }
        return i != e1 ? 1 : j != e2 ? -1 : 0;
    }

    template <typename Range1, typename Range2>
    int compare_3way(const Range1& r1, const Range2& r2) {
        return compare_3way(r1, r2, std::less<>());
    }

    // Arithmetic functions

    template <typename T> constexpr auto as_signed(T t) noexcept { return static_cast<std::make_signed_t<T>>(t); }
    template <typename T> constexpr auto as_unsigned(T t) noexcept { return static_cast<std::make_unsigned_t<T>>(t); }

    #ifdef __GNUC__

        template <typename T>
        constexpr int popcount(T t) noexcept {
            return __builtin_popcountll(uint64_t(t));
        }

        template <typename T>
        constexpr int bit_width(T t) noexcept {
            return t ? 64 - __builtin_clzll(uint64_t(t)) : 0;
        }

    #else

        template <typename T>
        constexpr int popcount(T t) noexcept {
            static constexpr int8_t bits16[16] = {0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4};
            int n = 0;
            for (; t; t >>= 4) { n += bits16[t & 0xf]; }
            return n;
        }

        template <typename T>
        constexpr int bit_width(T t) noexcept {
            int n = 0;
            for (; t > 0xff; t >>= 8) { n += 8; }
            for (; t; t >>= 1) { ++n; }
            return n;
        }

    #endif

    template <typename T>
    constexpr T bit_floor(T t) noexcept {
        return t ? T(1) << (bit_width(t) - 1) : 0;
    }

    template <typename T>
    constexpr T bit_ceil(T t) noexcept {
        return t > 1 ? T(1) << (bit_width(t - 1) - 1) << 1 : t;
    }

    template <typename T>
    constexpr bool has_single_bit(T t) noexcept {
        return popcount(t) == 1;
    }

    constexpr uint64_t letter_to_mask(char c) noexcept {
        return c >= 'A' && c <= 'Z' ? 1ull << (c - 'A') : c >= 'a' && c <= 'z' ? 1ull << (c - 'a' + 26) : 0;
    }

    namespace RS_Detail {

        template <typename T>
        constexpr T rotl_helper(T t, int n) noexcept {
            int tbits = 8 * sizeof(T);
            int nbits = n % tbits;
            return nbits ? T(t << nbits) | T(t >> (tbits - nbits)) : t;
        }

        template <typename T>
        constexpr T rotr_helper(T t, int n) noexcept {
            int tbits = 8 * sizeof(T);
            int nbits = n % tbits;
            return nbits ? T(t >> nbits) | T(t << (tbits - nbits)) : t;
        }

    }

    template <typename T>
    constexpr T rotl(T t, int n) noexcept {
        return n == 0 ? t : n < 0 ? RS_Detail::rotr_helper(t, - n) : RS_Detail::rotl_helper(t, n);
    }

    template <typename T>
    constexpr T rotr(T t, int n) noexcept {
        return n == 0 ? t : n < 0 ? RS_Detail::rotl_helper(t, - n) : RS_Detail::rotr_helper(t, n);
    }

    // String functions (forward)

    namespace RS_Detail {

        template <typename T, bool = std::is_signed<T>::value> struct SimpleAbs { constexpr T operator()(T t) const noexcept { return t; } };
        template <typename T> struct SimpleAbs<T, true> { constexpr T operator()(T t) const noexcept { return t < T(0) ? - t : t; } };

        template <typename T>
        Ustring int_to_string(T x, int base, size_t digits) {
            bool neg = x < T(0);
            auto b = as_unsigned(T(base));
            auto y = as_unsigned(SimpleAbs<T>()(x));
            Ustring s;
            do {
                auto d = int(y % b);
                s += char((d < 10 ? '0' : 'a' - 10) + d);
                y /= b;
            } while (y || s.size() < digits);
            if (neg)
                s += '-';
            std::reverse(s.begin(), s.end());
            return s;
        }

    }

    template <typename T> Ustring bin(T x, size_t digits = 8 * sizeof(T)) { return RS_Detail::int_to_string(x, 2, digits); }
    template <typename T> Ustring dec(T x, size_t digits = 1) { return RS_Detail::int_to_string(x, 10, digits); }
    template <typename T> Ustring hex(T x, size_t digits = 2 * sizeof(T)) { return RS_Detail::int_to_string(x, 16, digits); }

    // Date and time functions

    constexpr uint32_t utc_zone = 1;
    constexpr uint32_t local_zone = 2;

    template <typename R, typename P>
    void from_seconds(double s, std::chrono::duration<R, P>& d) noexcept {
        using namespace std::chrono;
        d = duration_cast<duration<R, P>>(duration<double>(s));
    }

    template <typename R, typename P>
    double to_seconds(const std::chrono::duration<R, P>& d) noexcept {
        using namespace std::chrono;
        return duration_cast<duration<double>>(d).count();
    }

    // Unfortunately strftime() doesn't set errno and simply returns zero on
    // any error. This means that there is no way to distinguish between an
    // invalid format string, an output buffer that is too small, and a
    // legitimately empty result. Here we try first with a reasonable output
    // length, and if that fails, try again with a much larger one; if it
    // still fails, give up. This could theoretically fail in the face of a
    // very long localized date format, but there doesn't seem to be a better
    // solution.

    inline Ustring format_date(std::chrono::system_clock::time_point tp, Uview format, uint32_t flags = utc_zone) {
        using namespace std::chrono;
        uint32_t zone = flags & (utc_zone | local_zone);
        if (popcount(zone) > 1 || flags - zone)
            throw std::invalid_argument("Invalid date flags: 0x" + hex(flags, 1));
        if (format.empty())
            return {};
        auto t = system_clock::to_time_t(tp);
        tm stm = zone == local_zone ? *localtime(&t) : *gmtime(&t);
        Ustring fs(format);
        Ustring result(std::max(2 * format.size(), size_t(100)), '\0');
        auto rc = strftime(&result[0], result.size(), fs.data(), &stm);
        if (rc == 0) {
            result.resize(10 * result.size(), '\0');
            rc = strftime(&result[0], result.size(), fs.data(), &stm);
        }
        result.resize(rc);
        result.shrink_to_fit();
        return result;
    }

    inline Ustring format_date(std::chrono::system_clock::time_point tp, int prec = 0, uint32_t flags = utc_zone) {
        using namespace std::literals;
        uint32_t zone = flags & (utc_zone | local_zone);
        if (popcount(zone) > 1 || flags - zone)
            throw std::invalid_argument("Invalid date flags: 0x" + hex(flags, 1));
        Ustring result = format_date(tp, "%Y-%m-%d %H:%M:%S"s, zone);
        if (prec > 0) {
            double sec = to_seconds(tp.time_since_epoch());
            double isec;
            double fsec = std::modf(sec, &isec);
            Ustring buf(prec + 3, '\0');
            snprintf(&buf[0], buf.size(), "%.*f", prec, fsec);
            result += buf.data() + 1;
        }
        return result;
    }

    template <typename R, typename P>
    Ustring format_time(const std::chrono::duration<R, P>& time, int prec = 0) {
        using namespace std::chrono;
        auto whole = duration_cast<seconds>(time);
        int64_t isec = whole.count();
        auto frac = time - duration_cast<duration<R, P>>(whole);
        double fsec = duration_cast<duration<double>>(frac).count();
        Ustring result;
        if (isec < 0 || fsec < 0)
            result += '-';
        isec = std::abs(isec);
        fsec = std::abs(fsec);
        int64_t d = isec / 86400;
        isec -= 86400 * d;
        if (d)
            result += dec(d) + 'd';
        int64_t h = isec / 3600;
        isec -= 3600 * h;
        if (d || h)
            result += dec(h, d ? 2 : 1) + 'h';
        int64_t m = isec / 60;
        if (d || h || m)
            result += dec(m, d || h ? 2 : 1) + 'm';
        isec -= 60 * m;
        result += dec(isec, d || h || m ? 2 : 1);
        if (prec > 0) {
            Ustring buf(prec + 3, '\0');
            snprintf(&buf[0], buf.size(), "%.*f", prec, fsec);
            result += buf.data() + 1;
        }
        result += 's';
        return result;
    }

    inline std::chrono::system_clock::time_point make_date(int year, int month, int day, int hour = 0, int min = 0, double sec = 0, uint32_t flags = utc_zone) {
        using namespace std::chrono;
        uint32_t zone = flags & (utc_zone | local_zone);
        if (popcount(zone) > 1 || flags - zone)
            throw std::invalid_argument("Invalid date flags: 0x" + hex(flags, 1));
        double isec = 0, fsec = modf(sec, &isec);
        if (fsec < 0) {
            isec -= 1;
            fsec += 1;
        }
        tm stm;
        std::memset(&stm, 0, sizeof(stm));
        stm.tm_sec = int(isec);
        stm.tm_min = min;
        stm.tm_hour = hour;
        stm.tm_mday = day;
        stm.tm_mon = month - 1;
        stm.tm_year = year - 1900;
        stm.tm_isdst = -1;
        time_t t;
        if (zone == local_zone)
            t = std::mktime(&stm);
        else
            #ifdef _XOPEN_SOURCE
                t = timegm(&stm);
            #else
                t = _mkgmtime(&stm);
            #endif
        system_clock::time_point::rep extra(int64_t(fsec * system_clock::time_point::duration(seconds(1)).count()));
        return system_clock::from_time_t(t) + system_clock::time_point::duration(extra);
    }

    // Keyword arguments

    template <typename T, int ID> struct Kwptr { const T* ptr; };
    template <typename T, int ID = 0> struct Kwarg { constexpr Kwptr<T, ID> operator=(const T& t) const noexcept { return {&t}; } };

    template <typename T, int ID> constexpr bool kwtest(Kwarg<T, ID>) { return false; }
    template <typename T, int ID, typename... Args> constexpr bool kwtest(Kwarg<T, ID>, Kwptr<T, ID>, Args...) { return true; }
    template <int ID, typename... Args> constexpr bool kwtest(Kwarg<bool, ID>, Kwarg<bool, ID>, Args...) { return true; }
    template <typename T, int ID, typename Arg1, typename... Args> constexpr bool kwtest(Kwarg<T, ID> key, Arg1, Args... args) { return kwtest(key, args...); }

    template <typename T, int ID> T kwget(Kwarg<T, ID>, const T& def) { return def; }
    template <typename T, int ID, typename... Args> T kwget(Kwarg<T, ID>, const T&, Kwptr<T, ID> p, Args...) { return *p.ptr; }
    template <int ID, typename... Args> bool kwget(Kwarg<bool, ID>, bool, Kwarg<bool, ID>, Args...) { return true; }
    template <typename T, int ID, typename Arg1, typename... Args> T kwget(Kwarg<T, ID> key, const T& def, Arg1, Args... args) { return kwget(key, def, args...); }

    // Range types

    template <typename Iterator>
    struct Irange {
        Iterator first, second;
        constexpr Iterator begin() const noexcept { return first; }
        constexpr Iterator end() const noexcept { return second; }
        constexpr bool empty() const noexcept { return first == second; }
        constexpr size_t size() const noexcept { return std::distance(first, second); }
        template <typename I2> operator Irange<I2>() const noexcept { return {first, second}; }
    };

    template <typename Iterator> constexpr Irange<Iterator> irange(const Iterator& i, const Iterator& j) { return {i, j}; }
    template <typename Iterator> constexpr Irange<Iterator> irange(const std::pair<Iterator, Iterator>& p) { return {p.first, p.second}; }

    namespace RS_Detail {

        template <typename T> struct ArrayCount;
        template <typename T, size_t N> struct ArrayCount<T[N]> { static constexpr size_t value = N; };

    }

    template <typename InputRange>
    size_t range_count(const InputRange& r) {
        using std::begin;
        using std::end;
        return std::distance(begin(r), end(r));
    }

    template <typename InputRange>
    bool range_empty(const InputRange& r) {
        using std::begin;
        using std::end;
        return begin(r) == end(r);
    }

    // Scope guards

    enum class ScopeState {
        exit,    // Invoke callback unconditionally in destructor
        fail,    // Invoke callback when scope unwinding due to exception, but not on normal exit
        success  // Invoke callback on normal exit, but not when scope unwinding due to exception
    };

    template <typename F, ScopeState S>
    class BasicScopeGuard {
    public:
        BasicScopeGuard() = default;
        BasicScopeGuard(F&& f) try:
            callback_(std::forward<F>(f)),
            inflight_(std::uncaught_exceptions()) {}
            catch (...) {
                if constexpr (S != ScopeState::success) {
                    try { f(); }
                    catch (...) {}
                }
                throw;
            }
        BasicScopeGuard(const BasicScopeGuard&) = delete;
        BasicScopeGuard(BasicScopeGuard&& g) noexcept:
            callback_(std::move(g.callback_)),
            inflight_(std::exchange(g.inflight_, -1)) {}
        ~BasicScopeGuard() noexcept { close(); }
        BasicScopeGuard& operator=(const BasicScopeGuard&) = delete;
        BasicScopeGuard& operator=(F&& f) {
            close();
            callback_ = std::forward<F>(f);
            inflight_ = std::uncaught_exceptions();
            return *this;
        }
        BasicScopeGuard& operator=(BasicScopeGuard&& g) noexcept {
            if (&g != this) {
                close();
                callback_ = std::move(g.callback_);
                inflight_ = std::exchange(g.inflight_, -1);
            }
            return *this;
        }
        void release() noexcept { inflight_ = -1; }
    private:
        F callback_;
        int inflight_ = -1;
        void close() noexcept {
            if (inflight_ >= 0) {
                bool call = true;
                if constexpr (S == ScopeState::fail)
                    call = std::uncaught_exceptions() > inflight_;
                else if constexpr (S == ScopeState::success)
                    call = std::uncaught_exceptions() <= inflight_;
                if (call)
                    try { callback_(); } catch (...) {}
                inflight_ = -1;
            }
        }
    };

    using ScopeExit = BasicScopeGuard<std::function<void()>, ScopeState::exit>;
    using ScopeFail = BasicScopeGuard<std::function<void()>, ScopeState::fail>;
    using ScopeSuccess = BasicScopeGuard<std::function<void()>, ScopeState::success>;

    template <typename F> inline auto scope_exit(F&& f) { return BasicScopeGuard<F, ScopeState::exit>(std::forward<F>(f)); }
    template <typename F> inline auto scope_fail(F&& f) { return BasicScopeGuard<F, ScopeState::fail>(std::forward<F>(f)); }
    template <typename F> inline auto scope_success(F&& f) { return BasicScopeGuard<F, ScopeState::success>(std::forward<F>(f)); }

    template <typename T> inline auto make_lock(T& t) { return std::unique_lock<T>(t); }
    template <typename T> inline auto make_shared_lock(T& t) { return std::shared_lock<T>(t); }

    // String functions

    constexpr bool ascii_iscntrl(char c) noexcept { return uint8_t(c) <= 31 || c == 127; }
    constexpr bool ascii_isdigit(char c) noexcept { return c >= '0' && c <= '9'; }
    constexpr bool ascii_isgraph(char c) noexcept { return c >= '!' && c <= '~'; }
    constexpr bool ascii_islower(char c) noexcept { return c >= 'a' && c <= 'z'; }
    constexpr bool ascii_isprint(char c) noexcept { return c >= ' ' && c <= '~'; }
    constexpr bool ascii_ispunct(char c) noexcept { return (c >= '!' && c <= '/') || (c >= ':' && c <= '@') || (c >= '[' && c <= '`') || (c >= '{' && c <= '~'); }
    constexpr bool ascii_isspace(char c) noexcept { return (c >= '\t' && c <= '\r') || c == ' '; }
    constexpr bool ascii_isupper(char c) noexcept { return c >= 'A' && c <= 'Z'; }
    constexpr bool ascii_isalpha(char c) noexcept { return ascii_islower(c) || ascii_isupper(c); }
    constexpr bool ascii_isalnum(char c) noexcept { return ascii_isalpha(c) || ascii_isdigit(c); }
    constexpr bool ascii_isxdigit(char c) noexcept { return ascii_isdigit(c) || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'); }
    constexpr bool ascii_isalnum_w(char c) noexcept { return ascii_isalnum(c) || c == '_'; }
    constexpr bool ascii_isalpha_w(char c) noexcept { return ascii_isalpha(c) || c == '_'; }
    constexpr bool ascii_ispunct_w(char c) noexcept { return ascii_ispunct(c) && c != '_'; }
    constexpr char ascii_tolower(char c) noexcept { return ascii_isupper(c) ? char(c + 32) : c; }
    constexpr char ascii_toupper(char c) noexcept { return ascii_islower(c) ? char(c - 32) : c; }
    constexpr bool is_ascii(char c) noexcept { return uint8_t(c) <= 127; }

    inline std::string ascii_lowercase(std::string_view s) {
        Ustring r(s);
        std::transform(r.begin(), r.end(), r.begin(), ascii_tolower);
        return r;
    }

    inline std::string ascii_uppercase(std::string_view s) {
        Ustring r(s);
        std::transform(r.begin(), r.end(), r.begin(), ascii_toupper);
        return r;
    }

    inline std::string ascii_titlecase(std::string_view s) {
        Ustring r(s);
        bool was_alpha = false;
        for (char& c: r) {
            if (was_alpha)
                c = ascii_tolower(c);
            else
                c = ascii_toupper(c);
            was_alpha = ascii_isalpha(c);
        }
        return r;
    }

    inline std::string ascii_sentencecase(std::string_view s) {
        Ustring r(s);
        bool new_sentence = true, was_break = false;
        for (char& c: r) {
            if (c == '\n' || c == '\f' || c == '\r') {
                if (was_break)
                    new_sentence = true;
                was_break = true;
            } else {
                was_break = false;
                if (c == '.') {
                    new_sentence = true;
                } else if (new_sentence && ascii_isalpha(c)) {
                    c = ascii_toupper(c);
                    new_sentence = false;
                }
            }
        }
        return r;
    }

    inline unsigned long long binnum(std::string_view str) noexcept {
        std::string s(str);
        return std::strtoull(s.data(), nullptr, 2);
    }

    inline long long decnum(std::string_view str) noexcept {
        std::string s(str);
        return std::strtoll(s.data(), nullptr, 10);
    }

    inline unsigned long long hexnum(std::string_view str) noexcept {
        std::string s(str);
        return std::strtoull(s.data(), nullptr, 16);
    }

    inline double fpnum(std::string_view str) noexcept {
        std::string s(str);
        return std::strtod(s.data(), nullptr);
    }

    template <typename C>
    std::basic_string<C> cstr(const C* ptr) {
        using S = std::basic_string<C>;
        return ptr ? S(ptr) : S();
    }

    template <typename C>
    std::basic_string<C> cstr(const C* ptr, size_t n) {
        using S = std::basic_string<C>;
        return ptr ? S(ptr, n) : S(n, '\0');
    }

    template <typename C>
    size_t cstr_size(const C* ptr) {
        if (! ptr)
            return 0;
        if constexpr (sizeof(C) == 1) {
            return std::strlen(reinterpret_cast<const char*>(ptr));
        } else if constexpr (sizeof(C) == sizeof(wchar_t)) {
            return std::wcslen(reinterpret_cast<const wchar_t*>(ptr));
        } else {
            size_t n = 0;
            while (ptr[n] != C(0))
                ++n;
            return n;
        }
    }

    inline Ustring dent(size_t depth) { return Ustring(4 * depth, ' '); }

    namespace {

        inline std::string quote_string(std::string_view str, bool escape_8bit) {
            std::string result = "\"";
            for (auto c: str) {
                auto b = uint8_t(c);
                switch (c) {
                    case 0:     result += "\\0"; break;
                    case '\t':  result += "\\t"; break;
                    case '\n':  result += "\\n"; break;
                    case '\f':  result += "\\f"; break;
                    case '\r':  result += "\\r"; break;
                    case '\"':  result += "\\\""; break;
                    case '\\':  result += "\\\\"; break;
                    default:
                        if (b <= 0x1f || b == 0x7f || (escape_8bit && b >= 0x80))
                            result += "\\x" + hex(b);
                        else
                            result += c;
                        break;
                }
            }
            result += '\"';
            return result;
        }

    }

    inline std::string quote(std::string_view str) {
        return quote_string(str, false);
    }

    inline Ustring bquote(std::string_view str) {
        return quote_string(str, true);
    }

    template <typename T>
    Ustring fp_format(T t, char mode = 'g', int prec = 6) {
        using namespace std::literals;
        static const Ustring modes = "EeFfGgZz";
        if (modes.find(mode) == npos)
            throw std::invalid_argument("Invalid floating point mode: " + quote(Ustring(1, mode)));
        if (t == 0) {
            switch (mode) {
                case 'E': case 'e':  return prec < 1 ? "0"s + mode + '0' : "0." + Ustring(prec, '0') + mode + "0";
                case 'F': case 'f':  return prec < 1 ? "0"s : "0." + Ustring(prec, '0');
                case 'G': case 'g':  return "0";
                case 'Z': case 'z':  return prec < 2 ? "0"s : "0." + Ustring(prec - 1, '0');
                default:             break;
            }
        }
        Ustring buf(20, '\0'), fmt;
        switch (mode) {
            case 'Z':  fmt = "%#.*LG"; break;
            case 'z':  fmt = "%#.*Lg"; break;
            default:   fmt = "%.*L"s + mode; break;
        }
        auto x = static_cast<long double>(t);
        int rc = 0;
        for (;;) {
            rc = snprintf(&buf[0], buf.size(), fmt.data(), prec, x);
            if (rc < 0)
                throw std::system_error(errno, std::generic_category(), "snprintf()");
            if (size_t(rc) < buf.size())
                break;
            buf.resize(2 * buf.size());
        }
        buf.resize(rc);
        if (mode != 'F' && mode != 'f') {
            size_t p = buf.find_first_of("Ee");
            if (p == npos)
                p = buf.size();
            if (buf[p - 1] == '.') {
                buf.erase(p - 1, 1);
                --p;
            }
            if (p < buf.size()) {
                ++p;
                if (buf[p] == '+')
                    buf.erase(p, 1);
                else if (buf[p] == '-')
                    ++p;
                size_t q = std::min(buf.find_first_not_of('0', p), buf.size() - 1);
                if (q > p)
                    buf.erase(p, q - p);
            }
        }
        return buf;
    }

    template <typename T>
    Ustring opt_fp_format(T t, char mode = 'g', int prec = 6) {
        if constexpr (std::is_floating_point_v<T>) {
            return fp_format(t, mode, prec);
        } else {
            (void)mode;
            (void)prec;
            using RS::to_str;
            return to_str(t);
        }
    }

    template <typename S>
    auto make_view(const S& s, size_t pos = 0, size_t len = npos) noexcept {
        using C = std::decay_t<decltype(s[0])>;
        using SV = std::basic_string_view<C>;
        SV view(s);
        if (pos == 0 && len == npos)
            return view;
        pos = std::min(pos, view.size());
        len = std::min(len, view.size() - pos);
        return SV(view.data() + pos, len);
    }

    inline Ustring roman(int n) {
        static constexpr std::pair<int, const char*> table[] = {
            { 900, "CM" }, { 500, "D" }, { 400, "CD" }, { 100, "C" },
            { 90, "XC" }, { 50, "L" }, { 40, "XL" }, { 10, "X" },
            { 9, "IX" }, { 5, "V" }, { 4, "IV" }, { 1, "I" },
        };
        if (n < 1)
            return {};
        Ustring s(size_t(n / 1000), 'M');
        n %= 1000;
        for (auto& t: table) {
            for (int q = n / t.first; q > 0; --q)
                s += t.second;
            n %= t.first;
        }
        return s;
    }

    inline int64_t si_to_int(Uview str) {
        using limits = std::numeric_limits<int64_t>;
        static constexpr const char* prefixes = "KMGTPEZY";
        Ustring s(str);
        char* endp = nullptr;
        errno = 0;
        int64_t n = std::strtoll(s.data(), &endp, 10);
        if (errno == ERANGE)
            throw std::range_error("Out of range: " + quote(s));
        if (errno || endp == s.data())
            throw std::invalid_argument("Invalid number: " + quote(s));
        if (ascii_isspace(*endp))
            ++endp;
        if (n && ascii_isalpha(*endp)) {
            auto pp = std::strchr(prefixes, ascii_toupper(*endp));
            if (pp) {
                int64_t steps = pp - prefixes + 1;
                double limit = std::log10(double(limits::max()) / double(std::abs(n))) / 3;
                if (double(steps) > limit)
                    throw std::range_error("Out of range: " + quote(s));
                for (int64_t i = 0; i < steps; ++i)
                    n *= 1000;
            }
        }
        return n;
    }

    inline double si_to_float(Uview str) {
        using limits = std::numeric_limits<double>;
        static constexpr const char* prefixes = "yzafpnum kMGTPEZY";
        Ustring s(str);
        char* endp = nullptr;
        errno = 0;
        double x = std::strtod(s.data(), &endp);
        if (errno == ERANGE)
            throw std::range_error("Out of range: " + quote(s));
        if (errno || endp == s.data())
            throw std::invalid_argument("Invalid number: " + quote(s));
        if (ascii_isspace(*endp))
            ++endp;
        char c = *endp;
        if (x != 0 && ascii_isalpha(c)) {
            if (c == 'K')
                c = 'k';
            auto pp = std::strchr(prefixes, c);
            if (pp) {
                auto steps = pp - prefixes - 8;
                double limit = std::log10(limits::max() / fabs(x)) / 3;
                if (double(steps) > limit)
                    throw std::range_error("Out of range: " + quote(s));
                x *= std::pow(1000.0, double(steps));
            }
        }
        return x;
    }

    inline Ustring unqualify(Uview str, Uview delims = ".:") {
        if (delims.empty())
            return Ustring(str);
        size_t pos = str.find_last_of(delims);
        if (pos == npos)
            return Ustring(str);
        else
            return Ustring(str.substr(pos + 1, npos));
    }

    template <typename Range>
    Ustring format_list(const Range& r, std::string_view prefix, std::string_view delimiter, std::string_view suffix) {
        Ustring s(prefix);
        for (auto&& t: r) {
            s += to_str(t);
            s += delimiter;
        }
        if (s.size() > prefix.size())
            s.resize(s.size() - delimiter.size());
        s += suffix;
        return s;

    }

    template <typename Range>
    Ustring format_list(const Range& r) {
        return format_list(r, "[", ",", "]");
    }

    template <typename Range>
    Ustring format_map(const Range& r, std::string_view prefix, std::string_view infix, std::string_view delimiter, std::string_view suffix) {
        Ustring s(prefix);
        for (auto&& kv: r) {
            s += to_str(kv.first);
            s += infix;
            s += to_str(kv.second);
            s += delimiter;
        }
        if (s.size() > prefix.size())
            s.resize(s.size() - delimiter.size());
        s += suffix;
        return s;

    }

    template <typename Range>
    Ustring format_map(const Range& r) {
        return format_map(r, "{", ":", ",", "}");
    }

    // Type names

    inline std::string demangle(const std::string& name) {
        #ifdef __GNUC__
            auto mangled = name;
            std::shared_ptr<char> demangled;
            int status = 0;
            for (;;) {
                if (mangled.empty())
                    return name;
                demangled.reset(abi::__cxa_demangle(mangled.data(), nullptr, nullptr, &status), free);
                if (status == -1)
                    throw std::bad_alloc();
                if (status == 0 && demangled)
                    return demangled.get();
                if (mangled[0] != '_')
                    return name;
                mangled.erase(0, 1);
            }
        #else
            return name;
        #endif
    }

    inline std::string type_name(const std::type_info& t) { return demangle(t.name()); }
    inline std::string type_name(const std::type_index& t) { return demangle(t.name()); }
    template <typename T> std::string type_name(const T& t) { return type_name(typeid(t)); }

    template <typename T>
    std::string type_name() {
        static const std::string name = type_name(typeid(T));
        return name;
    }

    // String conversions

    namespace RS_Detail {

        template <typename T> using InputOperatorArchetype = decltype(std::declval<std::istream&>() >> std::declval<T&>());
        template <typename T> using OutputOperatorArchetype = decltype(std::declval<std::ostream&>() << std::declval<T>());
        template <typename T> using StrMethodArchetype = decltype(std::declval<T>().str());
        template <typename T> using AdlToStringArchetype = decltype(to_string(std::declval<T>()));
        template <typename T> using StdToStringArchetype = decltype(std::to_string(std::declval<T>()));
        template <typename T> using StrToEnumArchetype = decltype(str_to_enum(std::string_view(), std::declval<T&>()));

        template <typename T> struct IsByteArray: public std::false_type {};
        template <typename A> struct IsByteArray<std::vector<unsigned char, A>>: public std::true_type {};
        template <size_t N> struct IsByteArray<std::array<unsigned char, N>>: public std::true_type {};

        template <typename T> struct IsOptional: public std::false_type {};
        template <typename T> struct IsOptional<std::optional<T>>: public std::true_type {};

        template <typename T> struct IsSharedPtr: public std::false_type {};
        template <typename T> struct IsSharedPtr<std::shared_ptr<T>>: public std::true_type {};

        template <typename T> struct IsUniquePtr: public std::false_type {};
        template <typename T, typename D> struct IsUniquePtr<std::unique_ptr<T, D>>: public std::true_type {};

        template <typename T> struct IsPair: public std::false_type {};
        template <typename T1, typename T2> struct IsPair<std::pair<T1, T2>>: public std::true_type {};

        template <typename T> struct IsTuple: public std::false_type {};
        template <typename... TS> struct IsTuple<std::tuple<TS...>>: public std::true_type {};

        template <size_t I, typename... TS>
        void append_tuple(const std::tuple<TS...>& t, std::string& s) {
            if constexpr (I < sizeof...(TS)) {
                s += to_str(std::get<I>(t));
                if constexpr (I + 1 < sizeof...(TS)) {
                    s += ',';
                    append_tuple<I + 1>(t, s);
                }
            }
        }

    }

    template <typename T>
    bool from_str(std::string_view view, T& t) noexcept {
        using namespace RS_Detail;
        try {
            if (view.empty()) {
                t = T();
                return true;
            }
            if constexpr (std::is_same_v<T, bool>) {
                if (view == "true") {
                    t = true;
                    return true;
                } else if (view == "false") {
                    t = false;
                    return true;
                } else {
                    intmax_t x = 0;
                    if (! from_str(view, x))
                        return false;
                    t = bool(x);
                    return true;
                }
            } else if constexpr (std::is_arithmetic_v<T>) {
                std::string str(view);
                auto begin = str.data(), end = begin + str.size();
                int base = 10;
                (void)base;
                if constexpr (std::is_integral_v<T>)
                    if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
                        base = 16;
                char* stop = nullptr;
                T rc = T();
                errno = 0;
                if constexpr (std::is_floating_point_v<T>) {
                    if constexpr (sizeof(T) <= sizeof(float))
                        rc = T(std::strtof(begin, &stop));
                    else if constexpr (sizeof(T) <= sizeof(double))
                        rc = T(std::strtod(begin, &stop));
                    else
                        rc = T(std::strtold(begin, &stop));
                } else if constexpr (std::is_signed_v<T>) {
                    if constexpr (sizeof(T) <= sizeof(long))
                        rc = T(std::strtol(begin, &stop, base));
                    else
                        rc = T(std::strtoll(begin, &stop, base));
                } else {
                    if constexpr (sizeof(T) <= sizeof(long))
                        rc = T(std::strtoul(begin, &stop, base));
                    else
                        rc = T(std::strtoull(begin, &stop, base));
                }
                if (errno != 0 || stop != end)
                    return false;
                t = rc;
                return true;
            } else if constexpr (Meta::is_detected<StrToEnumArchetype, T>) {
                return str_to_enum(view, t);
            } else if constexpr (std::is_constructible_v<T, std::string_view>) {
                t = static_cast<T>(view);
                return true;
            } else if constexpr (std::is_constructible_v<T, std::string>) {
                std::string str(view);
                t = static_cast<T>(str);
                return true;
            } else if constexpr (std::is_constructible_v<T, const char*>) {
                std::string str(view);
                t = static_cast<T>(str.data());
                return true;
            } else if constexpr (Meta::is_detected<InputOperatorArchetype, T>) {
                std::string str(view);
                std::istringstream in(str);
                T temp;
                in >> temp;
                if (! in)
                    return false;
                t = std::move(temp);
                return true;
            } else {
                return false;
            }
        }
        catch (...) {
            return false;
        }
    }

    template <typename T>
    T from_str(std::string_view view) {
        T t;
        if (! from_str(view, t))
            throw std::invalid_argument("Invalid conversion: " + quote(view) + " => " + type_name<T>());
        return t;
    }

    template <typename T>
    std::string to_str(const T& t) {
        using namespace RS_Detail;
        using namespace std::literals;
        if constexpr (std::is_same_v<T, bool>) {
            return t ? "true"s : "false"s;
        } else if constexpr (std::is_same_v<T, char>) {
            return std::string(1, t);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return t;
        } else if constexpr (std::is_same_v<T, std::string_view>) {
            return std::string(t);
        } else if constexpr (std::is_same_v<T, char*> || std::is_same_v<T, const char*>) {
            return cstr(t);
        } else if constexpr (IsByteArray<T>::value) {
            static constexpr const char* digits = "0123456789abcdef";
            size_t n = t.size();
            std::string s;
            s.reserve(2 * n);
            for (size_t i = 0; i < n; ++i) {
                s += digits[t[i] >> 4];
                s += digits[t[i] & 15];
                s += ' ';
            }
            if (! s.empty())
                s.pop_back();
            return s;
        } else if constexpr (std::is_integral_v<T>) {
            return std::to_string(t);
        } else if constexpr (std::is_floating_point_v<T>) {
            return fp_format(t);
        } else if constexpr (Meta::is_detected_convertible<std::string, StrMethodArchetype, T>) {
            return t.str();
        } else if constexpr (Meta::is_detected_convertible<std::string, AdlToStringArchetype, T>) {
            return to_string(t);
        } else if constexpr (Meta::is_detected_convertible<std::string, StdToStringArchetype, T>) {
            return std::to_string(t);
        } else if constexpr (std::is_constructible_v<std::string, T>) {
            return static_cast<std::string>(t);
        } else if constexpr (std::is_constructible_v<std::string_view, T>) {
            return std::string(static_cast<std::string_view>(t));
        } else if constexpr (std::is_constructible_v<const char*, T>) {
            return cstr(static_cast<const char*>(t));
        } else if constexpr (std::is_base_of_v<std::exception, T>) {
            return t.what();
        } else if constexpr (IsOptional<T>::value || IsSharedPtr<T>::value || IsUniquePtr<T>::value) {
            if (t)
                return to_str(*t);
            else
                return "null"s;
        } else if constexpr (IsPair<T>::value) {
            std::string s = "(";
            s += to_str(t.first);
            s += ',';
            s += to_str(t.second);
            s += ')';
            return s;
        } else if constexpr (IsTuple<T>::value) {
            std::string s = "(";
            append_tuple<0>(t, s);
            s += ')';
            return s;
        } else if constexpr (Meta::is_range<T>) {
            if constexpr (IsPair<Meta::RangeValue<T>>::value) {
                std::string s = "{";
                for (auto& [k,v]: t) {
                    s += to_str(k);
                    s += ':';
                    s += to_str(v);
                    s += ',';
                }
                if (s.size() > 1)
                    s.pop_back();
                s += '}';
                return s;
            } else {
                std::string s = "[";
                for (auto& x: t) {
                    s += to_str(x);
                    s += ',';
                }
                if (s.size() > 1)
                    s.pop_back();
                s += ']';
                return s;
            }
        } else if constexpr (Meta::is_detected<OutputOperatorArchetype, T>) {
            std::ostringstream out;
            out << t;
            return out.str();
        } else {
            return type_name(t);
        }
    }

    template <typename T>
    struct FromStr {
        T operator()(std::string_view s) const { using RS::from_str; return from_str<T>(s); }
    };

    struct ToStr {
        template <typename T> std::string operator()(const T& t) const { using RS::to_str; return to_str(t); }
    };

    // Version numbers

    class Version:
    public LessThanComparable<Version> {
    public:
        using value_type = unsigned;
        Version() noexcept {}
        template <typename... Args> Version(unsigned n, Args... args) { append(n, args...); trim(); }
        explicit Version(const Ustring& s);
        unsigned operator[](size_t i) const noexcept { return i < ver.size() ? ver[i] : 0; }
        const unsigned* begin() const noexcept { return ver.data(); }
        const unsigned* end() const noexcept { return ver.data() + ver.size(); }
        unsigned major() const noexcept { return (*this)[0]; }
        unsigned minor() const noexcept { return (*this)[1]; }
        unsigned patch() const noexcept { return (*this)[2]; }
        size_t size() const noexcept { return std::max(ver.size(), size_t(1)); }
        Ustring str(size_t min_elements = 2, char delimiter = '.') const;
        Ustring suffix() const { return suf; }
        uint32_t to32() const noexcept;
        static Version from32(uint32_t n) noexcept;
        friend bool operator==(const Version& lhs, const Version& rhs) noexcept { return lhs.compare(rhs) == 0; }
        friend bool operator<(const Version& lhs, const Version& rhs) noexcept { return lhs.compare(rhs) < 0; }
        friend std::ostream& operator<<(std::ostream& o, const Version& v) { return o << v.str(); }
        friend Ustring to_str(const Version& v) { return v.str(); }
    private:
        std::vector<unsigned> ver;
        Ustring suf;
        template <typename... Args> void append(unsigned n, Args... args) { ver.push_back(n); append(args...); }
        void append(const Ustring& s) { suf = s; }
        void append() {}
        int compare(const Version& v) const noexcept;
        void trim() { while (! ver.empty() && ver.back() == 0) ver.pop_back(); }
        static bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
        static bool is_space(char c) noexcept { return (c >= '\t' && c <= '\r') || c == ' '; }
    };

        inline Version::Version(const Ustring& s) {
            auto i = s.begin(), j = i, end = s.end();
            while (i != end) {
                j = std::find_if_not(i, end, is_digit);
                if (i == j)
                    break;
                Ustring part(i, j);
                ver.push_back(unsigned(std::strtoul(part.data(), nullptr, 10)));
                i = j;
                if (i == end || *i != '.')
                    break;
                ++i;
                if (i == end || ! is_digit(*i))
                    break;
            }
            j = std::find_if(i, end, is_space);
            suf.assign(i, j);
            trim();
        }

        inline Ustring Version::str(size_t min_elements, char delimiter) const {
            Ustring s;
            for (auto& v: ver) {
                s += std::to_string(v);
                s += delimiter;
            }
            for (size_t i = ver.size(); i < min_elements; ++i) {
                s += '0';
                s += delimiter;
            }
            if (! s.empty())
                s.pop_back();
            s += suf;
            return s;
        }

        inline uint32_t Version::to32() const noexcept {
            uint32_t v = 0;
            for (size_t i = 0; i < 4; ++i)
                v = (v << 8) | ((*this)[i] & 0xff);
            return v;
        }

        inline Version Version::from32(uint32_t n) noexcept {
            Version v;
            for (int i = 24; i >= 0 && n != 0; i -= 8)
                v.ver.push_back((n >> i) & 0xff);
            v.trim();
            return v;
        }

        inline int Version::compare(const Version& v) const noexcept {
            int c = compare_3way(ver, v.ver);
            if (c)
                return c;
            if (suf.empty() != v.suf.empty())
                return int(suf.empty()) - int(v.suf.empty());
            else
                return compare_3way(suf, v.suf);
        }

}
