//
// Created by lars on 30.12.19.
//

#ifndef VELOCYPACK_UNPACK_PROXY_H
#define VELOCYPACK_UNPACK_PROXY_H

namespace deserializer {
template <typename D, typename P = D>
struct unpack_proxy {
  using constructed_type = std::unique_ptr<P>;
  using plan = unpack_proxy<D, P>;
  using factory = utilities::make_unique_factory<P>;
};

namespace executor {

template <typename D, typename P>
struct plan_result_tuple<unpack_proxy<D, P>> {
  using type = std::tuple<P>;
};

template <typename D, typename P, typename H>
struct deserialize_plan_executor<unpack_proxy<D, P>, H> {
  using proxy_type = typename D::constructed_type;
  using tuple_type = std::tuple<proxy_type>;
  using result_type = result<tuple_type, deserialize_error>;

  template<typename C>
  static auto unpack(::deserializer::slice_type s, typename H::state_type h, C&& ctx) -> result_type {
    return deserialize<D, H, C>(s, h, std::forward<C>(ctx)).map([](typename D::constructed_type&& v) {
      return std::make_tuple(std::move(v));
    });
  }
};
}  // namespace executor

}  // namespace deserializer

#endif
