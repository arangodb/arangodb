/*
**********************************************************************
* Copyright (c) 2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
*/
#ifndef SCIFORMATHELPER_H
#define SCIFORMATHELPER_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#ifndef U_HIDE_DRAFT_API

#include "unicode/unistr.h"

/**
 * \file 
 * \brief C++ API: Formatter for measure objects.
 */

U_NAMESPACE_BEGIN

class DecimalFormatSymbols;
class FieldPositionIterator;
class DecimalFormatStaticSets;

/**
 * A helper class for formatting numbers in standard scientific notation
 * instead of E notation.
 *
 * Sample code:
 * <pre>
 * UErrorCode status = U_ZERO_ERROR;
 * DecimalFormat *decfmt = (DecimalFormat *)
 *     NumberFormat::createScientificInstance("en", status);
 * UnicodeString appendTo;
 * FieldPositionIterator fpositer;
 * decfmt->format(1.23456e-78, appendTo, &fpositer, status);
 * ScientificFormatHelper helper(*decfmt->getDecimalFormatSymbols(), status);
 * UnicodeString result;
 *
 * // result = "1.23456x10<sup>-78</sup>"
 * helper.insertMarkup(appendTo, fpositer, "<sup>", "</sup>", result, status));
 * </pre>
 *
 * @see NumberFormat
 * @draft ICU 54
 */
class U_I18N_API ScientificFormatHelper : public UObject {
 public:
    /**
     * Constructor.
     * @param symbols  comes from DecimalFormat instance used for default
     *                 scientific notation.
     * @param status   any error reported here.
     * @draft ICU 54
     */
    ScientificFormatHelper(const DecimalFormatSymbols &symbols, UErrorCode& status);

    /**
     * Copy constructor.
     * @draft ICU 54
     */
    ScientificFormatHelper(const ScientificFormatHelper &other);

    /**
     * Assignment operator.
     * @draft ICU 54
     */
    ScientificFormatHelper &operator=(const ScientificFormatHelper &other);

    /**
     * Destructor.
     * @draft ICU 54
     */
    virtual ~ScientificFormatHelper();

    /**
     * Formats standard scientific notation by surrounding exponent with
     * html to make it superscript.
     * @param s           the original formatted scientific notation
     *                    e.g "6.02e23". s is output from
     *                    NumberFormat::createScientificInstance()->format().
     * @param fpi         the FieldPositionIterator from the format call.
     *                    fpi is output from
     *                    NumberFormat::createScientificInstance()->format().
     * @param beginMarkup the start html for the exponent e.g "<sup>"
     * @param endMarkup   the end html for the exponent e.g "</sup>"
     * @param result      standard scientific notation appended here.
     * @param status      any error returned here. When status is set to a
     *                    non-zero error, the value of result is unspecified,
     *                    and client should fallback to using s for scientific
     *                    notation.
     * @return the value stored in result.
     * @draft ICU 54
     */
    UnicodeString &insertMarkup(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        const UnicodeString &beginMarkup,
        const UnicodeString &endMarkup,
        UnicodeString &result,
        UErrorCode &status) const;

    /**
     * Formats standard scientific notation by using superscript unicode
     * points 0..9.
     * @param s           the original formatted scientific notation
     *                    e.g "6.02e23". s is output from
     *                    NumberFormat::createScientificInstance()->format().
     * @param fpi         the FieldPositionIterator from the format call.
     *                    fpi is output from
     *                    NumberFormat::createScientificInstance()->format().
     * @param result      standard scientific notation appended here.
     * @param status      any error returned here. When status is set to a
     *                    non-zero error, the value of result is unspecified,
     *                    and client should fallback to using s for scientific
     *                    notation.
     * @return the value stored in result.
     * @draft ICU 54
     */
    UnicodeString &toSuperscriptExponentDigits(
        const UnicodeString &s,
        FieldPositionIterator &fpi,
        UnicodeString &result,
        UErrorCode &status) const;
 private:
    UnicodeString fPreExponent;
    const DecimalFormatStaticSets *fStaticSets;
};

U_NAMESPACE_END

#endif /* U_HIDE_DRAFT_API */

#endif /* !UCONFIG_NO_FORMATTING */
#endif 
