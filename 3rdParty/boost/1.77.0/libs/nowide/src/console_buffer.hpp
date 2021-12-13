//  Copyright (c) 2012 Artyom Beilis (Tonkikh)
//  Copyright (c) 2020 - 2021 Alexander Grund
//
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NOWIDE_DETAIL_CONSOLE_BUFFER_HPP_INCLUDED
#define BOOST_NOWIDE_DETAIL_CONSOLE_BUFFER_HPP_INCLUDED

#include <boost/nowide/config.hpp>
#include <boost/nowide/utf/utf.hpp>
#include <streambuf>
#include <vector>

#include <boost/config/abi_prefix.hpp> // must be the last #include

// Internal header, not meant to be used outside of the implementation
namespace boost {
namespace nowide {
    namespace detail {

        /// \cond INTERNAL
        class BOOST_NOWIDE_DECL console_output_buffer_base : public std::streambuf
        {
        protected:
            int sync() override;
            int overflow(int c) override;

        private:
            using decoder = utf::utf_traits<char>;
            using encoder = utf::utf_traits<wchar_t>;

            int write(const char* p, int n);
            virtual bool
            do_write(const wchar_t* buffer, std::size_t num_chars_to_write, std::size_t& num_chars_written) = 0;

            static constexpr int buffer_size = 1024;
            static constexpr int wbuffer_size = buffer_size * encoder::max_width;
            char buffer_[buffer_size];
            wchar_t wbuffer_[wbuffer_size];
        };

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable : 4251)
#endif

        class BOOST_NOWIDE_DECL console_input_buffer_base : public std::streambuf
        {
        protected:
            int sync() override;
            int pbackfail(int c) override;
            int underflow() override;

        private:
            using decoder = utf::utf_traits<wchar_t>;
            using encoder = utf::utf_traits<char>;

            size_t read();
            virtual bool do_read(wchar_t* buffer, std::size_t num_chars_to_read, std::size_t& num_chars_read) = 0;

            static constexpr size_t wbuffer_size = 1024;
            static constexpr size_t buffer_size = wbuffer_size * encoder::max_width;
            char buffer_[buffer_size];
            wchar_t wbuffer_[wbuffer_size];
            size_t wsize_ = 0;
            std::vector<char> pback_buffer_;
            bool was_newline_ = true;
        };

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

        /// \endcond

    } // namespace detail
} // namespace nowide
} // namespace boost

#include <boost/config/abi_suffix.hpp> // pops abi_prefix.hpp pragmas

#endif
