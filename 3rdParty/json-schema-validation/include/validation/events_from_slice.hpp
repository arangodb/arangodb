// Copyright (c) 2016-2020 Daniel Frey and Jan Christoph Uhde
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef ARANGO_SCHEMA_VALIADTION_EVENTS_FROM_SLICE_HEADER
#define ARANGO_SCHEMA_VALIADTION_EVENTS_FROM_SLICE_HEADER 1

#include <stdexcept>

#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include "tao/json/internal/format.hpp"
#include "types.hpp"

namespace tao::json::events {
// Events producer to generate events from a VPackSlice Value.

template<auto Recurse, template<typename...> class Traits, typename Consumer>
void from_value(Consumer& consumer,
                const VPackSlice& slice,
                VPackOptions const* options,
                VPackSlice const* base,
                bool ignore_special) {
    switch (slice.type()) {
        case arangodb::velocypack::ValueType::Illegal:
            throw std::logic_error("unable to produce events from illegal values");

        case arangodb::velocypack::ValueType::Null:
            consumer.null();
            return;

        case arangodb::velocypack::ValueType::Bool:
            consumer.boolean(slice.getBool());
            return;

        case arangodb::velocypack::ValueType::Int:
            consumer.number(slice.getInt());
            return;
        case arangodb::velocypack::ValueType::SmallInt:
            consumer.number(slice.getSmallInt());
            return;

        case arangodb::velocypack::ValueType::UInt:
            consumer.number(slice.getUInt());
            return;

        case arangodb::velocypack::ValueType::Double:
            consumer.number(slice.getDouble());
            return;

        case arangodb::velocypack::ValueType::String: {
            auto ref = slice.stringRef();
            consumer.string(std::string_view(ref.data(), ref.length()));
            return;
        }

        case arangodb::velocypack::ValueType::Binary: {
            auto len = slice.length();
            uint8_t const* start = slice.getBinary(len);
            consumer.binary(tao::binary_view(reinterpret_cast<std::byte const*>(start), len));
            return;
        }

        case arangodb::velocypack::ValueType::Array: {
            consumer.begin_array(slice.length());
            for (const auto& element : VPackArrayIterator(slice)) {
                Recurse(consumer, element, options, &slice, ignore_special);
                consumer.element();
            }
            consumer.end_array(slice.length());
            return;
        }

        case arangodb::velocypack::ValueType::Object: {
            const auto len = slice.length();
            consumer.begin_object(len);
            for (const auto& element : VPackObjectIterator(slice)) {
                VPackSlice key = element.key;
                if (key.isString()) {
                    auto key_ref = key.stringRef();
                    consumer.key(std::string_view(key_ref.data(), key_ref.length()));
                    Recurse(consumer, element.value, options, &slice, ignore_special);
                    consumer.member();
                } else if (!ignore_special) {
                    std::string translated_key;
                    using namespace arangodb::basics;
                    uint8_t which = static_cast<uint8_t>(key.getUInt()) + VelocyPackHelper::AttributeBase;
                    consumer.key(arangodb::validation::id_to_string(which));
                    VPackSlice val = VPackSlice(element.key.begin() + 1);
                    arangodb::velocypack::ValueLength length;
                    if (val.isString()) {
                        // value of _key, _id, _from, _to, and _rev is ASCII too
                        char const* pointer = val.getString(length);
                        consumer.string(std::string_view(pointer, length));
                    } else {
                        Recurse(consumer, val, options, &slice, ignore_special);
                    }
                } // ! ignore_special
            }
            consumer.end_object(len);
            return;
        }
        case VPackValueType::External: {
            Recurse(consumer,
                    VPackSlice(reinterpret_cast<uint8_t const*>(slice.getExternal())),
                    options,
                    base,
                    ignore_special);
            return;
        }

        case VPackValueType::Custom: {
            if (options == nullptr || options->customTypeHandler == nullptr || base == nullptr) {
                throw std::runtime_error("Could not extract custom attribute.");
            }
            std::string id = options->customTypeHandler->toString(slice, options, *base);
            consumer.string(std::move(id));
            return;
        }

        case arangodb::velocypack::ValueType::BCD:
        case arangodb::velocypack::ValueType::MaxKey:
        case arangodb::velocypack::ValueType::MinKey:
        case arangodb::velocypack::ValueType::None:
        case arangodb::velocypack::ValueType::Tagged:
        case arangodb::velocypack::ValueType::UTCDate:
            throw std::runtime_error(std::string("unsupported type: ") + slice.typeName());
    }
    throw std::logic_error(internal::format(
        "invalid value '", static_cast<std::uint8_t>(slice.type()), "' for arangodb::velocypack::ValueType"));
}

template<template<typename...> class Traits, typename Consumer>
void from_value(Consumer& consumer,
                const VPackSlice& slice,
                VPackOptions const* options,
                VPackSlice const* base,
                bool ignore_special) {
    // clang-format off
    from_value<static_cast<void (*)(Consumer&, const VPackSlice&, VPackOptions const*, VPackSlice const*, bool)>(&from_value<Traits, Consumer>)
              ,Traits
              ,Consumer
              >(consumer, slice, options, base, ignore_special);
    // clang-format on
}

} // namespace tao::json::events


#endif
