// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2008-2014, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
//! [getBestPatternExample1]
#include <iostream>
#include "unicode/smpdtfmt.h"
#include "unicode/dtptngen.h"
#include "unicode/ustdio.h"
//! [getBestPatternExample1]

using namespace std;
using namespace icu;

static void getBestPatternExample() {
	    
		u_printf("========================================================================\n");
		u_printf(" getBestPatternExample()\n");
        u_printf("\n");
        u_printf(" Use DateTimePatternGenerator to create customized date/time pattern:\n");
        u_printf(" yQQQQ,yMMMM, MMMMd, hhmm, jjmm per locale\n");
        u_printf("========================================================================\n");
		//! [getBestPatternExample]
	UnicodeString skeletons [] = {
		UnicodeString("yQQQQ"), // year + full name of quarter, i.e., 4th quarter 1999
        UnicodeString("yMMMM"), // year + full name of month, i.e., October 1999
        UnicodeString("MMMMd"), // full name of month + day of the month, i.e., October 25
        UnicodeString("hhmm"),  // 12-hour-cycle format, i.e., 1:32 PM
        UnicodeString("jjmm"), // preferred hour format for the given locale, i.e., 24-hour-cycle format for fr_FR
		0,
	};

	Locale locales[] = {
		Locale ("en_US"),
		Locale ("fr_FR"),
		Locale ("zh_CN"),
	};
	
	const char* filename = "sample.txt";
	/* open a UTF-8 file for writing */
	UFILE* f = u_fopen(filename, "w", NULL,"UTF-8");
	UnicodeString dateReturned;
	UErrorCode status =U_ZERO_ERROR;
	Calendar *cal = Calendar::createInstance(status);
	cal->set (1999,9,13,23,58,59);
	UDate date = cal->getTime(status);
	u_fprintf(f, "%-20S%-20S%-20S%-20S\n", UnicodeString("Skeleton").getTerminatedBuffer(),UnicodeString("en_US").getTerminatedBuffer(),UnicodeString("fr_FR").getTerminatedBuffer(),UnicodeString("zh_CN").getTerminatedBuffer());
	for (int i=0;skeletons[i]!=NULL;i++) {
		u_fprintf(f, "%-20S",skeletons[i].getTerminatedBuffer());
		for (int j=0;j<sizeof(locales)/sizeof(locales[0]);j++) {
			// create a DateTimePatternGenerator instance for given locale
			DateTimePatternGenerator *dtfg= DateTimePatternGenerator::createInstance(locales[j],status);
			// use getBestPattern method to get the best pattern for the given skeleton
			UnicodeString pattern = dtfg->getBestPattern(skeletons[i],status);
			// Constructs a SimpleDateFormat with the best pattern generated above and the given locale
			SimpleDateFormat *sdf = new SimpleDateFormat(pattern,locales[j],status);
			dateReturned.remove();
			// Get the format of the given date
			sdf->format(date,dateReturned,status);
			/* write Unicode string to file */
			u_fprintf(f, "%-20S", dateReturned.getTerminatedBuffer());
			delete dtfg;
			delete sdf;
		} 
		u_fprintf(f,"\n");
	}
	/* close the file resource */
	u_fclose(f);
	delete cal;
	//! [getBestPatternExample]
}

static void addPatternExample() {
		
		u_printf("========================================================================\n");
        u_printf(" addPatternExample()\n");
		u_printf("\n");
        u_printf(" Use addPattern API to add new '. von' to existing pattern\n");
        u_printf("========================================================================\n");
		//! [addPatternExample]
		UErrorCode status =U_ZERO_ERROR;
		UnicodeString conflictingPattern,dateReturned, pattern;
		Locale locale=Locale::getFrance();
		Calendar *cal = Calendar::createInstance(status);
		cal->set (1999,9,13,23,58,59);
		UDate date = cal->getTime(status);
        // Create an DateTimePatternGenerator instance for the given locale
		DateTimePatternGenerator *dtfg= DateTimePatternGenerator::createInstance(locale,status);
		SimpleDateFormat *sdf = new SimpleDateFormat(dtfg->getBestPattern("MMMMddHmm",status),locale,status);
        // Add '. von' to the existing pattern
        dtfg->addPattern("dd'. von' MMMM", true, conflictingPattern,status);
        // Apply the new pattern
        sdf->applyPattern(dtfg->getBestPattern("MMMMddHmm",status));
		dateReturned = sdf->format(date, dateReturned, status);
		pattern =sdf->toPattern(pattern);
		u_printf("%s\n", "New Pattern for FRENCH: ");
      	u_printf("%S\n", pattern.getTerminatedBuffer());
		u_printf("%s\n", "Date Time in new Pattern: ");
		u_printf("%S\n", dateReturned.getTerminatedBuffer());
		delete dtfg;
		delete sdf;
		delete cal;

		//! [addPatternExample]
        /* output of the sample code:
        ************************************************************************************************
         New Pattern for FRENCH: dd. 'von' MMMM HH:mm
         Date Time in new Pattern: 13. von octobre 23:58
     
        *************************************************************************************************/
 	}

static void replaceFieldTypesExample() {
		// Use repalceFieldTypes API to replace zone 'zzzz' with 'vvvv'
       u_printf("========================================================================\n");
       u_printf(" replaceFieldTypeExample()\n");
       u_printf("\n");
       u_printf(" Use replaceFieldTypes API to replace zone 'zzzz' with 'vvvv'\n");
       u_printf("========================================================================\n");
	   //! [replaceFieldTypesExample]
		UFILE *out = u_finit(stdout, NULL, "UTF-8");
		UErrorCode status =U_ZERO_ERROR;
		UnicodeString pattern,dateReturned;
		Locale locale =Locale::getFrance();
		Calendar *cal = Calendar::createInstance(status);
		cal->set (1999,9,13,23,58,59);
		UDate date = cal->getTime(status);
		TimeZone *zone = TimeZone::createTimeZone(UnicodeString("Europe/Paris"));
		DateTimePatternGenerator *dtfg = DateTimePatternGenerator::createInstance(locale,status);
	    SimpleDateFormat *sdf = new SimpleDateFormat("EEEE d MMMM y HH:mm:ss zzzz",locale,status);
		sdf->setTimeZone(*zone);
		pattern = sdf->toPattern(pattern);
		u_fprintf(out, "%S\n", UnicodeString("Pattern before replacement:").getTerminatedBuffer());
      	u_fprintf(out, "%S\n", pattern.getTerminatedBuffer());
		dateReturned.remove();
		dateReturned = sdf->format(date, dateReturned, status);
		u_fprintf(out, "%S\n", UnicodeString("Date/Time format in fr_FR:").getTerminatedBuffer());
		u_fprintf(out, "%S\n", dateReturned.getTerminatedBuffer());
        // Replace zone "zzzz" in the pattern with "vvvv"
		UnicodeString newPattern = dtfg->replaceFieldTypes(pattern, "vvvv", status);
		// Apply the new pattern
		sdf->applyPattern(newPattern);
		dateReturned.remove();
		dateReturned = sdf->format(date, dateReturned, status);
		u_fprintf(out, "%S\n", UnicodeString("Pattern after replacement:").getTerminatedBuffer());
     	u_fprintf(out, "%S\n", newPattern.getTerminatedBuffer());
     	u_fprintf(out, "%S\n", UnicodeString("Date/Time format in fr_FR:").getTerminatedBuffer());
		u_fprintf(out, "%S\n", dateReturned.getTerminatedBuffer());
		delete sdf;
		delete dtfg;
		delete zone;
		delete cal;
		u_fclose(out);
	//! [replaceFieldTypesExample]
    }

int main (int argc, char* argv[])
{
	getBestPatternExample();
	addPatternExample();
	replaceFieldTypesExample();
	return 0;
}
