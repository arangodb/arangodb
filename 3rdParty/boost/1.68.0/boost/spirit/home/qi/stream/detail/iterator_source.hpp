/*=============================================================================
    Copyright (c) 2001-2011 Hartmut Kaiser

    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#if !defined(BOOST_SPIRIT_ITERATOR_ISTREAM_MAY_05_2007_0110PM)
#define BOOST_SPIRIT_ITERATOR_ISTREAM_MAY_05_2007_0110PM

#if defined(_MSC_VER)
#pragma once
#endif

#include <boost/iostreams/stream.hpp>
#include <boost/detail/iterator.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace boost { namespace spirit { namespace qi { namespace detail
{
    ///////////////////////////////////////////////////////////////////////////
    template <typename Iterator>
    struct base_iterator_source
    {
        typedef typename
            boost::detail::iterator_traits<Iterator>::value_type
        char_type;
        typedef boost::iostreams::seekable_device_tag category;

        base_iterator_source (Iterator const& first_, Iterator const& last_)
          : first(first_), last(last_), pos(0)
        {}

        // Read up to n characters from the input sequence into the buffer s,
        // returning the number of characters read, or -1 to indicate
        // end-of-sequence.
        std::streamsize read (char_type* s, std::streamsize n)
        {
            if (first == last)
                return -1;

            std::streamsize bytes_read = 0;
            while (n--) {
                *s = *first;
                ++s; ++bytes_read;
                if (++first == last)
                    break;
            }

            pos += bytes_read;
            return bytes_read;
        }

        // Write is implemented only to satisfy the requirements of a
        // boost::iostreams::seekable_device. We need to have see support to
        // be able to figure out how many characters have been actually
        // consumed by the stream.
        std::streamsize write(const char_type*, std::streamsize)
        {
            BOOST_ASSERT(false);    // not supported
            return -1;
        }

        std::streampos seek(boost::iostreams::stream_offset, std::ios_base::seekdir way)
        {
            BOOST_ASSERT(way == std::ios_base::cur);    // only support queries
            return pos;                              // return current position
        }

        Iterator first;
        Iterator const& last;
        std::streamsize pos;

    private:
        // silence MSVC warning C4512: assignment operator could not be generated
        base_iterator_source& operator= (base_iterator_source const&);
    };

    template <typename Iterator, typename Enable = void>
    struct iterator_source : base_iterator_source<Iterator>
    {
        typedef base_iterator_source<Iterator> base_type;

        iterator_source (Iterator const& first_, Iterator const& last_) 
            : base_type(first_, last_) {}
    };

    // Specialization for random-access iterators. This also allows compilers
    // to fully optimize the case when the source range is contiguous
    template <typename Iterator>
    struct iterator_source<
            Iterator, 
            typename boost::enable_if_c<boost::is_convertible<
                typename boost::detail::iterator_traits<Iterator>::iterator_category, std::random_access_iterator_tag>::value>::type
        > : base_iterator_source<Iterator>
    {
        typedef base_iterator_source<Iterator> base_type;

        iterator_source (Iterator const& first_, Iterator const& last_) 
            : base_type(first_, last_) {}

        typedef typename base_type::char_type char_type;
        using base_type::first;
        using base_type::last;
        using base_type::pos;

        // Read up to n characters from the input sequence into the buffer s,
        // returning the number of characters read, or -1 to indicate
        // end-of-sequence.
        std::streamsize read (char_type* s, std::streamsize n)
        {
            if (first == last)
                return -1;

            n = std::min BOOST_PREVENT_MACRO_SUBSTITUTION(
                    static_cast<std::streamsize>(std::distance(first, last)),
                    n);

            // copy_n is only part of c++11, so emulate it
            std::copy(first, first + n, s);
            first += n;
            pos += n;

            return n;
        }
    };

}}}}

#endif
