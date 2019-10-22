#ifndef POLYMORPHIC_BASE_HPP
#define POLYMORPHIC_BASE_HPP

// MS compatible compilers support #pragma once
#if defined(_MSC_VER)
# pragma once
#endif

/////////1/////////2/////////3/////////4/////////5/////////6/////////7/////////8
// polymorphic_base.hpp    simple class test

// (C) Copyright 2002 Robert Ramey - http://www.rrsd.com . 
// Use, modification and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org for updates, documentation, and revision history.

#include <boost/config.hpp>

#include <boost/serialization/access.hpp>
#include <boost/serialization/assume_abstract.hpp>
#include <boost/serialization/export.hpp>
#include <boost/serialization/type_info_implementation.hpp>
#include <boost/serialization/extended_type_info_no_rtti.hpp>

#if defined(BOOST_ALL_DYN_LINK) || defined(BOOST_SERIALIZATION_DYN_LINK)
    #if defined(POLYMORPHIC_BASE_IMPORT)
        #define POLYMORPHIC_BASE_DLL_DECL BOOST_SYMBOL_IMPORT
        #pragma message ("polymorphic_base imported")
    #elif defined(POLYMORPHIC_BASE_EXPORT)
        #define POLYMORPHIC_BASE_DLL_DECL BOOST_SYMBOL_EXPORT
        #pragma message ("polymorphic_base exported")
    #endif
#endif

#ifndef POLYMORPHIC_BASE_DLL_DECL
    #define POLYMORPHIC_BASE_DLL_DECL
#endif

class BOOST_SYMBOL_VISIBLE polymorphic_base
{
    friend class boost::serialization::access;
    template<class Archive>
    POLYMORPHIC_BASE_DLL_DECL void serialize(
        Archive & /* ar */, 
        const unsigned int /* file_version */
    );
public:
    // note that since this class uses the "no_rtti"
    // extended_type_info implementation, it MUST
    // implement this function
    virtual const char * get_key() const = 0;
    POLYMORPHIC_BASE_DLL_DECL polymorphic_base();
    POLYMORPHIC_BASE_DLL_DECL virtual ~polymorphic_base();
};

BOOST_SERIALIZATION_ASSUME_ABSTRACT(polymorphic_base)

// the no_rtti system requires this !!!
BOOST_CLASS_EXPORT_KEY(polymorphic_base)

BOOST_CLASS_TYPE_INFO(
    polymorphic_base,
    extended_type_info_no_rtti<polymorphic_base>
)

#endif // POLYMORPHIC_BASE_HPP
