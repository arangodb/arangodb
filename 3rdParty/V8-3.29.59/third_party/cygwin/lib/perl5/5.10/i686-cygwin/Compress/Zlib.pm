
package Compress::Zlib;

require 5.004 ;
require Exporter;
use AutoLoader;
use Carp ;
use IO::Handle ;
use Scalar::Util qw(dualvar);

use IO::Compress::Base::Common 2.011 ;
use Compress::Raw::Zlib 2.011 ;
use IO::Compress::Gzip 2.011 ;
use IO::Uncompress::Gunzip 2.011 ;

use strict ;
use warnings ;
use bytes ;
our ($VERSION, $XS_VERSION, @ISA, @EXPORT, $AUTOLOAD);

$VERSION = '2.011';
$XS_VERSION = $VERSION; 
$VERSION = eval $VERSION;

@ISA = qw(Exporter);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
        deflateInit inflateInit

        compress uncompress

        gzopen $gzerrno
    );

push @EXPORT, @Compress::Raw::Zlib::EXPORT ;

BEGIN
{
    *zlib_version = \&Compress::Raw::Zlib::zlib_version;
}

sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my ($error, $val) = Compress::Raw::Zlib::constant($constname);
    Carp::croak $error if $error;
    no strict 'refs';
    *{$AUTOLOAD} = sub { $val };
    goto &{$AUTOLOAD};
}

use constant FLAG_APPEND             => 1 ;
use constant FLAG_CRC                => 2 ;
use constant FLAG_ADLER              => 4 ;
use constant FLAG_CONSUME_INPUT      => 8 ;

our (@my_z_errmsg);

@my_z_errmsg = (
    "need dictionary",     # Z_NEED_DICT     2
    "stream end",          # Z_STREAM_END    1
    "",                    # Z_OK            0
    "file error",          # Z_ERRNO        (-1)
    "stream error",        # Z_STREAM_ERROR (-2)
    "data error",          # Z_DATA_ERROR   (-3)
    "insufficient memory", # Z_MEM_ERROR    (-4)
    "buffer error",        # Z_BUF_ERROR    (-5)
    "incompatible version",# Z_VERSION_ERROR(-6)
    );


sub _set_gzerr
{
    my $value = shift ;

    if ($value == 0) {
        $Compress::Zlib::gzerrno = 0 ;
    }
    elsif ($value == Z_ERRNO() || $value > 2) {
        $Compress::Zlib::gzerrno = $! ;
    }
    else {
        $Compress::Zlib::gzerrno = dualvar($value+0, $my_z_errmsg[2 - $value]);
    }

    return $value ;
}

sub _save_gzerr
{
    my $gz = shift ;
    my $test_eof = shift ;

    my $value = $gz->errorNo() || 0 ;

    if ($test_eof) {
        #my $gz = $self->[0] ;
        # gzread uses Z_STREAM_END to denote a successful end
        $value = Z_STREAM_END() if $gz->eof() && $value == 0 ;
    }

    _set_gzerr($value) ;
}

sub gzopen($$)
{
    my ($file, $mode) = @_ ;

    my $gz ;
    my %defOpts = (Level    => Z_DEFAULT_COMPRESSION(),
                   Strategy => Z_DEFAULT_STRATEGY(),
                  );

    my $writing ;
    $writing = ! ($mode =~ /r/i) ;
    $writing = ($mode =~ /[wa]/i) ;

    $defOpts{Level}    = $1               if $mode =~ /(\d)/;
    $defOpts{Strategy} = Z_FILTERED()     if $mode =~ /f/i;
    $defOpts{Strategy} = Z_HUFFMAN_ONLY() if $mode =~ /h/i;
    $defOpts{Append}   = 1                if $mode =~ /a/i;

    my $infDef = $writing ? 'deflate' : 'inflate';
    my @params = () ;

    croak "gzopen: file parameter is not a filehandle or filename"
        unless isaFilehandle $file || isaFilename $file  || 
               (ref $file && ref $file eq 'SCALAR');

    return undef unless $mode =~ /[rwa]/i ;

    _set_gzerr(0) ;

    if ($writing) {
        $gz = new IO::Compress::Gzip($file, Minimal => 1, AutoClose => 1, 
                                     %defOpts) 
            or $Compress::Zlib::gzerrno = $IO::Compress::Gzip::GzipError;
    }
    else {
        $gz = new IO::Uncompress::Gunzip($file, 
                                         Transparent => 1,
                                         Append => 0, 
                                         AutoClose => 1, 
                                         MultiStream => 1,
                                         Strict => 0) 
            or $Compress::Zlib::gzerrno = $IO::Uncompress::Gunzip::GunzipError;
    }

    return undef
        if ! defined $gz ;

    bless [$gz, $infDef], 'Compress::Zlib::gzFile';
}

sub Compress::Zlib::gzFile::gzread
{
    my $self = shift ;

    return _set_gzerr(Z_STREAM_ERROR())
        if $self->[1] ne 'inflate';

    my $len = defined $_[1] ? $_[1] : 4096 ; 

    if ($self->gzeof() || $len == 0) {
        # Zap the output buffer to match ver 1 behaviour.
        $_[0] = "" ;
        return 0 ;
    }

    my $gz = $self->[0] ;
    my $status = $gz->read($_[0], $len) ; 
    _save_gzerr($gz, 1);
    return $status ;
}

sub Compress::Zlib::gzFile::gzreadline
{
    my $self = shift ;

    my $gz = $self->[0] ;
    {
        # Maintain backward compatibility with 1.x behaviour
        # It didn't support $/, so this can't either.
        local $/ = "\n" ;
        $_[0] = $gz->getline() ; 
    }
    _save_gzerr($gz, 1);
    return defined $_[0] ? length $_[0] : 0 ;
}

sub Compress::Zlib::gzFile::gzwrite
{
    my $self = shift ;
    my $gz = $self->[0] ;

    return _set_gzerr(Z_STREAM_ERROR())
        if $self->[1] ne 'deflate';

    $] >= 5.008 and (utf8::downgrade($_[0], 1) 
        or croak "Wide character in gzwrite");

    my $status = $gz->write($_[0]) ;
    _save_gzerr($gz);
    return $status ;
}

sub Compress::Zlib::gzFile::gztell
{
    my $self = shift ;
    my $gz = $self->[0] ;
    my $status = $gz->tell() ;
    _save_gzerr($gz);
    return $status ;
}

sub Compress::Zlib::gzFile::gzseek
{
    my $self   = shift ;
    my $offset = shift ;
    my $whence = shift ;

    my $gz = $self->[0] ;
    my $status ;
    eval { $status = $gz->seek($offset, $whence) ; };
    if ($@)
    {
        my $error = $@;
        $error =~ s/^.*: /gzseek: /;
        $error =~ s/ at .* line \d+\s*$//;
        croak $error;
    }
    _save_gzerr($gz);
    return $status ;
}

sub Compress::Zlib::gzFile::gzflush
{
    my $self = shift ;
    my $f    = shift ;

    my $gz = $self->[0] ;
    my $status = $gz->flush($f) ;
    my $err = _save_gzerr($gz);
    return $status ? 0 : $err;
}

sub Compress::Zlib::gzFile::gzclose
{
    my $self = shift ;
    my $gz = $self->[0] ;

    my $status = $gz->close() ;
    my $err = _save_gzerr($gz);
    return $status ? 0 : $err;
}

sub Compress::Zlib::gzFile::gzeof
{
    my $self = shift ;
    my $gz = $self->[0] ;

    return 0
        if $self->[1] ne 'inflate';

    my $status = $gz->eof() ;
    _save_gzerr($gz);
    return $status ;
}

sub Compress::Zlib::gzFile::gzsetparams
{
    my $self = shift ;
    croak "Usage: Compress::Zlib::gzFile::gzsetparams(file, level, strategy)"
        unless @_ eq 2 ;

    my $gz = $self->[0] ;
    my $level = shift ;
    my $strategy = shift;

    return _set_gzerr(Z_STREAM_ERROR())
        if $self->[1] ne 'deflate';
 
    my $status = *$gz->{Compress}->deflateParams(-Level   => $level, 
                                                -Strategy => $strategy);
    _save_gzerr($gz);
    return $status ;
}

sub Compress::Zlib::gzFile::gzerror
{
    my $self = shift ;
    my $gz = $self->[0] ;
    
    return $Compress::Zlib::gzerrno ;
}


sub compress($;$)
{
    my ($x, $output, $err, $in) =('', '', '', '') ;

    if (ref $_[0] ) {
        $in = $_[0] ;
        croak "not a scalar reference" unless ref $in eq 'SCALAR' ;
    }
    else {
        $in = \$_[0] ;
    }

    $] >= 5.008 and (utf8::downgrade($$in, 1) 
        or croak "Wide character in compress");

    my $level = (@_ == 2 ? $_[1] : Z_DEFAULT_COMPRESSION() );

    $x = new Compress::Raw::Zlib::Deflate -AppendOutput => 1, -Level => $level
            or return undef ;

    $err = $x->deflate($in, $output) ;
    return undef unless $err == Z_OK() ;

    $err = $x->flush($output) ;
    return undef unless $err == Z_OK() ;
    
    return $output ;

}

sub uncompress($)
{
    my ($x, $output, $err, $in) =('', '', '', '') ;

    if (ref $_[0] ) {
        $in = $_[0] ;
        croak "not a scalar reference" unless ref $in eq 'SCALAR' ;
    }
    else {
        $in = \$_[0] ;
    }

    $] >= 5.008 and (utf8::downgrade($$in, 1) 
        or croak "Wide character in uncompress");

    $x = new Compress::Raw::Zlib::Inflate -ConsumeInput => 0 or return undef ;
 
    $err = $x->inflate($in, $output) ;
    return undef unless $err == Z_STREAM_END() ;
 
    return $output ;
}


 
sub deflateInit(@)
{
    my ($got) = ParseParameters(0,
                {
                'Bufsize'       => [1, 1, Parse_unsigned, 4096],
                'Level'         => [1, 1, Parse_signed,   Z_DEFAULT_COMPRESSION()],
                'Method'        => [1, 1, Parse_unsigned, Z_DEFLATED()],
                'WindowBits'    => [1, 1, Parse_signed,   MAX_WBITS()],
                'MemLevel'      => [1, 1, Parse_unsigned, MAX_MEM_LEVEL()],
                'Strategy'      => [1, 1, Parse_unsigned, Z_DEFAULT_STRATEGY()],
                'Dictionary'    => [1, 1, Parse_any,      ""],
                }, @_ ) ;

    croak "Compress::Zlib::deflateInit: Bufsize must be >= 1, you specified " . 
            $got->value('Bufsize')
        unless $got->value('Bufsize') >= 1;

    my $obj ;
 
    my $status = 0 ;
    ($obj, $status) = 
      Compress::Raw::Zlib::_deflateInit(0,
                $got->value('Level'), 
                $got->value('Method'), 
                $got->value('WindowBits'), 
                $got->value('MemLevel'), 
                $got->value('Strategy'), 
                $got->value('Bufsize'),
                $got->value('Dictionary')) ;

    my $x = ($status == Z_OK() ? bless $obj, "Zlib::OldDeflate"  : undef) ;
    return wantarray ? ($x, $status) : $x ;
}
 
sub inflateInit(@)
{
    my ($got) = ParseParameters(0,
                {
                'Bufsize'       => [1, 1, Parse_unsigned, 4096],
                'WindowBits'    => [1, 1, Parse_signed,   MAX_WBITS()],
                'Dictionary'    => [1, 1, Parse_any,      ""],
                }, @_) ;


    croak "Compress::Zlib::inflateInit: Bufsize must be >= 1, you specified " . 
            $got->value('Bufsize')
        unless $got->value('Bufsize') >= 1;

    my $status = 0 ;
    my $obj ;
    ($obj, $status) = Compress::Raw::Zlib::_inflateInit(FLAG_CONSUME_INPUT,
                                $got->value('WindowBits'), 
                                $got->value('Bufsize'), 
                                $got->value('Dictionary')) ;

    my $x = ($status == Z_OK() ? bless $obj, "Zlib::OldInflate"  : undef) ;

    wantarray ? ($x, $status) : $x ;
}

package Zlib::OldDeflate ;

our (@ISA);
@ISA = qw(Compress::Raw::Zlib::deflateStream);


sub deflate
{
    my $self = shift ;
    my $output ;

    my $status = $self->SUPER::deflate($_[0], $output) ;
    wantarray ? ($output, $status) : $output ;
}

sub flush
{
    my $self = shift ;
    my $output ;
    my $flag = shift || Compress::Zlib::Z_FINISH();
    my $status = $self->SUPER::flush($output, $flag) ;
    
    wantarray ? ($output, $status) : $output ;
}

package Zlib::OldInflate ;

our (@ISA);
@ISA = qw(Compress::Raw::Zlib::inflateStream);

sub inflate
{
    my $self = shift ;
    my $output ;
    my $status = $self->SUPER::inflate($_[0], $output) ;
    wantarray ? ($output, $status) : $output ;
}

package Compress::Zlib ;

use IO::Compress::Gzip::Constants 2.011 ;

sub memGzip($)
{
  my $out;

  # if the deflation buffer isn't a reference, make it one
  my $string = (ref $_[0] ? $_[0] : \$_[0]) ;

  $] >= 5.008 and (utf8::downgrade($$string, 1) 
      or croak "Wide character in memGzip");

  IO::Compress::Gzip::gzip($string, \$out, Minimal => 1)
      or return undef ;

  return $out;
}


sub _removeGzipHeader($)
{
    my $string = shift ;

    return Z_DATA_ERROR() 
        if length($$string) < GZIP_MIN_HEADER_SIZE ;

    my ($magic1, $magic2, $method, $flags, $time, $xflags, $oscode) = 
        unpack ('CCCCVCC', $$string);

    return Z_DATA_ERROR()
        unless $magic1 == GZIP_ID1 and $magic2 == GZIP_ID2 and
           $method == Z_DEFLATED() and !($flags & GZIP_FLG_RESERVED) ;
    substr($$string, 0, GZIP_MIN_HEADER_SIZE) = '' ;

    # skip extra field
    if ($flags & GZIP_FLG_FEXTRA)
    {
        return Z_DATA_ERROR()
            if length($$string) < GZIP_FEXTRA_HEADER_SIZE ;

        my ($extra_len) = unpack ('v', $$string);
        $extra_len += GZIP_FEXTRA_HEADER_SIZE;
        return Z_DATA_ERROR()
            if length($$string) < $extra_len ;

        substr($$string, 0, $extra_len) = '';
    }

    # skip orig name
    if ($flags & GZIP_FLG_FNAME)
    {
        my $name_end = index ($$string, GZIP_NULL_BYTE);
        return Z_DATA_ERROR()
           if $name_end == -1 ;
        substr($$string, 0, $name_end + 1) =  '';
    }

    # skip comment
    if ($flags & GZIP_FLG_FCOMMENT)
    {
        my $comment_end = index ($$string, GZIP_NULL_BYTE);
        return Z_DATA_ERROR()
            if $comment_end == -1 ;
        substr($$string, 0, $comment_end + 1) = '';
    }

    # skip header crc
    if ($flags & GZIP_FLG_FHCRC)
    {
        return Z_DATA_ERROR()
            if length ($$string) < GZIP_FHCRC_SIZE ;
        substr($$string, 0, GZIP_FHCRC_SIZE) = '';
    }
    
    return Z_OK();
}


sub memGunzip($)
{
    # if the buffer isn't a reference, make it one
    my $string = (ref $_[0] ? $_[0] : \$_[0]);
 
    $] >= 5.008 and (utf8::downgrade($$string, 1) 
        or croak "Wide character in memGunzip");

    _removeGzipHeader($string) == Z_OK() 
        or return undef;
     
    my $bufsize = length $$string > 4096 ? length $$string : 4096 ;
    my $x = new Compress::Raw::Zlib::Inflate({-WindowBits => - MAX_WBITS(),
                         -Bufsize => $bufsize}) 

              or return undef;

    my $output = "" ;
    my $status = $x->inflate($string, $output);
    return undef 
        unless $status == Z_STREAM_END();

    if (length $$string >= 8)
    {
        my ($crc, $len) = unpack ("VV", substr($$string, 0, 8));
        substr($$string, 0, 8) = '';
        return undef 
            unless $len == length($output) and
                   $crc == crc32($output);
    }
    else
    {
        $$string = '';
    }
    return $output;   
}

# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__


=head1 NAME

Compress::Zlib - Interface to zlib compression library

=head1 SYNOPSIS

    use Compress::Zlib ;

    ($d, $status) = deflateInit( [OPT] ) ;
    $status = $d->deflate($input, $output) ;
    $status = $d->flush([$flush_type]) ;
    $d->deflateParams(OPTS) ;
    $d->deflateTune(OPTS) ;
    $d->dict_adler() ;
    $d->crc32() ;
    $d->adler32() ;
    $d->total_in() ;
    $d->total_out() ;
    $d->msg() ;
    $d->get_Strategy();
    $d->get_Level();
    $d->get_BufSize();

    ($i, $status) = inflateInit( [OPT] ) ;
    $status = $i->inflate($input, $output [, $eof]) ;
    $status = $i->inflateSync($input) ;
    $i->dict_adler() ;
    $d->crc32() ;
    $d->adler32() ;
    $i->total_in() ;
    $i->total_out() ;
    $i->msg() ;
    $d->get_BufSize();

    $dest = compress($source) ;
    $dest = uncompress($source) ;

    $gz = gzopen($filename or filehandle, $mode) ;
    $bytesread = $gz->gzread($buffer [,$size]) ;
    $bytesread = $gz->gzreadline($line) ;
    $byteswritten = $gz->gzwrite($buffer) ;
    $status = $gz->gzflush($flush) ;
    $offset = $gz->gztell() ;
    $status = $gz->gzseek($offset, $whence) ;
    $status = $gz->gzclose() ;
    $status = $gz->gzeof() ;
    $status = $gz->gzsetparams($level, $strategy) ;
    $errstring = $gz->gzerror() ; 
    $gzerrno

    $dest = Compress::Zlib::memGzip($buffer) ;
    $dest = Compress::Zlib::memGunzip($buffer) ;

    $crc = adler32($buffer [,$crc]) ;
    $crc = crc32($buffer [,$crc]) ;

    $crc = adler32_combine($crc1, $crc2, $len2)l
    $crc = crc32_combine($adler1, $adler2, $len2)

    ZLIB_VERSION
    ZLIB_VERNUM

=head1 DESCRIPTION

The I<Compress::Zlib> module provides a Perl interface to the I<zlib>
compression library (see L</AUTHOR> for details about where to get
I<zlib>). 

The C<Compress::Zlib> module can be split into two general areas of
functionality, namely a simple read/write interface to I<gzip> files
and a low-level in-memory compression/decompression interface.

Each of these areas will be discussed in the following sections.

=head2 Notes for users of Compress::Zlib version 1

The main change in C<Compress::Zlib> version 2.x is that it does not now
interface directly to the zlib library. Instead it uses the
C<IO::Compress::Gzip> and C<IO::Uncompress::Gunzip> modules for
reading/writing gzip files, and the C<Compress::Raw::Zlib> module for some
low-level zlib access. 

The interface provided by version 2 of this module should be 100% backward
compatible with version 1. If you find a difference in the expected
behaviour please contact the author (See L</AUTHOR>). See L<GZIP INTERFACE> 

With the creation of the C<IO::Compress> and C<IO::Uncompress> modules no
new features are planned for C<Compress::Zlib> - the new modules do
everything that C<Compress::Zlib> does and then some. Development on
C<Compress::Zlib> will be limited to bug fixes only.

If you are writing new code, your first port of call should be one of the
new C<IO::Compress> or C<IO::Uncompress> modules.

=head1 GZIP INTERFACE

A number of functions are supplied in I<zlib> for reading and writing
I<gzip> files that conform to RFC 1952. This module provides an interface
to most of them. 

If you have previously used C<Compress::Zlib> 1.x, the following
enhancements/changes have been made to the C<gzopen> interface:

=over 5

=item 1

If you want to to open either STDIN or STDOUT with C<gzopen>, you can now
optionally use the special filename "C<->" as a synonym for C<\*STDIN> and
C<\*STDOUT>.

=item 2 

In C<Compress::Zlib> version 1.x, C<gzopen> used the zlib library to open
the underlying file. This made things especially tricky when a Perl
filehandle was passed to C<gzopen>. Behind the scenes the numeric C file
descriptor had to be extracted from the Perl filehandle and this passed to
the zlib library.

Apart from being non-portable to some operating systems, this made it
difficult to use C<gzopen> in situations where you wanted to extract/create
a gzip data stream that is embedded in a larger file, without having to
resort to opening and closing the file multiple times. 

It also made it impossible to pass a perl filehandle that wasn't associated
with a real filesystem file, like, say, an C<IO::String>.

In C<Compress::Zlib> version 2.x, the C<gzopen> interface has been
completely rewritten to use the L<IO::Compress::Gzip|IO::Compress::Gzip>
for writing gzip files and L<IO::Uncompress::Gunzip|IO::Uncompress::Gunzip>
for reading gzip files. None of the limitations mentioned above apply.

=item 3

Addition of C<gzseek> to provide a restricted C<seek> interface.

=item 4.

Added C<gztell>.

=back

A more complete and flexible interface for reading/writing gzip
files/buffers is included with the module C<IO-Compress-Zlib>. See
L<IO::Compress::Gzip|IO::Compress::Gzip> and
L<IO::Uncompress::Gunzip|IO::Uncompress::Gunzip> for more details.

=over 5

=item B<$gz = gzopen($filename, $mode)>

=item B<$gz = gzopen($filehandle, $mode)>

This function opens either the I<gzip> file C<$filename> for reading or
writing or attaches to the opened filehandle, C<$filehandle>. 
It returns an object on success and C<undef> on failure.

When writing a gzip file this interface will I<always> create the smallest
possible gzip header (exactly 10 bytes). If you want greater control over
what gets stored in the gzip header (like the original filename or a
comment) use L<IO::Compress::Gzip|IO::Compress::Gzip> instead. Similarly if
you want to read the contents of the gzip header use
L<IO::Uncompress::Gunzip|IO::Uncompress::Gunzip>.

The second parameter, C<$mode>, is used to specify whether the file is
opened for reading or writing and to optionally specify a compression
level and compression strategy when writing. The format of the C<$mode>
parameter is similar to the mode parameter to the 'C' function C<fopen>,
so "rb" is used to open for reading, "wb" for writing and "ab" for
appending (writing at the end of the file).

To specify a compression level when writing, append a digit between 0
and 9 to the mode string -- 0 means no compression and 9 means maximum
compression.
If no compression level is specified Z_DEFAULT_COMPRESSION is used.

To specify the compression strategy when writing, append 'f' for filtered
data, 'h' for Huffman only compression, or 'R' for run-length encoding.
If no strategy is specified Z_DEFAULT_STRATEGY is used.

So, for example, "wb9" means open for writing with the maximum compression
using the default strategy and "wb4R" means open for writing with compression
level 4 and run-length encoding.

Refer to the I<zlib> documentation for the exact format of the C<$mode>
parameter.

=item B<$bytesread = $gz-E<gt>gzread($buffer [, $size]) ;>

Reads C<$size> bytes from the compressed file into C<$buffer>. If
C<$size> is not specified, it will default to 4096. If the scalar
C<$buffer> is not large enough, it will be extended automatically.

Returns the number of bytes actually read. On EOF it returns 0 and in
the case of an error, -1.

=item B<$bytesread = $gz-E<gt>gzreadline($line) ;>

Reads the next line from the compressed file into C<$line>. 

Returns the number of bytes actually read. On EOF it returns 0 and in
the case of an error, -1.

It is legal to intermix calls to C<gzread> and C<gzreadline>.

To maintain backward compatibility with version 1.x of this module
C<gzreadline> ignores the C<$/> variable - it I<always> uses the string
C<"\n"> as the line delimiter.  

If you want to read a gzip file a line at a time and have it respect the
C<$/> variable (or C<$INPUT_RECORD_SEPARATOR>, or C<$RS> when C<English> is
in use) see L<IO::Uncompress::Gunzip|IO::Uncompress::Gunzip>.

=item B<$byteswritten = $gz-E<gt>gzwrite($buffer) ;>

Writes the contents of C<$buffer> to the compressed file. Returns the
number of bytes actually written, or 0 on error.

=item B<$status = $gz-E<gt>gzflush($flush_type) ;>

Flushes all pending output into the compressed file.

This method takes an optional parameter, C<$flush_type>, that controls
how the flushing will be carried out. By default the C<$flush_type>
used is C<Z_FINISH>. Other valid values for C<$flush_type> are
C<Z_NO_FLUSH>, C<Z_SYNC_FLUSH>, C<Z_FULL_FLUSH> and C<Z_BLOCK>. It is
strongly recommended that you only set the C<flush_type> parameter if
you fully understand the implications of what it does - overuse of C<flush>
can seriously degrade the level of compression achieved. See the C<zlib>
documentation for details.

Returns 0 on success.

=item B<$offset = $gz-E<gt>gztell() ;>

Returns the uncompressed file offset.

=item B<$status = $gz-E<gt>gzseek($offset, $whence) ;>

Provides a sub-set of the C<seek> functionality, with the restriction
that it is only legal to seek forward in the compressed file.
It is a fatal error to attempt to seek backward.

When opened for writing, empty parts of the file will have NULL (0x00)
bytes written to them.

The C<$whence> parameter should be one of SEEK_SET, SEEK_CUR or SEEK_END.

Returns 1 on success, 0 on failure.

=item B<$gz-E<gt>gzclose>

Closes the compressed file. Any pending data is flushed to the file
before it is closed.

Returns 0 on success.

=item B<$gz-E<gt>gzsetparams($level, $strategy>

Change settings for the deflate stream C<$gz>.

The list of the valid options is shown below. Options not specified
will remain unchanged.

Note: This method is only available if you are running zlib 1.0.6 or better.

=over 5

=item B<$level>

Defines the compression level. Valid values are 0 through 9,
C<Z_NO_COMPRESSION>, C<Z_BEST_SPEED>, C<Z_BEST_COMPRESSION>, and
C<Z_DEFAULT_COMPRESSION>.

=item B<$strategy>

Defines the strategy used to tune the compression. The valid values are
C<Z_DEFAULT_STRATEGY>, C<Z_FILTERED> and C<Z_HUFFMAN_ONLY>. 

=back

=item B<$gz-E<gt>gzerror>

Returns the I<zlib> error message or number for the last operation
associated with C<$gz>. The return value will be the I<zlib> error
number when used in a numeric context and the I<zlib> error message
when used in a string context. The I<zlib> error number constants,
shown below, are available for use.

    Z_OK
    Z_STREAM_END
    Z_ERRNO
    Z_STREAM_ERROR
    Z_DATA_ERROR
    Z_MEM_ERROR
    Z_BUF_ERROR

=item B<$gzerrno>

The C<$gzerrno> scalar holds the error code associated with the most
recent I<gzip> routine. Note that unlike C<gzerror()>, the error is
I<not> associated with a particular file.

As with C<gzerror()> it returns an error number in numeric context and
an error message in string context. Unlike C<gzerror()> though, the
error message will correspond to the I<zlib> message when the error is
associated with I<zlib> itself, or the UNIX error message when it is
not (i.e. I<zlib> returned C<Z_ERRORNO>).

As there is an overlap between the error numbers used by I<zlib> and
UNIX, C<$gzerrno> should only be used to check for the presence of
I<an> error in numeric context. Use C<gzerror()> to check for specific
I<zlib> errors. The I<gzcat> example below shows how the variable can
be used safely.

=back

=head2 Examples

Here is an example script which uses the interface. It implements a
I<gzcat> function.

    use strict ;
    use warnings ;
    
    use Compress::Zlib ;
    
    # use stdin if no files supplied
    @ARGV = '-' unless @ARGV ;
    
    foreach my $file (@ARGV) {
        my $buffer ;
    
        my $gz = gzopen($file, "rb") 
             or die "Cannot open $file: $gzerrno\n" ;
    
        print $buffer while $gz->gzread($buffer) > 0 ;
    
        die "Error reading from $file: $gzerrno" . ($gzerrno+0) . "\n" 
            if $gzerrno != Z_STREAM_END ;
        
        $gz->gzclose() ;
    }

Below is a script which makes use of C<gzreadline>. It implements a
very simple I<grep> like script.

    use strict ;
    use warnings ;
    
    use Compress::Zlib ;
    
    die "Usage: gzgrep pattern [file...]\n"
        unless @ARGV >= 1;
    
    my $pattern = shift ;
    
    # use stdin if no files supplied
    @ARGV = '-' unless @ARGV ;
    
    foreach my $file (@ARGV) {
        my $gz = gzopen($file, "rb") 
             or die "Cannot open $file: $gzerrno\n" ;
    
        while ($gz->gzreadline($_) > 0) {
            print if /$pattern/ ;
        }
    
        die "Error reading from $file: $gzerrno\n" 
            if $gzerrno != Z_STREAM_END ;
        
        $gz->gzclose() ;
    }

This script, I<gzstream>, does the opposite of the I<gzcat> script
above. It reads from standard input and writes a gzip data stream to
standard output.

    use strict ;
    use warnings ;
    
    use Compress::Zlib ;
    
    binmode STDOUT;  # gzopen only sets it on the fd
    
    my $gz = gzopen(\*STDOUT, "wb")
          or die "Cannot open stdout: $gzerrno\n" ;
    
    while (<>) {
        $gz->gzwrite($_) 
          or die "error writing: $gzerrno\n" ;
    }

    $gz->gzclose ;

=head2 Compress::Zlib::memGzip

This function is used to create an in-memory gzip file with the minimum
possible gzip header (exactly 10 bytes).

    $dest = Compress::Zlib::memGzip($buffer) ;

If successful, it returns the in-memory gzip file, otherwise it returns
undef.

The C<$buffer> parameter can either be a scalar or a scalar reference.

See L<IO::Compress::Gzip|IO::Compress::Gzip> for an alternative way to
carry out in-memory gzip compression.

=head2 Compress::Zlib::memGunzip

This function is used to uncompress an in-memory gzip file.

    $dest = Compress::Zlib::memGunzip($buffer) ;

If successful, it returns the uncompressed gzip file, otherwise it
returns undef.

The C<$buffer> parameter can either be a scalar or a scalar reference. The
contents of the C<$buffer> parameter are destroyed after calling this function.

See L<IO::Uncompress::Gunzip|IO::Uncompress::Gunzip> for an alternative way
to carry out in-memory gzip uncompression.

=head1 COMPRESS/UNCOMPRESS

Two functions are provided to perform in-memory compression/uncompression of
RFC 1950 data streams. They are called C<compress> and C<uncompress>.

=over 5

=item B<$dest = compress($source [, $level] ) ;>

Compresses C<$source>. If successful it returns the compressed
data. Otherwise it returns I<undef>.

The source buffer, C<$source>, can either be a scalar or a scalar
reference.

The C<$level> parameter defines the compression level. Valid values are
0 through 9, C<Z_NO_COMPRESSION>, C<Z_BEST_SPEED>,
C<Z_BEST_COMPRESSION>, and C<Z_DEFAULT_COMPRESSION>.
If C<$level> is not specified C<Z_DEFAULT_COMPRESSION> will be used.

=item B<$dest = uncompress($source) ;>

Uncompresses C<$source>. If successful it returns the uncompressed
data. Otherwise it returns I<undef>.

The source buffer can either be a scalar or a scalar reference.

=back

Please note: the two functions defined above are I<not> compatible with
the Unix commands of the same name.

See L<IO::Deflate|IO::Deflate> and L<IO::Inflate|IO::Inflate> included with
this distribution for an alternative interface for reading/writing RFC 1950
files/buffers.

=head1 Deflate Interface

This section defines an interface that allows in-memory compression using
the I<deflate> interface provided by zlib.

Here is a definition of the interface available:

=head2 B<($d, $status) = deflateInit( [OPT] )>

Initialises a deflation stream. 

It combines the features of the I<zlib> functions C<deflateInit>,
C<deflateInit2> and C<deflateSetDictionary>.

If successful, it will return the initialised deflation stream, C<$d>
and C<$status> of C<Z_OK> in a list context. In scalar context it
returns the deflation stream, C<$d>, only.

If not successful, the returned deflation stream (C<$d>) will be
I<undef> and C<$status> will hold the exact I<zlib> error code.

The function optionally takes a number of named options specified as
C<< -Name=>value >> pairs. This allows individual options to be
tailored without having to specify them all in the parameter list.

For backward compatibility, it is also possible to pass the parameters
as a reference to a hash containing the name=>value pairs.

The function takes one optional parameter, a reference to a hash.  The
contents of the hash allow the deflation interface to be tailored.

Here is a list of the valid options:

=over 5

=item B<-Level>

Defines the compression level. Valid values are 0 through 9,
C<Z_NO_COMPRESSION>, C<Z_BEST_SPEED>, C<Z_BEST_COMPRESSION>, and
C<Z_DEFAULT_COMPRESSION>.

The default is Z_DEFAULT_COMPRESSION.

=item B<-Method>

Defines the compression method. The only valid value at present (and
the default) is Z_DEFLATED.

=item B<-WindowBits>

To create an RFC 1950 data stream, set C<WindowBits> to a positive number.

To create an RFC 1951 data stream, set C<WindowBits> to C<-MAX_WBITS>.

For a full definition of the meaning and valid values for C<WindowBits> refer
to the I<zlib> documentation for I<deflateInit2>.

Defaults to MAX_WBITS.

=item B<-MemLevel>

For a definition of the meaning and valid values for C<MemLevel>
refer to the I<zlib> documentation for I<deflateInit2>.

Defaults to MAX_MEM_LEVEL.

=item B<-Strategy>

Defines the strategy used to tune the compression. The valid values are
C<Z_DEFAULT_STRATEGY>, C<Z_FILTERED> and C<Z_HUFFMAN_ONLY>. 

The default is Z_DEFAULT_STRATEGY.

=item B<-Dictionary>

When a dictionary is specified I<Compress::Zlib> will automatically
call C<deflateSetDictionary> directly after calling C<deflateInit>. The
Adler32 value for the dictionary can be obtained by calling the method 
C<$d->dict_adler()>.

The default is no dictionary.

=item B<-Bufsize>

Sets the initial size for the deflation buffer. If the buffer has to be
reallocated to increase the size, it will grow in increments of
C<Bufsize>.

The default is 4096.

=back

Here is an example of using the C<deflateInit> optional parameter list
to override the default buffer size and compression level. All other
options will take their default values.

    deflateInit( -Bufsize => 300, 
                 -Level => Z_BEST_SPEED  ) ;

=head2 B<($out, $status) = $d-E<gt>deflate($buffer)>

Deflates the contents of C<$buffer>. The buffer can either be a scalar
or a scalar reference.  When finished, C<$buffer> will be
completely processed (assuming there were no errors). If the deflation
was successful it returns the deflated output, C<$out>, and a status
value, C<$status>, of C<Z_OK>.

On error, C<$out> will be I<undef> and C<$status> will contain the
I<zlib> error code.

In a scalar context C<deflate> will return C<$out> only.

As with the I<deflate> function in I<zlib>, it is not necessarily the
case that any output will be produced by this method. So don't rely on
the fact that C<$out> is empty for an error test.

=head2 B<($out, $status) = $d-E<gt>flush()>
=head2 B<($out, $status) = $d-E<gt>flush($flush_type)>

Typically used to finish the deflation. Any pending output will be
returned via C<$out>.
C<$status> will have a value C<Z_OK> if successful.

In a scalar context C<flush> will return C<$out> only.

Note that flushing can seriously degrade the compression ratio, so it
should only be used to terminate a decompression (using C<Z_FINISH>) or
when you want to create a I<full flush point> (using C<Z_FULL_FLUSH>).

By default the C<flush_type> used is C<Z_FINISH>. Other valid values
for C<flush_type> are C<Z_NO_FLUSH>, C<Z_PARTIAL_FLUSH>, C<Z_SYNC_FLUSH>
and C<Z_FULL_FLUSH>. It is strongly recommended that you only set the
C<flush_type> parameter if you fully understand the implications of
what it does. See the C<zlib> documentation for details.

=head2 B<$status = $d-E<gt>deflateParams([OPT])>

Change settings for the deflate stream C<$d>.

The list of the valid options is shown below. Options not specified
will remain unchanged.

=over 5

=item B<-Level>

Defines the compression level. Valid values are 0 through 9,
C<Z_NO_COMPRESSION>, C<Z_BEST_SPEED>, C<Z_BEST_COMPRESSION>, and
C<Z_DEFAULT_COMPRESSION>.

=item B<-Strategy>

Defines the strategy used to tune the compression. The valid values are
C<Z_DEFAULT_STRATEGY>, C<Z_FILTERED> and C<Z_HUFFMAN_ONLY>. 

=back

=head2 B<$d-E<gt>dict_adler()>

Returns the adler32 value for the dictionary.

=head2 B<$d-E<gt>msg()>

Returns the last error message generated by zlib.

=head2 B<$d-E<gt>total_in()>

Returns the total number of bytes uncompressed bytes input to deflate.

=head2 B<$d-E<gt>total_out()>

Returns the total number of compressed bytes output from deflate.

=head2 Example

Here is a trivial example of using C<deflate>. It simply reads standard
input, deflates it and writes it to standard output.

    use strict ;
    use warnings ;

    use Compress::Zlib ;

    binmode STDIN;
    binmode STDOUT;
    my $x = deflateInit()
       or die "Cannot create a deflation stream\n" ;

    my ($output, $status) ;
    while (<>)
    {
        ($output, $status) = $x->deflate($_) ;
    
        $status == Z_OK
            or die "deflation failed\n" ;
    
        print $output ;
    }
    
    ($output, $status) = $x->flush() ;
    
    $status == Z_OK
        or die "deflation failed\n" ;
    
    print $output ;

=head1 Inflate Interface

This section defines the interface available that allows in-memory
uncompression using the I<deflate> interface provided by zlib.

Here is a definition of the interface:

=head2 B<($i, $status) = inflateInit()>

Initialises an inflation stream. 

In a list context it returns the inflation stream, C<$i>, and the
I<zlib> status code in C<$status>. In a scalar context it returns the
inflation stream only.

If successful, C<$i> will hold the inflation stream and C<$status> will
be C<Z_OK>.

If not successful, C<$i> will be I<undef> and C<$status> will hold the
I<zlib> error code.

The function optionally takes a number of named options specified as
C<< -Name=>value >> pairs. This allows individual options to be
tailored without having to specify them all in the parameter list.
 
For backward compatibility, it is also possible to pass the parameters
as a reference to a hash containing the name=>value pairs.
 
The function takes one optional parameter, a reference to a hash.  The
contents of the hash allow the deflation interface to be tailored.
 
Here is a list of the valid options:

=over 5

=item B<-WindowBits>

To uncompress an RFC 1950 data stream, set C<WindowBits> to a positive number.

To uncompress an RFC 1951 data stream, set C<WindowBits> to C<-MAX_WBITS>.

For a full definition of the meaning and valid values for C<WindowBits> refer
to the I<zlib> documentation for I<inflateInit2>.

Defaults to MAX_WBITS.

=item B<-Bufsize>

Sets the initial size for the inflation buffer. If the buffer has to be
reallocated to increase the size, it will grow in increments of
C<Bufsize>. 

Default is 4096.

=item B<-Dictionary>

The default is no dictionary.

=back

Here is an example of using the C<inflateInit> optional parameter to
override the default buffer size.

    inflateInit( -Bufsize => 300 ) ;

=head2 B<($out, $status) = $i-E<gt>inflate($buffer)>

Inflates the complete contents of C<$buffer>. The buffer can either be
a scalar or a scalar reference.

Returns C<Z_OK> if successful and C<Z_STREAM_END> if the end of the
compressed data has been successfully reached. 
If not successful, C<$out> will be I<undef> and C<$status> will hold
the I<zlib> error code.

The C<$buffer> parameter is modified by C<inflate>. On completion it
will contain what remains of the input buffer after inflation. This
means that C<$buffer> will be an empty string when the return status is
C<Z_OK>. When the return status is C<Z_STREAM_END> the C<$buffer>
parameter will contains what (if anything) was stored in the input
buffer after the deflated data stream.

This feature is useful when processing a file format that encapsulates
a  compressed data stream (e.g. gzip, zip).

=head2 B<$status = $i-E<gt>inflateSync($buffer)>

Scans C<$buffer> until it reaches either a I<full flush point> or the
end of the buffer.

If a I<full flush point> is found, C<Z_OK> is returned and C<$buffer>
will be have all data up to the flush point removed. This can then be
passed to the C<deflate> method.

Any other return code means that a flush point was not found. If more
data is available, C<inflateSync> can be called repeatedly with more
compressed data until the flush point is found.

=head2 B<$i-E<gt>dict_adler()>

Returns the adler32 value for the dictionary.

=head2 B<$i-E<gt>msg()>

Returns the last error message generated by zlib.

=head2 B<$i-E<gt>total_in()>

Returns the total number of bytes compressed bytes input to inflate.

=head2 B<$i-E<gt>total_out()>

Returns the total number of uncompressed bytes output from inflate.

=head2 Example

Here is an example of using C<inflate>.

    use strict ;
    use warnings ;
    
    use Compress::Zlib ;
    
    my $x = inflateInit()
       or die "Cannot create a inflation stream\n" ;
    
    my $input = '' ;
    binmode STDIN;
    binmode STDOUT;
    
    my ($output, $status) ;
    while (read(STDIN, $input, 4096))
    {
        ($output, $status) = $x->inflate(\$input) ;
    
        print $output 
            if $status == Z_OK or $status == Z_STREAM_END ;
    
        last if $status != Z_OK ;
    }
    
    die "inflation failed\n"
        unless $status == Z_STREAM_END ;

=head1 CHECKSUM FUNCTIONS

Two functions are provided by I<zlib> to calculate checksums. For the
Perl interface, the order of the two parameters in both functions has
been reversed. This allows both running checksums and one off
calculations to be done.

    $crc = adler32($buffer [,$crc]) ;
    $crc = crc32($buffer [,$crc]) ;

The buffer parameters can either be a scalar or a scalar reference.

If the $crc parameters is C<undef>, the crc value will be reset.

If you have built this module with zlib 1.2.3 or better, two more
CRC-related functions are available.

    $crc = adler32_combine($crc1, $crc2, $len2)l
    $crc = crc32_combine($adler1, $adler2, $len2)

These functions allow checksums to be merged.

=head1 CONSTANTS

All the I<zlib> constants are automatically imported when you make use
of I<Compress::Zlib>.

=head1 SEE ALSO

L<IO::Compress::Gzip>, L<IO::Uncompress::Gunzip>, L<IO::Compress::Deflate>, L<IO::Uncompress::Inflate>, L<IO::Compress::RawDeflate>, L<IO::Uncompress::RawInflate>, L<IO::Compress::Bzip2>, L<IO::Uncompress::Bunzip2>, L<IO::Compress::Lzop>, L<IO::Uncompress::UnLzop>, L<IO::Compress::Lzf>, L<IO::Uncompress::UnLzf>, L<IO::Uncompress::AnyInflate>, L<IO::Uncompress::AnyUncompress>

L<Compress::Zlib::FAQ|Compress::Zlib::FAQ>

L<File::GlobMapper|File::GlobMapper>, L<Archive::Zip|Archive::Zip>,
L<Archive::Tar|Archive::Tar>,
L<IO::Zlib|IO::Zlib>

For RFC 1950, 1951 and 1952 see 
F<http://www.faqs.org/rfcs/rfc1950.html>,
F<http://www.faqs.org/rfcs/rfc1951.html> and
F<http://www.faqs.org/rfcs/rfc1952.html>

The I<zlib> compression library was written by Jean-loup Gailly
F<gzip@prep.ai.mit.edu> and Mark Adler F<madler@alumni.caltech.edu>.

The primary site for the I<zlib> compression library is
F<http://www.zlib.org>.

The primary site for gzip is F<http://www.gzip.org>.

=head1 AUTHOR

This module was written by Paul Marquess, F<pmqs@cpan.org>. 

=head1 MODIFICATION HISTORY

See the Changes file.

=head1 COPYRIGHT AND LICENSE

Copyright (c) 1995-2008 Paul Marquess. All rights reserved.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

