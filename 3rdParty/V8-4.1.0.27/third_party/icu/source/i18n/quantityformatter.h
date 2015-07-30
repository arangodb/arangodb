/*
******************************************************************************
* Copyright (C) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
******************************************************************************
* quantityformatter.h
*/

#ifndef __QUANTITY_FORMATTER_H__
#define __QUANTITY_FORMATTER_H__

#include "unicode/utypes.h"
#include "unicode/uobject.h"

#if !UCONFIG_NO_FORMATTING

U_NAMESPACE_BEGIN

class SimplePatternFormatter;
class UnicodeString;
class PluralRules;
class NumberFormat;
class Formattable;
class FieldPosition;

/**
 * A plural aware formatter that is good for expressing a single quantity and
 * a unit.
 * <p>
 * First use the add() methods to add a pattern for each plural variant.
 * There must be a pattern for the "other" variant.
 * Then use the format() method.
 * <p>
 * Concurrent calls only to const methods on a QuantityFormatter object are 
 * safe, but concurrent const and non-const method calls on a QuantityFormatter
 * object are not safe and require synchronization.
 * 
 */
class U_I18N_API QuantityFormatter : public UMemory {
public:
    /**
     * Default constructor.
     */
    QuantityFormatter();

    /**
     * Copy constructor.
     */
    QuantityFormatter(const QuantityFormatter& other);

    /**
     * Assignment operator
     */
    QuantityFormatter &operator=(const QuantityFormatter& other);

    /**
     * Destructor.
     */
    ~QuantityFormatter();

    /**
     * Removes all variants from this object including the "other" variant.
     */
    void reset();

    /**
      * Adds a plural variant.
      *
      * @param variant "zero", "one", "two", "few", "many", "other"
      * @param rawPattern the pattern for the variant e.g "{0} meters"
      * @param status any error returned here.
      * @return TRUE on success; FALSE if status was set to a non zero error.
      */
    UBool add(
            const char *variant,
            const UnicodeString &rawPattern,
            UErrorCode &status);

    /**
     * returns TRUE if this object has at least the "other" variant.
     */
    UBool isValid() const;

    /**
     * Gets the pattern formatter that would be used for a particular variant.
     * If isValid() returns TRUE, this method is guaranteed to return a
     * non-NULL value.
     */
    const SimplePatternFormatter *getByVariant(const char *variant) const;

    /**
     * Formats a quantity with this object appending the result to appendTo.
     * At least the "other" variant must be added to this object for this
     * method to work.
     * 
     * @param quantity the single quantity.
     * @param fmt formats the quantity
     * @param rules computes the plural variant to use.
     * @param appendTo result appended here.
     * @param status any error returned here.
     * @return appendTo
     */
    UnicodeString &format(
            const Formattable &quantity,
            const NumberFormat &fmt,
            const PluralRules &rules,
            UnicodeString &appendTo,
            FieldPosition &pos,
            UErrorCode &status) const;

private:
    SimplePatternFormatter *formatters[6];
};

U_NAMESPACE_END

#endif

#endif
