package Cwd;

=head1 NAME

Cwd - get pathname of current working directory

=head1 SYNOPSIS

    use Cwd;
    my $dir = getcwd;

    use Cwd 'abs_path';
    my $abs_path = abs_path($file);

=head1 DESCRIPTION

This module provides functions for determining the pathname of the
current working directory.  It is recommended that getcwd (or another
*cwd() function) be used in I<all> code to ensure portability.

By default, it exports the functions cwd(), getcwd(), fastcwd(), and
fastgetcwd() (and, on Win32, getdcwd()) into the caller's namespace.  


=head2 getcwd and friends

Each of these functions are called without arguments and return the
absolute path of the current working directory.

=over 4

=item getcwd

    my $cwd = getcwd();

Returns the current working directory.

Exposes the POSIX function getcwd(3) or re-implements it if it's not
available.

=item cwd

    my $cwd = cwd();

The cwd() is the most natural form for the current architecture. For
most systems it is identical to `pwd` (but without the trailing line
terminator).

=item fastcwd

    my $cwd = fastcwd();

A more dangerous version of getcwd(), but potentially faster.

It might conceivably chdir() you out of a directory that it can't
chdir() you back into.  If fastcwd encounters a problem it will return
undef but will probably leave you in a different directory.  For a
measure of extra security, if everything appears to have worked, the
fastcwd() function will check that it leaves you in the same directory
that it started in. If it has changed it will C<die> with the message
"Unstable directory path, current directory changed
unexpectedly". That should never happen.

=item fastgetcwd

  my $cwd = fastgetcwd();

The fastgetcwd() function is provided as a synonym for cwd().

=item getdcwd

    my $cwd = getdcwd();
    my $cwd = getdcwd('C:');

The getdcwd() function is also provided on Win32 to get the current working
directory on the specified drive, since Windows maintains a separate current
working directory for each drive.  If no drive is specified then the current
drive is assumed.

This function simply calls the Microsoft C library _getdcwd() function.

=back


=head2 abs_path and friends

These functions are exported only on request.  They each take a single
argument and return the absolute pathname for it.  If no argument is
given they'll use the current working directory.

=over 4

=item abs_path

  my $abs_path = abs_path($file);

Uses the same algorithm as getcwd().  Symbolic links and relative-path
components ("." and "..") are resolved to return the canonical
pathname, just like realpath(3).

=item realpath

  my $abs_path = realpath($file);

A synonym for abs_path().

=item fast_abs_path

  my $abs_path = fast_abs_path($file);

A more dangerous, but potentially faster version of abs_path.

=back

=head2 $ENV{PWD}

If you ask to override your chdir() built-in function, 

  use Cwd qw(chdir);

then your PWD environment variable will be kept up to date.  Note that
it will only be kept up to date if all packages which use chdir import
it from Cwd.


=head1 NOTES

=over 4

=item *

Since the path seperators are different on some operating systems ('/'
on Unix, ':' on MacPerl, etc...) we recommend you use the File::Spec
modules wherever portability is a concern.

=item *

Actually, on Mac OS, the C<getcwd()>, C<fastgetcwd()> and C<fastcwd()>
functions  are all aliases for the C<cwd()> function, which, on Mac OS,
calls `pwd`. Likewise, the C<abs_path()> function is an alias for
C<fast_abs_path()>.

=back

=head1 AUTHOR

Originally by the perl5-porters.

Maintained by Ken Williams <KWILLIAMS@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2004 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Portions of the C code in this library are copyright (c) 1994 by the
Regents of the University of California.  All rights reserved.  The
license on this code is compatible with the licensing of the rest of
the distribution - please see the source code in F<Cwd.xs> for the
details.

=head1 SEE ALSO

L<File::chdir>

=cut

use strict;
use Exporter;
use vars qw(@ISA @EXPORT @EXPORT_OK $VERSION);

$VERSION = '3.2701';

@ISA = qw/ Exporter /;
@EXPORT = qw(cwd getcwd fastcwd fastgetcwd);
push @EXPORT, qw(getdcwd) if $^O eq 'MSWin32';
@EXPORT_OK = qw(chdir abs_path fast_abs_path realpath fast_realpath);

# sys_cwd may keep the builtin command

# All the functionality of this module may provided by builtins,
# there is no sense to process the rest of the file.
# The best choice may be to have this in BEGIN, but how to return from BEGIN?

if ($^O eq 'os2') {
    local $^W = 0;

    *cwd                = defined &sys_cwd ? \&sys_cwd : \&_os2_cwd;
    *getcwd             = \&cwd;
    *fastgetcwd         = \&cwd;
    *fastcwd            = \&cwd;

    *fast_abs_path      = \&sys_abspath if defined &sys_abspath;
    *abs_path           = \&fast_abs_path;
    *realpath           = \&fast_abs_path;
    *fast_realpath      = \&fast_abs_path;

    return 1;
}

# If loading the XS stuff doesn't work, we can fall back to pure perl
eval {
  if ( $] >= 5.006 ) {
    require XSLoader;
    XSLoader::load( __PACKAGE__, $VERSION );
  } else {
    require DynaLoader;
    push @ISA, 'DynaLoader';
    __PACKAGE__->bootstrap( $VERSION );
  }
};

# Must be after the DynaLoader stuff:
$VERSION = eval $VERSION;

# Big nasty table of function aliases
my %METHOD_MAP =
  (
   VMS =>
   {
    cwd			=> '_vms_cwd',
    getcwd		=> '_vms_cwd',
    fastcwd		=> '_vms_cwd',
    fastgetcwd		=> '_vms_cwd',
    abs_path		=> '_vms_abs_path',
    fast_abs_path	=> '_vms_abs_path',
   },

   MSWin32 =>
   {
    # We assume that &_NT_cwd is defined as an XSUB or in the core.
    cwd			=> '_NT_cwd',
    getcwd		=> '_NT_cwd',
    fastcwd		=> '_NT_cwd',
    fastgetcwd		=> '_NT_cwd',
    abs_path		=> 'fast_abs_path',
    realpath		=> 'fast_abs_path',
   },

   dos => 
   {
    cwd			=> '_dos_cwd',
    getcwd		=> '_dos_cwd',
    fastgetcwd		=> '_dos_cwd',
    fastcwd		=> '_dos_cwd',
    abs_path		=> 'fast_abs_path',
   },

   qnx =>
   {
    cwd			=> '_qnx_cwd',
    getcwd		=> '_qnx_cwd',
    fastgetcwd		=> '_qnx_cwd',
    fastcwd		=> '_qnx_cwd',
    abs_path		=> '_qnx_abs_path',
    fast_abs_path	=> '_qnx_abs_path',
   },

   cygwin =>
   {
    getcwd		=> 'cwd',
    fastgetcwd		=> 'cwd',
    fastcwd		=> 'cwd',
    abs_path		=> 'fast_abs_path',
    realpath		=> 'fast_abs_path',
   },

   epoc =>
   {
    cwd			=> '_epoc_cwd',
    getcwd	        => '_epoc_cwd',
    fastgetcwd		=> '_epoc_cwd',
    fastcwd		=> '_epoc_cwd',
    abs_path		=> 'fast_abs_path',
   },

   MacOS =>
   {
    getcwd		=> 'cwd',
    fastgetcwd		=> 'cwd',
    fastcwd		=> 'cwd',
    abs_path		=> 'fast_abs_path',
   },
  );

$METHOD_MAP{NT} = $METHOD_MAP{MSWin32};
$METHOD_MAP{nto} = $METHOD_MAP{qnx};


# Find the pwd command in the expected locations.  We assume these
# are safe.  This prevents _backtick_pwd() consulting $ENV{PATH}
# so everything works under taint mode.
my $pwd_cmd;
foreach my $try ('/bin/pwd',
		 '/usr/bin/pwd',
		 '/QOpenSys/bin/pwd', # OS/400 PASE.
		) {

    if( -x $try ) {
        $pwd_cmd = $try;
        last;
    }
}
my $found_pwd_cmd = defined($pwd_cmd);
unless ($pwd_cmd) {
    # Isn't this wrong?  _backtick_pwd() will fail if somenone has
    # pwd in their path but it is not /bin/pwd or /usr/bin/pwd?
    # See [perl #16774]. --jhi
    $pwd_cmd = 'pwd';
}

# Lazy-load Carp
sub _carp  { require Carp; Carp::carp(@_)  }
sub _croak { require Carp; Carp::croak(@_) }

# The 'natural and safe form' for UNIX (pwd may be setuid root)
sub _backtick_pwd {
    # Localize %ENV entries in a way that won't create new hash keys
    my @localize = grep exists $ENV{$_}, qw(PATH IFS CDPATH ENV BASH_ENV);
    local @ENV{@localize};
    
    my $cwd = `$pwd_cmd`;
    # Belt-and-suspenders in case someone said "undef $/".
    local $/ = "\n";
    # `pwd` may fail e.g. if the disk is full
    chomp($cwd) if defined $cwd;
    $cwd;
}

# Since some ports may predefine cwd internally (e.g., NT)
# we take care not to override an existing definition for cwd().

unless ($METHOD_MAP{$^O}{cwd} or defined &cwd) {
    # The pwd command is not available in some chroot(2)'ed environments
    my $sep = $Config::Config{path_sep} || ':';
    my $os = $^O;  # Protect $^O from tainting


    # Try again to find a pwd, this time searching the whole PATH.
    if (defined $ENV{PATH} and $os ne 'MSWin32') {  # no pwd on Windows
	my @candidates = split($sep, $ENV{PATH});
	while (!$found_pwd_cmd and @candidates) {
	    my $candidate = shift @candidates;
	    $found_pwd_cmd = 1 if -x "$candidate/pwd";
	}
    }

    # MacOS has some special magic to make `pwd` work.
    if( $os eq 'MacOS' || $found_pwd_cmd )
    {
	*cwd = \&_backtick_pwd;
    }
    else {
	*cwd = \&getcwd;
    }
}

if ($^O eq 'cygwin') {
  # We need to make sure cwd() is called with no args, because it's
  # got an arg-less prototype and will die if args are present.
  local $^W = 0;
  my $orig_cwd = \&cwd;
  *cwd = sub { &$orig_cwd() }
}


# set a reasonable (and very safe) default for fastgetcwd, in case it
# isn't redefined later (20001212 rspier)
*fastgetcwd = \&cwd;

# A non-XS version of getcwd() - also used to bootstrap the perl build
# process, when miniperl is running and no XS loading happens.
sub _perl_getcwd
{
    abs_path('.');
}

# By John Bazik
#
# Usage: $cwd = &fastcwd;
#
# This is a faster version of getcwd.  It's also more dangerous because
# you might chdir out of a directory that you can't chdir back into.
    
sub fastcwd_ {
    my($odev, $oino, $cdev, $cino, $tdev, $tino);
    my(@path, $path);
    local(*DIR);

    my($orig_cdev, $orig_cino) = stat('.');
    ($cdev, $cino) = ($orig_cdev, $orig_cino);
    for (;;) {
	my $direntry;
	($odev, $oino) = ($cdev, $cino);
	CORE::chdir('..') || return undef;
	($cdev, $cino) = stat('.');
	last if $odev == $cdev && $oino == $cino;
	opendir(DIR, '.') || return undef;
	for (;;) {
	    $direntry = readdir(DIR);
	    last unless defined $direntry;
	    next if $direntry eq '.';
	    next if $direntry eq '..';

	    ($tdev, $tino) = lstat($direntry);
	    last unless $tdev != $odev || $tino != $oino;
	}
	closedir(DIR);
	return undef unless defined $direntry; # should never happen
	unshift(@path, $direntry);
    }
    $path = '/' . join('/', @path);
    if ($^O eq 'apollo') { $path = "/".$path; }
    # At this point $path may be tainted (if tainting) and chdir would fail.
    # Untaint it then check that we landed where we started.
    $path =~ /^(.*)\z/s		# untaint
	&& CORE::chdir($1) or return undef;
    ($cdev, $cino) = stat('.');
    die "Unstable directory path, current directory changed unexpectedly"
	if $cdev != $orig_cdev || $cino != $orig_cino;
    $path;
}
if (not defined &fastcwd) { *fastcwd = \&fastcwd_ }


# Keeps track of current working directory in PWD environment var
# Usage:
#	use Cwd 'chdir';
#	chdir $newdir;

my $chdir_init = 0;

sub chdir_init {
    if ($ENV{'PWD'} and $^O ne 'os2' and $^O ne 'dos' and $^O ne 'MSWin32') {
	my($dd,$di) = stat('.');
	my($pd,$pi) = stat($ENV{'PWD'});
	if (!defined $dd or !defined $pd or $di != $pi or $dd != $pd) {
	    $ENV{'PWD'} = cwd();
	}
    }
    else {
	my $wd = cwd();
	$wd = Win32::GetFullPathName($wd) if $^O eq 'MSWin32';
	$ENV{'PWD'} = $wd;
    }
    # Strip an automounter prefix (where /tmp_mnt/foo/bar == /foo/bar)
    if ($^O ne 'MSWin32' and $ENV{'PWD'} =~ m|(/[^/]+(/[^/]+/[^/]+))(.*)|s) {
	my($pd,$pi) = stat($2);
	my($dd,$di) = stat($1);
	if (defined $pd and defined $dd and $di == $pi and $dd == $pd) {
	    $ENV{'PWD'}="$2$3";
	}
    }
    $chdir_init = 1;
}

sub chdir {
    my $newdir = @_ ? shift : '';	# allow for no arg (chdir to HOME dir)
    $newdir =~ s|///*|/|g unless $^O eq 'MSWin32';
    chdir_init() unless $chdir_init;
    my $newpwd;
    if ($^O eq 'MSWin32') {
	# get the full path name *before* the chdir()
	$newpwd = Win32::GetFullPathName($newdir);
    }

    return 0 unless CORE::chdir $newdir;

    if ($^O eq 'VMS') {
	return $ENV{'PWD'} = $ENV{'DEFAULT'}
    }
    elsif ($^O eq 'MacOS') {
	return $ENV{'PWD'} = cwd();
    }
    elsif ($^O eq 'MSWin32') {
	$ENV{'PWD'} = $newpwd;
	return 1;
    }

    if (ref $newdir eq 'GLOB') { # in case a file/dir handle is passed in
	$ENV{'PWD'} = cwd();
    } elsif ($newdir =~ m#^/#s) {
	$ENV{'PWD'} = $newdir;
    } else {
	my @curdir = split(m#/#,$ENV{'PWD'});
	@curdir = ('') unless @curdir;
	my $component;
	foreach $component (split(m#/#, $newdir)) {
	    next if $component eq '.';
	    pop(@curdir),next if $component eq '..';
	    push(@curdir,$component);
	}
	$ENV{'PWD'} = join('/',@curdir) || '/';
    }
    1;
}


sub _perl_abs_path
{
    my $start = @_ ? shift : '.';
    my($dotdots, $cwd, @pst, @cst, $dir, @tst);

    unless (@cst = stat( $start ))
    {
	_carp("stat($start): $!");
	return '';
    }

    unless (-d _) {
        # Make sure we can be invoked on plain files, not just directories.
        # NOTE that this routine assumes that '/' is the only directory separator.
	
        my ($dir, $file) = $start =~ m{^(.*)/(.+)$}
	    or return cwd() . '/' . $start;
	
	# Can't use "-l _" here, because the previous stat was a stat(), not an lstat().
	if (-l $start) {
	    my $link_target = readlink($start);
	    die "Can't resolve link $start: $!" unless defined $link_target;
	    
	    require File::Spec;
            $link_target = $dir . '/' . $link_target
                unless File::Spec->file_name_is_absolute($link_target);
	    
	    return abs_path($link_target);
	}
	
	return $dir ? abs_path($dir) . "/$file" : "/$file";
    }

    $cwd = '';
    $dotdots = $start;
    do
    {
	$dotdots .= '/..';
	@pst = @cst;
	local *PARENT;
	unless (opendir(PARENT, $dotdots))
	{
	    # probably a permissions issue.  Try the native command.
	    return File::Spec->rel2abs( $start, _backtick_pwd() );
	}
	unless (@cst = stat($dotdots))
	{
	    _carp("stat($dotdots): $!");
	    closedir(PARENT);
	    return '';
	}
	if ($pst[0] == $cst[0] && $pst[1] == $cst[1])
	{
	    $dir = undef;
	}
	else
	{
	    do
	    {
		unless (defined ($dir = readdir(PARENT)))
	        {
		    _carp("readdir($dotdots): $!");
		    closedir(PARENT);
		    return '';
		}
		$tst[0] = $pst[0]+1 unless (@tst = lstat("$dotdots/$dir"))
	    }
	    while ($dir eq '.' || $dir eq '..' || $tst[0] != $pst[0] ||
		   $tst[1] != $pst[1]);
	}
	$cwd = (defined $dir ? "$dir" : "" ) . "/$cwd" ;
	closedir(PARENT);
    } while (defined $dir);
    chop($cwd) unless $cwd eq '/'; # drop the trailing /
    $cwd;
}


my $Curdir;
sub fast_abs_path {
    local $ENV{PWD} = $ENV{PWD} || ''; # Guard against clobberage
    my $cwd = getcwd();
    require File::Spec;
    my $path = @_ ? shift : ($Curdir ||= File::Spec->curdir);

    # Detaint else we'll explode in taint mode.  This is safe because
    # we're not doing anything dangerous with it.
    ($path) = $path =~ /(.*)/;
    ($cwd)  = $cwd  =~ /(.*)/;

    unless (-e $path) {
 	_croak("$path: No such file or directory");
    }

    unless (-d _) {
        # Make sure we can be invoked on plain files, not just directories.
	
	my ($vol, $dir, $file) = File::Spec->splitpath($path);
	return File::Spec->catfile($cwd, $path) unless length $dir;

	if (-l $path) {
	    my $link_target = readlink($path);
	    die "Can't resolve link $path: $!" unless defined $link_target;
	    
	    $link_target = File::Spec->catpath($vol, $dir, $link_target)
                unless File::Spec->file_name_is_absolute($link_target);
	    
	    return fast_abs_path($link_target);
	}
	
	return $dir eq File::Spec->rootdir
	  ? File::Spec->catpath($vol, $dir, $file)
	  : fast_abs_path(File::Spec->catpath($vol, $dir, '')) . '/' . $file;
    }

    if (!CORE::chdir($path)) {
 	_croak("Cannot chdir to $path: $!");
    }
    my $realpath = getcwd();
    if (! ((-d $cwd) && (CORE::chdir($cwd)))) {
 	_croak("Cannot chdir back to $cwd: $!");
    }
    $realpath;
}

# added function alias to follow principle of least surprise
# based on previous aliasing.  --tchrist 27-Jan-00
*fast_realpath = \&fast_abs_path;


# --- PORTING SECTION ---

# VMS: $ENV{'DEFAULT'} points to default directory at all times
# 06-Mar-1996  Charles Bailey  bailey@newman.upenn.edu
# Note: Use of Cwd::chdir() causes the logical name PWD to be defined
#   in the process logical name table as the default device and directory
#   seen by Perl. This may not be the same as the default device
#   and directory seen by DCL after Perl exits, since the effects
#   the CRTL chdir() function persist only until Perl exits.

sub _vms_cwd {
    return $ENV{'DEFAULT'};
}

sub _vms_abs_path {
    return $ENV{'DEFAULT'} unless @_;
    my $path = shift;

    if (-l $path) {
        my $link_target = readlink($path);
        die "Can't resolve link $path: $!" unless defined $link_target;
	    
        return _vms_abs_path($link_target);
    }

    if (defined &VMS::Filespec::vms_realpath) {
        my $path = $_[0];
        if ($path =~ m#(?<=\^)/# ) {
            # Unix format
            return VMS::Filespec::vms_realpath($path);
        }

	# VMS format

	my $new_path = VMS::Filespec::vms_realname($path); 

	# Perl expects directories to be in directory format
	$new_path = VMS::Filespec::pathify($new_path) if -d $path;
	return $new_path;
    }

    # Fallback to older algorithm if correct ones are not
    # available.

    # may need to turn foo.dir into [.foo]
    my $pathified = VMS::Filespec::pathify($path);
    $path = $pathified if defined $pathified;
	
    return VMS::Filespec::rmsexpand($path);
}

sub _os2_cwd {
    $ENV{'PWD'} = `cmd /c cd`;
    chomp $ENV{'PWD'};
    $ENV{'PWD'} =~ s:\\:/:g ;
    return $ENV{'PWD'};
}

sub _win32_cwd {
    if (defined &DynaLoader::boot_DynaLoader) {
	$ENV{'PWD'} = Win32::GetCwd();
    }
    else { # miniperl
	chomp($ENV{'PWD'} = `cd`);
    }
    $ENV{'PWD'} =~ s:\\:/:g ;
    return $ENV{'PWD'};
}

*_NT_cwd = defined &Win32::GetCwd ? \&_win32_cwd : \&_os2_cwd;

sub _dos_cwd {
    if (!defined &Dos::GetCwd) {
        $ENV{'PWD'} = `command /c cd`;
        chomp $ENV{'PWD'};
        $ENV{'PWD'} =~ s:\\:/:g ;
    } else {
        $ENV{'PWD'} = Dos::GetCwd();
    }
    return $ENV{'PWD'};
}

sub _qnx_cwd {
	local $ENV{PATH} = '';
	local $ENV{CDPATH} = '';
	local $ENV{ENV} = '';
    $ENV{'PWD'} = `/usr/bin/fullpath -t`;
    chomp $ENV{'PWD'};
    return $ENV{'PWD'};
}

sub _qnx_abs_path {
	local $ENV{PATH} = '';
	local $ENV{CDPATH} = '';
	local $ENV{ENV} = '';
    my $path = @_ ? shift : '.';
    local *REALPATH;

    defined( open(REALPATH, '-|') || exec '/usr/bin/fullpath', '-t', $path ) or
      die "Can't open /usr/bin/fullpath: $!";
    my $realpath = <REALPATH>;
    close REALPATH;
    chomp $realpath;
    return $realpath;
}

sub _epoc_cwd {
    $ENV{'PWD'} = EPOC::getcwd();
    return $ENV{'PWD'};
}


# Now that all the base-level functions are set up, alias the
# user-level functions to the right places

if (exists $METHOD_MAP{$^O}) {
  my $map = $METHOD_MAP{$^O};
  foreach my $name (keys %$map) {
    local $^W = 0;  # assignments trigger 'subroutine redefined' warning
    no strict 'refs';
    *{$name} = \&{$map->{$name}};
  }
}

# In case the XS version doesn't load.
*abs_path = \&_perl_abs_path unless defined &abs_path;
*getcwd = \&_perl_getcwd unless defined &getcwd;

# added function alias for those of us more
# used to the libc function.  --tchrist 27-Jan-00
*realpath = \&abs_path;

1;
