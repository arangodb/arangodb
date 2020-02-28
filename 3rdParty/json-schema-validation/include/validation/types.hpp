#ifndef ARANGO_SCHEMA_VALIADTION_TYPES_HEADER
#define ARANGO_SCHEMA_VALIADTION_TYPES_HEADER 1

#if __has_include("Basics/VelocyPackHelper")
    #include "Basics/VelocyPackHelper.h"
#else
    #include <cstdint>
namespace arangodb::basics {
class VelocyPackHelper {
    public:
    static uint8_t const KeyAttribute = 0x31;
    static uint8_t const RevAttribute = 0x32;
    static uint8_t const IdAttribute = 0x33;
    static uint8_t const FromAttribute = 0x34;
    static uint8_t const ToAttribute = 0x35;

    static uint8_t const AttributeBase = 0x30;
};
} // namespace arangodb::basics
#endif

#include <string>
namespace arangodb::validation {
[[nodiscard]] std::string const& id_to_string(std::uint8_t);
}


#endif
