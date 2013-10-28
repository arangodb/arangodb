/*
*****************************************************************************************
* Copyright (C) 2013, International Business Machines
* Corporation and others. All Rights Reserved.
*****************************************************************************************
*/

#ifndef UDISPLAYCONTEXT_H
#define UDISPLAYCONTEXT_H

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

/* Dont hide with #ifndef U_HIDE_DRAFT_API, needed by virtual methods */
/**
 * \file
 * \brief C API: Display context types (enum values)
 */

/**
 * Display context types, for getting values of a particular setting.
 * Note, the specific numeric values are internal and may change.
 * @draft ICU 51
 */
enum UDisplayContextType {
#ifndef U_HIDE_DRAFT_API
    /**
     * Type to retrieve the dialect handling setting, e.g.
     * UDISPCTX_STANDARD_NAMES or UDISPCTX_DIALECT_NAMES.
     * @draft ICU 51
     */
    UDISPCTX_TYPE_DIALECT_HANDLING = 0,
    /**
     * Type to retrieve the capitalization context setting, e.g.
     * UDISPCTX_CAPITALIZATION_NONE, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
     * UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, etc.
     * @draft ICU 51
     */
    UDISPCTX_TYPE_CAPITALIZATION = 1
#endif /* U_HIDE_DRAFT_API */
};
/**
*  @draft ICU 51
*/
typedef enum UDisplayContextType UDisplayContextType;

/* Dont hide with #ifndef U_HIDE_DRAFT_API, needed by virtual methods */
/**
 * Display context settings.
 * Note, the specific numeric values are internal and may change.
 * @draft ICU 51
 */
enum UDisplayContext {
#ifndef U_HIDE_DRAFT_API
    /**
     * ================================
     * DIALECT_HANDLING can be set to one of UDISPCTX_STANDARD_NAMES or
     * UDISPCTX_DIALECT_NAMES. Use UDisplayContextType UDISPCTX_TYPE_DIALECT_HANDLING
     * to get the value.
     */
    /**
     * A possible setting for DIALECT_HANDLING:
     * use standard names when generating a locale name,
     * e.g. en_GB displays as 'English (United Kingdom)'.
     * @draft ICU 51
     */
    UDISPCTX_STANDARD_NAMES = (UDISPCTX_TYPE_DIALECT_HANDLING<<8) + 0,
    /**
     * A possible setting for DIALECT_HANDLING:
     * use dialect names, when generating a locale name,
     * e.g. en_GB displays as 'British English'.
     * @draft ICU 51
     */
    UDISPCTX_DIALECT_NAMES = (UDISPCTX_TYPE_DIALECT_HANDLING<<8) + 1,
    /**
     * ================================
     * CAPITALIZATION can be set to one of UDISPCTX_CAPITALIZATION_NONE,
     * UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,
     * UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
     * UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU, or
     * UDISPCTX_CAPITALIZATION_FOR_STANDALONE.
     * Use UDisplayContextType UDISPCTX_TYPE_CAPITALIZATION to get the value.
     */
    /**
     * The capitalization context to be used is unknown (this is the default value).
     * @draft ICU 51
     */
    UDISPCTX_CAPITALIZATION_NONE = (UDISPCTX_TYPE_CAPITALIZATION<<8) + 0,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for the middle of a sentence.
     * @draft ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE = (UDISPCTX_TYPE_CAPITALIZATION<<8) + 1,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for the beginning of a sentence.
     * @draft ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE = (UDISPCTX_TYPE_CAPITALIZATION<<8) + 2,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for a user-interface list or menu item.
     * @draft ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU = (UDISPCTX_TYPE_CAPITALIZATION<<8) + 3,
    /**
     * The capitalization context if a date, date symbol or display name is to be
     * formatted with capitalization appropriate for stand-alone usage such as an
     * isolated name on a calendar page.
     * @draft ICU 51
     */
    UDISPCTX_CAPITALIZATION_FOR_STANDALONE = (UDISPCTX_TYPE_CAPITALIZATION<<8) + 4
#endif /* U_HIDE_DRAFT_API */
};
/**
*  @draft ICU 51
*/
typedef enum UDisplayContext UDisplayContext;

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
