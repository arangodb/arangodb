package IO::Uncompress::Inflate ;
# for RFC1950

use strict ;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.011 qw(:Status createSelfTiedObject);
use IO::Compress::Zlib::Constants 2.011 ;

use IO::Uncompress::RawInflate  2.011 ;

require Exporter ;
our ($VERSION, @ISA, @EXPORT_OK, %EXPORT_TAGS, $InflateError);

$VERSION = '2.011';
$InflateError = '';

@ISA    = qw( Exporter IO::Uncompress::RawInflate );
@EXPORT_OK = qw( $InflateError inflate ) ;
%EXPORT_TAGS = %IO::Uncompress::RawInflate::DEFLATE_CONSTANTS ;
push @{ $EXPORT_TAGS{all} }, @EXPORT_OK ;
Exporter::export_ok_tags('all');


sub new
{
    my $class = shift ;
    my $obj = createSelfTiedObject($class, \$InflateError);

    $obj->_create(undef, 0, @_);
}

sub inflate
{
    my $obj = createSelfTiedObject(undef, \$InflateError);
    return $obj->_inf(@_);
}

sub getExtraParams
{
    return ();
}

sub ckParams
{
    my $self = shift ;
    my $got = shift ;

    # gunzip always needs adler32
    $got->value('ADLER32' => 1);

    return 1;
}

sub ckMagic
{
    my $self = shift;

    my $magic ;
    $self->smartReadExact(\$magic, ZLIB_HEADER_SIZE);

    *$self->{HeaderPending} = $magic ;

    return $self->HeaderError("Header size is " . 
                                        ZLIB_HEADER_SIZE . " bytes") 
        if length $magic != ZLIB_HEADER_SIZE;

    #return $self->HeaderError("CRC mismatch.")
    return undef
        if ! $self->isZlibMagic($magic) ;
                      
    *$self->{Type} = 'rfc1950';
    return $magic;
}

sub readHeader
{
    my $self = shift;
    my $magic = shift ;

    return $self->_readDeflateHeader($magic) ;
}

sub chkTrailer
{
    my $self = shift;
    my $trailer = shift;

    my $ADLER32 = unpack("N", $trailer) ;
    *$self->{Info}{ADLER32} = $ADLER32;    
    return $self->TrailerError("CRC mismatch")
        if *$self->{Strict} && $ADLER32 != *$self->{Uncomp}->adler32() ;

    return STATUS_OK;
}



sub isZlibMagic
{
    my $self = shift;
    my $buffer = shift ;

    return 0 
        if length $buffer < ZLIB_HEADER_SIZE ;

    my $hdr = unpack("n", $buffer) ;
    #return 0 if $hdr % 31 != 0 ;
    return $self->HeaderError("CRC mismatch.")
        if $hdr % 31 != 0 ;

    my ($CMF, $FLG) = unpack "C C", $buffer;
    my $cm =    bits($CMF, ZLIB_CMF_CM_OFFSET,    ZLIB_CMF_CM_BITS) ;

    # Only Deflate supported
    return $self->HeaderError("Not Deflate (CM is $cm)") 
        if $cm != ZLIB_CMF_CM_DEFLATED ;

    # Max window value is 7 for Deflate.
    my $cinfo = bits($CMF, ZLIB_CMF_CINFO_OFFSET, ZLIB_CMF_CINFO_BITS) ;
    return $self->HeaderError("CINFO > " . ZLIB_CMF_CINFO_MAX . 
                              " (CINFO is $cinfo)") 
        if $cinfo > ZLIB_CMF_CINFO_MAX ;

    return 1;    
}

sub bits
{
    my $data   = shift ;
    my $offset = shift ;
    my $mask  = shift ;

    ($data >> $offset ) & $mask & 0xFF ;
}


sub _readDeflateHeader
{
    my ($self, $buffer) = @_ ;

#    if (! $buffer) {
#        $self->smartReadExact(\$buffer, ZLIB_HEADER_SIZE);
#
#        *$self->{HeaderPending} = $buffer ;
#
#        return $self->HeaderError("Header size is " . 
#                                            ZLIB_HEADER_SIZE . " bytes") 
#            if length $buffer != ZLIB_HEADER_SIZE;
#
#        return $self->HeaderError("CRC mismatch.")
#            if ! isZlibMagic($buffer) ;
#    }
                                        
    my ($CMF, $FLG) = unpack "C C", $buffer;
    my $FDICT = bits($FLG, ZLIB_FLG_FDICT_OFFSET,  ZLIB_FLG_FDICT_BITS ),

    my $cm = bits($CMF, ZLIB_CMF_CM_OFFSET, ZLIB_CMF_CM_BITS) ;
    $cm == ZLIB_CMF_CM_DEFLATED 
        or return $self->HeaderError("Not Deflate (CM is $cm)") ;

    my $DICTID;
    if ($FDICT) {
        $self->smartReadExact(\$buffer, ZLIB_FDICT_SIZE)
            or return $self->TruncatedHeader("FDICT");

        $DICTID = unpack("N", $buffer) ;
    }

    *$self->{Type} = 'rfc1950';

    return {
        'Type'          => 'rfc1950',
        'FingerprintLength'  => ZLIB_HEADER_SIZE,
        'HeaderLength'  => ZLIB_HEADER_SIZE,
        'TrailerLength' => ZLIB_TRAILER_SIZE,
        'Header'        => $buffer,

        CMF     =>      $CMF                                               ,
        CM      => bits($CMF, ZLIB_CMF_CM_OFFSET,     ZLIB_CMF_CM_BITS    ),
        CINFO   => bits($CMF, ZLIB_CMF_CINFO_OFFSET,  ZLIB_CMF_CINFO_BITS ),
        FLG     =>      $FLG                                               ,
        FCHECK  => bits($FLG, ZLIB_FLG_FCHECK_OFFSET, ZLIB_FLG_FCHECK_BITS),
        FDICT   => bits($FLG, ZLIB_FLG_FDICT_OFFSET,  ZLIB_FLG_FDICT_BITS ),
        FLEVEL  => bits($FLG, ZLIB_FLG_LEVEL_OFFSET,  ZLIB_FLG_LEVEL_BITS ),
        DICTID  =>      $DICTID                                            ,

    };
}




1 ;

__END__


=head1 NAME

IO::Uncompress::Inflate - Read RFC 1950 files/buffers

=head1 SYNOPSIS

    use IO::Uncompress::Inflate qw(inflate $InflateError) ;

    my $status = inflate $input => $output [,OPTS]
        or die "inflate failed: $InflateError\n";

    my $z = new IO::Uncompress::Inflate $input [OPTS] 
        or die "inflate failed: $InflateError\n";

    $status = $z->read($buffer)
    $status = $z->read($buffer, $length)
    $status = $z->read($buffer, $length, $offset)
    $line = $z->getline()
    $char = $z->getc()
    $char = $z->ungetc()
    $char = $z->opened()

    $status = $z->inflateSync()

    $data = $z->trailingData()
    $status = $z->nextStream()
    $data = $z->getHeaderInfo()
    $z->tell()
    $z->seek($position, $whence)
    $z->binmode()
    $z->fileno()
    $z->eof()
    $z->close()

    $InflateError ;

    # IO::File mode

    <$z>
    read($z, $buffer);
    read($z, $buffer, $length);
    read($z, $buffer, $length, $offset);
    tell($z)
    seek($z, $position, $whence)
    binmode($z)
    fileno($z)
    eof($z)
    close($z)

=head1 DESCRIPTION

This module provides a Perl interface that allows the reading of
files/buffers that conform to RFC 1950.

For writing RFC 1950 files/buffers, see the companion module IO::Compress::Deflate.

=head1 Functional Interface

A top-level function, C<inflate>, is provided to carry out
"one-shot" uncompression between buffers and/or files. For finer
control over the uncompression process, see the L</"OO Interface">
section.

    use IO::Uncompress::Inflate qw(inflate $InflateError) ;

    inflate $input => $output [,OPTS] 
        or die "inflate failed: $InflateError\n";

The functional interface needs Perl5.005 or better.

=head2 inflate $input => $output [, OPTS]

C<inflate> expects at least two parameters, C<$input> and C<$output>.

=head3 The C<$input> parameter

The parameter, C<$input>, is used to define the source of
the compressed data. 

It can take one of the following forms:

=over 5

=item A filename

If the C<$input> parameter is a simple scalar, it is assumed to be a
filename. This file will be opened for reading and the input data
will be read from it.

=item A filehandle

If the C<$input> parameter is a filehandle, the input data will be
read from it.
The string '-' can be used as an alias for standard input.

=item A scalar reference 

If C<$input> is a scalar reference, the input data will be read
from C<$$input>.

=item An array reference 

If C<$input> is an array reference, each element in the array must be a
filename.

The input data will be read from each file in turn. 

The complete array will be walked to ensure that it only
contains valid filenames before any data is uncompressed.

=item An Input FileGlob string

If C<$input> is a string that is delimited by the characters "<" and ">"
C<inflate> will assume that it is an I<input fileglob string>. The
input is the list of files that match the fileglob.

If the fileglob does not match any files ...

See L<File::GlobMapper|File::GlobMapper> for more details.

=back

If the C<$input> parameter is any other type, C<undef> will be returned.

=head3 The C<$output> parameter

The parameter C<$output> is used to control the destination of the
uncompressed data. This parameter can take one of these forms.

=over 5

=item A filename

If the C<$output> parameter is a simple scalar, it is assumed to be a
filename.  This file will be opened for writing and the uncompressed
data will be written to it.

=item A filehandle

If the C<$output> parameter is a filehandle, the uncompressed data
will be written to it.
The string '-' can be used as an alias for standard output.

=item A scalar reference 

If C<$output> is a scalar reference, the uncompressed data will be
stored in C<$$output>.

=item An Array Reference

If C<$output> is an array reference, the uncompressed data will be
pushed onto the array.

=item An Output FileGlob

If C<$output> is a string that is delimited by the characters "<" and ">"
C<inflate> will assume that it is an I<output fileglob string>. The
output is the list of files that match the fileglob.

When C<$output> is an fileglob string, C<$input> must also be a fileglob
string. Anything else is an error.

=back

If the C<$output> parameter is any other type, C<undef> will be returned.

=head2 Notes

When C<$input> maps to multiple compressed files/buffers and C<$output> is
a single file/buffer, after uncompression C<$output> will contain a
concatenation of all the uncompressed data from each of the input
files/buffers.

=head2 Optional Parameters

Unless specified below, the optional parameters for C<inflate>,
C<OPTS>, are the same as those used with the OO interface defined in the
L</"Constructor Options"> section below.

=over 5

=item C<< AutoClose => 0|1 >>

This option applies to any input or output data streams to 
C<inflate> that are filehandles.

If C<AutoClose> is specified, and the value is true, it will result in all
input and/or output filehandles being closed once C<inflate> has
completed.

This parameter defaults to 0.

=item C<< BinModeOut => 0|1 >>

When writing to a file or filehandle, set C<binmode> before writing to the
file.

Defaults to 0.

=item C<< Append => 0|1 >>

TODO

=item C<< MultiStream => 0|1 >>

If the input file/buffer contains multiple compressed data streams, this
option will uncompress the whole lot as a single data stream.

Defaults to 0.

=item C<< TrailingData => $scalar >>

Returns the data, if any, that is present immediately after the compressed
data stream once uncompression is complete. 

This option can be used when there is useful information immediately
following the compressed data stream, and you don't know the length of the
compressed data stream.

If the input is a buffer, C<trailingData> will return everything from the
end of the compressed data stream to the end of the buffer.

If the input is a filehandle, C<trailingData> will return the data that is
left in the filehandle input buffer once the end of the compressed data
stream has been reached. You can then use the filehandle to read the rest
of the input file. 

Don't bother using C<trailingData> if the input is a filename.

If you know the length of the compressed data stream before you start
uncompressing, you can avoid having to use C<trailingData> by setting the
C<InputLength> option.

=back

=head2 Examples

To read the contents of the file C<file1.txt.1950> and write the
compressed data to the file C<file1.txt>.

    use strict ;
    use warnings ;
    use IO::Uncompress::Inflate qw(inflate $InflateError) ;

    my $input = "file1.txt.1950";
    my $output = "file1.txt";
    inflate $input => $output
        or die "inflate failed: $InflateError\n";

To read from an existing Perl filehandle, C<$input>, and write the
uncompressed data to a buffer, C<$buffer>.

    use strict ;
    use warnings ;
    use IO::Uncompress::Inflate qw(inflate $InflateError) ;
    use IO::File ;

    my $input = new IO::File "<file1.txt.1950"
        or die "Cannot open 'file1.txt.1950': $!\n" ;
    my $buffer ;
    inflate $input => \$buffer 
        or die "inflate failed: $InflateError\n";

To uncompress all files in the directory "/my/home" that match "*.txt.1950" and store the compressed data in the same directory

    use strict ;
    use warnings ;
    use IO::Uncompress::Inflate qw(inflate $InflateError) ;

    inflate '</my/home/*.txt.1950>' => '</my/home/#1.txt>'
        or die "inflate failed: $InflateError\n";

and if you want to compress each file one at a time, this will do the trick

    use strict ;
    use warnings ;
    use IO::Uncompress::Inflate qw(inflate $InflateError) ;

    for my $input ( glob "/my/home/*.txt.1950" )
    {
        my $output = $input;
        $output =~ s/.1950// ;
        inflate $input => $output 
            or die "Error compressing '$input': $InflateError\n";
    }

=head1 OO Interface

=head2 Constructor

The format of the constructor for IO::Uncompress::Inflate is shown below

    my $z = new IO::Uncompress::Inflate $input [OPTS]
        or die "IO::Uncompress::Inflate failed: $InflateError\n";

Returns an C<IO::Uncompress::Inflate> object on success and undef on failure.
The variable C<$InflateError> will contain an error message on failure.

If you are running Perl 5.005 or better the object, C<$z>, returned from
IO::Uncompress::Inflate can be used exactly like an L<IO::File|IO::File> filehandle.
This means that all normal input file operations can be carried out with
C<$z>.  For example, to read a line from a compressed file/buffer you can
use either of these forms

    $line = $z->getline();
    $line = <$z>;

The mandatory parameter C<$input> is used to determine the source of the
compressed data. This parameter can take one of three forms.

=over 5

=item A filename

If the C<$input> parameter is a scalar, it is assumed to be a filename. This
file will be opened for reading and the compressed data will be read from it.

=item A filehandle

If the C<$input> parameter is a filehandle, the compressed data will be
read from it.
The string '-' can be used as an alias for standard input.

=item A scalar reference 

If C<$input> is a scalar reference, the compressed data will be read from
C<$$output>.

=back

=head2 Constructor Options

The option names defined below are case insensitive and can be optionally
prefixed by a '-'.  So all of the following are valid

    -AutoClose
    -autoclose
    AUTOCLOSE
    autoclose

OPTS is a combination of the following options:

=over 5

=item C<< AutoClose => 0|1 >>

This option is only valid when the C<$input> parameter is a filehandle. If
specified, and the value is true, it will result in the file being closed once
either the C<close> method is called or the IO::Uncompress::Inflate object is
destroyed.

This parameter defaults to 0.

=item C<< MultiStream => 0|1 >>

Allows multiple concatenated compressed streams to be treated as a single
compressed stream. Decompression will stop once either the end of the
file/buffer is reached, an error is encountered (premature eof, corrupt
compressed data) or the end of a stream is not immediately followed by the
start of another stream.

This parameter defaults to 0.

=item C<< Prime => $string >>

This option will uncompress the contents of C<$string> before processing the
input file/buffer.

This option can be useful when the compressed data is embedded in another
file/data structure and it is not possible to work out where the compressed
data begins without having to read the first few bytes. If this is the
case, the uncompression can be I<primed> with these bytes using this
option.

=item C<< Transparent => 0|1 >>

If this option is set and the input file/buffer is not compressed data,
the module will allow reading of it anyway.

In addition, if the input file/buffer does contain compressed data and
there is non-compressed data immediately following it, setting this option
will make this module treat the whole file/bufffer as a single data stream.

This option defaults to 1.

=item C<< BlockSize => $num >>

When reading the compressed input data, IO::Uncompress::Inflate will read it in
blocks of C<$num> bytes.

This option defaults to 4096.

=item C<< InputLength => $size >>

When present this option will limit the number of compressed bytes read
from the input file/buffer to C<$size>. This option can be used in the
situation where there is useful data directly after the compressed data
stream and you know beforehand the exact length of the compressed data
stream. 

This option is mostly used when reading from a filehandle, in which case
the file pointer will be left pointing to the first byte directly after the
compressed data stream.

This option defaults to off.

=item C<< Append => 0|1 >>

This option controls what the C<read> method does with uncompressed data.

If set to 1, all uncompressed data will be appended to the output parameter
of the C<read> method.

If set to 0, the contents of the output parameter of the C<read> method
will be overwritten by the uncompressed data.

Defaults to 0.

=item C<< Strict => 0|1 >>

This option controls whether the extra checks defined below are used when
carrying out the decompression. When Strict is on, the extra tests are
carried out, when Strict is off they are not.

The default for this option is off.

=over 5

=item 1

The ADLER32 checksum field must be present.

=item 2

The value of the ADLER32 field read must match the adler32 value of the
uncompressed data actually contained in the file.

=back

=back

=head2 Examples

TODO

=head1 Methods 

=head2 read

Usage is

    $status = $z->read($buffer)

Reads a block of compressed data (the size the the compressed block is
determined by the C<Buffer> option in the constructor), uncompresses it and
writes any uncompressed data into C<$buffer>. If the C<Append> parameter is
set in the constructor, the uncompressed data will be appended to the
C<$buffer> parameter. Otherwise C<$buffer> will be overwritten.

Returns the number of uncompressed bytes written to C<$buffer>, zero if eof
or a negative number on error.

=head2 read

Usage is

    $status = $z->read($buffer, $length)
    $status = $z->read($buffer, $length, $offset)

    $status = read($z, $buffer, $length)
    $status = read($z, $buffer, $length, $offset)

Attempt to read C<$length> bytes of uncompressed data into C<$buffer>.

The main difference between this form of the C<read> method and the
previous one, is that this one will attempt to return I<exactly> C<$length>
bytes. The only circumstances that this function will not is if end-of-file
or an IO error is encountered.

Returns the number of uncompressed bytes written to C<$buffer>, zero if eof
or a negative number on error.

=head2 getline

Usage is

    $line = $z->getline()
    $line = <$z>

Reads a single line. 

This method fully supports the use of of the variable C<$/> (or
C<$INPUT_RECORD_SEPARATOR> or C<$RS> when C<English> is in use) to
determine what constitutes an end of line. Paragraph mode, record mode and
file slurp mode are all supported. 

=head2 getc

Usage is 

    $char = $z->getc()

Read a single character.

=head2 ungetc

Usage is

    $char = $z->ungetc($string)

=head2 inflateSync

Usage is

    $status = $z->inflateSync()

TODO

=head2 getHeaderInfo

Usage is

    $hdr  = $z->getHeaderInfo();
    @hdrs = $z->getHeaderInfo();

This method returns either a hash reference (in scalar context) or a list
or hash references (in array context) that contains information about each
of the header fields in the compressed data stream(s).

=head2 tell

Usage is

    $z->tell()
    tell $z

Returns the uncompressed file offset.

=head2 eof

Usage is

    $z->eof();
    eof($z);

Returns true if the end of the compressed input stream has been reached.

=head2 seek

    $z->seek($position, $whence);
    seek($z, $position, $whence);

Provides a sub-set of the C<seek> functionality, with the restriction
that it is only legal to seek forward in the input file/buffer.
It is a fatal error to attempt to seek backward.

The C<$whence> parameter takes one the usual values, namely SEEK_SET,
SEEK_CUR or SEEK_END.

Returns 1 on success, 0 on failure.

=head2 binmode

Usage is

    $z->binmode
    binmode $z ;

This is a noop provided for completeness.

=head2 opened

    $z->opened()

Returns true if the object currently refers to a opened file/buffer. 

=head2 autoflush

    my $prev = $z->autoflush()
    my $prev = $z->autoflush(EXPR)

If the C<$z> object is associated with a file or a filehandle, this method
returns the current autoflush setting for the underlying filehandle. If
C<EXPR> is present, and is non-zero, it will enable flushing after every
write/print operation.

If C<$z> is associated with a buffer, this method has no effect and always
returns C<undef>.

B<Note> that the special variable C<$|> B<cannot> be used to set or
retrieve the autoflush setting.

=head2 input_line_number

    $z->input_line_number()
    $z->input_line_number(EXPR)

Returns the current uncompressed line number. If C<EXPR> is present it has
the effect of setting the line number. Note that setting the line number
does not change the current position within the file/buffer being read.

The contents of C<$/> are used to to determine what constitutes a line
terminator.

=head2 fileno

    $z->fileno()
    fileno($z)

If the C<$z> object is associated with a file or a filehandle, C<fileno>
will return the underlying file descriptor. Once the C<close> method is
called C<fileno> will return C<undef>.

If the C<$z> object is is associated with a buffer, this method will return
C<undef>.

=head2 close

    $z->close() ;
    close $z ;

Closes the output file/buffer. 

For most versions of Perl this method will be automatically invoked if
the IO::Uncompress::Inflate object is destroyed (either explicitly or by the
variable with the reference to the object going out of scope). The
exceptions are Perl versions 5.005 through 5.00504 and 5.8.0. In
these cases, the C<close> method will be called automatically, but
not until global destruction of all live objects when the program is
terminating.

Therefore, if you want your scripts to be able to run on all versions
of Perl, you should call C<close> explicitly and not rely on automatic
closing.

Returns true on success, otherwise 0.

If the C<AutoClose> option has been enabled when the IO::Uncompress::Inflate
object was created, and the object is associated with a file, the
underlying file will also be closed.

=head2 nextStream

Usage is

    my $status = $z->nextStream();

Skips to the next compressed data stream in the input file/buffer. If a new
compressed data stream is found, the eof marker will be cleared and C<$.>
will be reset to 0.

Returns 1 if a new stream was found, 0 if none was found, and -1 if an
error was encountered.

=head2 trailingData

Usage is

    my $data = $z->trailingData();

Returns the data, if any, that is present immediately after the compressed
data stream once uncompression is complete. It only makes sense to call
this method once the end of the compressed data stream has been
encountered.

This option can be used when there is useful information immediately
following the compressed data stream, and you don't know the length of the
compressed data stream.

If the input is a buffer, C<trailingData> will return everything from the
end of the compressed data stream to the end of the buffer.

If the input is a filehandle, C<trailingData> will return the data that is
left in the filehandle input buffer once the end of the compressed data
stream has been reached. You can then use the filehandle to read the rest
of the input file. 

Don't bother using C<trailingData> if the input is a filename.

If you know the length of the compressed data stream before you start
uncompressing, you can avoid having to use C<trailingData> by setting the
C<InputLength> option in the constructor.

=head1 Importing 

No symbolic constants are required by this IO::Uncompress::Inflate at present. 

=over 5

=item :all

Imports C<inflate> and C<$InflateError>.
Same as doing this

    use IO::Uncompress::Inflate qw(inflate $InflateError) ;

=back

=head1 EXAMPLES

=head2 Working with Net::FTP

See L<IO::Uncompress::Inflate::FAQ|IO::Uncompress::Inflate::FAQ/"Compressed files and Net::FTP">

=head1 SEE ALSO

L<Compress::Zlib>, L<IO::Compress::Gzip>, L<IO::Uncompress::Gunzip>, L<IO::Compress::Deflate>, L<IO::Compress::RawDeflate>, L<IO::Uncompress::RawInflate>, L<IO::Compress::Bzip2>, L<IO::Uncompress::Bunzip2>, L<IO::Compress::Lzop>, L<IO::Uncompress::UnLzop>, L<IO::Compress::Lzf>, L<IO::Uncompress::UnLzf>, L<IO::Uncompress::AnyInflate>, L<IO::Uncompress::AnyUncompress>

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

Copyright (c) 2005-2008 Paul Marquess. All rights reserved.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

