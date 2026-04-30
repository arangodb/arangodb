
#include <typeinfo>
#include <cxxabi.h>
#include <memory>
#include "util.h"

namespace utils {

    std::string demangle(const char* name) {
        int status = 0;
        std::unique_ptr<char, void (*)(void*)> result(
            abi::__cxa_demangle(name, nullptr, nullptr, &status), std::free);
        return (status == 0) ? result.get() : name;
    }

}  // namespace