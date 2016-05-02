/******************************************************************************
* Copyright (C) 2008-2014, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/
//! [dtitvfmtPreDefined1]
#include <iostream>
#include "unicode/dtitvfmt.h"
#include "unicode/ustdio.h"
//! [dtitvfmtPreDefined1]

using namespace std;

static void dtitvfmtPreDefined() {
	  
	u_printf("===============================================================================\n");
	u_printf(" dtitvfmtPreDefined()\n");
    u_printf("\n");
    u_printf(" Use DateIntervalFormat to get date interval format for pre-defined skeletons:\n");
    u_printf(" yMMMd, MMMMd, jm per locale\n");
    u_printf("===============================================================================\n");
	
	//! [dtitvfmtPreDefined] 
	UFILE *out = u_finit(stdout, NULL, "UTF-8");
	UErrorCode status =U_ZERO_ERROR;
	// create 3 Date Intervals
	UnicodeString data[] = {
		UnicodeString("2007-10-10 10:10:10"),
		UnicodeString("2008-10-10 10:10:10"),
		UnicodeString("2008-11-10 10:10:10"),
		UnicodeString("2008-11-10 15:10:10")
		};
	Calendar *cal = Calendar::createInstance(status);
	cal->set(2007,10,10,10,10,10);
	UDate date1 = cal->getTime(status);
	cal->set(2008,10,10,10,10,10);
	UDate date2 = cal->getTime(status);
	cal->set(2008,11,10,10,10,10);
	UDate date3 = cal->getTime(status);
	cal->set(2008,11,10,15,10,10);
	UDate date4 = cal->getTime(status);
    DateInterval* dtitvsample[] = {
			new DateInterval(date1,date2),
            new DateInterval(date2,date3),
			new DateInterval(date3,date4),
        };
 	UnicodeString skeletons[] = {
            UnicodeString("yMMMd"),
            UnicodeString("MMMMd"),
            UnicodeString("jm"),
			0,
		};
    u_fprintf(out,"%-10s%-22s%-22s%-35s%-35s\n", "Skeleton","from","to","Date Interval in en_US","Date Interval in Ja");
	int i=0;
	UnicodeString formatEn,formatJa;
	FieldPosition pos=0;
    for (int j=0;skeletons[j]!=NULL ;j++) {
 		 u_fprintf(out,"%-10S%-22S%-22S",skeletons[j].getTerminatedBuffer(),data[i].getTerminatedBuffer(),data[i+1].getTerminatedBuffer());
         //create a DateIntervalFormat instance for given skeleton, locale
		 DateIntervalFormat* dtitvfmtEn = DateIntervalFormat::createInstance(skeletons[j], Locale::getEnglish(),status);
         DateIntervalFormat* dtitvfmtJa = DateIntervalFormat::createInstance(skeletons[j], Locale::getJapanese(),status);
		 formatEn.remove();
		 formatJa.remove();
		 //get the DateIntervalFormat
		 dtitvfmtEn->format(dtitvsample[i],formatEn,pos,status);
		 dtitvfmtJa->format(dtitvsample[i],formatJa,pos,status);
         u_fprintf(out,"%-35S%-35S\n", formatEn.getTerminatedBuffer(),formatJa.getTerminatedBuffer());     
		 delete dtitvfmtEn;
		 delete dtitvfmtJa;
         i++;
        }
	u_fclose(out);
	//! [dtitvfmtPreDefined]
}

static void dtitvfmtCustomized() {
	   
	u_printf("===============================================================================\n");
	u_printf("\n");
	u_printf(" dtitvfmtCustomized()\n");
	u_printf("\n");
    u_printf(" Use DateIntervalFormat to create customized date interval format for yMMMd, Hm");
	u_printf("\n");
    u_printf("================================================================================\n");
	//! [dtitvfmtCustomized]
	UFILE *out = u_finit(stdout, NULL, "UTF-8");
	UErrorCode status =U_ZERO_ERROR;
	UnicodeString data[] = {
		UnicodeString("2007-9-10 10:10:10"),
		UnicodeString("2007-10-10 10:10:10"),
		UnicodeString("2007-10-10 22:10:10")
		};
	// to create 2 Date Intervals
    Calendar *cal1 = Calendar::createInstance(status);
	cal1->set(2007,9,10,10,10,10);
	Calendar *cal2 = Calendar::createInstance(status);
	cal2->set(2007,10,10,10,10,10);
	Calendar *cal3 = Calendar::createInstance(status);
	cal3->set(2007,10,10,22,10,10);
	DateInterval* dtitvsample[] = {
			new DateInterval(cal1->getTime(status),cal2->getTime(status)),
            new DateInterval(cal2->getTime(status),cal3->getTime(status))
	      };
	UnicodeString skeletons[] = {
            UnicodeString("yMMMd"),
            UnicodeString("Hm"),
			0,
        };
		u_printf("%-10s%-22s%-22s%-45s%-35s\n", "Skeleton", "from","to", "Date Interval in en_US","Date Interval in Ja");
		// Create an empty DateIntervalInfo object
        DateIntervalInfo dtitvinf =  DateIntervalInfo(status);
		// Set Date Time internal pattern for MONTH, HOUR_OF_DAY
        dtitvinf.setIntervalPattern("yMMMd", UCAL_MONTH, "y 'Diff' MMM d --- MMM d",status);
        dtitvinf.setIntervalPattern("Hm", UCAL_HOUR_OF_DAY, "yyyy MMM d HH:mm ~ HH:mm",status);
		// Set fallback interval pattern
        dtitvinf.setFallbackIntervalPattern("{0} ~~~ {1}",status);
		// Get the DateIntervalFormat with the custom pattern
        UnicodeString formatEn,formatJa;
		FieldPosition pos=0;
		for (int i=0;i<2;i++){
            for (int j=0;skeletons[j]!=NULL;j++) {
			u_fprintf(out,"%-10S%-22S%-22S", skeletons[i].getTerminatedBuffer(),data[j].getTerminatedBuffer(), data[j+1].getTerminatedBuffer());
            DateIntervalFormat* dtitvfmtEn = DateIntervalFormat::createInstance(skeletons[i],Locale::getEnglish(),dtitvinf,status);
            DateIntervalFormat* dtitvfmtJa = DateIntervalFormat::createInstance(skeletons[i],Locale::getJapanese(),dtitvinf,status);
			formatEn.remove();
			formatJa.remove();
			dtitvfmtEn->format(dtitvsample[j],formatEn,pos,status);
			dtitvfmtJa->format(dtitvsample[j],formatJa,pos,status);
			u_fprintf(out,"%-45S%-35S\n", formatEn.getTerminatedBuffer(),formatJa.getTerminatedBuffer());    
            }
       }
	u_fclose(out);
	//! [dtitvfmtCustomized]
}

int main (int argc, char* argv[])
{
	dtitvfmtPreDefined();
	dtitvfmtCustomized();
	return 0;
}
