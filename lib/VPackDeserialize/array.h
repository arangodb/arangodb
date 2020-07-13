#ifndef VELOCYPACK_ARRAY_H
#define VELOCYPACK_ARRAY_H
#include "deserialize-with.h"
#include "errors.h"
#include "types.h"
#include "utilities.h"
#include "vpack-types.h"

namespace deserializer {

/*
 * Array deserializer is used to deserialize an array of variable length but with
 * homogeneous types. Each entry is deserialized with the given deserializer.
 */
template <typename D, template <typename> typename C>
using array_deserializer_container_type = C<typename D::constructed_type>;

template <typename D, template <typename> typename C,
          typename F = utilities::identity_factory<array_deserializer_container_type<D, C>>>
struct array_deserializer {
  using plan = array_deserializer<D, C, F>;
  using factory = F;
  using constructed_type = array_deserializer_container_type<D, C>;
};

namespace executor {

template <typename D, template <typename> typename C, typename F, typename H>
struct deserialize_plan_executor<array_deserializer<D, C, F>, H> {
  using proxy_type = typename array_deserializer<D, C, F>::constructed_type;
  using tuple_type = std::tuple<proxy_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template<typename ctx>
  static auto unpack(slice_type slice, typename H::state_type, ctx && c) -> result_type {
    if (!slice.isArray()) {
      return result_type{deserialize_error{"array expected"}};
    }

    using namespace std::string_literals;
    std::size_t index = 0;
    proxy_type result;

    for (auto const& member : ::deserializer::array_iterator(slice)) {
      auto member_result = deserialize<D, hints::hint_list_empty, ctx>(member, {}, std::forward<ctx>(c));
      if (member_result) {
        result.emplace_back(std::move(member_result).get());
      } else {
        return std::move(member_result)
            .error()
            .wrap("at array index "s + std::to_string(index))
            .trace(index);
      }
      index++;
    }

    return result_type{std::move(result)};
  }
};

}  // namespace executor
}  // namespace deserializer

#endif  // VELOCYPACK_ARRAY_H
