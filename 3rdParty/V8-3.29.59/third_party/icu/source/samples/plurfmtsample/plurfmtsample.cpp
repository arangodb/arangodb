/********************************************************************************
* Copyright (C) 2008-2013, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*/

//! [PluralFormatExample1]
#include <iostream>
#include "unicode/plurfmt.h"
#include "unicode/msgfmt.h"
#include "unicode/ustdio.h"
//! [PluralFormatExample1] 

using namespace std;
static void PluralFormatExample() {
	  
	u_printf("=============================================================================\n");
	u_printf(" PluralFormatExample()\n");
    u_printf("\n");
    u_printf(" Use PluralFormat and Messageformat to get Plural Form for languages below:\n");
    u_printf(" English, Slovenian\n");
    u_printf("=============================================================================\n");
	
	//! [PluralFormatExample] 
	UErrorCode status =U_ZERO_ERROR; 
	Locale locEn = Locale("en");
    Locale locSl = Locale("sl");

    UnicodeString patEn = UnicodeString("one{dog} other{dogs}");                      // English 'dog'
    UnicodeString patSl = UnicodeString("one{pes} two{psa} few{psi} other{psov}");    // Slovenian translation of dog in Plural Form

    // Create a new PluralFormat for a given locale locale and pattern string
    PluralFormat plfmtEn = PluralFormat(locEn, patEn,status);
    PluralFormat plfmtSl = PluralFormat(locSl, patSl,status);
    // Constructs a MessageFormat for given pattern and locale.
    MessageFormat* msgfmtEn =  new MessageFormat("{0,number} {1}", locEn,status);
    MessageFormat* msgfmtSl =  new MessageFormat("{0,number} {1}", locSl,status);

	int numbers[] = {0, 1, 2, 3, 4, 5, 10, 100, 101, 102};
	u_printf("Output by using PluralFormat and MessageFormat API\n");
    u_printf("%-16s%-16s%-16s\n","Number", "English","Slovenian");
 
    // Use MessageFormat.format () to format the objects and append to the given StringBuffer
    for (int i=0;i<sizeof(numbers)/sizeof(int);i++) {
	      UnicodeString msgEn,msgSl;
		  FieldPosition fpos = 0;
		  Formattable argEn[]={Formattable(numbers[i]), Formattable(plfmtEn.format(numbers[i],status))};
		  Formattable argSl[]={Formattable(numbers[i]), Formattable(plfmtSl.format(numbers[i],status))};
		  msgfmtEn->format(argEn,2,msgEn,fpos,status);
		  msgfmtSl->format(argSl,2,msgSl,fpos,status);
  		  u_printf("%-16d%-16S%-16S\n", numbers[i], msgEn.getTerminatedBuffer(),msgSl.getTerminatedBuffer());
      }

     u_printf("\n");

      // Equivalent code with message format pattern
      UnicodeString msgPatEn = "{0,plural, one{# dog} other{# dogs}}";
      UnicodeString msgPatSl = "{0,plural, one{# pes} two{# psa} few{# psi} other{# psov}}";
 
	  MessageFormat* altMsgfmtEn = new MessageFormat(msgPatEn, locEn,status);
      MessageFormat* altMsgfmtSl = new MessageFormat(msgPatSl, locSl,status);
      u_printf("Same Output by using MessageFormat API only\n");
      u_printf("%-16s%-16s%-16s\n","Number", "English","Slovenian");
      for (int i=0;i<sizeof(numbers)/sizeof(int);i++) {
          UnicodeString msgEn,msgSl;
		  Formattable arg[] = {numbers[i]};
		  FieldPosition fPos =0;
		  altMsgfmtEn->format(arg, 1, msgEn, fPos, status);
          altMsgfmtSl->format(arg, 1, msgSl, fPos,status);
          u_printf("%-16d%-16S%-16S\n", numbers[i], msgEn.getTerminatedBuffer(), msgSl.getTerminatedBuffer());
      }

 	delete msgfmtEn;
	delete msgfmtSl;
	delete altMsgfmtEn;
	delete altMsgfmtSl;
	//! [PluralFormatExample]

	  /*  output of the sample code:
       ********************************************************************
        Number			English			Slovenian
        0				0 dogs			0 psov
        1				1 dog			1 pes
        2				2 dogs			2 psa
        3				3 dogs			3 psi
        4				4 dogs			4 psi
        5				5 dogs			5 psov
        10				10 dogs			10 psov
        100				100 dogs		100 psov
        101				101 dogs		101 pes
        102				102 dogs		102 psa

      *********************************************************************/
}
int main (int argc, char* argv[])
{
	PluralFormatExample();
	return 0;
}