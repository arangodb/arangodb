// Copyright (c) 2016-2020 Daniel Frey and Jan Christoph Uhde
// Please see LICENSE for license or visit https://github.com/taocpp/json/

#ifndef TAO_JSON_EVENTS_FROM_SILCE_HPP
#define TAO_JSON_EVENTS_FROM_SLICE_HPP

#include <stdexcept>

#include "tao/json/internal/format.hpp"

#include <velocypack/Iterator.h>
#include <velocypack/Options.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace tao::json::events {
// Events producer to generate events from a VPackSlice Value.

template<auto Recurse, template<typename...> class Traits, typename Consumer>
void from_value(Consumer& consumer, const VPackSlice& v, VPackOptions const* o) {
    switch (v.type()) {
        case arangodb::velocypack::ValueType::Illegal:
            throw std::logic_error("unable to produce events from illegal values");

        case arangodb::velocypack::ValueType::Null:
            consumer.null();
            return;

        case arangodb::velocypack::ValueType::Bool:
            consumer.boolean(v.getBool());
            return;

        case arangodb::velocypack::ValueType::Int:
            consumer.number(v.getInt());
            return;
        case arangodb::velocypack::ValueType::SmallInt:
            consumer.number(v.getSmallInt());
            return;

        case arangodb::velocypack::ValueType::UInt:
            consumer.number(v.getUInt());
            return;

        case arangodb::velocypack::ValueType::Double:
            consumer.number(v.getDouble());
            return;

        case arangodb::velocypack::ValueType::String: {
            auto ref = v.stringRef();
            consumer.string(std::string_view(ref.data(), ref.length()));
            return;
        }

        case arangodb::velocypack::ValueType::Binary:
            // FIXME consumer.binary( v.getExternal() );
            return;

        case arangodb::velocypack::ValueType::Array: {
            consumer.begin_array(v.length());
            for (const auto& e : VPackArrayIterator(v)) {
                Recurse(consumer, e, o);
                consumer.element();
            }
            consumer.end_array(v.length());
            return;
        }

        case arangodb::velocypack::ValueType::Object: {
            const auto s = v.length();
            consumer.begin_object(s);
            for (const auto& e : VPackObjectIterator(v)) {
                auto key_ref = e.key.stringRef();
                consumer.key(std::string_view(key_ref.data(), key_ref.length()));
                Recurse(consumer, e.value, o);
                consumer.member();
            }
            consumer.end_object(s);
            return;
        }
        case arangodb::velocypack::ValueType::BCD:
        case arangodb::velocypack::ValueType::Custom:
        case arangodb::velocypack::ValueType::MaxKey:
        case arangodb::velocypack::ValueType::MinKey:
        case arangodb::velocypack::ValueType::None:
        case arangodb::velocypack::ValueType::Tagged:
        case arangodb::velocypack::ValueType::UTCDate:
        case arangodb::velocypack::ValueType::External:

            throw std::runtime_error("unsupported type");
    }
    throw std::logic_error(internal::format(
        "invalid value '", static_cast<std::uint8_t>(v.type()), "' for arangodb::velocypack::ValueType"));
}

template<template<typename...> class Traits, typename Consumer>
void from_value(Consumer& consumer, const VPackSlice& v, VPackOptions const* o) {
    // clang-format off
    from_value<static_cast<void (*)(Consumer&, const VPackSlice&, VPackOptions const* o)>( &from_value<Traits, Consumer>)
              ,Traits
              ,Consumer
              >(consumer, v, o);
    // clang-format on
}

} // namespace tao::json::events


#endif
