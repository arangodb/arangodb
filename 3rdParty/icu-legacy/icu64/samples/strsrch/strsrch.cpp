/*************************************************************************
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html
 *
 *************************************************************************
 *************************************************************************
 * COPYRIGHT:
 * Copyright (C) 2002-2006 IBM, Inc.   All Rights Reserved.
 *
 *************************************************************************/

/** 
 * This program demos string collation
 */

const char gHelpString[] =
    "usage: strsrch [options*] -source source_string -pattern pattern_string\n"
    "-help            Display this message.\n"
    "-locale name     ICU locale to use.  Default is en_US\n"
    "-rules rule      Collation rules file (overrides locale)\n"
    "-french          French accent ordering\n"
    "-norm            Normalizing mode on\n"
    "-shifted         Shifted mode\n"
    "-lower           Lower case first\n"
    "-upper           Upper case first\n"
    "-case            Enable separate case level\n"
    "-level n         Sort level, 1 to 5, for Primary, Secndary, Tertiary, Quaternary, Identical\n"
	"-source string   Source string\n"
	"-pattern string  Pattern string to look for in source\n"
	"-overlap         Enable searching to be done on overlapping patterns\n"
	"-canonical       Enable searching to be done matching canonical equivalent patterns"
    "Example strsrch -rules \\u0026b\\u003ca -source a\\u0020b\\u0020bc -pattern b\n"
	"The format \\uXXXX is supported for the rules and comparison strings\n"
	;

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <unicode/utypes.h>
#include <unicode/ucol.h>
#include <unicode/usearch.h>
#include <unicode/ustring.h>

/** 
 * Command line option variables
 *    These global variables are set according to the options specified
 *    on the command line by the user.
 */
char const *opt_locale      = "en_US";
char *opt_rules        = nullptr;
UBool  opt_help        = false;
UBool  opt_norm        = false;
UBool  opt_french      = false;
UBool  opt_shifted     = false;
UBool  opt_lower       = false;
UBool  opt_upper       = false;
UBool  opt_case        = false;
UBool  opt_overlap     = false;
UBool  opt_canonical   = false;
int    opt_level       = 0;
char const *opt_source      = "International Components for Unicode";
char const *opt_pattern     = "Unicode";
UCollator *collator    = nullptr;
UStringSearch *search  = nullptr;
char16_t rules[100];
char16_t source[100];
char16_t pattern[100];

/** 
 * Definitions for the command line options
 */
struct OptSpec {
    const char *name;
    enum {FLAG, NUM, STRING} type;
    void *pVar;
};

OptSpec opts[] = {
    {"-locale",      OptSpec::STRING, &opt_locale},
    {"-rules",       OptSpec::STRING, &opt_rules},
	{"-source",      OptSpec::STRING, &opt_source},
    {"-pattern",     OptSpec::STRING, &opt_pattern},
    {"-norm",        OptSpec::FLAG,   &opt_norm},
    {"-french",      OptSpec::FLAG,   &opt_french},
    {"-shifted",     OptSpec::FLAG,   &opt_shifted},
    {"-lower",       OptSpec::FLAG,   &opt_lower},
    {"-upper",       OptSpec::FLAG,   &opt_upper},
    {"-case",        OptSpec::FLAG,   &opt_case},
    {"-level",       OptSpec::NUM,    &opt_level},
	{"-overlap",     OptSpec::FLAG,   &opt_overlap},
	{"-canonical",   OptSpec::FLAG,   &opt_canonical},
    {"-help",        OptSpec::FLAG,   &opt_help},
    {"-?",           OptSpec::FLAG,   &opt_help},
    {nullptr,        OptSpec::FLAG,   nullptr}
};

/**  
 * processOptions()  Function to read the command line options.
 */
UBool processOptions(int argc, const char **argv, OptSpec opts[])
{
    for (int argNum = 1; argNum < argc; argNum ++) {
        const char *pArgName = argv[argNum];
        OptSpec *pOpt;
        for (pOpt = opts; pOpt->name != nullptr; pOpt++) {
            if (strcmp(pOpt->name, pArgName) == 0) {
                switch (pOpt->type) {
                case OptSpec::FLAG:
                    *(UBool *)(pOpt->pVar) = true;
                    break;
                case OptSpec::STRING:
                    argNum ++;
                    if (argNum >= argc) {
                        fprintf(stderr, "value expected for \"%s\" option.\n", 
							    pOpt->name);
                        return false;
                    }
                    *(const char **)(pOpt->pVar) = argv[argNum];
                    break;
                case OptSpec::NUM:
                    argNum ++;
                    if (argNum >= argc) {
                        fprintf(stderr, "value expected for \"%s\" option.\n", 
							    pOpt->name);
                        return false;
                    }
                    char *endp;
                    int i = strtol(argv[argNum], &endp, 0);
                    if (endp == argv[argNum]) {
                        fprintf(stderr, 
							    "integer value expected for \"%s\" option.\n", 
								pOpt->name);
                        return false;
                    }
                    *(int *)(pOpt->pVar) = i;
                }
                break;
            }
        }
        if (pOpt->name == nullptr)
        {
            fprintf(stderr, "Unrecognized option \"%s\"\n", pArgName);
            return false;
        }
    }
	return true;
}

/**
 * Creates a collator
 */
UBool processCollator()
{
	// Set up an ICU collator
    UErrorCode status = U_ZERO_ERROR;

    if (opt_rules != nullptr) {
		u_unescape(opt_rules, rules, 100);
        collator = ucol_openRules(rules, -1, UCOL_OFF, UCOL_TERTIARY, 
			                  nullptr, &status);
    }
    else {
        collator = ucol_open(opt_locale, &status);
    }
	if (U_FAILURE(status)) {
        fprintf(stderr, "Collator creation failed.: %d\n", status);
        return false;
    }
    if (status == U_USING_DEFAULT_WARNING) {
        fprintf(stderr, "Warning, U_USING_DEFAULT_WARNING for %s\n", 
			    opt_locale);
    }
    if (status == U_USING_FALLBACK_WARNING) {
        fprintf(stderr, "Warning, U_USING_FALLBACK_ERROR for %s\n", 
			    opt_locale);
    }
    if (opt_norm) {
        ucol_setAttribute(collator, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    }
    if (opt_french) {
        ucol_setAttribute(collator, UCOL_FRENCH_COLLATION, UCOL_ON, &status);
    }
    if (opt_lower) {
        ucol_setAttribute(collator, UCOL_CASE_FIRST, UCOL_LOWER_FIRST, 
			              &status);
    }
    if (opt_upper) {
        ucol_setAttribute(collator, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, 
			              &status);
    }
    if (opt_case) {
        ucol_setAttribute(collator, UCOL_CASE_LEVEL, UCOL_ON, &status);
    }
    if (opt_shifted) {
        ucol_setAttribute(collator, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, 
			              &status);
    }
    if (opt_level != 0) {
        switch (opt_level) {
        case 1:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_PRIMARY, &status);
            break;
        case 2:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_SECONDARY, 
				              &status);
            break;
        case 3:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_TERTIARY, &status);
            break;
        case 4:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_QUATERNARY, 
				              &status);
            break;
        case 5:
            ucol_setAttribute(collator, UCOL_STRENGTH, UCOL_IDENTICAL, 
				              &status);
            break;
        default:
            fprintf(stderr, "-level param must be between 1 and 5\n");
            return false;
        }
    }
    if (U_FAILURE(status)) {
        fprintf(stderr, "Collator attribute setting failed.: %d\n", status);
        return false;
    }
	return true;
}

/**
 * Creates a string search
 */
UBool processStringSearch()
{
	u_unescape(opt_source, source, 100);
	u_unescape(opt_pattern, pattern, 100);
	UErrorCode status = U_ZERO_ERROR;
	search = usearch_openFromCollator(pattern, -1, source, -1, collator, nullptr, 
		                              &status);
	if (U_FAILURE(status)) {
		return false;
	}
	if (static_cast<bool>(opt_overlap)) {
		usearch_setAttribute(search, USEARCH_OVERLAP, USEARCH_ON, &status);
	}
	if (static_cast<bool>(opt_canonical)) {
		usearch_setAttribute(search, USEARCH_CANONICAL_MATCH, USEARCH_ON, 
			                 &status);
	}
	if (U_FAILURE(status)) {
		fprintf(stderr, "Error setting search attributes\n");
		return false;
	}
	return true;
}

UBool findPattern()
{
	UErrorCode status = U_ZERO_ERROR;
	int32_t offset = usearch_next(search, &status);
	if (offset == USEARCH_DONE) {
		fprintf(stdout, "Pattern not found in source\n");
	}
	while (offset != USEARCH_DONE) {
		fprintf(stdout, "Pattern found at offset %d size %d\n", offset,
				usearch_getMatchedLength(search));
		offset = usearch_next(search, &status);
	}
	if (U_FAILURE(status)) {
		fprintf(stderr, "Error in searching for pattern %d\n", status);
		return false;
	}
	fprintf(stdout, "End of search\n");
	return true;
}

/** 
 * Main   --  process command line, read in and pre-process the test file,
 *            call other functions to do the actual tests.
 */
int main(int argc, const char** argv) 
{
    if (!static_cast<bool>(processOptions(argc, argv, opts)) || static_cast<bool>(opt_help)) {
        printf(gHelpString);
        return -1;
    }

    if (!static_cast<bool>(processCollator())) {
		fprintf(stderr, "Error creating collator\n");
		return -1;
	}

	if (!static_cast<bool>(processStringSearch())) {
		fprintf(stderr, "Error creating string search\n");
		return -1;
	}

	fprintf(stdout, "Finding pattern %s in source %s\n", opt_pattern, 
		    opt_source);

	findPattern();
	ucol_close(collator);
	usearch_close(search);
	return 0;
}
