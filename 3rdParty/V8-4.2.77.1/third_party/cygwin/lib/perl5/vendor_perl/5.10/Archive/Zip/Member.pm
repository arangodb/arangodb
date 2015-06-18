package Archive::Zip::Member;

# A generic membet of an archive

use strict;
use vars qw( $VERSION @ISA );

BEGIN {
    $VERSION = '1.23';
    @ISA     = qw( Archive::Zip );
}

use Archive::Zip qw(
  :CONSTANTS
  :MISC_CONSTANTS
  :ERROR_CODES
  :PKZIP_CONSTANTS
  :UTILITY_METHODS
);

use Time::Local ();
use Compress::Zlib qw( Z_OK Z_STREAM_END MAX_WBITS );
use File::Path;
use File::Basename;

use constant ZIPFILEMEMBERCLASS   => 'Archive::Zip::ZipFileMember';
use constant NEWFILEMEMBERCLASS   => 'Archive::Zip::NewFileMember';
use constant STRINGMEMBERCLASS    => 'Archive::Zip::StringMember';
use constant DIRECTORYMEMBERCLASS => 'Archive::Zip::DirectoryMember';

# Unix perms for default creation of files/dirs.
use constant DEFAULT_DIRECTORY_PERMISSIONS => 040755;
use constant DEFAULT_FILE_PERMISSIONS      => 0100666;
use constant DIRECTORY_ATTRIB              => 040000;
use constant FILE_ATTRIB                   => 0100000;

# Returns self if successful, else undef
# Assumes that fh is positioned at beginning of central directory file header.
# Leaves fh positioned immediately after file header or EOCD signature.
sub _newFromZipFile {
    my $class = shift;
    my $self  = $class->ZIPFILEMEMBERCLASS->_newFromZipFile(@_);
    return $self;
}

sub newFromString {
    my $class = shift;
    my $self  = $class->STRINGMEMBERCLASS->_newFromString(@_);
    return $self;
}

sub newFromFile {
    my $class = shift;
    my $self  = $class->NEWFILEMEMBERCLASS->_newFromFileNamed(@_);
    return $self;
}

sub newDirectoryNamed {
    my $class = shift;
    my $self  = $class->DIRECTORYMEMBERCLASS->_newNamed(@_);
    return $self;
}

sub new {
    my $class = shift;
    my $self  = {
        'lastModFileDateTime'      => 0,
        'fileAttributeFormat'      => FA_UNIX,
        'versionMadeBy'            => 20,
        'versionNeededToExtract'   => 20,
        'bitFlag'                  => 0,
        'compressionMethod'        => COMPRESSION_STORED,
        'desiredCompressionMethod' => COMPRESSION_STORED,
        'desiredCompressionLevel'  => COMPRESSION_LEVEL_NONE,
        'internalFileAttributes'   => 0,
        'externalFileAttributes'   => 0,                        # set later
        'fileName'                 => '',
        'cdExtraField'             => '',
        'localExtraField'          => '',
        'fileComment'              => '',
        'crc32'                    => 0,
        'compressedSize'           => 0,
        'uncompressedSize'         => 0,
        @_
    };
    bless( $self, $class );
    $self->unixFileAttributes( $self->DEFAULT_FILE_PERMISSIONS );
    return $self;
}

sub _becomeDirectoryIfNecessary {
    my $self = shift;
    $self->_become(DIRECTORYMEMBERCLASS)
      if $self->isDirectory();
    return $self;
}

# Morph into given class (do whatever cleanup I need to do)
sub _become {
    return bless( $_[0], $_[1] );
}

sub versionMadeBy {
    shift->{'versionMadeBy'};
}

sub fileAttributeFormat {
    ( $#_ > 0 )
      ? ( $_[0]->{'fileAttributeFormat'} = $_[1] )
      : $_[0]->{'fileAttributeFormat'};
}

sub versionNeededToExtract {
    shift->{'versionNeededToExtract'};
}

sub bitFlag {
    shift->{'bitFlag'};
}

sub compressionMethod {
    shift->{'compressionMethod'};
}

sub desiredCompressionMethod {
    my $self                        = shift;
    my $newDesiredCompressionMethod = shift;
    my $oldDesiredCompressionMethod = $self->{'desiredCompressionMethod'};
    if ( defined($newDesiredCompressionMethod) ) {
        $self->{'desiredCompressionMethod'} = $newDesiredCompressionMethod;
        if ( $newDesiredCompressionMethod == COMPRESSION_STORED ) {
            $self->{'desiredCompressionLevel'} = 0;
            $self->{'bitFlag'} &= ~GPBF_HAS_DATA_DESCRIPTOR_MASK;

        } elsif ( $oldDesiredCompressionMethod == COMPRESSION_STORED ) {
            $self->{'desiredCompressionLevel'} = COMPRESSION_LEVEL_DEFAULT;
        }
    }
    return $oldDesiredCompressionMethod;
}

sub desiredCompressionLevel {
    my $self                       = shift;
    my $newDesiredCompressionLevel = shift;
    my $oldDesiredCompressionLevel = $self->{'desiredCompressionLevel'};
    if ( defined($newDesiredCompressionLevel) ) {
        $self->{'desiredCompressionLevel'}  = $newDesiredCompressionLevel;
        $self->{'desiredCompressionMethod'} = (
            $newDesiredCompressionLevel
            ? COMPRESSION_DEFLATED
            : COMPRESSION_STORED
        );
    }
    return $oldDesiredCompressionLevel;
}

sub fileName {
    my $self    = shift;
    my $newName = shift;
    if ($newName) {
        $newName =~ s{[\\/]+}{/}g;    # deal with dos/windoze problems
        $self->{'fileName'} = $newName;
    }
    return $self->{'fileName'};
}

sub lastModFileDateTime {
    my $modTime = shift->{'lastModFileDateTime'};
    $modTime =~ m/^(\d+)$/;           # untaint
    return $1;
}

sub lastModTime {
    my $self = shift;
    return _dosToUnixTime( $self->lastModFileDateTime() );
}

sub setLastModFileDateTimeFromUnix {
    my $self   = shift;
    my $time_t = shift;
    $self->{'lastModFileDateTime'} = _unixToDosTime($time_t);
}

sub internalFileAttributes {
    shift->{'internalFileAttributes'};
}

sub externalFileAttributes {
    shift->{'externalFileAttributes'};
}

# Convert UNIX permissions into proper value for zip file
# NOT A METHOD!
sub _mapPermissionsFromUnix {
    my $perms = shift;
    return $perms << 16;

    # TODO: map MS-DOS perms too (RHSA?)
}

# Convert ZIP permissions into Unix ones
#
# This was taken from Info-ZIP group's portable UnZip
# zipfile-extraction program, version 5.50.
# http://www.info-zip.org/pub/infozip/
#
# See the mapattr() function in unix/unix.c
# See the attribute format constants in unzpriv.h
#
# XXX Note that there's one situation that isn't implemented
# yet that depends on the "extra field."
sub _mapPermissionsToUnix {
    my $self = shift;

    my $format  = $self->{'fileAttributeFormat'};
    my $attribs = $self->{'externalFileAttributes'};

    my $mode = 0;

    if ( $format == FA_AMIGA ) {
        $attribs = $attribs >> 17 & 7;                         # Amiga RWE bits
        $mode    = $attribs << 6 | $attribs << 3 | $attribs;
        return $mode;
    }

    if ( $format == FA_THEOS ) {
        $attribs &= 0xF1FFFFFF;
        if ( ( $attribs & 0xF0000000 ) != 0x40000000 ) {
            $attribs &= 0x01FFFFFF;    # not a dir, mask all ftype bits
        }
        else {
            $attribs &= 0x41FFFFFF;    # leave directory bit as set
        }
    }

    if (   $format == FA_UNIX
        || $format == FA_VAX_VMS
        || $format == FA_ACORN
        || $format == FA_ATARI_ST
        || $format == FA_BEOS
        || $format == FA_QDOS
        || $format == FA_TANDEM )
    {
        $mode = $attribs >> 16;
        return $mode if $mode != 0 or not $self->localExtraField;

        # warn("local extra field is: ", $self->localExtraField, "\n");

        # XXX This condition is not implemented
        # I'm just including the comments from the info-zip section for now.

        # Some (non-Info-ZIP) implementations of Zip for Unix and
        # VMS (and probably others ??) leave 0 in the upper 16-bit
        # part of the external_file_attributes field. Instead, they
        # store file permission attributes in some extra field.
        # As a work-around, we search for the presence of one of
        # these extra fields and fall back to the MSDOS compatible
        # part of external_file_attributes if one of the known
        # e.f. types has been detected.
        # Later, we might implement extraction of the permission
        # bits from the VMS extra field. But for now, the work-around
        # should be sufficient to provide "readable" extracted files.
        # (For ASI Unix e.f., an experimental remap from the e.f.
        # mode value IS already provided!)
    }

    # PKWARE's PKZip for Unix marks entries as FA_MSDOS, but stores the
    # Unix attributes in the upper 16 bits of the external attributes
    # field, just like Info-ZIP's Zip for Unix.  We try to use that
    # value, after a check for consistency with the MSDOS attribute
    # bits (see below).
    if ( $format == FA_MSDOS ) {
        $mode = $attribs >> 16;
    }

    # FA_MSDOS, FA_OS2_HPFS, FA_WINDOWS_NTFS, FA_MACINTOSH, FA_TOPS20
    $attribs = !( $attribs & 1 ) << 1 | ( $attribs & 0x10 ) >> 4;

    # keep previous $mode setting when its "owner"
    # part appears to be consistent with DOS attribute flags!
    return $mode if ( $mode & 0700 ) == ( 0400 | $attribs << 6 );
    $mode = 0444 | $attribs << 6 | $attribs << 3 | $attribs;
    return $mode;
}

sub unixFileAttributes {
    my $self     = shift;
    my $oldPerms = $self->_mapPermissionsToUnix();
    if (@_) {
        my $perms = shift;
        if ( $self->isDirectory() ) {
            $perms &= ~FILE_ATTRIB;
            $perms |= DIRECTORY_ATTRIB;
        }
        else {
            $perms &= ~DIRECTORY_ATTRIB;
            $perms |= FILE_ATTRIB;
        }
        $self->{'externalFileAttributes'} = _mapPermissionsFromUnix($perms);
    }
    return $oldPerms;
}

sub localExtraField {
    ( $#_ > 0 )
      ? ( $_[0]->{'localExtraField'} = $_[1] )
      : $_[0]->{'localExtraField'};
}

sub cdExtraField {
    ( $#_ > 0 ) ? ( $_[0]->{'cdExtraField'} = $_[1] ) : $_[0]->{'cdExtraField'};
}

sub extraFields {
    my $self = shift;
    return $self->localExtraField() . $self->cdExtraField();
}

sub fileComment {
    ( $#_ > 0 )
      ? ( $_[0]->{'fileComment'} = pack( 'C0a*', $_[1] ) )
      : $_[0]->{'fileComment'};
}

sub hasDataDescriptor {
    my $self = shift;
    if (@_) {
        my $shouldHave = shift;
        if ($shouldHave) {
            $self->{'bitFlag'} |= GPBF_HAS_DATA_DESCRIPTOR_MASK;
        }
        else {
            $self->{'bitFlag'} &= ~GPBF_HAS_DATA_DESCRIPTOR_MASK;
        }
    }
    return $self->{'bitFlag'} & GPBF_HAS_DATA_DESCRIPTOR_MASK;
}

sub crc32 {
    shift->{'crc32'};
}

sub crc32String {
    sprintf( "%08x", shift->{'crc32'} );
}

sub compressedSize {
    shift->{'compressedSize'};
}

sub uncompressedSize {
    shift->{'uncompressedSize'};
}

sub isEncrypted {
    shift->bitFlag() & GPBF_ENCRYPTED_MASK;
}

sub isTextFile {
    my $self = shift;
    my $bit  = $self->internalFileAttributes() & IFA_TEXT_FILE_MASK;
    if (@_) {
        my $flag = shift;
        $self->{'internalFileAttributes'} &= ~IFA_TEXT_FILE_MASK;
        $self->{'internalFileAttributes'} |=
          ( $flag ? IFA_TEXT_FILE: IFA_BINARY_FILE );
    }
    return $bit == IFA_TEXT_FILE;
}

sub isBinaryFile {
    my $self = shift;
    my $bit  = $self->internalFileAttributes() & IFA_TEXT_FILE_MASK;
    if (@_) {
        my $flag = shift;
        $self->{'internalFileAttributes'} &= ~IFA_TEXT_FILE_MASK;
        $self->{'internalFileAttributes'} |=
          ( $flag ? IFA_BINARY_FILE: IFA_TEXT_FILE );
    }
    return $bit == IFA_BINARY_FILE;
}

sub extractToFileNamed {
    my $self = shift;
    my $name = shift;    # local FS name
    return _error("encryption unsupported") if $self->isEncrypted();
    mkpath( dirname($name) );    # croaks on error
    my ( $status, $fh ) = _newFileHandle( $name, 'w' );
    return _ioError("Can't open file $name for write") unless $status;
    my $retval = $self->extractToFileHandle($fh);
    $fh->close();
    utime( $self->lastModTime(), $self->lastModTime(), $name );
    return $retval;
}

sub isDirectory {
    return 0;
}

sub externalFileName {
    return undef;
}

# The following are used when copying data
sub _writeOffset {
    shift->{'writeOffset'};
}

sub _readOffset {
    shift->{'readOffset'};
}

sub writeLocalHeaderRelativeOffset {
    shift->{'writeLocalHeaderRelativeOffset'};
}

sub wasWritten { shift->{'wasWritten'} }

sub _dataEnded {
    shift->{'dataEnded'};
}

sub _readDataRemaining {
    shift->{'readDataRemaining'};
}

sub _inflater {
    shift->{'inflater'};
}

sub _deflater {
    shift->{'deflater'};
}

# Return the total size of my local header
sub _localHeaderSize {
    my $self = shift;
    return SIGNATURE_LENGTH + LOCAL_FILE_HEADER_LENGTH +
      length( $self->fileName() ) + length( $self->localExtraField() );
}

# Return the total size of my CD header
sub _centralDirectoryHeaderSize {
    my $self = shift;
    return SIGNATURE_LENGTH + CENTRAL_DIRECTORY_FILE_HEADER_LENGTH +
      length( $self->fileName() ) + length( $self->cdExtraField() ) +
      length( $self->fileComment() );
}

# DOS date/time format
# 0-4 (5) Second divided by 2
# 5-10 (6) Minute (0-59)
# 11-15 (5) Hour (0-23 on a 24-hour clock)
# 16-20 (5) Day of the month (1-31)
# 21-24 (4) Month (1 = January, 2 = February, etc.)
# 25-31 (7) Year offset from 1980 (add 1980 to get actual year)

# Convert DOS date/time format to unix time_t format
# NOT AN OBJECT METHOD!
sub _dosToUnixTime {
    my $dt = shift;
    return time() unless defined($dt);

    my $year = ( ( $dt >> 25 ) & 0x7f ) + 80;
    my $mon  = ( ( $dt >> 21 ) & 0x0f ) - 1;
    my $mday = ( ( $dt >> 16 ) & 0x1f );

    my $hour = ( ( $dt >> 11 ) & 0x1f );
    my $min  = ( ( $dt >> 5 ) & 0x3f );
    my $sec  = ( ( $dt << 1 ) & 0x3e );

    # catch errors
    my $time_t =
      eval { Time::Local::timelocal( $sec, $min, $hour, $mday, $mon, $year ); };
    return time() if ($@);
    return $time_t;
}

# Note, this isn't exactly UTC 1980, it's 1980 + 12 hours and 1
# minute so that nothing timezoney can muck us up.
my $safe_epoch = 315576060;

# convert a unix time to DOS date/time
# NOT AN OBJECT METHOD!
sub _unixToDosTime {
    my $time_t = shift;
    unless ($time_t) {
        _error("Tried to add member with zero or undef value for time");
        $time_t = $safe_epoch;
    }
    if ( $time_t < $safe_epoch ) {
        _ioError("Unsupported date before 1980 encountered, moving to 1980");
        $time_t = $safe_epoch;
    }
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

# Write my local header to a file handle.
# Stores the offset to the start of the header in my
# writeLocalHeaderRelativeOffset member.
# Returns AZ_OK on success.
sub _writeLocalFileHeader {
    my $self = shift;
    my $fh   = shift;

    my $signatureData = pack( SIGNATURE_FORMAT, LOCAL_FILE_HEADER_SIGNATURE );
    $fh->print($signatureData)
      or return _ioError("writing local header signature");

    my $header = pack(
        LOCAL_FILE_HEADER_FORMAT,
        $self->versionNeededToExtract(),
        $self->bitFlag(),
        $self->desiredCompressionMethod(),
        $self->lastModFileDateTime(),
        $self->crc32(),
        $self->compressedSize(),    # may need to be re-written later
        $self->uncompressedSize(),
        length( $self->fileName() ),
        length( $self->localExtraField() )
    );

    $fh->print($header) or return _ioError("writing local header");
    if ( $self->fileName() ) {
        $fh->print( $self->fileName() )
          or return _ioError("writing local header filename");
    }
    if ( $self->localExtraField() ) {
        $fh->print( $self->localExtraField() )
          or return _ioError("writing local extra field");
    }

    return AZ_OK;
}

sub _writeCentralDirectoryFileHeader {
    my $self = shift;
    my $fh   = shift;

    my $sigData =
      pack( SIGNATURE_FORMAT, CENTRAL_DIRECTORY_FILE_HEADER_SIGNATURE );
    $fh->print($sigData)
      or return _ioError("writing central directory header signature");

    my $fileNameLength    = length( $self->fileName() );
    my $extraFieldLength  = length( $self->cdExtraField() );
    my $fileCommentLength = length( $self->fileComment() );

    my $header = pack(
        CENTRAL_DIRECTORY_FILE_HEADER_FORMAT,
        $self->versionMadeBy(),
        $self->fileAttributeFormat(),
        $self->versionNeededToExtract(),
        $self->bitFlag(),
        $self->desiredCompressionMethod(),
        $self->lastModFileDateTime(),
        $self->crc32(),            # these three fields should have been updated
        $self->_writeOffset(),     # by writing the data stream out
        $self->uncompressedSize(), #
        $fileNameLength,
        $extraFieldLength,
        $fileCommentLength,
        0,                         # {'diskNumberStart'},
        $self->internalFileAttributes(),
        $self->externalFileAttributes(),
        $self->writeLocalHeaderRelativeOffset()
    );

    $fh->print($header)
      or return _ioError("writing central directory header");
    if ($fileNameLength) {
        $fh->print( $self->fileName() )
          or return _ioError("writing central directory header signature");
    }
    if ($extraFieldLength) {
        $fh->print( $self->cdExtraField() )
          or return _ioError("writing central directory extra field");
    }
    if ($fileCommentLength) {
        $fh->print( $self->fileComment() )
          or return _ioError("writing central directory file comment");
    }

    return AZ_OK;
}

# This writes a data descriptor to the given file handle.
# Assumes that crc32, writeOffset, and uncompressedSize are
# set correctly (they should be after a write).
# Further, the local file header should have the
# GPBF_HAS_DATA_DESCRIPTOR_MASK bit set.
sub _writeDataDescriptor {
    my $self   = shift;
    my $fh     = shift;
    my $header = pack(
        SIGNATURE_FORMAT . DATA_DESCRIPTOR_FORMAT,
        DATA_DESCRIPTOR_SIGNATURE,
        $self->crc32(),
        $self->_writeOffset(),    # compressed size
        $self->uncompressedSize()
    );

    $fh->print($header)
      or return _ioError("writing data descriptor");
    return AZ_OK;
}

# Re-writes the local file header with new crc32 and compressedSize fields.
# To be called after writing the data stream.
# Assumes that filename and extraField sizes didn't change since last written.
sub _refreshLocalFileHeader {
    my $self = shift;
    my $fh   = shift;

    my $here = $fh->tell();
    $fh->seek( $self->writeLocalHeaderRelativeOffset() + SIGNATURE_LENGTH,
        IO::Seekable::SEEK_SET )
      or return _ioError("seeking to rewrite local header");

    my $header = pack(
        LOCAL_FILE_HEADER_FORMAT,
        $self->versionNeededToExtract(),
        $self->bitFlag(),
        $self->desiredCompressionMethod(),
        $self->lastModFileDateTime(),
        $self->crc32(),
        $self->_writeOffset(),    # compressed size
        $self->uncompressedSize(),
        length( $self->fileName() ),
        length( $self->localExtraField() )
    );

    $fh->print($header)
      or return _ioError("re-writing local header");
    $fh->seek( $here, IO::Seekable::SEEK_SET )
      or return _ioError("seeking after rewrite of local header");

    return AZ_OK;
}

sub readChunk {
    my ( $self, $chunkSize ) = @_;

    if ( $self->readIsDone() ) {
        $self->endRead();
        my $dummy = '';
        return ( \$dummy, AZ_STREAM_END );
    }

    $chunkSize = $Archive::Zip::ChunkSize if not defined($chunkSize);
    $chunkSize = $self->_readDataRemaining()
      if $chunkSize > $self->_readDataRemaining();

    my $buffer = '';
    my $outputRef;
    my ( $bytesRead, $status ) = $self->_readRawChunk( \$buffer, $chunkSize );
    return ( \$buffer, $status ) unless $status == AZ_OK;

    $self->{'readDataRemaining'} -= $bytesRead;
    $self->{'readOffset'} += $bytesRead;

    if ( $self->compressionMethod() == COMPRESSION_STORED ) {
        $self->{'crc32'} = $self->computeCRC32( $buffer, $self->{'crc32'} );
    }

    ( $outputRef, $status ) = &{ $self->{'chunkHandler'} }( $self, \$buffer );
    $self->{'writeOffset'} += length($$outputRef);

    $self->endRead()
      if $self->readIsDone();

    return ( $outputRef, $status );
}

# Read the next raw chunk of my data. Subclasses MUST implement.
#	my ( $bytesRead, $status) = $self->_readRawChunk( \$buffer, $chunkSize );
sub _readRawChunk {
    my $self = shift;
    return $self->_subclassResponsibility();
}

# A place holder to catch rewindData errors if someone ignores
# the error code.
sub _noChunk {
    my $self = shift;
    return ( \undef, _error("trying to copy chunk when init failed") );
}

# Basically a no-op so that I can have a consistent interface.
# ( $outputRef, $status) = $self->_copyChunk( \$buffer );
sub _copyChunk {
    my ( $self, $dataRef ) = @_;
    return ( $dataRef, AZ_OK );
}

# ( $outputRef, $status) = $self->_deflateChunk( \$buffer );
sub _deflateChunk {
    my ( $self, $buffer ) = @_;
    my ( $out,  $status ) = $self->_deflater()->deflate($buffer);

    if ( $self->_readDataRemaining() == 0 ) {
        my $extraOutput;
        ( $extraOutput, $status ) = $self->_deflater()->flush();
        $out .= $extraOutput;
        $self->endRead();
        return ( \$out, AZ_STREAM_END );
    }
    elsif ( $status == Z_OK ) {
        return ( \$out, AZ_OK );
    }
    else {
        $self->endRead();
        my $retval = _error( 'deflate error', $status );
        my $dummy = '';
        return ( \$dummy, $retval );
    }
}

# ( $outputRef, $status) = $self->_inflateChunk( \$buffer );
sub _inflateChunk {
    my ( $self, $buffer ) = @_;
    my ( $out,  $status ) = $self->_inflater()->inflate($buffer);
    my $retval;
    $self->endRead() unless $status == Z_OK;
    if ( $status == Z_OK || $status == Z_STREAM_END ) {
        $retval = ( $status == Z_STREAM_END ) ? AZ_STREAM_END: AZ_OK;
        return ( \$out, $retval );
    }
    else {
        $retval = _error( 'inflate error', $status );
        my $dummy = '';
        return ( \$dummy, $retval );
    }
}

sub rewindData {
    my $self = shift;
    my $status;

    # set to trap init errors
    $self->{'chunkHandler'} = $self->can('_noChunk');

    # Work around WinZip bug with 0-length DEFLATED files
    $self->desiredCompressionMethod(COMPRESSION_STORED)
      if $self->uncompressedSize() == 0;

    # assume that we're going to read the whole file, and compute the CRC anew.
    $self->{'crc32'} = 0
      if ( $self->compressionMethod() == COMPRESSION_STORED );

    # These are the only combinations of methods we deal with right now.
    if (    $self->compressionMethod() == COMPRESSION_STORED
        and $self->desiredCompressionMethod() == COMPRESSION_DEFLATED )
    {
        ( $self->{'deflater'}, $status ) = Compress::Zlib::deflateInit(
            '-Level'      => $self->desiredCompressionLevel(),
            '-WindowBits' => -MAX_WBITS(),                     # necessary magic
            '-Bufsize'    => $Archive::Zip::ChunkSize,
            @_
        );    # pass additional options
        return _error( 'deflateInit error:', $status )
          unless $status == Z_OK;
        $self->{'chunkHandler'} = $self->can('_deflateChunk');
    }
    elsif ( $self->compressionMethod() == COMPRESSION_DEFLATED
        and $self->desiredCompressionMethod() == COMPRESSION_STORED )
    {
        ( $self->{'inflater'}, $status ) = Compress::Zlib::inflateInit(
            '-WindowBits' => -MAX_WBITS(),               # necessary magic
            '-Bufsize'    => $Archive::Zip::ChunkSize,
            @_
        );    # pass additional options
        return _error( 'inflateInit error:', $status )
          unless $status == Z_OK;
        $self->{'chunkHandler'} = $self->can('_inflateChunk');
    }
    elsif ( $self->compressionMethod() == $self->desiredCompressionMethod() ) {
        $self->{'chunkHandler'} = $self->can('_copyChunk');
    }
    else {
        return _error(
            sprintf(
                "Unsupported compression combination: read %d, write %d",
                $self->compressionMethod(),
                $self->desiredCompressionMethod()
            )
        );
    }

    $self->{'readDataRemaining'} =
      ( $self->compressionMethod() == COMPRESSION_STORED )
      ? $self->uncompressedSize()
      : $self->compressedSize();
    $self->{'dataEnded'}  = 0;
    $self->{'readOffset'} = 0;

    return AZ_OK;
}

sub endRead {
    my $self = shift;
    delete $self->{'inflater'};
    delete $self->{'deflater'};
    $self->{'dataEnded'}         = 1;
    $self->{'readDataRemaining'} = 0;
    return AZ_OK;
}

sub readIsDone {
    my $self = shift;
    return ( $self->_dataEnded() or !$self->_readDataRemaining() );
}

sub contents {
    my $self        = shift;
    my $newContents = shift;

    if ( defined($newContents) ) {

        # change our type and call the subclass contents method.
        $self->_become(STRINGMEMBERCLASS);
        return $self->contents( pack( 'C0a*', $newContents ) )
          ;    # in case of Unicode
    }
    else {
        my $oldCompression =
          $self->desiredCompressionMethod(COMPRESSION_STORED);
        my $status = $self->rewindData(@_);
        if ( $status != AZ_OK ) {
            $self->endRead();
            return $status;
        }
        my $retval = '';
        while ( $status == AZ_OK ) {
            my $ref;
            ( $ref, $status ) = $self->readChunk( $self->_readDataRemaining() );

            # did we get it in one chunk?
            if ( length($$ref) == $self->uncompressedSize() ) {
                $retval = $$ref;
            }
            else { $retval .= $$ref }
        }
        $self->desiredCompressionMethod($oldCompression);
        $self->endRead();
        $status = AZ_OK if $status == AZ_STREAM_END;
        $retval = undef unless $status == AZ_OK;
        return wantarray ? ( $retval, $status ) : $retval;
    }
}

sub extractToFileHandle {
    my $self = shift;
    return _error("encryption unsupported") if $self->isEncrypted();
    my $fh = shift;
    _binmode($fh);
    my $oldCompression = $self->desiredCompressionMethod(COMPRESSION_STORED);
    my $status         = $self->rewindData(@_);
    $status = $self->_writeData($fh) if $status == AZ_OK;
    $self->desiredCompressionMethod($oldCompression);
    $self->endRead();
    return $status;
}

# write local header and data stream to file handle
sub _writeToFileHandle {
    my $self         = shift;
    my $fh           = shift;
    my $fhIsSeekable = shift;
    my $offset       = shift;

    return _error("no member name given for $self")
      unless $self->fileName();

    $self->{'writeLocalHeaderRelativeOffset'} = $offset;
    $self->{'wasWritten'}                     = 0;

    # Determine if I need to write a data descriptor
    # I need to do this if I can't refresh the header
    # and I don't know compressed size or crc32 fields.
    my $headerFieldsUnknown = (
        ( $self->uncompressedSize() > 0 )
          and ($self->compressionMethod() == COMPRESSION_STORED
            or $self->desiredCompressionMethod() == COMPRESSION_DEFLATED )
    );

    my $shouldWriteDataDescriptor =
      ( $headerFieldsUnknown and not $fhIsSeekable );

    $self->hasDataDescriptor(1)
      if ($shouldWriteDataDescriptor);

    $self->{'writeOffset'} = 0;

    my $status = $self->rewindData();
    ( $status = $self->_writeLocalFileHeader($fh) )
      if $status == AZ_OK;
    ( $status = $self->_writeData($fh) )
      if $status == AZ_OK;
    if ( $status == AZ_OK ) {
        $self->{'wasWritten'} = 1;
        if ( $self->hasDataDescriptor() ) {
            $status = $self->_writeDataDescriptor($fh);
        }
        elsif ($headerFieldsUnknown) {
            $status = $self->_refreshLocalFileHeader($fh);
        }
    }

    return $status;
}

# Copy my (possibly compressed) data to given file handle.
# Returns C<AZ_OK> on success
sub _writeData {
    my $self    = shift;
    my $writeFh = shift;

    return AZ_OK if ( $self->uncompressedSize() == 0 );
    my $status;
    my $chunkSize = $Archive::Zip::ChunkSize;
    while ( $self->_readDataRemaining() > 0 ) {
        my $outRef;
        ( $outRef, $status ) = $self->readChunk($chunkSize);
        return $status if ( $status != AZ_OK and $status != AZ_STREAM_END );

        if ( length($$outRef) > 0 ) {
            $writeFh->print($$outRef)
              or return _ioError("write error during copy");
        }

        last if $status == AZ_STREAM_END;
    }
    $self->{'compressedSize'} = $self->_writeOffset();
    return AZ_OK;
}

# Return true if I depend on the named file
sub _usesFileNamed {
    return 0;
}

1;
