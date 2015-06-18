#
# Locale::Currency - ISO three letter codes for currency identification
#                    (ISO 4217)
#
# $Id: Currency.pm,v 2.7 2004/06/10 21:19:34 neilb Exp $
#

package Locale::Currency;
use strict;
require 5.002;

require Exporter;

#-----------------------------------------------------------------------
#	Public Global Variables
#-----------------------------------------------------------------------
use vars qw($VERSION @ISA @EXPORT);
$VERSION      = sprintf("%d.%02d", q$Revision: 2.7 $ =~ /(\d+)\.(\d+)/);
@ISA          = qw(Exporter);
@EXPORT       = qw(&code2currency &currency2code
                   &all_currency_codes &all_currency_names );

#-----------------------------------------------------------------------
#	Private Global Variables
#-----------------------------------------------------------------------
my %CODES      = ();
my %CURRENCIES = ();


#=======================================================================
#
# code2currency( CODE )
#
#=======================================================================
sub code2currency
{
    my $code = shift;


    return undef unless defined $code;
    $code = lc($code);
    if (exists $CODES{$code})
    {
        return $CODES{$code};
    }
    else
    {
        #---------------------------------------------------------------
        # no such currency code!
        #---------------------------------------------------------------
        return undef;
    }
}


#=======================================================================
#
# currency2code ( CURRENCY )
#
#=======================================================================
sub currency2code
{
    my $curr = shift;


    return undef unless defined $curr;
    $curr = lc($curr);
    if (exists $CURRENCIES{$curr})
    {
        return $CURRENCIES{$curr};
    }
    else
    {
        #---------------------------------------------------------------
        # no such currency!
        #---------------------------------------------------------------
        return undef;
    }
}


#=======================================================================
#
# all_currency_codes()
#
#=======================================================================
sub all_currency_codes
{
    return keys %CODES;
}


#=======================================================================
#
# all_currency_names()
#
#=======================================================================
sub all_currency_names
{
    return values %CODES;
}


#=======================================================================
# initialisation code - stuff the DATA into the CODES hash
#=======================================================================
{
    my    $code;
    my    $currency;
    local $_;


    while (<DATA>)
    {
        next unless /\S/;
        chop;
        ($code, $currency) = split(/:/, $_, 2);
        $CODES{$code} = $currency;
        $CURRENCIES{"\L$currency"} = $code;
    }

    close(DATA);
}

1;

__DATA__
adp:Andorran Peseta
aed:UAE Dirham
afa:Afghani
all:Lek
amd:Armenian Dram
ang:Netherlands Antillean Guilder
aoa:Kwanza
aon:New Kwanza
aor:Kwanza Reajustado
ars:Argentine Peso
ats:Schilling
aud:Australian Dollar
awg:Aruban Guilder
azm:Azerbaijanian Manat

bam:Convertible Marks
bbd:Barbados Dollar
bdt:Taka
bef:Belgian Franc
bgl:Lev
bgn:Bulgarian Lev
bhd:Bahraini Dinar
bhd:Dinar
bif:Burundi Franc
bmd:Bermudian Dollar
bnd:Brunei Dollar
bob:Boliviano
bov:MVDol
brl:Brazilian Real
bsd:Bahamian Dollar
btn:Ngultrum
bwp:Pula
byb:Belarussian Ruble
byr:Belarussian Ruble
bzd:Belize Dollar

cad:Canadian Dollar
cdf:Franc Congolais
chf:Swiss Franc
clf:Unidades de Formento
clp:Chilean Peso
cny:Yuan Renminbi
cop:Colombian Peso
crc:Costa Rican Colon
cup:Cuban Peso
cve:Cape Verde Escudo
cyp:Cyprus Pound
czk:Czech Koruna

dem:German Mark
djf:Djibouti Franc
dkk:Danish Krone
dop:Dominican Peso
dzd:Algerian Dinar

ecs:Sucre
ecv:Unidad de Valor Constante (UVC)
eek:Kroon
egp:Egyptian Pound
ern:Nakfa
esp:Spanish Peseta
etb:Ethiopian Birr
eur:Euro

fim:Markka
fjd:Fiji Dollar
fkp:Falkland Islands Pound
frf:French Franc

gbp:Pound Sterling
gel:Lari
ghc:Cedi
gip:Gibraltar Pound
gmd:Dalasi
gnf:Guinea Franc
grd:Drachma
gtq:Quetzal
gwp:Guinea-Bissau Peso
gyd:Guyana Dollar

hkd:Hong Kong Dollar
hnl:Lempira
hrk:Kuna
htg:Gourde
huf:Forint

idr:Rupiah
iep:Irish Pound
ils:Shekel
inr:Indian Rupee
iqd:Iraqi Dinar
irr:Iranian Rial
isk:Iceland Krona
itl:Italian Lira

jmd:Jamaican Dollar
jod:Jordanian Dinar
jpy:Yen

kes:Kenyan Shilling
kgs:Som
khr:Riel
kmf:Comoro Franc
kpw:North Korean Won
krw:Won
kwd:Kuwaiti Dinar
kyd:Cayman Islands Dollar
kzt:Tenge

lak:Kip
lbp:Lebanese Pound
lkr:Sri Lanka Rupee
lrd:Liberian Dollar
lsl:Loti
ltl:Lithuanian Litas
luf:Luxembourg Franc
lvl:Latvian Lats
lyd:Libyan Dinar

mad:Moroccan Dirham
mdl:Moldovan Leu
mgf:Malagasy Franc
mkd:Denar
mmk:Kyat
mnt:Tugrik
mop:Pataca
mro:Ouguiya
mtl:Maltese Lira
mur:Mauritius Rupee
mvr:Rufiyaa
mwk:Kwacha
mxn:Mexican Nuevo Peso
myr:Malaysian Ringgit
mzm:Metical

nad:Namibia Dollar
ngn:Naira
nio:Cordoba Oro
nlg:Netherlands Guilder
nok:Norwegian Krone
npr:Nepalese Rupee
nzd:New Zealand Dollar

omr:Rial Omani

pab:Balboa
pen:Nuevo Sol
pgk:Kina
php:Philippine Peso
pkr:Pakistan Rupee
pln:Zloty
pte:Portuguese Escudo
pyg:Guarani

qar:Qatari Rial

rol:Leu
rub:Russian Ruble
rur:Russian Ruble
rwf:Rwanda Franc

sar:Saudi Riyal
sbd:Solomon Islands Dollar
scr:Seychelles Rupee
sdd:Sudanese Dinar
sek:Swedish Krona
sgd:Singapore Dollar
shp:St. Helena Pound
sit:Tolar
skk:Slovak Koruna
sll:Leone
sos:Somali Shilling
srg:Surinam Guilder
std:Dobra
svc:El Salvador Colon
syp:Syrian Pound
szl:Lilangeni

thb:Baht
tjr:Tajik Ruble
tmm:Manat
tnd:Tunisian Dollar
top:Pa'anga
tpe:Timor Escudo
trl:Turkish Lira
ttd:Trinidad and Tobago Dollar
twd:New Taiwan Dollar
tzs:Tanzanian Shilling

uah:Hryvnia
uak:Karbovanets
ugx:Uganda Shilling
usd:US Dollar
usn:US Dollar (Next day)
uss:US Dollar (Same day)
uyu:Peso Uruguayo
uzs:Uzbekistan Sum

veb:Bolivar
vnd:Dong
vuv:Vatu

wst:Tala

xaf:CFA Franc BEAC
xag:Silver
xau:Gold
xba:European Composite Unit
xbb:European Monetary Unit
xbc:European Unit of Account 9
xb5:European Unit of Account 17
xcd:East Caribbean Dollar
xdr:SDR
xeu:ECU (until 1998-12-31)
xfu:UIC-Franc
xfo:Gold-Franc
xof:CFA Franc BCEAO
xpd:Palladium
xpf:CFP Franc
xpt:Platinum

yer:Yemeni Rial
yum:New Dinar

zal:Financial Rand
zar:Rand
zmk:Kwacha
zrn:New Zaire
zwd:Zimbabwe Dollar
