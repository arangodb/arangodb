// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 *
 *   Copyright (C) 2003-2014, International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *
 *******************************************************************************
 *   file name:  nptrans.h
 *   encoding:   UTF-8
 *   tab size:   8 (not used)
 *   indentation:4
 *
 *   created on: 2003feb1
 *   created by: Ram Viswanadha
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION
#if !UCONFIG_NO_IDNA

#include "nptrans.h"
#include "unicode/resbund.h"
#include "unicode/uniset.h"
#include "sprpimpl.h"
#include "cmemory.h"
#include "ustr_imp.h"
#include "intltest.h"

#ifdef NPTRANS_DEBUG 
#include <stdio.h>
#endif

const char NamePrepTransform::fgClassID=0;

//Factory method
NamePrepTransform* NamePrepTransform::createInstance(UParseError& parseError, UErrorCode& status){
    NamePrepTransform* transform = new NamePrepTransform(parseError, status);
    if(U_FAILURE(status)){
        delete transform;
        return nullptr;
    }
    return transform;
}

//constructor
NamePrepTransform::NamePrepTransform(UParseError& parseError, UErrorCode& status)
    : mapping(nullptr), unassigned(), prohibited(), labelSeparatorSet(), bundle(nullptr) {
        
    LocalPointer<Transliterator> lmapping;
    LocalUResourceBundlePointer   lbundle;

    const char* testDataName = IntlTest::loadTestData(status);
    
    if(U_FAILURE(status)){
        return;
    }
    
    lbundle.adoptInstead(ures_openDirect(testDataName,"idna_rules",&status));
    
    if(lbundle.isValid() && U_SUCCESS(status)){
        // create the mapping transliterator
        int32_t ruleLen = 0;
        const char16_t* ruleUChar = ures_getStringByKey(lbundle.getAlias(), "MapNFKC",&ruleLen, &status);
        int32_t mapRuleLen = 0;
        const char16_t *mapRuleUChar = ures_getStringByKey(lbundle.getAlias(), "MapNoNormalization", &mapRuleLen, &status);
        UnicodeString rule(mapRuleUChar, mapRuleLen);
        rule.append(ruleUChar, ruleLen);

        lmapping.adoptInstead( Transliterator::createFromRules(UnicodeString("NamePrepTransform", ""), rule,
                                                   UTRANS_FORWARD, parseError,status));
        if(U_FAILURE(status)) {
            return;
        }

        //create the unassigned set
        int32_t patternLen =0;
        const char16_t* pattern = ures_getStringByKey(lbundle.getAlias(),"UnassignedSet",&patternLen, &status);
        unassigned.applyPattern(UnicodeString(pattern, patternLen), status);

        //create prohibited set
        patternLen=0;
        pattern =  ures_getStringByKey(lbundle.getAlias(),"ProhibitedSet",&patternLen, &status);
        UnicodeString test(pattern,patternLen);
        prohibited.applyPattern(test,status);
#ifdef NPTRANS_DEBUG
        if(U_FAILURE(status)){
            printf("Construction of Unicode set failed\n");
        }

        if(U_SUCCESS(status)){
            if(prohibited.contains((char16_t) 0x644)){
                printf("The string contains 0x644 ... !!\n");
            }
            UnicodeString temp;
            prohibited.toPattern(temp,true);

            for(int32_t i=0;i<temp.length();i++){
                printf("%c", (char)temp.charAt(i));
            }
            printf("\n");
        }
#endif
        
        //create label separator set
        patternLen=0;
        pattern =  ures_getStringByKey(lbundle.getAlias(), "LabelSeparatorSet", &patternLen, &status);
        labelSeparatorSet.applyPattern(UnicodeString(pattern,patternLen),status);
    }

    if(U_SUCCESS(status) && (lmapping.isNull())) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    if (U_FAILURE(status)) {
        return;
    }
    mapping = lmapping.orphan();
    bundle  = lbundle.orphan();
}


UBool NamePrepTransform::isProhibited(UChar32 ch){ 
    return (UBool)(ch != ASCII_SPACE); 
}

NamePrepTransform::~NamePrepTransform(){
    delete mapping;
    mapping = nullptr;
    
    //close the bundle
    ures_close(bundle);
    bundle = nullptr;
}


int32_t NamePrepTransform::map(const char16_t* src, int32_t srcLength,
                        char16_t* dest, int32_t destCapacity,
                        UBool allowUnassigned,
                        UParseError* /*parseError*/,
                        UErrorCode& status ){

    if(U_FAILURE(status)){
        return 0;
    }
    //check arguments
    if(src==nullptr || srcLength<-1 || (dest==nullptr && destCapacity!=0)) {
        status=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    UnicodeString rsource(src,srcLength);
    // map the code points
    // transliteration also performs NFKC
    mapping->transliterate(rsource);
    
    const char16_t* buffer = rsource.getBuffer();
    int32_t bufLen = rsource.length();
    // check if unassigned
    if(allowUnassigned == false){
        int32_t bufIndex=0;
        UChar32 ch =0 ;
        for(;bufIndex<bufLen;){
            U16_NEXT(buffer, bufIndex, bufLen, ch);
            if(unassigned.contains(ch)){
                status = U_IDNA_UNASSIGNED_ERROR;
                return 0;
            }
        }
    }
    // check if there is enough room in the output
    if(bufLen < destCapacity){
        u_memcpy(dest, buffer, bufLen);
    }

    return u_terminateUChars(dest, destCapacity, bufLen, &status);
}


#define MAX_BUFFER_SIZE 300

int32_t NamePrepTransform::process( const char16_t* src, int32_t srcLength,
                                    char16_t* dest, int32_t destCapacity,
                                    UBool allowUnassigned,
                                    UParseError* parseError,
                                    UErrorCode& status ){
    // check error status
    if(U_FAILURE(status)){
        return 0;
    }
    
    //check arguments
    if(src==nullptr || srcLength<-1 || (dest==nullptr && destCapacity!=0)) {
        status=U_ILLEGAL_ARGUMENT_ERROR;
        return 0;
    }

    UnicodeString b1String;
    char16_t *b1 = b1String.getBuffer(MAX_BUFFER_SIZE);
    int32_t b1Len;

    int32_t b1Index = 0;
    UCharDirection direction=U_CHAR_DIRECTION_COUNT, firstCharDir=U_CHAR_DIRECTION_COUNT;
    UBool leftToRight=false, rightToLeft=false;

    b1Len = map(src, srcLength, b1, b1String.getCapacity(), allowUnassigned, parseError, status);
    b1String.releaseBuffer(b1Len);

    if(status == U_BUFFER_OVERFLOW_ERROR){
        // redo processing of string
        /* we do not have enough room so grow the buffer*/
        b1 = b1String.getBuffer(b1Len);
        status = U_ZERO_ERROR; // reset error
        b1Len = map(src, srcLength, b1, b1String.getCapacity(), allowUnassigned, parseError, status);
        b1String.releaseBuffer(b1Len);
    }

    if(U_FAILURE(status)){
        b1Len = 0;
        goto CLEANUP;
    }


    for(; b1Index<b1Len; ){

        UChar32 ch = 0;

        U16_NEXT(b1, b1Index, b1Len, ch);

        if(prohibited.contains(ch) && ch!=0x0020){
            status = U_IDNA_PROHIBITED_ERROR;
            b1Len = 0;
            goto CLEANUP;
        }

        direction = u_charDirection(ch);
        if(firstCharDir==U_CHAR_DIRECTION_COUNT){
            firstCharDir = direction;
        }
        if(direction == U_LEFT_TO_RIGHT){
            leftToRight = true;
        }
        if(direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC){
            rightToLeft = true;
        }
    }       
    
    // satisfy 2
    if( leftToRight == true && rightToLeft == true){
        status = U_IDNA_CHECK_BIDI_ERROR;
        b1Len = 0;
        goto CLEANUP;
    }

    //satisfy 3
    if( rightToLeft == true && 
        !((firstCharDir == U_RIGHT_TO_LEFT || firstCharDir == U_RIGHT_TO_LEFT_ARABIC) &&
          (direction == U_RIGHT_TO_LEFT || direction == U_RIGHT_TO_LEFT_ARABIC))
       ){
        status = U_IDNA_CHECK_BIDI_ERROR;
        return false;
    }

    if(b1Len <= destCapacity){
        u_memmove(dest, b1, b1Len);
    }

CLEANUP:
    return u_terminateUChars(dest, destCapacity, b1Len, &status);
}

UBool NamePrepTransform::isLabelSeparator(UChar32 ch, UErrorCode& status){
    // check error status
    if(U_FAILURE(status)){
        return false;
    }

    return labelSeparatorSet.contains(ch);
}

#endif /* #if !UCONFIG_NO_IDNA */
#endif /* #if !UCONFIG_NO_TRANSLITERATION */
