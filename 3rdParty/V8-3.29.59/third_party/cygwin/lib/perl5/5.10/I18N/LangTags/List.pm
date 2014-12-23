
require 5;
package I18N::LangTags::List;
#  Time-stamp: "2004-10-06 23:26:21 ADT"
use strict;
use vars qw(%Name %Is_Disrec $Debug $VERSION);
$VERSION = '0.35';
# POD at the end.

#----------------------------------------------------------------------
{
# read the table out of our own POD!
  my $seeking = 1;
  my $count = 0;
  my($disrec,$tag,$name);
  my $last_name = '';
  while(<I18N::LangTags::List::DATA>) {
    if($seeking) {
      $seeking = 0 if m/=for woohah/;
    } elsif( ($disrec, $tag, $name) =
          m/(\[?)\{([-0-9a-zA-Z]+)\}(?:\s*:)?\s*([^\[\]]+)/
    ) {
      $name =~ s/\s*[;\.]*\s*$//g;
      next unless $name;
      ++$count;
      print "<$tag> <$name>\n" if $Debug;
      $last_name = $Name{$tag} = $name;
      $Is_Disrec{$tag} = 1 if $disrec;
    } elsif (m/[Ff]ormerly \"([-a-z0-9]+)\"/) {
      $Name{$1} = "$last_name (old tag)" if $last_name;
      $Is_Disrec{$1} = 1;
    }
  }
  die "No tags read??" unless $count;
}
#----------------------------------------------------------------------

sub name {
  my $tag = lc($_[0] || return);
  $tag =~ s/^\s+//s;
  $tag =~ s/\s+$//s;
  
  my $alt;
  if($tag =~ m/^x-(.+)/) {
    $alt = "i-$1";
  } elsif($tag =~ m/^i-(.+)/) {
    $alt = "x-$1";
  } else {
    $alt = '';
  }
  
  my $subform = '';
  my $name = '';
  print "Input: {$tag}\n" if $Debug;
  while(length $tag) {
    last if $name = $Name{$tag};
    last if $name = $Name{$alt};
    if($tag =~ s/(-[a-z0-9]+)$//s) {
      print "Shaving off: $1 leaving $tag\n" if $Debug;
      $subform = "$1$subform";
       # and loop around again
       
      $alt =~ s/(-[a-z0-9]+)$//s && $Debug && print " alt -> $alt\n";
    } else {
      # we're trying to pull a subform off a primary tag. TILT!
      print "Aborting on: {$name}{$subform}\n" if $Debug;
      last;
    }
  }
  print "Output: {$name}{$subform}\n" if $Debug;
  
  return unless $name;   # Failure
  return $name unless $subform;   # Exact match
  $subform =~ s/^-//s;
  $subform =~ s/-$//s;
  return "$name (Subform \"$subform\")";
}

#--------------------------------------------------------------------------

sub is_decent {
  my $tag = lc($_[0] || return 0);
  #require I18N::LangTags;

  return 0 unless
    $tag =~ 
    /^(?:  # First subtag
         [xi] | [a-z]{2,3}
      )
      (?:  # Subtags thereafter
         -           # separator
         [a-z0-9]{1,8}  # subtag  
      )*
    $/xs;

  my @supers = ();
  foreach my $bit (split('-', $tag)) {
    push @supers, 
      scalar(@supers) ? ($supers[-1] . '-' . $bit) : $bit;
  }
  return 0 unless @supers;
  shift @supers if $supers[0] =~ m<^(i|x|sgn)$>s;
  return 0 unless @supers;

  foreach my $f ($tag, @supers) {
    return 0 if $Is_Disrec{$f};
    return 2 if $Name{$f};
     # so that decent subforms of indecent tags are decent
  }
  return 2 if $Name{$tag}; # not only is it decent, it's known!
  return 1;
}

#--------------------------------------------------------------------------
1;

__DATA__

=head1 NAME

I18N::LangTags::List -- tags and names for human languages

=head1 SYNOPSIS

  use I18N::LangTags::List;
  print "Parlez-vous... ", join(', ',
      I18N::LangTags::List::name('elx') || 'unknown_language',
      I18N::LangTags::List::name('ar-Kw') || 'unknown_language',
      I18N::LangTags::List::name('en') || 'unknown_language',
      I18N::LangTags::List::name('en-CA') || 'unknown_language',
    ), "?\n";

prints:

  Parlez-vous... Elamite, Kuwait Arabic, English, Canadian English?

=head1 DESCRIPTION

This module provides a function 
C<I18N::LangTags::List::name( I<langtag> ) > that takes
a language tag (see L<I18N::LangTags|I18N::LangTags>)
and returns the best attempt at an English name for it, or
undef if it can't make sense of the tag.

The function I18N::LangTags::List::name(...) is not exported.

This module also provides a function
C<I18N::LangTags::List::is_decent( I<langtag> )> that returns true iff
the language tag is syntactically valid and is for general use (like
"fr" or "fr-ca", below).  That is, it returns false for tags that are
syntactically invalid and for tags, like "aus", that are listed in
brackets below.  This function is not exported.

The map of tags-to-names that it uses is accessible as
%I18N::LangTags::List::Name, and it's the same as the list
that follows in this documentation, which should be useful
to you even if you don't use this module.

=head1 ABOUT LANGUAGE TAGS

Internet language tags, as defined in RFC 3066, are a formalism
for denoting human languages.  The two-letter ISO 639-1 language
codes are well known (as "en" for English), as are their forms
when qualified by a country code ("en-US").  Less well-known are the
arbitrary-length non-ISO codes (like "i-mingo"), and the 
recently (in 2001) introduced three-letter ISO-639-2 codes.

Remember these important facts:

=over

=item *

Language tags are not locale IDs.  A locale ID is written with a "_"
instead of a "-", (almost?) always matches C<m/^\w\w_\w\w\b/>, and
I<means> something different than a language tag.  A language tag
denotes a language.  A locale ID denotes a language I<as used in>
a particular place, in combination with non-linguistic
location-specific information such as what currency is used
there.  Locales I<also> often denote character set information,
as in "en_US.ISO8859-1".

=item *

Language tags are not for computer languages.

=item *

"Dialect" is not a useful term, since there is no objective
criterion for establishing when two language-forms are
dialects of eachother, or are separate languages.

=item *

Language tags are not case-sensitive.  en-US, en-us, En-Us, etc.,
are all the same tag, and denote the same language.

=item *

Not every language tag really refers to a single language.  Some
language tags refer to conditions: i-default (system-message text
in English plus maybe other languages), und (undetermined
language).  Others (notably lots of the three-letter codes) are
bibliographic tags that classify whole groups of languages, as
with cus "Cushitic (Other)" (i.e., a
language that has been classed as Cushtic, but which has no more
specific code) or the even less linguistically coherent
sai for "South American Indian (Other)".  Though useful in
bibliography, B<SUCH TAGS ARE NOT
FOR GENERAL USE>.  For further guidance, email me.

=item *

Language tags are not country codes.  In fact, they are often
distinct codes, as with language tag ja for Japanese, and
ISO 3166 country code C<.jp> for Japan.

=back

=head1 LIST OF LANGUAGES

The first part of each item is the language tag, between
{...}.  It
is followed by an English name for the language or language-group.
Language tags that I judge to be not for general use, are bracketed.

This list is in alphabetical order by English name of the language.

=for reminder
 The name in the =item line MUST NOT have E<...>'s in it!!

=for woohah START

=over

=item {ab} : Abkhazian

eq Abkhaz

=item {ace} : Achinese

=item {ach} : Acoli

=item {ada} : Adangme

=item {ady} : Adyghe

eq Adygei

=item {aa} : Afar

=item {afh} : Afrihili

(Artificial)

=item {af} : Afrikaans

=item [{afa} : Afro-Asiatic (Other)]

=item {ak} : Akan

(Formerly "aka".)

=item {akk} : Akkadian

(Historical)

=item {sq} : Albanian

=item {ale} : Aleut

=item [{alg} : Algonquian languages]

NOT Algonquin!

=item [{tut} : Altaic (Other)]

=item {am} : Amharic

NOT Aramaic!

=item {i-ami} : Ami

eq Amis.  eq 'Amis.  eq Pangca.

=item [{apa} : Apache languages]

=item {ar} : Arabic

Many forms are mutually un-intelligible in spoken media.
Notable forms:
{ar-ae} UAE Arabic;
{ar-bh} Bahrain Arabic;
{ar-dz} Algerian Arabic;
{ar-eg} Egyptian Arabic;
{ar-iq} Iraqi Arabic;
{ar-jo} Jordanian Arabic;
{ar-kw} Kuwait Arabic;
{ar-lb} Lebanese Arabic;
{ar-ly} Libyan Arabic;
{ar-ma} Moroccan Arabic;
{ar-om} Omani Arabic;
{ar-qa} Qatari Arabic;
{ar-sa} Sauda Arabic;
{ar-sy} Syrian Arabic;
{ar-tn} Tunisian Arabic;
{ar-ye} Yemen Arabic.

=item {arc} : Aramaic

NOT Amharic!  NOT Samaritan Aramaic!

=item {arp} : Arapaho

=item {arn} : Araucanian

=item {arw} : Arawak

=item {hy} : Armenian

=item {an} : Aragonese

=item [{art} : Artificial (Other)]

=item {ast} : Asturian

eq Bable.

=item {as} : Assamese

=item [{ath} : Athapascan languages]

eq Athabaskan.  eq Athapaskan.  eq Athabascan.

=item [{aus} : Australian languages]

=item [{map} : Austronesian (Other)]

=item {av} : Avaric

(Formerly "ava".)

=item {ae} : Avestan

eq Zend

=item {awa} : Awadhi

=item {ay} : Aymara

=item {az} : Azerbaijani

eq Azeri

Notable forms:
{az-Arab} Azerbaijani in Arabic script;
{az-Cyrl} Azerbaijani in Cyrillic script;
{az-Latn} Azerbaijani in Latin script.

=item {ban} : Balinese

=item [{bat} : Baltic (Other)]

=item {bal} : Baluchi

=item {bm} : Bambara

(Formerly "bam".)

=item [{bai} : Bamileke languages]

=item {bad} : Banda

=item [{bnt} : Bantu (Other)]

=item {bas} : Basa

=item {ba} : Bashkir

=item {eu} : Basque

=item {btk} : Batak (Indonesia)

=item {bej} : Beja

=item {be} : Belarusian

eq Belarussian.  eq Byelarussian.
eq Belorussian.  eq Byelorussian.
eq White Russian.  eq White Ruthenian.
NOT Ruthenian!

=item {bem} : Bemba

=item {bn} : Bengali

eq Bangla.

=item [{ber} : Berber (Other)]

=item {bho} : Bhojpuri

=item {bh} : Bihari

=item {bik} : Bikol

=item {bin} : Bini

=item {bi} : Bislama

eq Bichelamar.

=item {bs} : Bosnian

=item {bra} : Braj

=item {br} : Breton

=item {bug} : Buginese

=item {bg} : Bulgarian

=item {i-bnn} : Bunun

=item {bua} : Buriat

=item {my} : Burmese

=item {cad} : Caddo

=item {car} : Carib

=item {ca} : Catalan

eq CatalE<aacute>n.  eq Catalonian.

=item [{cau} : Caucasian (Other)]

=item {ceb} : Cebuano

=item [{cel} : Celtic (Other)]

Notable forms:
{cel-gaulish} Gaulish (Historical)

=item [{cai} : Central American Indian (Other)]

=item {chg} : Chagatai

(Historical?)

=item [{cmc} : Chamic languages]

=item {ch} : Chamorro

=item {ce} : Chechen

=item {chr} : Cherokee

eq Tsalagi

=item {chy} : Cheyenne

=item {chb} : Chibcha

(Historical)  NOT Chibchan (which is a language family).

=item {ny} : Chichewa

eq Nyanja.  eq Chinyanja.

=item {zh} : Chinese

Many forms are mutually un-intelligible in spoken media.
Notable forms:
{zh-Hans} Chinese, in simplified script;
{zh-Hant} Chinese, in traditional script;
{zh-tw} Taiwan Chinese;
{zh-cn} PRC Chinese;
{zh-sg} Singapore Chinese;
{zh-mo} Macau Chinese;
{zh-hk} Hong Kong Chinese;
{zh-guoyu} Mandarin [Putonghua/Guoyu];
{zh-hakka} Hakka [formerly "i-hakka"];
{zh-min} Hokkien;
{zh-min-nan} Southern Hokkien;
{zh-wuu} Shanghaiese;
{zh-xiang} Hunanese;
{zh-gan} Gan;
{zh-yue} Cantonese.

=for etc
{i-hakka} Hakka (old tag)

=item {chn} : Chinook Jargon

eq Chinook Wawa.

=item {chp} : Chipewyan

=item {cho} : Choctaw

=item {cu} : Church Slavic

eq Old Church Slavonic.

=item {chk} : Chuukese

eq Trukese.  eq Chuuk.  eq Truk.  eq Ruk.

=item {cv} : Chuvash

=item {cop} : Coptic

=item {kw} : Cornish

=item {co} : Corsican

eq Corse.

=item {cr} : Cree

NOT Creek!  (Formerly "cre".)

=item {mus} : Creek

NOT Cree!

=item [{cpe} : English-based Creoles and pidgins (Other)]

=item [{cpf} : French-based Creoles and pidgins (Other)]

=item [{cpp} : Portuguese-based Creoles and pidgins (Other)]

=item [{crp} : Creoles and pidgins (Other)]

=item {hr} : Croatian

eq Croat.

=item [{cus} : Cushitic (Other)]

=item {cs} : Czech

=item {dak} : Dakota

eq Nakota.  eq Latoka.

=item {da} : Danish

=item {dar} : Dargwa

=item {day} : Dayak

=item {i-default} : Default (Fallthru) Language

Defined in RFC 2277, this is for tagging text
(which must include English text, and might/should include text
in other appropriate languages) that is emitted in a context
where language-negotiation wasn't possible -- in SMTP mail failure
messages, for example.

=item {del} : Delaware

=item {din} : Dinka

=item {dv} : Divehi

eq Maldivian.  (Formerly "div".)

=item {doi} : Dogri

NOT Dogrib!

=item {dgr} : Dogrib

NOT Dogri!

=item [{dra} : Dravidian (Other)]

=item {dua} : Duala

=item {nl} : Dutch

eq Netherlander.  Notable forms:
{nl-nl} Netherlands Dutch;
{nl-be} Belgian Dutch.

=item {dum} : Middle Dutch (ca.1050-1350)

(Historical)

=item {dyu} : Dyula

=item {dz} : Dzongkha

=item {efi} : Efik

=item {egy} : Ancient Egyptian

(Historical)

=item {eka} : Ekajuk

=item {elx} : Elamite

(Historical)

=item {en} : English

Notable forms:
{en-au} Australian English;
{en-bz} Belize English;
{en-ca} Canadian English;
{en-gb} UK English;
{en-ie} Irish English;
{en-jm} Jamaican English;
{en-nz} New Zealand English;
{en-ph} Philippine English;
{en-tt} Trinidad English;
{en-us} US English;
{en-za} South African English;
{en-zw} Zimbabwe English.

=item {enm} : Old English (1100-1500)

(Historical)

=item {ang} : Old English (ca.450-1100)

eq Anglo-Saxon.  (Historical)

=item {i-enochian} : Enochian (Artificial)

=item {myv} : Erzya

=item {eo} : Esperanto

(Artificial)

=item {et} : Estonian

=item {ee} : Ewe

(Formerly "ewe".)

=item {ewo} : Ewondo

=item {fan} : Fang

=item {fat} : Fanti

=item {fo} : Faroese

=item {fj} : Fijian

=item {fi} : Finnish

=item [{fiu} : Finno-Ugrian (Other)]

eq Finno-Ugric.  NOT Ugaritic!

=item {fon} : Fon

=item {fr} : French

Notable forms:
{fr-fr} France French;
{fr-be} Belgian French;
{fr-ca} Canadian French;
{fr-ch} Swiss French;
{fr-lu} Luxembourg French;
{fr-mc} Monaco French.

=item {frm} : Middle French (ca.1400-1600)

(Historical)

=item {fro} : Old French (842-ca.1400)

(Historical)

=item {fy} : Frisian

=item {fur} : Friulian

=item {ff} : Fulah

(Formerly "ful".)

=item {gaa} : Ga

=item {gd} : Scots Gaelic

NOT Scots!

=item {gl} : Gallegan

eq Galician

=item {lg} : Ganda

(Formerly "lug".)

=item {gay} : Gayo

=item {gba} : Gbaya

=item {gez} : Geez

eq Ge'ez

=item {ka} : Georgian

=item {de} : German

Notable forms:
{de-at} Austrian German;
{de-be} Belgian German;
{de-ch} Swiss German;
{de-de} Germany German;
{de-li} Liechtenstein German;
{de-lu} Luxembourg German.

=item {gmh} : Middle High German (ca.1050-1500)

(Historical)

=item {goh} : Old High German (ca.750-1050)

(Historical)

=item [{gem} : Germanic (Other)]

=item {gil} : Gilbertese

=item {gon} : Gondi

=item {gor} : Gorontalo

=item {got} : Gothic

(Historical)

=item {grb} : Grebo

=item {grc} : Ancient Greek

(Historical)  (Until 15th century or so.)

=item {el} : Modern Greek

(Since 15th century or so.)

=item {gn} : Guarani

GuaranE<iacute>

=item {gu} : Gujarati

=item {gwi} : Gwich'in

eq Gwichin

=item {hai} : Haida

=item {ht} : Haitian

eq Haitian Creole

=item {ha} : Hausa

=item {haw} : Hawaiian

Hawai'ian

=item {he} : Hebrew

(Formerly "iw".)

=for etc
{iw} Hebrew (old tag)

=item {hz} : Herero

=item {hil} : Hiligaynon

=item {him} : Himachali

=item {hi} : Hindi

=item {ho} : Hiri Motu

=item {hit} : Hittite

(Historical)

=item {hmn} : Hmong

=item {hu} : Hungarian

=item {hup} : Hupa

=item {iba} : Iban

=item {is} : Icelandic

=item {io} : Ido

(Artificial)

=item {ig} : Igbo

(Formerly "ibo".)

=item {ijo} : Ijo

=item {ilo} : Iloko

=item [{inc} : Indic (Other)]

=item [{ine} : Indo-European (Other)]

=item {id} : Indonesian

(Formerly "in".)

=for etc
{in} Indonesian (old tag)

=item {inh} : Ingush

=item {ia} : Interlingua (International Auxiliary Language Association)

(Artificial)  NOT Interlingue!

=item {ie} : Interlingue

(Artificial)  NOT Interlingua!

=item {iu} : Inuktitut

A subform of "Eskimo".

=item {ik} : Inupiaq

A subform of "Eskimo".

=item [{ira} : Iranian (Other)]

=item {ga} : Irish

=item {mga} : Middle Irish (900-1200)

(Historical)

=item {sga} : Old Irish (to 900)

(Historical)

=item [{iro} : Iroquoian languages]

=item {it} : Italian

Notable forms:
{it-it} Italy Italian;
{it-ch} Swiss Italian.

=item {ja} : Japanese

(NOT "jp"!)

=item {jv} : Javanese

(Formerly "jw" because of a typo.)

=item {jrb} : Judeo-Arabic

=item {jpr} : Judeo-Persian

=item {kbd} : Kabardian

=item {kab} : Kabyle

=item {kac} : Kachin

=item {kl} : Kalaallisut

eq Greenlandic "Eskimo"

=item {xal} : Kalmyk

=item {kam} : Kamba

=item {kn} : Kannada

eq Kanarese.  NOT Canadian!

=item {kr} : Kanuri

(Formerly "kau".)

=item {krc} : Karachay-Balkar

=item {kaa} : Kara-Kalpak

=item {kar} : Karen

=item {ks} : Kashmiri

=item {csb} : Kashubian

eq Kashub

=item {kaw} : Kawi

=item {kk} : Kazakh

=item {kha} : Khasi

=item {km} : Khmer

eq Cambodian.  eq Kampuchean.

=item [{khi} : Khoisan (Other)]

=item {kho} : Khotanese

=item {ki} : Kikuyu

eq Gikuyu.

=item {kmb} : Kimbundu

=item {rw} : Kinyarwanda

=item {ky} : Kirghiz

=item {i-klingon} : Klingon

=item {kv} : Komi

=item {kg} : Kongo

(Formerly "kon".)

=item {kok} : Konkani

=item {ko} : Korean

=item {kos} : Kosraean

=item {kpe} : Kpelle

=item {kro} : Kru

=item {kj} : Kuanyama

=item {kum} : Kumyk

=item {ku} : Kurdish

=item {kru} : Kurukh

=item {kut} : Kutenai

=item {lad} : Ladino

eq Judeo-Spanish.  NOT Ladin (a minority language in Italy).

=item {lah} : Lahnda

NOT Lamba!

=item {lam} : Lamba

NOT Lahnda!

=item {lo} : Lao

eq Laotian.

=item {la} : Latin

(Historical)  NOT Ladin!  NOT Ladino!

=item {lv} : Latvian

eq Lettish.

=item {lb} : Letzeburgesch

eq Luxemburgian, eq Luxemburger.  (Formerly "i-lux".)

=for etc
{i-lux} Letzeburgesch (old tag)

=item {lez} : Lezghian

=item {li} : Limburgish

eq Limburger, eq Limburgan.  NOT Letzeburgesch!

=item {ln} : Lingala

=item {lt} : Lithuanian

=item {nds} : Low German

eq Low Saxon.  eq Low German.  eq Low Saxon.

=item {art-lojban} : Lojban (Artificial)

=item {loz} : Lozi

=item {lu} : Luba-Katanga

(Formerly "lub".)

=item {lua} : Luba-Lulua

=item {lui} : Luiseno

eq LuiseE<ntilde>o.

=item {lun} : Lunda

=item {luo} : Luo (Kenya and Tanzania)

=item {lus} : Lushai

=item {mk} : Macedonian

eq the modern Slavic language spoken in what was Yugoslavia.
NOT the form of Greek spoken in Greek Macedonia!

=item {mad} : Madurese

=item {mag} : Magahi

=item {mai} : Maithili

=item {mak} : Makasar

=item {mg} : Malagasy

=item {ms} : Malay

NOT Malayalam!

=item {ml} : Malayalam

NOT Malay!

=item {mt} : Maltese

=item {mnc} : Manchu

=item {mdr} : Mandar

NOT Mandarin!

=item {man} : Mandingo

=item {mni} : Manipuri

eq Meithei.

=item [{mno} : Manobo languages]

=item {gv} : Manx

=item {mi} : Maori

NOT Mari!

=item {mr} : Marathi

=item {chm} : Mari

NOT Maori!

=item {mh} : Marshall

eq Marshallese.

=item {mwr} : Marwari

=item {mas} : Masai

=item [{myn} : Mayan languages]

=item {men} : Mende

=item {mic} : Micmac

=item {min} : Minangkabau

=item {i-mingo} : Mingo

eq the Irquoian language West Virginia Seneca.  NOT New York Seneca!

=item [{mis} : Miscellaneous languages]

Don't use this.

=item {moh} : Mohawk

=item {mdf} : Moksha

=item {mo} : Moldavian

eq Moldovan.

=item [{mkh} : Mon-Khmer (Other)]

=item {lol} : Mongo

=item {mn} : Mongolian

eq Mongol.

=item {mos} : Mossi

=item [{mul} : Multiple languages]

Not for normal use.

=item [{mun} : Munda languages]

=item {nah} : Nahuatl

=item {nap} : Neapolitan

=item {na} : Nauru

=item {nv} : Navajo

eq Navaho.  (Formerly "i-navajo".)

=for etc
{i-navajo} Navajo (old tag)

=item {nd} : North Ndebele

=item {nr} : South Ndebele

=item {ng} : Ndonga

=item {ne} : Nepali

eq Nepalese.  Notable forms:
{ne-np} Nepal Nepali;
{ne-in} India Nepali.

=item {new} : Newari

=item {nia} : Nias

=item [{nic} : Niger-Kordofanian (Other)]

=item [{ssa} : Nilo-Saharan (Other)]

=item {niu} : Niuean

=item {nog} : Nogai

=item {non} : Old Norse

(Historical)

=item [{nai} : North American Indian]

Do not use this.

=item {no} : Norwegian

Note the two following forms:

=item {nb} : Norwegian Bokmal

eq BokmE<aring>l, (A form of Norwegian.)  (Formerly "no-bok".)

=for etc
{no-bok} Norwegian Bokmal (old tag)

=item {nn} : Norwegian Nynorsk

(A form of Norwegian.)  (Formerly "no-nyn".)

=for etc
{no-nyn} Norwegian Nynorsk (old tag)

=item [{nub} : Nubian languages]

=item {nym} : Nyamwezi

=item {nyn} : Nyankole

=item {nyo} : Nyoro

=item {nzi} : Nzima

=item {oc} : Occitan (post 1500)

eq ProvenE<ccedil>al, eq Provencal

=item {oj} : Ojibwa

eq Ojibwe.  (Formerly "oji".)

=item {or} : Oriya

=item {om} : Oromo

=item {osa} : Osage

=item {os} : Ossetian; Ossetic

=item [{oto} : Otomian languages]

Group of languages collectively called "OtomE<iacute>".

=item {pal} : Pahlavi

eq Pahlevi

=item {i-pwn} : Paiwan

eq Pariwan

=item {pau} : Palauan

=item {pi} : Pali

(Historical?)

=item {pam} : Pampanga

=item {pag} : Pangasinan

=item {pa} : Panjabi

eq Punjabi

=item {pap} : Papiamento

eq Papiamentu.

=item [{paa} : Papuan (Other)]

=item {fa} : Persian

eq Farsi.  eq Iranian.

=item {peo} : Old Persian (ca.600-400 B.C.)

=item [{phi} : Philippine (Other)]

=item {phn} : Phoenician

(Historical)

=item {pon} : Pohnpeian

NOT Pompeiian!

=item {pl} : Polish

=item {pt} : Portuguese

eq Portugese.  Notable forms:
{pt-pt} Portugal Portuguese;
{pt-br} Brazilian Portuguese.

=item [{pra} : Prakrit languages]

=item {pro} : Old Provencal (to 1500)

eq Old ProvenE<ccedil>al.  (Historical.)

=item {ps} : Pushto

eq Pashto.  eq Pushtu.

=item {qu} : Quechua

eq Quecha.

=item {rm} : Raeto-Romance

eq Romansh.

=item {raj} : Rajasthani

=item {rap} : Rapanui

=item {rar} : Rarotongan

=item [{qaa - qtz} : Reserved for local use.]

=item [{roa} : Romance (Other)]

NOT Romanian!  NOT Romany!  NOT Romansh!

=item {ro} : Romanian

eq Rumanian.  NOT Romany!

=item {rom} : Romany

eq Rom.  NOT Romanian!

=item {rn} : Rundi

=item {ru} : Russian

NOT White Russian!  NOT Rusyn!

=item [{sal} : Salishan languages]

Large language group.

=item {sam} : Samaritan Aramaic

NOT Aramaic!

=item {se} : Northern Sami

eq Lappish.  eq Lapp.  eq (Northern) Saami.

=item {sma} : Southern Sami

=item {smn} : Inari Sami

=item {smj} : Lule Sami

=item {sms} : Skolt Sami

=item [{smi} : Sami languages (Other)]

=item {sm} : Samoan

=item {sad} : Sandawe

=item {sg} : Sango

=item {sa} : Sanskrit

(Historical)

=item {sat} : Santali

=item {sc} : Sardinian

eq Sard.

=item {sas} : Sasak

=item {sco} : Scots

NOT Scots Gaelic!

=item {sel} : Selkup

=item [{sem} : Semitic (Other)]

=item {sr} : Serbian

eq Serb.  NOT Sorbian.

Notable forms:
{sr-Cyrl} : Serbian in Cyrillic script;
{sr-Latn} : Serbian in Latin script.

=item {srr} : Serer

=item {shn} : Shan

=item {sn} : Shona

=item {sid} : Sidamo

=item {sgn-...} : Sign Languages

Always use with a subtag.  Notable forms:
{sgn-gb} British Sign Language (BSL);
{sgn-ie} Irish Sign Language (ESL);
{sgn-ni} Nicaraguan Sign Language (ISN);
{sgn-us} American Sign Language (ASL).

(And so on with other country codes as the subtag.)

=item {bla} : Siksika

eq Blackfoot.  eq Pikanii.

=item {sd} : Sindhi

=item {si} : Sinhalese

eq Sinhala.

=item [{sit} : Sino-Tibetan (Other)]

=item [{sio} : Siouan languages]

=item {den} : Slave (Athapascan)

("Slavey" is a subform.)

=item [{sla} : Slavic (Other)]

=item {sk} : Slovak

eq Slovakian.

=item {sl} : Slovenian

eq Slovene.

=item {sog} : Sogdian

=item {so} : Somali

=item {son} : Songhai

=item {snk} : Soninke

=item {wen} : Sorbian languages

eq Wendish.  eq Sorb.  eq Lusatian.  eq Wend.  NOT Venda!  NOT Serbian!

=item {nso} : Northern Sotho

=item {st} : Southern Sotho

eq Sutu.  eq Sesotho.

=item [{sai} : South American Indian (Other)]

=item {es} : Spanish

Notable forms:
{es-ar} Argentine Spanish;
{es-bo} Bolivian Spanish;
{es-cl} Chilean Spanish;
{es-co} Colombian Spanish;
{es-do} Dominican Spanish;
{es-ec} Ecuadorian Spanish;
{es-es} Spain Spanish;
{es-gt} Guatemalan Spanish;
{es-hn} Honduran Spanish;
{es-mx} Mexican Spanish;
{es-pa} Panamanian Spanish;
{es-pe} Peruvian Spanish;
{es-pr} Puerto Rican Spanish;
{es-py} Paraguay Spanish;
{es-sv} Salvadoran Spanish;
{es-us} US Spanish;
{es-uy} Uruguayan Spanish;
{es-ve} Venezuelan Spanish.

=item {suk} : Sukuma

=item {sux} : Sumerian

(Historical)

=item {su} : Sundanese

=item {sus} : Susu

=item {sw} : Swahili

eq Kiswahili

=item {ss} : Swati

=item {sv} : Swedish

Notable forms:
{sv-se} Sweden Swedish;
{sv-fi} Finland Swedish.

=item {syr} : Syriac

=item {tl} : Tagalog

=item {ty} : Tahitian

=item [{tai} : Tai (Other)]

NOT Thai!

=item {tg} : Tajik

=item {tmh} : Tamashek

=item {ta} : Tamil

=item {i-tao} : Tao

eq Yami.

=item {tt} : Tatar

=item {i-tay} : Tayal

eq Atayal.  eq Atayan.

=item {te} : Telugu

=item {ter} : Tereno

=item {tet} : Tetum

=item {th} : Thai

NOT Tai!

=item {bo} : Tibetan

=item {tig} : Tigre

=item {ti} : Tigrinya

=item {tem} : Timne

eq Themne.  eq Timene.

=item {tiv} : Tiv

=item {tli} : Tlingit

=item {tpi} : Tok Pisin

=item {tkl} : Tokelau

=item {tog} : Tonga (Nyasa)

NOT Tsonga!

=item {to} : Tonga (Tonga Islands)

(Pronounced "Tong-a", not "Tong-ga")

NOT Tsonga!

=item {tsi} : Tsimshian

eq Sm'algyax

=item {ts} : Tsonga

NOT Tonga!

=item {i-tsu} : Tsou

=item {tn} : Tswana

Same as Setswana.

=item {tum} : Tumbuka

=item [{tup} : Tupi languages]

=item {tr} : Turkish

(Typically in Roman script)

=item {ota} : Ottoman Turkish (1500-1928)

(Typically in Arabic script)  (Historical)

=item {crh} : Crimean Turkish

eq Crimean Tatar

=item {tk} : Turkmen

eq Turkmeni.

=item {tvl} : Tuvalu

=item {tyv} : Tuvinian

eq Tuvan.  eq Tuvin.

=item {tw} : Twi

=item {udm} : Udmurt

=item {uga} : Ugaritic

NOT Ugric!

=item {ug} : Uighur

=item {uk} : Ukrainian

=item {umb} : Umbundu

=item {und} : Undetermined

Not a tag for normal use.

=item {ur} : Urdu

=item {uz} : Uzbek

eq E<Ouml>zbek

Notable forms:
{uz-Cyrl} Uzbek in Cyrillic script;
{uz-Latn} Uzbek in Latin script.

=item {vai} : Vai

=item {ve} : Venda

NOT Wendish!  NOT Wend!  NOT Avestan!  (Formerly "ven".)

=item {vi} : Vietnamese

eq Viet.

=item {vo} : Volapuk

eq VolapE<uuml>k.  (Artificial)

=item {vot} : Votic

eq Votian.  eq Vod.

=item [{wak} : Wakashan languages]

=item {wa} : Walloon

=item {wal} : Walamo

eq Wolaytta.

=item {war} : Waray

Presumably the Philippine language Waray-Waray (SamareE<ntilde>o),
not the smaller Philippine language Waray Sorsogon, nor the extinct
Australian language Waray.

=item {was} : Washo

eq Washoe

=item {cy} : Welsh

=item {wo} : Wolof

=item {x-...} : Unregistered (Semi-Private Use)

"x-" is a prefix for language tags that are not registered with ISO
or IANA.  Example, x-double-dutch

=item {xh} : Xhosa

=item {sah} : Yakut

=item {yao} : Yao

(The Yao in Malawi?)

=item {yap} : Yapese

eq Yap

=item {ii} : Sichuan Yi

=item {yi} : Yiddish

Formerly "ji".  Usually in Hebrew script.

Notable forms:
{yi-latn} Yiddish in Latin script

=item {yo} : Yoruba

=item [{ypk} : Yupik languages]

Several "Eskimo" languages.

=item {znd} : Zande

=item [{zap} : Zapotec]

(A group of languages.)

=item {zen} : Zenaga

NOT Zend.

=item {za} : Zhuang

=item {zu} : Zulu

=item {zun} : Zuni

eq ZuE<ntilde>i

=back

=for woohah END

=head1 SEE ALSO

L<I18N::LangTags|I18N::LangTags> and its "See Also" section.

=head1 COPYRIGHT AND DISCLAIMER

Copyright (c) 2001+ Sean M. Burke. All rights reserved.

You can redistribute and/or
modify this document under the same terms as Perl itself.

This document is provided in the hope that it will be
useful, but without any warranty;
without even the implied warranty of accuracy, authoritativeness,
completeness, merchantability, or fitness for a particular purpose.

Email any corrections or questions to me.

=head1 AUTHOR

Sean M. Burke, sburkeE<64>cpan.org

=cut


# To generate a list of just the two and three-letter codes:

#!/usr/local/bin/perl -w

require 5; # Time-stamp: "2001-03-13 21:53:39 MST"
 # Sean M. Burke, sburke@cpan.org
 # This program is for generating the language_codes.txt file
use strict;
use LWP::Simple;
use HTML::TreeBuilder 3.10;
my $root = HTML::TreeBuilder->new();
my $url = 'http://lcweb.loc.gov/standards/iso639-2/bibcodes.html';
$root->parse(get($url) || die "Can't get $url");
$root->eof();

my @codes;

foreach my $tr ($root->find_by_tag_name('tr')) {
  my @f = map $_->as_text(), $tr->content_list();
  #print map("<$_> ", @f), "\n";
  next unless @f == 5;
  pop @f; # nix the French name
  next if $f[-1] eq 'Language Name (English)'; # it's a header line
  my $xx = splice(@f, 2,1); # pull out the two-letter code
  $f[-1] =~ s/^\s+//;
  $f[-1] =~ s/\s+$//;
  if($xx =~ m/[a-zA-Z]/) {   # there's a two-letter code for it
    push   @codes, [ lc($f[-1]),   "$xx\t$f[-1]\n" ];
  } else { # print the three-letter codes.
    if($f[0] eq $f[1]) {
      push @codes, [ lc($f[-1]), "$f[1]\t$f[2]\n" ];
    } else { # shouldn't happen
      push @codes, [ lc($f[-1]), "@f !!!!!!!!!!\n" ]; 
    }
  }
}

print map $_->[1], sort {; $a->[0] cmp $b->[0] } @codes;
print "[ based on $url\n at ", scalar(localtime), "]\n",
  "[Note: doesn't include IANA-registered codes.]\n";
exit;
__END__

