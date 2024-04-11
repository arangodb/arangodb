// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/***********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 2013-2016, International Business Machines Corporation
 * and others. All Rights Reserved.
 ***********************************************************************/
 
/***********************************************************************
 * This testcase ported from ICU4J ( RegionTest.java ) to ICU4C        *
 * Try to keep them in sync if at all possible...!                     *
 ***********************************************************************/

#include "unicode/utypes.h"
#include "cstring.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/region.h"
#include "regiontst.h"

typedef struct KnownRegion {
  const char *code;
  int32_t numeric;
  const char *parent;
  URegionType type;
  const char *containingContinent;
} KnownRegion;

static KnownRegion knownRegions[] = {
    // Code, Num, Parent, Type,             Containing Continent
    { "TP" , 626, "035", URGN_TERRITORY, "142" },
    { "001", 1,  nullptr ,  URGN_WORLD,        nullptr },
    { "002", 2,  "001",  URGN_CONTINENT,    nullptr },
    { "003", 3,  nullptr,   URGN_GROUPING,     nullptr },
    { "005", 5,  "019",  URGN_SUBCONTINENT, "019" },
    { "009", 9,  "001",  URGN_CONTINENT,    nullptr},
    { "011", 11, "002",  URGN_SUBCONTINENT, "002" },
    { "013", 13, "019",  URGN_SUBCONTINENT, "019" },
    { "014", 14, "002",  URGN_SUBCONTINENT, "002" },
    { "015", 15, "002",  URGN_SUBCONTINENT, "002" },
    { "017", 17, "002",  URGN_SUBCONTINENT, "002" },
    { "018", 18, "002",  URGN_SUBCONTINENT, "002" },
    { "019", 19, "001",  URGN_CONTINENT, nullptr },
    { "021", 21, "019",  URGN_SUBCONTINENT, "019" },
    { "029", 29, "019",  URGN_SUBCONTINENT, "019" },
    { "030", 30, "142",  URGN_SUBCONTINENT, "142" },
    { "034", 34, "142",  URGN_SUBCONTINENT, "142" },
    { "035", 35, "142",  URGN_SUBCONTINENT, "142" },
    { "039", 39, "150",  URGN_SUBCONTINENT, "150"},
    { "053", 53, "009",  URGN_SUBCONTINENT, "009" },
    { "054", 54, "009",  URGN_SUBCONTINENT, "009" },
    { "057", 57, "009",  URGN_SUBCONTINENT, "009" },
    { "061", 61, "009",  URGN_SUBCONTINENT, "009" },
    { "142", 142, "001", URGN_CONTINENT, nullptr },
    { "143", 143, "142", URGN_SUBCONTINENT, "142" },
    { "145", 145, "142", URGN_SUBCONTINENT, "142" },
    { "150", 150, "001", URGN_CONTINENT, nullptr },
    { "151", 151, "150", URGN_SUBCONTINENT, "150" },
    { "154", 154, "150", URGN_SUBCONTINENT, "150" },
    { "155", 155, "150", URGN_SUBCONTINENT, "150" },
    { "419", 419, nullptr,  URGN_GROUPING , nullptr},
    { "AC" ,  -1, "QO" , URGN_TERRITORY, "009" },
    { "AD" ,  20, "039", URGN_TERRITORY, "150" },
    { "AE" , 784, "145", URGN_TERRITORY, "142" },
    { "AF" ,   4, "034", URGN_TERRITORY, "142" },
    { "AG" ,  28, "029", URGN_TERRITORY, "019" },
    { "AI" , 660, "029", URGN_TERRITORY, "019" },
    { "AL" ,   8, "039", URGN_TERRITORY, "150" },
    { "AM" ,  51, "145", URGN_TERRITORY, "142" },
    { "AN" , 530, nullptr,  URGN_DEPRECATED, nullptr },
    { "AO" ,  24, "017", URGN_TERRITORY, "002" },
    { "AQ" ,  10, "QO" , URGN_TERRITORY, "009" },
    { "AR" ,  32, "005", URGN_TERRITORY, "019" },
    { "AS" ,  16, "061", URGN_TERRITORY, "009" },
    { "AT" ,  40, "155", URGN_TERRITORY, "150" },
    { "AU" ,  36, "053", URGN_TERRITORY, "009" },
    { "AW" , 533, "029", URGN_TERRITORY, "019" },
    { "AX" , 248, "154", URGN_TERRITORY, "150" },
    { "AZ" ,  31, "145", URGN_TERRITORY, "142" },
    { "BA" ,  70, "039", URGN_TERRITORY, "150" },
    { "BB" ,  52, "029", URGN_TERRITORY, "019" },
    { "BD" ,  50, "034", URGN_TERRITORY, "142" },
    { "BE" ,  56, "155", URGN_TERRITORY, "150" },
    { "BF" , 854, "011", URGN_TERRITORY, "002" },
    { "BG" , 100, "151", URGN_TERRITORY, "150" },
    { "BH" ,  48, "145", URGN_TERRITORY, "142" },
    { "BI" , 108, "014", URGN_TERRITORY, "002" },
    { "BJ" , 204, "011", URGN_TERRITORY, "002" },
    { "BL" , 652, "029", URGN_TERRITORY, "019" },
    { "BM" ,  60, "021", URGN_TERRITORY, "019" },
    { "BN" ,  96, "035", URGN_TERRITORY, "142" },
    { "BO" ,  68, "005", URGN_TERRITORY, "019" },
    { "BQ" , 535, "029", URGN_TERRITORY, "019" },
    { "BR" ,  76, "005", URGN_TERRITORY, "019" },
    { "BS" ,  44, "029", URGN_TERRITORY, "019" },
    { "BT" ,  64, "034", URGN_TERRITORY, "142" },
    { "BU" , 104, "035", URGN_TERRITORY, "142" },
    { "BV" ,  74, "005", URGN_TERRITORY, "019" },
    { "BW" ,  72, "018", URGN_TERRITORY, "002" },
    { "BY" , 112, "151", URGN_TERRITORY, "150" },
    { "BZ" ,  84, "013", URGN_TERRITORY, "019" },
    { "CA" , 124, "021", URGN_TERRITORY, "019" },
    { "CC" , 166, "053", URGN_TERRITORY, "009" },
    { "CD" , 180, "017", URGN_TERRITORY, "002" },
    { "CF" , 140, "017", URGN_TERRITORY, "002" },
    { "CG" , 178, "017", URGN_TERRITORY, "002" },
    { "CH" , 756, "155", URGN_TERRITORY, "150" },
    { "CI" , 384, "011", URGN_TERRITORY, "002" },
    { "CK" , 184, "061", URGN_TERRITORY, "009" },
    { "CL" , 152, "005", URGN_TERRITORY, "019" },
    { "CM" , 120, "017", URGN_TERRITORY, "002" },
    { "CN" , 156, "030", URGN_TERRITORY, "142" },
    { "CO" , 170, "005", URGN_TERRITORY, "019" },
    { "CP" , -1 , "QO" , URGN_TERRITORY, "009" },
    { "CR" , 188, "013", URGN_TERRITORY, "019" },
    { "CU" , 192, "029", URGN_TERRITORY, "019" },
    { "CV" , 132, "011", URGN_TERRITORY, "002" },
    { "CW" , 531, "029", URGN_TERRITORY, "019" },
    { "CX" , 162, "053", URGN_TERRITORY, "009" },
    { "CY" , 196, "145", URGN_TERRITORY, "142" },
    { "CZ" , 203, "151", URGN_TERRITORY, "150" },
    { "DD" , 276, "155", URGN_TERRITORY, "150" },
    { "DE" , 276, "155", URGN_TERRITORY, "150" },
    { "DG" , -1 , "QO" , URGN_TERRITORY, "009" },
    { "DJ" , 262, "014", URGN_TERRITORY, "002" },
    { "DK" , 208, "154", URGN_TERRITORY, "150" },
    { "DM" , 212, "029", URGN_TERRITORY, "019" },
    { "DO" , 214, "029", URGN_TERRITORY, "019" },
    { "DZ" ,  12, "015", URGN_TERRITORY, "002" },
    { "EA" ,  -1, "015", URGN_TERRITORY, "002" },
    { "EC" , 218, "005", URGN_TERRITORY, "019" },
    { "EE" , 233, "154", URGN_TERRITORY, "150" },
    { "EG" , 818, "015", URGN_TERRITORY, "002" },
    { "EH" , 732, "015", URGN_TERRITORY, "002" },
    { "ER" , 232, "014", URGN_TERRITORY, "002" },
    { "ES" , 724, "039", URGN_TERRITORY, "150" },
    { "ET" , 231, "014", URGN_TERRITORY, "002" },
    { "EU" , 967, nullptr,  URGN_GROUPING, nullptr },
    { "FI" , 246, "154", URGN_TERRITORY, "150" },
    { "FJ" , 242, "054", URGN_TERRITORY, "009" },
    { "FK" , 238, "005", URGN_TERRITORY, "019" },
    { "FM" , 583, "057", URGN_TERRITORY, "009" },
    { "FO" , 234, "154", URGN_TERRITORY, "150" },
    { "FR" , 250, "155", URGN_TERRITORY, "150" },
    { "FX" , 250, "155", URGN_TERRITORY, "150" },
    { "GA" , 266, "017", URGN_TERRITORY, "002" },
    { "GB" , 826, "154", URGN_TERRITORY, "150" },
    { "GD" , 308, "029", URGN_TERRITORY, "019" },
    { "GE" , 268, "145", URGN_TERRITORY, "142" },
    { "GF" , 254, "005", URGN_TERRITORY, "019" },
    { "GG" , 831, "154", URGN_TERRITORY, "150" },
    { "GH" , 288, "011", URGN_TERRITORY, "002" },
    { "GI" , 292, "039", URGN_TERRITORY, "150" },
    { "GL" , 304, "021", URGN_TERRITORY, "019" },
    { "GM" , 270, "011", URGN_TERRITORY, "002" },
    { "GN" , 324, "011", URGN_TERRITORY, "002" },
    { "GP" , 312, "029", URGN_TERRITORY, "019" },
    { "GQ" , 226, "017", URGN_TERRITORY, "002" },
    { "GR" , 300, "039", URGN_TERRITORY, "150" },
    { "GS" , 239, "005", URGN_TERRITORY, "019" },
    { "GT" , 320, "013", URGN_TERRITORY, "019" },
    { "GU" , 316, "057", URGN_TERRITORY, "009" },
    { "GW" , 624, "011", URGN_TERRITORY, "002" },
    { "GY" , 328, "005", URGN_TERRITORY, "019" },
    { "HK" , 344, "030", URGN_TERRITORY, "142" },
    { "HM" , 334, "053", URGN_TERRITORY, "009" },
    { "HN" , 340, "013", URGN_TERRITORY, "019" },
    { "HR" , 191, "039", URGN_TERRITORY, "150" },
    { "HT" , 332, "029", URGN_TERRITORY, "019" },
    { "HU" , 348, "151", URGN_TERRITORY, "150" },
    { "IC" ,  -1, "015", URGN_TERRITORY, "002" },
    { "ID" , 360, "035", URGN_TERRITORY, "142" },
    { "IE" , 372, "154", URGN_TERRITORY, "150" },
    { "IL" , 376, "145", URGN_TERRITORY, "142" },
    { "IM" , 833, "154", URGN_TERRITORY, "150" },
    { "IN" , 356, "034", URGN_TERRITORY, "142" },
    { "IO" ,  86, "014", URGN_TERRITORY, "002" },
    { "IQ" , 368, "145", URGN_TERRITORY, "142" },
    { "IR" , 364, "034", URGN_TERRITORY, "142" },
    { "IS" , 352, "154", URGN_TERRITORY, "150" },
    { "IT" , 380, "039", URGN_TERRITORY, "150" },
    { "JE" , 832, "154", URGN_TERRITORY, "150" },
    { "JM" , 388, "029", URGN_TERRITORY, "019" },
    { "JO" , 400, "145", URGN_TERRITORY, "142" },
    { "JP" , 392, "030", URGN_TERRITORY, "142" },
    { "KE" , 404, "014", URGN_TERRITORY, "002" },
    { "KG" , 417, "143", URGN_TERRITORY, "142" },
    { "KH" , 116, "035", URGN_TERRITORY, "142" },
    { "KI" , 296, "057", URGN_TERRITORY, "009" },
    { "KM" , 174, "014", URGN_TERRITORY, "002" },
    { "KN" , 659, "029", URGN_TERRITORY, "019" },
    { "KP" , 408, "030", URGN_TERRITORY, "142" },
    { "KR" , 410, "030", URGN_TERRITORY, "142" },
    { "KW" , 414, "145", URGN_TERRITORY, "142" },
    { "KY" , 136, "029", URGN_TERRITORY, "019" },
    { "KZ" , 398, "143", URGN_TERRITORY, "142" },
    { "LA" , 418, "035", URGN_TERRITORY, "142" },
    { "LB" , 422, "145", URGN_TERRITORY, "142" },
    { "LC" , 662, "029", URGN_TERRITORY, "019" },
    { "LI" , 438, "155", URGN_TERRITORY, "150" },
    { "LK" , 144, "034", URGN_TERRITORY, "142" },
    { "LR" , 430, "011", URGN_TERRITORY, "002" },
    { "LS" , 426, "018", URGN_TERRITORY, "002" },
    { "LT" , 440, "154", URGN_TERRITORY, "150" },
    { "LU" , 442, "155", URGN_TERRITORY, "150" },
    { "LV" , 428, "154", URGN_TERRITORY, "150" },
    { "LY" , 434, "015", URGN_TERRITORY, "002" },
    { "MA" , 504, "015", URGN_TERRITORY, "002" },
    { "MC" , 492, "155", URGN_TERRITORY, "150" },
    { "MD" , 498, "151", URGN_TERRITORY, "150" },
    { "ME" , 499, "039", URGN_TERRITORY, "150" },
    { "MF" , 663, "029", URGN_TERRITORY, "019" },
    { "MG" , 450, "014", URGN_TERRITORY, "002" },
    { "MH" , 584, "057", URGN_TERRITORY, "009" },
    { "MK" , 807, "039", URGN_TERRITORY, "150" },
    { "ML" , 466, "011", URGN_TERRITORY, "002" },
    { "MM" , 104, "035", URGN_TERRITORY, "142" },
    { "MN" , 496, "030", URGN_TERRITORY, "142" },
    { "MO" , 446, "030", URGN_TERRITORY, "142" },
    { "MP" , 580, "057", URGN_TERRITORY, "009" },
    { "MQ" , 474, "029", URGN_TERRITORY, "019" },
    { "MR" , 478, "011", URGN_TERRITORY, "002" },
    { "MS" , 500, "029", URGN_TERRITORY, "019" },
    { "MT" , 470, "039", URGN_TERRITORY, "150" },
    { "MU" , 480, "014", URGN_TERRITORY, "002" },
    { "MV" , 462, "034", URGN_TERRITORY, "142" },
    { "MW" , 454, "014", URGN_TERRITORY, "002" },
    { "MX" , 484, "013", URGN_TERRITORY, "019"},
    { "MY" , 458, "035", URGN_TERRITORY, "142" },
    { "MZ" , 508, "014", URGN_TERRITORY, "002" },
    { "NA" , 516, "018", URGN_TERRITORY, "002" },
    { "NC" , 540, "054", URGN_TERRITORY, "009" },
    { "NE" , 562, "011", URGN_TERRITORY, "002" },
    { "NF" , 574, "053", URGN_TERRITORY, "009" },
    { "NG" , 566, "011", URGN_TERRITORY, "002" },
    { "NI" , 558, "013", URGN_TERRITORY, "019" },
    { "NL" , 528, "155", URGN_TERRITORY, "150" },
    { "NO" , 578, "154", URGN_TERRITORY, "150" },
    { "NP" , 524, "034", URGN_TERRITORY, "142" },
    { "NR" , 520, "057", URGN_TERRITORY, "009" },
    { "NT" , 536, nullptr , URGN_DEPRECATED, nullptr },
    { "NU" , 570, "061", URGN_TERRITORY, "009" },
    { "NZ" , 554, "053", URGN_TERRITORY, "009" },
    { "OM" , 512, "145", URGN_TERRITORY, "142" },
    { "PA" , 591, "013", URGN_TERRITORY, "019" },
    { "PE" , 604, "005", URGN_TERRITORY, "019" },
    { "PF" , 258, "061", URGN_TERRITORY, "009" },
    { "PG" , 598, "054", URGN_TERRITORY, "009" },
    { "PH" , 608, "035", URGN_TERRITORY, "142" },
    { "PK" , 586, "034", URGN_TERRITORY, "142" },
    { "PL" , 616, "151", URGN_TERRITORY, "150" },
    { "PM" , 666, "021", URGN_TERRITORY, "019" },
    { "PN" , 612, "061", URGN_TERRITORY, "009" },
    { "PR" , 630, "029", URGN_TERRITORY, "019" },
    { "PS" , 275, "145", URGN_TERRITORY, "142" },
    { "PT" , 620, "039", URGN_TERRITORY, "150" },
    { "PW" , 585, "057", URGN_TERRITORY, "009" },
    { "PY" , 600, "005", URGN_TERRITORY, "019" },
    { "QA" , 634, "145", URGN_TERRITORY, "142" },
    { "QO" , 961, "009", URGN_SUBCONTINENT, "009" },
    { "QU" , 967, nullptr,  URGN_GROUPING, nullptr },
    { "RE" , 638, "014", URGN_TERRITORY, "002" },
    { "RO" , 642, "151", URGN_TERRITORY, "150" },
    { "RS" , 688, "039", URGN_TERRITORY, "150" },
    { "RU" , 643, "151", URGN_TERRITORY, "150" },
    { "RW" , 646, "014", URGN_TERRITORY, "002" },
    { "SA" , 682, "145", URGN_TERRITORY, "142" },
    { "SB" ,  90, "054", URGN_TERRITORY, "009" },
    { "SC" , 690, "014", URGN_TERRITORY, "002" },
    { "SD" , 729, "015", URGN_TERRITORY, "002" },
    { "SE" , 752, "154", URGN_TERRITORY, "150" },
    { "SG" , 702, "035", URGN_TERRITORY, "142" },
    { "SH" , 654, "011", URGN_TERRITORY, "002" },
    { "SI" , 705, "039", URGN_TERRITORY, "150" },
    { "SJ" , 744, "154", URGN_TERRITORY, "150" },
    { "SK" , 703, "151", URGN_TERRITORY, "150" },
    { "SL" , 694, "011", URGN_TERRITORY, "002" },
    { "SM" , 674, "039", URGN_TERRITORY, "150" },
    { "SN" , 686, "011", URGN_TERRITORY, "002" },
    { "SO" , 706, "014", URGN_TERRITORY, "002" },
    { "SR" , 740, "005", URGN_TERRITORY, "019" },
    { "SS" , 728, "014", URGN_TERRITORY, "002" },
    { "ST" , 678, "017", URGN_TERRITORY, "002" },
    { "SU" , 810, nullptr , URGN_DEPRECATED , nullptr},
    { "SV" , 222, "013", URGN_TERRITORY, "019" },
    { "SX" , 534, "029", URGN_TERRITORY, "019" },
    { "SY" , 760, "145", URGN_TERRITORY, "142" },
    { "SZ" , 748, "018", URGN_TERRITORY, "002" },
    { "TA" ,  -1, "QO",  URGN_TERRITORY, "009" },
    { "TC" , 796, "029", URGN_TERRITORY, "019" },
    { "TD" , 148, "017", URGN_TERRITORY, "002" },
    { "TF" , 260, "014", URGN_TERRITORY, "002" },
    { "TG" , 768, "011", URGN_TERRITORY, "002" },
    { "TH" , 764, "035", URGN_TERRITORY, "142" },
    { "TJ" , 762, "143", URGN_TERRITORY, "142" },
    { "TK" , 772, "061", URGN_TERRITORY, "009" },
    { "TL" , 626, "035", URGN_TERRITORY, "142" },
    { "TM" , 795, "143", URGN_TERRITORY, "142" },
    { "TN" , 788, "015", URGN_TERRITORY, "002" },
    { "TO" , 776, "061", URGN_TERRITORY, "009" },
    { "TP" , 626, "035", URGN_TERRITORY, "142" },
    { "TR" , 792, "145", URGN_TERRITORY, "142" },
    { "TT" , 780, "029", URGN_TERRITORY, "019" },
    { "TV" , 798, "061", URGN_TERRITORY, "009" },
    { "TW" , 158, "030", URGN_TERRITORY, "142" },
    { "TZ" , 834, "014", URGN_TERRITORY, "002" },
    { "UA" , 804, "151", URGN_TERRITORY, "150" },
    { "UG" , 800, "014", URGN_TERRITORY, "002" },
    { "UM" , 581, "057", URGN_TERRITORY, "009" },
    { "US" , 840, "021", URGN_TERRITORY, "019" },
    { "UY" , 858, "005", URGN_TERRITORY, "019" },
    { "UZ" , 860, "143", URGN_TERRITORY, "142" },
    { "VA" , 336, "039", URGN_TERRITORY, "150" },
    { "VC" , 670, "029", URGN_TERRITORY, "019" },
    { "VE" , 862, "005", URGN_TERRITORY, "019" },
    { "VG" ,  92, "029", URGN_TERRITORY, "019" },
    { "VI" , 850, "029", URGN_TERRITORY, "019" },
    { "VN" , 704, "035", URGN_TERRITORY, "142" },
    { "VU" , 548, "054", URGN_TERRITORY, "009" },
    { "WF" , 876, "061", URGN_TERRITORY, "009" },
    { "WS" , 882, "061", URGN_TERRITORY, "009" },
    { "YD" , 887, "145", URGN_TERRITORY, "142" },
    { "YE" , 887, "145", URGN_TERRITORY, "142" },
    { "YT" , 175, "014", URGN_TERRITORY, "002" },
    { "ZA" , 710, "018", URGN_TERRITORY, "002" },
    { "ZM" , 894, "014", URGN_TERRITORY, "002" },
    { "ZR" , 180, "017", URGN_TERRITORY, "002" },
    { "ZW" , 716, "014", URGN_TERRITORY, "002" },
    { "ZZ" , 999, nullptr , URGN_UNKNOWN, nullptr }
    };

// *****************************************************************************
// class RegionTest
// *****************************************************************************


RegionTest::RegionTest() {
}

RegionTest::~RegionTest() {
}

void 
RegionTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* par )
{
   optionv = (par && *par=='v');

   TESTCASE_AUTO_BEGIN;
   TESTCASE_AUTO(TestKnownRegions);
   TESTCASE_AUTO(TestGetInstanceString);
   TESTCASE_AUTO(TestGetInstanceInt);
   TESTCASE_AUTO(TestGetContainedRegions);
   TESTCASE_AUTO(TestGetContainedRegionsWithType);
   TESTCASE_AUTO(TestGetContainingRegion);
   TESTCASE_AUTO(TestGetContainingRegionWithType);
   TESTCASE_AUTO(TestGetPreferredValues);
   TESTCASE_AUTO(TestContains);
   TESTCASE_AUTO(TestAvailableTerritories);
   TESTCASE_AUTO(TestNoContainedRegions);
   TESTCASE_AUTO(TestGroupingChildren);
   TESTCASE_AUTO_END;
}


void RegionTest::TestKnownRegions() {

    for (int32_t i = 0 ; i < UPRV_LENGTHOF(knownRegions) ; i++ ) {
        KnownRegion rd = knownRegions[i];
        UErrorCode status = U_ZERO_ERROR;
        const Region *r = Region::getInstance(rd.code,status);
        if ( r ) {
            int32_t n = r->getNumericCode();
            int32_t e = rd.numeric;
            if ( n != e ) {
                errln("Numeric code mismatch for region %s.  Expected:%d Got:%d",r->getRegionCode(),e,n);
            }

            if (r->getType() != rd.type) {
                errln("Expected region %s to be of type %d. Got: %d",r->getRegionCode(),rd.type,r->getType());
            }

            int32_t nc = rd.numeric;
            if ( nc > 0 ) {
                const Region *ncRegion = Region::getInstance(nc,status);
                if ( *ncRegion != *r && nc != 891 ) { // 891 is special case - CS and YU both deprecated codes for region 891
                    errln("Creating region %s by its numeric code returned a different region. Got: %s instead.",r->getRegionCode(),ncRegion->getRegionCode());
                }
             }
        } else {
            dataerrln("Known region %s was not recognized.",rd.code);
        }
    }
}

void RegionTest::TestGetInstanceString() {
    typedef struct TestData {
        const char *inputID;
        const char *expectedID;
        URegionType expectedType;
    } TestData;

    static TestData testData[] = {
    //  Input ID, Expected ID, Expected Type
        { "DE", "DE", URGN_TERRITORY },  // Normal region
        { "QU", "EU", URGN_GROUPING },   // Alias to a grouping
        { "DD", "DE", URGN_TERRITORY },  // Alias to a deprecated region (East Germany) with single preferred value
        { "276", "DE", URGN_TERRITORY }, // Numeric code for Germany
        { "278", "DE", URGN_TERRITORY }, // Numeric code for East Germany (Deprecated)
        { "SU", "SU", URGN_DEPRECATED }, // Alias to a deprecated region with multiple preferred values
        { "AN", "AN", URGN_DEPRECATED }, // Deprecated region with multiple preferred values
        { "SVK", "SK", URGN_TERRITORY }  // 3-letter code - Slovakia
    };


    UErrorCode status = U_ZERO_ERROR;
    const Region *r = Region::getInstance((const char *)nullptr,status);
    if ( status != U_ILLEGAL_ARGUMENT_ERROR ) {
        errcheckln(status, "Calling Region::getInstance(nullptr) should have triggered an U_ILLEGAL_ARGUMENT_ERROR, but didn't. - %s", u_errorName(status));
    }

    status = U_ZERO_ERROR;
    r = Region::getInstance("BOGUS",status);
    if ( status != U_ILLEGAL_ARGUMENT_ERROR ) {
        errcheckln(status, "Calling Region::getInstance(\"BOGUS\") should have triggered an U_ILLEGAL_ARGUMENT_ERROR, but didn't. - %s", u_errorName(status));
    }


    for (int32_t i = 0 ; i < UPRV_LENGTHOF(testData) ; i++ ) {
        TestData data = testData[i];
        status = U_ZERO_ERROR;
        r = Region::getInstance(data.inputID,status);
        const char *id;
        URegionType type;
        if ( r ) {
            id = r->getRegionCode();
            type = r->getType();
        } else {
            id = "nullptr";
            type = URGN_UNKNOWN;
        }
        if ( uprv_strcmp(id,data.expectedID)) {
            dataerrln("Unexpected region ID for Region::getInstance(\"%s\"); Expected: %s Got: %s",data.inputID,data.expectedID,id);
        }
        if ( type != data.expectedType) {
            dataerrln("Unexpected region type for Region::getInstance(\"%s\"); Expected: %d Got: %d",data.inputID,data.expectedType,type);
        }
    }
}

void RegionTest::TestGetInstanceInt() {
    typedef struct TestData {
        int32_t inputID;
        const char *expectedID;
        URegionType expectedType;
    } TestData;

    static TestData testData[] = {
        //  Input ID, Expected ID, Expected Type
        { 276, "DE",  URGN_TERRITORY }, // Numeric code for Germany
        { 278, "DE",  URGN_TERRITORY }, // Numeric code for East Germany (Deprecated)
        { 419, "419", URGN_GROUPING },  // Latin America
        { 736, "SD",  URGN_TERRITORY }, // Sudan (pre-2011) - changed numeric code after South Sudan split off
        { 729, "SD",  URGN_TERRITORY }, // Sudan (post-2011) - changed numeric code after South Sudan split off
    };

    UErrorCode status = U_ZERO_ERROR;
    Region::getInstance(-123,status);
    if ( status != U_ILLEGAL_ARGUMENT_ERROR ) {
        errcheckln(status, "Calling Region::getInstance(-123) should have triggered an U_ILLEGAL_ARGUMENT_ERROR, but didn't. - %s", u_errorName(status));
    }

    for (int32_t i = 0 ; i < UPRV_LENGTHOF(testData) ; i++ ) {
        TestData data = testData[i];
        status = U_ZERO_ERROR;
        const Region *r = Region::getInstance(data.inputID,status);
        const char *id;
        URegionType type;
        if ( r ) {
            id = r->getRegionCode();
            type = r->getType();
        } else {
            id = "nullptr";
            type = URGN_UNKNOWN;
        }
        if ( uprv_strcmp(data.expectedID,id)) {
            dataerrln("Unexpected region ID for Region.getInstance(%d)); Expected: %s Got: %s",data.inputID,data.expectedID,id);
        }
        if ( data.expectedType != type) {
            dataerrln("Unexpected region type for Region.getInstance(%d)); Expected: %d Got: %d",data.inputID,data.expectedType,type);
        }
    }
}

void RegionTest::TestGetContainedRegions() {
    for (int32_t i = 0 ; i < UPRV_LENGTHOF(knownRegions) ; i++ ) {
        KnownRegion rd = knownRegions[i];
        UErrorCode status = U_ZERO_ERROR;

        const Region *r = Region::getInstance(rd.code,status);
        if (r) {
            if (r->getType() == URGN_GROUPING) {
                continue;
            }
            StringEnumeration *containedRegions = r->getContainedRegions(status);
            if (U_FAILURE(status)) {
              errln("%s->getContainedRegions(status) failed: %s", r->getRegionCode(), u_errorName(status));
              continue;
            }
            for ( int32_t i = 0 ; i < containedRegions->count(status); i++ ) {
                const char *crID = containedRegions->next(nullptr,status);
                const Region *cr = Region::getInstance(crID,status);
                const Region *containingRegion = cr ? cr->getContainingRegion() : nullptr;
                if ( !containingRegion || *containingRegion != *r ) {
                    errln("Region: %s contains region %s. Expected containing region of this region to be the original region, but got %s",
                        r->getRegionCode(),cr->getRegionCode(),containingRegion?containingRegion->getRegionCode():"nullptr"); 
                }
            }
            delete containedRegions;
        } else {
            dataerrln("Known region %s was not recognized.",rd.code);
        }
    }
}

void RegionTest::TestGetContainedRegionsWithType() {
    for (int32_t i = 0 ; i < UPRV_LENGTHOF(knownRegions) ; i++ ) {
        KnownRegion rd = knownRegions[i];
        UErrorCode status = U_ZERO_ERROR;

        const Region *r = Region::getInstance(rd.code,status);
        if (r) {
            if (r->getType() != URGN_CONTINENT) {
                continue;
            }
            StringEnumeration *containedRegions = r->getContainedRegions(URGN_TERRITORY, status);
            if (U_FAILURE(status)) {
              errln("%s->getContainedRegions(URGN_TERRITORY, status) failed: %s", r->getRegionCode(), u_errorName(status));
              continue;
            }
            for ( int32_t j = 0 ; j < containedRegions->count(status); j++ ) {
                const char *crID = containedRegions->next(nullptr,status);
                const Region *cr = Region::getInstance(crID,status);
                const Region *containingRegion = cr ? cr->getContainingRegion(URGN_CONTINENT) : nullptr;
                if ( !containingRegion || *containingRegion != *r ) {
                    errln("Continent: %s contains territory %s. Expected containing continent of this region to be the original region, but got %s",
                        r->getRegionCode(),cr->getRegionCode(),containingRegion?containingRegion->getRegionCode():"nullptr"); 
                }
            }
            delete containedRegions;
        } else {
            dataerrln("Known region %s was not recognized.",rd.code);
        }
    }
}

void RegionTest::TestGetContainingRegion() {        
    for (int32_t i = 0 ; i < UPRV_LENGTHOF(knownRegions) ; i++ ) {
        KnownRegion rd = knownRegions[i];
        UErrorCode status = U_ZERO_ERROR;
        const Region *r = Region::getInstance(rd.code,status);
        if (r) {
            const Region *c = r->getContainingRegion();
            if (rd.parent == nullptr) {                   
                if ( c ) {
                    errln("Containing region for %s should have been nullptr.  Got: %s",r->getRegionCode(),c->getRegionCode());
                }
            } else {
                const Region *p = Region::getInstance(rd.parent,status);                   
                if ( !c || *p != *c ) {
                    errln("Expected containing continent of region %s to be %s. Got: %s",
                        r->getRegionCode(),p?p->getRegionCode():"nullptr",c?c->getRegionCode():"nullptr" );
                }
            }
        } else {
            dataerrln("Known region %s was not recognized.",rd.code);
        }
    }
}

void RegionTest::TestGetContainingRegionWithType() {        
    for (int32_t i = 0 ; i < UPRV_LENGTHOF(knownRegions) ; i++ ) {
        KnownRegion rd = knownRegions[i];
        UErrorCode status = U_ZERO_ERROR;

        const Region *r = Region::getInstance(rd.code,status);
        if (r) {
            const Region *c = r->getContainingRegion(URGN_CONTINENT);
            if (rd.containingContinent == nullptr) {
                 if ( c != nullptr) {
                     errln("Containing continent for %s should have been nullptr.  Got: %s",r->getRegionCode(), c->getRegionCode());
                 }
            } else {
                const Region *p = Region::getInstance(rd.containingContinent,status);                   
                if ( *p != *c ) {
                    errln("Expected containing continent of region %s to be %s. Got: %s",
                        r->getRegionCode(),p?p->getRegionCode():"nullptr",c?c->getRegionCode():"nullptr" );
                }
            }
        } else {
            dataerrln("Known region %s was not recognized.",rd.code);
        }
    }
}

void RegionTest::TestGetPreferredValues() {
    static const char *testData[6][17] = {
        //  Input ID, Expected Preferred Values...
        { "AN", "CW", "SX", "BQ", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr }, // Netherlands Antilles
        { "CS", "RS", "ME", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },     // Serbia & Montenegro
        { "FQ", "AQ", "TF", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },     // French Southern and Antarctic Territories
        { "NT", "IQ", "SA", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr },     // Neutral Zone
        { "PC", "FM", "MH", "MP", "PW", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, }, // Pacific Islands Trust Territory
        { "SU", "RU", "AM", "AZ", "BY", "EE", "GE", "KZ", "KG", "LV", "LT", "MD", "TJ", "TM", "UA", "UZ" , nullptr}, // Soviet Union
    };

    for ( int32_t i = 0 ; i < 6 ; i++ ) {
        const char **data = testData[i];
        UErrorCode status = U_ZERO_ERROR;
        const Region *r = Region::getInstance(data[0],status);
        if (r) {
            StringEnumeration *preferredValues = r->getPreferredValues(status);
            if (U_FAILURE(status)) {
              errln("%s->getPreferredValues(status) failed: %s", r->getRegionCode(), u_errorName(status));
              continue;
            }
            for ( int i = 1 ; data[i] ; i++ ) {
                UBool found = false;
                preferredValues->reset(status);
                while ( const char *check = preferredValues->next(nullptr,status) ) {
                    if ( !uprv_strcmp(check,data[i]) ) {
                        found = true;
                        break;
                    }
                }
                if ( !found ) {
                    errln("Region::getPreferredValues() for region \"%s\" should have contained \"%s\" but it didn't.",r->getRegionCode(),data[i]);
                }
            }
            delete preferredValues;
        } else {
            dataerrln("Known region %s was not recognized.",data[0]);
        }
    }
}

void RegionTest::TestContains() {        
    for (int32_t i = 0 ; i < UPRV_LENGTHOF(knownRegions) ; i++ ) {
        KnownRegion rd = knownRegions[i];
        UErrorCode status = U_ZERO_ERROR;

        const Region *r = Region::getInstance(rd.code,status);
        if (r) {
            const Region *c = r->getContainingRegion();
            while ( c ) {
                if ( !c->contains(*r)) {
                    errln("Region \"%s\" should have contained \"%s\" but it didn't.",c->getRegionCode(),r->getRegionCode());
                }
                c = c->getContainingRegion();
            }
        } else {
            dataerrln("Known region %s was not recognized.",rd.code);
        }
    }
}

void RegionTest::TestAvailableTerritories() {
    // Test to make sure that the set of territories contained in World and the set of all available
    // territories are one and the same.
    UErrorCode status = U_ZERO_ERROR;
    StringEnumeration *availableTerritories = Region::getAvailable(URGN_TERRITORY, status);
    if (U_FAILURE(status)) {
        dataerrln("Region::getAvailable(URGN_TERRITORY,status) failed: %s", u_errorName(status));
        return;
    }
    const Region *world = Region::getInstance("001",status);
    if (U_FAILURE(status)) {
        dataerrln("Region::getInstance(\"001\",status) failed: %s", u_errorName(status));
        return;
    }
    StringEnumeration *containedInWorld = world->getContainedRegions(URGN_TERRITORY, status);
    if (U_FAILURE(status)) {
        errln("world->getContainedRegions(URGN_TERRITORY, status) failed: %s", u_errorName(status));
        return;
    }
    if ( !availableTerritories || !containedInWorld || *availableTerritories != *containedInWorld ) {
        char availableTerritoriesString[1024] = "";
        char containedInWorldString[1024] = "";
        if ( availableTerritories ) {
            for (int32_t i = 0 ; i < availableTerritories->count(status) ; i++ ) {
                if ( i > 0 ) {
                    uprv_strcat(availableTerritoriesString," ");
                }
                uprv_strcat(availableTerritoriesString,availableTerritories->next(nullptr,status));
            }
        } else {
            uprv_strcpy(availableTerritoriesString,"nullptr");
        }
        if ( containedInWorld ) {
            for (int32_t i = 0 ; i < containedInWorld->count(status) ; i++ ) {
                if ( i > 0 ) {
                    uprv_strcat(containedInWorldString," ");
                }
                uprv_strcat(containedInWorldString,containedInWorld->next(nullptr,status));
            }
        } else {
            uprv_strcpy(containedInWorldString,"nullptr");
        }
        errln("Available territories and all territories contained in world should be the same set.\nAvailable          = %s\nContained in World = %s",
            availableTerritoriesString,containedInWorldString);
    }
    delete availableTerritories;
    delete containedInWorld;
}

void RegionTest::TestNoContainedRegions() {
  UErrorCode status = U_ZERO_ERROR;
  const Region *region = Region::getInstance("BM",status);
  if (U_FAILURE(status) || region == nullptr) {
      dataerrln("Fail called to Region::getInstance(\"BM\", status) - %s", u_errorName(status));
      return;
  }
  StringEnumeration *containedRegions = region->getContainedRegions(status);
  if (U_FAILURE(status)) {
      errln("%s->getContainedRegions(status) failed: %s", region->getRegionCode(), u_errorName(status));
      return;
  }
  const char *emptyStr = containedRegions->next(nullptr, status);
  if (U_FAILURE(status)||(emptyStr!=nullptr)) {
    errln("Error, 'BM' should have no subregions, but returned str=%p, err=%s\n", emptyStr, u_errorName(status));
  } else {
    logln("Success - BM has no subregions\n");
  }
  delete containedRegions;
}

void RegionTest::TestGroupingChildren() {
    const char* testGroupings[] = {
        "003", "021,013,029",
        "419", "013,029,005",
        "EU",  "AT,BE,CY,CZ,DE,DK,EE,ES,FI,FR,GR,HR,HU,IE,IT,LT,LU,LV,MT,NL,PL,PT,SE,SI,SK,BG,RO"
    };

    for (int32_t i = 0; i < UPRV_LENGTHOF(testGroupings); i += 2) {
        const char* groupingCode = testGroupings[i];
        const char* expectedChildren = testGroupings[i + 1];
        
        UErrorCode err = U_ZERO_ERROR;
        const Region* grouping = Region::getInstance(groupingCode, err);
        if (U_SUCCESS(err)) {
            StringEnumeration* actualChildren = grouping->getContainedRegions(err);
            if (U_SUCCESS(err)) {
                int32_t numActualChildren = actualChildren->count(err);
                int32_t numExpectedChildren = 0;
                const char* expectedChildStart = expectedChildren;
                const char* expectedChildEnd = nullptr;
                const char* actualChild = nullptr;
                while ((actualChild = actualChildren->next(nullptr, err)) != nullptr && *expectedChildStart != '\0') {
                    expectedChildEnd = uprv_strchr(expectedChildStart, ',');
                    if (expectedChildEnd == nullptr) {
                        expectedChildEnd = expectedChildStart + uprv_strlen(expectedChildStart);
                    }
                    if (uprv_strlen(actualChild) != size_t(expectedChildEnd - expectedChildStart) || uprv_strncmp(actualChild, expectedChildStart, expectedChildEnd - expectedChildStart) != 0) {
                        errln("Mismatch in child list for %s at position %d: expected %s, got %s\n", groupingCode, numExpectedChildren, expectedChildStart, actualChild);
                    }
                    expectedChildStart = (*expectedChildEnd != '\0') ? expectedChildEnd + 1 : expectedChildEnd;
                    ++numExpectedChildren;
                }
                while (expectedChildEnd != nullptr && *expectedChildEnd != '\0') {
                    expectedChildEnd = uprv_strchr(expectedChildEnd + 1, ',');
                    ++numExpectedChildren;
                }
                if (numExpectedChildren != numActualChildren) {
                    errln("Wrong number of children for %s: expected %d, got %d\n", groupingCode, numExpectedChildren, numActualChildren);
                }
                delete actualChildren;
            } else {
                errln("Couldn't create iterator for children of %s\n", groupingCode);
            }
        } else {
            errln("Region %s not found\n", groupingCode);
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
