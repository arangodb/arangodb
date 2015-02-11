package File::pushd;

$VERSION = '1.00';
@EXPORT  = qw( pushd tempd );
@ISA     = qw( Exporter );

use 5.004;
use strict;
#use warnings;
use Exporter;
use Carp;
use Cwd         qw( cwd abs_path );
use File::Path  qw( rmtree );
use File::Temp  qw();
use File::Spec;

use overload 
    q{""} => sub { File::Spec->canonpath( $_[0]->{_pushd} ) },
    fallback => 1;

#--------------------------------------------------------------------------#
# pushd()
#--------------------------------------------------------------------------#

sub pushd {
    my ($target_dir) = @_;
    
    my $orig = cwd;
    
    my $dest;
    eval { $dest   = $target_dir ? abs_path( $target_dir ) : $orig };
    
    croak "Can't locate directory $target_dir: $@" if $@;
    
    if ($dest ne $orig) { 
        chdir $dest or croak "Can't chdir to $dest\: $!";
    }

    my $self = bless { 
        _pushd => $dest,
        _original => $orig
    }, __PACKAGE__;

    return $self;
}

#--------------------------------------------------------------------------#
# tempd()
#--------------------------------------------------------------------------#

sub tempd {
    my $dir = pushd( File::Temp::tempdir( CLEANUP => 0 ) );
    $dir->{_tempd} = 1;
    return $dir;
}

#--------------------------------------------------------------------------#
# preserve()
#--------------------------------------------------------------------------#

sub preserve {
    my $self = shift;
    return 1 if ! $self->{"_tempd"};
    if ( @_ == 0 ) {
        return $self->{_preserve} = 1;
    }
    else {
        return $self->{_preserve} = $_[0] ? 1 : 0;
    }
}
    
#--------------------------------------------------------------------------#
# DESTROY()
# Revert to original directory as object is destroyed and cleanup
# if necessary
#--------------------------------------------------------------------------#

sub DESTROY {
    my ($self) = @_;
    my $orig = $self->{_original};
    chdir $orig if $orig; # should always be so, but just in case...
    if ( $self->{_tempd} && 
        !$self->{_preserve} ) {
        eval { rmtree( $self->{_pushd} ) };
        carp $@ if $@;
    }
}

1; #this line is important and will help the module return a true value
__END__

=begin wikidoc

= NAME

File::pushd - change directory temporarily for a limited scope

= VERSION

This documentation describes version %%VERSION%%.

= SYNOPSIS

 use File::pushd;

 chdir $ENV{HOME};
 
 # change directory again for a limited scope
 {
     my $dir = pushd( '/tmp' );
     # working directory changed to /tmp
 }
 # working directory has reverted to $ENV{HOME}

 # tempd() is equivalent to pushd( File::Temp::tempdir )
 {
     my $dir = tempd();
 }

 # object stringifies naturally as an absolute path
 {
    my $dir = pushd( '/tmp' );
    my $filename = File::Spec->catfile( $dir, "somefile.txt" );
    # gives /tmp/somefile.txt
 }
    
= DESCRIPTION

File::pushd does a temporary {chdir} that is easily and automatically
reverted, similar to {pushd} in some Unix command shells.  It works by
creating an object that caches the original working directory.  When the object
is destroyed, the destructor calls {chdir} to revert to the original working
directory.  By storing the object in a lexical variable with a limited scope,
this happens automatically at the end of the scope.

This is very handy when working with temporary directories for tasks like
testing; a function is provided to streamline getting a temporary
directory from [File::Temp].

For convenience, the object stringifies as the canonical form of the absolute
pathname of the directory entered.

= USAGE

 use File::pushd;

Using File::pushd automatically imports the {pushd} and {tempd} functions.

== pushd

 {
     my $dir = pushd( $target_directory );
 }

Caches the current working directory, calls {chdir} to change to the target
directory, and returns a File::pushd object.  When the object is
destroyed, the working directory reverts to the original directory.

The provided target directory can be a relative or absolute path. If
called with no arguments, it uses the current directory as its target and
returns to the current directory when the object is destroyed.

== tempd

 {
     my $dir = tempd();
 }

This function is like {pushd} but automatically creates and calls {chdir} to
a temporary directory created by [File::Temp]. Unlike normal [File::Temp]
cleanup which happens at the end of the program, this temporary directory is
removed when the object is destroyed. (But also see {preserve}.)  A warning
will be issued if the directory cannot be removed.

== preserve 

 {
     my $dir = tempd();
     $dir->preserve;      # mark to preserve at end of scope
     $dir->preserve(0);   # mark to delete at end of scope
 }

Controls whether a temporary directory will be cleaned up when the object is
destroyed.  With no arguments, {preserve} sets the directory to be preserved.
With an argument, the directory will be preserved if the argument is true, or
marked for cleanup if the argument is false.  Only {tempd} objects may be
marked for cleanup.  (Target directories to {pushd} are always preserved.)
{preserve} returns true if the directory will be preserved, and false
otherwise.

= SEE ALSO

* [File::chdir]

= BUGS

Please report any bugs or feature using the CPAN Request Tracker.  
Bugs can be submitted through the web interface at 
[http://rt.cpan.org/Dist/Display.html?Queue=File-pushd]

When submitting a bug or request, please include a test-file or a patch to an
existing test-file that illustrates the bug or desired feature.

= AUTHOR

David A. Golden (DAGOLDEN)

= COPYRIGHT AND LICENSE

Copyright (c) 2005, 2006, 2007 by David A. Golden

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at 
[http://www.apache.org/licenses/LICENSE-2.0]

Files produced as output though the use of this software, including
generated copies of boilerplate templates provided with this software,
shall not be considered Derivative Works, but shall be considered the
original work of the Licensor.

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

=end wikidoc

