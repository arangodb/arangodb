package Win32;

BEGIN {
    use strict;
    use vars qw|$VERSION $XS_VERSION @ISA @EXPORT @EXPORT_OK|;

    require Exporter;
    require DynaLoader;

    @ISA = qw|Exporter DynaLoader|;
    $VERSION = '0.36';
    $XS_VERSION = $VERSION;
    $VERSION = eval $VERSION;

    @EXPORT = qw(
	NULL
	WIN31_CLASS
	OWNER_SECURITY_INFORMATION
	GROUP_SECURITY_INFORMATION
	DACL_SECURITY_INFORMATION
	SACL_SECURITY_INFORMATION
	MB_ICONHAND
	MB_ICONQUESTION
	MB_ICONEXCLAMATION
	MB_ICONASTERISK
	MB_ICONWARNING
	MB_ICONERROR
	MB_ICONINFORMATION
	MB_ICONSTOP
    );
    @EXPORT_OK = qw(
        GetOSName
        SW_HIDE
        SW_SHOWNORMAL
        SW_SHOWMINIMIZED
        SW_SHOWMAXIMIZED
        SW_SHOWNOACTIVATE

        CSIDL_DESKTOP
        CSIDL_PROGRAMS
        CSIDL_PERSONAL
        CSIDL_FAVORITES
        CSIDL_STARTUP
        CSIDL_RECENT
        CSIDL_SENDTO
        CSIDL_STARTMENU
        CSIDL_MYMUSIC
        CSIDL_MYVIDEO
        CSIDL_DESKTOPDIRECTORY
        CSIDL_NETHOOD
        CSIDL_FONTS
        CSIDL_TEMPLATES
        CSIDL_COMMON_STARTMENU
        CSIDL_COMMON_PROGRAMS
        CSIDL_COMMON_STARTUP
        CSIDL_COMMON_DESKTOPDIRECTORY
        CSIDL_APPDATA
        CSIDL_PRINTHOOD
        CSIDL_LOCAL_APPDATA
        CSIDL_COMMON_FAVORITES
        CSIDL_INTERNET_CACHE
        CSIDL_COOKIES
        CSIDL_HISTORY
        CSIDL_COMMON_APPDATA
        CSIDL_WINDOWS
        CSIDL_SYSTEM
        CSIDL_PROGRAM_FILES
        CSIDL_MYPICTURES
        CSIDL_PROFILE
        CSIDL_PROGRAM_FILES_COMMON
        CSIDL_COMMON_TEMPLATES
        CSIDL_COMMON_DOCUMENTS
        CSIDL_COMMON_ADMINTOOLS
        CSIDL_ADMINTOOLS
        CSIDL_COMMON_MUSIC
        CSIDL_COMMON_PICTURES
        CSIDL_COMMON_VIDEO
        CSIDL_RESOURCES
        CSIDL_RESOURCES_LOCALIZED
        CSIDL_CDBURN_AREA
    );
}

# We won't bother with the constant stuff, too much of a hassle.  Just hard
# code it here.

sub NULL 				{ 0 }
sub WIN31_CLASS 			{ &NULL }

sub OWNER_SECURITY_INFORMATION		{ 0x00000001 }
sub GROUP_SECURITY_INFORMATION		{ 0x00000002 }
sub DACL_SECURITY_INFORMATION		{ 0x00000004 }
sub SACL_SECURITY_INFORMATION		{ 0x00000008 }

sub MB_ICONHAND				{ 0x00000010 }
sub MB_ICONQUESTION			{ 0x00000020 }
sub MB_ICONEXCLAMATION			{ 0x00000030 }
sub MB_ICONASTERISK			{ 0x00000040 }
sub MB_ICONWARNING			{ 0x00000030 }
sub MB_ICONERROR			{ 0x00000010 }
sub MB_ICONINFORMATION			{ 0x00000040 }
sub MB_ICONSTOP				{ 0x00000010 }

#
# Newly added constants.  These have an empty prototype, unlike the
# the ones above, which aren't prototyped for compatibility reasons.
#
sub SW_HIDE           ()		{ 0 }
sub SW_SHOWNORMAL     ()		{ 1 }
sub SW_SHOWMINIMIZED  ()		{ 2 }
sub SW_SHOWMAXIMIZED  ()		{ 3 }
sub SW_SHOWNOACTIVATE ()		{ 4 }

sub CSIDL_DESKTOP              ()       { 0x0000 }     # <desktop>
sub CSIDL_PROGRAMS             ()       { 0x0002 }     # Start Menu\Programs
sub CSIDL_PERSONAL             ()       { 0x0005 }     # "My Documents" folder
sub CSIDL_FAVORITES            ()       { 0x0006 }     # <user name>\Favorites
sub CSIDL_STARTUP              ()       { 0x0007 }     # Start Menu\Programs\Startup
sub CSIDL_RECENT               ()       { 0x0008 }     # <user name>\Recent
sub CSIDL_SENDTO               ()       { 0x0009 }     # <user name>\SendTo
sub CSIDL_STARTMENU            ()       { 0x000B }     # <user name>\Start Menu
sub CSIDL_MYMUSIC              ()       { 0x000D }     # "My Music" folder
sub CSIDL_MYVIDEO              ()       { 0x000E }     # "My Videos" folder
sub CSIDL_DESKTOPDIRECTORY     ()       { 0x0010 }     # <user name>\Desktop
sub CSIDL_NETHOOD              ()       { 0x0013 }     # <user name>\nethood
sub CSIDL_FONTS                ()       { 0x0014 }     # windows\fonts
sub CSIDL_TEMPLATES            ()       { 0x0015 }
sub CSIDL_COMMON_STARTMENU     ()       { 0x0016 }     # All Users\Start Menu
sub CSIDL_COMMON_PROGRAMS      ()       { 0x0017 }     # All Users\Start Menu\Programs
sub CSIDL_COMMON_STARTUP       ()       { 0x0018 }     # All Users\Startup
sub CSIDL_COMMON_DESKTOPDIRECTORY ()    { 0x0019 }     # All Users\Desktop
sub CSIDL_APPDATA              ()       { 0x001A }     # Application Data, new for NT4
sub CSIDL_PRINTHOOD            ()       { 0x001B }     # <user name>\PrintHood
sub CSIDL_LOCAL_APPDATA        ()       { 0x001C }     # non roaming, user\Local Settings\Application Data
sub CSIDL_COMMON_FAVORITES     ()       { 0x001F }
sub CSIDL_INTERNET_CACHE       ()       { 0x0020 }
sub CSIDL_COOKIES              ()       { 0x0021 }
sub CSIDL_HISTORY              ()       { 0x0022 }
sub CSIDL_COMMON_APPDATA       ()       { 0x0023 }     # All Users\Application Data
sub CSIDL_WINDOWS              ()       { 0x0024 }     # GetWindowsDirectory()
sub CSIDL_SYSTEM               ()       { 0x0025 }     # GetSystemDirectory()
sub CSIDL_PROGRAM_FILES        ()       { 0x0026 }     # C:\Program Files
sub CSIDL_MYPICTURES           ()       { 0x0027 }     # "My Pictures", new for Win2K
sub CSIDL_PROFILE              ()       { 0x0028 }     # USERPROFILE
sub CSIDL_PROGRAM_FILES_COMMON ()       { 0x002B }     # C:\Program Files\Common
sub CSIDL_COMMON_TEMPLATES     ()       { 0x002D }     # All Users\Templates
sub CSIDL_COMMON_DOCUMENTS     ()       { 0x002E }     # All Users\Documents
sub CSIDL_COMMON_ADMINTOOLS    ()       { 0x002F }     # All Users\Start Menu\Programs\Administrative Tools
sub CSIDL_ADMINTOOLS           ()       { 0x0030 }     # <user name>\Start Menu\Programs\Administrative Tools
sub CSIDL_COMMON_MUSIC         ()       { 0x0035 }     # All Users\My Music
sub CSIDL_COMMON_PICTURES      ()       { 0x0036 }     # All Users\My Pictures
sub CSIDL_COMMON_VIDEO         ()       { 0x0037 }     # All Users\My Video
sub CSIDL_RESOURCES            ()       { 0x0038 }     # %windir%\Resources\, For theme and other windows resources.
sub CSIDL_RESOURCES_LOCALIZED  ()       { 0x0039 }     # %windir%\Resources\<LangID>, for theme and other windows specific resources.
sub CSIDL_CDBURN_AREA          ()       { 0x003B }     # <user name>\Local Settings\Application Data\Microsoft\CD Burning

### This method is just a simple interface into GetOSVersion().  More
### specific or demanding situations should use that instead.

my ($found_os, $found_desc);

sub GetOSName {
    my ($os,$desc,$major, $minor, $build, $id)=("","");
    unless (defined $found_os) {
        # If we have a run this already, we have the results cached
        # If so, return them

        # Use the standard API call to determine the version
        ($desc, $major, $minor, $build, $id) = Win32::GetOSVersion();

        # If id==0 then its a win32s box -- Meaning Win3.11
        unless($id) {
            $os = 'Win32s';
        }
	else {
	    # Magic numbers from MSDN documentation of OSVERSIONINFO
	    # Most version names can be parsed from just the id and minor
	    # version
	    $os = {
		1 => {
		    0  => "95",
		    10 => "98",
		    90 => "Me"
		},
		2 => {
		    0  => "NT4",
		    1  => "XP/.Net",
		    2  => "2003",
		    51 => "NT3.51"
		}
	    }->{$id}->{$minor};
	}

        # This _really_ shouldnt happen.  At least not for quite a while
        # Politely warn and return undef
        unless (defined $os) {
            warn qq[Windows version [$id:$major:$minor] unknown!];
            return undef;
        }

        my $tag = "";

        # But distinguising W2k and Vista from NT4 requires looking at the major version
        if ($os eq "NT4") {
	    $os = {5 => "2000", 6 => "Vista"}->{$major} || "NT4";
        }

        # For the rest we take a look at the build numbers and try to deduce
	# the exact release name, but we put that in the $desc
        elsif ($os eq "95") {
            if ($build eq '67109814') {
                    $tag = '(a)';
            }
	    elsif ($build eq '67306684') {
                    $tag = '(b1)';
            }
	    elsif ($build eq '67109975') {
                    $tag = '(b2)';
            }
        }
	elsif ($os eq "98" && $build eq '67766446') {
            $tag = '(2nd ed)';
        }

	if (length $tag) {
	    if (length $desc) {
	        $desc = "$tag $desc";
	    }
	    else {
	        $desc = $tag;
	    }
	}

        # cache the results, so we dont have to do this again
        $found_os      = "Win$os";
        $found_desc    = $desc;
    }

    return wantarray ? ($found_os, $found_desc) : $found_os;
}

# "no warnings 'redefine';" doesn't work for 5.8.7 and earlier
local $^W = 0;
bootstrap Win32;

1;

__END__

=head1 NAME

Win32 - Interfaces to some Win32 API Functions

=head1 DESCRIPTION

The Win32 module contains functions to access Win32 APIs.

=head2 Alphabetical Listing of Win32 Functions

It is recommended to C<use Win32;> before any of these functions;
however, for backwards compatibility, those marked as [CORE] will
automatically do this for you.

In the function descriptions below the term I<Unicode string> is used
to indicate that the string may contain characters outside the system
codepage.  The caveat I<If supported by the core Perl version>
generally means Perl 5.8.9 and later, though some Unicode pathname
functionality may work on earlier versions.

=over

=item Win32::AbortSystemShutdown(MACHINE)

Aborts a system shutdown (started by the
InitiateSystemShutdown function) on the specified MACHINE.

=item Win32::BuildNumber()

[CORE] Returns the ActivePerl build number.  This function is
only available in the ActivePerl binary distribution.

=item Win32::CopyFile(FROM, TO, OVERWRITE)

[CORE] The Win32::CopyFile() function copies an existing file to a new
file.  All file information like creation time and file attributes will
be copied to the new file.  However it will B<not> copy the security
information.  If the destination file already exists it will only be
overwritten when the OVERWRITE parameter is true.  But even this will
not overwrite a read-only file; you have to unlink() it first
yourself.

=item Win32::CreateDirectory(DIRECTORY)

Creates the DIRECTORY and returns a true value on success.  Check $^E
on failure for extended error information.

DIRECTORY may contain Unicode characters outside the system codepage.
Once the directory has been created you can use
Win32::GetANSIPathName() to get a name that can be passed to system
calls and external programs.

=item Win32::CreateFile(FILE)

Creates the FILE and returns a true value on success.  Check $^E on
failure for extended error information.

FILE may contain Unicode characters outside the system codepage.  Once
the file has been created you can use Win32::GetANSIPathName() to get
a name that can be passed to system calls and external programs.

=item Win32::DomainName()

[CORE] Returns the name of the Microsoft Network domain or workgroup
that the owner of the current perl process is logged into.  The
"Workstation" service must be running to determine this
information.  This function does B<not> work on Windows 9x.

=item Win32::ExpandEnvironmentStrings(STRING)

Takes STRING and replaces all referenced environment variable
names with their defined values.  References to environment variables
take the form C<%VariableName%>.  Case is ignored when looking up the
VariableName in the environment.  If the variable is not found then the
original C<%VariableName%> text is retained.  Has the same effect
as the following:

	$string =~ s/%([^%]*)%/$ENV{$1} || "%$1%"/eg

However, this function may return a Unicode string if the environment
variable being expanded hasn't been assigned to via %ENV.  Access
to %ENV is currently always using byte semantics.

=item Win32::FormatMessage(ERRORCODE)

[CORE] Converts the supplied Win32 error number (e.g. returned by
Win32::GetLastError()) to a descriptive string.  Analogous to the
perror() standard-C library function.  Note that C<$^E> used
in a string context has much the same effect.

	C:\> perl -e "$^E = 26; print $^E;"
	The specified disk or diskette cannot be accessed

=item Win32::FsType()

[CORE] Returns the name of the filesystem of the currently active
drive (like 'FAT' or 'NTFS').  In list context it returns three values:
(FSTYPE, FLAGS, MAXCOMPLEN).  FSTYPE is the filesystem type as
before.  FLAGS is a combination of values of the following table:

	0x00000001  supports case-sensitive filenames
	0x00000002  preserves the case of filenames
	0x00000004  supports Unicode in filenames
	0x00000008  preserves and enforces ACLs
	0x00000010  supports file-based compression
	0x00000020  supports disk quotas
	0x00000040  supports sparse files
	0x00000080  supports reparse points
	0x00000100  supports remote storage
	0x00008000  is a compressed volume (e.g. DoubleSpace)
	0x00010000  supports object identifiers
	0x00020000  supports the Encrypted File System (EFS)

MAXCOMPLEN is the maximum length of a filename component (the part
between two backslashes) on this file system.

=item Win32::FreeLibrary(HANDLE)

Unloads a previously loaded dynamic-link library.  The HANDLE is
no longer valid after this call.  See L<LoadLibrary|Win32::LoadLibrary(LIBNAME)>
for information on dynamically loading a library.

=item Win32::GetANSIPathName(FILENAME)

Returns an ANSI version of FILENAME.  This may be the short name
if the long name cannot be represented in the system codepage.

While not currently implemented, it is possible that in the future
this function will convert only parts of the path to FILENAME to a
short form.

If FILENAME doesn't exist on the filesystem, or if the filesystem
doesn't support short ANSI filenames, then this function will
translate the Unicode name into the system codepage using replacement
characters.

=item Win32::GetArchName()

Use of this function is deprecated.  It is equivalent with
$ENV{PROCESSOR_ARCHITECTURE}.  This might not work on Win9X.

=item Win32::GetChipName()

Returns the processor type: 386, 486 or 586 for Intel processors,
21064 for the Alpha chip.

=item Win32::GetCwd()

[CORE] Returns the current active drive and directory.  This function
does not return a UNC path, since the functionality required for such
a feature is not available under Windows 95.

If supported by the core Perl version, this function will return an
ANSI path name for the current directory if the long pathname cannot
be represented in the system codepage.

=item Win32::GetCurrentThreadId()

Returns the thread identifier of the calling thread.  Until the thread
terminates, the thread identifier uniquely identifies the thread
throughout the system.

Note: the current process identifier is available via the predefined
$$ variable.

=item Win32::GetFileVersion(FILENAME)

Returns the file version number from the VERSIONINFO resource of
the executable file or DLL.  This is a tuple of four 16 bit numbers.
In list context these four numbers will be returned.  In scalar context
they are concatenated into a string, separated by dots.

=item Win32::GetFolderPath(FOLDER [, CREATE])

Returns the full pathname of one of the Windows special folders.
The folder will be created if it doesn't exist and the optional CREATE
argument is true.  The following FOLDER constants are defined by the
Win32 module, but only exported on demand:

        CSIDL_ADMINTOOLS
        CSIDL_APPDATA
        CSIDL_CDBURN_AREA
        CSIDL_COMMON_ADMINTOOLS
        CSIDL_COMMON_APPDATA
        CSIDL_COMMON_DESKTOPDIRECTORY
        CSIDL_COMMON_DOCUMENTS
        CSIDL_COMMON_FAVORITES
        CSIDL_COMMON_MUSIC
        CSIDL_COMMON_PICTURES
        CSIDL_COMMON_PROGRAMS
        CSIDL_COMMON_STARTMENU
        CSIDL_COMMON_STARTUP
        CSIDL_COMMON_TEMPLATES
        CSIDL_COMMON_VIDEO
        CSIDL_COOKIES
        CSIDL_DESKTOP
        CSIDL_DESKTOPDIRECTORY
        CSIDL_FAVORITES
        CSIDL_FONTS
        CSIDL_HISTORY
        CSIDL_INTERNET_CACHE
        CSIDL_LOCAL_APPDATA
        CSIDL_MYMUSIC
        CSIDL_MYPICTURES
        CSIDL_MYVIDEO
        CSIDL_NETHOOD
        CSIDL_PERSONAL
        CSIDL_PRINTHOOD
        CSIDL_PROFILE
        CSIDL_PROGRAMS
        CSIDL_PROGRAM_FILES
        CSIDL_PROGRAM_FILES_COMMON
        CSIDL_RECENT
        CSIDL_RESOURCES
        CSIDL_RESOURCES_LOCALIZED
        CSIDL_SENDTO
        CSIDL_STARTMENU
        CSIDL_STARTUP
        CSIDL_SYSTEM
        CSIDL_TEMPLATES
        CSIDL_WINDOWS

Note that not all folders are defined on all versions of Windows.

Please refer to the MSDN documentation of the CSIDL constants,
currently available at:

http://msdn.microsoft.com/library/default.asp?url=/library/en-us/shellcc/platform/shell/reference/enums/csidl.asp

This function will return an ANSI folder path if the long name cannot
be represented in the system codepage.  Use Win32::GetLongPathName()
on the result of Win32::GetFolderPath() if you want the Unicode
version of the folder name.

=item Win32::GetFullPathName(FILENAME)

[CORE] GetFullPathName combines the FILENAME with the current drive
and directory name and returns a fully qualified (aka, absolute)
path name.  In list context it returns two elements: (PATH, FILE) where
PATH is the complete pathname component (including trailing backslash)
and FILE is just the filename part.  Note that no attempt is made to
convert 8.3 components in the supplied FILENAME to longnames or
vice-versa.  Compare with Win32::GetShortPathName() and
Win32::GetLongPathName().

If supported by the core Perl version, this function will return an
ANSI path name if the full pathname cannot be represented in the
system codepage.

=item Win32::GetLastError()

[CORE] Returns the last error value generated by a call to a Win32 API
function.  Note that C<$^E> used in a numeric context amounts to the
same value.

=item Win32::GetLongPathName(PATHNAME)

[CORE] Returns a representation of PATHNAME composed of longname
components (if any).  The result may not necessarily be longer
than PATHNAME.  No attempt is made to convert PATHNAME to the
absolute path.  Compare with Win32::GetShortPathName() and
Win32::GetFullPathName().

This function may return the pathname in Unicode if it cannot be
represented in the system codepage.  Use Win32::GetANSIPathName()
before passing the path to a system call or another program.

=item Win32::GetNextAvailDrive()

[CORE] Returns a string in the form of "<d>:" where <d> is the first
available drive letter.

=item Win32::GetOSVersion()

[CORE] Returns the list (STRING, MAJOR, MINOR, BUILD, ID), where the
elements are, respectively: An arbitrary descriptive string, the major
version number of the operating system, the minor version number, the
build number, and a digit indicating the actual operating system.
For the ID, the values are 0 for Win32s, 1 for Windows 9X/Me and 2 for
Windows NT/2000/XP/2003/Vista.  In scalar context it returns just the ID.

Currently known values for ID MAJOR and MINOR are as follows:

    OS                    ID    MAJOR   MINOR
    Win32s                 0      -       -
    Windows 95             1      4       0
    Windows 98             1      4      10
    Windows Me             1      4      90
    Windows NT 3.51        2      3      51
    Windows NT 4           2      4       0
    Windows 2000           2      5       0
    Windows XP             2      5       1
    Windows Server 2003    2      5       2
    Windows Vista          2      6       0

On Windows NT 4 SP6 and later this function returns the following
additional values: SPMAJOR, SPMINOR, SUITEMASK, PRODUCTTYPE.

SPMAJOR and SPMINOR are are the version numbers of the latest
installed service pack.

SUITEMASK is a bitfield identifying the product suites available on
the system.  Known bits are:

    VER_SUITE_SMALLBUSINESS             0x00000001
    VER_SUITE_ENTERPRISE                0x00000002
    VER_SUITE_BACKOFFICE                0x00000004
    VER_SUITE_COMMUNICATIONS            0x00000008
    VER_SUITE_TERMINAL                  0x00000010
    VER_SUITE_SMALLBUSINESS_RESTRICTED  0x00000020
    VER_SUITE_EMBEDDEDNT                0x00000040
    VER_SUITE_DATACENTER                0x00000080
    VER_SUITE_SINGLEUSERTS              0x00000100
    VER_SUITE_PERSONAL                  0x00000200
    VER_SUITE_BLADE                     0x00000400
    VER_SUITE_EMBEDDED_RESTRICTED       0x00000800
    VER_SUITE_SECURITY_APPLIANCE        0x00001000

The VER_SUITE_xxx names are listed here to crossreference the Microsoft
documentation.  The Win32 module does not provide symbolic names for these
constants.

PRODUCTTYPE provides additional information about the system.  It should
be one of the following integer values:

    1 - Workstation (NT 4, 2000 Pro, XP Home, XP Pro, Vista)
    2 - Domaincontroller
    3 - Server

=item Win32::GetOSName()

In scalar context returns the name of the Win32 operating system
being used.  In list context returns a two element list of the OS name
and whatever edition information is known about the particular build
(for Win9X boxes) and whatever service packs have been installed.
The latter is roughly equivalent to the first item returned by
GetOSVersion() in list context.

Currently the possible values for the OS name are

 Win32s Win95 Win98 WinMe WinNT3.51 WinNT4 Win2000 WinXP/.Net Win2003 WinVista

This routine is just a simple interface into GetOSVersion().  More
specific or demanding situations should use that instead.  Another
option would be to use POSIX::uname(), however the latter appears to
report only the OS family name and not the specific OS.  In scalar
context it returns just the ID.

The name "WinXP/.Net" is used for historical reasons only, to maintain
backwards compatibility of the Win32 module.  Windows .NET Server has
been renamed as Windows 2003 Server before final release and uses a
different major/minor version number than Windows XP.

=item Win32::GetShortPathName(PATHNAME)

[CORE] Returns a representation of PATHNAME that is composed of short
(8.3) path components where available.  For path components where the
file system has not generated the short form the returned path will
use the long form, so this function might still for instance return a
path containing spaces.  Returns C<undef> when the PATHNAME does not
exist. Compare with Win32::GetFullPathName() and
Win32::GetLongPathName().

=item Win32::GetProcAddress(INSTANCE, PROCNAME)

Returns the address of a function inside a loaded library.  The
information about what you can do with this address has been lost in
the mist of time.  Use the Win32::API module instead of this deprecated
function.

=item Win32::GetTickCount()

[CORE] Returns the number of milliseconds elapsed since the last
system boot.  Resolution is limited to system timer ticks (about 10ms
on WinNT and 55ms on Win9X).

=item Win32::GuidGen()

Creates a globally unique 128 bit integer that can be used as a
persistent identifier in a distributed setting. To a very high degree
of certainty this function returns a unique value. No other
invocation, on the same or any other system (networked or not), should
return the same value.

The return value is formatted according to OLE conventions, as groups
of hex digits with surrounding braces.  For example:

    {09531CF1-D0C7-4860-840C-1C8C8735E2AD}
 
=item Win32::InitiateSystemShutdown

(MACHINE, MESSAGE, TIMEOUT, FORCECLOSE, REBOOT)

Shutsdown the specified MACHINE, notifying users with the
supplied MESSAGE, within the specified TIMEOUT interval.  Forces
closing of all documents without prompting the user if FORCECLOSE is
true, and reboots the machine if REBOOT is true.  This function works
only on WinNT.

=item Win32::IsAdminUser()

Returns non zero if the account in whose security context the
current process/thread is running belongs to the local group of
Administrators in the built-in system domain; returns 0 if not.
On Windows Vista it will only return non-zero if the process is
actually running with elevated privileges.  Returns C<undef>
and prints a warning if an error occurred.  This function always
returns 1 on Win9X.

=item Win32::IsWinNT()

[CORE] Returns non zero if the Win32 subsystem is Windows NT.

=item Win32::IsWin95()

[CORE] Returns non zero if the Win32 subsystem is Windows 95.

=item Win32::LoadLibrary(LIBNAME)

Loads a dynamic link library into memory and returns its module
handle.  This handle can be used with Win32::GetProcAddress() and
Win32::FreeLibrary().  This function is deprecated.  Use the Win32::API
module instead.

=item Win32::LoginName()

[CORE] Returns the username of the owner of the current perl process.
The return value may be a Unicode string.

=item Win32::LookupAccountName(SYSTEM, ACCOUNT, DOMAIN, SID, SIDTYPE)

Looks up ACCOUNT on SYSTEM and returns the domain name the SID and
the SID type.

=item Win32::LookupAccountSID(SYSTEM, SID, ACCOUNT, DOMAIN, SIDTYPE)

Looks up SID on SYSTEM and returns the account name, domain name,
and the SID type.

=item Win32::MsgBox(MESSAGE [, FLAGS [, TITLE]])

Create a dialogbox containing MESSAGE.  FLAGS specifies the
required icon and buttons according to the following table:

	0 = OK
	1 = OK and Cancel
	2 = Abort, Retry, and Ignore
	3 = Yes, No and Cancel
	4 = Yes and No
	5 = Retry and Cancel

	MB_ICONSTOP          "X" in a red circle
	MB_ICONQUESTION      question mark in a bubble
	MB_ICONEXCLAMATION   exclamation mark in a yellow triangle
	MB_ICONINFORMATION   "i" in a bubble

TITLE specifies an optional window title.  The default is "Perl".

The function returns the menu id of the selected push button:

	0  Error

	1  OK
	2  Cancel
	3  Abort
	4  Retry
	5  Ignore
	6  Yes
	7  No

=item Win32::NodeName()

[CORE] Returns the Microsoft Network node-name of the current machine.

=item Win32::OutputDebugString(STRING)

Sends a string to the application or system debugger for display.
The function does nothing if there is no active debugger.

Alternatively one can use the I<Debug Viewer> application to
watch the OutputDebugString() output:

http://www.microsoft.com/technet/sysinternals/utilities/debugview.mspx

=item Win32::RegisterServer(LIBRARYNAME)

Loads the DLL LIBRARYNAME and calls the function DllRegisterServer.

=item Win32::SetChildShowWindow(SHOWWINDOW)

[CORE] Sets the I<ShowMode> of child processes started by system().
By default system() will create a new console window for child
processes if Perl itself is not running from a console.  Calling
SetChildShowWindow(0) will make these new console windows invisible.
Calling SetChildShowWindow() without arguments reverts system() to the
default behavior.  The return value of SetChildShowWindow() is the
previous setting or C<undef>.

The following symbolic constants for SHOWWINDOW are available
(but not exported) from the Win32 module: SW_HIDE, SW_SHOWNORMAL,
SW_SHOWMINIMIZED, SW_SHOWMAXIMIZED and SW_SHOWNOACTIVATE.

=item Win32::SetCwd(NEWDIRECTORY)

[CORE] Sets the current active drive and directory.  This function does not
work with UNC paths, since the functionality required to required for
such a feature is not available under Windows 95.

=item Win32::SetLastError(ERROR)

[CORE] Sets the value of the last error encountered to ERROR.  This is
that value that will be returned by the Win32::GetLastError()
function.

=item Win32::Sleep(TIME)

[CORE] Pauses for TIME milliseconds.  The timeslices are made available
to other processes and threads.

=item Win32::Spawn(COMMAND, ARGS, PID)

[CORE] Spawns a new process using the supplied COMMAND, passing in
arguments in the string ARGS.  The pid of the new process is stored in
PID.  This function is deprecated.  Please use the Win32::Process module
instead.

=item Win32::UnregisterServer(LIBRARYNAME)

Loads the DLL LIBRARYNAME and calls the function
DllUnregisterServer.

=back

=cut
