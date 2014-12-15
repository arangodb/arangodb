/*
******************************************************************************
* Copyright (C) 1996-2013, International Business Machines Corporation and
* others. All Rights Reserved.
******************************************************************************
*/

/**
 * \file 
 * \brief C++ API: RuleBasedCollator class provides the simple implementation of Collator.
 */

/**
* File tblcoll.h
*
* Created by: Helena Shih
*
* Modification History:
*
*  Date        Name        Description
*  2/5/97      aliu        Added streamIn and streamOut methods.  Added
*                          constructor which reads RuleBasedCollator object from
*                          a binary file.  Added writeToFile method which streams
*                          RuleBasedCollator out to a binary file.  The streamIn
*                          and streamOut methods use istream and ostream objects
*                          in binary mode.
*  2/12/97     aliu        Modified to use TableCollationData sub-object to
*                          hold invariant data.
*  2/13/97     aliu        Moved several methods into this class from Collation.
*                          Added a private RuleBasedCollator(Locale&) constructor,
*                          to be used by Collator::createDefault().  General
*                          clean up.
*  2/20/97     helena      Added clone, operator==, operator!=, operator=, and copy
*                          constructor and getDynamicClassID.
*  3/5/97      aliu        Modified constructFromFile() to add parameter
*                          specifying whether or not binary loading is to be
*                          attempted.  This is required for dynamic rule loading.
* 05/07/97     helena      Added memory allocation error detection.
*  6/17/97     helena      Added IDENTICAL strength for compare, changed getRules to
*                          use MergeCollation::getPattern.
*  6/20/97     helena      Java class name change.
*  8/18/97     helena      Added internal API documentation.
* 09/03/97     helena      Added createCollationKeyValues().
* 02/10/98     damiba      Added compare with "length" parameter
* 08/05/98     erm         Synched with 1.2 version of RuleBasedCollator.java
* 04/23/99     stephen     Removed EDecompositionMode, merged with
*                          Normalizer::EMode
* 06/14/99     stephen     Removed kResourceBundleSuffix
* 11/02/99     helena      Collator performance enhancements.  Eliminates the
*                          UnicodeString construction and special case for NO_OP.
* 11/23/99     srl         More performance enhancements. Updates to NormalizerIterator
*                          internal state management.
* 12/15/99     aliu        Update to support Thai collation.  Move NormalizerIterator
*                          to implementation file.
* 01/29/01     synwee      Modified into a C++ wrapper which calls C API
*                          (ucol.h)
*/

#ifndef TBLCOLL_H
#define TBLCOLL_H

#include "unicode/utypes.h"

 
#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/ucol.h"
#include "unicode/sortkey.h"
#include "unicode/normlzr.h"

U_NAMESPACE_BEGIN

/**
* @stable ICU 2.0
*/
class StringSearch;
/**
* @stable ICU 2.0
*/
class CollationElementIterator;

/**
 * The RuleBasedCollator class provides the simple implementation of
 * Collator, using data-driven tables. The user can create a customized
 * table-based collation.
 * <P>
 * <em>Important: </em>The ICU collation service has been reimplemented 
 * in order to achieve better performance and UCA compliance. 
 * For details, see the 
 * <a href="http://source.icu-project.org/repos/icu/icuhtml/trunk/design/collation/ICU_collation_design.htm">
 * collation design document</a>.
 * <p>
 * RuleBasedCollator is a thin C++ wrapper over the C implementation.
 * <p>
 * For more information about the collation service see 
 * <a href="http://icu-project.org/userguide/Collate_Intro.html">the users guide</a>.
 * <p>
 * Collation service provides correct sorting orders for most locales supported in ICU. 
 * If specific data for a locale is not available, the orders eventually falls back
 * to the <a href="http://www.unicode.org/unicode/reports/tr10/">UCA sort order</a>. 
 * <p>
 * Sort ordering may be customized by providing your own set of rules. For more on
 * this subject see the <a href="http://icu-project.org/userguide/Collate_Customization.html">
 * Collation customization</a> section of the users guide.
 * <p>
 * Note, RuleBasedCollator is not to be subclassed.
 * @see        Collator
 * @version    2.0 11/15/2001
 */
class U_I18N_API RuleBasedCollator : public Collator
{
public:

  // constructor -------------------------------------------------------------

    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator(const UnicodeString& rules, UErrorCode& status);

    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param collationStrength default strength for comparison
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator(const UnicodeString& rules,
                       ECollationStrength collationStrength,
                       UErrorCode& status);

    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param decompositionMode the normalisation mode
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator(const UnicodeString& rules,
                    UColAttributeValue decompositionMode,
                    UErrorCode& status);

    /**
     * RuleBasedCollator constructor. This takes the table rules and builds a
     * collation table out of them. Please see RuleBasedCollator class
     * description for more details on the collation rule syntax.
     * @param rules the collation rules to build the collation table from.
     * @param collationStrength default strength for comparison
     * @param decompositionMode the normalisation mode
     * @param status reporting a success or an error.
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator(const UnicodeString& rules,
                    ECollationStrength collationStrength,
                    UColAttributeValue decompositionMode,
                    UErrorCode& status);

    /**
     * Copy constructor.
     * @param other the RuleBasedCollator object to be copied
     * @see Locale
     * @stable ICU 2.0
     */
    RuleBasedCollator(const RuleBasedCollator& other);


    /** Opens a collator from a collator binary image created using
    *  cloneBinary. Binary image used in instantiation of the 
    *  collator remains owned by the user and should stay around for 
    *  the lifetime of the collator. The API also takes a base collator
    *  which usualy should be UCA.
    *  @param bin binary image owned by the user and required through the
    *             lifetime of the collator
    *  @param length size of the image. If negative, the API will try to
    *                figure out the length of the image
    *  @param base fallback collator, usually UCA. Base is required to be
    *              present through the lifetime of the collator. Currently 
    *              it cannot be NULL.
    *  @param status for catching errors
    *  @return newly created collator
    *  @see cloneBinary
    *  @stable ICU 3.4
    */
    RuleBasedCollator(const uint8_t *bin, int32_t length, 
                    const RuleBasedCollator *base, 
                    UErrorCode &status);
    // destructor --------------------------------------------------------------

    /**
     * Destructor.
     * @stable ICU 2.0
     */
    virtual ~RuleBasedCollator();

    // public methods ----------------------------------------------------------

    /**
     * Assignment operator.
     * @param other other RuleBasedCollator object to compare with.
     * @stable ICU 2.0
     */
    RuleBasedCollator& operator=(const RuleBasedCollator& other);

    /**
     * Returns true if argument is the same as this object.
     * @param other Collator object to be compared.
     * @return true if arguments is the same as this object.
     * @stable ICU 2.0
     */
    virtual UBool operator==(const Collator& other) const;

    /**
     * Makes a copy of this object.
     * @return a copy of this object, owned by the caller
     * @stable ICU 2.0
     */
    virtual Collator* clone(void) const;

    /**
     * Creates a collation element iterator for the source string. The caller of
     * this method is responsible for the memory management of the return
     * pointer.
     * @param source the string over which the CollationElementIterator will
     *        iterate.
     * @return the collation element iterator of the source string using this as
     *         the based Collator.
     * @stable ICU 2.2
     */
    virtual CollationElementIterator* createCollationElementIterator(
                                           const UnicodeString& source) const;

    /**
     * Creates a collation element iterator for the source. The caller of this
     * method is responsible for the memory management of the returned pointer.
     * @param source the CharacterIterator which produces the characters over
     *        which the CollationElementItgerator will iterate.
     * @return the collation element iterator of the source using this as the
     *         based Collator.
     * @stable ICU 2.2
     */
    virtual CollationElementIterator* createCollationElementIterator(
                                         const CharacterIterator& source) const;

    // Make deprecated versions of Collator::compare() visible.
    using Collator::compare;

    /**
    * The comparison function compares the character data stored in two
    * different strings. Returns information about whether a string is less 
    * than, greater than or equal to another string.
    * @param source the source string to be compared with.
    * @param target the string that is to be compared with the source string.
    * @param status possible error code
    * @return Returns an enum value. UCOL_GREATER if source is greater
    * than target; UCOL_EQUAL if source is equal to target; UCOL_LESS if source is less
    * than target
    * @stable ICU 2.6
    **/
    virtual UCollationResult compare(const UnicodeString& source,
                                      const UnicodeString& target,
                                      UErrorCode &status) const;

    /**
    * Does the same thing as compare but limits the comparison to a specified 
    * length
    * @param source the source string to be compared with.
    * @param target the string that is to be compared with the source string.
    * @param length the length the comparison is limited to
    * @param status possible error code
    * @return Returns an enum value. UCOL_GREATER if source (up to the specified 
    *         length) is greater than target; UCOL_EQUAL if source (up to specified 
    *         length) is equal to target; UCOL_LESS if source (up to the specified 
    *         length) is less  than target.
    * @stable ICU 2.6
    */
    virtual UCollationResult compare(const UnicodeString& source,
                                      const UnicodeString& target,
                                      int32_t length,
                                      UErrorCode &status) const;

    /**
    * The comparison function compares the character data stored in two
    * different string arrays. Returns information about whether a string array 
    * is less than, greater than or equal to another string array.
    * @param source the source string array to be compared with.
    * @param sourceLength the length of the source string array.  If this value
    *        is equal to -1, the string array is null-terminated.
    * @param target the string that is to be compared with the source string.
    * @param targetLength the length of the target string array.  If this value
    *        is equal to -1, the string array is null-terminated.
    * @param status possible error code
    * @return Returns an enum value. UCOL_GREATER if source is greater
    * than target; UCOL_EQUAL if source is equal to target; UCOL_LESS if source is less
    * than target
    * @stable ICU 2.6
    */
    virtual UCollationResult compare(const UChar* source, int32_t sourceLength,
                                      const UChar* target, int32_t targetLength,
                                      UErrorCode &status) const;

    /**
     * Compares two strings using the Collator.
     * Returns whether the first one compares less than/equal to/greater than
     * the second one.
     * This version takes UCharIterator input.
     * @param sIter the first ("source") string iterator
     * @param tIter the second ("target") string iterator
     * @param status ICU status
     * @return UCOL_LESS, UCOL_EQUAL or UCOL_GREATER
     * @stable ICU 4.2
     */
    virtual UCollationResult compare(UCharIterator &sIter,
                                     UCharIterator &tIter,
                                     UErrorCode &status) const;

    /**
    * Transforms a specified region of the string into a series of characters
    * that can be compared with CollationKey.compare. Use a CollationKey when
    * you need to do repeated comparisions on the same string. For a single
    * comparison the compare method will be faster.
    * @param source the source string.
    * @param key the transformed key of the source string.
    * @param status the error code status.
    * @return the transformed key.
    * @see CollationKey
    * @stable ICU 2.0
    */
    virtual CollationKey& getCollationKey(const UnicodeString& source,
                                          CollationKey& key,
                                          UErrorCode& status) const;

    /**
    * Transforms a specified region of the string into a series of characters
    * that can be compared with CollationKey.compare. Use a CollationKey when
    * you need to do repeated comparisions on the same string. For a single
    * comparison the compare method will be faster.
    * @param source the source string.
    * @param sourceLength the length of the source string.
    * @param key the transformed key of the source string.
    * @param status the error code status.
    * @return the transformed key.
    * @see CollationKey
    * @stable ICU 2.0
    */
    virtual CollationKey& getCollationKey(const UChar *source,
                                          int32_t sourceLength,
                                          CollationKey& key,
                                          UErrorCode& status) const;

    /**
     * Generates the hash code for the rule-based collation object.
     * @return the hash code.
     * @stable ICU 2.0
     */
    virtual int32_t hashCode(void) const;

    /**
    * Gets the locale of the Collator
    * @param type can be either requested, valid or actual locale. For more
    *             information see the definition of ULocDataLocaleType in
    *             uloc.h
    * @param status the error code status.
    * @return locale where the collation data lives. If the collator
    *         was instantiated from rules, locale is empty.
    * @deprecated ICU 2.8 likely to change in ICU 3.0, based on feedback
    */
    virtual Locale getLocale(ULocDataLocaleType type, UErrorCode& status) const;

    /**
     * Gets the tailoring rules for this collator.
     * @return the collation tailoring from which this collator was created
     * @stable ICU 2.0
     */
    const UnicodeString& getRules(void) const;

    /**
     * Gets the version information for a Collator.
     * @param info the version # information, the result will be filled in
     * @stable ICU 2.0
     */
    virtual void getVersion(UVersionInfo info) const;

#ifndef U_HIDE_DEPRECATED_API 
    /**
     * Returns the maximum length of any expansion sequences that end with the
     * specified comparison order.
     *
     * This is specific to the kind of collation element values and sequences
     * returned by the CollationElementIterator.
     * Call CollationElementIterator::getMaxExpansion() instead.
     *
     * @param order a collation order returned by CollationElementIterator::previous
     *              or CollationElementIterator::next.
     * @return maximum size of the expansion sequences ending with the collation
     *         element, or 1 if the collation element does not occur at the end of
     *         any expansion sequence
     * @see CollationElementIterator#getMaxExpansion
     * @deprecated ICU 51 Use CollationElementIterator::getMaxExpansion() instead.
     */
    int32_t getMaxExpansion(int32_t order) const;
#endif  /* U_HIDE_DEPRECATED_API */

    /**
     * Returns a unique class ID POLYMORPHICALLY. Pure virtual override. This
     * method is to implement a simple version of RTTI, since not all C++
     * compilers support genuine RTTI. Polymorphic operator==() and clone()
     * methods call this method.
     * @return The class ID for this object. All objects of a given class have
     *         the same class ID. Objects of other classes have different class
     *         IDs.
     * @stable ICU 2.0
     */
    virtual UClassID getDynamicClassID(void) const;

    /**
     * Returns the class ID for this class. This is useful only for comparing to
     * a return value from getDynamicClassID(). For example:
     * <pre>
     * Base* polymorphic_pointer = createPolymorphicObject();
     * if (polymorphic_pointer->getDynamicClassID() ==
     *                                          Derived::getStaticClassID()) ...
     * </pre>
     * @return The class ID for all objects of this class.
     * @stable ICU 2.0
     */
    static UClassID U_EXPORT2 getStaticClassID(void);

#ifndef U_HIDE_DEPRECATED_API 
    /**
     * Do not use this method: The caller and the ICU library might use different heaps.
     * Use cloneBinary() instead which writes to caller-provided memory.
     *
     * Returns a binary format of this collator.
     * @param length Returns the length of the data, in bytes
     * @param status the error code status.
     * @return memory, owned by the caller, of size 'length' bytes.
     * @deprecated ICU 52. Use cloneBinary() instead.
     */
    uint8_t *cloneRuleData(int32_t &length, UErrorCode &status);
#endif  /* U_HIDE_DEPRECATED_API */

    /** Creates a binary image of a collator. This binary image can be stored and 
    *  later used to instantiate a collator using ucol_openBinary.
    *  This API supports preflighting.
    *  @param buffer a fill-in buffer to receive the binary image
    *  @param capacity capacity of the destination buffer
    *  @param status for catching errors
    *  @return size of the image
    *  @see ucol_openBinary
    *  @stable ICU 3.4
    */
    int32_t cloneBinary(uint8_t *buffer, int32_t capacity, UErrorCode &status);

    /**
     * Returns current rules. Delta defines whether full rules are returned or
     * just the tailoring.
     *
     * getRules(void) should normally be used instead.
     * See http://userguide.icu-project.org/collation/customization#TOC-Building-on-Existing-Locales
     * @param delta one of UCOL_TAILORING_ONLY, UCOL_FULL_RULES.
     * @param buffer UnicodeString to store the result rules
     * @stable ICU 2.2
     * @see UCOL_FULL_RULES
     */
    void getRules(UColRuleOption delta, UnicodeString &buffer);

    /**
     * Universal attribute setter
     * @param attr attribute type
     * @param value attribute value
     * @param status to indicate whether the operation went on smoothly or there were errors
     * @stable ICU 2.2
     */
    virtual void setAttribute(UColAttribute attr, UColAttributeValue value,
                              UErrorCode &status);

    /**
     * Universal attribute getter.
     * @param attr attribute type
     * @param status to indicate whether the operation went on smoothly or there were errors
     * @return attribute value
     * @stable ICU 2.2
     */
    virtual UColAttributeValue getAttribute(UColAttribute attr,
                                            UErrorCode &status) const;

    /**
     * Sets the variable top to a collation element value of a string supplied.
     * @param varTop one or more (if contraction) UChars to which the variable top should be set
     * @param len length of variable top string. If -1 it is considered to be zero terminated.
     * @param status error code. If error code is set, the return value is undefined. Errors set by this function are: <br>
     *    U_CE_NOT_FOUND_ERROR if more than one character was passed and there is no such a contraction<br>
     *    U_PRIMARY_TOO_LONG_ERROR if the primary for the variable top has more than two bytes
     * @return a 32 bit value containing the value of the variable top in upper 16 bits. Lower 16 bits are undefined
     * @stable ICU 2.0
     */
    virtual uint32_t setVariableTop(const UChar *varTop, int32_t len, UErrorCode &status);

    /**
     * Sets the variable top to a collation element value of a string supplied.
     * @param varTop an UnicodeString size 1 or more (if contraction) of UChars to which the variable top should be set
     * @param status error code. If error code is set, the return value is undefined. Errors set by this function are: <br>
     *    U_CE_NOT_FOUND_ERROR if more than one character was passed and there is no such a contraction<br>
     *    U_PRIMARY_TOO_LONG_ERROR if the primary for the variable top has more than two bytes
     * @return a 32 bit value containing the value of the variable top in upper 16 bits. Lower 16 bits are undefined
     * @stable ICU 2.0
     */
    virtual uint32_t setVariableTop(const UnicodeString &varTop, UErrorCode &status);

    /**
     * Sets the variable top to a collation element value supplied. Variable top is set to the upper 16 bits.
     * Lower 16 bits are ignored.
     * @param varTop CE value, as returned by setVariableTop or ucol)getVariableTop
     * @param status error code (not changed by function)
     * @stable ICU 2.0
     */
    virtual void setVariableTop(uint32_t varTop, UErrorCode &status);

    /**
     * Gets the variable top value of a Collator.
     * Lower 16 bits are undefined and should be ignored.
     * @param status error code (not changed by function). If error code is set, the return value is undefined.
     * @stable ICU 2.0
     */
    virtual uint32_t getVariableTop(UErrorCode &status) const;

    /**
     * Get an UnicodeSet that contains all the characters and sequences tailored in 
     * this collator.
     * @param status      error code of the operation
     * @return a pointer to a UnicodeSet object containing all the 
     *         code points and sequences that may sort differently than
     *         in the UCA. The object must be disposed of by using delete
     * @stable ICU 2.4
     */
    virtual UnicodeSet *getTailoredSet(UErrorCode &status) const;

    /**
     * Get the sort key as an array of bytes from an UnicodeString.
     * @param source string to be processed.
     * @param result buffer to store result in. If NULL, number of bytes needed
     *        will be returned.
     * @param resultLength length of the result buffer. If if not enough the
     *        buffer will be filled to capacity.
     * @return Number of bytes needed for storing the sort key
     * @stable ICU 2.0
     */
    virtual int32_t getSortKey(const UnicodeString& source, uint8_t *result,
                               int32_t resultLength) const;

    /**
     * Get the sort key as an array of bytes from an UChar buffer.
     * @param source string to be processed.
     * @param sourceLength length of string to be processed. If -1, the string
     *        is 0 terminated and length will be decided by the function.
     * @param result buffer to store result in. If NULL, number of bytes needed
     *        will be returned.
     * @param resultLength length of the result buffer. If if not enough the
     *        buffer will be filled to capacity.
     * @return Number of bytes needed for storing the sort key
     * @stable ICU 2.2
     */
    virtual int32_t getSortKey(const UChar *source, int32_t sourceLength,
                               uint8_t *result, int32_t resultLength) const;

    /**
     * Retrieves the reordering codes for this collator.
     * @param dest The array to fill with the script ordering.
     * @param destCapacity The length of dest. If it is 0, then dest may be NULL and the function
     *  will only return the length of the result without writing any of the result string (pre-flighting).
     * @param status A reference to an error code value, which must not indicate
     * a failure before the function call.
     * @return The length of the script ordering array.
     * @see ucol_setReorderCodes
     * @see Collator#getEquivalentReorderCodes
     * @see Collator#setReorderCodes
     * @stable ICU 4.8 
     */
     virtual int32_t getReorderCodes(int32_t *dest,
                                     int32_t destCapacity,
                                     UErrorCode& status) const;

    /**
     * Sets the ordering of scripts for this collator.
     * @param reorderCodes An array of script codes in the new order. This can be NULL if the 
     * length is also set to 0. An empty array will clear any reordering codes on the collator.
     * @param reorderCodesLength The length of reorderCodes.
     * @param status error code
     * @see Collator#getReorderCodes
     * @see Collator#getEquivalentReorderCodes
     * @stable ICU 4.8 
     */
     virtual void setReorderCodes(const int32_t* reorderCodes,
                                  int32_t reorderCodesLength,
                                  UErrorCode& status) ;

    /**
     * Retrieves the reorder codes that are grouped with the given reorder code. Some reorder
     * codes will be grouped and must reorder together.
     * @param reorderCode The reorder code to determine equivalence for. 
     * @param dest The array to fill with the script equivalene reordering codes.
     * @param destCapacity The length of dest. If it is 0, then dest may be NULL and the 
     * function will only return the length of the result without writing any of the result 
     * string (pre-flighting).
     * @param status A reference to an error code value, which must not indicate 
     * a failure before the function call.
     * @return The length of the of the reordering code equivalence array.
     * @see ucol_setReorderCodes
     * @see Collator#getReorderCodes
     * @see Collator#setReorderCodes
     * @stable ICU 4.8 
     */
    static int32_t U_EXPORT2 getEquivalentReorderCodes(int32_t reorderCode,
                                int32_t* dest,
                                int32_t destCapacity,
                                UErrorCode& status);

private:

    // private static constants -----------------------------------------------

    enum {
        /* need look up in .commit() */
        CHARINDEX = 0x70000000,
        /* Expand index follows */
        EXPANDCHARINDEX = 0x7E000000,
        /* contract indexes follows */
        CONTRACTCHARINDEX = 0x7F000000,
        /* unmapped character values */
        UNMAPPED = 0xFFFFFFFF,
        /* primary strength increment */
        PRIMARYORDERINCREMENT = 0x00010000,
        /* secondary strength increment */
        SECONDARYORDERINCREMENT = 0x00000100,
        /* tertiary strength increment */
        TERTIARYORDERINCREMENT = 0x00000001,
        /* mask off anything but primary order */
        PRIMARYORDERMASK = 0xffff0000,
        /* mask off anything but secondary order */
        SECONDARYORDERMASK = 0x0000ff00,
        /* mask off anything but tertiary order */
        TERTIARYORDERMASK = 0x000000ff,
        /* mask off ignorable char order */
        IGNORABLEMASK = 0x0000ffff,
        /* use only the primary difference */
        PRIMARYDIFFERENCEONLY = 0xffff0000,
        /* use only the primary and secondary difference */
        SECONDARYDIFFERENCEONLY = 0xffffff00,
        /* primary order shift */
        PRIMARYORDERSHIFT = 16,
        /* secondary order shift */
        SECONDARYORDERSHIFT = 8,
        /* starting value for collation elements */
        COLELEMENTSTART = 0x02020202,
        /* testing mask for primary low element */
        PRIMARYLOWZEROMASK = 0x00FF0000,
        /* reseting value for secondaries and tertiaries */
        RESETSECONDARYTERTIARY = 0x00000202,
        /* reseting value for tertiaries */
        RESETTERTIARY = 0x00000002,

        PRIMIGNORABLE = 0x0202
    };

    // private data members ---------------------------------------------------

    UBool dataIsOwned;

    UBool isWriteThroughAlias;

    /**
    * c struct for collation. All initialisation for it has to be done through
    * setUCollator().
    */
    UCollator *ucollator;

    /**
    * Rule UnicodeString
    */
    UnicodeString urulestring;

    // friend classes --------------------------------------------------------

    /**
    * Used to iterate over collation elements in a character source.
    */
    friend class CollationElementIterator;

    /**
    * Collator ONLY needs access to RuleBasedCollator(const Locale&,
    *                                                       UErrorCode&)
    */
    friend class Collator;

    /**
    * Searching over collation elements in a character source
    */
    friend class StringSearch;

    // private constructors --------------------------------------------------

    /**
     * Default constructor
     */
    RuleBasedCollator();

    /**
     * RuleBasedCollator constructor. This constructor takes a locale. The
     * only caller of this class should be Collator::createInstance(). If
     * createInstance() happens to know that the requested locale's collation is
     * implemented as a RuleBasedCollator, it can then call this constructor.
     * OTHERWISE IT SHOULDN'T, since this constructor ALWAYS RETURNS A VALID
     * COLLATION TABLE. It does this by falling back to defaults.
     * @param desiredLocale locale used
     * @param status error code status
     */
    RuleBasedCollator(const Locale& desiredLocale, UErrorCode& status);

    /**
     * common constructor implementation
     *
     * @param rules the collation rules to build the collation table from.
     * @param collationStrength default strength for comparison
     * @param decompositionMode the normalisation mode
     * @param status reporting a success or an error.
     */
    void
    construct(const UnicodeString& rules,
              UColAttributeValue collationStrength,
              UColAttributeValue decompositionMode,
              UErrorCode& status);

    // private methods -------------------------------------------------------

    /**
    * Creates the c struct for ucollator
    * @param locale desired locale
    * @param status error status
    */
    void setUCollator(const Locale& locale, UErrorCode& status);

    /**
    * Creates the c struct for ucollator
    * @param locale desired locale name
    * @param status error status
    */
    void setUCollator(const char* locale, UErrorCode& status);

    /**
    * Creates the c struct for ucollator. This used internally by StringSearch.
    * Hence the responsibility of cleaning up the ucollator is not done by
    * this RuleBasedCollator. The isDataOwned flag is set to FALSE.
    * @param collator new ucollator data
    */
    void setUCollator(UCollator *collator);

public:
#ifndef U_HIDE_INTERNAL_API
    /**
    * Get UCollator data struct. Used only by StringSearch & intltest.
    * @return UCollator data struct
    * @internal
    */
    const UCollator * getUCollator();
#endif  /* U_HIDE_INTERNAL_API */

protected:
   /**
    * Used internally by registraton to define the requested and valid locales.
    * @param requestedLocale the requsted locale
    * @param validLocale the valid locale
    * @param actualLocale the actual locale
    * @internal
    */
    virtual void setLocales(const Locale& requestedLocale, const Locale& validLocale, const Locale& actualLocale);

private:
    // if not owned and not a write through alias, copy the ucollator
    void checkOwned(void);

    // utility to init rule string used by checkOwned and construct
    void setRuleStringFromCollator();

public:
    /** Get the short definition string for a collator. This internal API harvests the collator's
     *  locale and the attribute set and produces a string that can be used for opening 
     *  a collator with the same properties using the ucol_openFromShortString API.
     *  This string will be normalized.
     *  The structure and the syntax of the string is defined in the "Naming collators"
     *  section of the users guide: 
     *  http://icu-project.org/userguide/Collate_Concepts.html#Naming_Collators
     *  This function supports preflighting.
     * 
     *  This is internal, and intended to be used with delegate converters.
     *
     *  @param locale a locale that will appear as a collators locale in the resulting
     *                short string definition. If NULL, the locale will be harvested 
     *                from the collator.
     *  @param buffer space to hold the resulting string
     *  @param capacity capacity of the buffer
     *  @param status for returning errors. All the preflighting errors are featured
     *  @return length of the resulting string
     *  @see ucol_openFromShortString
     *  @see ucol_normalizeShortDefinitionString
     *  @see ucol_getShortDefinitionString
     *  @internal
     */
    virtual int32_t internalGetShortDefinitionString(const char *locale,
                                                     char *buffer,
                                                     int32_t capacity,
                                                     UErrorCode &status) const;
};

// inline method implementation ---------------------------------------------

inline void RuleBasedCollator::setUCollator(const Locale &locale,
                                               UErrorCode &status)
{
    setUCollator(locale.getName(), status);
}


inline void RuleBasedCollator::setUCollator(UCollator     *collator)
{

    if (ucollator && dataIsOwned) {
        ucol_close(ucollator);
    }
    ucollator   = collator;
    dataIsOwned = FALSE;
    isWriteThroughAlias = TRUE;
    setRuleStringFromCollator();
}

#ifndef U_HIDE_INTERNAL_API
inline const UCollator * RuleBasedCollator::getUCollator()
{
    return ucollator;
}
#endif  /* U_HIDE_INTERNAL_API */

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_COLLATION */

#endif
