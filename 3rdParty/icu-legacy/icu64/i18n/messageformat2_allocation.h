// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#ifndef U_HIDE_DEPRECATED_API

#ifndef MESSAGEFORMAT2_UTILS_H
#define MESSAGEFORMAT2_UTILS_H

#if U_SHOW_CPLUSPLUS_API

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/unistr.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

namespace message2 {

    // Helpers

    template<typename T>
    static T* copyArray(const T* source, int32_t& len) { // `len` is an in/out param
        if (source == nullptr) {
            len = 0;
            return nullptr;
        }
        T* dest = new T[len];
        if (dest == nullptr) {
            // Set length to 0 to prevent the
            // array from being accessed
            len = 0;
        } else {
            for (int32_t i = 0; i < len; i++) {
                dest[i] = source[i];
            }
        }
        return dest;
    }

    template<typename T>
    static T* copyVectorToArray(const UVector& source, int32_t& len) {
        len = source.size();
        T* dest = new T[len];
        if (dest == nullptr) {
            // Set length to 0 to prevent the
            // array from being accessed
            len = 0;
        } else {
            for (int32_t i = 0; i < len; i++) {
                dest[i] = *(static_cast<T*>(source.elementAt(i)));
            }
        }
        return dest;
    }

    template<typename T>
    static T* moveVectorToArray(UVector& source, int32_t& len) {
        len = source.size();
        T* dest = new T[len];
        if (dest == nullptr) {
            // Set length to 0 to prevent the
            // array from being accessed
            len = 0;
        } else {
            for (int32_t i = 0; i < len; i++) {
                dest[i] = std::move(*static_cast<T*>(source.elementAt(i)));
            }
        }
        source.removeAllElements();
        return dest;
    }

    inline UVector* createUVectorNoAdopt(UErrorCode& status) {
        if (U_FAILURE(status)) {
            return nullptr;
        }
        LocalPointer<UVector> result(new UVector(status));
        if (U_FAILURE(status)) {
            return nullptr;
        }
        return result.orphan();
    }

    inline UVector* createUVector(UErrorCode& status) {
        UVector* result = createUVectorNoAdopt(status);
        if (U_FAILURE(status)) {
            return nullptr;
        }
        result->setDeleter(uprv_deleteUObject);
        return result;
    }

    static UBool stringsEqual(const UElement s1, const UElement s2) {
        return (*static_cast<UnicodeString*>(s1.pointer) == *static_cast<UnicodeString*>(s2.pointer));
    }

    inline UVector* createStringUVector(UErrorCode& status) {
        UVector* v = createUVector(status);
        if (U_FAILURE(status)) {
            return nullptr;
        }
        v->setComparer(stringsEqual);
        return v;
    }

    inline UVector* createStringVectorNoAdopt(UErrorCode& status) {
        UVector* v = createUVectorNoAdopt(status);
        if (U_FAILURE(status)) {
            return nullptr;
        }
        v->setComparer(stringsEqual);
        return v;
    }

    template<typename T>
    inline T* create(T&& node, UErrorCode& status) {
        if (U_FAILURE(status)) {
            return nullptr;
        }
        T* result = new T(std::move(node));
        if (result == nullptr) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
        return result;
    }

} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // MESSAGEFORMAT2_UTILS_H

#endif // U_HIDE_DEPRECATED_API
// eof
