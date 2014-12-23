# File.pm -- Low-level access to Win32 file/dir functions/constants.

package Win32API::File;

use strict;
use integer;
use Carp;
use Config qw( %Config );
use Fcntl qw( O_RDONLY O_RDWR O_WRONLY O_APPEND O_BINARY O_TEXT );
use vars qw( $VERSION @ISA );
use vars qw( @EXPORT @EXPORT_OK @EXPORT_FAIL %EXPORT_TAGS );

$VERSION= '0.1001_01';

use base qw( Exporter DynaLoader Tie::Handle IO::File );

# Math::BigInt optimizations courtesy of Tels
my $_64BITINT;
BEGIN {
    $_64BITINT = defined($Config{use64bitint}) &&
                 ($Config{use64bitint} eq 'define');

    require Math::BigInt unless $_64BITINT;
}

my $THIRTY_TWO = $_64BITINT ? 32 : Math::BigInt->new(32);

my $FFFFFFFF   = $_64BITINT ? 0xFFFFFFFF : Math::BigInt->new(0xFFFFFFFF);

@EXPORT= qw();
%EXPORT_TAGS= (
    Func =>	[qw(		attrLetsToBits		createFile
    	fileConstant		fileLastError		getLogicalDrives
	CloseHandle		CopyFile		CreateFile
	DefineDosDevice		DeleteFile		DeviceIoControl
	FdGetOsFHandle		GetDriveType		GetFileAttributes		GetFileType
	GetHandleInformation	GetLogicalDrives	GetLogicalDriveStrings
	GetOsFHandle		GetVolumeInformation	IsRecognizedPartition
	IsContainerPartition	MoveFile		MoveFileEx
	OsFHandleOpen		OsFHandleOpenFd		QueryDosDevice
	ReadFile		SetErrorMode		SetFilePointer
	SetHandleInformation	WriteFile		GetFileSize
	getFileSize		setFilePointer		GetOverlappedResult)],
    FuncA =>	[qw(
	CopyFileA		CreateFileA		DefineDosDeviceA
	DeleteFileA		GetDriveTypeA		GetFileAttributesA		GetLogicalDriveStringsA
	GetVolumeInformationA	MoveFileA		MoveFileExA
	QueryDosDeviceA )],
    FuncW =>	[qw(
	CopyFileW		CreateFileW		DefineDosDeviceW
	DeleteFileW		GetDriveTypeW		GetFileAttributesW		GetLogicalDriveStringsW
	GetVolumeInformationW	MoveFileW		MoveFileExW
	QueryDosDeviceW )],
    Misc =>		[qw(
	CREATE_ALWAYS		CREATE_NEW		FILE_BEGIN
	FILE_CURRENT		FILE_END		INVALID_HANDLE_VALUE
	OPEN_ALWAYS		OPEN_EXISTING		TRUNCATE_EXISTING )],
    DDD_ =>	[qw(
	DDD_EXACT_MATCH_ON_REMOVE			DDD_RAW_TARGET_PATH
	DDD_REMOVE_DEFINITION )],
    DRIVE_ =>	[qw(
	DRIVE_UNKNOWN		DRIVE_NO_ROOT_DIR	DRIVE_REMOVABLE
	DRIVE_FIXED		DRIVE_REMOTE		DRIVE_CDROM
	DRIVE_RAMDISK )],
    FILE_ =>	[qw(
	FILE_READ_DATA			FILE_LIST_DIRECTORY
	FILE_WRITE_DATA			FILE_ADD_FILE
	FILE_APPEND_DATA		FILE_ADD_SUBDIRECTORY
	FILE_CREATE_PIPE_INSTANCE	FILE_READ_EA
	FILE_WRITE_EA			FILE_EXECUTE
	FILE_TRAVERSE			FILE_DELETE_CHILD
	FILE_READ_ATTRIBUTES		FILE_WRITE_ATTRIBUTES
	FILE_ALL_ACCESS			FILE_GENERIC_READ
	FILE_GENERIC_WRITE		FILE_GENERIC_EXECUTE )],
    FILE_ATTRIBUTE_ =>	[qw(
    INVALID_FILE_ATTRIBUTES
    FILE_ATTRIBUTE_DEVICE        FILE_ATTRIBUTE_DIRECTORY
    FILE_ATTRIBUTE_ENCRYPTED     FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
    FILE_ATTRIBUTE_REPARSE_POINT FILE_ATTRIBUTE_SPARSE_FILE
	FILE_ATTRIBUTE_ARCHIVE		 FILE_ATTRIBUTE_COMPRESSED
	FILE_ATTRIBUTE_HIDDEN		 FILE_ATTRIBUTE_NORMAL
	FILE_ATTRIBUTE_OFFLINE		 FILE_ATTRIBUTE_READONLY
	FILE_ATTRIBUTE_SYSTEM		 FILE_ATTRIBUTE_TEMPORARY )],
    FILE_FLAG_ =>	[qw(
	FILE_FLAG_BACKUP_SEMANTICS	FILE_FLAG_DELETE_ON_CLOSE
	FILE_FLAG_NO_BUFFERING		FILE_FLAG_OVERLAPPED
	FILE_FLAG_POSIX_SEMANTICS	FILE_FLAG_RANDOM_ACCESS
	FILE_FLAG_SEQUENTIAL_SCAN	FILE_FLAG_WRITE_THROUGH
	FILE_FLAG_OPEN_REPARSE_POINT )],
    FILE_SHARE_ =>	[qw(
	FILE_SHARE_DELETE	FILE_SHARE_READ		FILE_SHARE_WRITE )],
    FILE_TYPE_ =>	[qw(
	FILE_TYPE_CHAR		FILE_TYPE_DISK		FILE_TYPE_PIPE
	FILE_TYPE_UNKNOWN )],
    FS_ =>	[qw(
	FS_CASE_IS_PRESERVED		FS_CASE_SENSITIVE
	FS_UNICODE_STORED_ON_DISK	FS_PERSISTENT_ACLS 
	FS_FILE_COMPRESSION		FS_VOL_IS_COMPRESSED )],
	FSCTL_ => [qw(
	FSCTL_SET_REPARSE_POINT		FSCTL_GET_REPARSE_POINT
	FSCTL_DELETE_REPARSE_POINT )],
    HANDLE_FLAG_ =>	[qw(
	HANDLE_FLAG_INHERIT		HANDLE_FLAG_PROTECT_FROM_CLOSE )],
    IOCTL_STORAGE_ =>	[qw(
	IOCTL_STORAGE_CHECK_VERIFY	IOCTL_STORAGE_MEDIA_REMOVAL
	IOCTL_STORAGE_EJECT_MEDIA	IOCTL_STORAGE_LOAD_MEDIA
	IOCTL_STORAGE_RESERVE		IOCTL_STORAGE_RELEASE
	IOCTL_STORAGE_FIND_NEW_DEVICES	IOCTL_STORAGE_GET_MEDIA_TYPES
	)],
    IOCTL_DISK_ =>	[qw(
	IOCTL_DISK_FORMAT_TRACKS	IOCTL_DISK_FORMAT_TRACKS_EX
	IOCTL_DISK_GET_DRIVE_GEOMETRY	IOCTL_DISK_GET_DRIVE_LAYOUT
	IOCTL_DISK_GET_MEDIA_TYPES	IOCTL_DISK_GET_PARTITION_INFO
	IOCTL_DISK_HISTOGRAM_DATA	IOCTL_DISK_HISTOGRAM_RESET
	IOCTL_DISK_HISTOGRAM_STRUCTURE	IOCTL_DISK_IS_WRITABLE
	IOCTL_DISK_LOGGING		IOCTL_DISK_PERFORMANCE
	IOCTL_DISK_REASSIGN_BLOCKS	IOCTL_DISK_REQUEST_DATA
	IOCTL_DISK_REQUEST_STRUCTURE	IOCTL_DISK_SET_DRIVE_LAYOUT
	IOCTL_DISK_SET_PARTITION_INFO	IOCTL_DISK_VERIFY )],
    GENERIC_ =>		[qw(
	GENERIC_ALL			GENERIC_EXECUTE
	GENERIC_READ			GENERIC_WRITE )],
    MEDIA_TYPE =>	[qw(
	Unknown			F5_1Pt2_512		F3_1Pt44_512
	F3_2Pt88_512		F3_20Pt8_512		F3_720_512
	F5_360_512		F5_320_512		F5_320_1024
	F5_180_512		F5_160_512		RemovableMedia
	FixedMedia		F3_120M_512 )],
    MOVEFILE_ =>	[qw(
	MOVEFILE_COPY_ALLOWED		MOVEFILE_DELAY_UNTIL_REBOOT
	MOVEFILE_REPLACE_EXISTING	MOVEFILE_WRITE_THROUGH )],
    SECURITY_ =>	[qw(
	SECURITY_ANONYMOUS		SECURITY_CONTEXT_TRACKING
	SECURITY_DELEGATION		SECURITY_EFFECTIVE_ONLY
	SECURITY_IDENTIFICATION		SECURITY_IMPERSONATION
	SECURITY_SQOS_PRESENT )],
    SEM_ =>		[qw(
	SEM_FAILCRITICALERRORS		SEM_NOGPFAULTERRORBOX
	SEM_NOALIGNMENTFAULTEXCEPT	SEM_NOOPENFILEERRORBOX )],
    PARTITION_ =>	[qw(
	PARTITION_ENTRY_UNUSED		PARTITION_FAT_12
	PARTITION_XENIX_1		PARTITION_XENIX_2
	PARTITION_FAT_16		PARTITION_EXTENDED
	PARTITION_HUGE			PARTITION_IFS
	PARTITION_FAT32			PARTITION_FAT32_XINT13
	PARTITION_XINT13		PARTITION_XINT13_EXTENDED
	PARTITION_PREP			PARTITION_UNIX
	VALID_NTFT			PARTITION_NTFT )],
);
@EXPORT_OK= ();
{
    my $key;
    foreach $key (  keys(%EXPORT_TAGS)  ) {
	push( @EXPORT_OK, @{$EXPORT_TAGS{$key}} );
	#push( @EXPORT_FAIL, @{$EXPORT_TAGS{$key}} )   unless  $key =~ /^Func/;
    }
}
$EXPORT_TAGS{ALL}= \@EXPORT_OK;

bootstrap Win32API::File $VERSION;

# Preloaded methods go here.

# To convert C constants to Perl code in cFile.pc
# [instead of C or C++ code in cFile.h]:
#    * Modify F<Makefile.PL> to add WriteMakeFile() =>
#      CONST2PERL/postamble => [[ "Win32API::File" => ]] WRITE_PERL => 1.
#    * Either comment out C<#include "cFile.h"> from F<File.xs>
#      or make F<cFile.h> an empty file.
#    * Make sure the following C<if> block is not commented out.
#    * "nmake clean", "perl Makefile.PL", "nmake"

if(  ! defined &GENERIC_READ  ) {
    require "Win32API/File/cFile.pc";
}

sub fileConstant
{
    my( $name )= @_;
    if(  1 != @_  ||  ! $name  ||  $name =~ /\W/  ) {
	require Carp;
	Carp::croak( 'Usage: ',__PACKAGE__,'::fileConstant("CONST_NAME")' );
    }
    my $proto= prototype $name;
    if(  defined \&$name
     &&  defined $proto
     &&  "" eq $proto  ) {
	no strict 'refs';
	return &$name;
    }
    return undef;
}

# We provide this for backwards compatibility:
sub constant
{
    my( $name )= @_;
    my $value= fileConstant( $name );
    if(  defined $value  ) {
	$!= 0;
	return $value;
    }
    $!= 11; # EINVAL
    return 0;
}

# BEGIN {
#     my $code= 'return _fileLastError(@_)';
#     local( $!, $^E )= ( 1, 1 );
#     if(  $! ne $^E  ) {
# 	$code= '
# 	    local( $^E )= _fileLastError(@_);
# 	    my $ret= $^E;
# 	    return $ret;
# 	';
#     }
#     eval "sub fileLastError { $code }";
#     die "$@"   if  $@;
# }

package Win32API::File::_error;

use overload
    '""' => sub {
	require Win32 unless defined &Win32::FormatMessage;
	$_ = Win32::FormatMessage(Win32API::File::_fileLastError());
	tr/\r\n//d;
	return $_;
    },
    '0+' => sub { Win32API::File::_fileLastError() },
    'fallback' => 1;

sub new { return bless {}, shift }
sub set { Win32API::File::_fileLastError($_[1]); return $_[0] }

package Win32API::File;

my $_error = Win32API::File::_error->new();

sub fileLastError {
    croak 'Usage: ',__PACKAGE__,'::fileLastError( [$setWin32ErrCode] )'	if @_ > 1;
    $_error->set($_[0]) if defined $_[0];
    return $_error;
}

# Since we ISA DynaLoader which ISA AutoLoader, we ISA AutoLoader so we
# need this next chunk to prevent Win32API::File->nonesuch() from
# looking for "nonesuch.al" and producing confusing error messages:
use vars qw($AUTOLOAD);
sub AUTOLOAD {
    require Carp;
    Carp::croak(
      "Can't locate method $AUTOLOAD via package Win32API::File" );
}

# Replace "&rout;" with "goto &rout;" when that is supported on Win32.

# Aliases for non-Unicode functions:
sub CopyFile			{ &CopyFileA; }
sub CreateFile			{ &CreateFileA; }
sub DefineDosDevice		{ &DefineDosDeviceA; }
sub DeleteFile			{ &DeleteFileA; }
sub GetDriveType		{ &GetDriveTypeA; }
sub GetFileAttributes	{ &GetFileAttributesA; }
sub GetLogicalDriveStrings	{ &GetLogicalDriveStringsA; }
sub GetVolumeInformation	{ &GetVolumeInformationA; }
sub MoveFile			{ &MoveFileA; }
sub MoveFileEx			{ &MoveFileExA; }
sub QueryDosDevice		{ &QueryDosDeviceA; }

sub OsFHandleOpen {
    if(  3 != @_  ) {
	croak 'Win32API::File Usage:  ',
	      'OsFHandleOpen(FILE,$hNativeHandle,"rwatb")';
    }
    my( $fh, $osfh, $access )= @_;
    if(  ! ref($fh)  ) {
	if(  $fh !~ /('|::)/  ) {
	    $fh= caller() . "::" . $fh;
	}
	no strict "refs";
	$fh= \*{$fh};
    }
    my( $mode, $pref );
    if(  $access =~ /r/i  ) {
	if(  $access =~ /w/i  ) {
	    $mode= O_RDWR;
	    $pref= "+<";
	} else {
	    $mode= O_RDONLY;
	    $pref= "<";
	}
    } else {
	if(  $access =~ /w/i  ) {
	    $mode= O_WRONLY;
	    $pref= ">";
	} else {
	#   croak qq<Win32API::File::OsFHandleOpen():  >,
	#	  qq<Access ($access) missing both "r" and "w">;
	    $mode= O_RDONLY;
	    $pref= "<";
	}
    }
    $mode |= O_APPEND   if  $access =~ /a/i;
    #$mode |= O_TEXT   if  $access =~ /t/i;
    # Some versions of the Fcntl module are broken and won't autoload O_TEXT:
    if(  $access =~ /t/i  ) {
	my $o_text= eval "O_TEXT";
	$o_text= 0x4000   if  $@;
	$mode |= $o_text;
    }
    $mode |= O_BINARY   if  $access =~ /b/i;
    my $fd = eval { OsFHandleOpenFd( $osfh, $mode ) };
    if ($@) {
	return tie *{$fh}, __PACKAGE__, $osfh;
    }
    return  undef if  $fd < 0;
    return  open( $fh, $pref."&=".$fd );
}

sub GetOsFHandle {
    if(  1 != @_  ) {
	croak 'Win32API::File Usage:  $OsFHandle= GetOsFHandle(FILE)';
    }
    my( $file )= @_;
    if(  ! ref($file)  ) {
	if(  $file !~ /('|::)/  ) {
	    $file= caller() . "::" . $file;
	}
	no strict "refs";
	# The eval "" is necessary in Perl 5.6, avoid it otherwise.
	my $tied = !defined($^]) || $^] < 5.008
                       ? eval "tied *{$file}"
                       : tied *{$file};

	if (UNIVERSAL::isa($tied => __PACKAGE__)) {
		return $tied->win32_handle;
	}

	$file= *{$file};
    }
    my( $fd )= fileno($file);
    if(  ! defined( $fd )  ) {
	if(  $file =~ /^\d+\Z/  ) {
	    $fd= $file;
	} else {
	    return ();	# $! should be set by fileno().
	}
    }
    my $h= FdGetOsFHandle( $fd );
    if(  INVALID_HANDLE_VALUE() == $h  ) {
	$h= "";
    } elsif(  "0" eq $h  ) {
	$h= "0 but true";
    }
    return $h;
}

sub getFileSize {
    croak 'Win32API::File Usage:  $size= getFileSize($hNativeHandle)'
	if @_ != 1;

    my $handle    = shift;
    my $high_size = 0;

    my $low_size = GetFileSize($handle, $high_size);

    my $retval = $_64BITINT ? $high_size : Math::BigInt->new($high_size);

    $retval <<= $THIRTY_TWO;
    $retval +=  $low_size;

    return $retval;
}

sub setFilePointer {
    croak 'Win32API::File Usage:  $pos= setFilePointer($hNativeHandle, $posl, $from_where)'
	if @_ != 3;

    my ($handle, $pos, $from_where) = @_;

    my ($pos_low, $pos_high) = ($pos, 0);

    if ($_64BITINT) {
	$pos_low  = ($pos & $FFFFFFFF);
	$pos_high = (($pos >> $THIRTY_TWO) & $FFFFFFFF);
    }
    elsif (UNIVERSAL::isa($pos => 'Math::BigInt')) {
	$pos_low  = ($pos & $FFFFFFFF)->numify();
	$pos_high = (($pos >> $THIRTY_TWO) & $FFFFFFFF)->numify();
    }

    my $retval = SetFilePointer($handle, $pos_low, $pos_high, $from_where);

    if (defined $pos_high && $pos_high != 0) {
	if (! $_64BITINT) {
	    $retval   = Math::BigInt->new($retval);
	    $pos_high = Math::BigInt->new($pos_high);
	}

	$retval += $pos_high << $THIRTY_TWO;
    }

    return $retval;
}

sub attrLetsToBits
{
    my( $lets )= @_;
    my( %a )= (
      "a"=>FILE_ATTRIBUTE_ARCHIVE(),	"c"=>FILE_ATTRIBUTE_COMPRESSED(),
      "h"=>FILE_ATTRIBUTE_HIDDEN(),	"o"=>FILE_ATTRIBUTE_OFFLINE(),
      "r"=>FILE_ATTRIBUTE_READONLY(),	"s"=>FILE_ATTRIBUTE_SYSTEM(),
      "t"=>FILE_ATTRIBUTE_TEMPORARY() );
    my( $bits )= 0;
    foreach(  split(//,$lets)  ) {
	croak "Win32API::File::attrLetsToBits: Unknown attribute letter ($_)"
	  unless  exists $a{$_};
	$bits |= $a{$_};
    }
    return $bits;
}

use vars qw( @_createFile_Opts %_createFile_Opts );
@_createFile_Opts= qw( Access Create Share Attributes
		       Flags Security Model );
@_createFile_Opts{@_createFile_Opts}= (1) x @_createFile_Opts;

sub createFile
{
    my $opts= "";
    if(  2 <= @_  &&  "HASH" eq ref($_[$#_])  ) {
	$opts= pop( @_ );
    }
    my( $sPath, $svAccess, $svShare )= @_;
    if(  @_ < 1  ||  3 < @_  ) {
	croak "Win32API::File::createFile() usage:  \$hObject= createFile(\n",
	      "  \$sPath, [\$svAccess_qrw_ktn_ce,[\$svShare_rwd,]]",
	      " [{Option=>\$Value}] )\n",
	      "    options: @_createFile_Opts\nCalled";
    }
    my( $create, $flags, $sec, $model )= ( "", 0, [], 0 );
    if(  ref($opts)  ) {
        my @err= grep( ! $_createFile_Opts{$_}, keys(%$opts) );
	@err  and  croak "_createFile:  Invalid options (@err)";
	$flags= $opts->{Flags}		if  exists( $opts->{Flags} );
	$flags |= attrLetsToBits( $opts->{Attributes} )
					if  exists( $opts->{Attributes} );
	$sec= $opts->{Security}		if  exists( $opts->{Security} );
	$model= $opts->{Model}		if  exists( $opts->{Model} );
	$svAccess= $opts->{Access}	if  exists( $opts->{Access} );
	$create= $opts->{Create}	if  exists( $opts->{Create} );
	$svShare= $opts->{Share}	if  exists( $opts->{Share} );
    }
    $svAccess= "r"		unless  defined($svAccess);
    $svShare= "rw"		unless  defined($svShare);
    if(  $svAccess =~ /^[qrw ktn ce]*$/i  ) {
	( my $c= $svAccess ) =~ tr/qrw QRW//d;
	$create= $c   if  "" ne $c  &&  "" eq $create;
	local( $_ )= $svAccess;
	$svAccess= 0;
	$svAccess |= GENERIC_READ()   if  /r/i;
	$svAccess |= GENERIC_WRITE()   if  /w/i;
    } elsif(  "?" eq $svAccess  ) {
	croak
	  "Win32API::File::createFile:  \$svAccess can use the following:\n",
	      "    One or more of the following:\n",
	      "\tq -- Query access (same as 0)\n",
	      "\tr -- Read access (GENERIC_READ)\n",
	      "\tw -- Write access (GENERIC_WRITE)\n",
	      "    At most one of the following:\n",
	      "\tk -- Keep if exists\n",
	      "\tt -- Truncate if exists\n",
	      "\tn -- New file only (fail if file already exists)\n",
	      "    At most one of the following:\n",
	      "\tc -- Create if doesn't exist\n",
	      "\te -- Existing file only (fail if doesn't exist)\n",
	      "  ''   is the same as 'q  k e'\n",
	      "  'r'  is the same as 'r  k e'\n",
	      "  'w'  is the same as 'w  t c'\n",
	      "  'rw' is the same as 'rw k c'\n",
	      "  'rt' or 'rn' implies 'c'.\n",
	      "  Or \$svAccess can be numeric.\n", "Called from";
    } elsif(  $svAccess == 0  &&  $svAccess !~ /^[-+.]*0/  ) {
	croak "Win32API::File::createFile:  Invalid \$svAccess ($svAccess)";
    }
    if(  $create =~ /^[ktn ce]*$/  ) {
        local( $_ )= $create;
        my( $k, $t, $n, $c, $e )= ( scalar(/k/i), scalar(/t/i),
	  scalar(/n/i), scalar(/c/i), scalar(/e/i) );
	if(  1 < $k + $t + $n  ) {
	    croak "Win32API::File::createFile: \$create must not use ",
	      qq<more than one of "k", "t", and "n" ($create)>;
	}
	if(  $c  &&  $e  ) {
	    croak "Win32API::File::createFile: \$create must not use ",
	      qq<both "c" and "e" ($create)>;
	}
	my $r= ( $svAccess & GENERIC_READ() ) == GENERIC_READ();
	my $w= ( $svAccess & GENERIC_WRITE() ) == GENERIC_WRITE();
	if(  ! $k  &&  ! $t  &&  ! $n  ) {
	    if(  $w  &&  ! $r  ) {		$t= 1;
	    } else {				$k= 1; }
	}
	if(  $k  ) {
	    if(  $c  ||  $w && ! $e  ) {	$create= OPEN_ALWAYS();
	    } else {				$create= OPEN_EXISTING(); }
	} elsif(  $t  ) {
	    if(  $e  ) {			$create= TRUNCATE_EXISTING();
	    } else {				$create= CREATE_ALWAYS(); }
	} else { # $n
	    if(  ! $e  ) {			$create= CREATE_NEW();
	    } else {
		croak "Win32API::File::createFile: \$create must not use ",
		  qq<both "n" and "e" ($create)>;
	    }
	}
    } elsif(  "?" eq $create  ) {
	croak 'Win32API::File::createFile: $create !~ /^[ktn ce]*$/;',
	      ' pass $svAccess as "?" for more information.';
    } elsif(  $create == 0  &&  $create ne "0"  ) {
	croak "Win32API::File::createFile: Invalid \$create ($create)";
    }
    if(  $svShare =~ /^[drw]*$/  ) {
        my %s= ( "d"=>FILE_SHARE_DELETE(), "r"=>FILE_SHARE_READ(),
	         "w"=>FILE_SHARE_WRITE() );
        my @s= split(//,$svShare);
	$svShare= 0;
	foreach( @s ) {
	    $svShare |= $s{$_};
	}
    } elsif(  $svShare == 0  &&  $svShare !~ /^[-+.]*0/  ) {
	croak "Win32API::File::createFile: Invalid \$svShare ($svShare)";
    }
    return  CreateFileA(
	      $sPath, $svAccess, $svShare, $sec, $create, $flags, $model );
}


sub getLogicalDrives
{
    my( $ref )= @_;
    my $s= "";
    if(  ! GetLogicalDriveStringsA( 256, $s )  ) {
	return undef;
    }
    if(  ! defined($ref)  ) {
	return  split( /\0/, $s );
    } elsif(  "ARRAY" ne ref($ref)  ) {
	croak 'Usage:  C<@arr= getLogicalDrives()> ',
	      'or C<getLogicalDrives(\\@arr)>', "\n";
    }
    @$ref= split( /\0/, $s );
    return $ref;
}

###############################################################################
#   Experimental Tied Handle and Object Oriented interface.                   #
###############################################################################

sub new {
	my $class = shift;
	$class = ref $class || $class;

	my $self = IO::File::new($class);
	tie *$self, __PACKAGE__;

	$self->open(@_) if @_;

	return $self;
}

sub TIEHANDLE {
	my ($class, $win32_handle) = @_;
	$class = ref $class || $class;

	return bless {
		_win32_handle => $win32_handle,
		_binmode      => 0,
		_buffered     => 0,
		_buffer       => '',
		_eof          => 0,
		_fileno       => undef,
		_access       => 'r',
		_append       => 0,
	}, $class;
}

# This is called for getting the tied object from hard refs to glob refs in
# some cases, for reasons I don't quite grok.

sub FETCH { return $_[0] }

# Public accessors

sub win32_handle{ $_[0]->{_win32_handle}||= $_[1] }

# Protected accessors

sub _buffer	{ $_[0]->{_buffer}	||= $_[1] }
sub _binmode	{ $_[0]->{_binmode}	||= $_[1] }
sub _fileno	{ $_[0]->{_fileno}	||= $_[1] }
sub _access	{ $_[0]->{_access}	||= $_[1] }
sub _append	{ $_[0]->{_append}	||= $_[1] }

# Tie interface

sub OPEN {
	my $self  = shift;
	my $expr  = shift;
	croak "Only the two argument form of open is supported at this time" if @_;
# FIXME: this needs to parse the full Perl open syntax in $expr

	my ($mixed, $mode, $path) =
		($expr =~ /^\s* (\+)? \s* (<|>|>>)? \s* (.*?) \s*$/x);

	croak "Unsupported open mode" if not $path;

	my $access = 'r';
	my $append = $mode eq '>>' ? 1 : 0;

	if ($mixed) {
		$access = 'rw';
	} elsif($mode eq '>') {
		$access = 'w';
	}

	my $w32_handle = createFile($path, $access);

	$self->win32_handle($w32_handle);

	$self->seek(1,2) if $append;

	$self->_access($access);
	$self->_append($append);

	return 1;
}

sub BINMODE {
	$_[0]->_binmode(1);
}

sub WRITE {
	my ($self, $buf, $len, $offset, $overlap) = @_;

	if ($offset) {
		$buf = substr($buf, $offset);
		$len = length($buf);
	}

	$len       = length($buf) if not defined $len;

	$overlap   = [] if not defined $overlap;;

	my $bytes_written = 0;

	WriteFile (
		$self->win32_handle, $buf, $len,
		$bytes_written, $overlap
	);

	return $bytes_written;
}

sub PRINT {
	my $self = shift;

	my $buf = join defined $, ? $, : "" => @_;

	$buf =~ s/\012/\015\012/sg unless $self->_binmode();

	$buf .= $\ if defined $\;

	$self->WRITE($buf, length($buf), 0);
}

sub READ {
	my $self = shift;
	my $into = \$_[0]; shift;
	my ($len, $offset, $overlap) = @_;

	my $buffer     = defined $self->_buffer ? $self->_buffer : "";
	my $buf_length = length($buffer);
	my $bytes_read = 0;
	my $data;
	$offset        = 0 if not defined $offset;

	if ($buf_length >= $len) {
		$data       = substr($buffer, 0, $len => "");
		$bytes_read = $len;
		$self->_buffer($buffer);
	} else {
		if ($buf_length > 0) {
			$len -= $buf_length;
			substr($$into, $offset) = $buffer;
			$offset += $buf_length;
		}

		$overlap ||= [];

		ReadFile (
			$self->win32_handle, $data, $len,
			$bytes_read, $overlap
		);
	}

	$$into = "" if not defined $$into;

	substr($$into, $offset) = $data;

	return $bytes_read;
}

sub READLINE {
	my $self = shift;
	my $line = "";

	while ((index $line, $/) == $[-1) { # read until end of line marker
		my $char = $self->GETC();

		last if !defined $char || $char eq '';

		$line .= $char;
	}

	return undef if $line eq '';

	return $line;
}


sub FILENO {
	my $self = shift;

	return $self->_fileno() if defined $self->_fileno();

	return -1 if $^O eq 'cygwin';

# FIXME: We don't always open the handle, better to query the handle or to set
# the right access info at TIEHANDLE time.

	my $access = $self->_access();
	my $mode   = $access eq 'rw' ? O_RDWR :
		$access eq 'w' ? O_WRONLY : O_RDONLY;

	$mode |= O_APPEND if $self->_append();

	$mode |= O_TEXT   if not $self->_binmode();

	return $self->_fileno ( OsfHandleOpenFd (
		$self->win32_handle, $mode
	));
}

sub SEEK {
	my ($self, $pos, $whence) = @_;

	$whence = 0 if not defined $whence;
	my @file_consts = map {
		fileConstant($_)
	} qw(FILE_BEGIN FILE_CURRENT FILE_END);

	my $from_where = $file_consts[$whence];

	return setFilePointer($self->win32_handle, $pos, $from_where);
}

sub TELL {
# SetFilePointer with position 0 at FILE_CURRENT will return position.
	return $_[0]->SEEK(0, 1);
}

sub EOF {
	my $self = shift;

	my $current = $self->TELL() + 0;
	my $end     = getFileSize($self->win32_handle) + 0;

	return $current == $end;
}

sub CLOSE {
	my $self = shift;

	my $retval = 1;
	
	if (defined $self->win32_handle) {
		$retval = CloseHandle($self->win32_handle);

		$self->win32_handle(undef);
	}

	return $retval;
}

# Only close the handle on explicit close, too many problems otherwise.
sub UNTIE {}

sub DESTROY {}

# End of Tie/OO Interface

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Win32API::File - Low-level access to Win32 system API calls for files/dirs.

=head1 SYNOPSIS

  use Win32API::File 0.08 qw( :ALL );

  MoveFile( $Source, $Destination )
    or  die "Can't move $Source to $Destination: ",fileLastError(),"\n";
  MoveFileEx( $Source, $Destination, MOVEFILE_REPLACE_EXISTING() )
    or  die "Can't move $Source to $Destination: ",fileLastError(),"\n";
  [...]

=head1 DESCRIPTION

This provides fairly low-level access to the Win32 System API
calls dealing with files and directories.

To pass in C<NULL> as the pointer to an optional buffer, pass in
an empty list reference, C<[]>.

Beyond raw access to the API calls and related constants, this module
handles smart buffer allocation and translation of return codes.

All functions, unless otherwise noted, return a true value for success
and a false value for failure and set C<$^E> on failure.

=head2 Object Oriented/Tied Handle Interface

WARNING: this is new code, use at your own risk.

This version of C<Win32API::File> can be used like an C<IO::File> object:

  my $file = Win32API::File->new("+> foo");
  binmode $file;
  print $file "hello there\n";
  seek $file, 0, 0;
  my $line = <$file>;
  $file->close;

It also supports tying via a win32 handle (for example, from C<createFile()>):

  tie FILE, 'Win32API::File', $win32_handle;
  print FILE "...";

It has not been extensively tested yet and buffered I/O is not yet implemented.

=head2 Exports

Nothing is exported by default.  The following tags can be used to
have large sets of symbols exported:  C<":Func">, C<":FuncA">,
C<":FuncW">, C<":Misc">, C<":DDD_">, C<":DRIVE_">, C<":FILE_">,
C<":FILE_ATTRIBUTE_">, C<":FILE_FLAG_">, C<":FILE_SHARE_">,
C<":FILE_TYPE_">, C<":FS_">, C<":FSCTL_">, C<":HANDLE_FLAG_">,
C<":IOCTL_STORAGE_">, C<":IOCTL_DISK_">, C<":GENERIC_">,
C<":MEDIA_TYPE">, C<":MOVEFILE_">, C<":SECURITY_">, C<":SEM_">,
and C<":PARTITION_">.

=over

=item C<":Func">

The basic function names:  C<attrLetsToBits>,         C<createFile>,
C<fileConstant>,           C<fileLastError>,          C<getLogicalDrives>,
C<setFilePointer>,         C<getFileSize>,
C<CloseHandle>,            C<CopyFile>,               C<CreateFile>,
C<DefineDosDevice>,        C<DeleteFile>,             C<DeviceIoControl>,
C<FdGetOsFHandle>,         C<GetDriveType>,           C<GetFileAttributes>,
C<GetFileSize>,            C<GetFileType>,            C<GetHandleInformation>,
C<GetLogicalDrives>,       C<GetLogicalDriveStrings>, C<GetOsFHandle>,
C<GetOverlappedResult>,    C<GetVolumeInformation>,   C<IsContainerPartition>,
C<IsRecognizedPartition>,  C<MoveFile>,               C<MoveFileEx>,
C<OsFHandleOpen>,          C<OsFHandleOpenFd>,        C<QueryDosDevice>,
C<ReadFile>,               C<SetErrorMode>,           C<SetFilePointer>,
C<SetHandleInformation>,   and C<WriteFile>.

=over

=item attrLetsToBits

=item C<$uBits= attrLetsToBits( $sAttributeLetters )>

Converts a string of file attribute letters into an unsigned value with
the corresponding bits set.  C<$sAttributeLetters> should contain zero
or more letters from C<"achorst">:

=over

=item C<"a">

C<FILE_ATTRIBUTE_ARCHIVE>

=item C<"c">

C<FILE_ATTRIBUTE_COMPRESSED>

=item C<"h">

C<FILE_ATTRIBUTE_HIDDEN>

=item C<"o">

C<FILE_ATTRIBUTE_OFFLINE>

=item C<"r">

C<FILE_ATTRIBUTE_READONLY>

=item C<"s">

C<FILE_ATTRIBUTE_SYSTEM>

=item C<"t">

C<FILE_ATTRIBUTE_TEMPORARY>

=back

=item createFile

=item C<$hObject= createFile( $sPath )>

=item C<$hObject= createFile( $sPath, $rvhvOptions )>

=item C<$hObject= createFile( $sPath, $svAccess )>

=item C<$hObject= createFile( $sPath, $svAccess, $rvhvOptions )>

=item C<$hObject= createFile( $sPath, $svAccess, $svShare )>

=item C<$hObject= createFile( $sPath, $svAccess, $svShare, $rvhvOptions )>

This is a Perl-friendly wrapper around C<CreateFile>.

On failure, C<$hObject> gets set to a false value and C<regLastError()>
and C<$^E> are set to the reason for the failure.  Otherwise,
C<$hObject> gets set to a Win32 native file handle which is alwasy
a true value [returns C<"0 but true"> in the impossible(?) case of
the handle having a value of C<0>].

C<$sPath> is the path to the file [or device, etc.] to be opened.  See
C<CreateFile> for more information on possible special values for
C<$sPath>.  

C<$svAccess> can be a number containing the bit mask representing
the specific type(s) of access to the file that you desire.  See the
C<$uAccess> parameter to C<CreateFile> for more information on these
values.

More likely, C<$svAccess> is a string describing the generic type of
access you desire and possibly the file creation options to use.  In
this case, C<$svAccess> should contain zero or more characters from
C<"qrw"> [access desired], zero or one character each from C<"ktn">
and C<"ce">, and optional white space.  These letters stand for,
respectively, "Query access", "Read access", "Write access", "Keep if
exists", "Truncate if exists", "New file only", "Create if none", and
"Existing file only".  Case is ignored.

You can pass in C<"?"> for C<$svAccess> to have an error message
displayed summarizing its possible values.  This is very handy when
doing on-the-fly programming using the Perl debugger:

    Win32API::File::createFile:  $svAccess can use the following:
	One or more of the following:
	    q -- Query access (same as 0)
	    r -- Read access (GENERIC_READ)
	    w -- Write access (GENERIC_WRITE)
	At most one of the following:
	    k -- Keep if exists
	    t -- Truncate if exists
	    n -- New file only (fail if file already exists)
	At most one of the following:
	    c -- Create if doesn't exist
	    e -- Existing file only (fail if doesn't exist)
      ''   is the same as 'q  k e'
      'r'  is the same as 'r  k e'
      'w'  is the same as 'w  t c'
      'rw' is the same as 'rw k c'
      'rt' or 'rn' implies 'c'.
      Or $access can be numeric.

C<$svAccess> is designed to be "do what I mean", so you can skip
the rest of its explanation unless you are interested in the complex
details.  Note that, if you want write access to a device, you need
to specify C<"k"> [and perhaps C<"e">, as in C<"w ke"> or C<"rw ke">]
since Win32 suggests C<OPEN_EXISTING> be used when opening a device.

=over

=item C<"q"> 

Stands for "Query access".  This is really a no-op since you always have
query access when you open a file.  You can specify C<"q"> to document
that you plan to query the file [or device, etc.].  This is especially
helpful when you don't want read nor write access since something like
C<"q"> or C<"q ke"> may be easier to understand than just C<""> or C<"ke">.

=item C<"r">

Stands for "Read access".  Sets the C<GENERIC_READ> bit(s) in the
C<$uAccess> that is passed to C<CreateFile>.  This is the default
access if the C<$svAccess> parameter is missing [or if it is C<undef>
and C<$rvhvOptions> doesn't specify an C<"Access"> option].

=item C<"w">

Stands for "Write access".  Sets the C<GENERIC_WRITE> bit(s) in the
C<$uAccess> that is passed to C<CreateFile>.

=item C<"k">

Stands for "Keep if exists".  If the requested file exists, then it is
opened.  This is the default unless C<GENERIC_WRITE> access has been
requested but C<GENERIC_READ> access has not been requested.   Contrast
with C<"t"> and C<"n">.

=item C<"t">

Stands for "Truncate if exists".  If the requested file exists, then
it is truncated to zero length and then opened.  This is the default if
C<GENERIC_WRITE> access has been requested and C<GENERIC_READ> access
has not been requested.  Contrast with C<"k"> and C<"n">.

=item C<"n">

Stands for "New file only".  If the requested file exists, then it is
not opened and the C<createFile> call fails.  Contrast with C<"k"> and
C<"t">.  Can't be used with C<"e">.

=item C<"c">

Stands for "Create if none".  If the requested file does not
exist, then it is created and then opened.  This is the default
if C<GENERIC_WRITE> access has been requested or if C<"t"> or
C<"n"> was specified.  Contrast with C<"e">.

=item C<"e">

Stands for "Existing file only".  If the requested file does not
exist, then nothing is opened and the C<createFile> call fails.  This
is the default unless C<GENERIC_WRITE> access has been requested or
C<"t"> or C<"n"> was specified.   Contrast with C<"c">.   Can't be
used with C<"n">.

=back

The characters from C<"ktn"> and C<"ce"> are combined to determine the
what value for C<$uCreate> to pass to C<CreateFile> [unless overridden
by C<$rvhvOptions>]:

=over

=item C<"kc">

C<OPEN_ALWAYS>

=item C<"ke">

C<OPEN_EXISTING>

=item C<"tc">

C<TRUNCATE_EXISTING>

=item C<"te">

C<CREATE_ALWAYS>

=item C<"nc">

C<CREATE_NEW>

=item C<"ne">

Illegal.

=back

C<$svShare> controls how the file is shared, that is, whether other
processes can have read, write, and/or delete access to the file while
we have it opened.  C<$svShare> will usually be a string containing zero
or more characters from C<"rwd"> but can also be a numeric bit mask.

C<"r"> sets the C<FILE_SHARE_READ> bit which allows other processes to have
read access to the file.  C<"w"> sets the C<FILE_SHARE_WRITE> bit which
allows other processes to have write access to the file.  C<"d"> sets the
C<FILE_SHARE_DELETE> bit which allows other processes to have delete access
to the file [ignored under Windows 95].

The default for C<$svShare> is C<"rw"> which provides the same sharing as
using regular perl C<open()>.

If another process currently has read, write, and/or delete access to
the file and you don't allow that level of sharing, then your call to
C<createFile> will fail.  If you requested read, write, and/or delete
access and another process already has the file open but doesn't allow
that level of sharing, then your call to C<createFile> will fail.  Once
you have the file open, if another process tries to open it with read,
write, and/or delete access and you don't allow that level of sharing,
then that process won't be allowed to open the file.

C<$rvhvOptions> is a reference to a hash where any keys must be from
the list C<qw( Access Create Share Attributes Flags Security Model )>.
The meaning of the value depends on the key name, as described below.
Any option values in C<$rvhvOptions> override the settings from
C<$svAccess> and C<$svShare> if they conflict.

=over

=item Flags => $uFlags

C<$uFlags> is an unsigned value having any of the C<FILE_FLAG_*> or
C<FILE_ATTRIBUTE_*> bits set.  Any C<FILE_ATTRIBUTE_*> bits set via the
C<Attributes> option are logically C<or>ed with these bits.  Defaults
to C<0>.

If opening the client side of a named pipe, then you can also specify
C<SECURITY_SQOS_PRESENT> along with one of the other C<SECURITY_*>
constants to specify the security quality of service to be used.

=item Attributes => $sAttributes

A string of zero or more characters from C<"achorst"> [see C<attrLetsToBits>
for more information] which are converted to C<FILE_ATTRIBUTE_*> bits to
be set in the C<$uFlags> argument passed to C<CreateFile>.

=item Security => $pSecurityAttributes

C<$pSecurityAttributes> should contain a C<SECURITY_ATTRIBUTES> structure
packed into a string or C<[]> [the default].

=item Model => $hModelFile

C<$hModelFile> should contain a handle opened with C<GENERIC_READ>
access to a model file from which file attributes and extended attributes
are to be copied.  Or C<$hModelFile> can be C<0> [the default].

=item Access => $sAccess

=item Access => $uAccess

C<$sAccess> should be a string of zero or more characters from
C<"qrw"> specifying the type of access desired:  "query" or C<0>,
"read" or C<GENERIC_READ> [the default], or "write" or
C<GENERIC_WRITE>.

C<$uAccess> should be an unsigned value containing bits set to
indicate the type of access desired.  C<GENERIC_READ> is the default.

=item Create => $sCreate

=item Create => $uCreate

C<$sCreate> should be a string constaing zero or one character from
C<"ktn"> and zero or one character from C<"ce">.  These stand for
"Keep if exists", "Truncate if exists", "New file only", "Create if
none", and "Existing file only".  These are translated into a
C<$uCreate> value.

C<$uCreate> should be one of C<OPEN_ALWAYS>, C<OPEN_EXISTING>,
C<TRUNCATE_EXISTING>, C<CREATE_ALWAYS>, or C<CREATE_NEW>.

=item Share => $sShare

=item Share => $uShare

C<$sShare> should be a string with zero or more characters from
C<"rwd"> that is translated into a C<$uShare> value.  C<"rw"> is
the default.

C<$uShare> should be an unsigned value having zero or more of the
following bits set:  C<FILE_SHARE_READ>, C<FILE_SHARE_WRITE>, and
C<FILE_SHARE_DELETE>.  C<FILE_SHARE_READ|FILE_SHARE_WRITE> is the
default.

=back

Examples:

    $hFlop= createFile( "//./A:", "r", "r" )
      or  die "Can't prevent others from writing to floppy: $^E\n";
    $hDisk= createFile( "//./C:", "rw ke", "" )
      or  die "Can't get exclusive access to C: $^E\n";
    $hDisk= createFile( $sFilePath, "ke",
      { Access=>FILE_READ_ATTRIBUTES } )
      or  die "Can't read attributes of $sFilePath: $^E\n";
    $hTemp= createFile( "$ENV{Temp}/temp.$$", "wn", "",
      { Attributes=>"hst", Flags=>FILE_FLAG_DELETE_ON_CLOSE() } )
      or  die "Can't create temporary file, temp.$$: $^E\n";

=item getLogicalDrives

=item C<@roots= getLogicalDrives()>

Returns the paths to the root directories of all logical drives
currently defined.  This includes all types of drive lettters, such
as floppies, CD-ROMs, hard disks, and network shares.  A typical
return value on a poorly equipped computer would be C<("A:\\","C:\\")>.

=item CloseHandle

=item C<CloseHandle( $hObject )>

Closes a Win32 native handle, such as one opened via C<CreateFile>. 
Like most routines, returns a true value if successful and a false
value [and sets C<$^E> and C<regLastError()>] on failure.

=item CopyFile

=item C<CopyFile( $sOldFileName, $sNewFileName, $bFailIfExists )>

C<$sOldFileName> is the path to the file to be copied. 
C<$sNewFileName> is the path to where the file should be copied. 
Note that you can B<NOT> just specify a path to a directory in
C<$sNewFileName> to copy the file to that directory using the
same file name.

If C<$bFailIfExists> is true and C<$sNewFileName> is the path to
a file that already exists, then C<CopyFile> will fail.  If
C<$bFailIfExists> is falsea, then the copy of the C<$sOldFileNmae>
file will overwrite the C<$sNewFileName> file if it already exists.

Like most routines, returns a true value if successful and a false
value [and sets C<$^E> and C<regLastError()>] on failure.

=item CreateFile

=item C<$hObject= CreateFile( $sPath, $uAccess, $uShare, $pSecAttr, $uCreate, $uFlags, $hModel )>

On failure, C<$hObject> gets set to a false value and C<$^E> and
C<fileLastError()> are set to the reason for the failure.  Otherwise,
C<$hObject> gets set to a Win32 native file handle which is always a
true value [returns C<"0 but true"> in the impossible(?) case of the
handle having a value of C<0>].

C<$sPath> is the path to the file [or device, etc.] to be opened.

C<$sPath> can use C<"/"> or C<"\\"> as path delimiters and can even
mix the two.  We will usually only use C<"/"> in our examples since
using C<"\\"> is usually harder to read.

Under Windows NT, C<$sPath> can start with C<"//?/"> to allow the use
of paths longer than C<MAX_PATH> [for UNC paths, replace the leading
C<"//"> with C<"//?/UNC/">, as in C<"//?/UNC/Server/Share/Dir/File.Ext">].

C<$sPath> can start with C<"//./"> to indicate that the rest of the
path is the name of a "DOS device."  You can use C<QueryDosDevice>
to list all current DOS devices and can add or delete them with
C<DefineDosDevice>.  If you get the source-code distribution of this
module from CPAN, then it includes an example script, F<ex/ListDevs.plx>
that will list all current DOS devices and their "native" definition.
Again, note that this doesn't work under Win95 nor Win98.

The most common such DOS devices include:

=over

=item C<"//./PhysicalDrive0">

Your entire first hard disk.  Doesn't work under Windows 95.  This
allows you to read or write raw sectors of your hard disk and to use
C<DeviceIoControl> to perform miscellaneous queries and operations
to the hard disk.   Writing raw sectors and certain other operations
can seriously damage your files or the function of your computer.

Locking this for exclusive access [by specifying C<0> for C<$uShare>]
doesn't prevent access to the partitions on the disk nor their file
systems.  So other processes can still access any raw sectors within
a partition and can use the file system on the disk as usual.

=item C<"//./C:">

Your F<C:> partition.  Doesn't work under Windows 95.  This allows
you to read or write raw sectors of that partition and to use
C<DeviceIoControl> to perform miscellaneous queries and operations
to the partition.  Writing raw sectors and certain other operations
can seriously damage your files or the function of your computer.

Locking this for exclusive access doesn't prevent access to the
physical drive that the partition is on so other processes can
still access the raw sectors that way.  Locking this for exclusive
access B<does> prevent other processes from opening the same raw
partition and B<does> prevent access to the file system on it.  It
even prevents the current process from accessing the file system
on that partition.

=item C<"//./A:">

The raw floppy disk.  Doesn't work under Windows 95.  This allows
you to read or write raw sectors of the floppy disk and to use
C<DeviceIoControl> to perform miscellaneous queries and operations
to the floopy disk or drive.

Locking this for exclusive access prevents all access to the floppy.

=item C<"//./PIPE/PipeName">

A named pipe, created via C<CreateNamedPipe>.

=back

C<$uAccess> is an unsigned value with bits set indicating the
type of access desired.  Usually either C<0> ["query" access],
C<GENERIC_READ>, C<GENERIC_WRITE>, C<GENERIC_READ|GENERIC_WRITE>,
or C<GENERIC_ALL>.  More specific types of access can be specified,
such as C<FILE_APPEND_DATA> or C<FILE_READ_EA>.

C<$uShare> controls how the file is shared, that is, whether other
processes can have read, write, and/or delete access to the file while
we have it opened.  C<$uShare> is an unsigned value with zero or more
of these bits set:  C<FILE_SHARE_READ>, C<FILE_SHARE_WRITE>, and
C<FILE_SHARE_DELETE>.

If another process currently has read, write, and/or delete access to
the file and you don't allow that level of sharing, then your call to
C<CreateFile> will fail.  If you requested read, write, and/or delete
access and another process already has the file open but doesn't allow
that level of sharing, thenn your call to C<createFile> will fail.  Once
you have the file open, if another process tries to open it with read,
write, and/or delete access and you don't allow that level of sharing,
then that process won't be allowed to open the file.

C<$pSecAttr> should either be C<[]> [for C<NULL>] or a
C<SECURITY_ATTRIBUTES> data structure packed into a string.
For example, if C<$pSecDesc> contains a C<SECURITY_DESCRIPTOR>
structure packed into a string, perhaps via:

    RegGetKeySecurity( $key, 4, $pSecDesc, 1024 );

then you can set C<$pSecAttr> via:

    $pSecAttr= pack( "L P i", 12, $pSecDesc, $bInheritHandle );

C<$uCreate> is one of the following values:  C<OPEN_ALWAYS>,
C<OPEN_EXISTING>, C<TRUNCATE_EXISTING>, C<CREATE_ALWAYS>, and
C<CREATE_NEW>.

C<$uFlags> is an unsigned value with zero or more bits set indicating
attributes to associate with the file [C<FILE_ATTRIBUTE_*> values] or
special options [C<FILE_FLAG_*> values].

If opening the client side of a named pipe, then you can also set
C<$uFlags> to include C<SECURITY_SQOS_PRESENT> along with one of the
other C<SECURITY_*> constants to specify the security quality of
service to be used.

C<$hModel> is C<0> [or C<[]>, both of which mean C<NULL>] or a Win32
native handle opened with C<GENERIC_READ> access to a model file from
which file attributes and extended attributes are to be copied if a
new file gets created.

Examples:

    $hFlop= CreateFile( "//./A:", GENERIC_READ(),
      FILE_SHARE_READ(), [], OPEN_EXISTING(), 0, [] )
      or  die "Can't prevent others from writing to floppy: $^E\n";
    $hDisk= CreateFile( $sFilePath, FILE_READ_ATTRIBUTES(),
      FILE_SHARE_READ()|FILE_SHARE_WRITE(), [], OPEN_EXISTING(), 0, [] )
      or  die "Can't read attributes of $sFilePath: $^E\n";
    $hTemp= CreateFile( "$ENV{Temp}/temp.$$", GENERIC_WRITE(), 0,
      CREATE_NEW(), FILE_FLAG_DELETE_ON_CLOSE()|attrLetsToBits("hst"), [] )
      or  die "Can't create temporary file, temp.$$: $^E\n";

=item DefineDosDevice

=item C<DefineDosDevice( $uFlags, $sDosDeviceName, $sTargetPath )>

Defines a new DOS device, overrides the current definition of a DOS
device, or deletes a definition of a DOS device.  Like most routines,
returns a true value if successful and a false value [and sets C<$^E>
and C<regLastError()>] on failure.

C<$sDosDeviceName> is the name of a DOS device for which we'd like
to add or delete a definition.

C<$uFlags> is an unsigned value with zero or more of the following
bits set:

=over

=item C<DDD_RAW_TARGET_PATH>

Indicates that C<$sTargetPath> will be a raw Windows NT object name. 
This usually means that C<$sTargetPath> starts with C<"\\Device\\">. 
Note that you cannot use C<"/"> in place of C<"\\"> in raw target path
names.

=item C<DDD_REMOVE_DEFINITION>

Requests that a definition be deleted.  If C<$sTargetPath> is
C<[]> [for C<NULL>], then the most recently added definition for
C<$sDosDeviceName> is removed.  Otherwise the most recently added
definition matching C<$sTargetPath> is removed.

If the last definition is removed, then the DOS device name is
also deleted.

=item C<DDD_EXACT_MATCH_ON_REMOVE>

When deleting a definition, this bit causes each C<$sTargetPath> to
be compared to the full-length definition when searching for the most
recently added match.  If this bit is not set, then C<$sTargetPath>
only needs to match a prefix of the definition.

=back

C<$sTargetPath> is the DOS device's specific definition that you
wish to add or delete.  For C<DDD_RAW_TARGET_PATH>, these usually
start with C<"\\Device\\">.  If the C<DDD_RAW_TARGET_PATH> bit is
not set, then C<$sTargetPath> is just an ordinary path to some file
or directory, providing the functionality of the B<subst> command.

=item DeleteFile

=item C<DeleteFile( $sFileName )>

Deletes the named file.  Compared to Perl's C<unlink>, C<DeleteFile>
has the advantage of not deleting read-only files.  For B<some>
versions of Perl, C<unlink> silently calls C<chmod> whether it needs
to or not before deleting the file so that files that you have
protected by marking them as read-only are not always protected from
Perl's C<unlink>.

Like most routines, returns a true value if successful and a false
value [and sets C<$^E> and C<regLastError()>] on failure.

=item DeviceIoControl

=item C<DeviceIoControl( $hDevice, $uIoControlCode, $pInBuf, $lInBuf, $opOutBuf, $lOutBuf, $olRetBytes, $pOverlapped )>

Requests a special operation on an I/O [input/output] device, such
as ejecting a tape or formatting a disk.  Like most routines, returns
a true value if successful and a false value [and sets C<$^E> and
C<regLastError()>] on failure.

C<$hDevice> is a Win32 native file handle to a device [return value
from C<CreateFile>].

C<$uIoControlCode> is an unsigned value [a C<IOCTL_*> or C<FSCTL_*>
constant] indicating the type query or other operation to be performed.

C<$pInBuf> is C<[]> [for C<NULL>] or a data structure packed into a
string.  The type of data structure depends on the C<$uIoControlCode>
value.  C<$lInBuf> is C<0> or the length of the structure in
C<$pInBuf>.  If C<$pInBuf> is not C<[]> and C<$lInBuf> is C<0>, then
C<$lInBuf> will automatically be set to C<length($pInBuf)> for you.

C<$opOutBuf> is C<[]> [for C<NULL>] or will be set to contain a
returned data structure packed into a string.  C<$lOutBuf> indicates
how much space to allocate in C<$opOutBuf> for C<DeviceIoControl> to
store the data structure.  If C<$lOutBuf> is a number and C<$opOutBuf>
already has a buffer allocated for it that is larger than C<$lOutBuf>
bytes, then this larger buffer size will be passed to C<DeviceIoControl>.
However, you can force a specific buffer size to be passed to
C<DeviceIoControl> by prepending a C<"="> to the front of C<$lOutBuf>.

C<$olRetBytes> is C<[]> or is a scalar to receive the number of bytes
written to C<$opOutBuf>.  Even when C<$olRetBytes> is C<[]>, a valid
pointer to a C<DWORD> [and not C<NULL>] is passed to C<DeviceIoControl>.
In this case, C<[]> just means that you don't care about the value
that might be written to C<$olRetBytes>, which is usually the case
since you can usually use C<length($opOutBuf)> instead.

C<$pOverlapped> is C<[]> or is a C<OVERLAPPED> structure packed into
a string.  This is only useful if C<$hDevice> was opened with the
C<FILE_FLAG_OVERLAPPED> flag set.

=item FdGetOsFHandle

=item C<$hNativeHandle= FdGetOsFHandle( $ivFd )>

C<FdGetOsFHandle> simply calls C<_get_osfhandle()>.  It was renamed
to better fit in with the rest the function names of this module,
in particular to distinguish it from C<GetOsFHandle>.  It takes an
integer file descriptor [as from Perl's C<fileno>] and returns the
Win32 native file handle associated with that file descriptor or
C<INVALID_HANDLE_VALUE> if C<$ivFd> is not an open file descriptor.

When you call Perl's C<open> to set a Perl file handle [like C<STDOUT>],
Perl calls C's C<fopen> to set a stdio C<FILE *>.  C's C<fopen> calls
something like Unix's C<open>, that is, Win32's C<_sopen>, to get an
integer file descriptor [where 0 is for C<STDIN>, 1 for C<STDOUT>, etc.].
Win32's C<_sopen> calls C<CreateFile> to set a C<HANDLE>, a Win32 native
file handle.  So every Perl file handle [like C<STDOUT>] has an integer
file descriptor associated with it that you can get via C<fileno>.  And,
under Win32, every file descriptor has a Win32 native file handle
associated with it.  C<FdGetOsFHandle> lets you get access to that.

C<$hNativeHandle> is set to C<INVALID_HANDLE_VALUE> [and
C<lastFileError()> and C<$^E> are set] if C<FdGetOsFHandle> fails. 
See also C<GetOsFHandle> which provides a friendlier interface.

=item fileConstant

=item C<$value= fileConstant( $sConstantName )>

Fetch the value of a constant.  Returns C<undef> if C<$sConstantName>
is not the name of a constant supported by this module.  Never sets
C<$!> nor C<$^E>.

This function is rarely used since you will usually get the value of a
constant by having that constant imported into your package by listing
the constant name in the C<use Win32API::File> statement and then
simply using the constant name in your code [perhaps followed by
C<()>].  This function is useful for verifying constant names not in
Perl code, for example, after prompting a user to type in a constant
name.

=item fileLastError

=item C<$svError= fileLastError();>

=item C<fileLastError( $uError );>

Returns the last error encountered by a routine from this module. 
It is just like C<$^E> except it isn't changed by anything except
routines from this module.  Ideally you could just use C<$^E>, but
current versions of Perl often overwrite C<$^E> before you get a
chance to check it and really old versions of Perl don't really
support C<$^E> under Win32.

Just like C<$^E>, in a numeric context C<fileLastError()> returns
the numeric error value while in a string context it returns a
text description of the error [actually it returns a Perl scalar
that contains both values so C<$x= fileLastError()> causes C<$x>
to give different values in string vs. numeric contexts].

The last form sets the error returned by future calls to
C<fileLastError()> and should not be used often.  C<$uError> must
be a numeric error code.  Also returns the dual-valued version
of C<$uError>.

=item GetDriveType

=item C<$uDriveType= GetDriveType( $sRootPath )>

Takes a string giving the path to the root directory of a file system
[called a "drive" because every file system is assigned a "drive letter"]
and returns an unsigned value indicating the type of drive the file
system is on.  The return value should be one of:

=over

=item C<DRIVE_UNKNOWN>

None of the following.

=item C<DRIVE_NO_ROOT_DIR>

A "drive" that does not have a file system.  This can be a drive letter
that hasn't been defined or a drive letter assigned to a partition
that hasn't been formatted yet.

=item C<DRIVE_REMOVABLE>

A floppy diskette drive or other removable media drive, but not a CD-ROM
drive.

=item C<DRIVE_FIXED>

An ordinary hard disk partition.

=item C<DRIVE_REMOTE>

A network share.

=item C<DRIVE_CDROM>

A CD-ROM drive.

=item C<DRIVE_RAMDISK>

A "ram disk" or memory-resident virtual file system used for high-speed
access to small amounts of temporary file space.

=back

=item GetFileAttributes

=item C<$uAttrs = GetFileAttributes( $sPath )>

Takes a path string and returns an unsigned value with attribute flags.
If it fails, it returns INVALID_FILE_ATTRIBUTES, otherwise it can be
one or more of the following values:

=over

=item C<FILE_ATTRIBUTE_ARCHIVE>

The file or directory is an archive file or directory. Applications use
this attribute to mark files for backup or removal.

=item C<FILE_ATTRIBUTE_COMPRESSED>

The file or directory is compressed. For a file, this means that all of
the data in the file is compressed. For a directory, this means that
compression is the default for newly created files and subdirectories. 

=item C<FILE_ATTRIBUTE_DEVICE>

Reserved; do not use. 

=item C<FILE_ATTRIBUTE_DIRECTORY>

The handle identifies a directory. 

=item C<FILE_ATTRIBUTE_ENCRYPTED>

The file or directory is encrypted. For a file, this means that all data
streams in the file are encrypted. For a directory, this means that
encryption is the default for newly created files and subdirectories. 

=item C<FILE_ATTRIBUTE_HIDDEN>

The file or directory is hidden. It is not included in an ordinary directory
listing. 

=item C<FILE_ATTRIBUTE_NORMAL>

The file or directory has no other attributes set. This attribute is valid
only if used alone. 

=item C<FILE_ATTRIBUTE_NOT_CONTENT_INDEXED>

The file will not be indexed by the content indexing service. 

=item C<FILE_ATTRIBUTE_OFFLINE>

The data of the file is not immediately available. This attribute indicates
that the file data has been physically moved to offline storage. This
attribute is used by Remote Storage, the hierarchical storage management
software. Applications should not arbitrarily change this attribute. 

=item C<FILE_ATTRIBUTE_READONLY>

The file or directory is read-only. Applications can read the file but cannot
write to it or delete it. In the case of a directory, applications cannot
delete it. 

=item C<FILE_ATTRIBUTE_REPARSE_POINT>

The file or directory has an associated reparse point. 

=item C<FILE_ATTRIBUTE_SPARSE_FILE>

The file is a sparse file. 

=item C<FILE_ATTRIBUTE_SYSTEM>

The file or directory is part of, or is used exclusively by, the operating
system. 

=item C<FILE_ATTRIBUTE_TEMPORARY>

The file is being used for temporary storage. File systems avoid writing
data back to mass storage if sufficient cache memory is available, because
often the application deletes the temporary file shortly after the handle is
closed. In that case, the system can entirely avoid writing the data.
Otherwise, the data will be written after the handle is closed. 

=back

=item GetFileType

=item C<$uFileType= GetFileType( $hFile )>

Takes a Win32 native file handle and returns a C<FILE_TYPE_*> constant
indicating the type of the file opened on that handle:

=over

=item C<FILE_TYPE_UNKNOWN>

None of the below.  Often a special device.

=item C<FILE_TYPE_DISK>

An ordinary disk file.

=item C<FILE_TYPE_CHAR>

What Unix would call a "character special file", that is, a device that
works on character streams such as a printer port or a console.

=item C<FILE_TYPE_PIPE>

Either a named or anonymous pipe.

=back

=item getFileSize

=item C<$size= getFileSize( $hFile )>

This is a Perl-friendly wrapper for the C<GetFileSize> (below) API call.

It takes a Win32 native file handle and returns the size in bytes. Since the
size can be a 64 bit value, on non 64 bit integer Perls the value returned will
be an object of type C<Math::BigInt>.

=item GetFileSize

=item C<$iSizeLow= GetFileSize($win32Handle, $iSizeHigh)>

Returns the size of a file pointed to by C<$win32Handle>, optionally storing
the high order 32 bits into C<$iSizeHigh> if it is not C<[]>. If $iSizeHigh is
C<[]>, a non-zero value indicates success. Otherwise, on failure the return
value will be C<0xffffffff> and C<fileLastError()> will not be C<NO_ERROR>.

=item GetOverlappedResult

=item C<$bRetval= GetOverlappedResult( $win32Handle, $pOverlapped,
 $numBytesTransferred, $bWait )>

Used for asynchronous IO in Win32 to get the result of a pending IO operation,
such as when a file operation returns C<ERROR_IO_PENDING>. Returns a false
value on failure. The C<$overlapped> structure and C<$numBytesTransferred>
will be modified with the results of the operation.

As far as creating the C<$pOverlapped> structure, you are currently on your own.

See L<http://msdn.microsoft.com/library/default.asp?url=/library/en-us/dllproc/base/getoverlappedresult.asp> for more information.

=item GetLogicalDrives

=item C<$uDriveBits= GetLogicalDrives()>

Returns an unsigned value with one bit set for each drive letter currently
defined.  If "A:" is currently a valid drive letter, then the C<1> bit
will be set in C<$uDriveBits>.  If "B:" is valid, then the C<2> bit will
be set.  If "Z:" is valid, then the C<2**26> [C<0x4000000>] bit will be
set.

=item GetLogicalDriveStrings

=item C<$olOutLength= GetLogicalDriveStrings( $lBufSize, $osBuffer )>

For each currently defined drive letter, a C<'\0'>-terminated string
of the path to the root of its file system is constructed.  All of
these strings are concatenated into a single larger string and an
extra terminating C<'\0'> is added.  This larger string is returned
in C<$osBuffer>.  Note that this includes drive letters that have
been defined but that have no file system, such as drive letters
assigned to unformatted partitions.

C<$lBufSize> is the size of the buffer to allocate to store this
list of strings.  C<26*4+1> is always sufficient and should usually
be used.

C<$osBuffer> is a scalar to be set to contain the constructed string.

C<$olOutLength> is the number of bytes actually written to C<$osBuffer>
but C<length($osBuffer)> can also be used to determine this.

For example, on a poorly equipped computer,

    GetLogicalDriveStrings( 4*26+1, $osBuffer );

might set C<$osBuffer> to the 9-character string, C<"A:\\\0C:\\\0\0">.

=item GetHandleInformation

=item C<GetHandleInformation( $hObject, $ouFlags )>

Retrieves the flags associated with a Win32 native file handle or object
handle.

C<$hObject> is an open Win32 native file handle or an open Win32 native
handle to some other type of object.

C<$ouFlags> will be set to an unsigned value having zero or more of
the bits C<HANDLE_FLAG_INHERIT> and C<HANDLE_FLAG_PROTECT_FROM_CLOSE>
set.  See the C<":HANDLE_FLAG_"> export class for the meanings of these
bits.

=item GetOsFHandle

=item C<$hNativeHandle= GetOsFHandle( FILE )>

Takes a Perl file handle [like C<STDIN>] and returns the Win32 native
file handle associated with it.  See C<FdGetOsFHandle> for more
information about Win32 native file handles.

C<$hNativeHandle> is set to a false value [and C<lastFileError()> and
C<$^E> are set] if C<GetOsFHandle> fails.    C<GetOsFHandle> returns
C<"0 but true"> in the impossible(?) case of the handle having a value
of C<0>.

=item GetVolumeInformation

=item C<GetVolumeInformation( $sRootPath, $osVolName, $lVolName, $ouSerialNum, $ouMaxNameLen, $ouFsFlags, $osFsType, $lFsType )>

Gets information about a file system volume, returning a true
value if successful.  On failure, returns a false value and sets
C<fileLastError()> and C<$^E>.

C<$sRootPath> is a string specifying the path to the root of the file system,
for example, C<"C:/">.

C<$osVolName> is a scalar to be set to the string representing the
volume name, also called the file system label.  C<$lVolName> is the
number of bytes to allocate for the C<$osVolName> buffer [see
L<Buffer Sizes> for more information].

C<$ouSerialNum> is C<[]> [for C<NULL>] or will be set to the numeric
value of the volume's serial number.

C<$ouMaxNameLen> is C<[]> [for C<NULL>] or will be set to the maximum
length allowed for a file name or directory name within the file system.

C<$osFsType> is a scalar to be set to the string representing the
file system type, such as C<"FAT"> or C<"NTFS">.  C<$lFsType> is the
number of bytes to allocate for the C<$osFsType> buffer [see
L<Buffer Sizes> for more information].

C<$ouFsFlags> is C<[]> [for C<NULL>] or will be set to an unsigned integer
with bits set indicating properties of the file system:

=over

=item C<FS_CASE_IS_PRESERVED>

The file system preserves the case of file names [usually true].
That is, it doesn't change the case of file names such as forcing
them to upper- or lower-case.

=item C<FS_CASE_SENSITIVE>

The file system supports the ability to not ignore the case of file
names [but might ignore case the way you are using it].  That is, the
file system has the ability to force you to get the letter case of a
file's name exactly right to be able to open it.  This is true for
"NTFS" file systems, even though case in file names is usually still
ignored.

=item C<FS_UNICODE_STORED_ON_DISK>

The file system preserves Unicode in file names [true for "NTFS"].

=item C<FS_PERSISTENT_ACLS>

The file system supports setting Access Control Lists on files [true
for "NTFS"].

=item C<FS_FILE_COMPRESSION>

The file system supports compression on a per-file basis [true for
"NTFS"].

=item C<FS_VOL_IS_COMPRESSED>

The entire file system is compressed such as via "DoubleSpace".

=back

=item IsRecognizedPartition

=item C<IsRecognizedPartition( $ivPartitionType )>

Takes a partition type and returns whether that partition type is
supported under Win32.  C<$ivPartitonType> is an integer value as from
the operating system byte of a hard disk's DOS-compatible partition
table [that is, a partition table for x86-based Win32, not, for
example, one used with Windows NT for Alpha processors].  For example,
the C<PartitionType> member of the C<PARTITION_INFORMATION> structure.

Common values for C<$ivPartitionType> include C<PARTITION_FAT_12==1>,
C<PARTITION_FAT_16==4>, C<PARTITION_EXTENDED==5>, C<PARTITION_FAT32==0xB>.

=item IsContainerPartition

=item C<IsContainerPartition( $ivPartitionType )>

Takes a partition type and returns whether that partition is a
"container" partition that is supported under Win32, that is, whether
it is an "extended" partition that can contain "logical" partitions. 
C<$ivPartitonType> is as for C<IsRecognizedPartition>.

=item MoveFile

=item C<MoveFile( $sOldName, $sNewName )>

Renames a file or directory.  C<$sOldName> is the name of the existing
file or directory that is to be renamed.  C<$sNewName> is the new name
to give the file or directory.  Returns a true value if the move
succeeds.  For failure, returns a false value and sets
C<fileLastErorr()> and C<$^E> to the reason for the failure.

Files can be "renamed" between file systems and the file contents and
some attributes will be moved.  Directories can only be renamed within
one file system.  If there is already a file or directory named
C<$sNewName>, then C<MoveFile> will fail.

=item MoveFileEx

=item C<MoveFileEx( $sOldName, $sNewName, $uFlags )>

Renames a file or directory.  C<$sOldName> is the name of the existing
file or directory that is to be renamed.  C<$sNewName> is the new name
to give the file or directory.  Returns a true value if the move
succeeds.  For failure, returns a false value and sets
C<fileLastErorr()> and C<$^E> to the reason for the failure.

C<$uFlags> is an unsigned value with zero or more of the following bits set:

=over

=item C<MOVEFILE_REPLACE_EXISTING>

If this bit is set and a file [but not a directory] named C<$sNewName>
already exists, then it will be replaced by C<$sOldName>.  If this bit
is not set then C<MoveFileEx> will fail rather than replace an existing
C<$sNewName>.

=item C<MOVEFILE_COPY_ALLOWED>

Allows files [but not directories] to be moved between file systems
by copying the C<$sOldName> file data and some attributes to
C<$sNewName> and then deleting C<$sOldName>.  If this bit is not set
[or if C<$sOldName> denotes a directory] and C<$sNewName> refers to a
different file system than C<$sOldName>, then C<MoveFileEx> will fail.

=item C<MOVEFILE_DELAY_UNTIL_REBOOT>

Preliminary verifications are made and then an entry is added to the
Registry to cause the rename [or delete] operation to be done the
next time this copy of the operating system is booted [right after
any automatic file system checks have completed].  This is not
supported under Windows 95.

When this bit is set, C<$sNewName> can be C<[]> [for C<NULL>] to
indicate that C<$sOldName> should be deleted during the next boot
rather than renamed.

Setting both the C<MOVEFILE_COPY_ALLOWED> and
C<MOVEFILE_DELAY_UNTIL_REBOOT> bits will cause C<MoveFileEx> to fail.

=item C<MOVEFILE_WRITE_THROUGH>

Ensures that C<MoveFileEx> won't return until the operation has
finished and been flushed to disk.   This is not supported under
Windows 95.  Only affects file renames to another file system,
forcing a buffer flush at the end of the copy operation.

=back

=item OsFHandleOpen

=item C<OsFHandleOpen( FILE, $hNativeHandle, $sMode )>

Opens a Perl file handle based on an already open Win32 native
file handle [much like C's C<fdopen()> does with a file descriptor].
Returns a true value if the open operation succeeded.  For failure,
returns a false value and sets C<$!> [and possibly C<fileLastError()>
and C<$^E>] to the reason for the failure.

C<FILE> is a Perl file handle [in any of the supported forms, a
bareword, a string, a typeglob, or a reference to a typeglob] that
will be opened.  If C<FILE> is already open, it will automatically
be closed before it is reopened.

C<$hNativeHandle> is an open Win32 native file handle, probably the
return value from C<CreateFile> or C<createFile>.

C<$sMode> is string of zero or more letters from C<"rwatb">.  These
are translated into a combination C<O_RDONLY> [C<"r">], C<O_WRONLY>
[C<"w">], C<O_RDWR> [C<"rw">], C<O_APPEND> [C<"a">], C<O_TEXT>
[C<"t">], and C<O_BINARY> [C<"b">] flags [see the L<Fcntl> module]
that is passed to C<OsFHandleOpenFd>.   Currently only C<O_APPEND>
and C<O_TEXT> have any significance.

Also, a C<"r"> and/or C<"w"> in C<$sMode> is used to decide how the
file descriptor is converted into a Perl file handle, even though this
doesn't appear to make a difference.  One of the following is used:

    open( FILE, "<&=".$ivFd )	# "r" w/o "w"
    open( FILE, ">&=".$ivFd )	# "w" w/o "r"
    open( FILE, "+<&=".$ivFd )	# both "r" and "w"

C<OsFHandleOpen> eventually calls the Win32-specific C routine
C<_open_osfhandle()> or Perl's "improved" version called
C<win32_open_osfhandle()>.  Prior to Perl5.005, C's
C<_open_osfhandle()> is called which will fail if
C<GetFileType($hNativeHandle)> would return C<FILE_TYPE_UNKNOWN>.  For
Perl5.005 and later, C<OsFHandleOpen> calls C<win32_open_osfhandle()>
from the Perl DLL which doesn't have this restriction.

=item OsFHandleOpenFd

=item C<$ivFD= OsFHandleOpenFd( $hNativeHandle, $uMode )>

Opens a file descriptor [C<$ivFD>] based on an already open Win32
native file handle, C<$hNativeHandle>.  This just calls the
Win32-specific C routine C<_open_osfhandle()> or Perl's "improved"
version called C<win32_open_osfhandle()>.  Prior to Perl5.005 and in Cygwin
Perl, C's C<_open_osfhandle()> is called which will fail if
C<GetFileType($hNativeHandle)> would return C<FILE_TYPE_UNKNOWN>.  For
Perl5.005 and later, C<OsFHandleOpenFd> calls C<win32_open_osfhandle()> from
the Perl DLL which doesn't have this restriction.

C<$uMode> the logical combination of zero or more C<O_*> constants
exported by the C<Fcntl> module.  Currently only C<O_APPEND> and
C<O_TEXT> have any significance.

C<$ivFD> will be non-negative if the open operation was successful. 
For failure, C<-1> is returned and C<$!> [and possibly
C<fileLastError()> and C<$^E>] is set to the reason for the failure.

=item QueryDosDevice

=item C<$olTargetLen= QueryDosDevice( $sDosDeviceName, $osTargetPath, $lTargetBuf )>

Looks up the definition of a given "DOS" device name, yielding the
active Windows NT native device name along with any currently dormant
definitions.

C<$sDosDeviceName> is the name of the "DOS" device whose definitions
we want.  For example, C<"C:">, C<"COM1">, or C<"PhysicalDrive0">.
If C<$sDosDeviceName> is C<[]> [for C<NULL>], the list of all DOS
device names is returned instead.

C<$osTargetPath> will be assigned a string containing the list of
definitions.  The definitions are each C<'\0'>-terminate and are
concatenated into the string, most recent first, with an extra C<'\0'>
at the end of the whole string [see C<GetLogicalDriveStrings> for
a sample of this format].

C<$lTargetBuf> is the size [in bytes] of the buffer to allocate for
C<$osTargetPath>.  See L<Buffer Sizes> for more information.

C<$olTargetLen> is set to the number of bytes written to
C<$osTargetPath> but you can also use C<length($osTargetPath)>
to determine this.

For failure, C<0> is returned and C<fileLastError()> and C<$^E> are
set to the reason for the failure.

=item ReadFile

=item C<ReadFile( $hFile, $opBuffer, $lBytes, $olBytesRead, $pOverlapped )>

Reads bytes from a file or file-like device.  Returns a true value if
the read operation was successful.  For failure, returns a false value
and sets C<fileLastError()> and C<$^E> for the reason for the failure.

C<$hFile> is a Win32 native file handle that is already open to the
file or device to read from.

C<$opBuffer> will be set to a string containing the bytes read.

C<$lBytes> is the number of bytes you would like to read. 
C<$opBuffer> is automatically initialized to have a buffer large
enough to hold that many bytes.  Unlike other buffer sizes, C<$lBytes>
does not need to have a C<"="> prepended to it to prevent a larger
value to be passed to the underlying Win32 C<ReadFile> API.  However,
a leading C<"="> will be silently ignored, even if Perl warnings are
enabled.

If C<$olBytesRead> is not C<[]>, it will be set to the actual number
of bytes read, though C<length($opBuffer)> can also be used to
determine this.

C<$pOverlapped> is C<[]> or is a C<OVERLAPPED> structure packed
into a string.  This is only useful if C<$hFile> was opened with
the C<FILE_FLAG_OVERLAPPED> flag set.

=item SetErrorMode

=item C<$uOldMode= SetErrorMode( $uNewMode )>

Sets the mode controlling system error handling B<and> returns the
previous mode value.  Both C<$uOldMode> and C<$uNewMode> will have
zero or more of the following bits set:

=over

=item C<SEM_FAILCRITICALERRORS>

If set, indicates that when a critical error is encountered, the call
that triggered the error fails immediately.  Normally this bit is not
set, which means that a critical error causes a dialogue box to appear
notifying the desktop user that some application has triggered a
critical error.   The dialogue box allows the desktop user to decide
whether the critical error is returned to the process, is ignored, or
the offending operation is retried.

This affects the C<CreateFile> and C<GetVolumeInformation> calls.

Setting this bit is useful for allowing you to check whether a floppy
diskette is in the floppy drive.

=item C<SEM_NOALIGNMENTFAULTEXCEPT>

If set, this causes memory access misalignment faults to be
automatically fixed in a manner invisible to the process.  This flag
is ignored on x86-based versions of Windows NT.  This flag is not
supported on Windows 95.

=item C<SEM_NOGPFAULTERRORBOX>

If set, general protection faults do not generate a dialogue box but
can instead be handled by the process via an exception handler.  This
bit should not be set by programs that don't know how to handle such
faults.

=item C<SEM_NOOPENFILEERRORBOX>

If set, then when an attempt to continue reading from or writing to
an already open file [usually on a removable medium like a floppy
diskette] finds the file no longer available, the call will
immediately fail.  Normally this bit is not set, which means that
instead a dialogue box will appear notifying the desktop user that
some application has run into this problem.   The dialogue box allows
the desktop user to decide whether the failure is returned to the
process, is ignored, or the offending operation is retried.

This affects the C<ReadFile> and C<WriteFile> calls.

=back

=item setFilePointer

=item C<$uNewPos = setFilePointer( $hFile, $ivOffset, $uFromWhere )>

This is a perl-friendly wrapper for the SetFilePointer API (below).
C<$ivOffset> can be a 64 bit integer or C<Math::BigInt> object if your Perl
doesn't have 64 bit integers. The return value is the new offset and will
likewise be a 64 bit integer or a C<Math::BigInt> object.

=item SetFilePointer

=item C<$uNewPos = SetFilePointer( $hFile, $ivOffset, $ioivOffsetHigh, $uFromWhere )>

The native Win32 version of C<seek()>.  C<SetFilePointer> sets the
position within a file where the next read or write operation will
start from.

C<$hFile> is a Win32 native file handle.

C<$uFromWhere> is either C<FILE_BEGIN>, C<FILE_CURRENT>, or
C<FILE_END>, indicating that the new file position is being specified
relative to the beginning of the file, the current file pointer, or
the end of the file, respectively.

C<$ivOffset> is [if C<$ioivOffsetHigh> is C<[]>] the offset [in bytes]
to the new file position from the position specified via
C<$uFromWhere>.  If C<$ioivOffsetHigh> is not C<[]>, then C<$ivOffset>
is converted to an unsigned value to be used as the low-order 4 bytes
of the offset.

C<$ioivOffsetHigh> can be C<[]> [for C<NULL>] to indicate that you are
only specifying a 4-byte offset and the resulting file position will
be 0xFFFFFFFE or less [just under 4GB].  Otherwise C<$ioivOfffsetHigh>
starts out with the high-order 4 bytes [signed] of the offset and gets
set to the [unsigned] high-order 4 bytes of the resulting file position.

The underlying C<SetFilePointer> returns C<0xFFFFFFFF> to indicate
failure, but if C<$ioivOffsetHigh> is not C<[]>, you would also have
to check C<$^E> to determine whether C<0xFFFFFFFF> indicates an error
or not.  C<Win32API::File::SetFilePointer> does this checking for you
and returns a false value if and only if the underlying
C<SetFilePointer> failed.  For this reason, C<$uNewPos> is set to
C<"0 but true"> if you set the file pointer to the beginning of the
file [or any position with 0 for the low-order 4 bytes].

So the return value will be true if the seek operation was successful.
For failure, a false value is returned and C<fileLastError()> and
C<$^E> are set to the reason for the failure.

=item SetHandleInformation

=item C<SetHandleInformation( $hObject, $uMask, $uFlags )>

Sets the flags associated with a Win32 native file handle or object
handle.  Returns a true value if the operation was successful.  For
failure, returns a false value and sets C<fileLastError()> and C<$^E>
for the reason for the failure.

C<$hObject> is an open Win32 native file handle or an open Win32 native
handle to some other type of object.

C<$uMask> is an unsigned value having one or more of the bits
C<HANDLE_FLAG_INHERIT> and C<HANDLE_FLAG_PROTECT_FROM_CLOSE> set.
Only bits set in C<$uMask> will be modified by C<SetHandleInformation>.

C<$uFlags> is an unsigned value having zero or more of the bits
C<HANDLE_FLAG_INHERIT> and C<HANDLE_FLAG_PROTECT_FROM_CLOSE> set.
For each bit set in C<$uMask>, the cooresponding bit in the handle's
flags is set to the value of the cooresponding bit in C<$uFlags>.

If C<$uOldFlags> were the value of the handle's flags before the
call to C<SetHandleInformation>, then the value of the handle's
flags afterward would be:

    ( $uOldFlags & ~$uMask ) | ( $uFlags & $uMask )

[at least as far as the C<HANDLE_FLAG_INHERIT> and
C<HANDLE_FLAG_PROTECT_FROM_CLOSE> bits are concerned.]

See the C<":HANDLE_FLAG_"> export class for the meanings of these bits.

=item WriteFile

=item C<WriteFile( $hFile, $pBuffer, $lBytes, $ouBytesWritten, $pOverlapped )>

Write bytes to a file or file-like device.  Returns a true value if
the operation was successful.  For failure, returns a false value and
sets C<fileLastError()> and C<$^E> for the reason for the failure.

C<$hFile> is a Win32 native file handle that is already open to the
file or device to be written to.

C<$pBuffer> is a string containing the bytes to be written.

C<$lBytes> is the number of bytes you would like to write.  If
C<$pBuffer> is not at least C<$lBytes> long, C<WriteFile> croaks.  You
can specify C<0> for C<$lBytes> to write C<length($pBuffer)> bytes.
A leading C<"="> on C<$lBytes> will be silently ignored, even if Perl
warnings are enabled.

C<$ouBytesWritten> will be set to the actual number of bytes written
unless you specify it as C<[]>.

C<$pOverlapped> is C<[]> or is an C<OVERLAPPED> structure packed
into a string.  This is only useful if C<$hFile> was opened with
the C<FILE_FLAG_OVERLAPPED> flag set.

=back

=item C<":FuncA">

The ASCII-specific functions.  Each of these is just the same as the
version without the trailing "A".

	CopyFileA
	CreateFileA
	DefineDosDeviceA
	DeleteFileA
	GetDriveTypeA
	GetFileAttributesA
	GetLogicalDriveStringsA
	GetVolumeInformationA
	MoveFileA
	MoveFileExA
	QueryDosDeviceA

=item C<":FuncW">

The wide-character-specific (Unicode) functions.  Each of these is
just the same as the version without the trailing "W" except that
strings are expected in Unicode and some lengths are measured as
number of C<WCHAR>s instead of number of bytes, as indicated below.

=over

=item CopyFileW

=item C<CopyFileW( $swOldFileName, $swNewFileName, $bFailIfExists )>

C<$swOldFileName> and C<$swNewFileName> are Unicode strings.

=item CreateFileW

=item C<$hObject= CreateFileW( $swPath, $uAccess, $uShare, $pSecAttr, $uCreate, $uFlags, $hModel )>

C<$swPath> is Unicode.

=item DefineDosDeviceW

=item C<DefineDosDeviceW( $uFlags, $swDosDeviceName, $swTargetPath )>

C<$swDosDeviceName> and C<$swTargetPath> are Unicode.

=item DeleteFileW

=item C<DeleteFileW( $swFileName )>

C<$swFileName> is Unicode.

=item GetDriveTypeW

=item C<$uDriveType= GetDriveTypeW( $swRootPath )>

C<$swRootPath> is Unicode.

=item GetFileAttributesW

=item C<$uAttrs= GetFileAttributesW( $swPath )>

C<$swPath> is Unicode.

=item GetLogicalDriveStringsW

=item C<$olwOutLength= GetLogicalDriveStringsW( $lwBufSize, $oswBuffer )>

Unicode is stored in C<$oswBuffer>.  C<$lwBufSize> and C<$olwOutLength>
are measured as number of C<WCHAR>s.

=item GetVolumeInformationW

=item C<GetVolumeInformationW( $swRootPath, $oswVolName, $lwVolName, $ouSerialNum, $ouMaxNameLen, $ouFsFlags, $oswFsType, $lwFsType )>

C<$swRootPath> is Unicode and Unicode is written to C<$oswVolName> and
C<$oswFsType>.  C<$lwVolName> and C<$lwFsType> are measures as number
of C<WCHAR>s.

=item MoveFileW

=item C<MoveFileW( $swOldName, $swNewName )>

C<$swOldName> and C<$swNewName> are Unicode.

=item MoveFileExW

=item C<MoveFileExW( $swOldName, $swNewName, $uFlags )>

C<$swOldName> and C<$swNewName> are Unicode.

=item QueryDosDeviceW

=item C<$olwTargetLen= QueryDosDeviceW( $swDeviceName, $oswTargetPath, $lwTargetBuf )>

C<$swDeviceName> is Unicode and Unicode is written to
C<$oswTargetPath>.  C<$lwTargetBuf> and C<$olwTargetLen> are measured
as number of  C<WCHAR>s.

=back

=item C<":Misc">

Miscellaneous constants.  Used for the C<$uCreate> argument of
C<CreateFile> or the C<$uFromWhere> argument of C<SetFilePointer>.
Plus C<INVALID_HANDLE_VALUE>, which you usually won't need to check
for since most routines translate it into a false value.

	CREATE_ALWAYS		CREATE_NEW		OPEN_ALWAYS
	OPEN_EXISTING		TRUNCATE_EXISTING	INVALID_HANDLE_VALUE
	FILE_BEGIN		FILE_CURRENT		FILE_END

=item C<":DDD_">

Constants for the C<$uFlags> argument of C<DefineDosDevice>.

	DDD_EXACT_MATCH_ON_REMOVE
	DDD_RAW_TARGET_PATH
	DDD_REMOVE_DEFINITION

=item C<":DRIVE_">

Constants returned by C<GetDriveType>.

	DRIVE_UNKNOWN		DRIVE_NO_ROOT_DIR	DRIVE_REMOVABLE
	DRIVE_FIXED		DRIVE_REMOTE		DRIVE_CDROM
	DRIVE_RAMDISK

=item C<":FILE_">

Specific types of access to files that can be requested via the
C<$uAccess> argument to C<CreateFile>.

	FILE_READ_DATA			FILE_LIST_DIRECTORY
	FILE_WRITE_DATA			FILE_ADD_FILE
	FILE_APPEND_DATA		FILE_ADD_SUBDIRECTORY
	FILE_CREATE_PIPE_INSTANCE	FILE_READ_EA
	FILE_WRITE_EA			FILE_EXECUTE
	FILE_TRAVERSE			FILE_DELETE_CHILD
	FILE_READ_ATTRIBUTES		FILE_WRITE_ATTRIBUTES
	FILE_ALL_ACCESS			FILE_GENERIC_READ
	FILE_GENERIC_WRITE		FILE_GENERIC_EXECUTE )],

=item C<":FILE_ATTRIBUTE_">

File attribute constants.  Returned by C<attrLetsToBits> and used in
the C<$uFlags> argument to C<CreateFile>.

	FILE_ATTRIBUTE_ARCHIVE			FILE_ATTRIBUTE_COMPRESSED
	FILE_ATTRIBUTE_HIDDEN			FILE_ATTRIBUTE_NORMAL
	FILE_ATTRIBUTE_OFFLINE			FILE_ATTRIBUTE_READONLY
	FILE_ATTRIBUTE_SYSTEM			FILE_ATTRIBUTE_TEMPORARY

In addition, C<GetFileAttributes> can return these constants (or
INVALID_FILE_ATTRIBUTES in case of an error).

	FILE_ATTRIBUTE_DEVICE			FILE_ATTRIBUTE_DIRECTORY
	FILE_ATTRIBUTE_ENCRYPTED		FILE_ATTRIBUTE_NOT_CONTENT_INDEXED
	FILE_ATTRIBUTE_REPARSE_POINT	FILE_ATTRIBUTE_SPARSE_FILE

=item C<":FILE_FLAG_">

File option flag constants.  Used in the C<$uFlags> argument to
C<CreateFile>.

	FILE_FLAG_BACKUP_SEMANTICS	FILE_FLAG_DELETE_ON_CLOSE
	FILE_FLAG_NO_BUFFERING		FILE_FLAG_OVERLAPPED
	FILE_FLAG_POSIX_SEMANTICS	FILE_FLAG_RANDOM_ACCESS
	FILE_FLAG_SEQUENTIAL_SCAN	FILE_FLAG_WRITE_THROUGH
	FILE_FLAG_OPEN_REPARSE_POINT

=item C<":FILE_SHARE_">

File sharing constants.  Used in the C<$uShare> argument to
C<CreateFile>.

	FILE_SHARE_DELETE	FILE_SHARE_READ		FILE_SHARE_WRITE

=item C<":FILE_TYPE_">

File type constants.  Returned by C<GetFileType>.

	FILE_TYPE_CHAR		FILE_TYPE_DISK
	FILE_TYPE_PIPE		FILE_TYPE_UNKNOWN

=item C<":FS_">

File system characteristics constants.  Placed in the C<$ouFsFlags>
argument to C<GetVolumeInformation>.

	FS_CASE_IS_PRESERVED		FS_CASE_SENSITIVE
	FS_UNICODE_STORED_ON_DISK	FS_PERSISTENT_ACLS 
	FS_FILE_COMPRESSION		FS_VOL_IS_COMPRESSED

=item C<":HANDLE_FLAG_">

Flag bits modifying the behavior of an object handle and accessed via
C<GetHandleInformation> and C<SetHandleInformation>.

=over

=item HANDLE_FLAG_INHERIT

If this bit is set, then children of this process who inherit handles
[that is, processes created by calls to the Win32 C<CreateProcess> API
with the C<bInheritHandles> parameter specified as C<TRUE>], will inherit
this particular object handle.

=item HANDLE_FLAG_PROTECT_FROM_CLOSE

If this bit is set, then calls to C<CloseHandle> against this handle
will be ignored, leaving the handle open and usable.

=back

=item C<":IOCTL_STORAGE_">

I/O control operations for generic storage devices.  Used in the
C<$uIoControlCode> argument to C<DeviceIoControl>.  Includes
C<IOCTL_STORAGE_CHECK_VERIFY>, C<IOCTL_STORAGE_MEDIA_REMOVAL>,
C<IOCTL_STORAGE_EJECT_MEDIA>, C<IOCTL_STORAGE_LOAD_MEDIA>,
C<IOCTL_STORAGE_RESERVE>, C<IOCTL_STORAGE_RELEASE>,
C<IOCTL_STORAGE_FIND_NEW_DEVICES>, and
C<IOCTL_STORAGE_GET_MEDIA_TYPES>.

=over

=item C<IOCTL_STORAGE_CHECK_VERIFY>

Verify that a device's media is accessible.  C<$pInBuf> and C<$opOutBuf>
should both be C<[]>.  If C<DeviceIoControl> returns a true value, then
the media is currently accessible.

=item C<IOCTL_STORAGE_MEDIA_REMOVAL>

Allows the device's media to be locked or unlocked.  C<$opOutBuf> should
be C<[]>.  C<$pInBuf> should be a C<PREVENT_MEDIA_REMOVAL> data structure,
which is simply an interger containing a boolean value:

    $pInBuf= pack( "i", $bPreventMediaRemoval );

=item C<IOCTL_STORAGE_EJECT_MEDIA>

Requests that the device eject the media.  C<$pInBuf> and C<$opOutBuf>
should both be C<[]>.  

=item C<IOCTL_STORAGE_LOAD_MEDIA>

Requests that the device load the media.  C<$pInBuf> and C<$opOutBuf>
should both be C<[]>.

=item C<IOCTL_STORAGE_RESERVE>

Requests that the device be reserved.  C<$pInBuf> and C<$opOutBuf>
should both be C<[]>.

=item C<IOCTL_STORAGE_RELEASE>

Releases a previous device reservation.  C<$pInBuf> and C<$opOutBuf>
should both be C<[]>.

=item C<IOCTL_STORAGE_FIND_NEW_DEVICES>

No documentation on this IOCTL operation was found.

=item C<IOCTL_STORAGE_GET_MEDIA_TYPES>

Requests information about the type of media supported by the device. 
C<$pInBuf> should be C<[]>.  C<$opOutBuf> will be set to contain a
vector of C<DISK_GEOMETRY> data structures, which can be decoded via:

    # Calculate the number of DISK_GEOMETRY structures returned:
    my $cStructs= length($opOutBuf)/(4+4+4+4+4+4);
    my @fields= unpack( "L l I L L L" x $cStructs, $opOutBuf )
    my( @ucCylsLow, @ivcCylsHigh, @uMediaType, @uTracksPerCyl,
      @uSectsPerTrack, @uBytesPerSect )= ();
    while(  @fields  ) {
	push( @ucCylsLow, unshift @fields );
	push( @ivcCylsHigh, unshift @fields );
	push( @uMediaType, unshift @fields );
	push( @uTracksPerCyl, unshift @fields );
	push( @uSectsPerTrack, unshift @fields );
	push( @uBytesPerSect, unshift @fields );
    }

For the C<$i>th type of supported media, the following variables will
contain the following data.

=over

=item C<$ucCylsLow[$i]>

The low-order 4 bytes of the total number of cylinders.

=item C<$ivcCylsHigh[$i]> 

The high-order 4 bytes of the total number of cylinders.

=item C<$uMediaType[$i]>

A code for the type of media.  See the C<":MEDIA_TYPE"> export class.

=item C<$uTracksPerCyl[$i]>

The number of tracks in each cylinder.

=item C<$uSectsPerTrack[$i]>

The number of sectors in each track.

=item C<$uBytesPerSect[$i]>

The number of bytes in each sector.

=back

=back

=item C<":IOCTL_DISK_">

I/O control operations for disk devices.  Used in the C<$uIoControlCode>
argument to C<DeviceIoControl>.  Most of these are to be used on
physical drive devices like C<"//./PhysicalDrive0">.  However,
C<IOCTL_DISK_GET_PARTITION_INFO> and C<IOCTL_DISK_SET_PARTITION_INFO>
should only be used on a single-partition device like C<"//./C:">.  Also,
C<IOCTL_DISK_GET_MEDIA_TYPES> is documented as having been superceded but
is still useful when used on a floppy device like C<"//./A:">.

Includes C<IOCTL_DISK_FORMAT_TRACKS>, C<IOCTL_DISK_FORMAT_TRACKS_EX>,
C<IOCTL_DISK_GET_DRIVE_GEOMETRY>, C<IOCTL_DISK_GET_DRIVE_LAYOUT>,
C<IOCTL_DISK_GET_MEDIA_TYPES>, C<IOCTL_DISK_GET_PARTITION_INFO>,
C<IOCTL_DISK_HISTOGRAM_DATA>, C<IOCTL_DISK_HISTOGRAM_RESET>,
C<IOCTL_DISK_HISTOGRAM_STRUCTURE>, C<IOCTL_DISK_IS_WRITABLE>,
C<IOCTL_DISK_LOGGING>, C<IOCTL_DISK_PERFORMANCE>,
C<IOCTL_DISK_REASSIGN_BLOCKS>, C<IOCTL_DISK_REQUEST_DATA>,
C<IOCTL_DISK_REQUEST_STRUCTURE>, C<IOCTL_DISK_SET_DRIVE_LAYOUT>,
C<IOCTL_DISK_SET_PARTITION_INFO>, and C<IOCTL_DISK_VERIFY>.

=over

=item C<IOCTL_DISK_GET_DRIVE_GEOMETRY>

Request information about the size and geometry of the disk.  C<$pInBuf>
should be C<[]>.  C<$opOutBuf> will be set to a C<DISK_GEOMETRY> data
structure which can be decode via:

    ( $ucCylsLow, $ivcCylsHigh, $uMediaType, $uTracksPerCyl,
      $uSectsPerTrack, $uBytesPerSect )= unpack( "L l I L L L", $opOutBuf );

=over

=item C<$ucCylsLow>

The low-order 4 bytes of the total number of cylinders.

=item C<$ivcCylsHigh> 

The high-order 4 bytes of the total number of cylinders.

=item C<$uMediaType>

A code for the type of media.  See the C<":MEDIA_TYPE"> export class.

=item C<$uTracksPerCyl>

The number of tracks in each cylinder.

=item C<$uSectsPerTrack>

The number of sectors in each track.

=item C<$uBytesPerSect>

The number of bytes in each sector.

=back

=item C<IOCTL_DISK_GET_PARTITION_INFO>

Request information about the size and geometry of the partition. 
C<$pInBuf> should be C<[]>.  C<$opOutBuf> will be set to a
C<PARTITION_INFORMATION> data structure which can be decode via:

    ( $uStartLow, $ivStartHigh, $ucHiddenSects, $uPartitionSeqNumber,
      $uPartitionType, $bActive, $bRecognized, $bToRewrite )=
      unpack( "L l L L C c c c", $opOutBuf );

=over

=item C<$uStartLow> and C<$ivStartHigh>

The low-order and high-order [respectively] 4 bytes of the starting
offset of the partition, measured in bytes.

=item C<$ucHiddenSects>

The number of "hidden" sectors for this partition.  Actually this is
the number of sectors found prior to this partiton, that is, the
starting offset [as found in C<$uStartLow> and C<$ivStartHigh>]
divided by the number of bytes per sector.

=item C<$uPartitionSeqNumber>

The sequence number of this partition.  Partitions are numbered
starting as C<1> [with "partition 0" meaning the entire disk].  
Sometimes this field may be C<0> and you'll have to infer the
partition sequence number from how many partitions preceed it on
the disk.

=item C<$uPartitionType>

The type of partition.  See the C<":PARTITION_"> export class for a
list of known types.  See also C<IsRecognizedPartition> and
C<IsContainerPartition>.

=item C<$bActive>

C<1> for the active [boot] partition, C<0> otherwise.

=item C<$bRecognized>

Whether this type of partition is support under Win32.

=item C<$bToRewrite>

Whether to update this partition information.  This field is not used
by C<IOCTL_DISK_GET_PARTITION_INFO>.  For
C<IOCTL_DISK_SET_DRIVE_LAYOUT>, you must set this field to a true
value for any partitions you wish to have changed, added, or deleted.

=back

=item C<IOCTL_DISK_SET_PARTITION_INFO>

Change the type of the partition.  C<$opOutBuf> should be C<[]>.
C<$pInBuf> should be a C<SET_PARTITION_INFORMATION> data structure
which is just a single byte containing the new parition type [see
the C<":PARTITION_"> export class for a list of known types]:

    $pInBuf= pack( "C", $uPartitionType );

=item C<IOCTL_DISK_GET_DRIVE_LAYOUT>

Request information about the disk layout.  C<$pInBuf> should be C<[]>.
C<$opOutBuf> will be set to contain C<DRIVE_LAYOUT_INFORMATION>
structure including several C<PARTITION_INFORMATION> structures:

    my( $cPartitions, $uDiskSignature )= unpack( "L L", $opOutBuf );
    my @fields= unpack( "x8" . ( "L l L L C c c c" x $cPartitions ),
		        $opOutBuf );
    my( @uStartLow, @ivStartHigh, @ucHiddenSects,
      @uPartitionSeqNumber, @uPartitionType, @bActive,
      @bRecognized, @bToRewrite )= ();
    for(  1..$cPartition  ) {
	push( @uStartLow, unshift @fields );
	push( @ivStartHigh, unshift @fields );
	push( @ucHiddenSects, unshift @fields );
	push( @uPartitionSeqNumber, unshift @fields );
	push( @uPartitionType, unshift @fields );
	push( @bActive, unshift @fields );
	push( @bRecognized, unshift @fields );
	push( @bToRewrite, unshift @fields );
    }

=over

=item C<$cPartitions>

If the number of partitions on the disk.

=item C<$uDiskSignature>

Is the disk signature, a unique number assigned by Disk Administrator
[F<WinDisk.exe>] and used to identify the disk.  This allows drive
letters for partitions on that disk to remain constant even if the
SCSI Target ID of the disk gets changed.

=back

See C<IOCTL_DISK_GET_PARTITION_INFORMATION> for information on the
remaining these fields.

=item C<IOCTL_DISK_GET_MEDIA_TYPES>

Is supposed to be superseded by C<IOCTL_STORAGE_GET_MEDIA_TYPES> but
is still useful for determining the types of floppy diskette formats
that can be produced by a given floppy drive.  See
F<ex/FormatFloppy.plx> for an example.

=item C<IOCTL_DISK_SET_DRIVE_LAYOUT>

Change the partition layout of the disk.  C<$pOutBuf> should be C<[]>.
C<$pInBuf> should be a C<DISK_LAYOUT_INFORMATION> data structure
including several C<PARTITION_INFORMATION> data structures.

    # Already set:  $cPartitions, $uDiskSignature, @uStartLow, @ivStartHigh,
    #   @ucHiddenSects, @uPartitionSeqNumber, @uPartitionType, @bActive,
    #   @bRecognized, and @bToRewrite.
    my( @fields, $prtn )= ();
    for $prtn (  1..$cPartition  ) {
	push( @fields, $uStartLow[$prtn-1], $ivStartHigh[$prtn-1],
	    $ucHiddenSects[$prtn-1], $uPartitionSeqNumber[$prtn-1],
	    $uPartitionType[$prtn-1], $bActive[$prtn-1],
	    $bRecognized[$prtn-1], $bToRewrite[$prtn-1] );
    }
    $pInBuf= pack( "L L" . ( "L l L L C c c c" x $cPartitions ),
		   $cPartitions, $uDiskSignature, @fields );

To delete a partition, zero out all fields except for C<$bToRewrite>
which should be set to C<1>.  To add a partition, increment
C<$cPartitions> and add the information for the new partition
into the arrays, making sure that you insert C<1> into @bToRewrite.

See C<IOCTL_DISK_GET_DRIVE_LAYOUT> and
C<IOCTL_DISK_GET_PARITITON_INFORMATION> for descriptions of the
fields.

=item C<IOCTL_DISK_VERIFY>

Performs a logical format of [part of] the disk.  C<$opOutBuf> should
be C<[]>.  C<$pInBuf> should contain a C<VERIFY_INFORMATION> data
structure:

    $pInBuf= pack( "L l L",
		   $uStartOffsetLow, $ivStartOffsetHigh, $uLength );

=over

=item C<$uStartOffsetLow> and C<$ivStartOffsetHigh>

The low-order and high-order [respectively] 4 bytes of the offset [in
bytes] where the formatting should begin.

=item C<$uLength>

The length [in bytes] of the section to be formatted.

=back

=item C<IOCTL_DISK_FORMAT_TRACKS>

Format a range of tracks on the disk.  C<$opOutBuf> should be C<[]>. 
C<$pInBuf> should contain a C<FORMAT_PARAMETERS> data structure:

    $pInBuf= pack( "L L L L L", $uMediaType,
		   $uStartCyl, $uEndCyl, $uStartHead, $uEndHead );

C<$uMediaType> if the type of media to be formatted.  Mostly used to
specify the density to use when formatting a floppy diskette.  See the
C<":MEDIA_TYPE"> export class for more information.

The remaining fields specify the starting and ending cylinder and
head of the range of tracks to be formatted.

=item C<IOCTL_DISK_REASSIGN_BLOCKS>

Reassign a list of disk blocks to the disk's spare-block pool. 
C<$opOutBuf> should be C<[]>.  C<$pInBuf> should be a
C<REASSIGN_BLOCKS> data structure:

    $pInBuf= pack( "S S L*", 0, $cBlocks, @uBlockNumbers );

=item C<IOCTL_DISK_PERFORMANCE>

Request information about disk performance.  C<$pInBuf> should be C<[]>.
C<$opOutBuf> will be set to contain a C<DISK_PERFORMANCE> data structure:

    my( $ucBytesReadLow, $ivcBytesReadHigh,
	$ucBytesWrittenLow, $ivcBytesWrittenHigh,
	$uReadTimeLow, $ivReadTimeHigh,
	$uWriteTimeLow, $ivWriteTimeHigh,
	$ucReads, $ucWrites, $uQueueDepth )=
	unpack( "L l L l L l L l L L L", $opOutBuf );

=item C<IOCTL_DISK_IS_WRITABLE>

No documentation on this IOCTL operation was found.

=item C<IOCTL_DISK_LOGGING>

Control disk logging.  Little documentation for this IOCTL operation
was found.  It makes use of a C<DISK_LOGGING> data structure:

=over

=item DISK_LOGGING_START

Start logging each disk request in a buffer internal to the disk device
driver of size C<$uLogBufferSize>:

    $pInBuf= pack( "C L L", 0, 0, $uLogBufferSize );

=item DISK_LOGGING_STOP

Stop loggin each disk request:

    $pInBuf= pack( "C L L", 1, 0, 0 );

=item DISK_LOGGING_DUMP

Copy the interal log into the supplied buffer:

    $pLogBuffer= ' ' x $uLogBufferSize
    $pInBuf= pack( "C P L", 2, $pLogBuffer, $uLogBufferSize );

    ( $uByteOffsetLow[$i], $ivByteOffsetHigh[$i],
      $uStartTimeLow[$i], $ivStartTimeHigh[$i],
      $uEndTimeLog[$i], $ivEndTimeHigh[$i],
      $hVirtualAddress[$i], $ucBytes[$i],
      $uDeviceNumber[$i], $bWasReading[$i] )=
      unpack( "x".(8+8+8+4+4+1+1+2)." L l L l L l L L C c x2", $pLogBuffer );

=item DISK_LOGGING_BINNING

Keep statics grouped into bins based on request sizes.

    $pInBuf= pack( "C P L", 3, $pUnknown, $uUnknownSize );

=back

=item C<IOCTL_DISK_FORMAT_TRACKS_EX>

No documentation on this IOCTL is included.

=item C<IOCTL_DISK_HISTOGRAM_STRUCTURE>

No documentation on this IOCTL is included.

=item C<IOCTL_DISK_HISTOGRAM_DATA>

No documentation on this IOCTL is included.

=item C<IOCTL_DISK_HISTOGRAM_RESET>

No documentation on this IOCTL is included.

=item C<IOCTL_DISK_REQUEST_STRUCTURE>

No documentation on this IOCTL operation was found.

=item C<IOCTL_DISK_REQUEST_DATA>

No documentation on this IOCTL operation was found.

=back

=item C<":FSCTL_">

File system control operations.  Used in the C<$uIoControlCode>
argument to C<DeviceIoControl>.

Includes C<FSCTL_SET_REPARSE_POINT>, C<FSCTL_GET_REPARSE_POINT>,
C<FSCTL_DELETE_REPARSE_POINT>.

=over

=item C<FSCTL_SET_REPARSE_POINT>

Sets reparse point data to be associated with $hDevice.

=item C<FSCTL_GET_REPARSE_POINT>

Retrieves the reparse point data associated with $hDevice.

=item C<FSCTL_DELETE_REPARSE_POINT>

Deletes the reparse point data associated with $hDevice.

=back

=item C<":GENERIC_">

Constants specifying generic access permissions that are not specific
to one type of object.

	GENERIC_ALL			GENERIC_EXECUTE
	GENERIC_READ		GENERIC_WRITE

=item C<":MEDIA_TYPE">

Different classes of media that a device can support.  Used in the
C<$uMediaType> field of a C<DISK_GEOMETRY> structure.

=over

=item C<Unknown>

Format is unknown.

=item C<F5_1Pt2_512>

5.25" floppy, 1.2MB [really 1,200KB] total space, 512 bytes/sector.

=item C<F3_1Pt44_512>

3.5" floppy, 1.44MB [really 1,440KB] total space, 512 bytes/sector.

=item C<F3_2Pt88_512>

3.5" floppy, 2.88MB [really 2,880KB] total space, 512 bytes/sector.

=item C<F3_20Pt8_512>

3.5" floppy, 20.8MB total space, 512 bytes/sector.

=item C<F3_720_512>

3.5" floppy, 720KB total space, 512 bytes/sector.

=item C<F5_360_512>

5.25" floppy, 360KB total space, 512 bytes/sector.

=item C<F5_320_512>

5.25" floppy, 320KB total space, 512 bytes/sector.

=item C<F5_320_1024>

5.25" floppy, 320KB total space, 1024 bytes/sector.

=item C<F5_180_512>

5.25" floppy, 180KB total space, 512 bytes/sector.

=item C<F5_160_512>

5.25" floppy, 160KB total space, 512 bytes/sector.

=item C<RemovableMedia>

Some type of removable media other than a floppy diskette.

=item C<FixedMedia>

A fixed hard disk.

=item C<F3_120M_512>

3.5" floppy, 120MB total space.

=back

=item C<":MOVEFILE_">

Constants for use in C<$uFlags> arguments to C<MoveFileEx>.

	MOVEFILE_COPY_ALLOWED		MOVEFILE_DELAY_UNTIL_REBOOT
	MOVEFILE_REPLACE_EXISTING	MOVEFILE_WRITE_THROUGH

=item C<":SECURITY_">

Security quality of service values that can be used in the C<$uFlags>
argument to C<CreateFile> if opening the client side of a named pipe.

	SECURITY_ANONYMOUS		SECURITY_CONTEXT_TRACKING
	SECURITY_DELEGATION		SECURITY_EFFECTIVE_ONLY
	SECURITY_IDENTIFICATION		SECURITY_IMPERSONATION
	SECURITY_SQOS_PRESENT

=item C<":SEM_">

Constants to be used with C<SetErrorMode>.

	SEM_FAILCRITICALERRORS		SEM_NOGPFAULTERRORBOX
	SEM_NOALIGNMENTFAULTEXCEPT	SEM_NOOPENFILEERRORBOX

=item C<":PARTITION_">

Constants describing partition types.

	PARTITION_ENTRY_UNUSED		PARTITION_FAT_12
	PARTITION_XENIX_1		PARTITION_XENIX_2
	PARTITION_FAT_16		PARTITION_EXTENDED
	PARTITION_HUGE			PARTITION_IFS
	PARTITION_FAT32			PARTITION_FAT32_XINT13
	PARTITION_XINT13		PARTITION_XINT13_EXTENDED
	PARTITION_PREP			PARTITION_UNIX
	VALID_NTFT			PARTITION_NTFT

=item C<":ALL">

All of the above.

=back

=head1 BUGS

None known at this time.

=head1 AUTHOR

Tye McQueen, tye@metronet.com, http://www.metronet.com/~tye/.

=head1 SEE ALSO

The pyramids.

=cut
