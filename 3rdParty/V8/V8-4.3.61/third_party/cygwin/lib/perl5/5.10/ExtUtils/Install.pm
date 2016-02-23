package ExtUtils::Install;
use strict;

use vars qw(@ISA @EXPORT $VERSION $MUST_REBOOT %Config);

use AutoSplit;
use Carp ();
use Config qw(%Config);
use Cwd qw(cwd);
use Exporter;
use ExtUtils::Packlist;
use File::Basename qw(dirname);
use File::Compare qw(compare);
use File::Copy;
use File::Find qw(find);
use File::Path;
use File::Spec;


@ISA = ('Exporter');
@EXPORT = ('install','uninstall','pm_to_blib', 'install_default');

=pod 

=head1 NAME

ExtUtils::Install - install files from here to there

=head1 SYNOPSIS

  use ExtUtils::Install;

  install({ 'blib/lib' => 'some/install/dir' } );

  uninstall($packlist);

  pm_to_blib({ 'lib/Foo/Bar.pm' => 'blib/lib/Foo/Bar.pm' });
    
=head1 VERSION

1.51

=cut

$VERSION = '1.50_01';
$VERSION = eval $VERSION;

=pod

=head1 DESCRIPTION

Handles the installing and uninstalling of perl modules, scripts, man
pages, etc...

Both install() and uninstall() are specific to the way
ExtUtils::MakeMaker handles the installation and deinstallation of
perl modules. They are not designed as general purpose tools.

On some operating systems such as Win32 installation may not be possible
until after a reboot has occured. This can have varying consequences:
removing an old DLL does not impact programs using the new one, but if
a new DLL cannot be installed properly until reboot then anything
depending on it must wait. The package variable

  $ExtUtils::Install::MUST_REBOOT

is used to store this status.

If this variable is true then such an operation has occured and
anything depending on this module cannot proceed until a reboot
has occured.

If this value is defined but false then such an operation has
ocurred, but should not impact later operations.

=begin _private

=item _chmod($$;$)

Wrapper to chmod() for debugging and error trapping.

=item _warnonce(@)

Warns about something only once.

=item _choke(@)

Dies with a special message.

=end _private

=cut

my $Is_VMS     = $^O eq 'VMS';
my $Is_MacPerl = $^O eq 'MacOS';
my $Is_Win32   = $^O eq 'MSWin32';
my $Is_cygwin  = $^O eq 'cygwin';
my $CanMoveAtBoot = ($Is_Win32 || $Is_cygwin);

# *note* CanMoveAtBoot is only incidentally the same condition as below
# this needs not hold true in the future.
my $Has_Win32API_File = ($Is_Win32 || $Is_cygwin)
    ? (eval {require Win32API::File; 1} || 0)
    : 0;


my $Inc_uninstall_warn_handler;

# install relative to here

my $INSTALL_ROOT = $ENV{PERL_INSTALL_ROOT};

my $Curdir = File::Spec->curdir;
my $Updir  = File::Spec->updir;

sub _estr(@) {
    return join "\n",'!' x 72,@_,'!' x 72,'';
}

{my %warned;
sub _warnonce(@) {
    my $first=shift;
    my $msg=_estr "WARNING: $first",@_;
    warn $msg unless $warned{$msg}++;
}}

sub _choke(@) {
    my $first=shift;
    my $msg=_estr "ERROR: $first",@_;
    Carp::croak($msg);
}


sub _chmod($$;$) {
    my ( $mode, $item, $verbose )=@_;
    $verbose ||= 0;
    if (chmod $mode, $item) {
        print "chmod($mode, $item)\n" if $verbose > 1;
    } else {
        my $err="$!";
        _warnonce "WARNING: Failed chmod($mode, $item): $err\n"
            if -e $item;
    }
}

=begin _private

=item _move_file_at_boot( $file, $target, $moan  )

OS-Specific, Win32/Cygwin

Schedules a file to be moved/renamed/deleted at next boot.
$file should be a filespec of an existing file
$target should be a ref to an array if the file is to be deleted
otherwise it should be a filespec for a rename. If the file is existing
it will be replaced.

Sets $MUST_REBOOT to 0 to indicate a deletion operation has occured
and sets it to 1 to indicate that a move operation has been requested.

returns 1 on success, on failure if $moan is false errors are fatal.
If $moan is true then returns 0 on error and warns instead of dies.

=end _private

=cut



sub _move_file_at_boot { #XXX OS-SPECIFIC
    my ( $file, $target, $moan  )= @_;
    Carp::confess("Panic: Can't _move_file_at_boot on this platform!")
         unless $CanMoveAtBoot;

    my $descr= ref $target
                ? "'$file' for deletion"
                : "'$file' for installation as '$target'";

    if ( ! $Has_Win32API_File ) {

        my @msg=(
            "Cannot schedule $descr at reboot.",
            "Try installing Win32API::File to allow operations on locked files",
            "to be scheduled during reboot. Or try to perform the operation by",
            "hand yourself. (You may need to close other perl processes first)"
        );
        if ( $moan ) { _warnonce(@msg) } else { _choke(@msg) }
        return 0;
    }
    my $opts= Win32API::File::MOVEFILE_DELAY_UNTIL_REBOOT();
    $opts= $opts | Win32API::File::MOVEFILE_REPLACE_EXISTING()
        unless ref $target;

    _chmod( 0666, $file );
    _chmod( 0666, $target ) unless ref $target;

    if (Win32API::File::MoveFileEx( $file, $target, $opts )) {
        $MUST_REBOOT ||= ref $target ? 0 : 1;
        return 1;
    } else {
        my @msg=(
            "MoveFileEx $descr at reboot failed: $^E",
            "You may try to perform the operation by hand yourself. ",
            "(You may need to close other perl processes first).",
        );
        if ( $moan ) { _warnonce(@msg) } else { _choke(@msg) }
    }
    return 0;
}


=begin _private

=item _unlink_or_rename( $file, $tryhard, $installing )

OS-Specific, Win32/Cygwin

Tries to get a file out of the way by unlinking it or renaming it. On
some OS'es (Win32 based) DLL files can end up locked such that they can
be renamed but not deleted. Likewise sometimes a file can be locked such
that it cant even be renamed or changed except at reboot. To handle
these cases this routine finds a tempfile name that it can either rename
the file out of the way or use as a proxy for the install so that the
rename can happen later (at reboot).

  $file : the file to remove.
  $tryhard : should advanced tricks be used for deletion
  $installing : we are not merely deleting but we want to overwrite

When $tryhard is not true if the unlink fails its fatal. When $tryhard
is true then the file is attempted to be renamed. The renamed file is
then scheduled for deletion. If the rename fails then $installing
governs what happens. If it is false the failure is fatal. If it is true
then an attempt is made to schedule installation at boot using a
temporary file to hold the new file. If this fails then a fatal error is
thrown, if it succeeds it returns the temporary file name (which will be
a derivative of the original in the same directory) so that the caller can
use it to install under. In all other cases of success returns $file.
On failure throws a fatal error.

=end _private

=cut



sub _unlink_or_rename { #XXX OS-SPECIFIC
    my ( $file, $tryhard, $installing )= @_;

    _chmod( 0666, $file );
    my $unlink_count = 0;
    while (unlink $file) { $unlink_count++; }
    return $file if $unlink_count > 0;
    my $error="$!";

    _choke("Cannot unlink '$file': $!")
          unless $CanMoveAtBoot && $tryhard;

    my $tmp= "AAA";
    ++$tmp while -e "$file.$tmp";
    $tmp= "$file.$tmp";

    warn "WARNING: Unable to unlink '$file': $error\n",
         "Going to try to rename it to '$tmp'.\n";

    if ( rename $file, $tmp ) {
        warn "Rename succesful. Scheduling '$tmp'\nfor deletion at reboot.\n";
        # when $installing we can set $moan to true.
        # IOW, if we cant delete the renamed file at reboot its
        # not the end of the world. The other cases are more serious
        # and need to be fatal.
        _move_file_at_boot( $tmp, [], $installing );
        return $file;
    } elsif ( $installing ) {
        _warnonce("Rename failed: $!. Scheduling '$tmp'\nfor".
             " installation as '$file' at reboot.\n");
        _move_file_at_boot( $tmp, $file );
        return $tmp;
    } else {
        _choke("Rename failed:$!", "Cannot procede.");
    }

}


=pod

=head2 Functions

=begin _private

=item _get_install_skip

Handles loading the INSTALL.SKIP file. Returns an array of patterns to use.

=cut



sub _get_install_skip {
    my ( $skip, $verbose )= @_;
    if ($ENV{EU_INSTALL_IGNORE_SKIP}) {
        print "EU_INSTALL_IGNORE_SKIP is set, ignore skipfile settings\n"
            if $verbose>2;
        return [];
    }
    if ( ! defined $skip ) {
        print "Looking for install skip list\n"
            if $verbose>2;
        for my $file ( 'INSTALL.SKIP', $ENV{EU_INSTALL_SITE_SKIPFILE} ) {
            next unless $file;
            print "\tChecking for $file\n"
                if $verbose>2;
            if (-e $file) {
                $skip= $file;
                last;
            }
        }
    }
    if ($skip && !ref $skip) {
        print "Reading skip patterns from '$skip'.\n"
            if $verbose;
        if (open my $fh,$skip ) {
            my @patterns;
            while (<$fh>) {
                chomp;
                next if /^\s*(?:#|$)/;
                print "\tSkip pattern: $_\n" if $verbose>3;
                push @patterns, $_;
            }
            $skip= \@patterns;
        } else {
            warn "Can't read skip file:'$skip':$!\n";
            $skip=[];
        }
    } elsif ( UNIVERSAL::isa($skip,'ARRAY') ) {
        print "Using array for skip list\n"
            if $verbose>2;
    } elsif ($verbose) {
        print "No skip list found.\n"
            if $verbose>1;
        $skip= [];
    }
    warn "Got @{[0+@$skip]} skip patterns.\n"
        if $verbose>3;
    return $skip
}

=pod

=item _have_write_access

Abstract a -w check that tries to use POSIX::access() if possible.

=cut

{
    my  $has_posix;
    sub _have_write_access {
        my $dir=shift;
        unless (defined $has_posix) {
            $has_posix= (!$Is_cygwin && !$Is_Win32
			 && eval 'local $^W; require POSIX; 1') || 0;
        }
        if ($has_posix) {
            return POSIX::access($dir, POSIX::W_OK());
        } else {
            return -w $dir;
        }
    }
}

=pod

=item _can_write_dir(C<$dir>)

Checks whether a given directory is writable, taking account
the possibility that the directory might not exist and would have to
be created first.

Returns a list, containing: C<($writable, $determined_by, @create)>

C<$writable> says whether whether the directory is (hypothetically) writable

C<$determined_by> is the directory the status was determined from. It will be
either the C<$dir>, or one of its parents.

C<@create> is a list of directories that would probably have to be created
to make the requested directory. It may not actually be correct on
relative paths with C<..> in them. But for our purposes it should work ok

=cut


sub _can_write_dir {
    my $dir=shift;
    return
        unless defined $dir and length $dir;

    my ($vol, $dirs, $file) = File::Spec->splitpath($dir,1);
    my @dirs = File::Spec->splitdir($dirs);
    unshift @dirs, File::Spec->curdir
        unless File::Spec->file_name_is_absolute($dir);

    my $path='';
    my @make;
    while (@dirs) {
        if ($Is_VMS) {
            $dir = File::Spec->catdir($vol,@dirs);
        }
        else {
            $dir = File::Spec->catdir(@dirs);
            $dir = File::Spec->catpath($vol,$dir,'')
                    if defined $vol and length $vol;
        }
        next if ( $dir eq $path );
        if ( ! -e $dir ) {
            unshift @make,$dir;
            next;
        }
        if ( _have_write_access($dir) ) {
            return 1,$dir,@make
        } else {
            return 0,$dir,@make
        }
    } continue {
        pop @dirs;
    }
    return 0;
}

=pod

=item _mkpath($dir,$show,$mode,$verbose,$dry_run)

Wrapper around File::Path::mkpath() to handle errors.

If $verbose is true and >1 then additional diagnostics will be produced, also
this will force $show to true.

If $dry_run is true then the directory will not be created but a check will be
made to see whether it would be possible to write to the directory, or that
it would be possible to create the directory.

If $dry_run is not true dies if the directory can not be created or is not
writable.

=cut

sub _mkpath {
    my ($dir,$show,$mode,$verbose,$dry_run)=@_;
    if ( $verbose && $verbose > 1 && ! -d $dir) {
        $show= 1;
        printf "mkpath(%s,%d,%#o)\n", $dir, $show, $mode;
    }
    if (!$dry_run) {
        if ( ! eval { File::Path::mkpath($dir,$show,$mode); 1 } ) {
            _choke("Can't create '$dir'","$@");
        }

    }
    my ($can,$root,@make)=_can_write_dir($dir);
    if (!$can) {
        my @msg=(
            "Can't create '$dir'",
            $root ? "Do not have write permissions on '$root'"
                  : "Unknown Error"
        );
        if ($dry_run) {
            _warnonce @msg;
        } else {
            _choke @msg;
        }
    } elsif ($show and $dry_run) {
        print "$_\n" for @make;
    }
    
}

=pod

=item _copy($from,$to,$verbose,$dry_run)

Wrapper around File::Copy::copy to handle errors.

If $verbose is true and >1 then additional dignostics will be emitted.

If $dry_run is true then the copy will not actually occur.

Dies if the copy fails.

=cut


sub _copy {
    my ( $from, $to, $verbose, $dry_run)=@_;
    if ($verbose && $verbose>1) {
        printf "copy(%s,%s)\n", $from, $to;
    }
    if (!$dry_run) {
        File::Copy::copy($from,$to)
            or Carp::croak( _estr "ERROR: Cannot copy '$from' to '$to': $!" );
    }
}

=pod

=item _chdir($from)

Wrapper around chdir to catch errors.

If not called in void context returns the cwd from before the chdir.

dies on error.

=cut

sub _chdir {
    my ($dir)= @_;
    my $ret;
    if (defined wantarray) {
        $ret= cwd;
    }
    chdir $dir
        or _choke("Couldn't chdir to '$dir': $!");
    return $ret;
}

=pod

=end _private

=over 4

=item B<install>

    # deprecated forms
    install(\%from_to);
    install(\%from_to, $verbose, $dry_run, $uninstall_shadows, 
                $skip, $always_copy, \%result);

    # recommended form as of 1.47                
    install([ 
        from_to => \%from_to,
        verbose => 1, 
        dry_run => 0,
        uninstall_shadows => 1,
        skip => undef,
        always_copy => 1,
        result => \%install_results,
    ]);
    

Copies each directory tree of %from_to to its corresponding value
preserving timestamps and permissions.

There are two keys with a special meaning in the hash: "read" and
"write".  These contain packlist files.  After the copying is done,
install() will write the list of target files to $from_to{write}. If
$from_to{read} is given the contents of this file will be merged into
the written file. The read and the written file may be identical, but
on AFS it is quite likely that people are installing to a different
directory than the one where the files later appear.

If $verbose is true, will print out each file removed.  Default is
false.  This is "make install VERBINST=1". $verbose values going
up to 5 show increasingly more diagnostics output.

If $dry_run is true it will only print what it was going to do
without actually doing it.  Default is false.

If $uninstall_shadows is true any differing versions throughout @INC
will be uninstalled.  This is "make install UNINST=1"

As of 1.37_02 install() supports the use of a list of patterns to filter out 
files that shouldn't be installed. If $skip is omitted or undefined then 
install will try to read the list from INSTALL.SKIP in the CWD. This file is 
a list of regular expressions and is just like the MANIFEST.SKIP file used 
by L<ExtUtils::Manifest>.

A default site INSTALL.SKIP may be provided by setting then environment 
variable EU_INSTALL_SITE_SKIPFILE, this will only be used when there isn't a 
distribution specific INSTALL.SKIP. If the environment variable 
EU_INSTALL_IGNORE_SKIP is true then no install file filtering will be 
performed.

If $skip is undefined then the skip file will be autodetected and used if it 
is found. If $skip is a reference to an array then it is assumed the array 
contains the list of patterns, if $skip is a true non reference it is 
assumed to be the filename holding the list of patterns, any other value of 
$skip is taken to mean that no install filtering should occur.

B<Changes As of Version 1.47>

As of version 1.47 the following additions were made to the install interface.
Note that the new argument style and use of the %result hash is recommended.

The $always_copy parameter which when true causes files to be updated 
regardles as to whether they have changed, if it is defined but false then 
copies are made only if the files have changed, if it is undefined then the 
value of the environment variable EU_INSTALL_ALWAYS_COPY is used as default.

The %result hash will be populated with the various keys/subhashes reflecting 
the install. Currently these keys and their structure are:

    install             => { $target    => $source },
    install_fail        => { $target    => $source },
    install_unchanged   => { $target    => $source },
        
    install_filtered    => { $source    => $pattern },
    
    uninstall           => { $uninstalled => $source },
    uninstall_fail      => { $uninstalled => $source },
        
where C<$source> is the filespec of the file being installed. C<$target> is where
it is being installed to, and C<$uninstalled> is any shadow file that is in C<@INC>
or C<$ENV{PERL5LIB}> or other standard locations, and C<$pattern> is the pattern that
caused a source file to be skipped. In future more keys will be added, such as to
show created directories, however this requires changes in other modules and must 
therefore wait.
        
These keys will be populated before any exceptions are thrown should there be an 
error. 

Note that all updates of the %result are additive, the hash will not be
cleared before use, thus allowing status results of many installs to be easily
aggregated.

B<NEW ARGUMENT STYLE>

If there is only one argument and it is a reference to an array then
the array is assumed to contain a list of key-value pairs specifying 
the options. In this case the option "from_to" is mandatory. This style
means that you dont have to supply a cryptic list of arguments and can
use a self documenting argument list that is easier to understand.

This is now the recommended interface to install().

B<RETURN>

If all actions were successful install will return a hashref of the results 
as described above for the $result parameter. If any action is a failure 
then install will die, therefore it is recommended to pass in the $result 
parameter instead of using the return value. If the result parameter is 
provided then the returned hashref will be the passed in hashref.

=cut

sub install { #XXX OS-SPECIFIC
    my($from_to,$verbose,$dry_run,$uninstall_shadows,$skip,$always_copy,$result) = @_;
    if (@_==1 and eval { 1+@$from_to }) {
        my %opts        = @$from_to;
        $from_to        = $opts{from_to} 
                            or Carp::confess("from_to is a mandatory parameter");
        $verbose        = $opts{verbose};
        $dry_run        = $opts{dry_run};
        $uninstall_shadows  = $opts{uninstall_shadows};
        $skip           = $opts{skip};
        $always_copy    = $opts{always_copy};
        $result         = $opts{result};
    }
    
    $result ||= {};
    $verbose ||= 0;
    $dry_run  ||= 0;

    $skip= _get_install_skip($skip,$verbose);
    $always_copy =  $ENV{EU_INSTALL_ALWAYS_COPY}
                 || $ENV{EU_ALWAYS_COPY} 
                 || 0
        unless defined $always_copy;

    my(%from_to) = %$from_to;
    my(%pack, $dir, %warned);
    my($packlist) = ExtUtils::Packlist->new();

    local(*DIR);
    for (qw/read write/) {
        $pack{$_}=$from_to{$_};
        delete $from_to{$_};
    }
    my $tmpfile = install_rooted_file($pack{"read"});
    $packlist->read($tmpfile) if (-f $tmpfile);
    my $cwd = cwd();
    my @found_files;
    my %check_dirs;
    
    MOD_INSTALL: foreach my $source (sort keys %from_to) {
        #copy the tree to the target directory without altering
        #timestamp and permission and remember for the .packlist
        #file. The packlist file contains the absolute paths of the
        #install locations. AFS users may call this a bug. We'll have
        #to reconsider how to add the means to satisfy AFS users also.

        #October 1997: we want to install .pm files into archlib if
        #there are any files in arch. So we depend on having ./blib/arch
        #hardcoded here.

        my $targetroot = install_rooted_dir($from_to{$source});

        my $blib_lib  = File::Spec->catdir('blib', 'lib');
        my $blib_arch = File::Spec->catdir('blib', 'arch');
        if ($source eq $blib_lib and
            exists $from_to{$blib_arch} and
            directory_not_empty($blib_arch)
        ){
            $targetroot = install_rooted_dir($from_to{$blib_arch});
            print "Files found in $blib_arch: installing files in $blib_lib into architecture dependent library tree\n";
        }

        next unless -d $source;
        _chdir($source);
        # 5.5.3's File::Find missing no_chdir option
        # XXX OS-SPECIFIC
        # File::Find seems to always be Unixy except on MacPerl :(
        my $current_directory= $Is_MacPerl ? $Curdir : '.';
        find(sub {
            my ($mode,$size,$atime,$mtime) = (stat)[2,7,8,9];

            return if !-f _;
            my $origfile = $_;

            return if $origfile eq ".exists";
            my $targetdir  = File::Spec->catdir($targetroot, $File::Find::dir);
            my $targetfile = File::Spec->catfile($targetdir, $origfile);
            my $sourcedir  = File::Spec->catdir($source, $File::Find::dir);
            my $sourcefile = File::Spec->catfile($sourcedir, $origfile);

            for my $pat (@$skip) {
                if ( $sourcefile=~/$pat/ ) {
                    print "Skipping $targetfile (filtered)\n"
                        if $verbose>1;
                    $result->{install_filtered}{$sourcefile} = $pat;
                    return;
                }
            }
            # we have to do this for back compat with old File::Finds
            # and because the target is relative
            my $save_cwd = _chdir($cwd); 
            my $diff = 0;
            # XXX: I wonder how useful this logic is actually -- demerphq
            if ( $always_copy or !-f $targetfile or -s $targetfile != $size) {
                $diff++;
            } else {
                # we might not need to copy this file
                $diff = compare($sourcefile, $targetfile);
            }
            $check_dirs{$targetdir}++ 
                unless -w $targetfile;
            
            push @found_files,
                [ $diff, $File::Find::dir, $origfile,
                  $mode, $size, $atime, $mtime,
                  $targetdir, $targetfile, $sourcedir, $sourcefile,
                  
                ];  
            #restore the original directory we were in when File::Find
            #called us so that it doesnt get horribly confused.
            _chdir($save_cwd);                
        }, $current_directory ); 
        _chdir($cwd);
    }   
    foreach my $targetdir (sort keys %check_dirs) {
        _mkpath( $targetdir, 0, 0755, $verbose, $dry_run );
    }
    foreach my $found (@found_files) {
        my ($diff, $ffd, $origfile, $mode, $size, $atime, $mtime,
            $targetdir, $targetfile, $sourcedir, $sourcefile)= @$found;
        
        my $realtarget= $targetfile;
        if ($diff) {
            eval {
                if (-f $targetfile) {
                    print "_unlink_or_rename($targetfile)\n" if $verbose>1;
                    $targetfile= _unlink_or_rename( $targetfile, 'tryhard', 'install' )
                        unless $dry_run;
                } elsif ( ! -d $targetdir ) {
                    _mkpath( $targetdir, 0, 0755, $verbose, $dry_run );
                }
                print "Installing $targetfile\n";
            
                _copy( $sourcefile, $targetfile, $verbose, $dry_run, );
                
            
                #XXX OS-SPECIFIC
                print "utime($atime,$mtime,$targetfile)\n" if $verbose>1;
                utime($atime,$mtime + $Is_VMS,$targetfile) unless $dry_run>1;
    
    
                $mode = 0444 | ( $mode & 0111 ? 0111 : 0 );
                $mode = $mode | 0222
                    if $realtarget ne $targetfile;
                _chmod( $mode, $targetfile, $verbose );
                $result->{install}{$targetfile} = $sourcefile;
                1
            } or do {
                $result->{install_fail}{$targetfile} = $sourcefile;
                die $@;
            };
        } else {
            $result->{install_unchanged}{$targetfile} = $sourcefile;
            print "Skipping $targetfile (unchanged)\n" if $verbose;
        }

        if ( $uninstall_shadows ) {
            inc_uninstall($sourcefile,$ffd, $verbose,
                          $dry_run,
                          $realtarget ne $targetfile ? $realtarget : "",
                          $result);
        }

        # Record the full pathname.
        $packlist->{$targetfile}++;
    }

    if ($pack{'write'}) {
        $dir = install_rooted_dir(dirname($pack{'write'}));
        _mkpath( $dir, 0, 0755, $verbose, $dry_run );
        print "Writing $pack{'write'}\n";
        $packlist->write(install_rooted_file($pack{'write'})) unless $dry_run;
    }

    _do_cleanup($verbose);
    return $result;
}

=begin _private

=item _do_cleanup

Standardize finish event for after another instruction has occured.
Handles converting $MUST_REBOOT to a die for instance.

=end _private

=cut

sub _do_cleanup {
    my ($verbose) = @_;
    if ($MUST_REBOOT) {
        die _estr "Operation not completed! ",
            "You must reboot to complete the installation.",
            "Sorry.";
    } elsif (defined $MUST_REBOOT & $verbose) {
        warn _estr "Installation will be completed at the next reboot.\n",
             "However it is not necessary to reboot immediately.\n";
    }
}

=begin _undocumented

=item install_rooted_file( $file )

Returns $file, or catfile($INSTALL_ROOT,$file) if $INSTALL_ROOT
is defined.

=item install_rooted_dir( $dir )

Returns $dir, or catdir($INSTALL_ROOT,$dir) if $INSTALL_ROOT
is defined.

=end _undocumented

=cut


sub install_rooted_file {
    if (defined $INSTALL_ROOT) {
        File::Spec->catfile($INSTALL_ROOT, $_[0]);
    } else {
        $_[0];
    }
}


sub install_rooted_dir {
    if (defined $INSTALL_ROOT) {
        File::Spec->catdir($INSTALL_ROOT, $_[0]);
    } else {
        $_[0];
    }
}

=begin _undocumented

=item forceunlink( $file, $tryhard )

Tries to delete a file. If $tryhard is true then we will use whatever
devious tricks we can to delete the file. Currently this only applies to
Win32 in that it will try to use Win32API::File to schedule a delete at
reboot. A wrapper for _unlink_or_rename().

=end _undocumented

=cut


sub forceunlink {
    my ( $file, $tryhard )= @_; #XXX OS-SPECIFIC
    _unlink_or_rename( $file, $tryhard, not("installing") );
}

=begin _undocumented

=item directory_not_empty( $dir )

Returns 1 if there is an .exists file somewhere in a directory tree.
Returns 0 if there is not.

=end _undocumented

=cut

sub directory_not_empty ($) {
  my($dir) = @_;
  my $files = 0;
  find(sub {
           return if $_ eq ".exists";
           if (-f) {
             $File::Find::prune++;
             $files = 1;
           }
       }, $dir);
  return $files;
}

=pod

=item B<install_default> I<DISCOURAGED>

    install_default();
    install_default($fullext);

Calls install() with arguments to copy a module from blib/ to the
default site installation location.

$fullext is the name of the module converted to a directory
(ie. Foo::Bar would be Foo/Bar).  If $fullext is not specified, it
will attempt to read it from @ARGV.

This is primarily useful for install scripts.

B<NOTE> This function is not really useful because of the hard-coded
install location with no way to control site vs core vs vendor
directories and the strange way in which the module name is given.
Consider its use discouraged.

=cut

sub install_default {
  @_ < 2 or Carp::croak("install_default should be called with 0 or 1 argument");
  my $FULLEXT = @_ ? shift : $ARGV[0];
  defined $FULLEXT or die "Do not know to where to write install log";
  my $INST_LIB = File::Spec->catdir($Curdir,"blib","lib");
  my $INST_ARCHLIB = File::Spec->catdir($Curdir,"blib","arch");
  my $INST_BIN = File::Spec->catdir($Curdir,'blib','bin');
  my $INST_SCRIPT = File::Spec->catdir($Curdir,'blib','script');
  my $INST_MAN1DIR = File::Spec->catdir($Curdir,'blib','man1');
  my $INST_MAN3DIR = File::Spec->catdir($Curdir,'blib','man3');
  install({
           read => "$Config{sitearchexp}/auto/$FULLEXT/.packlist",
           write => "$Config{installsitearch}/auto/$FULLEXT/.packlist",
           $INST_LIB => (directory_not_empty($INST_ARCHLIB)) ?
                         $Config{installsitearch} :
                         $Config{installsitelib},
           $INST_ARCHLIB => $Config{installsitearch},
           $INST_BIN => $Config{installbin} ,
           $INST_SCRIPT => $Config{installscript},
           $INST_MAN1DIR => $Config{installman1dir},
           $INST_MAN3DIR => $Config{installman3dir},
          },1,0,0);
}


=item B<uninstall>

    uninstall($packlist_file);
    uninstall($packlist_file, $verbose, $dont_execute);

Removes the files listed in a $packlist_file.

If $verbose is true, will print out each file removed.  Default is
false.

If $dont_execute is true it will only print what it was going to do
without actually doing it.  Default is false.

=cut

sub uninstall {
    my($fil,$verbose,$dry_run) = @_;
    $verbose ||= 0;
    $dry_run  ||= 0;

    die _estr "ERROR: no packlist file found: '$fil'"
        unless -f $fil;
    # my $my_req = $self->catfile(qw(auto ExtUtils Install forceunlink.al));
    # require $my_req; # Hairy, but for the first
    my ($packlist) = ExtUtils::Packlist->new($fil);
    foreach (sort(keys(%$packlist))) {
        chomp;
        print "unlink $_\n" if $verbose;
        forceunlink($_,'tryhard') unless $dry_run;
    }
    print "unlink $fil\n" if $verbose;
    forceunlink($fil, 'tryhard') unless $dry_run;
    _do_cleanup($verbose);
}

=begin _undocumented

=item inc_uninstall($filepath,$libdir,$verbose,$dry_run,$ignore,$results)

Remove shadowed files. If $ignore is true then it is assumed to hold
a filename to ignore. This is used to prevent spurious warnings from
occuring when doing an install at reboot.

We now only die when failing to remove a file that has precedence over
our own, when our install has precedence we only warn.

$results is assumed to contain a hashref which will have the keys
'uninstall' and 'uninstall_fail' populated with  keys for the files
removed and values of the source files they would shadow.

=end _undocumented

=cut

sub inc_uninstall {
    my($filepath,$libdir,$verbose,$dry_run,$ignore,$results) = @_;
    my($dir);
    $ignore||="";
    my $file = (File::Spec->splitpath($filepath))[2];
    my %seen_dir = ();
    
    my @PERL_ENV_LIB = split $Config{path_sep}, defined $ENV{'PERL5LIB'}
      ? $ENV{'PERL5LIB'} : $ENV{'PERLLIB'} || '';
        
    my @dirs=( @PERL_ENV_LIB, 
               @INC, 
               @Config{qw(archlibexp
                          privlibexp
                          sitearchexp
                          sitelibexp)});        
    
    #warn join "\n","---",@dirs,"---";
    my $seen_ours;
    foreach $dir ( @dirs ) {
        my $canonpath = $Is_VMS ? $dir : File::Spec->canonpath($dir);
        next if $canonpath eq $Curdir;
        next if $seen_dir{$canonpath}++;
        my $targetfile = File::Spec->catfile($canonpath,$libdir,$file);
        next unless -f $targetfile;

        # The reason why we compare file's contents is, that we cannot
        # know, which is the file we just installed (AFS). So we leave
        # an identical file in place
        my $diff = 0;
        if ( -f $targetfile && -s _ == -s $filepath) {
            # We have a good chance, we can skip this one
            $diff = compare($filepath,$targetfile);
        } else {
            $diff++;
        }
        print "#$file and $targetfile differ\n" if $diff && $verbose > 1;

        if (!$diff or $targetfile eq $ignore) {
            $seen_ours = 1;
            next;
        }
        if ($dry_run) {
            $results->{uninstall}{$targetfile} = $filepath;
            if ($verbose) {
                $Inc_uninstall_warn_handler ||= ExtUtils::Install::Warn->new();
                $libdir =~ s|^\./||s ; # That's just cosmetics, no need to port. It looks prettier.
                $Inc_uninstall_warn_handler->add(
                                     File::Spec->catfile($libdir, $file),
                                     $targetfile
                                    );
            }
            # if not verbose, we just say nothing
        } else {
            print "Unlinking $targetfile (shadowing?)\n" if $verbose;
            eval {
                die "Fake die for testing" 
                    if $ExtUtils::Install::Testing and
                       ucase(File::Spec->canonpath($ExtUtils::Install::Testing)) eq ucase($targetfile);
                forceunlink($targetfile,'tryhard');
                $results->{uninstall}{$targetfile} = $filepath;
                1;
            } or do {
                $results->{fail_uninstall}{$targetfile} = $filepath;
                if ($seen_ours) { 
                    warn "Failed to remove probably harmless shadow file '$targetfile'\n";
                } else {
                    die "$@\n";
                }
            };
        }
    }
}

=begin _undocumented

=item run_filter($cmd,$src,$dest)

Filter $src using $cmd into $dest.

=end _undocumented

=cut

sub run_filter {
    my ($cmd, $src, $dest) = @_;
    local(*CMD, *SRC);
    open(CMD, "|$cmd >$dest") || die "Cannot fork: $!";
    open(SRC, $src)           || die "Cannot open $src: $!";
    my $buf;
    my $sz = 1024;
    while (my $len = sysread(SRC, $buf, $sz)) {
        syswrite(CMD, $buf, $len);
    }
    close SRC;
    close CMD or die "Filter command '$cmd' failed for $src";
}

=pod

=item B<pm_to_blib>

    pm_to_blib(\%from_to, $autosplit_dir);
    pm_to_blib(\%from_to, $autosplit_dir, $filter_cmd);

Copies each key of %from_to to its corresponding value efficiently.
Filenames with the extension .pm are autosplit into the $autosplit_dir.
Any destination directories are created.

$filter_cmd is an optional shell command to run each .pm file through
prior to splitting and copying.  Input is the contents of the module,
output the new module contents.

You can have an environment variable PERL_INSTALL_ROOT set which will
be prepended as a directory to each installed file (and directory).

=cut

sub pm_to_blib {
    my($fromto,$autodir,$pm_filter) = @_;

    _mkpath($autodir,0,0755);
    while(my($from, $to) = each %$fromto) {
        if( -f $to && -s $from == -s $to && -M $to < -M $from ) {
            print "Skip $to (unchanged)\n";
            next;
        }

        # When a pm_filter is defined, we need to pre-process the source first
        # to determine whether it has changed or not.  Therefore, only perform
        # the comparison check when there's no filter to be ran.
        #    -- RAM, 03/01/2001

        my $need_filtering = defined $pm_filter && length $pm_filter &&
                             $from =~ /\.pm$/;

        if (!$need_filtering && 0 == compare($from,$to)) {
            print "Skip $to (unchanged)\n";
            next;
        }
        if (-f $to){
            # we wont try hard here. its too likely to mess things up.
            forceunlink($to);
        } else {
            _mkpath(dirname($to),0,0755);
        }
        if ($need_filtering) {
            run_filter($pm_filter, $from, $to);
            print "$pm_filter <$from >$to\n";
        } else {
            _copy( $from, $to );
            print "cp $from $to\n";
        }
        my($mode,$atime,$mtime) = (stat $from)[2,8,9];
        utime($atime,$mtime+$Is_VMS,$to);
        _chmod(0444 | ( $mode & 0111 ? 0111 : 0 ),$to);
        next unless $from =~ /\.pm$/;
        _autosplit($to,$autodir);
    }
}


=begin _private

=item _autosplit

From 1.0307 back, AutoSplit will sometimes leave an open filehandle to
the file being split.  This causes problems on systems with mandatory
locking (ie. Windows).  So we wrap it and close the filehandle.

=end _private

=cut

sub _autosplit { #XXX OS-SPECIFIC
    my $retval = autosplit(@_);
    close *AutoSplit::IN if defined *AutoSplit::IN{IO};

    return $retval;
}


package ExtUtils::Install::Warn;

sub new { bless {}, shift }

sub add {
    my($self,$file,$targetfile) = @_;
    push @{$self->{$file}}, $targetfile;
}

sub DESTROY {
    unless(defined $INSTALL_ROOT) {
        my $self = shift;
        my($file,$i,$plural);
        foreach $file (sort keys %$self) {
            $plural = @{$self->{$file}} > 1 ? "s" : "";
            print "## Differing version$plural of $file found. You might like to\n";
            for (0..$#{$self->{$file}}) {
                print "rm ", $self->{$file}[$_], "\n";
                $i++;
            }
        }
        $plural = $i>1 ? "all those files" : "this file";
        my $inst = (_invokant() eq 'ExtUtils::MakeMaker')
                 ? ( $Config::Config{make} || 'make' ).' install'
                     . ( $Is_VMS ? '/MACRO="UNINST"=1' : ' UNINST=1' )
                 : './Build install uninst=1';
        print "## Running '$inst' will unlink $plural for you.\n";
    }
}

=begin _private

=item _invokant

Does a heuristic on the stack to see who called us for more intelligent
error messages. Currently assumes we will be called only by Module::Build
or by ExtUtils::MakeMaker.

=end _private

=cut

sub _invokant {
    my @stack;
    my $frame = 0;
    while (my $file = (caller($frame++))[1]) {
        push @stack, (File::Spec->splitpath($file))[2];
    }

    my $builder;
    my $top = pop @stack;
    if ($top =~ /^Build/i || exists($INC{'Module/Build.pm'})) {
        $builder = 'Module::Build';
    } else {
        $builder = 'ExtUtils::MakeMaker';
    }
    return $builder;
}

=pod

=back

=head1 ENVIRONMENT

=over 4

=item B<PERL_INSTALL_ROOT>

Will be prepended to each install path.

=item B<EU_INSTALL_IGNORE_SKIP>

Will prevent the automatic use of INSTALL.SKIP as the install skip file.

=item B<EU_INSTALL_SITE_SKIPFILE>

If there is no INSTALL.SKIP file in the make directory then this value
can be used to provide a default.

=item B<EU_INSTALL_ALWAYS_COPY>

If this environment variable is true then normal install processes will
always overwrite older identical files during the install process.

Note that the alias EU_ALWAYS_COPY will be supported if EU_INSTALL_ALWAYS_COPY
is not defined until at least the 1.50 release. Please ensure you use the
correct EU_INSTALL_ALWAYS_COPY. 

=back

=head1 AUTHOR

Original author lost in the mists of time.  Probably the same as Makemaker.

Production release currently maintained by demerphq C<yves at cpan.org>,
extensive changes by Michael G. Schwern.

Send bug reports via http://rt.cpan.org/.  Please send your
generated Makefile along with your report.

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

See L<http://www.perl.com/perl/misc/Artistic.html>


=cut

1;
