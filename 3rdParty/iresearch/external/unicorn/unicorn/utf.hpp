#pragma once

#include "unicorn/character.hpp"
#include "unicorn/utility.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace RS::Unicorn {

    // Constants

    // Remember that any other set of flags that might be combined with these
    // needs to skip the bits that are already spoken for.

    struct Utf {

        static constexpr uint32_t ignore = 1;   // Assume valid UTF input
        static constexpr uint32_t replace = 2;  // Replace invalid UTF with U+FFFD
        static constexpr uint32_t throws = 4;   // Throw EncodingError on invalid UTF
        static constexpr uint32_t mask = ignore | replace | throws;

    };

    namespace UnicornDetail {

        // UtfEncoding::decode() reads one encoded character from src, writes
        // it to dst, and returns the number of code units consumed. n is the
        // maximum number of code units available. On failure, it sets dst to
        // an invalid character, and returns the minimum number of code units
        // to skip (at least 1). It can assume src!=nullptr and n>0.

        // UtfEncoding::decode_fast() does the same as decode(), but without
        // checking for valid Unicode.

        // UtfEncoding::decode_prev() reads the encoded character immediately
        // preceding src+pos, writes it to dst, and returns the number of code
        // units stepped back. On failure, it sets dst to an invalid
        // character, and returns the start of the invalid code sequence. It
        // can assume src!=nullptr and pos>0.

        // UtfEncoding::encode() encodes the character in src and writes it to
        // dst. It can assume char_is_unicode(src), dst!=nullptr, and enough
        // room for max_units.

        template <typename C> struct UtfEncoding;

        template <>
        struct UtfEncoding<char> {
            static constexpr size_t max_units = 4;
            static size_t decode(const char* src, size_t n, char32_t& dst) noexcept;
            static size_t decode_fast(const char* src, size_t n, char32_t& dst) noexcept;
            static size_t decode_prev(const char* src, size_t pos, char32_t& dst) noexcept;
            static size_t encode(char32_t src, char* dst) noexcept;
            static const char* name() noexcept { return "UTF-8"; }
        };

        inline size_t UtfEncoding<char>::decode_fast(const char* src, size_t n, char32_t& dst) noexcept {
            auto code = reinterpret_cast<const uint8_t*>(src);
            if (code[0] <= 0xc1 || n < 2) {
                dst = code[0];
                return 1;
            } else if (code[0] <= 0xdf || n < 3) {
                dst = (char32_t(code[0] & 0x1f) << 6)
                    | char32_t(code[1] & 0x3f);
                return 2;
            } else if (code[0] <= 0xef || n < 4) {
                dst = (char32_t(code[0] & 0x0f) << 12)
                    | (char32_t(code[1] & 0x3f) << 6)
                    | char32_t(code[2] & 0x3f);
                return 3;
            } else {
                dst = (char32_t(code[0] & 0x07) << 18)
                    | (char32_t(code[1] & 0x3f) << 12)
                    | (char32_t(code[2] & 0x3f) << 6)
                    | char32_t(code[3] & 0x3f);
                return 4;
            }
        }


        template <>
        struct UtfEncoding<char16_t> {
            static constexpr size_t max_units = 2;
            static size_t decode(const char16_t* src, size_t n, char32_t& dst) noexcept;
            static size_t decode_fast(const char16_t* src, size_t n, char32_t& dst) noexcept;
            static size_t decode_prev(const char16_t* src, size_t pos, char32_t& dst) noexcept;
            static size_t encode(char32_t src, char16_t* dst) noexcept;
            static const char* name() noexcept { return "UTF-16"; }
        };

        inline size_t UtfEncoding<char16_t>::decode_fast(const char16_t* src, size_t n, char32_t& dst) noexcept {
            if (n >= 2 && char_is_surrogate(src[0])) {
                dst = 0x10000 + ((char32_t(src[0]) & 0x3ff) << 10) + (char32_t(src[1]) & 0x3ff);
                return 2;
            } else {
                dst = src[0];
                return 1;
            }
        }

        template <>
        struct UtfEncoding<char32_t> {
            static constexpr size_t max_units = 1;
            static size_t decode(const char32_t* src, size_t /*n*/, char32_t& dst) noexcept { dst = *src; return 1; }
            static size_t decode_fast(const char32_t* src, size_t /*n*/, char32_t& dst) noexcept { dst = *src; return 1; }
            static size_t decode_prev(const char32_t* src, size_t pos, char32_t& dst) noexcept { dst = src[pos - 1]; return 1; }
            static size_t encode(char32_t src, char32_t* dst) noexcept { *dst = src; return 1; }
            static const char* name() noexcept { return "UTF-32"; }
        };

        template <>
        struct UtfEncoding<wchar_t> {
            using utf = UtfEncoding<WcharEquivalent>;
            static constexpr size_t max_units = utf::max_units;
            static size_t decode(const wchar_t* src, size_t n, char32_t& dst) noexcept { return utf::decode(reinterpret_cast<const WcharEquivalent*>(src), n, dst); }
            static size_t decode_fast(const wchar_t* src, size_t n, char32_t& dst) noexcept { return utf::decode_fast(reinterpret_cast<const WcharEquivalent*>(src), n, dst); }
            static size_t decode_prev(const wchar_t* src, size_t pos, char32_t& dst) noexcept { return utf::decode_prev(reinterpret_cast<const WcharEquivalent*>(src), pos, dst); }
            static size_t encode(char32_t src, wchar_t* dst) noexcept { return utf::encode(src, reinterpret_cast<WcharEquivalent*>(dst)); }
            static const char* name() noexcept { return "wchar_t"; }
        };

        template <typename C> inline void append_error(std::basic_string<C>& str) { str += static_cast<C>(replacement_char); }
        inline void append_error(Ustring& str) { str += utf8_replacement; }

    }

    // Exceptions

    class EncodingError:
    public std::runtime_error {
    public:
        EncodingError(): std::runtime_error(prefix({}, 0)), enc(), ofs(0) {}
        explicit EncodingError(const Ustring& encoding, size_t offset = 0, char32_t c = 0):
            std::runtime_error(prefix(encoding, offset) + hexcode(&c, 1)), enc(std::make_shared<Ustring>(encoding)), ofs(offset) {}
        template <typename C> EncodingError(const Ustring& encoding, size_t offset, const C* ptr, size_t n = 1):
            std::runtime_error(prefix(encoding, offset) + hexcode(ptr, n)), enc(std::make_shared<Ustring>(encoding)), ofs(offset) {}
        const char* encoding() const noexcept { static const char c = 0; return enc ? enc->data() : &c; }
        size_t offset() const noexcept { return ofs; }
    private:
        std::shared_ptr<Ustring> enc;
        size_t ofs;
        static Ustring prefix(const Ustring& encoding, size_t offset);
        template <typename C> static Ustring hexcode(const C* ptr, size_t n);
    };

        template <typename C>
        Ustring EncodingError::hexcode(const C* ptr, size_t n) {
            using utype = std::make_unsigned_t<C>;
            if (! ptr || ! n)
                return {};
            Ustring s = "; hex";
            auto uptr = reinterpret_cast<const utype*>(ptr);
            for (size_t i = 0; i < n; ++i)
                s += ' ' + hex(uptr[i]);
            return s;
        }

    // Single character functions

    size_t char_from_utf8(const char* src, size_t n, char32_t& dst) noexcept;
    size_t char_from_utf16(const char16_t* src, size_t n, char32_t& dst) noexcept;
    size_t char_to_utf8(char32_t src, char* dst) noexcept;
    size_t char_to_utf16(char32_t src, char16_t* dst) noexcept;

    template <typename C>
    size_t code_units(char32_t c) {
        if (! char_is_unicode(c))
            return 0;
        else if constexpr (sizeof(C) == 1)
            return c <= 0x7f ? 1 : c <= 0x7ff ? 2 : c <= 0xffff ? 3 : 4;
        else if constexpr (sizeof(C) == 2)
            return c <= 0xffff ? 1 : 2;
        else
            return 1;
    }

    template <typename C>
    bool is_single_unit(C c) {
        if constexpr (sizeof(C) == 1)
            return uint8_t(c) <= 0x7f;
        else
            return char_is_unicode(c);
    }

    template <typename C>
    bool is_start_unit(C c) {
        return (sizeof(C) == 1 && uint8_t(c) >= 0xc2 && uint8_t(c) <= 0xf4)
            || (sizeof(C) == 2 && char_is_high_surrogate(c));
    }

    template <typename C>
    bool is_nonstart_unit(C c) {
        return (sizeof(C) == 1 && uint8_t(c) >= 0x80 && uint8_t(c) <= 0xbf)
            || (sizeof(C) == 2 && char_is_low_surrogate(c));
    }

    template <typename C>
    bool is_invalid_unit(C c) {
        return (sizeof(C) == 1 && ((uint8_t(c) >= 0xc0 && uint8_t(c) <= 0xc1) || uint8_t(c) >= 0xf5))
            || (sizeof(C) == 4 && ! char_is_unicode(c));
    }

    template <typename C>
    bool is_initial_unit(C c) {
        return is_single_unit(c) || is_start_unit(c);
    }

    // UTF decoding iterator

    template <typename C>
    class UtfIterator:
    public BidirectionalIterator<UtfIterator<C>, const char32_t> {
    public:
        using code_unit = C;
        using string_type = std::basic_string<C>;
        using view_type = std::basic_string_view<C>;
        UtfIterator() noexcept { static const string_type dummy; sptr = dummy; }
        UtfIterator(view_type src, size_t offset = 0, uint32_t flags = 0):
            sptr(src), ofs(std::min(offset, src.size())), fset(flags) { if (popcount(fset & Utf::mask) == 0) fset |= Utf::ignore; ++*this; }
        const char32_t& operator*() const noexcept { return u; }
        UtfIterator& operator++();
        UtfIterator& operator--();
        size_t count() const noexcept { return units; }
        size_t offset() const noexcept { return ofs; }
        UtfIterator offset_by(ptrdiff_t n) const noexcept;
        const C* ptr() const noexcept { return !sptr.empty() ? sptr.data() + ofs : nullptr; }
        const view_type& source() const noexcept { return sptr; }
        string_type str() const { return !sptr.empty() ? string_type(sptr.substr(ofs, units)) : string_type(); }
        bool valid() const noexcept { return ok; }
        view_type view() const noexcept {
          if (!sptr.empty()) {
            return {sptr.data() + ofs, units};
          } else {
            return {};
          }
        }
        friend bool operator==(const UtfIterator& lhs, const UtfIterator& rhs) noexcept { return lhs.ofs == rhs.ofs; }
    private:
        view_type sptr;                     // Source string
        size_t ofs = 0;                     // Offset of current character in source
        size_t units = 0;                   // Code units in current character
        char32_t u = 0;                     // Current decoded character
        uint32_t fset = Utf::ignore;        // Error handling flag
        bool ok = false;                    // Current character is valid
    };

    template <typename C>
    UtfIterator<C>& UtfIterator<C>::operator++() {
        using namespace UnicornDetail;
        ofs = std::min(ofs + units, sptr.size());
        units = 0;
        u = 0;
        ok = false;
        if (ofs == sptr.size()) {
            // do nothing
        } else if (fset & Utf::ignore) {
            units = UtfEncoding<C>::decode_fast(sptr.data() + ofs, sptr.size() - ofs, u);
            ok = true;
        } else {
            units = UtfEncoding<C>::decode(sptr.data() + ofs, sptr.size() - ofs, u);
            ok = char_is_unicode(u);
            if (! ok) {
                u = replacement_char;
                if (fset & Utf::throws)
                    throw EncodingError(UtfEncoding<C>::name(), ofs, sptr.data() + ofs, units);
            }
        }
        return *this;
    }

    template <typename C>
    UtfIterator<C>& UtfIterator<C>::operator--() {
        using namespace UnicornDetail;
        units = 0;
        u = 0;
        ok = false;
        if (ofs == 0)
            return *this;
        units = UtfEncoding<C>::decode_prev(sptr.data(), ofs, u);
        ofs -= units;
        ok = (fset & Utf::ignore) || char_is_unicode(u);
        if (! ok) {
            u = replacement_char;
            if (fset & Utf::throws)
                throw EncodingError(UtfEncoding<C>::name(), ofs, sptr.data() + ofs, units);
        }
        return *this;
    }

    template <typename C>
    UtfIterator<C> UtfIterator<C>::offset_by(ptrdiff_t n) const noexcept {
        if (sptr.empty())
            return *this;
        auto i = *this;
        i.ofs = std::clamp(size_t(ptrdiff_t(ofs) + n), size_t(0), sptr.size());
        i.units = 0;
        ++i;
        return i;
    }

    using Utf8Iterator = UtfIterator<char>;
    using Utf8Range = Irange<Utf8Iterator>;
    using Utf16Iterator = UtfIterator<char16_t>;
    using Utf16Range = Irange<Utf16Iterator>;
    using Utf32Iterator = UtfIterator<char32_t>;
    using Utf32Range = Irange<Utf32Iterator>;
    using WcharIterator = UtfIterator<wchar_t>;
    using WcharRange = Irange<WcharIterator>;

    template <typename container>
    UtfIterator<typename std::decay<container>::type::value_type> utf_begin(container&& src, uint32_t flags = 0) {
        return {std::forward<container>(src), 0, flags};
    }

    template <typename container>
    UtfIterator<typename std::decay<container>::type::value_type> utf_end(container&& src, uint32_t flags = 0) {
        return {std::forward<container>(src), src.size(), flags};
    }

    template <typename container>
    UtfIterator<typename std::decay<container>::type::value_type> utf_iterator(container&& src, size_t offset, uint32_t flags = 0) {
        return {std::forward<container>(src), offset, flags};
    }

    template <typename container>
    Irange<UtfIterator<typename std::decay<container>::type::value_type>> utf_range(container&& src, uint32_t flags = 0) {
        return {utf_begin(std::forward<container>(src), flags), utf_end(std::forward<container>(src), flags)};
    }

    template <typename C>
    std::basic_string<C> u_str(const UtfIterator<C>& i, const UtfIterator<C>& j) {
      return std::basic_string<C>(i.source().substr(i.offset(), j.offset() - i.offset()));
    }

    template <typename C>
    std::basic_string<C> u_str(const Irange<UtfIterator<C>>& range) {
        return u_str(range.begin(), range.end());
    }

    
    template <typename C>
    std::basic_string_view<C> u_view(const UtfIterator<C>& i, const UtfIterator<C>& j) {
      return std::basic_string_view<C>(i.source().substr(i.offset(), j.offset() - i.offset()));
    }

    template <typename C>
    std::basic_string_view<C> u_view(const Irange<UtfIterator<C>>& range) {
        return u_view(range.begin(), range.end());
    }

    // UTF encoding iterator

    template <typename C>
    class UtfWriter:
    public OutputIterator<UtfWriter<C>> {
    public:
        using code_unit = C;
        using string_type = std::basic_string<C>;
        UtfWriter() noexcept {}
        explicit UtfWriter(string_type& dst) noexcept: sptr(&dst) { if (popcount(fset & Utf::mask) == 0) fset |= Utf::ignore; }
        UtfWriter(string_type& dst, uint32_t flags) noexcept: sptr(&dst), fset(flags) { if (popcount(fset & Utf::mask) == 0) fset |= Utf::ignore; }
        UtfWriter& operator=(char32_t u);
        bool valid() const noexcept { return ok; }
    private:
        string_type* sptr = nullptr;  // Destination string
        uint32_t fset = Utf::ignore;  // Error handling flag
        bool ok = false;              // Most recent character is valid
    };

    template <typename C>
    UtfWriter<C>& UtfWriter<C>::operator=(char32_t u) {
        using namespace UnicornDetail;
        if (! sptr)
            return *this;
        bool fast = fset & Utf::ignore;
        auto pos = sptr->size();
        sptr->resize(pos + UtfEncoding<C>::max_units);
        size_t rc = 0;
        if (fast || char_is_unicode(u))
            rc = UtfEncoding<C>::encode(u, &(*sptr)[pos]);
        sptr->resize(pos + rc);
        if (fast) {
            ok = true;
        } else {
            ok = rc > 0;
            if (! ok) {
                if (fset & Utf::throws)
                    throw EncodingError(UtfEncoding<C>::name(), pos, &u);
                else
                    append_error(*sptr);
            }
        }
        return *this;
    }

    using Utf8Writer = UtfWriter<char>;
    using Utf16Writer = UtfWriter<char16_t>;
    using Utf32Writer = UtfWriter<char32_t>;
    using WcharWriter = UtfWriter<wchar_t>;

    template <typename C>
    UtfWriter<C> utf_writer(std::basic_string<C>& dst, uint32_t flags = 0) noexcept {
        return {dst, flags};
    }

    // UTF conversion functions

    namespace UnicornDetail {

        template <typename C1, typename C2>
        struct Recode {
            void operator()(const C1* src, size_t n, std::basic_string<C2>& dst, uint32_t flags) const {
                if (! src)
                    return;
                if (popcount(flags & Utf::mask) == 0)
                    flags |= Utf::ignore;
                if constexpr (sizeof(C1) == sizeof(C2)) {
                    if (flags & Utf::ignore) {
                        dst.resize(dst.size() + n);
                        memcpy(&dst[0] + dst.size() - n, src, n * sizeof(C1));
                        return;
                    }
                }
                size_t pos = 0;
                char32_t u = 0;
                C2 buf[UtfEncoding<C2>::max_units];
                while (pos < n) {
                    auto rc = UtfEncoding<C1>::decode(src + pos, n - pos, u);
                    if (! (flags & Utf::ignore) && ! char_is_unicode(u)) {
                        if (flags & Utf::throws)
                            throw EncodingError(UtfEncoding<C1>::name(), pos, src + pos, rc);
                        u = replacement_char;
                    }
                    pos += rc;
                    rc = UtfEncoding<C2>::encode(u, buf);
                    dst.append(buf, rc);
                }
            }
        };

        template <typename C1>
        struct Recode<C1, char32_t> {
            void operator()(const C1* src, size_t n, std::u32string& dst, uint32_t flags) const {
                if (! src)
                    return;
                if (popcount(flags & Utf::mask) == 0)
                    flags |= Utf::ignore;
                if constexpr (sizeof(C1) == 4) {
                    if (flags & Utf::ignore) {
                        dst.resize(dst.size() + n);
                        memcpy(&dst[0] + dst.size() - n, src, 4 * n);
                        return;
                    }
                }
                size_t pos = 0;
                char32_t u = 0;
                if (flags & Utf::ignore) {
                    while (pos < n) {
                        pos += UtfEncoding<C1>::decode(src + pos, n - pos, u);
                        dst += u;
                    }
                } else {
                    while (pos < n) {
                        auto rc = UtfEncoding<C1>::decode(src + pos, n - pos, u);
                        if (char_is_unicode(u))
                            dst += u;
                        else if (flags & Utf::throws)
                            throw EncodingError(UtfEncoding<C1>::name(), pos, src + pos, rc);
                        else
                            dst += replacement_char;
                        pos += rc;
                    }
                }
            }
        };

        template <typename C2>
        struct Recode<char32_t, C2> {
            void operator()(const char32_t* src, size_t n, std::basic_string<C2>& dst, uint32_t flags) const {
                if (! src)
                    return;
                if (popcount(flags & Utf::mask) == 0)
                    flags |= Utf::ignore;
                if constexpr (sizeof(C2) == 4) {
                    if (flags & Utf::ignore) {
                        dst.resize(dst.size() + n);
                        memcpy(&dst[0] + dst.size() - n, src, 4 * n);
                        return;
                    }
                }
                char32_t u = 0;
                C2 buf[UtfEncoding<C2>::max_units];
                for (size_t pos = 0; pos < n; ++pos) {
                    if (n == npos && src[pos] == 0)
                        break;
                    else if ((flags & Utf::ignore) || char_is_unicode(src[pos]))
                        u = src[pos];
                    else if (flags & Utf::throws)
                        throw EncodingError(UtfEncoding<char32_t>::name(), pos, src + pos);
                    else
                        u = replacement_char;
                    auto rc = UtfEncoding<C2>::encode(u, buf);
                    dst.append(buf, rc);
                }
            }
        };

        template <typename C>
        struct Recode<C, C> {
            void operator()(const C* src, size_t n, std::basic_string<C>& dst, uint32_t flags) const {
                if (! src)
                    return;
                if (popcount(flags & Utf::mask) == 0)
                    flags |= Utf::ignore;
                size_t pos = 0;
                char32_t u = 0;
                C buf[UtfEncoding<C>::max_units];
                if (flags & Utf::ignore) {
                    dst.append(src, n);
                    return;
                }
                while (pos < n) {
                    auto rc = UtfEncoding<C>::decode(src + pos, n - pos, u);
                    if (char_is_unicode(u)) {
                        dst.append(src + pos, rc);
                    } else if (flags & Utf::throws) {
                        throw EncodingError(UtfEncoding<C>::name(), pos, src + pos, rc);
                    } else {
                        auto rc2 = UtfEncoding<C>::encode(replacement_char, buf);
                        dst.append(buf, rc2);
                    }
                    pos += rc;
                }
            }
        };

        template <>
        struct Recode<char32_t, char32_t> {
            void operator()(const char32_t* src, size_t n, std::u32string& dst, uint32_t flags) const {
                if (! src)
                    return;
                if (popcount(flags & Utf::mask) == 0)
                    flags |= Utf::ignore;
                if (flags & Utf::ignore) {
                    dst.append(src, n);
                    return;
                }
                for (size_t pos = 0; pos < n; ++pos) {
                    if (n == npos && src[pos] == 0)
                        break;
                    else if ((flags & Utf::throws) || char_is_unicode(src[pos]))
                        dst += src[pos];
                    else if (flags & Utf::throws)
                        throw EncodingError(UtfEncoding<char32_t>::name(), pos, src + pos);
                    else
                        dst += replacement_char;
                }
            }
        };

    };

    template <typename C1, typename C2>
    void recode(const std::basic_string<C1>& src, std::basic_string<C2>& dst, uint32_t flags = 0) {
        std::basic_string<C2> result;
        UnicornDetail::Recode<C1, C2>()(src.data(), src.size(), result, flags);
        dst = std::move(result);
    }

    template <typename C1, typename C2>
    void recode(const std::basic_string<C1>& src, size_t offset, std::basic_string<C2>& dst, uint32_t flags = 0) {
        std::basic_string<C2> result;
        if (offset < src.size())
            UnicornDetail::Recode<C1, C2>()(src.data() + offset, src.size() - offset, result, flags);
        dst = std::move(result);
    }

    template <typename C1, typename C2>
    void recode(const C1* src, size_t count, std::basic_string<C2>& dst, uint32_t flags = 0) {
        std::basic_string<C2> result;
        UnicornDetail::Recode<C1, C2>()(src, count, result, flags);
        dst = std::move(result);
    }

    template <typename C2, typename C1>
    std::basic_string<C2> recode(const std::basic_string<C1>& src, uint32_t flags = 0) {
        std::basic_string<C2> result;
        UnicornDetail::Recode<C1, C2>()(src.data(), src.size(), result, flags);
        return result;
    }

    template <typename C2, typename C1>
    std::basic_string<C2> recode(const std::basic_string<C1>& src, size_t offset, uint32_t flags) {
        std::basic_string<C2> result;
        if (offset < src.size())
            UnicornDetail::Recode<C1, C2>()(src.data() + offset, src.size() - offset, result, flags);
        return result;
    }

    template <typename C>
    Ustring to_utf8(const std::basic_string<C>& src, uint32_t flags = 0) {
        return recode<char>(src, flags);
    }

    template <typename C>
    std::u16string to_utf16(const std::basic_string<C>& src, uint32_t flags = 0) {
        return recode<char16_t>(src, flags);
    }

    template <typename C>
    std::u32string to_utf32(const std::basic_string<C>& src, uint32_t flags = 0) {
        return recode<char32_t>(src, flags);
    }

    template <typename C>
    std::wstring to_wstring(const std::basic_string<C>& src, uint32_t flags = 0) {
        return recode<wchar_t>(src, flags);
    }

    template <typename C>
    NativeString to_native(const std::basic_string<C>& src, uint32_t flags = 0) {
        return recode<NativeCharacter>(src, flags);
    }

    // UTF validation functions

    template <typename C>
    void check_string(const std::basic_string<C>& str) {
        using namespace UnicornDetail;
        auto data = str.data();
        size_t pos = 0, size = str.size();
        char32_t u = 0;
        while (pos < size) {
            auto rc = UtfEncoding<C>::decode(data + pos, size - pos, u);
            if (! char_is_unicode(u))
                throw EncodingError(UtfEncoding<C>::name(), pos, data + pos, rc);
            pos += rc;
        }
    }

    template <typename C>
    bool valid_string(const std::basic_string<C>& str) noexcept {
        using namespace UnicornDetail;
        auto data = str.data();
        size_t pos = 0, size = str.size();
        char32_t u = 0;
        while (pos < size) {
            auto rc = UtfEncoding<C>::decode(data + pos, size - pos, u);
            if (char_is_unicode(u))
                pos += rc;
            else
                return false;
        }
        return true;
    }

    template <typename C>
    std::basic_string<C> sanitize(const std::basic_string<C>& str) {
        std::basic_string<C> result;
        recode(str, result, Utf::replace);
        return result;
    }

    template <typename C>
    void sanitize_in(std::basic_string<C>& str) {
        std::basic_string<C> result;
        recode(str, result, Utf::replace);
        str = std::move(result);
    }

    template <typename C>
    size_t valid_count(const std::basic_string<C>& str) noexcept {
        using namespace UnicornDetail;
        auto data = str.data();
        size_t pos = 0, size = str.size();
        char32_t u = 0;
        while (pos < size) {
            auto rc = UtfEncoding<C>::decode(data + pos, size - pos, u);
            if (! char_is_unicode(u))
                return pos;
            pos += rc;
        }
        return npos;
    }

}
