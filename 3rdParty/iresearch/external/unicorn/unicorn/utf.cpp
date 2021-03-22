#include "unicorn/utf.hpp"

using namespace std::literals;

namespace RS::Unicorn {

    namespace {

        constexpr auto not_unicode = char32_t(-1);

    }

    namespace UnicornDetail {

        //  UTF-8 byte distribution:
        //      00-7f = Single byte character
        //      80-bf = Second or later byte of a multibyte character
        //      c0-c1 = Not allowed
        //      c2-df = First byte of a 2-byte character
        //      e0-ef = First byte of a 3-byte character
        //      f0-f4 = First byte of a 4-byte character
        //      f5-ff = Not allowed

        size_t UtfEncoding<char>::decode(const char* src, size_t n, char32_t& dst) noexcept {
            dst = not_unicode;
            auto code = reinterpret_cast<const uint8_t*>(src);
            if (code[0] <= 0x7f)
                dst = code[0];
            if (code[0] <= 0xc1 || code[0] >= 0xf5 || n < 2)
                return 1;
            switch (code[0]) {
                case 0xe0:  if (code[1] < 0xa0 || code[1] > 0xbf) return 1; break;
                case 0xed:  if (code[1] < 0x80 || code[1] > 0x9f) return 1; break;
                case 0xf0:  if (code[1] < 0x90 || code[1] > 0xbf) return 1; break;
                case 0xf4:  if (code[1] < 0x80 || code[1] > 0x8f) return 1; break;
                default:    if (code[1] < 0x80 || code[1] > 0xbf) return 1; break;
            }
            if (code[0] <= 0xdf) {
                dst = (char32_t(code[0] & 0x1f) << 6)
                    | char32_t(code[1] & 0x3f);
                return 2;
            } else if (code[0] <= 0xef) {
                if (n < 3 || code[2] < 0x80 || code[2] > 0xbf)
                    return 2;
                dst = (char32_t(code[0] & 0x0f) << 12)
                    | (char32_t(code[1] & 0x3f) << 6)
                    | char32_t(code[2] & 0x3f);
                return 3;
            } else {
                if (n < 3 || code[2] < 0x80 || code[2] > 0xbf)
                    return 2;
                else if (n < 4 || code[3] < 0x80 || code[3] > 0xbf)
                    return 3;
                dst = (char32_t(code[0] & 0x07) << 18)
                    | (char32_t(code[1] & 0x3f) << 12)
                    | (char32_t(code[2] & 0x3f) << 6)
                    | char32_t(code[3] & 0x3f);
                return 4;
            }
        }

        size_t UtfEncoding<char>::decode_prev(const char* src, size_t pos, char32_t& dst) noexcept {
            auto code = reinterpret_cast<const uint8_t*>(src);
            size_t start = pos - 1;
            while (start > 0 && code[start] >= 0x80 && code[start] <= 0xbf)
                --start;
            do {
                if (decode(src + start, pos - start, dst) == pos - start)
                    break;
                ++start;
            } while (pos - start > 1);
            return pos - start;
        }

        size_t UtfEncoding<char>::encode(char32_t src, char* dst) noexcept {
            auto code = reinterpret_cast<uint8_t*>(dst);
            if (src <= 0x7f) {
                code[0] = uint8_t(src);
                return 1;
            } else if (src <= 0x7ff) {
                code[0] = 0xc0 | uint8_t(src >> 6);
                code[1] = 0x80 | uint8_t(src & 0x3f);
                return 2;
            } else if (src <= 0xffff) {
                code[0] = 0xe0 | uint8_t(src >> 12);
                code[1] = 0x80 | uint8_t((src >> 6) & 0x3f);
                code[2] = 0x80 | uint8_t(src & 0x3f);
                return 3;
            } else {
                code[0] = 0xf0 | uint8_t(src >> 18);
                code[1] = 0x80 | uint8_t((src >> 12) & 0x3f);
                code[2] = 0x80 | uint8_t((src >> 6) & 0x3f);
                code[3] = 0x80 | uint8_t(src & 0x3f);
                return 4;
            }
        }

        size_t UtfEncoding<char16_t>::decode(const char16_t* src, size_t n, char32_t& dst) noexcept {
            if (! char_is_surrogate(src[0])) {
                dst = src[0];
                return 1;
            } else if (n >= 2 && char_is_high_surrogate(src[0]) && char_is_low_surrogate(src[1])) {
                dst = 0x10000 + ((char32_t(src[0]) & 0x3ff) << 10) + (char32_t(src[1]) & 0x3ff);
                return 2;
            } else {
                dst = not_unicode;
                return 1;
            }
        }

        size_t UtfEncoding<char16_t>::decode_prev(const char16_t* src, size_t pos, char32_t& dst) noexcept {
            if (char_is_unicode(src[pos - 1])) {
                dst = src[pos - 1];
                return 1;
            } else if (pos >= 2 && char_is_high_surrogate(src[pos - 2]) && char_is_low_surrogate(src[pos - 1])) {
                dst = 0x10000 + ((char32_t(src[pos - 2]) & 0x3ff) << 10) + (char32_t(src[pos - 1]) & 0x3ff);
                return 2;
            } else {
                dst = not_unicode;
                return 1;
            }
        }

        size_t UtfEncoding<char16_t>::encode(char32_t src, char16_t* dst) noexcept {
            if (char_is_bmp(src)) {
                dst[0] = char16_t(src);
                return 1;
            } else {
                dst[0] = first_high_surrogate_char + char16_t((src >> 10) - 0x40);
                dst[1] = first_low_surrogate_char + char16_t(src & 0x3ff);
                return 2;
            }
        }

    }

    // Exceptions

    Ustring EncodingError::prefix(const Ustring& encoding, size_t offset) {
        Ustring s = "Encoding error";
        if (! encoding.empty())
            s += " (" + encoding + ")";
        if (offset > 0)
            s += "; offset " + std::to_string(offset);
        return s;
    }

    // Single character functions

    size_t char_from_utf8(const char* src, size_t n, char32_t& dst) noexcept {
        char32_t c = 0;
        size_t units = UnicornDetail::UtfEncoding<char>::decode(src, n, c);
        if (c == not_unicode)
            return 0;
        dst = c;
        return units;
    }

    size_t char_from_utf16(const char16_t* src, size_t n, char32_t& dst) noexcept {
        char32_t c = 0;
        size_t units = UnicornDetail::UtfEncoding<char16_t>::decode(src, n, c);
        if (c == not_unicode)
            return 0;
        dst = c;
        return units;
    }

    size_t char_to_utf8(char32_t src, char* dst) noexcept {
        if (! char_is_unicode(src))
            return 0;
        else
            return UnicornDetail::UtfEncoding<char>::encode(src, dst);
    }

    size_t char_to_utf16(char32_t src, char16_t* dst) noexcept {
        if (! char_is_unicode(src))
            return 0;
        else
            return UnicornDetail::UtfEncoding<char16_t>::encode(src, dst);
    }

}
