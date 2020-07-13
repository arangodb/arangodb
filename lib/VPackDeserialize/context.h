#ifndef DESERIALIZER_CONTEXT_H
#define DESERIALIZER_CONTEXT_H
#include "utilities.h"
#include "plan-executor.h"

namespace deserializer {

namespace context {

template<typename D, typename Q>
struct context_modify_plan{
  using constructed_type = typename D::constructed_type;
};

template<typename D, auto M>
struct from_member;

template <typename D, typename A, typename B, A B::*member>
struct from_member<D, member> {
  using plan = context_modify_plan<D, utilities::member_extractor<member>>;
  using factory = typename D::F;
  using constructed_type = typename D::R;
};

}

namespace executor {

template <typename D, typename Q, typename H>
struct deserialize_plan_executor<context::context_modify_plan<D, Q>, H> {
  template <typename ctx>
  static auto unpack(::deserializer::slice_type s, typename H::state_type hints, ctx&& c) {
    return deserialize<D, H, Q::context_type>(s, hints, Q::exec(std::forward<ctx>(c)));
  }
};



}

}



#endif  // DESERIALIZER_CONTEXT_H
