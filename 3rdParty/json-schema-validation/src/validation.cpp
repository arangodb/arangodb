#include <cassert>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <validation/validation.hpp>

#if __has_include("Basics/VelocyPackHelper")
    #include "Basics/VelocyPackHelper.h"
#else
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

namespace {
using namespace arangodb;
using namespace arangodb::validation;
tao::json::value slice_object_to_value(VPackSlice const& slice, VPackOptions const* options, VPackSlice const* base) {
    assert(slice.isObject());
    tao::json::value rv;
    rv.prepare_object();

    VPackObjectIterator it(slice, true);
    while (it.valid()) {
        velocypack::ValueLength length;
        VPackSlice key = it.key(false);

        if (key.isString()) {
            // regular attribute
            rv.try_emplace(key.copyString(), slice_to_value(it.value(), options, &slice));
        } else {
            // optimized code path for translated system attributes
            VPackSlice val = VPackSlice(key.begin() + 1);
            tao::json::value sub;
            if (val.isString()) {
                // value of _key, _id, _from, _to, and _rev is ASCII too
                sub = std::string(val.getString(length), length);
            } else {
                sub = slice_to_value(val, options, &slice);
            }


            using namespace arangodb::basics;
            uint8_t which = static_cast<uint8_t>(key.getUInt()) + VelocyPackHelper::AttributeBase;
            switch (which) {
                case VelocyPackHelper::KeyAttribute: {
                    rv.try_emplace("_key", std::move(sub));
                    break;
                }
                case VelocyPackHelper::RevAttribute: {
                    rv.try_emplace("_rev", std::move(sub));
                    break;
                }
                case VelocyPackHelper::IdAttribute: {
                    rv.try_emplace("_id", std::move(sub));
                    break;
                }
                case VelocyPackHelper::FromAttribute: {
                    rv.try_emplace("_from", std::move(sub));
                    break;
                }
                case VelocyPackHelper::ToAttribute: {
                    rv.try_emplace("_to", std::move(sub));
                    break;
                }
            }
        }

        it.next();
    }

    return rv;
}

tao::json::value slice_array_to_value(VPackSlice const& slice, VPackOptions const* options, VPackSlice const* base) {
    assert(slice.isArray());
    VPackArrayIterator it(slice);
    tao::json::value rv;
    auto a = rv.prepare_array();
    a.resize(it.size());

    while (it.valid()) {
        tao::json::value val = slice_to_value(it.value(), options, &slice);
        rv.push_back(std::move(val));
        it.next();
    }

    return rv;
}
} // namespace

namespace arangodb::validation {
bool validate(VPackSlice const, tao::json::schema const&) {
    // todo: implement validation on slice (use event interface)
    throw std::runtime_error("not implemented");
}

bool validate(tao::json::value const& doc, tao::json::schema const& schema) {
    return schema.validate(doc);
}

tao::json::value slice_to_value(VPackSlice const& slice, VPackOptions const* options, VPackSlice const* base) {
    tao::json::value rv;
    switch (slice.type()) {
        case VPackValueType::Null: {
            rv.set_null();
            return rv;
        }
        case VPackValueType::Bool: {
            rv.set_boolean(slice.getBool());
            return rv;
        }
        case VPackValueType::Double: {
            rv.set_double(slice.getDouble());
            return rv;
        }
        case VPackValueType::Int: {
            rv.set_signed(slice.getInt());
            return rv;
        }
        case VPackValueType::UInt: {
            rv.set_unsigned(slice.getUInt());
            return rv;
        }
        case VPackValueType::SmallInt: {
            rv.set_signed(slice.getNumericValue<int32_t>());
            return rv;
        }
        case VPackValueType::String: {
            rv.set_string(slice.copyString());
            return rv;
        }
        case VPackValueType::Array: {
            return slice_array_to_value(slice, options, base);
            return rv;
        }
        case VPackValueType::Object: {
            return slice_object_to_value(slice, options, base);
            return rv;
        }
        case VPackValueType::External: {
            return slice_to_value(VPackSlice(reinterpret_cast<uint8_t const*>(slice.getExternal())), options, base);
            return rv;
        }
        case VPackValueType::Custom: {
            if (options == nullptr || options->customTypeHandler == nullptr || base == nullptr) {

                throw std::runtime_error("Could not extract custom attribute.");
            }
            std::string id = options->customTypeHandler->toString(slice, options, *base);
            rv.set_string(std::move(id));
            return rv;
        }
        case VPackValueType::None:
        default:
            break;
            return rv;
    }
    return rv;
}

std::unique_ptr<VPackBuilder> ValueToSlice(tao::json::value doc) {
    throw std::runtime_error("not implemented");
    auto rv = std::make_unique<VPackBuilder>();
    VPackBuilder& b = *rv;
    return rv;
}

} // namespace arangodb::validation
