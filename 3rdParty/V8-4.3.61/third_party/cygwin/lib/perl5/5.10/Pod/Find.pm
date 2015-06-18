#############################################################################  
# Pod/Find.pm -- finds files containing POD documentation
#
# Author: Marek Rouchal <marekr@cpan.org>
# 
# Copyright (C) 1999-2000 by Marek Rouchal (and borrowing code
# from Nick Ing-Simmon's PodToHtml). All rights reserved.
# This file is part of "PodParser". Pod::Find is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

package Pod::Find;

use vars qw($VERSION);
$VERSION = 1.34;   ## Current version of this package
require  5.005;   ## requires this Perl version or later
use Carp;

#############################################################################

=head1 NAME

Pod::Find - find POD documents in directory trees

=head1 SYNOPSIS

  use Pod::Find qw(pod_find simplify_name);
  my %pods = pod_find({ -verbose => 1, -inc => 1 });
  foreach(keys %pods) {
     print "found library POD `$pods{$_}' in $_\n";
  }

  print "podname=",simplify_name('a/b/c/mymodule.pod'),"\n";

  $location = pod_where( { -inc => 1 }, "Pod::Find" );

=head1 DESCRIPTION

B<Pod::Find> provides a set of functions to locate POD files.  Note that
no function is exported by default to avoid pollution of your namespace,
so be sure to specify them in the B<use> statement if you need them:

  use Pod::Find qw(pod_find);

From this version on the typical SCM (software configuration management)
files/directories like RCS, CVS, SCCS, .svn are ignored.

=cut

use strict;
#use diagnostics;
use Exporter;
use File::Spec;
use File::Find;
use Cwd;

use vars qw(@ISA @EXPORT_OK $VERSION);
@ISA = qw(Exporter);
@EXPORT_OK = qw(&pod_find &simplify_name &pod_where &contains_pod);

# package global variables
my $SIMPLIFY_RX;

=head2 C<pod_find( { %opts } , @directories )>

The function B<pod_find> searches for POD documents in a given set of
files and/or directories. It returns a hash with the file names as keys
and the POD name as value. The POD name is derived from the file name
and its position in the directory tree.

E.g. when searching in F<$HOME/perl5lib>, the file
F<$HOME/perl5lib/MyModule.pm> would get the POD name I<MyModule>,
whereas F<$HOME/perl5lib/Myclass/Subclass.pm> would be
I<Myclass::Subclass>. The name information can be used for POD
translators.

Only text files containing at least one valid POD command are found.

A warning is printed if more than one POD file with the same POD name
is found, e.g. F<CPAN.pm> in different directories. This usually
indicates duplicate occurrences of modules in the I<@INC> search path.

B<OPTIONS> The first argument for B<pod_find> may be a hash reference
with options. The rest are either directories that are searched
recursively or files.  The POD names of files are the plain basenames
with any Perl-like extension (.pm, .pl, .pod) stripped.

=over 4

=item C<-verbose =E<gt> 1>

Print progress information while scanning.

=item C<-perl =E<gt> 1>

Apply Perl-specific heuristics to find the correct PODs. This includes
stripping Perl-like extensions, omitting subdirectories that are numeric
but do I<not> match the current Perl interpreter's version id, suppressing
F<site_perl> as a module hierarchy name etc.

=item C<-script =E<gt> 1>

Search for PODs in the current Perl interpreter's installation 
B<scriptdir>. This is taken from the local L<Config|Config> module.

=item C<-inc =E<gt> 1>

Search for PODs in the current Perl interpreter's I<@INC> paths. This
automatically considers paths specified in the C<PERL5LIB> environment
as this is prepended to I<@INC> by the Perl interpreter itself.

=back

=cut

# return a hash of the POD files found
# first argument may be a hashref (options),
# rest is a list of directories to search recursively
sub pod_find
{
    my %opts;
    if(ref $_[0]) {
        %opts = %{shift()};
    }

    $opts{-verbose} ||= 0;
    $opts{-perl}    ||= 0;

    my (@search) = @_;

    if($opts{-script}) {
        require Config;
        push(@search, $Config::Config{scriptdir})
            if -d $Config::Config{scriptdir};
        $opts{-perl} = 1;
    }

    if($opts{-inc}) {
        if ($^O eq 'MacOS') {
            # tolerate '.', './some_dir' and '(../)+some_dir' on Mac OS
            my @new_INC = @INC;
            for (@new_INC) {
                if ( $_ eq '.' ) {
                    $_ = ':';
                } elsif ( $_ =~ s|^((?:\.\./)+)|':' x (length($1)/3)|e ) {
                    $_ = ':'. $_;
                } else {
                    $_ =~ s|^\./|:|;
                }
            }
            push(@search, grep($_ ne File::Spec->curdir, @new_INC));
        } else {
            push(@search, grep($_ ne File::Spec->curdir, @INC));
        }

        $opts{-perl} = 1;
    }

    if($opts{-perl}) {
        require Config;
        # this code simplifies the POD name for Perl modules:
        # * remove "site_perl"
        # * remove e.g. "i586-linux" (from 'archname')
        # * remove e.g. 5.00503
        # * remove pod/ if followed by *.pod (e.g. in pod/perlfunc.pod)

        # Mac OS:
        # * remove ":?site_perl:"
        # * remove :?pod: if followed by *.pod (e.g. in :pod:perlfunc.pod)

        if ($^O eq 'MacOS') {
            $SIMPLIFY_RX =
              qq!^(?i:\:?site_perl\:|\:?pod\:(?=.*?\\.pod\\z))*!;
        } else {
            $SIMPLIFY_RX =
              qq!^(?i:site(_perl)?/|\Q$Config::Config{archname}\E/|\\d+\\.\\d+([_.]?\\d+)?/|pod/(?=.*?\\.pod\\z))*!;
        }
    }

    my %dirs_visited;
    my %pods;
    my %names;
    my $pwd = cwd();

    foreach my $try (@search) {
        unless(File::Spec->file_name_is_absolute($try)) {
            # make path absolute
            $try = File::Spec->catfile($pwd,$try);
        }
        # simplify path
        # on VMS canonpath will vmsify:[the.path], but File::Find::find
        # wants /unixy/paths
        $try = File::Spec->canonpath($try) if ($^O ne 'VMS');
        $try = VMS::Filespec::unixify($try) if ($^O eq 'VMS');
        my $name;
        if(-f $try) {
            if($name = _check_and_extract_name($try, $opts{-verbose})) {
                _check_for_duplicates($try, $name, \%names, \%pods);
            }
            next;
        }
        my $root_rx = $^O eq 'MacOS' ? qq!^\Q$try\E! : qq!^\Q$try\E/!;
        File::Find::find( sub {
            my $item = $File::Find::name;
            if(-d) {
                if($item =~ m{/(?:RCS|CVS|SCCS|\.svn)$}) {
                    $File::Find::prune = 1;
                    return;
                }
                elsif($dirs_visited{$item}) {
                    warn "Directory '$item' already seen, skipping.\n"
                        if($opts{-verbose});
                    $File::Find::prune = 1;
                    return;
                }
                else {
                    $dirs_visited{$item} = 1;
                }
                if($opts{-perl} && /^(\d+\.[\d_]+)\z/s && eval "$1" != $]) {
                    $File::Find::prune = 1;
                    warn "Perl $] version mismatch on $_, skipping.\n"
                        if($opts{-verbose});
                }
                return;
            }
            if($name = _check_and_extract_name($item, $opts{-verbose}, $root_rx)) {
                _check_for_duplicates($item, $name, \%names, \%pods);
            }
        }, $try); # end of File::Find::find
    }
    chdir $pwd;
    %pods;
}

sub _check_for_duplicates {
    my ($file, $name, $names_ref, $pods_ref) = @_;
    if($$names_ref{$name}) {
        warn "Duplicate POD found (shadowing?): $name ($file)\n";
        warn "    Already seen in ",
            join(' ', grep($$pods_ref{$_} eq $name, keys %$pods_ref)),"\n";
    }
    else {
        $$names_ref{$name} = 1;
    }
    $$pods_ref{$file} = $name;
}

sub _check_and_extract_name {
    my ($file, $verbose, $root_rx) = @_;

    # check extension or executable flag
    # this involves testing the .bat extension on Win32!
    unless(-f $file && -T $file && ($file =~ /\.(pod|pm|plx?)\z/i || -x $file )) {
      return undef;
    }

    return undef unless contains_pod($file,$verbose);

    # strip non-significant path components
    # TODO what happens on e.g. Win32?
    my $name = $file;
    if(defined $root_rx) {
        $name =~ s!$root_rx!!s;
        $name =~ s!$SIMPLIFY_RX!!os if(defined $SIMPLIFY_RX);
    }
    else {
        if ($^O eq 'MacOS') {
            $name =~ s/^.*://s;
        } else {
            $name =~ s:^.*/::s;
        }
    }
    _simplify($name);
    $name =~ s!/+!::!g; #/
    if ($^O eq 'MacOS') {
        $name =~ s!:+!::!g; # : -> ::
    } else {
        $name =~ s!/+!::!g; # / -> ::
    }
    $name;
}

=head2 C<simplify_name( $str )>

The function B<simplify_name> is equivalent to B<basename>, but also
strips Perl-like extensions (.pm, .pl, .pod) and extensions like
F<.bat>, F<.cmd> on Win32 and OS/2, or F<.com> on VMS, respectively.

=cut

# basic simplification of the POD name:
# basename & strip extension
sub simplify_name {
    my ($str) = @_;
    # remove all path components
    if ($^O eq 'MacOS') {
        $str =~ s/^.*://s;
    } else {
        $str =~ s:^.*/::s;
    }
    _simplify($str);
    $str;
}

# internal sub only
sub _simplify {
    # strip Perl's own extensions
    $_[0] =~ s/\.(pod|pm|plx?)\z//i;
    # strip meaningless extensions on Win32 and OS/2
    $_[0] =~ s/\.(bat|exe|cmd)\z//i if($^O =~ /mswin|os2/i);
    # strip meaningless extensions on VMS
    $_[0] =~ s/\.(com)\z//i if($^O eq 'VMS');
}

# contribution from Tim Jenness <t.jenness@jach.hawaii.edu>

=head2 C<pod_where( { %opts }, $pod )>

Returns the location of a pod document given a search directory
and a module (e.g. C<File::Find>) or script (e.g. C<perldoc>) name.

Options:

=over 4

=item C<-inc =E<gt> 1>

Search @INC for the pod and also the C<scriptdir> defined in the
L<Config|Config> module.

=item C<-dirs =E<gt> [ $dir1, $dir2, ... ]>

Reference to an array of search directories. These are searched in order
before looking in C<@INC> (if B<-inc>). Current directory is used if
none are specified.

=item C<-verbose =E<gt> 1>

List directories as they are searched

=back

Returns the full path of the first occurrence to the file.
Package names (eg 'A::B') are automatically converted to directory
names in the selected directory. (eg on unix 'A::B' is converted to
'A/B'). Additionally, '.pm', '.pl' and '.pod' are appended to the
search automatically if required.

A subdirectory F<pod/> is also checked if it exists in any of the given
search directories. This ensures that e.g. L<perlfunc|perlfunc> is
found.

It is assumed that if a module name is supplied, that that name
matches the file name. Pods are not opened to check for the 'NAME'
entry.

A check is made to make sure that the file that is found does 
contain some pod documentation.

=cut

sub pod_where {

  # default options
  my %options = (
         '-inc' => 0,
         '-verbose' => 0,
         '-dirs' => [ File::Spec->curdir ],
        );

  # Check for an options hash as first argument
  if (defined $_[0] && ref($_[0]) eq 'HASH') {
    my $opt = shift;

    # Merge default options with supplied options
    %options = (%options, %$opt);
  }

  # Check usage
  carp 'Usage: pod_where({options}, $pod)' unless (scalar(@_));

  # Read argument
  my $pod = shift;

  # Split on :: and then join the name together using File::Spec
  my @parts = split (/::/, $pod);

  # Get full directory list
  my @search_dirs = @{ $options{'-dirs'} };

  if ($options{'-inc'}) {

    require Config;

    # Add @INC
    if ($^O eq 'MacOS' && $options{'-inc'}) {
        # tolerate '.', './some_dir' and '(../)+some_dir' on Mac OS
        my @new_INC = @INC;
        for (@new_INC) {
            if ( $_ eq '.' ) {
                $_ = ':';
            } elsif ( $_ =~ s|^((?:\.\./)+)|':' x (length($1)/3)|e ) {
                $_ = ':'. $_;
            } else {
                $_ =~ s|^\./|:|;
            }
        }
        push (@search_dirs, @new_INC);
    } elsif ($options{'-inc'}) {
        push (@search_dirs, @INC);
    }

    # Add location of pod documentation for perl man pages (eg perlfunc)
    # This is a pod directory in the private install tree
    #my $perlpoddir = File::Spec->catdir($Config::Config{'installprivlib'},
    #					'pod');
    #push (@search_dirs, $perlpoddir)
    #  if -d $perlpoddir;

    # Add location of binaries such as pod2text
    push (@search_dirs, $Config::Config{'scriptdir'})
      if -d $Config::Config{'scriptdir'};
  }

  warn "Search path is: ".join(' ', @search_dirs)."\n"
        if $options{'-verbose'};

  # Loop over directories
  Dir: foreach my $dir ( @search_dirs ) {

    # Don't bother if can't find the directory
    if (-d $dir) {
      warn "Looking in directory $dir\n" 
        if $options{'-verbose'};

      # Now concatenate this directory with the pod we are searching for
      my $fullname = File::Spec->catfile($dir, @parts);
      warn "Filename is now $fullname\n"
        if $options{'-verbose'};

      # Loop over possible extensions
      foreach my $ext ('', '.pod', '.pm', '.pl') {
        my $fullext = $fullname . $ext;
        if (-f $fullext && 
         contains_pod($fullext, $options{'-verbose'}) ) {
          warn "FOUND: $fullext\n" if $options{'-verbose'};
          return $fullext;
        }
      }
    } else {
      warn "Directory $dir does not exist\n"
        if $options{'-verbose'};
      next Dir;
    }
    # for some strange reason the path on MacOS/darwin/cygwin is
    # 'pods' not 'pod'
    # this could be the case also for other systems that
    # have a case-tolerant file system, but File::Spec
    # does not recognize 'darwin' yet. And cygwin also has "pods",
    # but is not case tolerant. Oh well...
    if((File::Spec->case_tolerant || $^O =~ /macos|darwin|cygwin/i)
     && -d File::Spec->catdir($dir,'pods')) {
      $dir = File::Spec->catdir($dir,'pods');
      redo Dir;
    }
    if(-d File::Spec->catdir($dir,'pod')) {
      $dir = File::Spec->catdir($dir,'pod');
      redo Dir;
    }
  }
  # No match;
  return undef;
}

=head2 C<contains_pod( $file , $verbose )>

Returns true if the supplied filename (not POD module) contains some pod
information.

=cut

sub contains_pod {
  my $file = shift;
  my $verbose = 0;
  $verbose = shift if @_;

  # check for one line of POD
  unless(open(POD,"<$file")) {
    warn "Error: $file is unreadable: $!\n";
    return undef;
  }
  
  local $/ = undef;
  my $pod = <POD>;
  close(POD) || die "Error closing $file: $!\n";
  unless($pod =~ /^=(head\d|pod|over|item)\b/m) {
    warn "No POD in $file, skipping.\n"
      if($verbose);
    return 0;
  }

  return 1;
}

=head1 AUTHOR

Please report bugs using L<http://rt.cpan.org>.

Marek Rouchal E<lt>marekr@cpan.orgE<gt>,
heavily borrowing code from Nick Ing-Simmons' PodToHtml.

Tim Jenness E<lt>t.jenness@jach.hawaii.eduE<gt> provided
C<pod_where> and C<contains_pod>.

=head1 SEE ALSO

L<Pod::Parser>, L<Pod::Checker>, L<perldoc>

=cut

1;

