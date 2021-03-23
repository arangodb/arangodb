#ifndef IRESEARCH_ABSL_STRINGS_INTERNAL_STR_FORMAT_FLOAT_CONVERSION_H_
#define IRESEARCH_ABSL_STRINGS_INTERNAL_STR_FORMAT_FLOAT_CONVERSION_H_

#include "absl/strings/internal/str_format/extension.h"

namespace iresearch_absl {
IRESEARCH_ABSL_NAMESPACE_BEGIN
namespace str_format_internal {

bool ConvertFloatImpl(float v, const FormatConversionSpecImpl &conv,
                      FormatSinkImpl *sink);

bool ConvertFloatImpl(double v, const FormatConversionSpecImpl &conv,
                      FormatSinkImpl *sink);

bool ConvertFloatImpl(long double v, const FormatConversionSpecImpl &conv,
                      FormatSinkImpl *sink);

}  // namespace str_format_internal
IRESEARCH_ABSL_NAMESPACE_END
}  // namespace absl

#endif  // IRESEARCH_ABSL_STRINGS_INTERNAL_STR_FORMAT_FLOAT_CONVERSION_H_
