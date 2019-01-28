/*=============================================================================
    Copyright (c) 2010-2011 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#if !defined(BOOST_SPIRIT_QUICKBOOK_VALUES_PARSE_HPP)
#define BOOST_SPIRIT_QUICKBOOK_VALUES_PARSE_HPP

#include <boost/spirit/include/phoenix1_functions.hpp>
#include "parsers.hpp"
#include "scoped.hpp"
#include "values.hpp"

#include <iostream>

namespace quickbook
{
    namespace ph = phoenix;

    struct value_builder_save : scoped_action_base
    {
        explicit value_builder_save(value_builder& builder_) : builder(builder_)
        {
        }

        bool start()
        {
            builder.save();
            return true;
        }

        void cleanup() { builder.restore(); }

        value_builder& builder;

      private:
        value_builder_save& operator=(value_builder_save const&);
    };

    struct value_builder_list : scoped_action_base
    {
        explicit value_builder_list(value_builder& builder_) : builder(builder_)
        {
        }

        bool start(value::tag_type tag = value::default_tag)
        {
            builder.start_list(tag);
            return true;
        }

        void success(parse_iterator, parse_iterator) { builder.finish_list(); }
        void failure() { builder.clear_list(); }

        value_builder& builder;

      private:
        value_builder_list& operator=(value_builder_list const&);
    };

    struct value_entry
    {
        template <
            typename Arg1,
            typename Arg2 = void,
            typename Arg3 = void,
            typename Arg4 = void>
        struct result
        {
            typedef void type;
        };

        explicit value_entry(value_builder& builder_, file_ptr* current_file_)
            : builder(builder_), current_file(current_file_)
        {
        }

        void operator()(
            parse_iterator begin,
            parse_iterator end,
            value::tag_type tag = value::default_tag) const
        {
            builder.insert(
                qbk_value(*current_file, begin.base(), end.base(), tag));
        }

        void operator()(int v, value::tag_type tag = value::default_tag) const
        {
            builder.insert(int_value(v, tag));
        }

        value_builder& builder;
        file_ptr* current_file;

      private:
        value_entry& operator=(value_entry const&);
    };

    struct value_sort
    {
        typedef void result_type;

        explicit value_sort(value_builder& builder_) : builder(builder_) {}

        void operator()() const { builder.sort_list(); }

        value_builder& builder;

      private:
        value_sort& operator=(value_sort const&);
    };

    struct value_parser
    {
        explicit value_parser(file_ptr* current_file)
            : builder()
            , save(value_builder_save(builder))
            , list(value_builder_list(builder))
            , entry(value_entry(builder, current_file))
            , sort(value_sort(builder))
        {
        }

        value release() { return builder.release(); }

        value_builder builder;
        scoped_parser<value_builder_save> save;
        scoped_parser<value_builder_list> list;
        ph::function<value_entry> entry;
        ph::function<value_sort> sort;
    };
}

#endif
