package File::Spec::Mac;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '3.2701';

@ISA = qw(File::Spec::Unix);

my $macfiles;
if ($^O eq 'MacOS') {
	$macfiles = eval { require Mac::Files };
}

sub case_tolerant { 1 }


=head1 NAME

File::Spec::Mac - File::Spec for Mac OS (Classic)

=head1 SYNOPSIS

 require File::Spec::Mac; # Done internally by File::Spec if needed

=head1 DESCRIPTION

Methods for manipulating file specifications.

=head1 METHODS

=over 2

=item canonpath

On Mac OS, there's nothing to be done. Returns what it's given.

=cut

sub canonpath {
    my ($self,$path) = @_;
    return $path;
}

=item catdir()

Concatenate two or more directory names to form a path separated by colons
(":") ending with a directory. Resulting paths are B<relative> by default,
but can be forced to be absolute (but avoid this, see below). Automatically
puts a trailing ":" on the end of the complete path, because that's what's
done in MacPerl's environment and helps to distinguish a file path from a
directory path.

B<IMPORTANT NOTE:> Beginning with version 1.3 of this module, the resulting
path is relative by default and I<not> absolute. This decision was made due
to portability reasons. Since C<File::Spec-E<gt>catdir()> returns relative paths
on all other operating systems, it will now also follow this convention on Mac
OS. Note that this may break some existing scripts.

The intended purpose of this routine is to concatenate I<directory names>.
But because of the nature of Macintosh paths, some additional possibilities
are allowed to make using this routine give reasonable results for some
common situations. In other words, you are also allowed to concatenate
I<paths> instead of directory names (strictly speaking, a string like ":a"
is a path, but not a name, since it contains a punctuation character ":").

So, beside calls like

    catdir("a") = ":a:"
    catdir("a","b") = ":a:b:"
    catdir() = ""                    (special case)

calls like the following

    catdir(":a:") = ":a:"
    catdir(":a","b") = ":a:b:"
    catdir(":a:","b") = ":a:b:"
    catdir(":a:",":b:") = ":a:b:"
    catdir(":") = ":"

are allowed.

Here are the rules that are used in C<catdir()>; note that we try to be as
compatible as possible to Unix:

=over 2

=item 1.

The resulting path is relative by default, i.e. the resulting path will have a
leading colon.

=item 2.

A trailing colon is added automatically to the resulting path, to denote a
directory.

=item 3.

Generally, each argument has one leading ":" and one trailing ":"
removed (if any). They are then joined together by a ":". Special
treatment applies for arguments denoting updir paths like "::lib:",
see (4), or arguments consisting solely of colons ("colon paths"),
see (5).

=item 4.

When an updir path like ":::lib::" is passed as argument, the number
of directories to climb up is handled correctly, not removing leading
or trailing colons when necessary. E.g.

    catdir(":::a","::b","c")    = ":::a::b:c:"
    catdir(":::a::","::b","c")  = ":::a:::b:c:"

=item 5.

Adding a colon ":" or empty string "" to a path at I<any> position
doesn't alter the path, i.e. these arguments are ignored. (When a ""
is passed as the first argument, it has a special meaning, see
(6)). This way, a colon ":" is handled like a "." (curdir) on Unix,
while an empty string "" is generally ignored (see
C<Unix-E<gt>canonpath()> ). Likewise, a "::" is handled like a ".."
(updir), and a ":::" is handled like a "../.." etc.  E.g.

    catdir("a",":",":","b")   = ":a:b:"
    catdir("a",":","::",":b") = ":a::b:"

=item 6.

If the first argument is an empty string "" or is a volume name, i.e. matches
the pattern /^[^:]+:/, the resulting path is B<absolute>.

=item 7.

Passing an empty string "" as the first argument to C<catdir()> is
like passingC<File::Spec-E<gt>rootdir()> as the first argument, i.e.

    catdir("","a","b")          is the same as

    catdir(rootdir(),"a","b").

This is true on Unix, where C<catdir("","a","b")> yields "/a/b" and
C<rootdir()> is "/". Note that C<rootdir()> on Mac OS is the startup
volume, which is the closest in concept to Unix' "/". This should help
to run existing scripts originally written for Unix.

=item 8.

For absolute paths, some cleanup is done, to ensure that the volume
name isn't immediately followed by updirs. This is invalid, because
this would go beyond "root". Generally, these cases are handled like
their Unix counterparts:

 Unix:
    Unix->catdir("","")                 =  "/"
    Unix->catdir("",".")                =  "/"
    Unix->catdir("","..")               =  "/"              # can't go beyond root
    Unix->catdir("",".","..","..","a")  =  "/a"
 Mac:
    Mac->catdir("","")                  =  rootdir()         # (e.g. "HD:")
    Mac->catdir("",":")                 =  rootdir()
    Mac->catdir("","::")                =  rootdir()         # can't go beyond root
    Mac->catdir("",":","::","::","a")   =  rootdir() . "a:"  # (e.g. "HD:a:")

However, this approach is limited to the first arguments following
"root" (again, see C<Unix-E<gt>canonpath()> ). If there are more
arguments that move up the directory tree, an invalid path going
beyond root can be created.

=back

As you've seen, you can force C<catdir()> to create an absolute path
by passing either an empty string or a path that begins with a volume
name as the first argument. However, you are strongly encouraged not
to do so, since this is done only for backward compatibility. Newer
versions of File::Spec come with a method called C<catpath()> (see
below), that is designed to offer a portable solution for the creation
of absolute paths.  It takes volume, directory and file portions and
returns an entire path. While C<catdir()> is still suitable for the
concatenation of I<directory names>, you are encouraged to use
C<catpath()> to concatenate I<volume names> and I<directory
paths>. E.g.

    $dir      = File::Spec->catdir("tmp","sources");
    $abs_path = File::Spec->catpath("MacintoshHD:", $dir,"");

yields

    "MacintoshHD:tmp:sources:" .

=cut

sub catdir {
	my $self = shift;
	return '' unless @_;
	my @args = @_;
	my $first_arg;
	my $relative;

	# take care of the first argument

	if ($args[0] eq '')  { # absolute path, rootdir
		shift @args;
		$relative = 0;
		$first_arg = $self->rootdir;

	} elsif ($args[0] =~ /^[^:]+:/) { # absolute path, volume name
		$relative = 0;
		$first_arg = shift @args;
		# add a trailing ':' if need be (may be it's a path like HD:dir)
		$first_arg = "$first_arg:" unless ($first_arg =~ /:\Z(?!\n)/);

	} else { # relative path
		$relative = 1;
		if ( $args[0] =~ /^::+\Z(?!\n)/ ) {
			# updir colon path ('::', ':::' etc.), don't shift
			$first_arg = ':';
		} elsif ($args[0] eq ':') {
			$first_arg = shift @args;
		} else {
			# add a trailing ':' if need be
			$first_arg = shift @args;
			$first_arg = "$first_arg:" unless ($first_arg =~ /:\Z(?!\n)/);
		}
	}

	# For all other arguments,
	# (a) ignore arguments that equal ':' or '',
	# (b) handle updir paths specially:
	#     '::' 			-> concatenate '::'
	#     '::' . '::' 	-> concatenate ':::' etc.
	# (c) add a trailing ':' if need be

	my $result = $first_arg;
	while (@args) {
		my $arg = shift @args;
		unless (($arg eq '') || ($arg eq ':')) {
			if ($arg =~ /^::+\Z(?!\n)/ ) { # updir colon path like ':::'
				my $updir_count = length($arg) - 1;
				while ((@args) && ($args[0] =~ /^::+\Z(?!\n)/) ) { # while updir colon path
					$arg = shift @args;
					$updir_count += (length($arg) - 1);
				}
				$arg = (':' x $updir_count);
			} else {
				$arg =~ s/^://s; # remove a leading ':' if any
				$arg = "$arg:" unless ($arg =~ /:\Z(?!\n)/); # ensure trailing ':'
			}
			$result .= $arg;
		}#unless
	}

	if ( ($relative) && ($result !~ /^:/) ) {
		# add a leading colon if need be
		$result = ":$result";
	}

	unless ($relative) {
		# remove updirs immediately following the volume name
		$result =~ s/([^:]+:)(:*)(.*)\Z(?!\n)/$1$3/;
	}

	return $result;
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename. Resulting paths are B<relative>
by default, but can be forced to be absolute (but avoid this).

B<IMPORTANT NOTE:> Beginning with version 1.3 of this module, the
resulting path is relative by default and I<not> absolute. This
decision was made due to portability reasons. Since
C<File::Spec-E<gt>catfile()> returns relative paths on all other
operating systems, it will now also follow this convention on Mac OS.
Note that this may break some existing scripts.

The last argument is always considered to be the file portion. Since
C<catfile()> uses C<catdir()> (see above) for the concatenation of the
directory portions (if any), the following with regard to relative and
absolute paths is true:

    catfile("")     = ""
    catfile("file") = "file"

but

    catfile("","")        = rootdir()         # (e.g. "HD:")
    catfile("","file")    = rootdir() . file  # (e.g. "HD:file")
    catfile("HD:","file") = "HD:file"

This means that C<catdir()> is called only when there are two or more
arguments, as one might expect.

Note that the leading ":" is removed from the filename, so that

    catfile("a","b","file")  = ":a:b:file"    and

    catfile("a","b",":file") = ":a:b:file"

give the same answer.

To concatenate I<volume names>, I<directory paths> and I<filenames>,
you are encouraged to use C<catpath()> (see below).

=cut

sub catfile {
    my $self = shift;
    return '' unless @_;
    my $file = pop @_;
    return $file unless @_;
    my $dir = $self->catdir(@_);
    $file =~ s/^://s;
    return $dir.$file;
}

=item curdir

Returns a string representing the current directory. On Mac OS, this is ":".

=cut

sub curdir {
    return ":";
}

=item devnull

Returns a string representing the null device. On Mac OS, this is "Dev:Null".

=cut

sub devnull {
    return "Dev:Null";
}

=item rootdir

Returns a string representing the root directory.  Under MacPerl,
returns the name of the startup volume, since that's the closest in
concept, although other volumes aren't rooted there. The name has a
trailing ":", because that's the correct specification for a volume
name on Mac OS.

If Mac::Files could not be loaded, the empty string is returned.

=cut

sub rootdir {
#
#  There's no real root directory on Mac OS. The name of the startup
#  volume is returned, since that's the closest in concept.
#
    return '' unless $macfiles;
    my $system = Mac::Files::FindFolder(&Mac::Files::kOnSystemDisk,
	&Mac::Files::kSystemFolderType);
    $system =~ s/:.*\Z(?!\n)/:/s;
    return $system;
}

=item tmpdir

Returns the contents of $ENV{TMPDIR}, if that directory exits or the
current working directory otherwise. Under MacPerl, $ENV{TMPDIR} will
contain a path like "MacintoshHD:Temporary Items:", which is a hidden
directory on your startup volume.

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    $tmpdir = $_[0]->_tmpdir( $ENV{TMPDIR} );
}

=item updir

Returns a string representing the parent directory. On Mac OS, this is "::".

=cut

sub updir {
    return "::";
}

=item file_name_is_absolute

Takes as argument a path and returns true, if it is an absolute path.
If the path has a leading ":", it's a relative path. Otherwise, it's an
absolute path, unless the path doesn't contain any colons, i.e. it's a name
like "a". In this particular case, the path is considered to be relative
(i.e. it is considered to be a filename). Use ":" in the appropriate place
in the path if you want to distinguish unambiguously. As a special case,
the filename '' is always considered to be absolute. Note that with version
1.2 of File::Spec::Mac, this does no longer consult the local filesystem.

E.g.

    File::Spec->file_name_is_absolute("a");             # false (relative)
    File::Spec->file_name_is_absolute(":a:b:");         # false (relative)
    File::Spec->file_name_is_absolute("MacintoshHD:");  # true (absolute)
    File::Spec->file_name_is_absolute("");              # true (absolute)


=cut

sub file_name_is_absolute {
    my ($self,$file) = @_;
    if ($file =~ /:/) {
	return (! ($file =~ m/^:/s) );
    } elsif ( $file eq '' ) {
        return 1 ;
    } else {
	return 0; # i.e. a file like "a"
    }
}

=item path

Returns the null list for the MacPerl application, since the concept is
usually meaningless under Mac OS. But if you're using the MacPerl tool under
MPW, it gives back $ENV{Commands} suitably split, as is done in
:lib:ExtUtils:MM_Mac.pm.

=cut

sub path {
#
#  The concept is meaningless under the MacPerl application.
#  Under MPW, it has a meaning.
#
    return unless exists $ENV{Commands};
    return split(/,/, $ENV{Commands});
}

=item splitpath

    ($volume,$directories,$file) = File::Spec->splitpath( $path );
    ($volume,$directories,$file) = File::Spec->splitpath( $path, $no_file );

Splits a path into volume, directory, and filename portions.

On Mac OS, assumes that the last part of the path is a filename unless
$no_file is true or a trailing separator ":" is present.

The volume portion is always returned with a trailing ":". The directory portion
is always returned with a leading (to denote a relative path) and a trailing ":"
(to denote a directory). The file portion is always returned I<without> a leading ":".
Empty portions are returned as empty string ''.

The results can be passed to C<catpath()> to get back a path equivalent to
(usually identical to) the original path.


=cut

sub splitpath {
    my ($self,$path, $nofile) = @_;
    my ($volume,$directory,$file);

    if ( $nofile ) {
        ( $volume, $directory ) = $path =~ m|^((?:[^:]+:)?)(.*)|s;
    }
    else {
        $path =~
            m|^( (?: [^:]+: )? )
               ( (?: .*: )? )
               ( .* )
             |xs;
        $volume    = $1;
        $directory = $2;
        $file      = $3;
    }

    $volume = '' unless defined($volume);
	$directory = ":$directory" if ( $volume && $directory ); # take care of "HD::dir"
    if ($directory) {
        # Make sure non-empty directories begin and end in ':'
        $directory .= ':' unless (substr($directory,-1) eq ':');
        $directory = ":$directory" unless (substr($directory,0,1) eq ':');
    } else {
	$directory = '';
    }
    $file = '' unless defined($file);

    return ($volume,$directory,$file);
}


=item splitdir

The opposite of C<catdir()>.

    @dirs = File::Spec->splitdir( $directories );

$directories should be only the directory portion of the path on systems
that have the concept of a volume or that have path syntax that differentiates
files from directories. Consider using C<splitpath()> otherwise.

Unlike just splitting the directories on the separator, empty directory names
(C<"">) can be returned. Since C<catdir()> on Mac OS always appends a trailing
colon to distinguish a directory path from a file path, a single trailing colon
will be ignored, i.e. there's no empty directory name after it.

Hence, on Mac OS, both

    File::Spec->splitdir( ":a:b::c:" );    and
    File::Spec->splitdir( ":a:b::c" );

yield:

    ( "a", "b", "::", "c")

while

    File::Spec->splitdir( ":a:b::c::" );

yields:

    ( "a", "b", "::", "c", "::")


=cut

sub splitdir {
	my ($self, $path) = @_;
	my @result = ();
	my ($head, $sep, $tail, $volume, $directories);

	return @result if ( (!defined($path)) || ($path eq '') );
	return (':') if ($path eq ':');

	( $volume, $sep, $directories ) = $path =~ m|^((?:[^:]+:)?)(:*)(.*)|s;

	# deprecated, but handle it correctly
	if ($volume) {
		push (@result, $volume);
		$sep .= ':';
	}

	while ($sep || $directories) {
		if (length($sep) > 1) {
			my $updir_count = length($sep) - 1;
			for (my $i=0; $i<$updir_count; $i++) {
				# push '::' updir_count times;
				# simulate Unix '..' updirs
				push (@result, '::');
			}
		}
		$sep = '';
		if ($directories) {
			( $head, $sep, $tail ) = $directories =~ m|^((?:[^:]+)?)(:*)(.*)|s;
			push (@result, $head);
			$directories = $tail;
		}
	}
	return @result;
}


=item catpath

    $path = File::Spec->catpath($volume,$directory,$file);

Takes volume, directory and file portions and returns an entire path. On Mac OS,
$volume, $directory and $file are concatenated.  A ':' is inserted if need be. You
may pass an empty string for each portion. If all portions are empty, the empty
string is returned. If $volume is empty, the result will be a relative path,
beginning with a ':'. If $volume and $directory are empty, a leading ":" (if any)
is removed form $file and the remainder is returned. If $file is empty, the
resulting path will have a trailing ':'.


=cut

sub catpath {
    my ($self,$volume,$directory,$file) = @_;

    if ( (! $volume) && (! $directory) ) {
	$file =~ s/^:// if $file;
	return $file ;
    }

    # We look for a volume in $volume, then in $directory, but not both

    my ($dir_volume, $dir_dirs) = $self->splitpath($directory, 1);

    $volume = $dir_volume unless length $volume;
    my $path = $volume; # may be ''
    $path .= ':' unless (substr($path, -1) eq ':'); # ensure trailing ':'

    if ($directory) {
	$directory = $dir_dirs if $volume;
	$directory =~ s/^://; # remove leading ':' if any
	$path .= $directory;
	$path .= ':' unless (substr($path, -1) eq ':'); # ensure trailing ':'
    }

    if ($file) {
	$file =~ s/^://; # remove leading ':' if any
	$path .= $file;
    }

    return $path;
}

=item abs2rel

Takes a destination path and an optional base path and returns a relative path
from the base path to the destination path:

    $rel_path = File::Spec->abs2rel( $path ) ;
    $rel_path = File::Spec->abs2rel( $path, $base ) ;

Note that both paths are assumed to have a notation that distinguishes a
directory path (with trailing ':') from a file path (without trailing ':').

If $base is not present or '', then the current working directory is used.
If $base is relative, then it is converted to absolute form using C<rel2abs()>.
This means that it is taken to be relative to the current working directory.

If $path and $base appear to be on two different volumes, we will not
attempt to resolve the two paths, and we will instead simply return
$path.  Note that previous versions of this module ignored the volume
of $base, which resulted in garbage results part of the time.

If $base doesn't have a trailing colon, the last element of $base is
assumed to be a filename.  This filename is ignored.  Otherwise all path
components are assumed to be directories.

If $path is relative, it is converted to absolute form using C<rel2abs()>.
This means that it is taken to be relative to the current working directory.

Based on code written by Shigio Yamaguchi.


=cut

# maybe this should be done in canonpath() ?
sub _resolve_updirs {
	my $path = shift @_;
	my $proceed;

	# resolve any updirs, e.g. "HD:tmp::file" -> "HD:file"
	do {
		$proceed = ($path =~ s/^(.*):[^:]+::(.*?)\z/$1:$2/);
	} while ($proceed);

	return $path;
}


sub abs2rel {
    my($self,$path,$base) = @_;

    # Clean up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        $path = $self->rel2abs( $path ) ;
    }

    # Figure out the effective $base and clean it up.
    if ( !defined( $base ) || $base eq '' ) {
	$base = $self->_cwd();
    }
    elsif ( ! $self->file_name_is_absolute( $base ) ) {
        $base = $self->rel2abs( $base ) ;
	$base = _resolve_updirs( $base ); # resolve updirs in $base
    }
    else {
	$base = _resolve_updirs( $base );
    }

    # Split up paths - ignore $base's file
    my ( $path_vol, $path_dirs, $path_file ) =  $self->splitpath( $path );
    my ( $base_vol, $base_dirs )             =  $self->splitpath( $base );

    return $path unless lc( $path_vol ) eq lc( $base_vol );

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path_dirs );
    my @basechunks = $self->splitdir( $base_dirs );
	
    while ( @pathchunks &&
	    @basechunks &&
	    lc( $pathchunks[0] ) eq lc( $basechunks[0] ) ) {
        shift @pathchunks ;
        shift @basechunks ;
    }

    # @pathchunks now has the directories to descend in to.
    # ensure relative path, even if @pathchunks is empty
    $path_dirs = $self->catdir( ':', @pathchunks );

    # @basechunks now contains the number of directories to climb out of.
    $base_dirs = (':' x @basechunks) . ':' ;

    return $self->catpath( '', $self->catdir( $base_dirs, $path_dirs ), $path_file ) ;
}

=item rel2abs

Converts a relative path to an absolute path:

    $abs_path = File::Spec->rel2abs( $path ) ;
    $abs_path = File::Spec->rel2abs( $path, $base ) ;

Note that both paths are assumed to have a notation that distinguishes a
directory path (with trailing ':') from a file path (without trailing ':').

If $base is not present or '', then $base is set to the current working
directory. If $base is relative, then it is converted to absolute form
using C<rel2abs()>. This means that it is taken to be relative to the
current working directory.

If $base doesn't have a trailing colon, the last element of $base is
assumed to be a filename.  This filename is ignored.  Otherwise all path
components are assumed to be directories.

If $path is already absolute, it is returned and $base is ignored.

Based on code written by Shigio Yamaguchi.

=cut

sub rel2abs {
    my ($self,$path,$base) = @_;

    if ( ! $self->file_name_is_absolute($path) ) {
        # Figure out the effective $base and clean it up.
        if ( !defined( $base ) || $base eq '' ) {
	    $base = $self->_cwd();
        }
        elsif ( ! $self->file_name_is_absolute($base) ) {
            $base = $self->rel2abs($base) ;
        }

	# Split up paths

	# igonore $path's volume
        my ( $path_dirs, $path_file ) = ($self->splitpath($path))[1,2] ;

        # ignore $base's file part
	my ( $base_vol, $base_dirs ) = $self->splitpath($base) ;

	# Glom them together
	$path_dirs = ':' if ($path_dirs eq '');
	$base_dirs =~ s/:$//; # remove trailing ':', if any
	$base_dirs = $base_dirs . $path_dirs;

        $path = $self->catpath( $base_vol, $base_dirs, $path_file );
    }
    return $path;
}


=back

=head1 AUTHORS

See the authors list in I<File::Spec>. Mac OS support by Paul Schinder
<schinder@pobox.com> and Thomas Wegner <wegner_thomas@yahoo.com>.

=head1 COPYRIGHT

Copyright (c) 2004 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

See L<File::Spec> and L<File::Spec::Unix>.  This package overrides the
implementation of these methods, not the semantics.

=cut

1;
