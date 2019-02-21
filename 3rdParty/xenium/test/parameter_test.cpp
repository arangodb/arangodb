#include <xenium/parameter.hpp>
#include <xenium/ramalhete_queue.hpp>

#include <gtest/gtest.h>

struct my_reclaimer {};
struct my_backoff {};

namespace {
  using pack = xenium::parameter::pack<
    xenium::policy::entries_per_node<42>,
    xenium::policy::reclaimer<my_reclaimer>,
    xenium::policy::backoff<my_backoff>>;

  TEST(Parameter, type_param_extracts_type_from_specified_policy)
  {
    using reclaimer = typename xenium::parameter::type_param<xenium::policy::reclaimer, pack>::type;
    static_assert(std::is_same<reclaimer, my_reclaimer>::value, "");

    using backoff = typename xenium::parameter::type_param<xenium::policy::backoff, pack>::type;
    static_assert(std::is_same<backoff, my_backoff>::value, "");
  }

  TEST(Parameter, type_param_extracts_value_from_specified_policy)
  {
    constexpr auto entries_per_node = xenium::parameter::value_param<unsigned, xenium::policy::entries_per_node, pack, 0>::value;
    static_assert(entries_per_node == 42, "");
  }
}
