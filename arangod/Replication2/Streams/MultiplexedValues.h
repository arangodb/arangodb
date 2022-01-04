////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Replication2/Streams/Streams.h"

namespace arangodb::replication2::streams {

template<typename Descriptor,
         typename Type = stream_descriptor_type_t<Descriptor>>
struct DescriptorValueTag {
  using DescriptorType = Descriptor;
  explicit DescriptorValueTag(Type value) : value(std::move(value)) {}
  Type value;
};

template<typename... Descriptors>
struct MultiplexedVariant {
  using VariantType = std::variant<DescriptorValueTag<Descriptors>...>;

  [[nodiscard]] auto variant() & -> VariantType& { return _value; }
  [[nodiscard]] auto variant() && -> VariantType&& { return std::move(_value); }
  [[nodiscard]] auto variant() const& -> VariantType& { return _value; }

  template<typename... Args>
  explicit MultiplexedVariant(std::in_place_t, Args&&... args)
      : _value(std::forward<Args>(args)...) {}

 private:
  VariantType _value;
};

struct MultiplexedValues {
  template<typename Descriptor,
           typename Type = stream_descriptor_type_t<Descriptor>>
  static void toVelocyPack(Type const& v, velocypack::Builder& builder) {
    using PrimaryTag = stream_descriptor_primary_tag_t<Descriptor>;
    using Serializer = typename PrimaryTag::serializer;
    velocypack::ArrayBuilder ab(&builder);
    builder.add(velocypack::Value(PrimaryTag::tag));
    static_assert(std::is_invocable_r_v<
                  void, Serializer, serializer_tag_t<Type>,
                  std::add_lvalue_reference_t<std::add_const_t<Type>>,
                  std::add_lvalue_reference_t<velocypack::Builder>>);
    std::invoke(Serializer{}, serializer_tag<Type>, v, builder);
  }

  template<typename... Descriptors>
  static auto fromVelocyPack(velocypack::Slice slice)
      -> MultiplexedVariant<Descriptors...> {
    TRI_ASSERT(slice.isArray());
    auto [tag, valueSlice] = slice.unpackTuple<StreamTag, velocypack::Slice>();
    return FromVelocyPackHelper<MultiplexedVariant<Descriptors...>,
                                Descriptors...>::extract(tag, valueSlice);
  }

 private:
  template<typename ValueType, typename Descriptor, typename... Other>
  struct FromVelocyPackHelper {
    static auto extract(StreamTag tag, velocypack::Slice slice) -> ValueType {
      return extractTags(stream_descriptor_tags_t<Descriptor>{}, tag, slice);
    }

    template<typename Tag, typename... Tags>
    static auto extractTags(tag_descriptor_set<Tag, Tags...>, StreamTag tag,
                            velocypack::Slice slice) -> ValueType {
      if (Tag::tag == tag) {
        return extractValue<typename Tag::deserializer>(slice);
      } else if constexpr (sizeof...(Tags) > 0) {
        return extractTags(tag_descriptor_set<Tags...>{}, tag, slice);
      } else if constexpr (sizeof...(Other) > 0) {
        return FromVelocyPackHelper<ValueType, Other...>::extract(tag, slice);
      } else {
        std::abort();
      }
    }

    template<typename Deserializer,
             typename Type = stream_descriptor_type_t<Descriptor>>
    static auto extractValue(velocypack::Slice slice) -> ValueType {
      static_assert(
          std::is_invocable_r_v<Type, Deserializer, serializer_tag_t<Type>,
                                velocypack::Slice>);
      auto value = std::invoke(Deserializer{}, serializer_tag<Type>, slice);
      return ValueType(std::in_place,
                       std::in_place_type<DescriptorValueTag<Descriptor>>,
                       std::move(value));
    }
  };
};
}  // namespace arangodb::replication2::streams
