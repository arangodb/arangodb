package File::Spec::VMS;

use strict;
use vars qw(@ISA $VERSION);
require File::Spec::Unix;

$VERSION = '3.2701';

@ISA = qw(File::Spec::Unix);

use File::Basename;
use VMS::Filespec;

=head1 NAME

File::Spec::VMS - methods for VMS file specs

=head1 SYNOPSIS

 require File::Spec::VMS; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over 4

=item canonpath (override)

Removes redundant portions of file specifications according to VMS syntax.

=cut

sub canonpath {
    my($self,$path) = @_;

    return undef unless defined $path;

    if ($path =~ m|/|) { # Fake Unix
      my $pathify = $path =~ m|/\Z(?!\n)|;
      $path = $self->SUPER::canonpath($path);
      if ($pathify) { return vmspath($path); }
      else          { return vmsify($path);  }
    }
    else {
	$path =~ tr/<>/[]/;			# < and >       ==> [ and ]
	$path =~ s/\]\[\./\.\]\[/g;		# ][.		==> .][
	$path =~ s/\[000000\.\]\[/\[/g;		# [000000.][	==> [
	$path =~ s/\[000000\./\[/g;		# [000000.	==> [
	$path =~ s/\.\]\[000000\]/\]/g;		# .][000000]	==> ]
	$path =~ s/\.\]\[/\./g;			# foo.][bar     ==> foo.bar
	1 while ($path =~ s/([\[\.])(-+)\.(-+)([\.\]])/$1$2$3$4/);
						# That loop does the following
						# with any amount of dashes:
						# .-.-.		==> .--.
						# [-.-.		==> [--.
						# .-.-]		==> .--]
						# [-.-]		==> [--]
	1 while ($path =~ s/([\[\.])[^\]\.]+\.-(-+)([\]\.])/$1$2$3/);
						# That loop does the following
						# with any amount (minimum 2)
						# of dashes:
						# .foo.--.	==> .-.
						# .foo.--]	==> .-]
						# [foo.--.	==> [-.
						# [foo.--]	==> [-]
						#
						# And then, the remaining cases
	$path =~ s/\[\.-/[-/;			# [.-		==> [-
	$path =~ s/\.[^\]\.]+\.-\./\./g;	# .foo.-.	==> .
	$path =~ s/\[[^\]\.]+\.-\./\[/g;	# [foo.-.	==> [
	$path =~ s/\.[^\]\.]+\.-\]/\]/g;	# .foo.-]	==> ]
	$path =~ s/\[[^\]\.]+\.-\]/\[000000\]/g;# [foo.-]       ==> [000000]
	$path =~ s/\[\]// unless $path eq '[]';	# []		==>
	return $path;
    }
}

=item catdir (override)

Concatenates a list of file specifications, and returns the result as a
VMS-syntax directory specification.  No check is made for "impossible"
cases (e.g. elements other than the first being absolute filespecs).

=cut

sub catdir {
    my $self = shift;
    my $dir = pop;
    my @dirs = grep {defined() && length()} @_;

    my $rslt;
    if (@dirs) {
	my $path = (@dirs == 1 ? $dirs[0] : $self->catdir(@dirs));
	my ($spath,$sdir) = ($path,$dir);
	$spath =~ s/\.dir\Z(?!\n)//; $sdir =~ s/\.dir\Z(?!\n)//; 
	$sdir = $self->eliminate_macros($sdir) unless $sdir =~ /^[\w\-]+\Z(?!\n)/s;
	$rslt = $self->fixpath($self->eliminate_macros($spath)."/$sdir",1);

	# Special case for VMS absolute directory specs: these will have had device
	# prepended during trip through Unix syntax in eliminate_macros(), since
	# Unix syntax has no way to express "absolute from the top of this device's
	# directory tree".
	if ($spath =~ /^[\[<][^.\-]/s) { $rslt =~ s/^[^\[<]+//s; }
    }
    else {
	if    (not defined $dir or not length $dir) { $rslt = ''; }
	elsif ($dir =~ /^\$\([^\)]+\)\Z(?!\n)/s)          { $rslt = $dir; }
	else                                        { $rslt = vmspath($dir); }
    }
    return $self->canonpath($rslt);
}

=item catfile (override)

Concatenates a list of file specifications, and returns the result as a
VMS-syntax file specification.

=cut

sub catfile {
    my $self = shift;
    my $file = $self->canonpath(pop());
    my @files = grep {defined() && length()} @_;

    my $rslt;
    if (@files) {
	my $path = (@files == 1 ? $files[0] : $self->catdir(@files));
	my $spath = $path;
	$spath =~ s/\.dir\Z(?!\n)//;
	if ($spath =~ /^[^\)\]\/:>]+\)\Z(?!\n)/s && basename($file) eq $file) {
	    $rslt = "$spath$file";
	}
	else {
	    $rslt = $self->eliminate_macros($spath);
	    $rslt = vmsify($rslt.((defined $rslt) && ($rslt ne '') ? '/' : '').unixify($file));
	}
    }
    else { $rslt = (defined($file) && length($file)) ? vmsify($file) : ''; }
    return $self->canonpath($rslt);
}


=item curdir (override)

Returns a string representation of the current directory: '[]'

=cut

sub curdir {
    return '[]';
}

=item devnull (override)

Returns a string representation of the null device: '_NLA0:'

=cut

sub devnull {
    return "_NLA0:";
}

=item rootdir (override)

Returns a string representation of the root directory: 'SYS$DISK:[000000]'

=cut

sub rootdir {
    return 'SYS$DISK:[000000]';
}

=item tmpdir (override)

Returns a string representation of the first writable directory
from the following list or '' if none are writable:

    sys$scratch:
    $ENV{TMPDIR}

Since perl 5.8.0, if running under taint mode, and if $ENV{TMPDIR}
is tainted, it is not used.

=cut

my $tmpdir;
sub tmpdir {
    return $tmpdir if defined $tmpdir;
    $tmpdir = $_[0]->_tmpdir( 'sys$scratch:', $ENV{TMPDIR} );
}

=item updir (override)

Returns a string representation of the parent directory: '[-]'

=cut

sub updir {
    return '[-]';
}

=item case_tolerant (override)

VMS file specification syntax is case-tolerant.

=cut

sub case_tolerant {
    return 1;
}

=item path (override)

Translate logical name DCL$PATH as a searchlist, rather than trying
to C<split> string value of C<$ENV{'PATH'}>.

=cut

sub path {
    my (@dirs,$dir,$i);
    while ($dir = $ENV{'DCL$PATH;' . $i++}) { push(@dirs,$dir); }
    return @dirs;
}

=item file_name_is_absolute (override)

Checks for VMS directory spec as well as Unix separators.

=cut

sub file_name_is_absolute {
    my ($self,$file) = @_;
    # If it's a logical name, expand it.
    $file = $ENV{$file} while $file =~ /^[\w\$\-]+\Z(?!\n)/s && $ENV{$file};
    return scalar($file =~ m!^/!s             ||
		  $file =~ m![<\[][^.\-\]>]!  ||
		  $file =~ /:[^<\[]/);
}

=item splitpath (override)

Splits using VMS syntax.

=cut

sub splitpath {
    my($self,$path) = @_;
    my($dev,$dir,$file) = ('','','');

    vmsify($path) =~ /(.+:)?([\[<].*[\]>])?(.*)/s;
    return ($1 || '',$2 || '',$3);
}

=item splitdir (override)

Split dirspec using VMS syntax.

=cut

sub splitdir {
    my($self,$dirspec) = @_;
    my @dirs = ();
    return @dirs if ( (!defined $dirspec) || ('' eq $dirspec) );
    $dirspec =~ tr/<>/[]/;			# < and >	==> [ and ]
    $dirspec =~ s/\]\[\./\.\]\[/g;		# ][.		==> .][
    $dirspec =~ s/\[000000\.\]\[/\[/g;		# [000000.][	==> [
    $dirspec =~ s/\[000000\./\[/g;		# [000000.	==> [
    $dirspec =~ s/\.\]\[000000\]/\]/g;		# .][000000]	==> ]
    $dirspec =~ s/\.\]\[/\./g;			# foo.][bar	==> foo.bar
    while ($dirspec =~ s/(^|[\[\<\.])\-(\-+)($|[\]\>\.])/$1-.$2$3/g) {}
						# That loop does the following
						# with any amount of dashes:
						# .--.		==> .-.-.
						# [--.		==> [-.-.
						# .--]		==> .-.-]
						# [--]		==> [-.-]
    $dirspec = "[$dirspec]" unless $dirspec =~ /[\[<]/; # make legal
    $dirspec =~ s/^(\[|<)\./$1/;
    @dirs = split /(?<!\^)\./, vmspath($dirspec);
    $dirs[0] =~ s/^[\[<]//s;  $dirs[-1] =~ s/[\]>]\Z(?!\n)//s;
    @dirs;
}


=item catpath (override)

Construct a complete filespec using VMS syntax

=cut

sub catpath {
    my($self,$dev,$dir,$file) = @_;
    
    # We look for a volume in $dev, then in $dir, but not both
    my ($dir_volume, $dir_dir, $dir_file) = $self->splitpath($dir);
    $dev = $dir_volume unless length $dev;
    $dir = length $dir_file ? $self->catfile($dir_dir, $dir_file) : $dir_dir;
    
    if ($dev =~ m|^/+([^/]+)|) { $dev = "$1:"; }
    else { $dev .= ':' unless $dev eq '' or $dev =~ /:\Z(?!\n)/; }
    if (length($dev) or length($dir)) {
      $dir = "[$dir]" unless $dir =~ /[\[<\/]/;
      $dir = vmspath($dir);
    }
    "$dev$dir$file";
}

=item abs2rel (override)

Use VMS syntax when converting filespecs.

=cut

sub abs2rel {
    my $self = shift;
    return vmspath(File::Spec::Unix::abs2rel( $self, @_ ))
        if grep m{/}, @_;

    my($path,$base) = @_;
    $base = $self->_cwd() unless defined $base and length $base;

    for ($path, $base) { $_ = $self->canonpath($_) }

    # Are we even starting $path on the same (node::)device as $base?  Note that
    # logical paths or nodename differences may be on the "same device" 
    # but the comparison that ignores device differences so as to concatenate 
    # [---] up directory specs is not even a good idea in cases where there is 
    # a logical path difference between $path and $base nodename and/or device.
    # Hence we fall back to returning the absolute $path spec
    # if there is a case blind device (or node) difference of any sort
    # and we do not even try to call $parse() or consult %ENV for $trnlnm()
    # (this module needs to run on non VMS platforms after all).
    
    my ($path_volume, $path_directories, $path_file) = $self->splitpath($path);
    my ($base_volume, $base_directories, $base_file) = $self->splitpath($base);
    return $path unless lc($path_volume) eq lc($base_volume);

    for ($path, $base) { $_ = $self->rel2abs($_) }

    # Now, remove all leading components that are the same
    my @pathchunks = $self->splitdir( $path_directories );
    my $pathchunks = @pathchunks;
    unshift(@pathchunks,'000000') unless $pathchunks[0] eq '000000';
    my @basechunks = $self->splitdir( $base_directories );
    my $basechunks = @basechunks;
    unshift(@basechunks,'000000') unless $basechunks[0] eq '000000';

    while ( @pathchunks && 
            @basechunks && 
            lc( $pathchunks[0] ) eq lc( $basechunks[0] ) 
          ) {
        shift @pathchunks ;
        shift @basechunks ;
    }

    # @basechunks now contains the directories to climb out of,
    # @pathchunks now has the directories to descend in to.
    if ((@basechunks > 0) || ($basechunks != $pathchunks)) {
      $path_directories = join '.', ('-' x @basechunks, @pathchunks) ;
    }
    else {
      $path_directories = join '.', @pathchunks;
    }
    $path_directories = '['.$path_directories.']';
    return $self->canonpath( $self->catpath( '', $path_directories, $path_file ) ) ;
}


=item rel2abs (override)

Use VMS syntax when converting filespecs.

=cut

sub rel2abs {
    my $self = shift ;
    my ($path,$base ) = @_;
    return undef unless defined $path;
    if ($path =~ m/\//) {
	$path = ( -d $path || $path =~ m/\/\z/  # educated guessing about
		   ? vmspath($path)             # whether it's a directory
		   : vmsify($path) );
    }
    $base = vmspath($base) if defined $base && $base =~ m/\//;
    # Clean up and split up $path
    if ( ! $self->file_name_is_absolute( $path ) ) {
        # Figure out the effective $base and clean it up.
        if ( !defined( $base ) || $base eq '' ) {
            $base = $self->_cwd;
        }
        elsif ( ! $self->file_name_is_absolute( $base ) ) {
            $base = $self->rel2abs( $base ) ;
        }
        else {
            $base = $self->canonpath( $base ) ;
        }

        # Split up paths
        my ( $path_directories, $path_file ) =
            ($self->splitpath( $path ))[1,2] ;

        my ( $base_volume, $base_directories ) =
            $self->splitpath( $base ) ;

        $path_directories = '' if $path_directories eq '[]' ||
                                  $path_directories eq '<>';
        my $sep = '' ;
        $sep = '.'
            if ( $base_directories =~ m{[^.\]>]\Z(?!\n)} &&
                 $path_directories =~ m{^[^.\[<]}s
            ) ;
        $base_directories = "$base_directories$sep$path_directories";
        $base_directories =~ s{\.?[\]>][\[<]\.?}{.};

        $path = $self->catpath( $base_volume, $base_directories, $path_file );
   }

    return $self->canonpath( $path ) ;
}


# eliminate_macros() and fixpath() are MakeMaker-specific methods
# which are used inside catfile() and catdir().  MakeMaker has its own
# copies as of 6.06_03 which are the canonical ones.  We leave these
# here, in peace, so that File::Spec continues to work with MakeMakers
# prior to 6.06_03.
# 
# Please consider these two methods deprecated.  Do not patch them,
# patch the ones in ExtUtils::MM_VMS instead.
sub eliminate_macros {
    my($self,$path) = @_;
    return '' unless (defined $path) && ($path ne '');
    $self = {} unless ref $self;

    if ($path =~ /\s/) {
      return join ' ', map { $self->eliminate_macros($_) } split /\s+/, $path;
    }

    my($npath) = unixify($path);
    my($complex) = 0;
    my($head,$macro,$tail);

    # perform m##g in scalar context so it acts as an iterator
    while ($npath =~ m#(.*?)\$\((\S+?)\)(.*)#gs) { 
        if ($self->{$2}) {
            ($head,$macro,$tail) = ($1,$2,$3);
            if (ref $self->{$macro}) {
                if (ref $self->{$macro} eq 'ARRAY') {
                    $macro = join ' ', @{$self->{$macro}};
                }
                else {
                    print "Note: can't expand macro \$($macro) containing ",ref($self->{$macro}),
                          "\n\t(using MMK-specific deferred substitutuon; MMS will break)\n";
                    $macro = "\cB$macro\cB";
                    $complex = 1;
                }
            }
            else { ($macro = unixify($self->{$macro})) =~ s#/\Z(?!\n)##; }
            $npath = "$head$macro$tail";
        }
    }
    if ($complex) { $npath =~ s#\cB(.*?)\cB#\${$1}#gs; }
    $npath;
}

# Deprecated.  See the note above for eliminate_macros().
sub fixpath {
    my($self,$path,$force_path) = @_;
    return '' unless $path;
    $self = bless {} unless ref $self;
    my($fixedpath,$prefix,$name);

    if ($path =~ /\s/) {
      return join ' ',
             map { $self->fixpath($_,$force_path) }
	     split /\s+/, $path;
    }

    if ($path =~ m#^\$\([^\)]+\)\Z(?!\n)#s || $path =~ m#[/:>\]]#) { 
        if ($force_path or $path =~ /(?:DIR\)|\])\Z(?!\n)/) {
            $fixedpath = vmspath($self->eliminate_macros($path));
        }
        else {
            $fixedpath = vmsify($self->eliminate_macros($path));
        }
    }
    elsif ((($prefix,$name) = ($path =~ m#^\$\(([^\)]+)\)(.+)#s)) && $self->{$prefix}) {
        my($vmspre) = $self->eliminate_macros("\$($prefix)");
        # is it a dir or just a name?
        $vmspre = ($vmspre =~ m|/| or $prefix =~ /DIR\Z(?!\n)/) ? vmspath($vmspre) : '';
        $fixedpath = ($vmspre ? $vmspre : $self->{$prefix}) . $name;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    else {
        $fixedpath = $path;
        $fixedpath = vmspath($fixedpath) if $force_path;
    }
    # No hints, so we try to guess
    if (!defined($force_path) and $fixedpath !~ /[:>(.\]]/) {
        $fixedpath = vmspath($fixedpath) if -d $fixedpath;
    }

    # Trim off root dirname if it's had other dirs inserted in front of it.
    $fixedpath =~ s/\.000000([\]>])/$1/;
    # Special case for VMS absolute directory specs: these will have had device
    # prepended during trip through Unix syntax in eliminate_macros(), since
    # Unix syntax has no way to express "absolute from the top of this device's
    # directory tree".
    if ($path =~ /^[\[>][^.\-]/) { $fixedpath =~ s/^[^\[<]+//; }
    $fixedpath;
}


=back

=head1 COPYRIGHT

Copyright (c) 2004 by the Perl 5 Porters.  All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=head1 SEE ALSO

See L<File::Spec> and L<File::Spec::Unix>.  This package overrides the
implementation of these methods, not the semantics.

An explanation of VMS file specs can be found at
L<"http://h71000.www7.hp.com/doc/731FINAL/4506/4506pro_014.html#apps_locating_naming_files">.

=cut

1;
