package IO::Uncompress::Unzip;

require 5.004 ;

# for RFC1952

use strict ;
use warnings;
use bytes;

use IO::Uncompress::RawInflate  2.011 ;
use IO::Compress::Base::Common  2.011 qw(:Status createSelfTiedObject);
use IO::Uncompress::Adapter::Inflate  2.011 ;
use IO::Uncompress::Adapter::Identity 2.011 ;
use IO::Compress::Zlib::Extra 2.011 ;
use IO::Compress::Zip::Constants 2.011 ;

use Compress::Raw::Zlib  2.011 qw(crc32) ;

BEGIN
{
    eval { require IO::Uncompress::Adapter::Bunzip2 ;
           import  IO::Uncompress::Adapter::Bunzip2 } ;
}


require Exporter ;

our ($VERSION, @ISA, @EXPORT_OK, %EXPORT_TAGS, $UnzipError, %headerLookup);

$VERSION = '2.011';
$UnzipError = '';

@ISA    = qw(Exporter IO::Uncompress::RawInflate);
@EXPORT_OK = qw( $UnzipError unzip );
%EXPORT_TAGS = %IO::Uncompress::RawInflate::EXPORT_TAGS ;
push @{ $EXPORT_TAGS{all} }, @EXPORT_OK ;
Exporter::export_ok_tags('all');

%headerLookup = (
        ZIP_CENTRAL_HDR_SIG,            \&skipCentralDirectory,
        ZIP_END_CENTRAL_HDR_SIG,        \&skipEndCentralDirectory,
        ZIP64_END_CENTRAL_REC_HDR_SIG,  \&skipCentralDirectory64Rec,
        ZIP64_END_CENTRAL_LOC_HDR_SIG,  \&skipCentralDirectory64Loc,
        ZIP64_ARCHIVE_EXTRA_SIG,        \&skipArchiveExtra,
        ZIP64_DIGITAL_SIGNATURE_SIG,    \&skipDigitalSignature,
        );

sub new
{
    my $class = shift ;
    my $obj = createSelfTiedObject($class, \$UnzipError);
    $obj->_create(undef, 0, @_);
}

sub unzip
{
    my $obj = createSelfTiedObject(undef, \$UnzipError);
    return $obj->_inf(@_) ;
}

sub getExtraParams
{
    use IO::Compress::Base::Common  2.011 qw(:Parse);

    
    return (
#            # Zip header fields
            'Name'      => [1, 1, Parse_any,       undef],

#            'Streaming' => [1, 1, Parse_boolean,   1],
        );    
}

sub ckParams
{
    my $self = shift ;
    my $got = shift ;

    # unzip always needs crc32
    $got->value('CRC32' => 1);

    *$self->{UnzipData}{Name} = $got->value('Name');

    return 1;
}

sub mkUncomp
{
    my $self = shift ;
    my $got = shift ;

     my $magic = $self->ckMagic()
        or return 0;

    *$self->{Info} = $self->readHeader($magic)
        or return undef ;

    return 1;

}

sub ckMagic
{
    my $self = shift;

    my $magic ;
    $self->smartReadExact(\$magic, 4);

    *$self->{HeaderPending} = $magic ;

    return $self->HeaderError("Minimum header size is " . 
                              4 . " bytes") 
        if length $magic != 4 ;                                    

    return $self->HeaderError("Bad Magic")
        if ! _isZipMagic($magic) ;

    *$self->{Type} = 'zip';

    return $magic ;
}



sub readHeader
{
    my $self = shift;
    my $magic = shift ;

    my $name =  *$self->{UnzipData}{Name} ;
    my $hdr = $self->_readZipHeader($magic) ;

    while (defined $hdr)
    {
        if (! defined $name || $hdr->{Name} eq $name)
        {
            return $hdr ;
        }

        # skip the data
        my $buffer;
        if (*$self->{ZipData}{Streaming}) {

            while (1) {

                my $b;
                my $status = $self->smartRead(\$b, 1024 * 16);
                return undef
                    if $status <= 0 ;

                my $temp_buf;
                my $out;
                $status = *$self->{Uncomp}->uncompr(\$b, \$temp_buf, 0, $out);

                return $self->saveErrorString(undef, *$self->{Uncomp}{Error}, 
                                                     *$self->{Uncomp}{ErrorNo})
                    if $self->saveStatus($status) == STATUS_ERROR;                

                if ($status == STATUS_ENDSTREAM) {
                    *$self->{Uncomp}->reset();
                    $self->pushBack($b)  ;
                    last;
                }
            }

            # skip the trailer
            $self->smartReadExact(\$buffer, $hdr->{TrailerLength})
                or return $self->saveErrorString(undef, "Truncated file");
        }
        else {
            my $c = $hdr->{CompressedLength}->get32bit();
            $self->smartReadExact(\$buffer, $c)
                or return $self->saveErrorString(undef, "Truncated file");
            $buffer = '';
        }

        $self->chkTrailer($buffer) == STATUS_OK
            or return $self->saveErrorString(undef, "Truncated file");

        $hdr = $self->_readFullZipHeader();

        return $self->saveErrorString(undef, "Cannot find '$name'")
            if $self->smartEof();
    }

    return undef;
}

sub chkTrailer
{
    my $self = shift;
    my $trailer = shift;

    my ($sig, $CRC32, $cSize, $uSize) ;
    my ($cSizeHi, $uSizeHi) = (0, 0);
    if (*$self->{ZipData}{Streaming}) {
        $sig   = unpack ("V", substr($trailer, 0, 4));
        $CRC32 = unpack ("V", substr($trailer, 4, 4));

        if (*$self->{ZipData}{Zip64} ) {
            $cSize = U64::newUnpack_V64 substr($trailer,  8, 8);
            $uSize = U64::newUnpack_V64 substr($trailer, 16, 8);
        }
        else {
            $cSize = U64::newUnpack_V32 substr($trailer,  8, 4);
            $uSize = U64::newUnpack_V32 substr($trailer, 12, 4);
        }

        return $self->TrailerError("Data Descriptor signature, got $sig")
            if $sig != ZIP_DATA_HDR_SIG;
    }
    else {
        ($CRC32, $cSize, $uSize) = 
            (*$self->{ZipData}{Crc32},
             *$self->{ZipData}{CompressedLen},
             *$self->{ZipData}{UnCompressedLen});
    }

    if (*$self->{Strict}) {
        return $self->TrailerError("CRC mismatch")
            if $CRC32  != *$self->{ZipData}{CRC32} ;

        return $self->TrailerError("CSIZE mismatch.")
            if ! $cSize->equal(*$self->{CompSize});

        return $self->TrailerError("USIZE mismatch.")
            if ! $uSize->equal(*$self->{UnCompSize});
    }

    my $reachedEnd = STATUS_ERROR ;
    # check for central directory or end of central directory
    while (1)
    {
        my $magic ;
        my $got = $self->smartRead(\$magic, 4);

        return $self->saveErrorString(STATUS_ERROR, "Truncated file")
            if $got != 4 && *$self->{Strict};

        if ($got == 0) {
            return STATUS_EOF ;
        }
        elsif ($got < 0) {
            return STATUS_ERROR ;
        }
        elsif ($got < 4) {
            $self->pushBack($magic)  ;
            return STATUS_OK ;
        }

        my $sig = unpack("V", $magic) ;

        my $hdr;
        if ($hdr = $headerLookup{$sig})
        {
            if (&$hdr($self, $magic) != STATUS_OK ) {
                if (*$self->{Strict}) {
                    return STATUS_ERROR ;
                }
                else {
                    $self->clearError();
                    return STATUS_OK ;
                }
            }

            if ($sig == ZIP_END_CENTRAL_HDR_SIG)
            {
                return STATUS_OK ;
                last;
            }
        }
        elsif ($sig == ZIP_LOCAL_HDR_SIG)
        {
            $self->pushBack($magic)  ;
            return STATUS_OK ;
        }
        else
        {
            # put the data back
            $self->pushBack($magic)  ;
            last;
        }
    }

    return $reachedEnd ;
}

sub skipCentralDirectory
{
    my $self = shift;
    my $magic = shift ;

    my $buffer;
    $self->smartReadExact(\$buffer, 46 - 4)
        or return $self->TrailerError("Minimum header size is " . 
                                     46 . " bytes") ;

    my $keep = $magic . $buffer ;
    *$self->{HeaderPending} = $keep ;

   #my $versionMadeBy      = unpack ("v", substr($buffer, 4-4,  2));
   #my $extractVersion     = unpack ("v", substr($buffer, 6-4,  2));
   #my $gpFlag             = unpack ("v", substr($buffer, 8-4,  2));
   #my $compressedMethod   = unpack ("v", substr($buffer, 10-4, 2));
   #my $lastModTime        = unpack ("V", substr($buffer, 12-4, 4));
   #my $crc32              = unpack ("V", substr($buffer, 16-4, 4));
    my $compressedLength   = unpack ("V", substr($buffer, 20-4, 4));
    my $uncompressedLength = unpack ("V", substr($buffer, 24-4, 4));
    my $filename_length    = unpack ("v", substr($buffer, 28-4, 2)); 
    my $extra_length       = unpack ("v", substr($buffer, 30-4, 2));
    my $comment_length     = unpack ("v", substr($buffer, 32-4, 2));
   #my $disk_start         = unpack ("v", substr($buffer, 34-4, 2));
   #my $int_file_attrib    = unpack ("v", substr($buffer, 36-4, 2));
   #my $ext_file_attrib    = unpack ("V", substr($buffer, 38-4, 2));
   #my $lcl_hdr_offset     = unpack ("V", substr($buffer, 42-4, 2));

    
    my $filename;
    my $extraField;
    my $comment ;
    if ($filename_length)
    {
        $self->smartReadExact(\$filename, $filename_length)
            or return $self->TruncatedTrailer("filename");
        $keep .= $filename ;
    }

    if ($extra_length)
    {
        $self->smartReadExact(\$extraField, $extra_length)
            or return $self->TruncatedTrailer("extra");
        $keep .= $extraField ;
    }

    if ($comment_length)
    {
        $self->smartReadExact(\$comment, $comment_length)
            or return $self->TruncatedTrailer("comment");
        $keep .= $comment ;
    }

    return STATUS_OK ;
}

sub skipArchiveExtra
{
    my $self = shift;
    my $magic = shift ;

    my $buffer;
    $self->smartReadExact(\$buffer, 4)
        or return $self->TrailerError("Minimum header size is " . 
                                     4 . " bytes") ;

    my $keep = $magic . $buffer ;

    my $size = unpack ("V", $buffer);

    $self->smartReadExact(\$buffer, $size)
        or return $self->TrailerError("Minimum header size is " . 
                                     $size . " bytes") ;

    $keep .= $buffer ;
    *$self->{HeaderPending} = $keep ;

    return STATUS_OK ;
}


sub skipCentralDirectory64Rec
{
    my $self = shift;
    my $magic = shift ;

    my $buffer;
    $self->smartReadExact(\$buffer, 8)
        or return $self->TrailerError("Minimum header size is " . 
                                     8 . " bytes") ;

    my $keep = $magic . $buffer ;

    my ($sizeLo, $sizeHi)  = unpack ("V V", $buffer);

    # TODO - take SizeHi into account
    $self->smartReadExact(\$buffer, $sizeLo)
        or return $self->TrailerError("Minimum header size is " . 
                                     $sizeLo . " bytes") ;

    $keep .= $buffer ;
    *$self->{HeaderPending} = $keep ;

   #my $versionMadeBy      = unpack ("v",   substr($buffer,  0, 2));
   #my $extractVersion     = unpack ("v",   substr($buffer,  2, 2));
   #my $diskNumber         = unpack ("V",   substr($buffer,  4, 4));
   #my $cntrlDirDiskNo     = unpack ("V",   substr($buffer,  8, 4));
   #my $entriesInThisCD    = unpack ("V V", substr($buffer, 12, 8));
   #my $entriesInCD        = unpack ("V V", substr($buffer, 20, 8));
   #my $sizeOfCD           = unpack ("V V", substr($buffer, 28, 8));
   #my $offsetToCD         = unpack ("V V", substr($buffer, 36, 8));

    return STATUS_OK ;
}

sub skipCentralDirectory64Loc
{
    my $self = shift;
    my $magic = shift ;

    my $buffer;
    $self->smartReadExact(\$buffer, 20 - 4)
        or return $self->TrailerError("Minimum header size is " . 
                                     20 . " bytes") ;

    my $keep = $magic . $buffer ;
    *$self->{HeaderPending} = $keep ;

   #my $startCdDisk        = unpack ("V",   substr($buffer,  4-4, 4));
   #my $offsetToCD         = unpack ("V V", substr($buffer,  8-4, 8));
   #my $diskCount          = unpack ("V",   substr($buffer, 16-4, 4));

    return STATUS_OK ;
}

sub skipEndCentralDirectory
{
    my $self = shift;
    my $magic = shift ;

    my $buffer;
    $self->smartReadExact(\$buffer, 22 - 4)
        or return $self->TrailerError("Minimum header size is " . 
                                     22 . " bytes") ;

    my $keep = $magic . $buffer ;
    *$self->{HeaderPending} = $keep ;

   #my $diskNumber         = unpack ("v", substr($buffer, 4-4,  2));
   #my $cntrlDirDiskNo     = unpack ("v", substr($buffer, 6-4,  2));
   #my $entriesInThisCD    = unpack ("v", substr($buffer, 8-4,  2));
   #my $entriesInCD        = unpack ("v", substr($buffer, 10-4, 2));
   #my $sizeOfCD           = unpack ("V", substr($buffer, 12-4, 2));
   #my $offsetToCD         = unpack ("V", substr($buffer, 16-4, 2));
    my $comment_length     = unpack ("v", substr($buffer, 20-4, 2));

    
    my $comment ;
    if ($comment_length)
    {
        $self->smartReadExact(\$comment, $comment_length)
            or return $self->TruncatedTrailer("comment");
        $keep .= $comment ;
    }

    return STATUS_OK ;
}


sub _isZipMagic
{
    my $buffer = shift ;
    return 0 if length $buffer < 4 ;
    my $sig = unpack("V", $buffer) ;
    return $sig == ZIP_LOCAL_HDR_SIG ;
}


sub _readFullZipHeader($)
{
    my ($self) = @_ ;
    my $magic = '' ;

    $self->smartReadExact(\$magic, 4);

    *$self->{HeaderPending} = $magic ;

    return $self->HeaderError("Minimum header size is " . 
                              30 . " bytes") 
        if length $magic != 4 ;                                    


    return $self->HeaderError("Bad Magic")
        if ! _isZipMagic($magic) ;

    my $status = $self->_readZipHeader($magic);
    delete *$self->{Transparent} if ! defined $status ;
    return $status ;
}

sub _readZipHeader($)
{
    my ($self, $magic) = @_ ;
    my ($HeaderCRC) ;
    my ($buffer) = '' ;

    $self->smartReadExact(\$buffer, 30 - 4)
        or return $self->HeaderError("Minimum header size is " . 
                                     30 . " bytes") ;

    my $keep = $magic . $buffer ;
    *$self->{HeaderPending} = $keep ;

    my $extractVersion     = unpack ("v", substr($buffer, 4-4,  2));
    my $gpFlag             = unpack ("v", substr($buffer, 6-4,  2));
    my $compressedMethod   = unpack ("v", substr($buffer, 8-4,  2));
    my $lastModTime        = unpack ("V", substr($buffer, 10-4, 4));
    my $crc32              = unpack ("V", substr($buffer, 14-4, 4));
    my $compressedLength   = new U64 unpack ("V", substr($buffer, 18-4, 4));
    my $uncompressedLength = new U64 unpack ("V", substr($buffer, 22-4, 4));
    my $filename_length    = unpack ("v", substr($buffer, 26-4, 2)); 
    my $extra_length       = unpack ("v", substr($buffer, 28-4, 2));

    my $filename;
    my $extraField;
    my @EXTRA = ();
    my $streamingMode = ($gpFlag & ZIP_GP_FLAG_STREAMING_MASK) ? 1 : 0 ;

    return $self->HeaderError("Streamed Stored content not supported")
        if $streamingMode && $compressedMethod == 0 ;

    return $self->HeaderError("Encrypted content not supported")
        if $gpFlag & (ZIP_GP_FLAG_ENCRYPTED_MASK|ZIP_GP_FLAG_STRONG_ENCRYPTED_MASK);

    return $self->HeaderError("Patch content not supported")
        if $gpFlag & ZIP_GP_FLAG_PATCHED_MASK;

    *$self->{ZipData}{Streaming} = $streamingMode;


    if ($filename_length)
    {
        $self->smartReadExact(\$filename, $filename_length)
            or return $self->TruncatedHeader("Filename");
        $keep .= $filename ;
    }

    my $zip64 = 0 ;

    if ($extra_length)
    {
        $self->smartReadExact(\$extraField, $extra_length)
            or return $self->TruncatedHeader("Extra Field");

        my $bad = IO::Compress::Zlib::Extra::parseRawExtra($extraField,
                                                \@EXTRA, 1, 0);
        return $self->HeaderError($bad)
            if defined $bad;

        $keep .= $extraField ;

        my %Extra ;
        for (@EXTRA)
        {
            $Extra{$_->[0]} = \$_->[1];
        }
        
        if (defined $Extra{ZIP_EXTRA_ID_ZIP64()})
        {
            $zip64 = 1 ;

            my $buff = ${ $Extra{ZIP_EXTRA_ID_ZIP64()} };

            # TODO - This code assumes that all the fields in the Zip64
            # extra field aren't necessarily present. The spec says that
            # they only exist if the equivalent local headers are -1.
            # Need to check that info-zip fills out -1 in the local header
            # correctly.

            if (! $streamingMode) {
                my $offset = 0 ;

                $uncompressedLength = U64::newUnpack_V64 substr($buff,  0, 8)
                    if $uncompressedLength == 0xFFFF ;

                $offset += 8 ;

                $compressedLength = U64::newUnpack_V64 substr($buff, $offset, 8)
                    if $compressedLength == 0xFFFF ;

                $offset += 8 ;

                #my $cheaderOffset = U64::newUnpack_V64 substr($buff, 16, 8);
                #my $diskNumber = unpack ("V", substr($buff, 24, 4));
           }
        }
    }

    *$self->{ZipData}{Zip64} = $zip64;

    if (! $streamingMode) {
        *$self->{ZipData}{Streaming} = 0;
        *$self->{ZipData}{Crc32} = $crc32;
        *$self->{ZipData}{CompressedLen} = $compressedLength;
        *$self->{ZipData}{UnCompressedLen} = $uncompressedLength;
        *$self->{CompressedInputLengthRemaining} =
            *$self->{CompressedInputLength} = $compressedLength->get32bit();
    }

    *$self->{ZipData}{Method} = $compressedMethod;
    if ($compressedMethod == ZIP_CM_DEFLATE)
    {
        *$self->{Type} = 'zip-deflate';
        my $obj = IO::Uncompress::Adapter::Inflate::mkUncompObject(1,0,0);

        *$self->{Uncomp} = $obj;
        *$self->{ZipData}{CRC32} = crc32(undef);
    }
    elsif ($compressedMethod == ZIP_CM_BZIP2)
    {
        return $self->HeaderError("Unsupported Compression format $compressedMethod")
            if ! defined $IO::Uncompress::Adapter::Bunzip2::VERSION ;
        
        *$self->{Type} = 'zip-bzip2';
        
        my $obj = IO::Uncompress::Adapter::Bunzip2::mkUncompObject();

        *$self->{Uncomp} = $obj;
        *$self->{ZipData}{CRC32} = crc32(undef);
    }
    elsif ($compressedMethod == ZIP_CM_STORE)
    {
        # TODO -- add support for reading uncompressed

        *$self->{Type} = 'zip-stored';
        
        my $obj = IO::Uncompress::Adapter::Identity::mkUncompObject();

        *$self->{Uncomp} = $obj;
    }
    else
    {
        return $self->HeaderError("Unsupported Compression format $compressedMethod");
    }

    return {
        'Type'               => 'zip',
        'FingerprintLength'  => 4,
        #'HeaderLength'       => $compressedMethod == 8 ? length $keep : 0,
        'HeaderLength'       => length $keep,
        'Zip64'              => $zip64,
        'TrailerLength'      => ! $streamingMode ? 0 : $zip64 ? 24 : 16,
        'Header'             => $keep,
        'CompressedLength'   => $compressedLength ,
        'UncompressedLength' => $uncompressedLength ,
        'CRC32'              => $crc32 ,
        'Name'               => $filename,
        'Time'               => _dosToUnixTime($lastModTime),
        'Stream'             => $streamingMode,

        'MethodID'           => $compressedMethod,
        'MethodName'         => $compressedMethod == ZIP_CM_DEFLATE 
                                 ? "Deflated" 
                                 : $compressedMethod == ZIP_CM_BZIP2
                                     ? "Bzip2"
                                     : $compressedMethod == ZIP_CM_STORE
                                         ? "Stored"
                                         : "Unknown" ,

#        'TextFlag'      => $flag & GZIP_FLG_FTEXT ? 1 : 0,
#        'HeaderCRCFlag' => $flag & GZIP_FLG_FHCRC ? 1 : 0,
#        'NameFlag'      => $flag & GZIP_FLG_FNAME ? 1 : 0,
#        'CommentFlag'   => $flag & GZIP_FLG_FCOMMENT ? 1 : 0,
#        'ExtraFlag'     => $flag & GZIP_FLG_FEXTRA ? 1 : 0,
#        'Comment'       => $comment,
#        'OsID'          => $os,
#        'OsName'        => defined $GZIP_OS_Names{$os} 
#                                 ? $GZIP_OS_Names{$os} : "Unknown",
#        'HeaderCRC'     => $HeaderCRC,
#        'Flags'         => $flag,
#        'ExtraFlags'    => $xfl,
        'ExtraFieldRaw' => $extraField,
        'ExtraField'    => [ @EXTRA ],


      }
}

sub filterUncompressed
{
    my $self = shift ;

    if (*$self->{ZipData}{Method} == 12) {
        *$self->{ZipData}{CRC32} = crc32(${$_[0]}, *$self->{ZipData}{CRC32});
    }
    else {
        *$self->{ZipData}{CRC32} = *$self->{Uncomp}->crc32() ;
    }
}    


# from Archive::Zip
sub _dosToUnixTime
{
    #use Time::Local 'timelocal_nocheck';
    use Time::Local 'timelocal';

	my $dt = shift;

	my $year = ( ( $dt >> 25 ) & 0x7f ) + 80;
	my $mon  = ( ( $dt >> 21 ) & 0x0f ) - 1;
	my $mday = ( ( $dt >> 16 ) & 0x1f );

	my $hour = ( ( $dt >> 11 ) & 0x1f );
	my $min  = ( ( $dt >> 5 ) & 0x3f );
	my $sec  = ( ( $dt << 1 ) & 0x3e );

	# catch errors
	my $time_t =
	  eval { timelocal( $sec, $min, $hour, $mday, $mon, $year ); };
	return 0 
        if $@;
	return $time_t;
}


1;

__END__


=head1 NAME

IO::Uncompress::Unzip - Read zip files/buffers

=head1 SYNOPSIS

    use IO::Uncompress::Unzip qw(unzip $UnzipError) ;

    my $status = unzip $input => $output [,OPTS]
        or die "unzip failed: $UnzipError\n";

    my $z = new IO::Uncompress::Unzip $input [OPTS] 
        or die "unzip failed: $UnzipError\n";

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

    $UnzipError ;

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
zlib files/buffers.

For writing zip files/buffers, see the companion module IO::Compress::Zip.

=head1 Functional Interface

A top-level function, C<unzip>, is provided to carry out
"one-shot" uncompression between buffers and/or files. For finer
control over the uncompression process, see the L</"OO Interface">
section.

    use IO::Uncompress::Unzip qw(unzip $UnzipError) ;

    unzip $input => $output [,OPTS] 
        or die "unzip failed: $UnzipError\n";

The functional interface needs Perl5.005 or better.

=head2 unzip $input => $output [, OPTS]

C<unzip> expects at least two parameters, C<$input> and C<$output>.

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
C<unzip> will assume that it is an I<input fileglob string>. The
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
C<unzip> will assume that it is an I<output fileglob string>. The
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

Unless specified below, the optional parameters for C<unzip>,
C<OPTS>, are the same as those used with the OO interface defined in the
L</"Constructor Options"> section below.

=over 5

=item C<< AutoClose => 0|1 >>

This option applies to any input or output data streams to 
C<unzip> that are filehandles.

If C<AutoClose> is specified, and the value is true, it will result in all
input and/or output filehandles being closed once C<unzip> has
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

To read the contents of the file C<file1.txt.zip> and write the
compressed data to the file C<file1.txt>.

    use strict ;
    use warnings ;
    use IO::Uncompress::Unzip qw(unzip $UnzipError) ;

    my $input = "file1.txt.zip";
    my $output = "file1.txt";
    unzip $input => $output
        or die "unzip failed: $UnzipError\n";

To read from an existing Perl filehandle, C<$input>, and write the
uncompressed data to a buffer, C<$buffer>.

    use strict ;
    use warnings ;
    use IO::Uncompress::Unzip qw(unzip $UnzipError) ;
    use IO::File ;

    my $input = new IO::File "<file1.txt.zip"
        or die "Cannot open 'file1.txt.zip': $!\n" ;
    my $buffer ;
    unzip $input => \$buffer 
        or die "unzip failed: $UnzipError\n";

To uncompress all files in the directory "/my/home" that match "*.txt.zip" and store the compressed data in the same directory

    use strict ;
    use warnings ;
    use IO::Uncompress::Unzip qw(unzip $UnzipError) ;

    unzip '</my/home/*.txt.zip>' => '</my/home/#1.txt>'
        or die "unzip failed: $UnzipError\n";

and if you want to compress each file one at a time, this will do the trick

    use strict ;
    use warnings ;
    use IO::Uncompress::Unzip qw(unzip $UnzipError) ;

    for my $input ( glob "/my/home/*.txt.zip" )
    {
        my $output = $input;
        $output =~ s/.zip// ;
        unzip $input => $output 
            or die "Error compressing '$input': $UnzipError\n";
    }

=head1 OO Interface

=head2 Constructor

The format of the constructor for IO::Uncompress::Unzip is shown below

    my $z = new IO::Uncompress::Unzip $input [OPTS]
        or die "IO::Uncompress::Unzip failed: $UnzipError\n";

Returns an C<IO::Uncompress::Unzip> object on success and undef on failure.
The variable C<$UnzipError> will contain an error message on failure.

If you are running Perl 5.005 or better the object, C<$z>, returned from
IO::Uncompress::Unzip can be used exactly like an L<IO::File|IO::File> filehandle.
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
either the C<close> method is called or the IO::Uncompress::Unzip object is
destroyed.

This parameter defaults to 0.

=item C<< MultiStream => 0|1 >>

Treats the complete zip file/buffer as a single compressed data
stream. When reading in multi-stream mode each member of the zip
file/buffer will be uncompressed in turn until the end of the file/buffer
is encountered.

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

When reading the compressed input data, IO::Uncompress::Unzip will read it in
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
the IO::Uncompress::Unzip object is destroyed (either explicitly or by the
variable with the reference to the object going out of scope). The
exceptions are Perl versions 5.005 through 5.00504 and 5.8.0. In
these cases, the C<close> method will be called automatically, but
not until global destruction of all live objects when the program is
terminating.

Therefore, if you want your scripts to be able to run on all versions
of Perl, you should call C<close> explicitly and not rely on automatic
closing.

Returns true on success, otherwise 0.

If the C<AutoClose> option has been enabled when the IO::Uncompress::Unzip
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

No symbolic constants are required by this IO::Uncompress::Unzip at present. 

=over 5

=item :all

Imports C<unzip> and C<$UnzipError>.
Same as doing this

    use IO::Uncompress::Unzip qw(unzip $UnzipError) ;

=back

=head1 EXAMPLES

=head2 Working with Net::FTP

See L<IO::Uncompress::Unzip::FAQ|IO::Uncompress::Unzip::FAQ/"Compressed files and Net::FTP">

=head2 Walking through a zip file

The code below can be used to traverse a zip file, one compressed data
stream at a time.

    use IO::Uncompress::Unzip qw($UnzipError);

    my $zipfile = "somefile.zip";
    my $u = new IO::Uncompress::Unzip $zipfile
        or die "Cannot open $zipfile: $UnzipError";

    my $status;
    for ($status = 1; ! $u->eof(); $status = $u->nextStream())
    {
 
        my $name = $u->getHeaderInfo()->{Name};
        warn "Processing member $name\n" ;

        my $buff;
        while (($status = $u->read($buff)) > 0) {
            # Do something here
        }

        last unless $status == 0;
    }

    die "Error processing $zipfile: $!\n"
        if $status < 0 ;

Each individual compressed data stream is read until the logical
end-of-file is reached. Then C<nextStream> is called. This will skip to the
start of the next compressed data stream and clear the end-of-file flag.

It is also worth noting that C<nextStream> can be called at any time -- you
don't have to wait until you have exhausted a compressed data stream before
skipping to the next one.

=head1 SEE ALSO

L<Compress::Zlib>, L<IO::Compress::Gzip>, L<IO::Uncompress::Gunzip>, L<IO::Compress::Deflate>, L<IO::Uncompress::Inflate>, L<IO::Compress::RawDeflate>, L<IO::Uncompress::RawInflate>, L<IO::Compress::Bzip2>, L<IO::Uncompress::Bunzip2>, L<IO::Compress::Lzop>, L<IO::Uncompress::UnLzop>, L<IO::Compress::Lzf>, L<IO::Uncompress::UnLzf>, L<IO::Uncompress::AnyInflate>, L<IO::Uncompress::AnyUncompress>

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

