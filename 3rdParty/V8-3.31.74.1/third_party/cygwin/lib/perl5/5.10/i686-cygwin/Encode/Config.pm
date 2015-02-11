#
# Demand-load module list
#
package Encode::Config;
our $VERSION = do { my @r = ( q$Revision: 2.5 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

use strict;
use warnings;

our %ExtModule = (

    # Encode::Byte
    #iso-8859-1 is in Encode.pm itself
    'iso-8859-2'            => 'Encode::Byte',
    'iso-8859-3'            => 'Encode::Byte',
    'iso-8859-4'            => 'Encode::Byte',
    'iso-8859-5'            => 'Encode::Byte',
    'iso-8859-6'            => 'Encode::Byte',
    'iso-8859-7'            => 'Encode::Byte',
    'iso-8859-8'            => 'Encode::Byte',
    'iso-8859-9'            => 'Encode::Byte',
    'iso-8859-10'           => 'Encode::Byte',
    'iso-8859-11'           => 'Encode::Byte',
    'iso-8859-13'           => 'Encode::Byte',
    'iso-8859-14'           => 'Encode::Byte',
    'iso-8859-15'           => 'Encode::Byte',
    'iso-8859-16'           => 'Encode::Byte',
    'koi8-f'                => 'Encode::Byte',
    'koi8-r'                => 'Encode::Byte',
    'koi8-u'                => 'Encode::Byte',
    'viscii'                => 'Encode::Byte',
    'cp424'                 => 'Encode::Byte',
    'cp437'                 => 'Encode::Byte',
    'cp737'                 => 'Encode::Byte',
    'cp775'                 => 'Encode::Byte',
    'cp850'                 => 'Encode::Byte',
    'cp852'                 => 'Encode::Byte',
    'cp855'                 => 'Encode::Byte',
    'cp856'                 => 'Encode::Byte',
    'cp857'                 => 'Encode::Byte',
    'cp858'                 => 'Encode::Byte',
    'cp860'                 => 'Encode::Byte',
    'cp861'                 => 'Encode::Byte',
    'cp862'                 => 'Encode::Byte',
    'cp863'                 => 'Encode::Byte',
    'cp864'                 => 'Encode::Byte',
    'cp865'                 => 'Encode::Byte',
    'cp866'                 => 'Encode::Byte',
    'cp869'                 => 'Encode::Byte',
    'cp874'                 => 'Encode::Byte',
    'cp1006'                => 'Encode::Byte',
    'cp1250'                => 'Encode::Byte',
    'cp1251'                => 'Encode::Byte',
    'cp1252'                => 'Encode::Byte',
    'cp1253'                => 'Encode::Byte',
    'cp1254'                => 'Encode::Byte',
    'cp1255'                => 'Encode::Byte',
    'cp1256'                => 'Encode::Byte',
    'cp1257'                => 'Encode::Byte',
    'cp1258'                => 'Encode::Byte',
    'AdobeStandardEncoding' => 'Encode::Byte',
    'MacArabic'             => 'Encode::Byte',
    'MacCentralEurRoman'    => 'Encode::Byte',
    'MacCroatian'           => 'Encode::Byte',
    'MacCyrillic'           => 'Encode::Byte',
    'MacFarsi'              => 'Encode::Byte',
    'MacGreek'              => 'Encode::Byte',
    'MacHebrew'             => 'Encode::Byte',
    'MacIcelandic'          => 'Encode::Byte',
    'MacRoman'              => 'Encode::Byte',
    'MacRomanian'           => 'Encode::Byte',
    'MacRumanian'           => 'Encode::Byte',
    'MacSami'               => 'Encode::Byte',
    'MacThai'               => 'Encode::Byte',
    'MacTurkish'            => 'Encode::Byte',
    'MacUkrainian'          => 'Encode::Byte',
    'nextstep'              => 'Encode::Byte',
    'hp-roman8'             => 'Encode::Byte',
    #'gsm0338'               => 'Encode::Byte',
    'gsm0338'               => 'Encode::GSM0338',

    # Encode::EBCDIC
    'cp37'     => 'Encode::EBCDIC',
    'cp500'    => 'Encode::EBCDIC',
    'cp875'    => 'Encode::EBCDIC',
    'cp1026'   => 'Encode::EBCDIC',
    'cp1047'   => 'Encode::EBCDIC',
    'posix-bc' => 'Encode::EBCDIC',

    # Encode::Symbol
    'dingbats'      => 'Encode::Symbol',
    'symbol'        => 'Encode::Symbol',
    'AdobeSymbol'   => 'Encode::Symbol',
    'AdobeZdingbat' => 'Encode::Symbol',
    'MacDingbats'   => 'Encode::Symbol',
    'MacSymbol'     => 'Encode::Symbol',

    # Encode::Unicode
    'UCS-2BE'  => 'Encode::Unicode',
    'UCS-2LE'  => 'Encode::Unicode',
    'UTF-16'   => 'Encode::Unicode',
    'UTF-16BE' => 'Encode::Unicode',
    'UTF-16LE' => 'Encode::Unicode',
    'UTF-32'   => 'Encode::Unicode',
    'UTF-32BE' => 'Encode::Unicode',
    'UTF-32LE' => 'Encode::Unicode',
    'UTF-7'    => 'Encode::Unicode::UTF7',
);

unless ( ord("A") == 193 ) {
    %ExtModule = (
        %ExtModule,
        'euc-cn'         => 'Encode::CN',
        'gb12345-raw'    => 'Encode::CN',
        'gb2312-raw'     => 'Encode::CN',
        'hz'             => 'Encode::CN',
        'iso-ir-165'     => 'Encode::CN',
        'cp936'          => 'Encode::CN',
        'MacChineseSimp' => 'Encode::CN',

        '7bit-jis'      => 'Encode::JP',
        'euc-jp'        => 'Encode::JP',
        'iso-2022-jp'   => 'Encode::JP',
        'iso-2022-jp-1' => 'Encode::JP',
        'jis0201-raw'   => 'Encode::JP',
        'jis0208-raw'   => 'Encode::JP',
        'jis0212-raw'   => 'Encode::JP',
        'cp932'         => 'Encode::JP',
        'MacJapanese'   => 'Encode::JP',
        'shiftjis'      => 'Encode::JP',

        'euc-kr'      => 'Encode::KR',
        'iso-2022-kr' => 'Encode::KR',
        'johab'       => 'Encode::KR',
        'ksc5601-raw' => 'Encode::KR',
        'cp949'       => 'Encode::KR',
        'MacKorean'   => 'Encode::KR',

        'big5-eten'      => 'Encode::TW',
        'big5-hkscs'     => 'Encode::TW',
        'cp950'          => 'Encode::TW',
        'MacChineseTrad' => 'Encode::TW',

        #'big5plus'           => 'Encode::HanExtra',
        #'euc-tw'             => 'Encode::HanExtra',
        #'gb18030'            => 'Encode::HanExtra',

        'MIME-Header' => 'Encode::MIME::Header',
        'MIME-B'      => 'Encode::MIME::Header',
        'MIME-Q'      => 'Encode::MIME::Header',

        'MIME-Header-ISO_2022_JP' => 'Encode::MIME::Header::ISO_2022_JP',
    );
}

#
# Why not export ? to keep ConfigLocal Happy!
#
while ( my ( $enc, $mod ) = each %ExtModule ) {
    $Encode::ExtModule{$enc} = $mod;
}

1;
__END__

=head1 NAME

Encode::Config -- internally used by Encode

=cut
