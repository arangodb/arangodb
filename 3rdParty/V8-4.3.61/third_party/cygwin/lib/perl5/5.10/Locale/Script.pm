#
# Locale::Script - ISO codes for script identification (ISO 15924)
#
# $Id: Script.pm,v 2.7 2004/06/10 21:19:34 neilb Exp $
#

package Locale::Script;
use strict;
require 5.002;

require Exporter;
use Carp;
use Locale::Constants;


#-----------------------------------------------------------------------
#	Public Global Variables
#-----------------------------------------------------------------------
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK);
$VERSION   = sprintf("%d.%02d", q$Revision: 2.7 $ =~ /(\d+)\.(\d+)/);
@ISA       = qw(Exporter);
@EXPORT    = qw(code2script script2code
                all_script_codes all_script_names
		script_code2code
		LOCALE_CODE_ALPHA_2 LOCALE_CODE_ALPHA_3 LOCALE_CODE_NUMERIC);

#-----------------------------------------------------------------------
#	Private Global Variables
#-----------------------------------------------------------------------
my $CODES     = [];
my $COUNTRIES = [];


#=======================================================================
#
# code2script ( CODE [, CODESET ] )
#
#=======================================================================
sub code2script
{
    my $code = shift;
    my $codeset = @_ > 0 ? shift : LOCALE_CODE_DEFAULT;


    return undef unless defined $code;

    #-------------------------------------------------------------------
    # Make sure the code is in the right form before we use it
    # to look up the corresponding script.
    # We have to sprintf because the codes are given as 3-digits,
    # with leading 0's. Eg 070 for Egyptian demotic.
    #-------------------------------------------------------------------
    if ($codeset == LOCALE_CODE_NUMERIC)
    {
	return undef if ($code =~ /\D/);
	$code = sprintf("%.3d", $code);
    }
    else
    {
	$code = lc($code);
    }

    if (exists $CODES->[$codeset]->{$code})
    {
        return $CODES->[$codeset]->{$code};
    }
    else
    {
        #---------------------------------------------------------------
        # no such script code!
        #---------------------------------------------------------------
        return undef;
    }
}


#=======================================================================
#
# script2code ( SCRIPT [, CODESET ] )
#
#=======================================================================
sub script2code
{
    my $script = shift;
    my $codeset = @_ > 0 ? shift : LOCALE_CODE_DEFAULT;


    return undef unless defined $script;
    $script = lc($script);
    if (exists $COUNTRIES->[$codeset]->{$script})
    {
        return $COUNTRIES->[$codeset]->{$script};
    }
    else
    {
        #---------------------------------------------------------------
        # no such script!
        #---------------------------------------------------------------
        return undef;
    }
}


#=======================================================================
#
# script_code2code ( CODE, IN-CODESET, OUT-CODESET )
#
#=======================================================================
sub script_code2code
{
    (@_ == 3) or croak "script_code2code() takes 3 arguments!";

    my $code = shift;
    my $inset = shift;
    my $outset = shift;
    my $outcode;
    my $script;


    return undef if $inset == $outset;
    $script = code2script($code, $inset);
    return undef if not defined $script;
    $outcode = script2code($script, $outset);
    return $outcode;
}


#=======================================================================
#
# all_script_codes()
#
#=======================================================================
sub all_script_codes
{
    my $codeset = @_ > 0 ? shift : LOCALE_CODE_DEFAULT;

    return keys %{ $CODES->[$codeset] };
}


#=======================================================================
#
# all_script_names()
#
#=======================================================================
sub all_script_names
{
    my $codeset = @_ > 0 ? shift : LOCALE_CODE_DEFAULT;

    return values %{ $CODES->[$codeset] };
}


#=======================================================================
#
# initialisation code - stuff the DATA into the ALPHA2 hash
#
#=======================================================================
{
    my   ($alpha2, $alpha3, $numeric);
    my    $script;
    local $_;


    while (<DATA>)
    {
        next unless /\S/;
        chop;
        ($alpha2, $alpha3, $numeric, $script) = split(/:/, $_, 4);

        $CODES->[LOCALE_CODE_ALPHA_2]->{$alpha2} = $script;
        $COUNTRIES->[LOCALE_CODE_ALPHA_2]->{"\L$script"} = $alpha2;

	if ($alpha3)
	{
            $CODES->[LOCALE_CODE_ALPHA_3]->{$alpha3} = $script;
            $COUNTRIES->[LOCALE_CODE_ALPHA_3]->{"\L$script"} = $alpha3;
	}

	if ($numeric)
	{
            $CODES->[LOCALE_CODE_NUMERIC]->{$numeric} = $script;
            $COUNTRIES->[LOCALE_CODE_NUMERIC]->{"\L$script"} = $numeric;
	}

    }

    close(DATA);
}

1;

__DATA__
am:ama:130:Aramaic
ar:ara:160:Arabic
av:ave:151:Avestan
bh:bhm:300:Brahmi (Ashoka)
bi:bid:372:Buhid
bn:ben:325:Bengali
bo:bod:330:Tibetan
bp:bpm:285:Bopomofo
br:brl:570:Braille
bt:btk:365:Batak
bu:bug:367:Buginese (Makassar)
by:bys:550:Blissymbols
ca:cam:358:Cham
ch:chu:221:Old Church Slavonic
ci:cir:291:Cirth
cm:cmn:402:Cypro-Minoan
co:cop:205:Coptic
cp:cpr:403:Cypriote syllabary
cy:cyr:220:Cyrillic
ds:dsr:250:Deserel (Mormon)
dv:dvn:315:Devanagari (Nagari)
ed:egd:070:Egyptian demotic
eg:egy:050:Egyptian hieroglyphs
eh:egh:060:Egyptian hieratic
el:ell:200:Greek
eo:eos:210:Etruscan and Oscan
et:eth:430:Ethiopic
gl:glg:225:Glagolitic
gm:gmu:310:Gurmukhi
gt:gth:206:Gothic
gu:guj:320:Gujarati
ha:han:500:Han ideographs
he:heb:125:Hebrew
hg:hgl:420:Hangul
hm:hmo:450:Pahawh Hmong
ho:hoo:371:Hanunoo
hr:hrg:410:Hiragana
hu:hun:176:Old Hungarian runic
hv:hvn:175:Kok Turki runic
hy:hye:230:Armenian
iv:ivl:610:Indus Valley
ja:jap:930:(alias for Han + Hiragana + Katakana)
jl:jlg:445:Cherokee syllabary
jw:jwi:360:Javanese
ka:kam:241:Georgian (Mxedruli)
kh:khn:931:(alias for Hangul + Han)
kk:kkn:411:Katakana
km:khm:354:Khmer
kn:kan:345:Kannada
kr:krn:357:Karenni (Kayah Li)
ks:kst:305:Kharoshthi
kx:kax:240:Georgian (Xucuri)
la:lat:217:Latin
lf:laf:215:Latin (Fraktur variant)
lg:lag:216:Latin (Gaelic variant)
lo:lao:356:Lao
lp:lpc:335:Lepcha (Rong)
md:mda:140:Mandaean
me:mer:100:Meroitic
mh:may:090:Mayan hieroglyphs
ml:mlm:347:Malayalam
mn:mon:145:Mongolian
my:mya:350:Burmese
na:naa:400:Linear A
nb:nbb:401:Linear B
og:ogm:212:Ogham
or:ory:327:Oriya
os:osm:260:Osmanya
ph:phx:115:Phoenician
ph:pah:150:Pahlavi
pl:pld:282:Pollard Phonetic
pq:pqd:295:Klingon plQaD
pr:prm:227:Old Permic
ps:pst:600:Phaistos Disk
rn:rnr:211:Runic (Germanic)
rr:rro:620:Rongo-rongo
sa:sar:110:South Arabian
si:sin:348:Sinhala
sj:syj:137:Syriac (Jacobite variant)
sl:slb:440:Unified Canadian Aboriginal Syllabics
sn:syn:136:Syriac (Nestorian variant)
sw:sww:281:Shavian (Shaw)
sy:syr:135:Syriac (Estrangelo)
ta:tam:346:Tamil
tb:tbw:373:Tagbanwa
te:tel:340:Telugu
tf:tfn:120:Tifnagh
tg:tag:370:Tagalog
th:tha:352:Thai
tn:tna:170:Thaana
tw:twr:290:Tengwar
va:vai:470:Vai
vs:vsp:280:Visible Speech
xa:xas:000:Cuneiform, Sumero-Akkadian
xf:xfa:105:Cuneiform, Old Persian
xk:xkn:412:(alias for Hiragana + Katakana)
xu:xug:106:Cuneiform, Ugaritic
yi:yii:460:Yi
zx:zxx:997:Unwritten language
zy:zyy:998:Undetermined script
zz:zzz:999:Uncoded script
