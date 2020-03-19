#ifndef ARANGO_SCHEMA_VALIADTION_TYPES_HEADER
#define ARANGO_SCHEMA_VALIADTION_TYPES_HEADER 1

#if __has_include("Basics/VelocyPackHelper.h")
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

enum class SpecialProperties {
    None = 0,
    Key = 1,
    Id = 2,
    Rev = 4,
    From = 8,
    To = 16,
    All = 31,
};

[[nodiscard]] std::string const& id_to_string(std::uint8_t);

[[nodiscard]] inline bool enum_contains_value(SpecialProperties tested, SpecialProperties test_for) {
    auto tested_int = static_cast<std::underlying_type_t<SpecialProperties>>(tested);
    auto test_for_int = static_cast<std::underlying_type_t<SpecialProperties>>(test_for);
    return tested_int & test_for_int;
}

[[nodiscard]] SpecialProperties view_to_special(std::string_view);
[[nodiscard]] bool skip_special(std::string_view, SpecialProperties);

} // namespace arangodb::validation

#endif
