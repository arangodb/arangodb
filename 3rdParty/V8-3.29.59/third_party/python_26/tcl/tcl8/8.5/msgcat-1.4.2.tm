# msgcat.tcl --
#
#	This file defines various procedures which implement a
#	message catalog facility for Tcl programs.  It should be
#	loaded with the command "package require msgcat".
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
# Copyright (c) 1998 by Mark Harrison.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
# 
# RCS: @(#) $Id: msgcat.tcl,v 1.26 2006/11/03 00:34:52 hobbs Exp $

package require Tcl 8.5
# When the version number changes, be sure to update the pkgIndex.tcl file,
# and the installation directory in the Makefiles.
package provide msgcat 1.4.2

namespace eval msgcat {
    namespace export mc mcload mclocale mcmax mcmset mcpreferences mcset \
	    mcunknown

    # Records the current locale as passed to mclocale
    variable Locale ""

    # Records the list of locales to search
    variable Loclist {}

    # Records the mapping between source strings and translated strings.  The
    # dict key is of the form "<locale> <namespace> <src>", where locale and
    # namespace should be themselves dict values and the value is
    # the translated string.
    variable Msgs [dict create]

    # Map of language codes used in Windows registry to those of ISO-639
    if { $::tcl_platform(platform) eq "windows" } {
	variable WinRegToISO639 [dict create  {*}{
	    01 ar 0401 ar_SA 0801 ar_IQ 0c01 ar_EG 1001 ar_LY 1401 ar_DZ
		  1801 ar_MA 1c01 ar_TN 2001 ar_OM 2401 ar_YE 2801 ar_SY
		  2c01 ar_JO 3001 ar_LB 3401 ar_KW 3801 ar_AE 3c01 ar_BH
		  4001 ar_QA
	    02 bg 0402 bg_BG
	    03 ca 0403 ca_ES
	    04 zh 0404 zh_TW 0804 zh_CN 0c04 zh_HK 1004 zh_SG 1404 zh_MO
	    05 cs 0405 cs_CZ
	    06 da 0406 da_DK
	    07 de 0407 de_DE 0807 de_CH 0c07 de_AT 1007 de_LU 1407 de_LI
	    08 el 0408 el_GR
	    09 en 0409 en_US 0809 en_GB 0c09 en_AU 1009 en_CA 1409 en_NZ
		  1809 en_IE 1c09 en_ZA 2009 en_JM 2409 en_GD 2809 en_BZ
		  2c09 en_TT 3009 en_ZW 3409 en_PH
	    0a es 040a es_ES 080a es_MX 0c0a es_ES@modern 100a es_GT 140a es_CR
		  180a es_PA 1c0a es_DO 200a es_VE 240a es_CO 280a es_PE
		  2c0a es_AR 300a es_EC 340a es_CL 380a es_UY 3c0a es_PY
		  400a es_BO 440a es_SV 480a es_HN 4c0a es_NI 500a es_PR
	    0b fi 040b fi_FI
	    0c fr 040c fr_FR 080c fr_BE 0c0c fr_CA 100c fr_CH 140c fr_LU
		  180c fr_MC
	    0d he 040d he_IL
	    0e hu 040e hu_HU
	    0f is 040f is_IS
	    10 it 0410 it_IT 0810 it_CH
	    11 ja 0411 ja_JP
	    12 ko 0412 ko_KR
	    13 nl 0413 nl_NL 0813 nl_BE
	    14 no 0414 no_NO 0814 nn_NO
	    15 pl 0415 pl_PL
	    16 pt 0416 pt_BR 0816 pt_PT
	    17 rm 0417 rm_CH
	    18 ro 0418 ro_RO
	    19 ru
	    1a hr 041a hr_HR 081a sr_YU 0c1a sr_YU@cyrillic
	    1b sk 041b sk_SK
	    1c sq 041c sq_AL
	    1d sv 041d sv_SE 081d sv_FI
	    1e th 041e th_TH
	    1f tr 041f tr_TR
	    20 ur 0420 ur_PK 0820 ur_IN
	    21 id 0421 id_ID
	    22 uk 0422 uk_UA
	    23 be 0423 be_BY
	    24 sl 0424 sl_SI
	    25 et 0425 et_EE
	    26 lv 0426 lv_LV
	    27 lt 0427 lt_LT
	    28 tg 0428 tg_TJ
	    29 fa 0429 fa_IR
	    2a vi 042a vi_VN
	    2b hy 042b hy_AM
	    2c az 042c az_AZ@latin 082c az_AZ@cyrillic
	    2d eu
	    2e wen 042e wen_DE
	    2f mk 042f mk_MK
	    30 bnt 0430 bnt_TZ
	    31 ts 0431 ts_ZA
	    33 ven 0433 ven_ZA
	    34 xh 0434 xh_ZA
	    35 zu 0435 zu_ZA
	    36 af 0436 af_ZA
	    37 ka 0437 ka_GE
	    38 fo 0438 fo_FO
	    39 hi 0439 hi_IN
	    3a mt 043a mt_MT
	    3b se 043b se_NO
	    043c gd_UK 083c ga_IE
	    3d yi 043d yi_IL
	    3e ms 043e ms_MY 083e ms_BN
	    3f kk 043f kk_KZ
	    40 ky 0440 ky_KG
	    41 sw 0441 sw_KE
	    42 tk 0442 tk_TM
	    43 uz 0443 uz_UZ@latin 0843 uz_UZ@cyrillic
	    44 tt 0444 tt_RU
	    45 bn 0445 bn_IN
	    46 pa 0446 pa_IN
	    47 gu 0447 gu_IN
	    48 or 0448 or_IN
	    49 ta
	    4a te 044a te_IN
	    4b kn 044b kn_IN
	    4c ml 044c ml_IN
	    4d as 044d as_IN
	    4e mr 044e mr_IN
	    4f sa 044f sa_IN
	    50 mn
	    51 bo 0451 bo_CN
	    52 cy 0452 cy_GB
	    53 km 0453 km_KH
	    54 lo 0454 lo_LA
	    55 my 0455 my_MM
	    56 gl 0456 gl_ES
	    57 kok 0457 kok_IN
	    58 mni 0458 mni_IN
	    59 sd
	    5a syr 045a syr_TR
	    5b si 045b si_LK
	    5c chr 045c chr_US
	    5d iu 045d iu_CA
	    5e am 045e am_ET
	    5f ber 045f ber_MA
	    60 ks 0460 ks_PK 0860 ks_IN
	    61 ne 0461 ne_NP 0861 ne_IN
	    62 fy 0462 fy_NL
	    63 ps
	    64 tl 0464 tl_PH
	    65 div 0465 div_MV
	    66 bin 0466 bin_NG
	    67 ful 0467 ful_NG
	    68 ha 0468 ha_NG
	    69 nic 0469 nic_NG
	    6a yo 046a yo_NG
	    70 ibo 0470 ibo_NG
	    71 kau 0471 kau_NG
	    72 om 0472 om_ET
	    73 ti 0473 ti_ET
	    74 gn 0474 gn_PY
	    75 cpe 0475 cpe_US
	    76 la 0476 la_VA
	    77 so 0477 so_SO
	    78 sit 0478 sit_CN
	    79 pap 0479 pap_AN
	}]
    }
}

# msgcat::mc --
#
#	Find the translation for the given string based on the current
#	locale setting. Check the local namespace first, then look in each
#	parent namespace until the source is found.  If additional args are
#	specified, use the format command to work them into the traslated
#	string.
#
# Arguments:
#	src	The string to translate.
#	args	Args to pass to the format command
#
# Results:
#	Returns the translated string.  Propagates errors thrown by the 
#	format command.

proc msgcat::mc {src args} {
    # Check for the src in each namespace starting from the local and
    # ending in the global.

    variable Msgs
    variable Loclist
    variable Locale

    set ns [uplevel 1 [list ::namespace current]]
    
    while {$ns != ""} {
	foreach loc $Loclist {
	    if {[dict exists $Msgs $loc $ns $src]} {
		if {[llength $args] == 0} {
		    return [dict get $Msgs $loc $ns $src]
		} else {
		    return [format [dict get $Msgs $loc $ns $src] {*}$args]
		}
	    }
	}
	set ns [namespace parent $ns]
    }
    # we have not found the translation
    return [uplevel 1 [list [namespace origin mcunknown] \
	    $Locale $src {*}$args]]
}

# msgcat::mclocale --
#
#	Query or set the current locale.
#
# Arguments:
#	newLocale	(Optional) The new locale string. Locale strings
#			should be composed of one or more sublocale parts
#			separated by underscores (e.g. en_US).
#
# Results:
#	Returns the current locale.

proc msgcat::mclocale {args} {
    variable Loclist
    variable Locale
    set len [llength $args]

    if {$len > 1} {
	return -code error "wrong # args: should be\
		\"[lindex [info level 0] 0] ?newLocale?\""
    }

    if {$len == 1} {
	set newLocale [lindex $args 0]
	if {$newLocale ne [file tail $newLocale]} {
	    return -code error "invalid newLocale value \"$newLocale\":\
		    could be path to unsafe code."
	}
	set Locale [string tolower $newLocale]
	set Loclist {}
	set word ""
	foreach part [split $Locale _] {
	    set word [string trim "${word}_${part}" _]
	    if {$word ne [lindex $Loclist 0]} {
		set Loclist [linsert $Loclist 0 $word]
	    }
	}
	lappend Loclist {}
	set Locale [lindex $Loclist 0]
    }
    return $Locale
}

# msgcat::mcpreferences --
#
#	Fetch the list of locales used to look up strings, ordered from
#	most preferred to least preferred.
#
# Arguments:
#	None.
#
# Results:
#	Returns an ordered list of the locales preferred by the user.

proc msgcat::mcpreferences {} {
    variable Loclist
    return $Loclist
}

# msgcat::mcload --
#
#	Attempt to load message catalogs for each locale in the
#	preference list from the specified directory.
#
# Arguments:
#	langdir		The directory to search.
#
# Results:
#	Returns the number of message catalogs that were loaded.

proc msgcat::mcload {langdir} {
    set x 0
    foreach p [mcpreferences] {
	if { $p eq {} } {
	    set p ROOT
	}
	set langfile [file join $langdir $p.msg]
	if {[file exists $langfile]} {
	    incr x
	    uplevel 1 [list ::source -encoding utf-8 $langfile]
	}
    }
    return $x
}

# msgcat::mcset --
#
#	Set the translation for a given string in a specified locale.
#
# Arguments:
#	locale		The locale to use.
#	src		The source string.
#	dest		(Optional) The translated string.  If omitted,
#			the source string is used.
#
# Results:
#	Returns the new locale.

proc msgcat::mcset {locale src {dest ""}} {
    variable Msgs
    if {[llength [info level 0]] == 3} { ;# dest not specified
	set dest $src
    }

    set ns [uplevel 1 [list ::namespace current]]
    
    set locale [string tolower $locale]
    
    # create nested dictionaries if they do not exist
    if {![dict exists $Msgs $locale]} {
        dict set Msgs $locale  [dict create] 
    }
    if {![dict exists $Msgs $locale $ns]} {
        dict set Msgs $locale $ns [dict create]
    }
    dict set Msgs $locale $ns $src $dest
    return $dest
}

# msgcat::mcmset --
#
#	Set the translation for multiple strings in a specified locale.
#
# Arguments:
#	locale		The locale to use.
#	pairs		One or more src/dest pairs (must be even length)
#
# Results:
#	Returns the number of pairs processed

proc msgcat::mcmset {locale pairs } {
    variable Msgs

    set length [llength $pairs]
    if {$length % 2} {
	return -code error "bad translation list:\
		 should be \"[lindex [info level 0] 0] locale {src dest ...}\""
    }
    
    set locale [string tolower $locale]
    set ns [uplevel 1 [list ::namespace current]]

    # create nested dictionaries if they do not exist
    if {![dict exists $Msgs $locale]} {
        dict set Msgs $locale  [dict create] 
    }
    if {![dict exists $Msgs $locale $ns]} {
        dict set Msgs $locale $ns [dict create]
    }    
    foreach {src dest} $pairs {
        dict set Msgs $locale $ns $src $dest
    }

    return $length
}

# msgcat::mcunknown --
#
#	This routine is called by msgcat::mc if a translation cannot
#	be found for a string.  This routine is intended to be replaced
#	by an application specific routine for error reporting
#	purposes.  The default behavior is to return the source string.  
#	If additional args are specified, the format command will be used
#	to work them into the traslated string.
#
# Arguments:
#	locale		The current locale.
#	src		The string to be translated.
#	args		Args to pass to the format command
#
# Results:
#	Returns the translated value.

proc msgcat::mcunknown {locale src args} {
    if {[llength $args]} {
	return [format $src {*}$args]
    } else {
	return $src
    }
}

# msgcat::mcmax --
#
#	Calculates the maximum length of the translated strings of the given 
#	list.
#
# Arguments:
#	args	strings to translate.
#
# Results:
#	Returns the length of the longest translated string.

proc msgcat::mcmax {args} {
    set max 0
    foreach string $args {
	set translated [uplevel 1 [list [namespace origin mc] $string]]
        set len [string length $translated]
        if {$len>$max} {
	    set max $len
        }
    }
    return $max
}

# Convert the locale values stored in environment variables to a form
# suitable for passing to [mclocale]
proc msgcat::ConvertLocale {value} {
    # Assume $value is of form: $language[_$territory][.$codeset][@modifier]
    # Convert to form: $language[_$territory][_$modifier]
    #
    # Comment out expanded RE version -- bugs alleged
    # regexp -expanded {
    #	^		# Match all the way to the beginning
    #	([^_.@]*)	# Match "lanugage"; ends with _, ., or @
    #	(_([^.@]*))?	# Match (optional) "territory"; starts with _
    #	([.]([^@]*))?	# Match (optional) "codeset"; starts with .
    #	(@(.*))?	# Match (optional) "modifier"; starts with @
    #	$		# Match all the way to the end
    # } $value -> language _ territory _ codeset _ modifier
    if {![regexp {^([^_.@]+)(_([^.@]*))?([.]([^@]*))?(@(.*))?$} $value \
	    -> language _ territory _ codeset _ modifier]} {
	return -code error "invalid locale '$value': empty language part"
    }
    set ret $language
    if {[string length $territory]} {
	append ret _$territory
    }
    if {[string length $modifier]} {
	append ret _$modifier
    }
    return $ret
}

# Initialize the default locale
proc msgcat::Init {} {
    #
    # set default locale, try to get from environment
    #
    foreach varName {LC_ALL LC_MESSAGES LANG} {
	if {[info exists ::env($varName)] && ("" ne $::env($varName))} {
	    if {![catch {
		mclocale [ConvertLocale $::env($varName)]
	    }]} {
		return
	    }
	}
    }
    #
    # On Darwin, fallback to current CFLocale identifier if available.
    #
    if {$::tcl_platform(os) eq "Darwin" && $::tcl_platform(platform) eq "unix"
	    && [info exists ::tcl::mac::locale] && $::tcl::mac::locale ne ""} {
	if {![catch {
	    mclocale [ConvertLocale $::tcl::mac::locale]
	}]} {
	    return
	}
    }
    #
    # The rest of this routine is special processing for Windows;
    # all other platforms, get out now.
    #
    if { $::tcl_platform(platform) ne "windows" } {
	mclocale C
	return
    }
    #
    # On Windows, try to set locale depending on registry settings,
    # or fall back on locale of "C".  
    #
    set key {HKEY_CURRENT_USER\Control Panel\International}
    if {[catch {package require registry}] \
	    || [catch {registry get $key "locale"} locale]} {
        mclocale C
	return
    }
    #
    # Keep trying to match against smaller and smaller suffixes
    # of the registry value, since the latter hexadigits appear
    # to determine general language and earlier hexadigits determine
    # more precise information, such as territory.  For example,
    #     0409 - English - United States
    #     0809 - English - United Kingdom
    # Add more translations to the WinRegToISO639 array above.
    #
    variable WinRegToISO639
    set locale [string tolower $locale]
    while {[string length $locale]} {
	if {![catch {
		mclocale [ConvertLocale [dict get $WinRegToISO639 $locale]]
	}]} {
	    return
	}
	set locale [string range $locale 1 end]
    }
    #
    # No translation known.  Fall back on "C" locale
    #
    mclocale C
}
msgcat::Init
