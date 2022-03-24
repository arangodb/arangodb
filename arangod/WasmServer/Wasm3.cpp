#include "Wasm3cpp.h"

using namespace wasm3;

auto wasm3::environment::new_runtime(size_t stack_size_bytes) -> runtime {
  return runtime(m_env, stack_size_bytes);
}

auto wasm3::environment::parse_module(std::istream& in) -> module {
  return module(m_env, in);
}

auto wasm3::environment::parse_module(const uint8_t* data, size_t size)
    -> module {
  return module(m_env, data, size);
}

auto wasm3::runtime::find_function(const char* name)
    -> std::optional<function> {
  M3Function* m_func = nullptr;
  M3Result err = m3_FindFunction(&m_func, m_runtime.get(), name);
  if (err != m3Err_none or m_func == nullptr) {
    return std::nullopt;
  }
  return function(m_func);
}

template<typename Func>
void wasm3::module::link(const char* module, const char* function_name,
                         Func* function) {
  M3Result ret = detail::m3_wrapper<Func>::link(m_module.get(), module,
                                                function_name, function);
  detail::check_error(ret);
}

template<typename Func>
void wasm3::module::link_optional(const char* module, const char* function_name,
                                  Func* function) {
  M3Result ret = detail::m3_wrapper<Func>::link(m_module.get(), module,
                                                function_name, function);
  if (ret == m3Err_functionLookupFailed) {
    return;
  }
  detail::check_error(ret);
}

void wasm3::runtime::load(module& mod) { mod.load_into(m_runtime.get()); }
