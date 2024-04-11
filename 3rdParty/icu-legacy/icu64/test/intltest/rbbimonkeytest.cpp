// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/


#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_REGULAR_EXPRESSIONS && !UCONFIG_NO_FORMATTING

#include "rbbimonkeytest.h"
#include "unicode/utypes.h"
#include "unicode/brkiter.h"
#include "unicode/utf16.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"

#include "charstr.h"
#include "cmemory.h"
#include "cstr.h"
#include "uelement.h"
#include "uhash.h"

#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

using namespace icu;


void RBBIMonkeyTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* params) {
    fParams = params;            // Work around TESTCASE_AUTO not being able to pass params to test function.

    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testMonkey);
    TESTCASE_AUTO_END;
}

//---------------------------------------------------------------------------------------
//
//   class BreakRule implementation.
//
//---------------------------------------------------------------------------------------

BreakRule::BreakRule()      // :  all field default initialized.
{
}

BreakRule::~BreakRule() {}


//---------------------------------------------------------------------------------------
//
//   class BreakRules implementation.
//
//---------------------------------------------------------------------------------------
BreakRules::BreakRules(RBBIMonkeyImpl *monkeyImpl, UErrorCode &status)  :
        fMonkeyImpl(monkeyImpl), fBreakRules(status), fType(UBRK_COUNT) {
    fCharClasses.adoptInstead(uhash_open(uhash_hashUnicodeString,
                                         uhash_compareUnicodeString,
                                         nullptr,      // value comparator.
                                         &status));
    if (U_FAILURE(status)) {
        return;
    }
    uhash_setKeyDeleter(fCharClasses.getAlias(), uprv_deleteUObject);
    uhash_setValueDeleter(fCharClasses.getAlias(), uprv_deleteUObject);
    fBreakRules.setDeleter(uprv_deleteUObject);

    fCharClassList.adoptInstead(new UVector(status));

    fSetRefsMatcher.adoptInstead(new RegexMatcher(UnicodeString(
             "(?!(?:\\{|=|\\[:)[ \\t]{0,4})"              // Negative look behind for '{' or '=' or '[:'
                                                          //   (the identifier is a unicode property name or value)
             "(?<ClassName>[A-Za-z_][A-Za-z0-9_]*)"),     // The char class name
        0, status));

    // Match comments and blank lines. Matches will be replaced with "", stripping the comments from the rules.
    fCommentsMatcher.adoptInstead(new RegexMatcher(UnicodeString(
                "(^|(?<=;))"                    // Start either at start of line, or just after a ';' (look-behind for ';')
                "[ \\t]*+"                      //   Match white space.
                "(#.*)?+"                       //   Optional # plus whatever follows
                "\\R$"                          //   new-line at end of line.
            ), 0, status));

    // Match (initial parse) of a character class definition line.
    fClassDefMatcher.adoptInstead(new RegexMatcher(UnicodeString(
                "[ \\t]*"                                // leading white space
                "(?<ClassName>[A-Za-z_][A-Za-z0-9_]*)"   // The char class name
                "[ \\t]*=[ \\t]*"                        //   =
                "(?<ClassDef>.*?)"                       // The char class UnicodeSet expression
                "[ \\t]*;$"),                     // ; <end of line>
            0, status));

    // Match (initial parse) of a break rule line.
    fRuleDefMatcher.adoptInstead(new RegexMatcher(UnicodeString(
                "[ \\t]*"                                // leading white space
                "(?<RuleName>[A-Za-z_][A-Za-z0-9_.]*)"    // The rule name
                "[ \\t]*:[ \\t]*"                        //   :
                "(?<RuleDef>.*?)"                        // The rule definition
                "[ \\t]*;$"),                            // ; <end of line>
            0, status));

}


BreakRules::~BreakRules() {}


CharClass *BreakRules::addCharClass(const UnicodeString &name, const UnicodeString &definition, UErrorCode &status) {

    // Create the expanded definition for this char class,
    // replacing any set references with the corresponding definition.

    UnicodeString expandedDef;
    UnicodeString emptyString;
    fSetRefsMatcher->reset(definition);
    while (fSetRefsMatcher->find() && U_SUCCESS(status)) {
        const UnicodeString name =
                fSetRefsMatcher->group(fSetRefsMatcher->pattern().groupNumberFromName("ClassName", status), status);
        CharClass *nameClass = static_cast<CharClass *>(uhash_get(fCharClasses.getAlias(), &name));
        const UnicodeString &expansionForName = nameClass ? nameClass->fExpandedDef : name;

        fSetRefsMatcher->appendReplacement(expandedDef, emptyString, status);
        expandedDef.append(expansionForName);
    }
    fSetRefsMatcher->appendTail(expandedDef);

    // Verify that the expanded set definition is valid.

    if (fMonkeyImpl->fDumpExpansions) {
        printf("epandedDef: %s\n", CStr(expandedDef)());
    }

    LocalPointer<UnicodeSet> s(new UnicodeSet(expandedDef, USET_IGNORE_SPACE, nullptr, status), status);
    if (U_FAILURE(status)) {
        IntlTest::gTest->errln("%s:%d: error %s creating UnicodeSet %s\n    Expanded set definition: %s",
                               __FILE__, __LINE__, u_errorName(status), CStr(name)(), CStr(expandedDef)());
        return nullptr;
    }
    CharClass *cclass = new CharClass(name, definition, expandedDef, s.orphan());
    CharClass *previousClass = static_cast<CharClass *>(uhash_put(fCharClasses.getAlias(),
                                                        new UnicodeString(name),   // Key, owned by hash table.
                                                        cclass,                    // Value, owned by hash table.
                                                        &status));

    if (previousClass != nullptr) {
        // Duplicate class def.
        // These are legitimate, they are adjustments of an existing class.
        // TODO: will need to keep the old around when we handle tailorings.
        IntlTest::gTest->logln("Redefinition of character class %s\n", CStr(cclass->fName)());
        delete previousClass;
    }
    return cclass;
}


void BreakRules::addRule(const UnicodeString &name, const UnicodeString &definition, UErrorCode &status) {
    LocalPointer<BreakRule> thisRule(new BreakRule);
    thisRule->fName = name;
    thisRule->fRule = definition;

    // If the rule name contains embedded digits, pad the first numeric field to a fixed length with leading zeroes,
    // This gives a numeric sort order that matches Unicode UAX rule numbering conventions.
    UnicodeString emptyString;

    // Expand the char class definitions within the rule.
    fSetRefsMatcher->reset(definition);
    while (fSetRefsMatcher->find() && U_SUCCESS(status)) {
        const UnicodeString name =
                fSetRefsMatcher->group(fSetRefsMatcher->pattern().groupNumberFromName("ClassName", status), status);
        CharClass *nameClass = static_cast<CharClass *>(uhash_get(fCharClasses.getAlias(), &name));
        if (!nameClass) {
            IntlTest::gTest->errln("%s:%d char class \"%s\" unrecognized in rule \"%s\"",
                __FILE__, __LINE__, CStr(name)(), CStr(definition)());
        }
        const UnicodeString &expansionForName = nameClass ? nameClass->fExpandedDef : name;

        fSetRefsMatcher->appendReplacement(thisRule->fExpandedRule, emptyString, status);
        thisRule->fExpandedRule.append(expansionForName);
    }
    fSetRefsMatcher->appendTail(thisRule->fExpandedRule);

    // If rule begins with a '^' rule chaining is disallowed.
    // Strip off the '^' from the rule expression, and set the flag.
    if (thisRule->fExpandedRule.charAt(0) == u'^') {
        thisRule->fInitialMatchOnly = true;
        thisRule->fExpandedRule.remove(0, 1);
        thisRule->fExpandedRule.trim();
    }

    // Replace the divide sign (\u00f7) with a regular expression named capture.
    // When running the rules, a match that includes this group means we found a break position.

    int32_t dividePos = thisRule->fExpandedRule.indexOf((char16_t)0x00f7);
    if (dividePos >= 0) {
        thisRule->fExpandedRule.replace(dividePos, 1, UnicodeString("(?<BreakPosition>)"));
    }
    if (thisRule->fExpandedRule.indexOf((char16_t)0x00f7) != -1) {
        status = U_ILLEGAL_ARGUMENT_ERROR;   // TODO: produce a good error message.
    }

    // UAX break rule set definitions can be empty, just [].
    // Regular expression set expressions don't accept this. Substitute with [^\u0000-\U0010ffff], which
    // also matches nothing.

    static const char16_t emptySet[] = {(char16_t)0x5b, (char16_t)0x5d, 0};
    int32_t where = 0;
    while ((where = thisRule->fExpandedRule.indexOf(emptySet, 2, 0)) >= 0) {
        thisRule->fExpandedRule.replace(where, 2, UnicodeString("[^\\u0000-\\U0010ffff]"));
    }
    if (fMonkeyImpl->fDumpExpansions) {
        printf("fExpandedRule: %s\n", CStr(thisRule->fExpandedRule)());
    }

    // Compile a regular expression for this rule.
    thisRule->fRuleMatcher.adoptInstead(new RegexMatcher(thisRule->fExpandedRule, UREGEX_COMMENTS | UREGEX_DOTALL, status));
    if (U_FAILURE(status)) {
        IntlTest::gTest->errln("%s:%d Error creating regular expression for %s",
                __FILE__, __LINE__, CStr(thisRule->fExpandedRule)());
        return;
    }

    // Put this new rule into the vector of all Rules.
    fBreakRules.adoptElement(thisRule.orphan(), status);
}


bool BreakRules::setKeywordParameter(const UnicodeString &keyword, const UnicodeString &value, UErrorCode &status) {
    if (keyword == UnicodeString("locale")) {
        CharString localeName;
        localeName.append(CStr(value)(), -1, status);
        fLocale = Locale::createFromName(localeName.data());
        return true;
    }
    if (keyword == UnicodeString("type")) {
        if (value == UnicodeString("grapheme")) {
            fType = UBRK_CHARACTER;
        } else if (value == UnicodeString("word")) {
            fType = UBRK_WORD;
        } else if (value == UnicodeString("line")) {
            fType = UBRK_LINE;
        } else if (value == UnicodeString("sentence")) {
            fType = UBRK_SENTENCE;
        } else {
            IntlTest::gTest->errln("%s:%d Unrecognized break type %s", __FILE__, __LINE__,  CStr(value)());
        }
        return true;
    }
    // TODO: add tailoring base setting here.
    return false;
}

RuleBasedBreakIterator *BreakRules::createICUBreakIterator(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return nullptr;
    }
    RuleBasedBreakIterator *bi = nullptr;
    switch(fType) {
        case UBRK_CHARACTER:
            bi = dynamic_cast<RuleBasedBreakIterator *>(BreakIterator::createCharacterInstance(fLocale, status));
            break;
        case UBRK_WORD:
            bi = dynamic_cast<RuleBasedBreakIterator *>(BreakIterator::createWordInstance(fLocale, status));
            break;
        case UBRK_LINE:
            bi = dynamic_cast<RuleBasedBreakIterator *>(BreakIterator::createLineInstance(fLocale, status));
            break;
        case UBRK_SENTENCE:
            bi = dynamic_cast<RuleBasedBreakIterator *>(BreakIterator::createSentenceInstance(fLocale, status));
            break;
        default:
            IntlTest::gTest->errln("%s:%d Bad break iterator type of %d", __FILE__, __LINE__, fType);
            status = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return bi;
}


void BreakRules::compileRules(UCHARBUF *rules, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }

    UnicodeString emptyString;
    for (;;) {    // Loop once per input line.
        if (U_FAILURE(status)) {
            return;
        }
        int32_t lineLength = 0;
        const char16_t *lineBuf = ucbuf_readline(rules, &lineLength, &status);
        if (lineBuf == nullptr) {
            break;
        }
        UnicodeString line(lineBuf, lineLength);

        // Strip comment lines.
        fCommentsMatcher->reset(line);
        line = fCommentsMatcher->replaceFirst(emptyString, status);
        if (line.isEmpty()) {
            continue;
        }

        // Recognize character class definition and keyword lines
        fClassDefMatcher->reset(line);
        if (fClassDefMatcher->matches(status)) {
            UnicodeString className = fClassDefMatcher->group(fClassDefMatcher->pattern().groupNumberFromName("ClassName", status), status);
            UnicodeString classDef  = fClassDefMatcher->group(fClassDefMatcher->pattern().groupNumberFromName("ClassDef", status), status);
            if (fMonkeyImpl->fDumpExpansions) {
                printf("scanned class: %s = %s\n", CStr(className)(), CStr(classDef)());
            }
            if (setKeywordParameter(className, classDef, status)) {
                // The scanned item was "type = ..." or "locale = ...", etc.
                //   which are not actual character classes.
                continue;
            }
            addCharClass(className, classDef, status);
            continue;
        }

        // Recognize rule lines.
        fRuleDefMatcher->reset(line);
        if (fRuleDefMatcher->matches(status)) {
            UnicodeString ruleName = fRuleDefMatcher->group(fRuleDefMatcher->pattern().groupNumberFromName("RuleName", status), status);
            UnicodeString ruleDef  = fRuleDefMatcher->group(fRuleDefMatcher->pattern().groupNumberFromName("RuleDef", status), status);
            if (fMonkeyImpl->fDumpExpansions) {
                printf("scanned rule: %s : %s\n", CStr(ruleName)(), CStr(ruleDef)());
            }
            addRule(ruleName, ruleDef, status);
            continue;
        }

        IntlTest::gTest->errln("%s:%d: Unrecognized line in rule file %s: \"%s\"\n",
            __FILE__, __LINE__, fMonkeyImpl->fRuleFileName, CStr(line)());
    }

    // Build the vector of char classes, omitting the dictionary class if there is one.
    // This will be used when constructing the random text to be tested.

    // Also compute the "other" set, consisting of any characters not included in
    // one or more of the user defined sets.

    UnicodeSet otherSet((UChar32)0, 0x10ffff);
    int32_t pos = UHASH_FIRST;
    const UHashElement *el = nullptr;
    while ((el = uhash_nextElement(fCharClasses.getAlias(), &pos)) != nullptr) {
        const UnicodeString *ccName = static_cast<const UnicodeString *>(el->key.pointer);
        CharClass *cclass = static_cast<CharClass *>(el->value.pointer);
        // printf("    Adding %s\n", CStr(*ccName)());
        if (*ccName != cclass->fName) {
            IntlTest::gTest->errln("%s:%d: internal error, set names (%s, %s) inconsistent.\n",
                    __FILE__, __LINE__, CStr(*ccName)(), CStr(cclass->fName)());
        }
        const UnicodeSet *set = cclass->fSet.getAlias();
        otherSet.removeAll(*set);
        if (*ccName == UnicodeString("dictionary")) {
            fDictionarySet = *set;
        } else {
            fCharClassList->addElement(cclass, status);
        }
    }

    if (!otherSet.isEmpty()) {
        // fprintf(stderr, "have an other set.\n");
        UnicodeString pattern;
        CharClass *cclass = addCharClass(UnicodeString("__Others"), otherSet.toPattern(pattern), status);
        fCharClassList->addElement(cclass, status);
    }
}


const CharClass *BreakRules::getClassForChar(UChar32 c, int32_t *iter) const {
   int32_t localIter = 0;
   int32_t &it = iter? *iter : localIter;

   while (it < fCharClassList->size()) {
       const CharClass *cc = static_cast<const CharClass *>(fCharClassList->elementAt(it));
       ++it;
       if (cc->fSet->contains(c)) {
           return cc;
       }
    }
    return nullptr;
}

//---------------------------------------------------------------------------------------
//
//   class MonkeyTestData implementation.
//
//---------------------------------------------------------------------------------------

void MonkeyTestData::set(BreakRules *rules, IntlTest::icu_rand &rand, UErrorCode &status) {
    const int32_t dataLength = 1000;

    // Fill the test string with random characters.
    // First randomly pick a char class, then randomly pick a character from that class.
    // Exclude any characters from the dictionary set.

    // std::cout << "Populating Test Data" << std::endl;
    fRandomSeed = rand.getSeed();         // Save initial seed for use in error messages,
                                          // allowing recreation of failing data.
    fBkRules = rules;
    fString.remove();
    for (int32_t n=0; n<dataLength;) {
        int charClassIndex = rand() % rules->fCharClassList->size();
        const CharClass *cclass = static_cast<CharClass *>(rules->fCharClassList->elementAt(charClassIndex));
        if (cclass->fSet->size() == 0) {
            // Some rules or tailorings do end up with empty char classes.
            continue;
        }
        int32_t charIndex = rand() % cclass->fSet->size();
        UChar32 c = cclass->fSet->charAt(charIndex);
        if (U16_IS_TRAIL(c) && fString.length() > 0 && U16_IS_LEAD(fString.charAt(fString.length()-1))) {
            // Character classes may contain unpaired surrogates, e.g. Grapheme_Cluster_Break = Control.
            // Don't let random unpaired surrogates combine in the test data because they might
            // produce an unwanted dictionary character.
            continue;
        }

        if (!rules->fDictionarySet.contains(c)) {
            fString.append(c);
            ++n;
        }
    }

    // Reset each rule matcher regex with this new string.
    //    (Although we are always using the same string object, ICU regular expressions
    //    don't like the underlying string data changing without doing a reset).

    for (int32_t ruleNum=0; ruleNum<rules->fBreakRules.size(); ruleNum++) {
        BreakRule *rule = static_cast<BreakRule *>(rules->fBreakRules.elementAt(ruleNum));
            rule->fRuleMatcher->reset(fString);
    }

    // Init the expectedBreaks, actualBreaks and ruleForPosition strings (used as arrays).
    // Expected and Actual breaks are one longer than the input string; a non-zero value
    // will indicate a boundary preceding that position.

    clearActualBreaks();
    fExpectedBreaks  = fActualBreaks;
    fRuleForPosition = fActualBreaks;
    f2ndRuleForPos   = fActualBreaks;

    // Apply reference rules to find the expected breaks.

    fExpectedBreaks.setCharAt(0, (char16_t)1);  // Force an expected break before the start of the text.
                                             // ICU always reports a break there.
                                             // The reference rules do not have a means to do so.
    int32_t strIdx = 0;
    bool    initialMatch = true;             // True at start of text, and immediately after each boundary,
                                             // for control over rule chaining.
    while (strIdx < fString.length()) {
        BreakRule *matchingRule = nullptr;
        UBool      hasBreak = false;
        int32_t ruleNum = 0;
        int32_t matchStart = 0;
        int32_t matchEnd = 0;
        int32_t breakGroup = 0;
        for (ruleNum=0; ruleNum<rules->fBreakRules.size(); ruleNum++) {
            BreakRule *rule = static_cast<BreakRule *>(rules->fBreakRules.elementAt(ruleNum));
            if (rule->fInitialMatchOnly && !initialMatch) {
                // Skip checking this '^' rule. (No rule chaining)
                continue;
            }
            rule->fRuleMatcher->reset();
            if (rule->fRuleMatcher->lookingAt(strIdx, status)) {
                // A candidate rule match, check further to see if we take it or continue to check other rules.
                // Matches of zero or one codepoint count only if they also specify a break.
                matchStart = rule->fRuleMatcher->start(status);
                matchEnd = rule->fRuleMatcher->end(status);
                breakGroup = rule->fRuleMatcher->pattern().groupNumberFromName("BreakPosition", status);
                hasBreak = U_SUCCESS(status);
                if (status == U_REGEX_INVALID_CAPTURE_GROUP_NAME) {
                    status = U_ZERO_ERROR;
                }
                if (hasBreak || fString.moveIndex32(matchStart, 1) < matchEnd) {
                    matchingRule = rule;
                    break;
                }
            }
        }
        if (matchingRule == nullptr) {
            // No reference rule matched. This is an error in the rules that should never happen.
            IntlTest::gTest->errln("%s:%d Trouble with monkey test reference rules at position %d. ",
                 __FILE__, __LINE__, strIdx);
            dump(strIdx);
            status = U_INVALID_FORMAT_ERROR;
            return;
        }
        if (matchingRule->fRuleMatcher->group(status).length() == 0) {
            // Zero length rule match. This is also an error in the rule expressions.
            IntlTest::gTest->errln("%s:%d Zero length rule match.",
                __FILE__, __LINE__);
            status =  U_INVALID_FORMAT_ERROR;
            return;
        }

        // Record which rule matched over the length of the match.
        for (int i = matchStart; i < matchEnd; i++) {
            if (fRuleForPosition.charAt(i) == 0) {
                fRuleForPosition.setCharAt(i, (char16_t)ruleNum);
            } else {
                f2ndRuleForPos.setCharAt(i, (char16_t)ruleNum);
            }
        }

        // Break positions appear in rules as a matching named capture of zero length at the break position,
        //   the adjusted pattern contains (?<BreakPosition>)
        if (hasBreak) {
            int32_t breakPos = matchingRule->fRuleMatcher->start(breakGroup, status);
            if (U_FAILURE(status) || breakPos < 0) {
                // Rule specified a break, but that break wasn't part of the match, even
                // though the rule as a whole matched.
                // Can't happen with regular expressions derived from (equivalent to) ICU break rules.
                // Shouldn't get here.
                IntlTest::gTest->errln("%s:%d Internal Rule Error.", __FILE__, __LINE__);
                status =  U_INVALID_FORMAT_ERROR;
                break;
            }
            fExpectedBreaks.setCharAt(breakPos, (char16_t)1);
            // printf("recording break at %d\n", breakPos);
            // For the next iteration, pick up applying rules immediately after the break,
            // which may differ from end of the match. The matching rule may have included
            // context following the boundary that needs to be looked at again.
            strIdx = matchingRule->fRuleMatcher->end(breakGroup, status);
            initialMatch = true;
        } else {
            // Original rule didn't specify a break.
            // Continue applying rules starting on the last code point of this match.
            strIdx = fString.moveIndex32(matchEnd, -1);
            initialMatch = false;
            if (strIdx == matchStart) {
                // Match was only one code point, no progress if we continue.
                // Shouldn't get here, case is filtered out at top of loop.
                CharString ruleName;
                ruleName.appendInvariantChars(matchingRule->fName, status);
                IntlTest::gTest->errln("%s:%d Rule %s internal error",
                        __FILE__, __LINE__, ruleName.data());
                status = U_INVALID_FORMAT_ERROR;
                break;
            }
        }
        if (U_FAILURE(status)) {
            IntlTest::gTest->errln("%s:%d status = %s. Unexpected failure, perhaps problem internal to test.",
                __FILE__, __LINE__, u_errorName(status));
            break;
        }
    }
}

void MonkeyTestData::clearActualBreaks() {
    fActualBreaks.remove();
    // Actual Breaks length is one longer than the data string length, allowing
    //    for breaks before the first and after the last character in the data.
    for (int32_t i=0; i<=fString.length(); i++) {
        fActualBreaks.append((char16_t)0);
    }
}

void MonkeyTestData::dump(int32_t around) const {
    printf("\n"
           "         char                        break  Rule                     Character\n"
           "   pos   code   class                 R I   name                     name\n"
           "---------------------------------------------------------------------------------------------\n");

    int32_t start;
    int32_t end;

    if (around == -1) {
        start = 0;
        end = fString.length();
    } else {
        // Display context around a failure.
        start = fString.moveIndex32(around, -30);
        end = fString.moveIndex32(around, +30);
    }

    for (int charIdx = start; charIdx < end; charIdx=fString.moveIndex32(charIdx, 1)) {
        UErrorCode status = U_ZERO_ERROR;
        UChar32 c = fString.char32At(charIdx);
        const CharClass *cc = fBkRules->getClassForChar(c);
        CharString ccName;
        ccName.appendInvariantChars(cc->fName, status);
        CharString ruleName, secondRuleName;
        const BreakRule *rule = static_cast<BreakRule *>(fBkRules->fBreakRules.elementAt(fRuleForPosition.charAt(charIdx)));
        ruleName.appendInvariantChars(rule->fName, status);
        if (f2ndRuleForPos.charAt(charIdx) > 0) {
            const BreakRule *secondRule = static_cast<BreakRule *>(fBkRules->fBreakRules.elementAt(f2ndRuleForPos.charAt(charIdx)));
            secondRuleName.appendInvariantChars(secondRule->fName, status);
        }
        char cName[200];
        u_charName(c, U_EXTENDED_CHAR_NAME, cName, sizeof(cName), &status);

        printf("  %4.1d %6.4x   %-20s  %c %c   %-10s %-10s    %s\n",
            charIdx, c, ccName.data(),
            fExpectedBreaks.charAt(charIdx) ? '*' : '.',
            fActualBreaks.charAt(charIdx) ? '*' : '.',
            ruleName.data(), secondRuleName.data(), cName
        );
    }
}


//---------------------------------------------------------------------------------------
//
//   class RBBIMonkeyImpl
//
//---------------------------------------------------------------------------------------

RBBIMonkeyImpl::RBBIMonkeyImpl(UErrorCode &status) : fDumpExpansions(false), fThread(this) {
    (void)status;    // suppress unused parameter compiler warning.
}


// RBBIMonkeyImpl setup       does all of the setup for a single rule set - compiling the
//                            reference rules and creating the icu breakiterator to test,
//                            with its type and locale coming from the reference rules.

void RBBIMonkeyImpl::setup(const char *ruleFile, UErrorCode &status) {
    fRuleFileName = ruleFile;
    openBreakRules(ruleFile, status);
    if (U_FAILURE(status)) {
        IntlTest::gTest->errln("%s:%d Error %s opening file %s.", __FILE__, __LINE__, u_errorName(status), ruleFile);
        return;
    }
    fRuleSet.adoptInstead(new BreakRules(this, status));
    fRuleSet->compileRules(fRuleCharBuffer.getAlias(), status);
    if (U_FAILURE(status)) {
        IntlTest::gTest->errln("%s:%d Error %s processing file %s.", __FILE__, __LINE__, u_errorName(status), ruleFile);
        return;
    }
    fBI.adoptInstead(fRuleSet->createICUBreakIterator(status));
    fTestData.adoptInstead(new MonkeyTestData());
}


RBBIMonkeyImpl::~RBBIMonkeyImpl() {
}


void RBBIMonkeyImpl::openBreakRules(const char *fileName, UErrorCode &status) {
    CharString path;
    path.append(IntlTest::getSourceTestData(status), status);
    path.append("break_rules" U_FILE_SEP_STRING, status);
    path.appendPathPart(fileName, status);
    const char *codePage = "UTF-8";
    fRuleCharBuffer.adoptInstead(ucbuf_open(path.data(), &codePage, true, false, &status));
}


void RBBIMonkeyImpl::startTest() {
    fThread.start();   // invokes runTest() in a separate thread.
}

void RBBIMonkeyImpl::join() {
    fThread.join();
}


#define MONKEY_ERROR(msg, index) UPRV_BLOCK_MACRO_BEGIN { \
    IntlTest::gTest->errln("%s:%d %s at index %d. Parameters to reproduce: @rules=%s,seed=%u,loop=1,verbose ", \
                    __FILE__, __LINE__, msg, index, fRuleFileName, fTestData->fRandomSeed); \
    if (fVerbose) { fTestData->dump(index); } \
    status = U_INVALID_STATE_ERROR;  \
} UPRV_BLOCK_MACRO_END

void RBBIMonkeyImpl::runTest() {
    UErrorCode status = U_ZERO_ERROR;
    int32_t errorCount = 0;
    for (int64_t loopCount = 0; fLoopCount < 0 || loopCount < fLoopCount; loopCount++) {
        status = U_ZERO_ERROR;
        fTestData->set(fRuleSet.getAlias(), fRandomGenerator, status);
        if (fBI.isNull()) {
            IntlTest::gTest->dataerrln("Unable to run test because fBI is null.");
            return;
        }
        // fTestData->dump();
        testForwards(status);
        testPrevious(status);
        testFollowing(status);
        testPreceding(status);
        testIsBoundary(status);
        testIsBoundaryRandom(status);

        if (fLoopCount < 0 && loopCount % 100 == 0) {
            fprintf(stderr, ".");
        }
        if (U_FAILURE(status)) {
            if (++errorCount > 10) {
                return;
            }
        }
    }
}

void RBBIMonkeyImpl::testForwards(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fTestData->clearActualBreaks();
    fBI->setText(fTestData->fString);
    int32_t previousBreak = -2;
    for (int32_t bk=fBI->first(); bk != BreakIterator::DONE; bk=fBI->next()) {
        if (bk <= previousBreak) {
            MONKEY_ERROR("Break Iterator Stall", bk);
            return;
        }
        if (bk < 0 || bk > fTestData->fString.length()) {
            MONKEY_ERROR("Boundary out of bounds", bk);
            return;
        }
        fTestData->fActualBreaks.setCharAt(bk, 1);
    }
    checkResults("testForwards", FORWARD, status);
}

void RBBIMonkeyImpl::testFollowing(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fTestData->clearActualBreaks();
    fBI->setText(fTestData->fString);
    int32_t nextBreak = -1;
    for (int32_t i=-1 ; i<fTestData->fString.length(); ++i) {
        int32_t bk = fBI->following(i);
        if (bk == BreakIterator::DONE && i == fTestData->fString.length()) {
            continue;
        }
        if (bk == nextBreak && bk > i) {
            // i is in the gap between two breaks.
            continue;
        }
        if (i == nextBreak && bk > nextBreak) {
            fTestData->fActualBreaks.setCharAt(bk, 1);
            nextBreak = bk;
            continue;
        }
        MONKEY_ERROR("following(i)", i);
        return;
    }
    checkResults("testFollowing", FORWARD, status);
}



void RBBIMonkeyImpl::testPrevious(UErrorCode &status) {
    if (U_FAILURE(status)) {return;}

    fTestData->clearActualBreaks();
    fBI->setText(fTestData->fString);
    int32_t previousBreak = INT32_MAX;
    for (int32_t bk=fBI->last(); bk != BreakIterator::DONE; bk=fBI->previous()) {
         if (bk >= previousBreak) {
            MONKEY_ERROR("Break Iterator Stall", bk);
            return;
        }
        if (bk < 0 || bk > fTestData->fString.length()) {
            MONKEY_ERROR("Boundary out of bounds", bk);
            return;
        }
        fTestData->fActualBreaks.setCharAt(bk, 1);
    }
    checkResults("testPrevius", REVERSE, status);
}


void RBBIMonkeyImpl::testPreceding(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fTestData->clearActualBreaks();
    fBI->setText(fTestData->fString);
    int32_t nextBreak = fTestData->fString.length()+1;
    for (int32_t i=fTestData->fString.length()+1 ; i>=0; --i) {
        int32_t bk = fBI->preceding(i);
        // printf("i:%d  bk:%d  nextBreak:%d\n", i, bk, nextBreak);
        if (bk == BreakIterator::DONE && i == 0) {
            continue;
        }
        if (bk == nextBreak && bk < i) {
            // i is in the gap between two breaks.
            continue;
        }
        if (i<fTestData->fString.length() && fTestData->fString.getChar32Start(i) < i) {
            // i indexes to a trailing surrogate.
            // Break Iterators treat an index to either half as referring to the supplemental code point,
            // with preceding going to some preceding code point.
            if (fBI->preceding(i) != fBI->preceding(fTestData->fString.getChar32Start(i))) {
                MONKEY_ERROR("preceding of trailing surrogate error", i);
            }
            continue;
        }
        if (i == nextBreak && bk < nextBreak) {
            fTestData->fActualBreaks.setCharAt(bk, 1);
            nextBreak = bk;
            continue;
        }
        MONKEY_ERROR("preceding(i)", i);
        return;
    }
    checkResults("testPreceding", REVERSE, status);
}


void RBBIMonkeyImpl::testIsBoundary(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fTestData->clearActualBreaks();
    fBI->setText(fTestData->fString);
    for (int i=fTestData->fString.length(); i>=0; --i) {
        if (fBI->isBoundary(i)) {
            fTestData->fActualBreaks.setCharAt(i, 1);
        }
    }
    checkResults("testForwards", FORWARD, status);
}

void RBBIMonkeyImpl::testIsBoundaryRandom(UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    fBI->setText(fTestData->fString);
    
    int stringLen = fTestData->fString.length();
    for (int i=stringLen; i>=0; --i) {
        int strIdx = fRandomGenerator() % stringLen;
        if (fTestData->fExpectedBreaks.charAt(strIdx) != fBI->isBoundary(strIdx)) {
            IntlTest::gTest->errln("%s:%d testIsBoundaryRandom failure at index %d. Parameters to reproduce: @rules=%s,seed=%u,loop=1,verbose ",
                    __FILE__, __LINE__, strIdx, fRuleFileName, fTestData->fRandomSeed);
            if (fVerbose) {
                fTestData->dump(i);
            }
            status = U_INVALID_STATE_ERROR;
            break;
        }
    }
}
        


void RBBIMonkeyImpl::checkResults(const char *msg, CheckDirection direction, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return;
    }
    if (direction == FORWARD) {
        for (int i=0; i<=fTestData->fString.length(); ++i) {
            if (fTestData->fExpectedBreaks.charAt(i) != fTestData->fActualBreaks.charAt(i)) {
                IntlTest::gTest->errln("%s:%d %s failure at index %d. Parameters to reproduce: @rules=%s,seed=%u,loop=1,verbose ",
                        __FILE__, __LINE__, msg, i, fRuleFileName, fTestData->fRandomSeed);
                if (fVerbose) {
                    fTestData->dump(i);
                }
                status = U_INVALID_STATE_ERROR;   // Prevent the test from continuing, which would likely
                break;                            // produce many redundant errors.
            }
        }
    } else {
        for (int i=fTestData->fString.length(); i>=0; i--) {
            if (fTestData->fExpectedBreaks.charAt(i) != fTestData->fActualBreaks.charAt(i)) {
                IntlTest::gTest->errln("%s:%d %s failure at index %d. Parameters to reproduce: @rules=%s,seed=%u,loop=1,verbose ",
                        __FILE__, __LINE__, msg, i, fRuleFileName, fTestData->fRandomSeed);
                if (fVerbose) {
                    fTestData->dump(i);
                }
                status = U_INVALID_STATE_ERROR;
                break;
            }
        }
    }
}



//---------------------------------------------------------------------------------------
//
//   class RBBIMonkeyTest implementation.
//
//---------------------------------------------------------------------------------------
RBBIMonkeyTest::RBBIMonkeyTest() {
}

RBBIMonkeyTest::~RBBIMonkeyTest() {
}


//     params, taken from this->fParams.
//       rules=file_name   Name of file containing the reference rules.
//       seed=nnnnn        Random number starting seed.
//                         Setting the seed allows errors to be reproduced.
//       loop=nnn          Looping count.  Controls running time.
//                         -1:  run forever.
//                          0 or greater:  run length.
//       expansions        debug option, show expansions of rules and sets.
//       verbose           Display details of the failure.
//
//     Parameters on the intltest command line follow the test name, and are preceded by '@'.
//     For example,
//           intltest rbbi/RBBIMonkeyTest/testMonkey@rules=line.txt,loop=-1
//
void RBBIMonkeyTest::testMonkey() {
    // printf("Test parameters: %s\n", fParams);
    UnicodeString params(fParams);
    UErrorCode status = U_ZERO_ERROR;

    const char *tests[] = {"grapheme.txt", "word.txt", "line.txt", "line_cj.txt", "sentence.txt", "line_normal.txt",
                           "line_normal_cj.txt", "line_loose.txt", "line_loose_cj.txt", "word_POSIX.txt",
                           nullptr };
    CharString testNameFromParams;
    if (getStringParam("rules", params, testNameFromParams, status)) {
        tests[0] = testNameFromParams.data();
        tests[1] = nullptr;
    }

    int64_t loopCount = quick? 100 : 5000;
    getIntParam("loop", params, loopCount, status);

    UBool dumpExpansions = false;
    getBoolParam("expansions", params, dumpExpansions, status);

    UBool verbose = false;
    getBoolParam("verbose", params, verbose, status);

    int64_t seed = 0;
    getIntParam("seed", params, seed, status);

    if (params.length() != 0) {
        // Options processing did not consume all of the parameters. Something unrecognized was present.
        CharString unrecognizedParameters;
        unrecognizedParameters.append(CStr(params)(), -1, status);
        errln("%s:%d unrecognized test parameter(s) \"%s\"", __FILE__, __LINE__, unrecognizedParameters.data());
        return;
    }

    UVector startedTests(status);
    if (U_FAILURE(status)) {
        errln("%s:%d: error %s while setting up test.", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    // Monkey testing is multi-threaded.
    // Each set of break rules to be tested is run in a separate thread.
    // Each thread/set of rules gets a separate RBBIMonkeyImpl object.
    int32_t i;
    for (i=0; tests[i] != nullptr; ++i) {
        logln("beginning testing of %s", tests[i]);
        LocalPointer<RBBIMonkeyImpl> test(new RBBIMonkeyImpl(status));
        if (U_FAILURE(status)) {
            dataerrln("%s:%d: error %s while starting test %s.", __FILE__, __LINE__, u_errorName(status), tests[i]);
            break;
        }
        test->fDumpExpansions = dumpExpansions;
        test->fVerbose = verbose;
        test->fRandomGenerator.seed(static_cast<uint32_t>(seed));
        test->fLoopCount = static_cast<int32_t>(loopCount);
        test->setup(tests[i], status);
        if (U_FAILURE(status)) {
            dataerrln("%s:%d: error %s while starting test %s.", __FILE__, __LINE__, u_errorName(status), tests[i]);
            break;
        }
        test->startTest();
        startedTests.addElement(test.orphan(), status);
        if (U_FAILURE(status)) {
            errln("%s:%d: error %s while starting test %s.", __FILE__, __LINE__, u_errorName(status), tests[i]);
            break;
        }
    }

    for (i=0; i<startedTests.size(); ++i) {
        RBBIMonkeyImpl *test = static_cast<RBBIMonkeyImpl *>(startedTests.elementAt(i));
        test->join();
        delete test;
    }
}


UBool  RBBIMonkeyTest::getIntParam(UnicodeString name, UnicodeString &params, int64_t &val, UErrorCode &status) {
    name.append(" *= *(-?\\d+) *,? *");
    RegexMatcher m(name, params, 0, status);
    if (m.find()) {
        // The param exists.  Convert the string to an int.
        CharString str;
        str.append(CStr(m.group(1, status))(), -1, status);
        val = strtol(str.data(),  nullptr, 10);

        // Delete this parameter from the params string.
        m.reset();
        params = m.replaceFirst(UnicodeString(), status);
        return true;
    }
    return false;
}

UBool RBBIMonkeyTest::getStringParam(UnicodeString name, UnicodeString &params, CharString &dest, UErrorCode &status) {
    name.append(" *= *([^ ,]*) *,? *");
    RegexMatcher m(name, params, 0, status);
    if (m.find()) {
        // The param exists.
        dest.append(CStr(m.group(1, status))(), -1, status);

        // Delete this parameter from the params string.
        m.reset();
        params = m.replaceFirst(UnicodeString(), status);
        return true;
    }
    return false;
}

UBool RBBIMonkeyTest::getBoolParam(UnicodeString name, UnicodeString &params, UBool &dest, UErrorCode &status) {
    name.append("(?: *= *(true|false))? *,? *");
    RegexMatcher m(name, params, UREGEX_CASE_INSENSITIVE, status);
    if (m.find()) {
        if (m.start(1, status) > 0) {
            // user option included a value.
            dest = m.group(1, status).caseCompare(UnicodeString("true"), U_FOLD_CASE_DEFAULT) == 0;
        } else {
            // No explicit user value, implies true.
            dest = true;
        }

        // Delete this parameter from the params string.
        m.reset();
        params = m.replaceFirst(UnicodeString(), status);
        return true;
    }
    return false;
}

#endif /* !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_REGULAR_EXPRESSIONS && !UCONFIG_NO_FORMATTING */
