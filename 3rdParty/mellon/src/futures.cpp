#include "mellon/futures.h"

using namespace mellon;

detail::invalid_pointer_type detail::invalid_pointer_inline_value;
detail::invalid_pointer_type detail::invalid_pointer_future_abandoned;
detail::invalid_pointer_type detail::invalid_pointer_promise_abandoned;
detail::invalid_pointer_type detail::invalid_pointer_promise_fulfilled;

const char* promise_abandoned_error::what() const noexcept {
  return "promise abandoned";
}

#ifdef MELLON_RECORD_BACKTRACE
#define UNW_LOCAL_ONLY
#include <cxxabi.h>
#include <libunwind.h>
#include <sstream>
#include <iomanip>

#ifdef __linux__
#include <elf.h>
#include <sys/auxv.h>
#endif

thread_local std::vector<std::string>* detail::current_backtrace_ptr;

auto detail::generate_backtrace_string() noexcept -> std::vector<std::string> try {
  std::vector<std::string> bt;

  unw_context_t context;
  unw_getcontext(&context);

  unw_cursor_t cursor;
  unw_init_local(&cursor, &context);

#ifdef __linux__
  // The address of the program headers of the executable.
  long base = getauxval(AT_PHDR) - sizeof(Elf64_Ehdr);
#else
  long base = 0;
#endif

  while (unw_step(&cursor) > 0) {
    unw_word_t offset, pc;
    unw_get_reg(&cursor, UNW_REG_IP, &pc);

    constexpr size_t kMax = 1024;
    char mangled[kMax + 1];

    std::stringstream line;
    line << "[+0x" << std::setfill('0') << std::setw (16) << std::hex << (pc - base) << "] ";
    auto res = unw_get_proc_name(&cursor, mangled, kMax, &offset);
    if (res == 0 || res == -UNW_ENOMEM) {
      int ok;
      size_t len = kMax;
      char* demangled = abi::__cxa_demangle(mangled, nullptr, &len, &ok);

      line << (ok == 0 ? demangled : mangled) << ' ' << "(+0x" << std::hex << offset << ')';
      free(demangled);
    } else if (res == -UNW_ENOINFO) {
      line << "<no information available>";
    } else {
      line << "<symbol lookup failed: "<< -res << ">";
    }
    bt.emplace_back(line.str());
  }

  return bt;
} catch (...) {
  return std::vector<std::string>{};
}
#endif

#ifdef FUTURES_COUNT_ALLOC
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <sstream>

namespace {
template <typename T, std::size_t N>
std::ostream& operator<<(std::ostream& os, std::array<T, N> const& arr) {
  os << "[";
  for (std::size_t i = 0; i < N; i++) {
    if (i != 0) {
      os << ",";
    }
    os << arr[i];
  }
  os << "]";
  return os;
}
}  // namespace

struct allocation_printer {
  ~allocation_printer() {
    std::stringstream ss;
    ss << "[FUTURES]" << mellon::detail::message_prefix
       << " number_of_allocations=" << mellon::detail::number_of_allocations
       << " number_of_bytes_allocated=" << mellon::detail::number_of_bytes_allocated
       << " number_of_inline_value_placements=" << mellon::detail::number_of_inline_value_placements
       << " number_of_inline_value_allocs=" << mellon::detail::number_of_inline_value_allocs
       << " histogram_value_sizes=" << mellon::detail::histogram_value_sizes
       << " number_of_and_then_on_inline_future=" << mellon::detail::number_of_and_then_on_inline_future
       << " number_of_temporary_objects=" << mellon::detail::number_of_temporary_objects
       << " number_of_step_usage=" << mellon::detail::number_of_step_usage
       << " number_of_promises_created=" << mellon::detail::number_of_promises_created
       << " number_of_prealloc_usage=" << mellon::detail::number_of_prealloc_usage
       << " number_of_final_usage=" << mellon::detail::number_of_final_usage
       << " histogram_final_lambda_sizes=" << mellon::detail::histogram_final_lambda_sizes
       << std::endl;
    std::cout << ss.str();
  }
};

#ifdef FUTURES_COUNT_ALLOC
std::atomic<std::size_t> mellon::detail::number_of_allocations = 0;
std::atomic<std::size_t> mellon::detail::number_of_bytes_allocated = 0;
std::atomic<std::size_t> mellon::detail::number_of_inline_value_placements = 0;
std::atomic<std::size_t> mellon::detail::number_of_temporary_objects = 0;
std::atomic<std::size_t> mellon::detail::number_of_prealloc_usage = 0;

std::atomic<std::size_t> mellon::detail::number_of_inline_value_allocs = 0;
std::atomic<std::size_t> mellon::detail::number_of_final_usage = 0;
std::atomic<std::size_t> mellon::detail::number_of_step_usage = 0;
std::atomic<std::size_t> mellon::detail::number_of_promises_created = 0;

std::atomic<std::size_t> mellon::detail::number_of_and_then_on_inline_future = 0;

std::array<std::atomic<std::size_t>, 10> mellon::detail::histogram_value_sizes{};
std::array<std::atomic<std::size_t>, 10> mellon::detail::histogram_final_lambda_sizes{};
std::string mellon::detail::message_prefix;

allocation_printer printer;
#endif
#endif
