#
# Locale::Language - ISO two letter codes for language identification (ISO 639)
#
# $Id: Language.pm,v 2.7 2004/06/10 21:19:34 neilb Exp $
#

package Locale::Language;
use strict;
require 5.002;

require Exporter;

#-----------------------------------------------------------------------
#	Public Global Variables
#-----------------------------------------------------------------------
use vars qw($VERSION @ISA @EXPORT);
$VERSION      = sprintf("%d.%02d", q$Revision: 2.7 $ =~ /(\d+)\.(\d+)/);
@ISA          = qw(Exporter);
@EXPORT       = qw(&code2language &language2code
                   &all_language_codes &all_language_names );

#-----------------------------------------------------------------------
#	Private Global Variables
#-----------------------------------------------------------------------
my %CODES     = ();
my %LANGUAGES = ();


#=======================================================================
#
# code2language ( CODE )
#
#=======================================================================
sub code2language
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
        # no such language code!
        #---------------------------------------------------------------
        return undef;
    }
}


#=======================================================================
#
# language2code ( LANGUAGE )
#
#=======================================================================
sub language2code
{
    my $lang = shift;


    return undef unless defined $lang;
    $lang = lc($lang);
    if (exists $LANGUAGES{$lang})
    {
        return $LANGUAGES{$lang};
    }
    else
    {
        #---------------------------------------------------------------
        # no such language!
        #---------------------------------------------------------------
        return undef;
    }
}


#=======================================================================
#
# all_language_codes()
#
#=======================================================================
sub all_language_codes
{
    return keys %CODES;
}


#=======================================================================
#
# all_language_names()
#
#=======================================================================
sub all_language_names
{
    return values %CODES;
}


#=======================================================================
# initialisation code - stuff the DATA into the CODES hash
#=======================================================================
{
    my    $code;
    my    $language;
    local $_;


    while (<DATA>)
    {
        next unless /\S/;
        chop;
        ($code, $language) = split(/:/, $_, 2);
        $CODES{$code} = $language;
        $LANGUAGES{"\L$language"} = $code;
    }

    close(DATA);
}

1;

__DATA__
aa:Afar
ab:Abkhazian
ae:Avestan
af:Afrikaans
am:Amharic
ar:Arabic
as:Assamese
ay:Aymara
az:Azerbaijani

ba:Bashkir
be:Belarusian
bg:Bulgarian
bh:Bihari
bi:Bislama
bn:Bengali
bo:Tibetan
br:Breton
bs:Bosnian

ca:Catalan
ce:Chechen
ch:Chamorro
co:Corsican
cs:Czech
cu:Church Slavic
cv:Chuvash
cy:Welsh

da:Danish
de:German
dz:Dzongkha

el:Greek
en:English
eo:Esperanto
es:Spanish
et:Estonian
eu:Basque

fa:Persian
fi:Finnish
fj:Fijian
fo:Faeroese
fr:French
fy:Frisian

ga:Irish
gd:Gaelic (Scots)
gl:Gallegan
gn:Guarani
gu:Gujarati
gv:Manx

ha:Hausa
he:Hebrew
hi:Hindi
ho:Hiri Motu
hr:Croatian
hu:Hungarian
hy:Armenian
hz:Herero

ia:Interlingua
id:Indonesian
ie:Interlingue
ik:Inupiaq
is:Icelandic
it:Italian
iu:Inuktitut

ja:Japanese
jw:Javanese

ka:Georgian
ki:Kikuyu
kj:Kuanyama
kk:Kazakh
kl:Kalaallisut
km:Khmer
kn:Kannada
ko:Korean
ks:Kashmiri
ku:Kurdish
kv:Komi
kw:Cornish
ky:Kirghiz

la:Latin
lb:Letzeburgesch
ln:Lingala
lo:Lao
lt:Lithuanian
lv:Latvian

mg:Malagasy
mh:Marshall
mi:Maori
mk:Macedonian
ml:Malayalam
mn:Mongolian
mo:Moldavian
mr:Marathi
ms:Malay
mt:Maltese
my:Burmese

na:Nauru
nb:Norwegian Bokmal
nd:Ndebele, North
ne:Nepali
ng:Ndonga
nl:Dutch
nn:Norwegian Nynorsk
no:Norwegian
nr:Ndebele, South
nv:Navajo
ny:Chichewa; Nyanja

oc:Occitan (post 1500)
om:Oromo
or:Oriya
os:Ossetian; Ossetic

pa:Panjabi
pi:Pali
pl:Polish
ps:Pushto
pt:Portuguese

qu:Quechua

rm:Rhaeto-Romance
rn:Rundi
ro:Romanian
ru:Russian
rw:Kinyarwanda

sa:Sanskrit
sc:Sardinian
sd:Sindhi
se:Sami
sg:Sango
si:Sinhalese
sk:Slovak
sl:Slovenian
sm:Samoan
sn:Shona
so:Somali
sq:Albanian
sr:Serbian
ss:Swati
st:Sotho
su:Sundanese
sv:Swedish
sw:Swahili

ta:Tamil
te:Telugu
tg:Tajik
th:Thai
ti:Tigrinya
tk:Turkmen
tl:Tagalog
tn:Tswana
to:Tonga
tr:Turkish
ts:Tsonga
tt:Tatar
tw:Twi

ug:Uighur
uk:Ukrainian
ur:Urdu
uz:Uzbek

vi:Vietnamese
vo:Volapuk

wo:Wolof

xh:Xhosa

yi:Yiddish
yo:Yoruba

za:Zhuang
zh:Chinese
zu:Zulu
