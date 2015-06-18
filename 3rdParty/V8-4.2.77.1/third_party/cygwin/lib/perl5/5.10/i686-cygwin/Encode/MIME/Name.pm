package Encode::MIME::Name;
use strict;
use warnings;
our $VERSION = do { my @r = ( q$Revision: 1.1 $ =~ /\d+/g ); sprintf "%d." . "%02d" x $#r, @r };

our %MIME_NAME_OF = (
    'AdobeStandardEncoding' => 'Adobe-Standard-Encoding',
    'AdobeSymbol'           => 'Adobe-Symbol-Encoding',
    'ascii'                 => 'US-ASCII',
    'big5-hkscs'            => 'Big5-HKSCS',
    'cp1026'                => 'IBM1026',
    'cp1047'                => 'IBM1047',
    'cp1250'                => 'windows-1250',
    'cp1251'                => 'windows-1251',
    'cp1252'                => 'windows-1252',
    'cp1253'                => 'windows-1253',
    'cp1254'                => 'windows-1254',
    'cp1255'                => 'windows-1255',
    'cp1256'                => 'windows-1256',
    'cp1257'                => 'windows-1257',
    'cp1258'                => 'windows-1258',
    'cp37'                  => 'IBM037',
    'cp424'                 => 'IBM424',
    'cp437'                 => 'IBM437',
    'cp500'                 => 'IBM500',
    'cp775'                 => 'IBM775',
    'cp850'                 => 'IBM850',
    'cp852'                 => 'IBM852',
    'cp855'                 => 'IBM855',
    'cp857'                 => 'IBM857',
    'cp860'                 => 'IBM860',
    'cp861'                 => 'IBM861',
    'cp862'                 => 'IBM862',
    'cp863'                 => 'IBM863',
    'cp864'                 => 'IBM864',
    'cp865'                 => 'IBM865',
    'cp866'                 => 'IBM866',
    'cp869'                 => 'IBM869',
    'cp936'                 => 'GBK',
    'euc-jp'                => 'EUC-JP',
    'euc-kr'                => 'EUC-KR',
    #'gb2312-raw'            => 'GB2312', # no, you're wrong, I18N::Charset
    'hp-roman8'             => 'hp-roman8',
    'hz'                    => 'HZ-GB-2312',
    'iso-2022-jp'           => 'ISO-2022-JP',
    'iso-2022-jp-1'         => 'ISO-2022-JP',
    'iso-2022-kr'           => 'ISO-2022-KR',
    'iso-8859-1'            => 'ISO-8859-1',
    'iso-8859-10'           => 'ISO-8859-10',
    'iso-8859-13'           => 'ISO-8859-13',
    'iso-8859-14'           => 'ISO-8859-14',
    'iso-8859-15'           => 'ISO-8859-15',
    'iso-8859-16'           => 'ISO-8859-16',
    'iso-8859-2'            => 'ISO-8859-2',
    'iso-8859-3'            => 'ISO-8859-3',
    'iso-8859-4'            => 'ISO-8859-4',
    'iso-8859-5'            => 'ISO-8859-5',
    'iso-8859-6'            => 'ISO-8859-6',
    'iso-8859-7'            => 'ISO-8859-7',
    'iso-8859-8'            => 'ISO-8859-8',
    'iso-8859-9'            => 'ISO-8859-9',
    #'jis0201-raw'           => 'JIS_X0201',
    #'jis0208-raw'           => 'JIS_C6226-1983',
    #'jis0212-raw'           => 'JIS_X0212-1990',
    'koi8-r'                => 'KOI8-R',
    'koi8-u'                => 'KOI8-U',
    #'ksc5601-raw'           => 'KS_C_5601-1987',
    'shiftjis'              => 'Shift_JIS',
    'UTF-16'                => 'UTF-16',
    'UTF-16BE'              => 'UTF-16BE',
    'UTF-16LE'              => 'UTF-16LE',
    'UTF-32'                => 'UTF-32',
    'UTF-32BE'              => 'UTF-32BE',
    'UTF-32LE'              => 'UTF-32LE',
    'UTF-7'                 => 'UTF-7',
    'utf8'                  => 'UTF-8',
    'utf-8-strict'          => 'UTF-8',
    'viscii'                => 'VISCII',
);

sub get_mime_name($) { $MIME_NAME_OF{$_[0]} };

1;
__END__

=head1 NAME

Encode::MIME::NAME -- internally used by Encode

=head1 SEE ALSO

L<I18N::Charset>

=cut
