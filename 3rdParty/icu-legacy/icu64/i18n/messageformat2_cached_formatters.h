// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#ifndef U_HIDE_DEPRECATED_API

#ifndef MESSAGEFORMAT2_CACHED_FORMATTERS_H
#define MESSAGEFORMAT2_CACHED_FORMATTERS_H

#if U_SHOW_CPLUSPLUS_API

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/messageformat2_data_model_names.h"
#include "unicode/messageformat2_function_registry.h"
#include "hash.h"

U_NAMESPACE_BEGIN

namespace message2 {

    using namespace data_model;

    // Formatter cache
    // --------------

    class MessageFormatter;

    // Map from function names to Formatters
    class CachedFormatters : public UObject {
    private:
        friend class MessageFormatter;

        // Maps stringified FunctionNames onto Formatter*
        // Adopts its values
        Hashtable cache;
        CachedFormatters() { cache.setValueDeleter(uprv_deleteUObject); }
    public:
        // Returns a pointer because Formatter is an abstract class
        const Formatter* getFormatter(const FunctionName& f) {
            return static_cast<const Formatter*>(cache.get(f));
        }
        // Adopts its argument
        void adoptFormatter(const FunctionName& f, Formatter* val, UErrorCode& status) {
            cache.put(f, val, status);
        }
        CachedFormatters& operator=(const CachedFormatters&) = delete;

        virtual ~CachedFormatters();
    };

} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // MESSAGEFORMAT2_CACHED_FORMATTERS_H

#endif // U_HIDE_DEPRECATED_API
// eof
