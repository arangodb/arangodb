package IO::Compress::Zip ;

use strict ;
use warnings;
use bytes;

use IO::Compress::Base::Common  2.011 qw(:Status createSelfTiedObject);
use IO::Compress::RawDeflate 2.011 ;
use IO::Compress::Adapter::Deflate 2.011 ;
use IO::Compress::Adapter::Identity 2.011 ;
use IO::Compress::Zlib::Extra 2.011 ;
use IO::Compress::Zip::Constants 2.011 ;


use Compress::Raw::Zlib  2.011 qw(crc32) ;
BEGIN
{
    eval { require IO::Compress::Adapter::Bzip2 ; 
           import  IO::Compress::Adapter::Bzip2 2.011 ; 
           require IO::Compress::Bzip2 ; 
           import  IO::Compress::Bzip2 2.011 ; 
         } ;
}


require Exporter ;

our ($VERSION, @ISA, @EXPORT_OK, %EXPORT_TAGS, $ZipError);

$VERSION = '2.011';
$ZipError = '';

@ISA = qw(Exporter IO::Compress::RawDeflate);
@EXPORT_OK = qw( $ZipError zip ) ;
%EXPORT_TAGS = %IO::Compress::RawDeflate::DEFLATE_CONSTANTS ;
push @{ $EXPORT_TAGS{all} }, @EXPORT_OK ;

$EXPORT_TAGS{zip_method} = [qw( ZIP_CM_STORE ZIP_CM_DEFLATE ZIP_CM_BZIP2 )];
push @{ $EXPORT_TAGS{all} }, @{ $EXPORT_TAGS{zip_method} };

Exporter::export_ok_tags('all');

sub new
{
    my $class = shift ;

    my $obj = createSelfTiedObject($class, \$ZipError);    
    $obj->_create(undef, @_);
}

sub zip
{
    my $obj = createSelfTiedObject(undef, \$ZipError);    
    return $obj->_def(@_);
}

sub mkComp
{
    my $self = shift ;
    my $got = shift ;

    my ($obj, $errstr, $errno) ;

    if (*$self->{ZipData}{Method} == ZIP_CM_STORE) {
        ($obj, $errstr, $errno) = IO::Compress::Adapter::Identity::mkCompObject(
                                                 $got->value('Level'),
                                                 $got->value('Strategy')
                                                 );
    }
    elsif (*$self->{ZipData}{Method} == ZIP_CM_DEFLATE) {
        ($obj, $errstr, $errno) = IO::Compress::Adapter::Deflate::mkCompObject(
                                                 $got->value('CRC32'),
                                                 $got->value('Adler32'),
                                                 $got->value('Level'),
                                                 $got->value('Strategy')
                                                 );
    }
    elsif (*$self->{ZipData}{Method} == ZIP_CM_BZIP2) {
        ($obj, $errstr, $errno) = IO::Compress::Adapter::Bzip2::mkCompObject(
                                                $got->value('BlockSize100K'),
                                                $got->value('WorkFactor'),
                                                $got->value('Verbosity')
                                               );
        *$self->{ZipData}{CRC32} = crc32(undef);
    }

    return $self->saveErrorString(undef, $errstr, $errno)
       if ! defined $obj;

    if (! defined *$self->{ZipData}{StartOffset}) {
        *$self->{ZipData}{StartOffset} = 0;
        *$self->{ZipData}{Offset} = new U64 ;
    }

    return $obj;    
}

sub reset
{
    my $self = shift ;

    *$self->{Compress}->reset();
    *$self->{ZipData}{CRC32} = Compress::Raw::Zlib::crc32('');

    return STATUS_OK;    
}

sub filterUncompressed
{
    my $self = shift ;

    if (*$self->{ZipData}{Method} == ZIP_CM_DEFLATE) {
        *$self->{ZipData}{CRC32} = *$self->{Compress}->crc32();
    }
    else {
        *$self->{ZipData}{CRC32} = crc32(${$_[0]}, *$self->{ZipData}{CRC32});

    }
}

sub mkHeader
{
    my $self  = shift;
    my $param = shift ;
    
    *$self->{ZipData}{StartOffset} = *$self->{ZipData}{Offset}->get32bit() ;

    my $filename = '';
    $filename = $param->value('Name') || '';

    my $comment = '';
    $comment = $param->value('Comment') || '';

    my $hdr = '';

    my $time = _unixToDosTime($param->value('Time'));

    my $extra = '';
    my $ctlExtra = '';
    my $empty = 0;
    my $osCode = $param->value('OS_Code') ;
    my $extFileAttr = 0 ;
    
    # This code assumes Unix.
    $extFileAttr = 0666 << 16 
        if $osCode == ZIP_OS_CODE_UNIX ;

    if (*$self->{ZipData}{Zip64}) {
        $empty = 0xFFFF;

        my $x = '';
        $x .= pack "V V", 0, 0 ; # uncompressedLength   
        $x .= pack "V V", 0, 0 ; # compressedLength   
        $x .= *$self->{ZipData}{Offset}->getPacked_V64() ; # offset to local hdr
        #$x .= pack "V  ", 0    ; # disk no

        $x = IO::Compress::Zlib::Extra::mkSubField(ZIP_EXTRA_ID_ZIP64, $x);
        $extra .= $x;
        $ctlExtra .= $x;
    }

    if (! $param->value('Minimal')) {
        if (defined $param->value('exTime'))
        {
            $extra .= mkExtendedTime($param->value('MTime'), 
                                    $param->value('ATime'), 
                                    $param->value('CTime'));

            $ctlExtra .= mkExtendedTime($param->value('MTime'));
        }

        if ( $param->value('UID') && $osCode == ZIP_OS_CODE_UNIX)
        {
            $extra    .= mkUnix2Extra( $param->value('UID'), $param->value('GID'));
            $ctlExtra .= mkUnix2Extra();
        }

        $extFileAttr = $param->value('ExtAttr') 
            if defined $param->value('ExtAttr') ;

        $extra .= $param->value('ExtraFieldLocal') 
            if defined $param->value('ExtraFieldLocal');

        $ctlExtra .= $param->value('ExtraFieldCentral') 
            if defined $param->value('ExtraFieldCentral');
    }

    my $gpFlag = 0 ;    
    $gpFlag |= ZIP_GP_FLAG_STREAMING_MASK
        if *$self->{ZipData}{Stream} ;

    my $method = *$self->{ZipData}{Method} ;

    my $version = $ZIP_CM_MIN_VERSIONS{$method};
    $version = ZIP64_MIN_VERSION
        if ZIP64_MIN_VERSION > $version && *$self->{ZipData}{Zip64};
    my $madeBy = ($param->value('OS_Code') << 8) + $version;
    my $extract = $version;

    *$self->{ZipData}{Version} = $version;
    *$self->{ZipData}{MadeBy} = $madeBy;

    my $ifa = 0;
    $ifa |= ZIP_IFA_TEXT_MASK
        if $param->value('TextFlag');

    $hdr .= pack "V", ZIP_LOCAL_HDR_SIG ; # signature
    $hdr .= pack 'v', $extract   ; # extract Version & OS
    $hdr .= pack 'v', $gpFlag    ; # general purpose flag (set streaming mode)
    $hdr .= pack 'v', $method    ; # compression method (deflate)
    $hdr .= pack 'V', $time      ; # last mod date/time
    $hdr .= pack 'V', 0          ; # crc32               - 0 when streaming
    $hdr .= pack 'V', $empty     ; # compressed length   - 0 when streaming
    $hdr .= pack 'V', $empty     ; # uncompressed length - 0 when streaming
    $hdr .= pack 'v', length $filename ; # filename length
    $hdr .= pack 'v', length $extra ; # extra length
    
    $hdr .= $filename ;
    $hdr .= $extra ;


    my $ctl = '';

    $ctl .= pack "V", ZIP_CENTRAL_HDR_SIG ; # signature
    $ctl .= pack 'v', $madeBy    ; # version made by
    $ctl .= pack 'v', $extract   ; # extract Version
    $ctl .= pack 'v', $gpFlag    ; # general purpose flag (streaming mode)
    $ctl .= pack 'v', $method    ; # compression method (deflate)
    $ctl .= pack 'V', $time      ; # last mod date/time
    $ctl .= pack 'V', 0          ; # crc32
    $ctl .= pack 'V', $empty     ; # compressed length
    $ctl .= pack 'V', $empty     ; # uncompressed length
    $ctl .= pack 'v', length $filename ; # filename length
    $ctl .= pack 'v', length $ctlExtra ; # extra length
    $ctl .= pack 'v', length $comment ;  # file comment length
    $ctl .= pack 'v', 0          ; # disk number start 
    $ctl .= pack 'v', $ifa       ; # internal file attributes
    $ctl .= pack 'V', $extFileAttr   ; # external file attributes
    if (! *$self->{ZipData}{Zip64}) {
        $ctl .= pack 'V', *$self->{ZipData}{Offset}->get32bit()  ; # offset to local header
    }
    else {
        $ctl .= pack 'V', $empty ; # offset to local header
    }
    
    $ctl .= $filename ;
    *$self->{ZipData}{StartOffset64} = 4 + length $ctl;
    $ctl .= $ctlExtra ;
    $ctl .= $comment ;

    *$self->{ZipData}{Offset}->add(length $hdr) ;

    *$self->{ZipData}{CentralHeader} = $ctl;

    return $hdr;
}

sub mkTrailer
{
    my $self = shift ;

    my $crc32 ;
    if (*$self->{ZipData}{Method} == ZIP_CM_DEFLATE) {
        $crc32 = pack "V", *$self->{Compress}->crc32();
    }
    else {
        $crc32 = pack "V", *$self->{ZipData}{CRC32};
    }

    my $ctl = *$self->{ZipData}{CentralHeader} ;

    my $sizes ;
    if (! *$self->{ZipData}{Zip64}) {
        $sizes .= *$self->{CompSize}->getPacked_V32() ;   # Compressed size
        $sizes .= *$self->{UnCompSize}->getPacked_V32() ; # Uncompressed size
    }
    else {
        $sizes .= *$self->{CompSize}->getPacked_V64() ;   # Compressed size
        $sizes .= *$self->{UnCompSize}->getPacked_V64() ; # Uncompressed size
    }

    my $data = $crc32 . $sizes ;


    my $hdr = '';

    if (*$self->{ZipData}{Stream}) {
        $hdr  = pack "V", ZIP_DATA_HDR_SIG ;                       # signature
        $hdr .= $data ;
    }
    else {
        $self->writeAt(*$self->{ZipData}{StartOffset} + 14, $data)
            or return undef;
    }

    if (! *$self->{ZipData}{Zip64})
      { substr($ctl, 16, length $data) = $data }
    else {
        substr($ctl, 16, length $crc32) = $crc32 ;
        my $s  = *$self->{UnCompSize}->getPacked_V64() ; # Uncompressed size
           $s .= *$self->{CompSize}->getPacked_V64() ;   # Compressed size
        substr($ctl, *$self->{ZipData}{StartOffset64}, length $s) = $s ;
    }

    *$self->{ZipData}{Offset}->add(length($hdr));
    *$self->{ZipData}{Offset}->add( *$self->{CompSize} );
    push @{ *$self->{ZipData}{CentralDir} }, $ctl ;

    return $hdr;
}

sub mkFinalTrailer
{
    my $self = shift ;

    my $comment = '';
    $comment = *$self->{ZipData}{ZipComment} ;

    my $cd_offset = *$self->{ZipData}{Offset}->get32bit() ; # offset to start central dir

    my $entries = @{ *$self->{ZipData}{CentralDir} };
    my $cd = join '', @{ *$self->{ZipData}{CentralDir} };
    my $cd_len = length $cd ;

    my $z64e = '';

    if ( *$self->{ZipData}{Zip64} ) {

        my $v  = *$self->{ZipData}{Version} ;
        my $mb = *$self->{ZipData}{MadeBy} ;
        $z64e .= pack 'v', $v             ; # Version made by
        $z64e .= pack 'v', $mb            ; # Version to extract
        $z64e .= pack 'V', 0              ; # number of disk
        $z64e .= pack 'V', 0              ; # number of disk with central dir
        $z64e .= U64::pack_V64 $entries   ; # entries in central dir on this disk
        $z64e .= U64::pack_V64 $entries   ; # entries in central dir
        $z64e .= U64::pack_V64 $cd_len    ; # size of central dir
        $z64e .= *$self->{ZipData}{Offset}->getPacked_V64() ; # offset to start central dir

        $z64e  = pack("V", ZIP64_END_CENTRAL_REC_HDR_SIG) # signature
              .  U64::pack_V64(length $z64e)
              .  $z64e ;

        *$self->{ZipData}{Offset}->add(length $cd) ; 

        $z64e .= pack "V", ZIP64_END_CENTRAL_LOC_HDR_SIG; # signature
        $z64e .= pack 'V', 0              ; # number of disk with central dir
        $z64e .= *$self->{ZipData}{Offset}->getPacked_V64() ; # offset to end zip64 central dir
        $z64e .= pack 'V', 1              ; # Total number of disks 

        # TODO - fix these when info-zip 3 is fixed.
        #$cd_len = 
        #$cd_offset = 
        $entries = 0xFFFF ;
    }

    my $ecd = '';
    $ecd .= pack "V", ZIP_END_CENTRAL_HDR_SIG ; # signature
    $ecd .= pack 'v', 0          ; # number of disk
    $ecd .= pack 'v', 0          ; # number of disk with central dir
    $ecd .= pack 'v', $entries   ; # entries in central dir on this disk
    $ecd .= pack 'v', $entries   ; # entries in central dir
    $ecd .= pack 'V', $cd_len    ; # size of central dir
    $ecd .= pack 'V', $cd_offset ; # offset to start central dir
    $ecd .= pack 'v', length $comment ; # zipfile comment length
    $ecd .= $comment;

    return $cd . $z64e . $ecd ;
}

sub ckParams
{
    my $self = shift ;
    my $got = shift;
    
    $got->value('CRC32' => 1);

    if (! $got->parsed('Time') ) {
        # Modification time defaults to now.
        $got->value('Time' => time) ;
    }

    if (! $got->parsed('exTime') ) {
        my $timeRef = $got->value('exTime');
        if ( defined $timeRef) {
            return $self->saveErrorString(undef, "exTime not a 3-element array ref")   
                if ref $timeRef ne 'ARRAY' || @$timeRef != 3;
        }

        $got->value("MTime", $timeRef->[1]);
        $got->value("ATime", $timeRef->[0]);
        $got->value("CTime", $timeRef->[2]);
    }
    
    # Unix2 Extended Attribute
    if (! $got->parsed('exUnix2') ) {
        my $timeRef = $got->value('exUnix2');
        if ( defined $timeRef) {
            return $self->saveErrorString(undef, "exUnix2 not a 2-element array ref")   
                if ref $timeRef ne 'ARRAY' || @$timeRef != 2;
        }

        $got->value("UID", $timeRef->[0]);
        $got->value("GID", $timeRef->[1]);
    }

    *$self->{ZipData}{Zip64} = $got->value('Zip64');
    *$self->{ZipData}{Stream} = $got->value('Stream');

    return $self->saveErrorString(undef, "Zip64 only supported if Stream enabled")   
        if  *$self->{ZipData}{Zip64} && ! *$self->{ZipData}{Stream} ;

    my $method = $got->value('Method');
    return $self->saveErrorString(undef, "Unknown Method '$method'")   
        if ! defined $ZIP_CM_MIN_VERSIONS{$method};

    return $self->saveErrorString(undef, "Bzip2 not available")
        if $method == ZIP_CM_BZIP2 and 
           ! defined $IO::Compress::Adapter::Bzip2::VERSION;

    *$self->{ZipData}{Method} = $method;

    *$self->{ZipData}{ZipComment} = $got->value('ZipComment') ;

    for my $name (qw( ExtraFieldLocal ExtraFieldCentral ))
    {
        my $data = $got->value($name) ;
        if (defined $data) {
            my $bad = IO::Compress::Zlib::Extra::parseExtraField($data, 1, 0) ;
            return $self->saveErrorString(undef, "Error with $name Parameter: $bad")
                if $bad ;

            $got->value($name, $data) ;
        }
    }

    return undef
        if defined $IO::Compress::Bzip2::VERSION
            and ! IO::Compress::Bzip2::ckParams($self, $got);

    return 1 ;
}

#sub newHeader
#{
#    my $self = shift ;
#
#    return $self->mkHeader(*$self->{Got});
#}

sub getExtraParams
{
    my $self = shift ;

    use IO::Compress::Base::Common  2.011 qw(:Parse);
    use Compress::Raw::Zlib  2.011 qw(Z_DEFLATED Z_DEFAULT_COMPRESSION Z_DEFAULT_STRATEGY);

    my @Bzip2 = ();
    
    @Bzip2 = IO::Compress::Bzip2::getExtraParams($self)
        if defined $IO::Compress::Bzip2::VERSION;
    
    return (
            # zlib behaviour
            $self->getZlibParams(),

            'Stream'    => [1, 1, Parse_boolean,   1],
           #'Store'     => [0, 1, Parse_boolean,   0],
            'Method'    => [0, 1, Parse_unsigned,  ZIP_CM_DEFLATE],
            
#            # Zip header fields
            'Minimal'   => [0, 1, Parse_boolean,   0],
            'Zip64'     => [0, 1, Parse_boolean,   0],
            'Comment'   => [0, 1, Parse_any,       ''],
            'ZipComment'=> [0, 1, Parse_any,       ''],
            'Name'      => [0, 1, Parse_any,       ''],
            'Time'      => [0, 1, Parse_any,       undef],
            'exTime'    => [0, 1, Parse_any,       undef],
            'exUnix2'   => [0, 1, Parse_any,       undef], 
            'ExtAttr'   => [0, 1, Parse_any,       0],
            'OS_Code'   => [0, 1, Parse_unsigned,  $Compress::Raw::Zlib::gzip_os_code],
            
           'TextFlag'  => [0, 1, Parse_boolean,   0],
           'ExtraFieldLocal'  => [0, 1, Parse_any,    undef],
           'ExtraFieldCentral'=> [0, 1, Parse_any,    undef],

            @Bzip2,
        );
}

sub getInverseClass
{
    return ('IO::Uncompress::Unzip',
                \$IO::Uncompress::Unzip::UnzipError);
}

sub getFileInfo
{
    my $self = shift ;
    my $params = shift;
    my $filename = shift ;

    my ($mode, $uid, $gid, $atime, $mtime, $ctime) 
                = (stat($filename))[2, 4,5, 8,9,10] ;

    $params->value('Name' => $filename)
        if ! $params->parsed('Name') ;

    $params->value('Time' => $mtime) 
        if ! $params->parsed('Time') ;
    
    if ( ! $params->parsed('exTime'))
    {
        $params->value('MTime' => $mtime) ;
        $params->value('ATime' => $atime) ;
        $params->value('CTime' => undef) ; # No Creation time
    }

    # NOTE - Unix specific code alert
    $params->value('ExtAttr' => $mode << 16) 
        if ! $params->parsed('ExtAttr');

    $params->value('UID' => $uid) ;
    $params->value('GID' => $gid) ;
    
}

sub mkExtendedTime
{
    # order expected is m, a, c

    my $times = '';
    my $bit = 1 ;
    my $flags = 0;

    for my $time (@_)
    {
        if (defined $time)
        {
            $flags |= $bit;
            $times .= pack("V", $time);
        }

        $bit <<= 1 ;
    }

    return IO::Compress::Zlib::Extra::mkSubField(ZIP_EXTRA_ID_EXT_TIMESTAMP,
                                                 pack("C", $flags) .  $times);
}

sub mkUnix2Extra
{
    my $ids = '';
    for my $id (@_)
    {
        $ids .= pack("v", $id);
    }

    return IO::Compress::Zlib::Extra::mkSubField(ZIP_EXTRA_ID_INFO_ZIP_UNIX2, 
                                                 $ids);
}


# from Archive::Zip
sub _unixToDosTime    # Archive::Zip::Member
{
	my $time_t = shift;
    # TODO - add something to cope with unix time < 1980 
	my ( $sec, $min, $hour, $mday, $mon, $year ) = localtime($time_t);
	my $dt = 0;
	$dt += ( $sec >> 1 );
	$dt += ( $min << 5 );
	$dt += ( $hour << 11 );
	$dt += ( $mday << 16 );
	$dt += ( ( $mon + 1 ) << 21 );
	$dt += ( ( $year - 80 ) << 25 );
	return $dt;
}

1;

__END__

=head1 NAME

IO::Compress::Zip - Write zip files/buffers
 
 

=head1 SYNOPSIS

    use IO::Compress::Zip qw(zip $ZipError) ;

    my $status = zip $input => $output [,OPTS] 
        or die "zip failed: $ZipError\n";

    my $z = new IO::Compress::Zip $output [,OPTS]
        or die "zip failed: $ZipError\n";

    $z->print($string);
    $z->printf($format, $string);
    $z->write($string);
    $z->syswrite($string [, $length, $offset]);
    $z->flush();
    $z->tell();
    $z->eof();
    $z->seek($position, $whence);
    $z->binmode();
    $z->fileno();
    $z->opened();
    $z->autoflush();
    $z->input_line_number();
    $z->newStream( [OPTS] );
    
    $z->deflateParams();
    
    $z->close() ;

    $ZipError ;

    # IO::File mode

    print $z $string;
    printf $z $format, $string;
    tell $z
    eof $z
    seek $z, $position, $whence
    binmode $z
    fileno $z
    close $z ;
    

=head1 DESCRIPTION

This module provides a Perl interface that allows writing zip 
compressed data to files or buffer.

The primary purpose of this module is to provide streaming write access to
zip files and buffers. It is not a general-purpose file archiver. If that
is what you want, check out C<Archive::Zip>.

At present three compression methods are supported by IO::Compress::Zip,
namely Store (no compression at all), Deflate and Bzip2.

Note that to create Bzip2 content, the module C<IO::Compress::Bzip2> must
be installed.

For reading zip files/buffers, see the companion module 
L<IO::Uncompress::Unzip|IO::Uncompress::Unzip>.

=head1 Functional Interface

A top-level function, C<zip>, is provided to carry out
"one-shot" compression between buffers and/or files. For finer
control over the compression process, see the L</"OO Interface">
section.

    use IO::Compress::Zip qw(zip $ZipError) ;

    zip $input => $output [,OPTS] 
        or die "zip failed: $ZipError\n";

The functional interface needs Perl5.005 or better.

=head2 zip $input => $output [, OPTS]

C<zip> expects at least two parameters, C<$input> and C<$output>.

=head3 The C<$input> parameter

The parameter, C<$input>, is used to define the source of
the uncompressed data. 

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
contains valid filenames before any data is compressed.

=item An Input FileGlob string

If C<$input> is a string that is delimited by the characters "<" and ">"
C<zip> will assume that it is an I<input fileglob string>. The
input is the list of files that match the fileglob.

If the fileglob does not match any files ...

See L<File::GlobMapper|File::GlobMapper> for more details.

=back

If the C<$input> parameter is any other type, C<undef> will be returned.

In addition, if C<$input> is a simple filename, the default values for
the C<Name>, C<Time>, C<ExtAttr> and C<exTime> options will be sourced from that file.

If you do not want to use these defaults they can be overridden by
explicitly setting the C<Name>, C<Time>, C<ExtAttr> and C<exTime> options or by setting the
C<Minimal> parameter.

=head3 The C<$output> parameter

The parameter C<$output> is used to control the destination of the
compressed data. This parameter can take one of these forms.

=over 5

=item A filename

If the C<$output> parameter is a simple scalar, it is assumed to be a
filename.  This file will be opened for writing and the compressed
data will be written to it.

=item A filehandle

If the C<$output> parameter is a filehandle, the compressed data
will be written to it.
The string '-' can be used as an alias for standard output.

=item A scalar reference 

If C<$output> is a scalar reference, the compressed data will be
stored in C<$$output>.

=item An Array Reference

If C<$output> is an array reference, the compressed data will be
pushed onto the array.

=item An Output FileGlob

If C<$output> is a string that is delimited by the characters "<" and ">"
C<zip> will assume that it is an I<output fileglob string>. The
output is the list of files that match the fileglob.

When C<$output> is an fileglob string, C<$input> must also be a fileglob
string. Anything else is an error.

=back

If the C<$output> parameter is any other type, C<undef> will be returned.

=head2 Notes

When C<$input> maps to multiple files/buffers and C<$output> is a single
file/buffer the input files/buffers will each be stored
in C<$output> as a distinct entry.

=head2 Optional Parameters

Unless specified below, the optional parameters for C<zip>,
C<OPTS>, are the same as those used with the OO interface defined in the
L</"Constructor Options"> section below.

=over 5

=item C<< AutoClose => 0|1 >>

This option applies to any input or output data streams to 
C<zip> that are filehandles.

If C<AutoClose> is specified, and the value is true, it will result in all
input and/or output filehandles being closed once C<zip> has
completed.

This parameter defaults to 0.

=item C<< BinModeIn => 0|1 >>

When reading from a file or filehandle, set C<binmode> before reading.

Defaults to 0.

=item C<< Append => 0|1 >>

TODO

=back

=head2 Examples

To read the contents of the file C<file1.txt> and write the compressed
data to the file C<file1.txt.zip>.

    use strict ;
    use warnings ;
    use IO::Compress::Zip qw(zip $ZipError) ;

    my $input = "file1.txt";
    zip $input => "$input.zip"
        or die "zip failed: $ZipError\n";

To read from an existing Perl filehandle, C<$input>, and write the
compressed data to a buffer, C<$buffer>.

    use strict ;
    use warnings ;
    use IO::Compress::Zip qw(zip $ZipError) ;
    use IO::File ;

    my $input = new IO::File "<file1.txt"
        or die "Cannot open 'file1.txt': $!\n" ;
    my $buffer ;
    zip $input => \$buffer 
        or die "zip failed: $ZipError\n";

To compress all files in the directory "/my/home" that match "*.txt"
and store the compressed data in the same directory

    use strict ;
    use warnings ;
    use IO::Compress::Zip qw(zip $ZipError) ;

    zip '</my/home/*.txt>' => '<*.zip>'
        or die "zip failed: $ZipError\n";

and if you want to compress each file one at a time, this will do the trick

    use strict ;
    use warnings ;
    use IO::Compress::Zip qw(zip $ZipError) ;

    for my $input ( glob "/my/home/*.txt" )
    {
        my $output = "$input.zip" ;
        zip $input => $output 
            or die "Error compressing '$input': $ZipError\n";
    }

=head1 OO Interface

=head2 Constructor

The format of the constructor for C<IO::Compress::Zip> is shown below

    my $z = new IO::Compress::Zip $output [,OPTS]
        or die "IO::Compress::Zip failed: $ZipError\n";

It returns an C<IO::Compress::Zip> object on success and undef on failure. 
The variable C<$ZipError> will contain an error message on failure.

If you are running Perl 5.005 or better the object, C<$z>, returned from 
IO::Compress::Zip can be used exactly like an L<IO::File|IO::File> filehandle. 
This means that all normal output file operations can be carried out 
with C<$z>. 
For example, to write to a compressed file/buffer you can use either of 
these forms

    $z->print("hello world\n");
    print $z "hello world\n";

The mandatory parameter C<$output> is used to control the destination
of the compressed data. This parameter can take one of these forms.

=over 5

=item A filename

If the C<$output> parameter is a simple scalar, it is assumed to be a
filename. This file will be opened for writing and the compressed data
will be written to it.

=item A filehandle

If the C<$output> parameter is a filehandle, the compressed data will be
written to it.
The string '-' can be used as an alias for standard output.

=item A scalar reference 

If C<$output> is a scalar reference, the compressed data will be stored
in C<$$output>.

=back

If the C<$output> parameter is any other type, C<IO::Compress::Zip>::new will
return undef.

=head2 Constructor Options

C<OPTS> is any combination of the following options:

=over 5

=item C<< AutoClose => 0|1 >>

This option is only valid when the C<$output> parameter is a filehandle. If
specified, and the value is true, it will result in the C<$output> being
closed once either the C<close> method is called or the C<IO::Compress::Zip>
object is destroyed.

This parameter defaults to 0.

=item C<< Append => 0|1 >>

Opens C<$output> in append mode. 

The behaviour of this option is dependent on the type of C<$output>.

=over 5

=item * A Buffer

If C<$output> is a buffer and C<Append> is enabled, all compressed data
will be append to the end if C<$output>. Otherwise C<$output> will be
cleared before any data is written to it.

=item * A Filename

If C<$output> is a filename and C<Append> is enabled, the file will be
opened in append mode. Otherwise the contents of the file, if any, will be
truncated before any compressed data is written to it.

=item * A Filehandle

If C<$output> is a filehandle, the file pointer will be positioned to the
end of the file via a call to C<seek> before any compressed data is written
to it.  Otherwise the file pointer will not be moved.

=back

This parameter defaults to 0.

=item C<< Name => $string >>

Stores the contents of C<$string> in the zip filename header field. If
C<Name> is not specified, no zip filename field will be created.

=item C<< Time => $number >>

Sets the last modified time field in the zip header to $number.

This field defaults to the time the C<IO::Compress::Zip> object was created
if this option is not specified.

=item C<< ExtAttr => $attr >>

This option controls the "external file attributes" field in the central
header of the zip file. This is a 4 byte field.

If you are running a Unix derivative this value defaults to 

    0666 << 16

This should allow read/write access to any files that are extracted from
the zip file/buffer.

For all other systems it defaults to 0.

=item C<< exTime => [$atime, $mtime, $ctime] >>

This option expects an array reference with exactly three elements:
C<$atime>, C<mtime> and C<$ctime>. These correspond to the last access
time, last modification time and creation time respectively.

It uses these values to set the extended timestamp field (ID is "UT") in
the local zip header using the three values, $atime, $mtime, $ctime. In
addition it sets the extended timestamp field in the central zip header
using C<$mtime>.

If any of the three values is C<undef> that time value will not be used.
So, for example, to set only the C<$mtime> you would use this

    exTime => [undef, $mtime, undef]

If the C<Minimal> option is set to true, this option will be ignored.

By default no extended time field is created.

=item C<< exUnix2 => [$uid, $gid] >>

This option expects an array reference with exactly two elements: C<$uid>
and C<$gid>. These values correspond to the numeric user ID and group ID
of the owner of the files respectively.

When the C<exUnix2> option is present it will trigger the creation of a
Unix2 extra field (ID is "Ux") in the local zip. This will be populated
with C<$uid> and C<$gid>. In addition an empty Unix2 extra field will also
be created in the central zip header

If the C<Minimal> option is set to true, this option will be ignored.

By default no Unix2 extra field is created.

=item C<< Comment => $comment >>

Stores the contents of C<$comment> in the Central File Header of
the zip file.

By default, no comment field is written to the zip file.

=item C<< ZipComment => $comment >>

Stores the contents of C<$comment> in the End of Central Directory record
of the zip file.

By default, no comment field is written to the zip file.

=item C<< Method => $method >>

Controls which compression method is used. At present three compression
methods are supported, namely Store (no compression at all), Deflate and
Bzip2.

The symbols, ZIP_CM_STORE, ZIP_CM_DEFLATE and ZIP_CM_BZIP2 are used to
select the compression method.

These constants are not imported by C<IO::Compress::Zip> by default.

    use IO::Compress::Zip qw(:zip_method);
    use IO::Compress::Zip qw(:constants);
    use IO::Compress::Zip qw(:all);

Note that to create Bzip2 content, the module C<IO::Compress::Bzip2> must
be installed. A fatal error will be thrown if you attempt to create Bzip2
content when C<IO::Compress::Bzip2> is not available.

The default method is ZIP_CM_DEFLATE.

=item C<< Stream => 0|1 >>

This option controls whether the zip file/buffer output is created in
streaming mode.

Note that when outputting to a file with streaming mode disabled (C<Stream>
is 0), the output file must be seekable.

The default is 1.

=item C<< Zip64 => 0|1 >>

Create a Zip64 zip file/buffer. This option should only be used if you want
to store files larger than 4 Gig.

If you intend to manipulate the Zip64 zip files created with this module
using an external zip/unzip make sure that it supports streaming Zip64.  

In particular, if you are using Info-Zip you need to have zip version 3.x
or better to update a Zip64 archive and unzip version 6.x to read a zip64
archive. At the time of writing both are beta status.

When the C<Zip64> option is enabled, the C<Stream> option I<must> be
enabled as well.

The default is 0.

=item C<< TextFlag => 0|1 >>

This parameter controls the setting of a bit in the zip central header. It
is used to signal that the data stored in the zip file/buffer is probably
text.

The default is 0. 

=item C<< ExtraFieldLocal => $data >>
=item C<< ExtraFieldCentral => $data >>

The C<ExtraFieldLocal> option is used to store additional metadata in the
local header for the zip file/buffer. The C<ExtraFieldCentral> does the
same for the matching central header.

An extra field consists of zero or more subfields. Each subfield consists
of a two byte header followed by the subfield data.

The list of subfields can be supplied in any of the following formats

    ExtraFieldLocal => [$id1, $data1,
                        $id2, $data2,
                         ...
                       ]

    ExtraFieldLocal => [ [$id1 => $data1],
                         [$id2 => $data2],
                         ...
                       ]

    ExtraFieldLocal => { $id1 => $data1,
                         $id2 => $data2,
                         ...
                       }

Where C<$id1>, C<$id2> are two byte subfield ID's. 

If you use the hash syntax, you have no control over the order in which
the ExtraSubFields are stored, plus you cannot have SubFields with
duplicate ID.

Alternatively the list of subfields can by supplied as a scalar, thus

    ExtraField => $rawdata

The Extended Time field (ID "UT"), set using the C<exTime> option, and the
Unix2 extra field (ID "Ux), set using the C<exUnix2> option, are examples
of extra fields.

If the C<Minimal> option is set to true, this option will be ignored.

The maximum size of an extra field 65535 bytes.

=item C<< Minimal => 1|0 >>

If specified, this option will disable the creation of all extra fields
in the zip local and central headers. So the C<exTime>, C<exUnix2>,
C<ExtraFieldLocal> and C<ExtraFieldCentral> options will be ignored.

This parameter defaults to 0.

=item C<< BlockSize100K => number >>

Specify the number of 100K blocks bzip2 uses during compression. 

Valid values are from 1 to 9, where 9 is best compression.

This option is only valid if the C<Method> is ZIP_CM_BZIP2. It is ignored
otherwise.

The default is 1.

=item C<< WorkFactor => number >>

Specifies how much effort bzip2 should take before resorting to a slower
fallback compression algorithm.

Valid values range from 0 to 250, where 0 means use the default value 30.

This option is only valid if the C<Method> is ZIP_CM_BZIP2. It is ignored
otherwise.

The default is 0.

=item -Level 

Defines the compression level used by zlib. The value should either be
a number between 0 and 9 (0 means no compression and 9 is maximum
compression), or one of the symbolic constants defined below.

   Z_NO_COMPRESSION
   Z_BEST_SPEED
   Z_BEST_COMPRESSION
   Z_DEFAULT_COMPRESSION

The default is Z_DEFAULT_COMPRESSION.

Note, these constants are not imported by C<IO::Compress::Zip> by default.

    use IO::Compress::Zip qw(:strategy);
    use IO::Compress::Zip qw(:constants);
    use IO::Compress::Zip qw(:all);

=item -Strategy 

Defines the strategy used to tune the compression. Use one of the symbolic
constants defined below.

   Z_FILTERED
   Z_HUFFMAN_ONLY
   Z_RLE
   Z_FIXED
   Z_DEFAULT_STRATEGY

The default is Z_DEFAULT_STRATEGY.

=item C<< Strict => 0|1 >>

This is a placeholder option.

=back

=head2 Examples

TODO

=head1 Methods 

=head2 print

Usage is

    $z->print($data)
    print $z $data

Compresses and outputs the contents of the C<$data> parameter. This
has the same behaviour as the C<print> built-in.

Returns true if successful.

=head2 printf

Usage is

    $z->printf($format, $data)
    printf $z $format, $data

Compresses and outputs the contents of the C<$data> parameter.

Returns true if successful.

=head2 syswrite

Usage is

    $z->syswrite $data
    $z->syswrite $data, $length
    $z->syswrite $data, $length, $offset

Compresses and outputs the contents of the C<$data> parameter.

Returns the number of uncompressed bytes written, or C<undef> if
unsuccessful.

=head2 write

Usage is

    $z->write $data
    $z->write $data, $length
    $z->write $data, $length, $offset

Compresses and outputs the contents of the C<$data> parameter.

Returns the number of uncompressed bytes written, or C<undef> if
unsuccessful.

=head2 flush

Usage is

    $z->flush;
    $z->flush($flush_type);

Flushes any pending compressed data to the output file/buffer.

This method takes an optional parameter, C<$flush_type>, that controls
how the flushing will be carried out. By default the C<$flush_type>
used is C<Z_FINISH>. Other valid values for C<$flush_type> are
C<Z_NO_FLUSH>, C<Z_SYNC_FLUSH>, C<Z_FULL_FLUSH> and C<Z_BLOCK>. It is
strongly recommended that you only set the C<flush_type> parameter if
you fully understand the implications of what it does - overuse of C<flush>
can seriously degrade the level of compression achieved. See the C<zlib>
documentation for details.

Returns true on success.

=head2 tell

Usage is

    $z->tell()
    tell $z

Returns the uncompressed file offset.

=head2 eof

Usage is

    $z->eof();
    eof($z);

Returns true if the C<close> method has been called.

=head2 seek

    $z->seek($position, $whence);
    seek($z, $position, $whence);

Provides a sub-set of the C<seek> functionality, with the restriction
that it is only legal to seek forward in the output file/buffer.
It is a fatal error to attempt to seek backward.

Empty parts of the file/buffer will have NULL (0x00) bytes written to them.

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

This method always returns C<undef> when compressing. 

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

Flushes any pending compressed data and then closes the output file/buffer. 

For most versions of Perl this method will be automatically invoked if
the IO::Compress::Zip object is destroyed (either explicitly or by the
variable with the reference to the object going out of scope). The
exceptions are Perl versions 5.005 through 5.00504 and 5.8.0. In
these cases, the C<close> method will be called automatically, but
not until global destruction of all live objects when the program is
terminating.

Therefore, if you want your scripts to be able to run on all versions
of Perl, you should call C<close> explicitly and not rely on automatic
closing.

Returns true on success, otherwise 0.

If the C<AutoClose> option has been enabled when the IO::Compress::Zip
object was created, and the object is associated with a file, the
underlying file will also be closed.

=head2 newStream([OPTS])

Usage is

    $z->newStream( [OPTS] )

Closes the current compressed data stream and starts a new one.

OPTS consists of any of the the options that are available when creating
the C<$z> object.

See the L</"Constructor Options"> section for more details.

=head2 deflateParams

Usage is

    $z->deflateParams

TODO

=head1 Importing 

A number of symbolic constants are required by some methods in 
C<IO::Compress::Zip>. None are imported by default.

=over 5

=item :all

Imports C<zip>, C<$ZipError> and all symbolic
constants that can be used by C<IO::Compress::Zip>. Same as doing this

    use IO::Compress::Zip qw(zip $ZipError :constants) ;

=item :constants

Import all symbolic constants. Same as doing this

    use IO::Compress::Zip qw(:flush :level :strategy :zip_method) ;

=item :flush

These symbolic constants are used by the C<flush> method.

    Z_NO_FLUSH
    Z_PARTIAL_FLUSH
    Z_SYNC_FLUSH
    Z_FULL_FLUSH
    Z_FINISH
    Z_BLOCK

=item :level

These symbolic constants are used by the C<Level> option in the constructor.

    Z_NO_COMPRESSION
    Z_BEST_SPEED
    Z_BEST_COMPRESSION
    Z_DEFAULT_COMPRESSION

=item :strategy

These symbolic constants are used by the C<Strategy> option in the constructor.

    Z_FILTERED
    Z_HUFFMAN_ONLY
    Z_RLE
    Z_FIXED
    Z_DEFAULT_STRATEGY

=item :zip_method

These symbolic constants are used by the C<Method> option in the
constructor.

    ZIP_CM_STORE
    ZIP_CM_DEFLATE
    ZIP_CM_BZIP2

    
    

=back

=head1 EXAMPLES

=head2 Apache::GZip Revisited

See L<IO::Compress::Zlib::FAQ|IO::Compress::Zlib::FAQ/"Apache::GZip Revisited">

    

=head2 Working with Net::FTP

See L<IO::Compress::Zlib::FAQ|IO::Compress::Zlib::FAQ/"Compressed files and Net::FTP">

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

