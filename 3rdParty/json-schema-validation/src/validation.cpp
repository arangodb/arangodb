#include <cassert>

#include <tao/json/contrib/schema.hpp>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <validation/events_from_slice.hpp>
#include <validation/types.hpp>
#include <validation/validation.hpp>

namespace {
using namespace arangodb;
using namespace arangodb::validation;
[[nodiscard]] tao::json::value slice_object_to_value(VPackSlice const& slice,
                                                     bool ignore_special,
                                                     VPackOptions const* options,
                                                     VPackSlice const* base) {
    assert(slice.isObject());
    tao::json::value rv;
    rv.prepare_object();

    VPackObjectIterator it(slice, true);
    while (it.valid()) {
        velocypack::ValueLength length;
        VPackSlice key = it.key(false);

        if (key.isString()) {
            // regular attribute
            rv.try_emplace(key.copyString(), slice_to_value(it.value(), ignore_special, options, &slice));
        } else if (!ignore_special) {
            // optimized code path for translated system attributes
            tao::json::value sub;
            VPackSlice val = VPackSlice(key.begin() + 1);
            if (val.isString()) {
                // value of _key, _id, _from, _to, and _rev is ASCII too
                char const* pointer = val.getString(length);
                sub = std::string(pointer, length);
            } else {
                sub = slice_to_value(val, ignore_special, options, &slice);
            }

            using namespace arangodb::basics;
            uint8_t which = static_cast<uint8_t>(key.getUInt()) + VelocyPackHelper::AttributeBase;
            rv.try_emplace(id_to_string(which), std::move(sub));
        }

        it.next();
    }

    return rv;
}

[[nodiscard]] tao::json::value slice_array_to_value(VPackSlice const& slice,
                                                    bool ignore_special,
                                                    VPackOptions const* options,
                                                    VPackSlice const* base) {
    assert(slice.isArray());
    VPackArrayIterator it(slice);
    tao::json::value rv;
    auto a = rv.prepare_array();
    a.resize(it.size());

    while (it.valid()) {
        tao::json::value val = slice_to_value(it.value(), ignore_special, options, &slice);
        rv.push_back(std::move(val));
        it.next();
    }

    return rv;
}

template<template<typename...> class Traits>
[[nodiscard]] bool validate(const tao::json::basic_schema<Traits>& schema,
                            VPackSlice const& v,
                            VPackOptions const* options = &VPackOptions::Defaults,
                            bool ignore_special = true) {
    const auto c = schema.consumer();
    tao::json::events::from_value<Traits>(*c, v, options, nullptr, ignore_special);
    return c->finalize();
}

// use basics strings
std::string const key_string = "_key";
std::string const rev_string = "_rev";
std::string const id_string = "_id";
std::string const from_string = "_form";
std::string const to_string = "_to";

} // namespace

namespace arangodb::validation {

bool validate(VPackSlice const doc, VPackOptions const* options, tao::json::schema const& schema) {
    return ::validate(schema, doc, options, true);
}

bool validate(tao::json::value const& doc, tao::json::schema const& schema) {
    return schema.validate(doc);
}

tao::json::value
    slice_to_value(VPackSlice const& slice, bool ignore_special, VPackOptions const* options, VPackSlice const* base) {
    tao::json::value rv;
    switch (slice.type()) {
        case VPackValueType::Null: {
            rv.set_null();
            return rv;
        }
        case VPackValueType::Bool: {
            rv.set_boolean(slice.getBool());
            return rv;
        }
        case VPackValueType::Double: {
            rv.set_double(slice.getDouble());
            return rv;
        }
        case VPackValueType::Int: {
            rv.set_signed(slice.getInt());
            return rv;
        }
        case VPackValueType::UInt: {
            rv.set_unsigned(slice.getUInt());
            return rv;
        }
        case VPackValueType::SmallInt: {
            rv.set_signed(slice.getNumericValue<int32_t>());
            return rv;
        }
        case VPackValueType::String: {
            rv.set_string(slice.copyString());
            return rv;
        }
        case VPackValueType::Array: {
            return slice_array_to_value(slice, ignore_special, options, base);
            return rv;
        }
        case VPackValueType::Object: {
            return slice_object_to_value(slice, ignore_special, options, base);
            return rv;
        }
        case VPackValueType::External: {
            return slice_to_value(
                VPackSlice(reinterpret_cast<uint8_t const*>(slice.getExternal())), ignore_special, options, base);
            return rv;
        }
        case VPackValueType::Custom: {
            if (options == nullptr || options->customTypeHandler == nullptr || base == nullptr) {
                throw std::runtime_error("Could not extract custom attribute.");
            }
            std::string id = options->customTypeHandler->toString(slice, options, *base);
            rv.set_string(std::move(id));
            return rv;
        }
        case VPackValueType::None:
        default:
            break;
            return rv;
    }
    return rv;
}

tao::json::basic_schema<tao::json::traits>* new_schema(VPackSlice const& schema) {
    auto tao_value_schema = slice_to_value(schema);
    return new tao::json::schema(tao_value_schema);
}

[[nodiscard]] std::string const& id_to_string(std::uint8_t id) {
    using namespace basics;
    using namespace std::literals::string_literals;
    switch (id) {
        case VelocyPackHelper::KeyAttribute: {
            return key_string;
        }
        case VelocyPackHelper::RevAttribute: {
            return rev_string;
        }
        case VelocyPackHelper::IdAttribute: {
            return id_string;
        }
        case VelocyPackHelper::FromAttribute: {
            return from_string;
        }
        case VelocyPackHelper::ToAttribute: {
            return to_string;
        }
    }
    assert(false);
    throw std::runtime_error("can not translate id");
    return key_string; // make sure we return something
}

} // namespace arangodb::validation
