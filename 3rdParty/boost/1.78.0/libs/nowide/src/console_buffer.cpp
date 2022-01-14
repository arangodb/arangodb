//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//  Copyright (c) 2020 - 2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_NOWIDE_SOURCE
#include "console_buffer.hpp"
#include <cassert>
#include <cstring>

namespace boost {
namespace nowide {
    namespace detail {
        int console_output_buffer_base::sync()
        {
            return overflow(traits_type::eof());
        }

        int console_output_buffer_base::overflow(int c)
        {
            const auto n = static_cast<int>(pptr() - pbase());
            int r = 0;

            if(n > 0 && (r = write(pbase(), n)) < 0)
                return -1;
            if(r < n)
                std::memmove(pbase(), pbase() + r, n - r);
            setp(buffer_, buffer_ + buffer_size);
            pbump(n - r);
            if(c != traits_type::eof())
                sputc(traits_type::to_char_type(c));
            return 0;
        }

        int console_output_buffer_base::write(const char* p, int n)
        {
            const char* b = p;
            const char* e = p + n;
            // Should be impossible unless someone messes with the pointers
            if(n > buffer_size)
                return -1; // LCOV_EXCL_LINE
            wchar_t* out = wbuffer_;
            utf::code_point c;
            size_t decoded = 0;
            while((c = decoder::decode(p, e)) != utf::incomplete)
            {
                if(c == utf::illegal)
                    c = BOOST_NOWIDE_REPLACEMENT_CHARACTER;
                assert(out - wbuffer_ + encoder::width(c) <= static_cast<int>(wbuffer_size));
                out = encoder::encode(c, out);
                decoded = p - b;
            }
            std::size_t num_chars_written = 0;
            if(!do_write(wbuffer_, static_cast<std::size_t>(out - wbuffer_), num_chars_written))
                return -1;
            return static_cast<int>(decoded);
        }

        //////////////////////////////////////////////////////////////////////////

        int console_input_buffer_base::sync()
        {
            wsize_ = 0;
            was_newline_ = true;
            pback_buffer_.clear();
            setg(0, 0, 0);
            return 0;
        }

        int console_input_buffer_base::pbackfail(int c)
        {
            if(c == traits_type::eof())
                return traits_type::eof();

            if(gptr() != eback())
            {
                gbump(-1);
                *gptr() = traits_type::to_char_type(c);
                return 0;
            }

            if(pback_buffer_.empty())
                pback_buffer_.push_back(traits_type::to_char_type(c));
            else
                pback_buffer_.insert(pback_buffer_.begin(), traits_type::to_char_type(c));

            char* pFirst = pback_buffer_.data();
            char* pLast = pFirst + pback_buffer_.size();
            setg(pFirst, pFirst, pLast);

            return 0;
        }

        int console_input_buffer_base::underflow()
        {
            if(!pback_buffer_.empty())
                pback_buffer_.clear();

            const size_t n = read();
            setg(buffer_, buffer_, buffer_ + n);
            if(n == 0)
                return traits_type::eof();
            return std::char_traits<char>::to_int_type(*gptr());
        }

        size_t console_input_buffer_base::read()
        {
            std::size_t read_wchars = 0;
            if(!do_read(wbuffer_ + wsize_, wbuffer_size - wsize_, read_wchars))
                return 0;
            wsize_ += read_wchars;
            char* out = buffer_;
            const wchar_t* cur_input_ptr = wbuffer_;
            const wchar_t* const end_input_ptr = wbuffer_ + wsize_;
            while(cur_input_ptr != end_input_ptr)
            {
                const wchar_t* const prev_input_ptr = cur_input_ptr;
                utf::code_point c = decoder::decode(cur_input_ptr, end_input_ptr);
                // If incomplete restore to beginning of incomplete char to use on next buffer
                if(c == utf::incomplete)
                {
                    cur_input_ptr = prev_input_ptr;
                    break;
                }
                if(c == utf::illegal)
                    c = BOOST_NOWIDE_REPLACEMENT_CHARACTER;
                assert(out - buffer_ + encoder::width(c) <= static_cast<int>(buffer_size));
                // Skip \r chars as std::cin does
                if(c != '\r')
                    out = encoder::encode(c, out);
            }

            wsize_ = end_input_ptr - cur_input_ptr;
            if(wsize_ > 0)
                std::memmove(wbuffer_, end_input_ptr - wsize_, sizeof(wchar_t) * wsize_);

            // A CTRL+Z at the start of the line should be treated as EOF
            if(was_newline_ && out > buffer_ && buffer_[0] == '\x1a')
            {
                sync();
                return 0;
            }
            was_newline_ = out == buffer_ || out[-1] == '\n';

            return out - buffer_;
        }
    } // namespace detail
} // namespace nowide
} // namespace boost
