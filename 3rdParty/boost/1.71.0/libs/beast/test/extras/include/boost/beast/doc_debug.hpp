//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_BEAST_DOC_DEBUG_HPP
#define BOOST_BEAST_DOC_DEBUG_HPP

namespace beast {

#if BOOST_BEAST_DOXYGEN

/// doc type (documentation debug helper)
using doc_type = int;

/// doc enum (documentation debug helper)
enum doc_enum
{
    /// One (documentation debug helper)
    one,

    /// Two (documentation debug helper)
    two
};

/// doc enum class (documentation debug helper)
enum class doc_enum_class : unsigned
{
    /// one (documentation debug helper)
    one,

    /// two (documentation debug helper)
    two
};

/// doc func (documentation debug helper)
void doc_func();

/// doc class (documentation debug helper)
struct doc_class
{
    /// doc class member func (documentation debug helper)
    void func();
};

/// (documentation debug helper)
namespace nested {

/// doc type (documentation debug helper)
using nested_doc_type = int;

/// doc enum (documentation debug helper)
enum nested_doc_enum
{
    /// One (documentation debug helper)
    one,

    /// Two (documentation debug helper)
    two
};

/// doc enum class (documentation debug helper)
enum class nested_doc_enum_class : unsigned
{
    /// one (documentation debug helper)
    one,

    /// two (documentation debug helper)
    two
};

/// doc func (documentation debug helper)
void nested_doc_func();

/// doc class (documentation debug helper)
struct nested_doc_class
{
    /// doc class member func (documentation debug helper)
    void func();
};

} // nested

/** This is here to help troubleshoot doc/reference.xsl problems

    Embedded references:

    @li type @ref doc_type

    @li enum @ref doc_enum

    @li enum item @ref doc_enum::one

    @li enum_class @ref doc_enum_class

    @li enum_class item @ref doc_enum_class::one

    @li func @ref doc_func

    @li class @ref doc_class

    @li class func @ref doc_class::func

    @li nested type @ref nested::nested_doc_type

    @li nested enum @ref nested::nested_doc_enum

    @li nested enum item @ref nested::nested_doc_enum::one

    @li nested enum_class @ref nested::nested_doc_enum_class

    @li nested enum_class item @ref nested::nested_doc_enum_class::one

    @li nested func @ref nested::nested_doc_func

    @li nested class @ref nested::nested_doc_class

    @li nested class func @ref nested::nested_doc_class::func
*/
void doc_debug();

namespace nested {

/** This is here to help troubleshoot doc/reference.xsl problems

    Embedded references:

    @li type @ref doc_type

    @li enum @ref doc_enum

    @li enum item @ref doc_enum::one

    @li enum_class @ref doc_enum_class

    @li enum_class item @ref doc_enum_class::one

    @li func @ref doc_func

    @li class @ref doc_class

    @li class func @ref doc_class::func

    @li nested type @ref nested_doc_type

    @li nested enum @ref nested_doc_enum

    @li nested enum item @ref nested_doc_enum::one

    @li nested enum_class @ref nested_doc_enum_class

    @li nested enum_class item @ref nested_doc_enum_class::one

    @li nested func @ref nested_doc_func

    @li nested class @ref nested_doc_class

    @li nested class func @ref nested_doc_class::func
*/
void nested_doc_debug();

} // nested

#endif

} // beast

#endif
