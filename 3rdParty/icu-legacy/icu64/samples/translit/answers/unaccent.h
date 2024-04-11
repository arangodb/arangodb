/***********************************************************************
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 ***********************************************************************
 ***********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1999-2002, International Business Machines Corporation and
 * others. All Rights Reserved.
 ***********************************************************************/

#include "unicode/translit.h"
#include "unicode/normlzr.h"

using icu::Normalizer;
using icu::Replaceable;
using icu::Transliterator;

class UnaccentTransliterator : public Transliterator {
    
 public:
    
    /**
     * Constructor
     */
    UnaccentTransliterator();

    /**
     * Destructor
     */
    virtual ~UnaccentTransliterator();

    virtual UClassID getDynamicClassID() const override;
    U_I18N_API static UClassID U_EXPORT2 getStaticClassID();

 protected:

    /**
     * Implement Transliterator API
     */
    virtual void handleTransliterate(Replaceable& text,
                                     UTransPosition& index,
                                     UBool incremental) const;

 private:

    /**
     * Unaccent a single character using normalizer.
     */
    char16_t unaccent(char16_t c) const;

    Normalizer normalizer;
};
