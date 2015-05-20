package File::Path;

use 5.005_04;
use strict;

use Cwd 'getcwd';
use File::Basename ();
use File::Spec     ();

BEGIN {
    if ($] < 5.006) {
        # can't say 'opendir my $dh, $dirname'
        # need to initialise $dh
        eval "use Symbol";
    }
}

use Exporter ();
use vars qw($VERSION @ISA @EXPORT);
$VERSION = '2.04';
@ISA     = qw(Exporter);
@EXPORT  = qw(mkpath rmtree);

my $Is_VMS   = $^O eq 'VMS';
my $Is_MacOS = $^O eq 'MacOS';

# These OSes complain if you want to remove a file that you have no
# write permission to:
my $Force_Writeable = grep {$^O eq $_} qw(amigaos dos epoc MSWin32 MacOS os2);

sub _carp {
    require Carp;
    goto &Carp::carp;
}

sub _croak {
    require Carp;
    goto &Carp::croak;
}

sub _error {
    my $arg     = shift;
    my $message = shift;
    my $object  = shift;

    if ($arg->{error}) {
        $object = '' unless defined $object;
        push @{${$arg->{error}}}, {$object => "$message: $!"};
    }
    else {
        _carp(defined($object) ? "$message for $object: $!" : "$message: $!");
    }
}

sub mkpath {
    my $old_style = (
        UNIVERSAL::isa($_[0],'ARRAY')
        or (@_ == 2 and (defined $_[1] ? $_[1] =~ /\A\d+\z/ : 1))
        or (@_ == 3
            and (defined $_[1] ? $_[1] =~ /\A\d+\z/ : 1)
            and (defined $_[2] ? $_[2] =~ /\A\d+\z/ : 1)
        )
    ) ? 1 : 0;

    my $arg;
    my $paths;

    if ($old_style) {
        my ($verbose, $mode);
        ($paths, $verbose, $mode) = @_;
        $paths = [$paths] unless UNIVERSAL::isa($paths,'ARRAY');
        $arg->{verbose} = defined $verbose ? $verbose : 0;
        $arg->{mode}    = defined $mode    ? $mode    : 0777;
    }
    else {
        if (@_ > 0 and UNIVERSAL::isa($_[-1], 'HASH')) {
            $arg = pop @_;
            exists $arg->{mask} and $arg->{mode} = delete $arg->{mask};
            $arg->{mode} = 0777 unless exists $arg->{mode};
            ${$arg->{error}} = [] if exists $arg->{error};
        }
        else {
            @{$arg}{qw(verbose mode)} = (0, 0777);
        }
        $paths = [@_];
    }
    return _mkpath($arg, $paths);
}

sub _mkpath {
    my $arg   = shift;
    my $paths = shift;

    local($")=$Is_MacOS ? ":" : "/";
    my(@created,$path);
    foreach $path (@$paths) {
        next unless length($path);
        $path .= '/' if $^O eq 'os2' and $path =~ /^\w:\z/s; # feature of CRT 
        # Logic wants Unix paths, so go with the flow.
        if ($Is_VMS) {
            next if $path eq '/';
            $path = VMS::Filespec::unixify($path);
        }
        next if -d $path;
        my $parent = File::Basename::dirname($path);
        unless (-d $parent or $path eq $parent) {
            push(@created,_mkpath($arg, [$parent]));
        }
        print "mkdir $path\n" if $arg->{verbose};
        if (mkdir($path,$arg->{mode})) {
            push(@created, $path);
        }
        else {
            my $save_bang = $!;
            my ($e, $e1) = ($save_bang, $^E);
            $e .= "; $e1" if $e ne $e1;
            # allow for another process to have created it meanwhile
            if (!-d $path) {
                $! = $save_bang;
                if ($arg->{error}) {
                    push @{${$arg->{error}}}, {$path => $e};
                }
                else {
                    _croak("mkdir $path: $e");
                }
            }
        }
    }
    return @created;
}

sub rmtree {
    my $old_style = (
        UNIVERSAL::isa($_[0],'ARRAY')
        or (@_ == 2 and (defined $_[1] ? $_[1] =~ /\A\d+\z/ : 1))
        or (@_ == 3
            and (defined $_[1] ? $_[1] =~ /\A\d+\z/ : 1)
            and (defined $_[2] ? $_[2] =~ /\A\d+\z/ : 1)
        )
    ) ? 1 : 0;

    my $arg;
    my $paths;

    if ($old_style) {
        my ($verbose, $safe);
        ($paths, $verbose, $safe) = @_;
        $arg->{verbose} = defined $verbose ? $verbose : 0;
        $arg->{safe}    = defined $safe    ? $safe    : 0;

        if (defined($paths) and length($paths)) {
            $paths = [$paths] unless UNIVERSAL::isa($paths,'ARRAY');
        }
        else {
            _carp ("No root path(s) specified\n");
            return 0;
        }
    }
    else {
        if (@_ > 0 and UNIVERSAL::isa($_[-1],'HASH')) {
            $arg = pop @_;
            ${$arg->{error}}  = [] if exists $arg->{error};
            ${$arg->{result}} = [] if exists $arg->{result};
        }
        else {
            @{$arg}{qw(verbose safe)} = (0, 0);
        }
        $paths = [@_];
    }

    $arg->{prefix} = '';
    $arg->{depth}  = 0;

    $arg->{cwd} = getcwd() or do {
        _error($arg, "cannot fetch initial working directory");
        return 0;
    };
    for ($arg->{cwd}) { /\A(.*)\Z/; $_ = $1 } # untaint

    @{$arg}{qw(device inode)} = (stat $arg->{cwd})[0,1] or do {
        _error($arg, "cannot stat initial working directory", $arg->{cwd});
        return 0;
    };

    return _rmtree($arg, $paths);
}

sub _rmtree {
    my $arg   = shift;
    my $paths = shift;

    my $count  = 0;
    my $curdir = File::Spec->curdir();
    my $updir  = File::Spec->updir();

    my (@files, $root);
    ROOT_DIR:
    foreach $root (@$paths) {
        if ($Is_MacOS) {
            $root  = ":$root" unless $root =~ /:/;
            $root .= ":"      unless $root =~ /:\z/;
        }
        else {
            $root =~ s{/\z}{};
        }

        # since we chdir into each directory, it may not be obvious
        # to figure out where we are if we generate a message about
        # a file name. We therefore construct a semi-canonical
        # filename, anchored from the directory being unlinked (as
        # opposed to being truly canonical, anchored from the root (/).

        my $canon = $arg->{prefix}
            ? File::Spec->catfile($arg->{prefix}, $root)
            : $root
        ;

        my ($ldev, $lino, $perm) = (lstat $root)[0,1,2] or next ROOT_DIR;

        if ( -d _ ) {
            $root = VMS::Filespec::pathify($root) if $Is_VMS;
            if (!chdir($root)) {
                # see if we can escalate privileges to get in
                # (e.g. funny protection mask such as -w- instead of rwx)
                $perm &= 07777;
                my $nperm = $perm | 0700;
                if (!($arg->{safe} or $nperm == $perm or chmod($nperm, $root))) {
                    _error($arg, "cannot make child directory read-write-exec", $canon);
                    next ROOT_DIR;
                }
                elsif (!chdir($root)) {
                    _error($arg, "cannot chdir to child", $canon);
                    next ROOT_DIR;
                }
            }

            my ($device, $inode, $perm) = (stat $curdir)[0,1,2] or do {
                _error($arg, "cannot stat current working directory", $canon);
                next ROOT_DIR;
            };

            ($ldev eq $device and $lino eq $inode)
                or _croak("directory $canon changed before chdir, expected dev=$ldev inode=$lino, actual dev=$device ino=$inode, aborting.");

            $perm &= 07777; # don't forget setuid, setgid, sticky bits
            my $nperm = $perm | 0700;

            # notabene: 0700 is for making readable in the first place,
            # it's also intended to change it to writable in case we have
            # to recurse in which case we are better than rm -rf for 
            # subtrees with strange permissions

            if (!($arg->{safe} or $nperm == $perm or chmod($nperm, $curdir))) {
                _error($arg, "cannot make directory read+writeable", $canon);
                $nperm = $perm;
            }

            my $d;
            $d = gensym() if $] < 5.006;
            if (!opendir $d, $curdir) {
                _error($arg, "cannot opendir", $canon);
                @files = ();
            }
            else {
                no strict 'refs';
                if (!defined ${"\cTAINT"} or ${"\cTAINT"}) {
                    # Blindly untaint dir names if taint mode is
                    # active, or any perl < 5.006
                    @files = map { /\A(.*)\z/s; $1 } readdir $d;
                }
                else {
                    @files = readdir $d;
                }
                closedir $d;
            }

            if ($Is_VMS) {
                # Deleting large numbers of files from VMS Files-11
                # filesystems is faster if done in reverse ASCIIbetical order.
                # include '.' to '.;' from blead patch #31775
                @files = map {$_ eq '.' ? '.;' : $_} reverse @files;
                ($root = VMS::Filespec::unixify($root)) =~ s/\.dir\z//;
            }
            @files = grep {$_ ne $updir and $_ ne $curdir} @files;

            if (@files) {
                # remove the contained files before the directory itself
                my $narg = {%$arg};
                @{$narg}{qw(device inode cwd prefix depth)}
                    = ($device, $inode, $updir, $canon, $arg->{depth}+1);
                $count += _rmtree($narg, \@files);
            }

            # restore directory permissions of required now (in case the rmdir
            # below fails), while we are still in the directory and may do so
            # without a race via '.'
            if ($nperm != $perm and not chmod($perm, $curdir)) {
                _error($arg, "cannot reset chmod", $canon);
            }

            # don't leave the client code in an unexpected directory
            chdir($arg->{cwd})
                or _croak("cannot chdir to $arg->{cwd} from $canon: $!, aborting.");

            # ensure that a chdir upwards didn't take us somewhere other
            # than we expected (see CVE-2002-0435)
            ($device, $inode) = (stat $curdir)[0,1]
                or _croak("cannot stat prior working directory $arg->{cwd}: $!, aborting.");

            ($arg->{device} eq $device and $arg->{inode} eq $inode)
                or _croak("previous directory $arg->{cwd} changed before entering $canon, expected dev=$ldev inode=$lino, actual dev=$device ino=$inode, aborting.");

            if ($arg->{depth} or !$arg->{keep_root}) {
                if ($arg->{safe} &&
                    ($Is_VMS ? !&VMS::Filespec::candelete($root) : !-w $root)) {
                    print "skipped $root\n" if $arg->{verbose};
                    next ROOT_DIR;
                }
                if (!chmod $perm | 0700, $root) {
                    if ($Force_Writeable) {
                        _error($arg, "cannot make directory writeable", $canon);
                    }
                }
                print "rmdir $root\n" if $arg->{verbose};
                if (rmdir $root) {
                    push @{${$arg->{result}}}, $root if $arg->{result};
                    ++$count;
                }
                else {
                    _error($arg, "cannot remove directory", $canon);
                    if (!chmod($perm, ($Is_VMS ? VMS::Filespec::fileify($root) : $root))
                    ) {
                        _error($arg, sprintf("cannot restore permissions to 0%o",$perm), $canon);
                    }
                }
            }
        }
        else {
            # not a directory
            $root = VMS::Filespec::vmsify("./$root")
                if $Is_VMS 
                   && !File::Spec->file_name_is_absolute($root)
                   && ($root !~ m/(?<!\^)[\]>]+/);  # not already in VMS syntax

            if ($arg->{safe} &&
                ($Is_VMS ? !&VMS::Filespec::candelete($root)
                         : !(-l $root || -w $root)))
            {
                print "skipped $root\n" if $arg->{verbose};
                next ROOT_DIR;
            }

            my $nperm = $perm & 07777 | 0600;
            if ($nperm != $perm and not chmod $nperm, $root) {
                if ($Force_Writeable) {
                    _error($arg, "cannot make file writeable", $canon);
                }
            }
            print "unlink $canon\n" if $arg->{verbose};
            # delete all versions under VMS
            for (;;) {
                if (unlink $root) {
                    push @{${$arg->{result}}}, $root if $arg->{result};
                }
                else {
                    _error($arg, "cannot unlink file", $canon);
                    $Force_Writeable and chmod($perm, $root) or
                        _error($arg, sprintf("cannot restore permissions to 0%o",$perm), $canon);
                    last;
                }
                ++$count;
                last unless $Is_VMS && lstat $root;
            }
        }
    }

    return $count;
}

1;
__END__

=head1 NAME

File::Path - Create or remove directory trees

=head1 VERSION

This document describes version 2.04 of File::Path, released
2007-11-13.

=head1 SYNOPSIS

    use File::Path;

    # modern
    mkpath( 'foo/bar/baz', '/zug/zwang', {verbose => 1} );

    rmtree(
        'foo/bar/baz', '/zug/zwang',
        { verbose => 1, error  => \my $err_list }
    );

    # traditional
    mkpath(['/foo/bar/baz', 'blurfl/quux'], 1, 0711);
    rmtree(['foo/bar/baz', 'blurfl/quux'], 1, 1);

=head1 DESCRIPTION

The C<mkpath> function provides a convenient way to create directories
of arbitrary depth. Similarly, the C<rmtree> function provides a
convenient way to delete an entire directory subtree from the
filesystem, much like the Unix command C<rm -r>.

Both functions may be called in one of two ways, the traditional,
compatible with code written since the dawn of time, and modern,
that offers a more flexible and readable idiom. New code should use
the modern interface.

=head2 FUNCTIONS

The modern way of calling C<mkpath> and C<rmtree> is with a list
of directories to create, or remove, respectively, followed by an
optional hash reference containing keys to control the
function's behaviour.

=head3 C<mkpath>

The following keys are recognised as parameters to C<mkpath>.
The function returns the list of files actually created during the
call.

  my @created = mkpath(
    qw(/tmp /flub /home/nobody),
    {verbose => 1, mode => 0750},
  );
  print "created $_\n" for @created;

=over 4

=item mode

The numeric permissions mode to apply to each created directory
(defaults to 0777), to be modified by the current C<umask>. If the
directory already exists (and thus does not need to be created),
the permissions will not be modified.

C<mask> is recognised as an alias for this parameter.

=item verbose

If present, will cause C<mkpath> to print the name of each directory
as it is created. By default nothing is printed.

=item error

If present, will be interpreted as a reference to a list, and will
be used to store any errors that are encountered.  See the ERROR
HANDLING section for more information.

If this parameter is not used, certain error conditions may raise
a fatal error that will cause the program will halt, unless trapped
in an C<eval> block.

=back

=head3 C<rmtree>

=over 4

=item verbose

If present, will cause C<rmtree> to print the name of each file as
it is unlinked. By default nothing is printed.

=item safe

When set to a true value, will cause C<rmtree> to skip the files
for which the process lacks the required privileges needed to delete
files, such as delete privileges on VMS. In other words, the code
will make no attempt to alter file permissions. Thus, if the process
is interrupted, no filesystem object will be left in a more
permissive mode.

=item keep_root

When set to a true value, will cause all files and subdirectories
to be removed, except the initially specified directories. This comes
in handy when cleaning out an application's scratch directory.

  rmtree( '/tmp', {keep_root => 1} );

=item result

If present, will be interpreted as a reference to a list, and will
be used to store the list of all files and directories unlinked
during the call. If nothing is unlinked, a reference to an empty
list is returned (rather than C<undef>).

  rmtree( '/tmp', {result => \my $list} );
  print "unlinked $_\n" for @$list;

This is a useful alternative to the C<verbose> key.

=item error

If present, will be interpreted as a reference to a list,
and will be used to store any errors that are encountered.
See the ERROR HANDLING section for more information.

Removing things is a much more dangerous proposition than
creating things. As such, there are certain conditions that
C<rmtree> may encounter that are so dangerous that the only
sane action left is to kill the program.

Use C<error> to trap all that is reasonable (problems with
permissions and the like), and let it die if things get out
of hand. This is the safest course of action.

=back

=head2 TRADITIONAL INTERFACE

The old interfaces of C<mkpath> and C<rmtree> take a reference to
a list of directories (to create or remove), followed by a series
of positional, numeric, modal parameters that control their behaviour.

This design made it difficult to add additional functionality, as
well as posed the problem of what to do when the calling code only
needs to set the last parameter. Even though the code doesn't care
how the initial positional parameters are set, the programmer is
forced to learn what the defaults are, and specify them.

Worse, if it turns out in the future that it would make more sense
to change the default behaviour of the first parameter (for example,
to avoid a security vulnerability), all existing code will remain
hard-wired to the wrong defaults.

Finally, a series of numeric parameters are much less self-documenting
in terms of communicating to the reader what the code is doing. Named
parameters do not have this problem.

In the traditional API, C<mkpath> takes three arguments:

=over 4

=item *

The name of the path to create, or a reference to a list of paths
to create,

=item *

a boolean value, which if TRUE will cause C<mkpath> to print the
name of each directory as it is created (defaults to FALSE), and

=item *

the numeric mode to use when creating the directories (defaults to
0777), to be modified by the current umask.

=back

It returns a list of all directories (including intermediates, determined
using the Unix '/' separator) created.  In scalar context it returns
the number of directories created.

If a system error prevents a directory from being created, then the
C<mkpath> function throws a fatal error with C<Carp::croak>. This error
can be trapped with an C<eval> block:

  eval { mkpath($dir) };
  if ($@) {
    print "Couldn't create $dir: $@";
  }

In the traditional API, C<rmtree> takes three arguments:

=over 4

=item *

the root of the subtree to delete, or a reference to a list of
roots. All of the files and directories below each root, as well
as the roots themselves, will be deleted. If you want to keep
the roots themselves, you must use the modern API.

=item *

a boolean value, which if TRUE will cause C<rmtree> to print a
message each time it examines a file, giving the name of the file,
and indicating whether it's using C<rmdir> or C<unlink> to remove
it, or that it's skipping it.  (defaults to FALSE)

=item *

a boolean value, which if TRUE will cause C<rmtree> to skip any
files to which you do not have delete access (if running under VMS)
or write access (if running under another OS). This will change
in the future when a criterion for 'delete permission' under OSs
other than VMS is settled.  (defaults to FALSE)

=back

It returns the number of files, directories and symlinks successfully
deleted.  Symlinks are simply deleted and not followed.

Note also that the occurrence of errors in C<rmtree> using the
traditional interface can be determined I<only> by trapping diagnostic
messages using C<$SIG{__WARN__}>; it is not apparent from the return
value. (The modern interface may use the C<error> parameter to
record any problems encountered).

=head2 ERROR HANDLING

If C<mkpath> or C<rmtree> encounter an error, a diagnostic message
will be printed to C<STDERR> via C<carp> (for non-fatal errors),
or via C<croak> (for fatal errors).

If this behaviour is not desirable, the C<error> attribute may be
used to hold a reference to a variable, which will be used to store
the diagnostics. The result is a reference to a list of hash
references. For each hash reference, the key is the name of the
file, and the value is the error message (usually the contents of
C<$!>). An example usage looks like:

  rmpath( 'foo/bar', 'bar/rat', {error => \my $err} );
  for my $diag (@$err) {
    my ($file, $message) = each %$diag;
    print "problem unlinking $file: $message\n";
  }

If no errors are encountered, C<$err> will point to an empty list
(thus there is no need to test for C<undef>). If a general error
is encountered (for instance, C<rmtree> attempts to remove a directory
tree that does not exist), the diagnostic key will be empty, only
the value will be set:

  rmpath( '/no/such/path', {error => \my $err} );
  for my $diag (@$err) {
    my ($file, $message) = each %$diag;
    if ($file eq '') {
      print "general error: $message\n";
    }
  }

=head2 NOTES

C<File::Path> blindly exports C<mkpath> and C<rmtree> into the
current namespace. These days, this is considered bad style, but
to change it now would break too much code. Nonetheless, you are
invited to specify what it is you are expecting to use:

  use File::Path 'rmtree';

=head3 HEURISTICS

The functions detect (as far as possible) which way they are being
called and will act appropriately. It is important to remember that
the heuristic for detecting the old style is either the presence
of an array reference, or two or three parameters total and second
and third parameters are numeric. Hence...

    mkpath 486, 487, 488;

... will not assume the modern style and create three directories, rather
it will create one directory verbosely, setting the permission to
0750 (488 being the decimal equivalent of octal 750). Here, old
style trumps new. It must, for backwards compatibility reasons.

If you want to ensure there is absolutely no ambiguity about which
way the function will behave, make sure the first parameter is a
reference to a one-element list, to force the old style interpretation:

    mkpath [486], 487, 488;

and get only one directory created. Or add a reference to an empty
parameter hash, to force the new style:

    mkpath 486, 487, 488, {};

... and hence create the three directories. If the empty hash
reference seems a little strange to your eyes, or you suspect a
subsequent programmer might I<helpfully> optimise it away, you
can add a parameter set to a default value:

    mkpath 486, 487, 488, {verbose => 0};

=head3 SECURITY CONSIDERATIONS

There were race conditions 1.x implementations of File::Path's
C<rmtree> function (although sometimes patched depending on the OS
distribution or platform). The 2.0 version contains code to avoid the
problem mentioned in CVE-2002-0435.

See the following pages for more information:

  http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=286905
  http://www.nntp.perl.org/group/perl.perl5.porters/2005/01/msg97623.html
  http://www.debian.org/security/2005/dsa-696

Additionally, unless the C<safe> parameter is set (or the
third parameter in the traditional interface is TRUE), should a
C<rmtree> be interrupted, files that were originally in read-only
mode may now have their permissions set to a read-write (or "delete
OK") mode.

=head1 DIAGNOSTICS

FATAL errors will cause the program to halt (C<croak>), since the
problem is so severe that it would be dangerous to continue. (This
can always be trapped with C<eval>, but it's not a good idea. Under
the circumstances, dying is the best thing to do).

SEVERE errors may be trapped using the modern interface. If the
they are not trapped, or the old interface is used, such an error
will cause the program will halt.

All other errors may be trapped using the modern interface, otherwise
they will be C<carp>ed about. Program execution will not be halted.

=over 4

=item mkdir [path]: [errmsg] (SEVERE)

C<mkpath> was unable to create the path. Probably some sort of
permissions error at the point of departure, or insufficient resources
(such as free inodes on Unix).

=item No root path(s) specified

C<mkpath> was not given any paths to create. This message is only
emitted if the routine is called with the traditional interface.
The modern interface will remain silent if given nothing to do.

=item No such file or directory

On Windows, if C<mkpath> gives you this warning, it may mean that
you have exceeded your filesystem's maximum path length.

=item cannot fetch initial working directory: [errmsg]

C<rmtree> attempted to determine the initial directory by calling
C<Cwd::getcwd>, but the call failed for some reason. No attempt
will be made to delete anything.

=item cannot stat initial working directory: [errmsg]

C<rmtree> attempted to stat the initial directory (after having
successfully obtained its name via C<getcwd>), however, the call
failed for some reason. No attempt will be made to delete anything.

=item cannot chdir to [dir]: [errmsg]

C<rmtree> attempted to set the working directory in order to
begin deleting the objects therein, but was unsuccessful. This is
usually a permissions issue. The routine will continue to delete
other things, but this directory will be left intact.

=item directory [dir] changed before chdir, expected dev=[n] inode=[n], actual dev=[n] ino=[n], aborting. (FATAL)

C<rmtree> recorded the device and inode of a directory, and then
moved into it. It then performed a C<stat> on the current directory
and detected that the device and inode were no longer the same. As
this is at the heart of the race condition problem, the program
will die at this point.

=item cannot make directory [dir] read+writeable: [errmsg]

C<rmtree> attempted to change the permissions on the current directory
to ensure that subsequent unlinkings would not run into problems,
but was unable to do so. The permissions remain as they were, and
the program will carry on, doing the best it can.

=item cannot read [dir]: [errmsg]

C<rmtree> tried to read the contents of the directory in order
to acquire the names of the directory entries to be unlinked, but
was unsuccessful. This is usually a permissions issue. The
program will continue, but the files in this directory will remain
after the call.

=item cannot reset chmod [dir]: [errmsg]

C<rmtree>, after having deleted everything in a directory, attempted
to restore its permissions to the original state but failed. The
directory may wind up being left behind.

=item cannot chdir to [parent-dir] from [child-dir]: [errmsg], aborting. (FATAL)

C<rmtree>, after having deleted everything and restored the permissions
of a directory, was unable to chdir back to the parent. This is usually
a sign that something evil this way comes.

=item cannot stat prior working directory [dir]: [errmsg], aborting. (FATAL)

C<rmtree> was unable to stat the parent directory after have returned
from the child. Since there is no way of knowing if we returned to
where we think we should be (by comparing device and inode) the only
way out is to C<croak>.

=item previous directory [parent-dir] changed before entering [child-dir], expected dev=[n] inode=[n], actual dev=[n] ino=[n], aborting. (FATAL)

When C<rmtree> returned from deleting files in a child directory, a
check revealed that the parent directory it returned to wasn't the one
it started out from. This is considered a sign of malicious activity.

=item cannot make directory [dir] writeable: [errmsg]

Just before removing a directory (after having successfully removed
everything it contained), C<rmtree> attempted to set the permissions
on the directory to ensure it could be removed and failed. Program
execution continues, but the directory may possibly not be deleted.

=item cannot remove directory [dir]: [errmsg]

C<rmtree> attempted to remove a directory, but failed. This may because
some objects that were unable to be removed remain in the directory, or
a permissions issue. The directory will be left behind.

=item cannot restore permissions of [dir] to [0nnn]: [errmsg]

After having failed to remove a directory, C<rmtree> was unable to
restore its permissions from a permissive state back to a possibly
more restrictive setting. (Permissions given in octal).

=item cannot make file [file] writeable: [errmsg]

C<rmtree> attempted to force the permissions of a file to ensure it
could be deleted, but failed to do so. It will, however, still attempt
to unlink the file.

=item cannot unlink file [file]: [errmsg]

C<rmtree> failed to remove a file. Probably a permissions issue.

=item cannot restore permissions of [file] to [0nnn]: [errmsg]

After having failed to remove a file, C<rmtree> was also unable
to restore the permissions on the file to a possibly less permissive
setting. (Permissions given in octal).

=back

=head1 SEE ALSO

=over 4

=item *

L<File::Remove>

Allows files and directories to be moved to the Trashcan/Recycle
Bin (where they may later be restored if necessary) if the operating
system supports such functionality. This feature may one day be
made available directly in C<File::Path>.

=item *

L<File::Find::Rule>

When removing directory trees, if you want to examine each file to
decide whether to delete it (and possibly leaving large swathes
alone), F<File::Find::Rule> offers a convenient and flexible approach
to examining directory trees.

=back

=head1 BUGS

Please report all bugs on the RT queue:

L<http://rt.cpan.org/NoAuth/Bugs.html?Dist=File-Path>

=head1 ACKNOWLEDGEMENTS

Paul Szabo identified the race condition originally, and Brendan
O'Dea wrote an implementation for Debian that addressed the problem.
That code was used as a basis for the current code. Their efforts
are greatly appreciated.

=head1 AUTHORS

Tim Bunce <F<Tim.Bunce@ig.co.uk>> and Charles Bailey
<F<bailey@newman.upenn.edu>>. Currently maintained by David Landgren
<F<david@landgren.net>>.

=head1 COPYRIGHT

This module is copyright (C) Charles Bailey, Tim Bunce and
David Landgren 1995-2007.  All rights reserved.

=head1 LICENSE

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
