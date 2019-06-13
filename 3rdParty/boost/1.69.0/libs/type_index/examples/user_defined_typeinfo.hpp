// Copyright 2013-2018 Antony Polukhin

// Distributed under the Boost Software License, Version 1.0.
// (See the accompanying file LICENSE_1_0.txt
// or a copy at <http://www.boost.org/LICENSE_1_0.txt>.)

#ifndef USER_DEFINED_TYPEINFO_HPP
#define USER_DEFINED_TYPEINFO_HPP

//[type_index_userdefined_usertypes
/*`
    The following example shows how a user defined type_info can be created and used.
    Example works with and without RTTI.

    Consider situation when user uses only those types in `typeid()`:
*/

#include <vector>
#include <string>

namespace my_namespace {

class my_class;
struct my_struct;

typedef std::vector<my_class> my_classes;
typedef std::string my_string;

} // namespace my_namespace

//] [/type_index_userdefined_usertypes]


//[type_index_userdefined_enum
/*`
    In that case user may wish to save space in binary and create it's own type system.
    For that case `detail::typenum<>` meta function is added. Depending on the input type T
    this function will return different numeric values.
*/
#include <boost/type_index/type_index_facade.hpp>

namespace my_namespace { namespace detail {
    template <class T> struct typenum;
    template <> struct typenum<void>{       enum {value = 0}; };
    template <> struct typenum<my_class>{   enum {value = 1}; };
    template <> struct typenum<my_struct>{  enum {value = 2}; };
    template <> struct typenum<my_classes>{ enum {value = 3}; };
    template <> struct typenum<my_string>{  enum {value = 4}; };

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable: 4510 4512 4610) // non-copyable non-constructable type
#endif

    // my_typeinfo structure is used to save type number
    struct my_typeinfo {
        const char* const type_;
    };

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif

    const my_typeinfo infos[5] = {
        {"void"}, {"my_class"}, {"my_struct"}, {"my_classes"}, {"my_string"}
    };

    template <class T>
    inline const my_typeinfo& my_typeinfo_construct() {
        return infos[typenum<T>::value];
    }
}} // my_namespace::detail

//] [/type_index_userdefined_usertypes]


//[type_index_my_type_index
/*`
    `my_type_index` is a user created type_index class. If in doubt during this phase, you can always
    take a look at the `<boost/type_index/ctti_type_index.hpp>` or `<boost/type_index/stl_type_index.hpp>`
    files. Documentation for `type_index_facade` could be also useful.
*/

/*`
    Since we are not going to override `type_index_facade::hash_code()` we must additionally include
    `<boost/container_hash/hash.hpp>`.
*/
#include <boost/container_hash/hash.hpp>

/*`
    See implementation of `my_type_index`:
*/
namespace my_namespace {

class my_type_index: public boost::typeindex::type_index_facade<my_type_index, detail::my_typeinfo> {
    const detail::my_typeinfo* data_;

public:
    typedef detail::my_typeinfo type_info_t;

    inline my_type_index() BOOST_NOEXCEPT
        : data_(&detail::my_typeinfo_construct<void>())
    {}

    inline my_type_index(const type_info_t& data) BOOST_NOEXCEPT
        : data_(&data)
    {}

    inline const type_info_t&  type_info() const BOOST_NOEXCEPT {
        return *data_;
    }

    inline const char*  raw_name() const BOOST_NOEXCEPT {
        return data_->type_;
    }

    inline std::string  pretty_name() const {
        return data_->type_;
    }

    template <class T>
    inline static my_type_index type_id() BOOST_NOEXCEPT {
        return detail::my_typeinfo_construct<T>();
    }

    template <class T>
    inline static my_type_index type_id_with_cvr() BOOST_NOEXCEPT {
        return detail::my_typeinfo_construct<T>();
    }

    template <class T>
    inline static my_type_index type_id_runtime(const T& variable) BOOST_NOEXCEPT;
};

} // namespace my_namespace

/*`
    Note that we have used the boost::typeindex::type_index_facade class as base.
    That class took care about all the helper function and operators (comparison, hashing, ostreaming and others).
*/

//] [/type_index_my_type_index]

//[type_index_my_type_index_register_class
/*`
    Usually to allow runtime type info we need to register class with some macro. 
    Let's see how a `MY_TYPEINDEX_REGISTER_CLASS` macro could be implemented for our `my_type_index` class:
*/
namespace my_namespace { namespace detail {

    template <class T>
    inline const my_typeinfo& my_typeinfo_construct_ref(const T*) {
        return my_typeinfo_construct<T>();
    }

#define MY_TYPEINDEX_REGISTER_CLASS                                             \
    virtual const my_namespace::detail::my_typeinfo& type_id_runtime() const {  \
        return my_namespace::detail::my_typeinfo_construct_ref(this);           \
    }

}} // namespace my_namespace::detail

//] [/type_index_my_type_index_register_class]

//[type_index_my_type_index_type_id_runtime_implmentation
/*`
    Now when we have a MY_TYPEINDEX_REGISTER_CLASS, let's implement a `my_type_index::type_id_runtime` method:
*/
namespace my_namespace {
    template <class T>
    my_type_index my_type_index::type_id_runtime(const T& variable) BOOST_NOEXCEPT {
        // Classes that were marked with `MY_TYPEINDEX_REGISTER_CLASS` will have a
        // `type_id_runtime()` method.
        return variable.type_id_runtime();
    }
}
//] [/type_index_my_type_index_type_id_runtime_implmentation]

//[type_index_my_type_index_type_id_runtime_classes
/*`
    Consider the situation, when `my_class` and `my_struct` are polymorphic classes:
*/

namespace my_namespace {

class my_class {
public:
    MY_TYPEINDEX_REGISTER_CLASS
    virtual ~my_class() {} 
};

struct my_struct: public my_class {
    MY_TYPEINDEX_REGISTER_CLASS
};

} // namespace my_namespace

//] [/type_index_my_type_index_type_id_runtime_classes]


//[type_index_my_type_index_worldwide_typedefs
/*`
    You'll also need to add some typedefs and macro to your "user_defined_typeinfo.hpp" header file:
*/
#define BOOST_TYPE_INDEX_REGISTER_CLASS MY_TYPEINDEX_REGISTER_CLASS
namespace boost { namespace typeindex {
    typedef my_namespace::my_type_index type_index;
}}
//] [/type_index_my_type_index_worldwide_typedefs]


#endif // USER_DEFINED_TYPEINFO_HPP

