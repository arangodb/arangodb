=head1 NAME

File::Basename - Parse file paths into directory, filename and suffix.

=head1 SYNOPSIS

    use File::Basename;

    ($name,$path,$suffix) = fileparse($fullname,@suffixlist);
    $name = fileparse($fullname,@suffixlist);

    $basename = basename($fullname,@suffixlist);
    $dirname  = dirname($fullname);


=head1 DESCRIPTION

These routines allow you to parse file paths into their directory, filename
and suffix.

B<NOTE>: C<dirname()> and C<basename()> emulate the behaviours, and
quirks, of the shell and C functions of the same name.  See each
function's documentation for details.  If your concern is just parsing
paths it is safer to use L<File::Spec>'s C<splitpath()> and
C<splitdir()> methods.

It is guaranteed that

    # Where $path_separator is / for Unix, \ for Windows, etc...
    dirname($path) . $path_separator . basename($path);

is equivalent to the original path for all systems but VMS.


=cut


package File::Basename;

# A bit of juggling to insure that C<use re 'taint';> always works, since
# File::Basename is used during the Perl build, when the re extension may
# not be available.
BEGIN {
  unless (eval { require re; })
    { eval ' sub re::import { $^H |= 0x00100000; } ' } # HINT_RE_TAINT
  import re 'taint';
}


use strict;
use 5.006;
use warnings;
our(@ISA, @EXPORT, $VERSION, $Fileparse_fstype, $Fileparse_igncase);
require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(fileparse fileparse_set_fstype basename dirname);
$VERSION = "2.77";

fileparse_set_fstype($^O);


=over 4

=item C<fileparse>
X<fileparse>

    my($filename, $directories, $suffix) = fileparse($path);
    my($filename, $directories, $suffix) = fileparse($path, @suffixes);
    my $filename                         = fileparse($path, @suffixes);

The C<fileparse()> routine divides a file path into its $directories, $filename
and (optionally) the filename $suffix.

$directories contains everything up to and including the last
directory separator in the $path including the volume (if applicable).
The remainder of the $path is the $filename.

     # On Unix returns ("baz", "/foo/bar/", "")
     fileparse("/foo/bar/baz");

     # On Windows returns ("baz", "C:\foo\bar\", "")
     fileparse("C:\foo\bar\baz");

     # On Unix returns ("", "/foo/bar/baz/", "")
     fileparse("/foo/bar/baz/");

If @suffixes are given each element is a pattern (either a string or a
C<qr//>) matched against the end of the $filename.  The matching
portion is removed and becomes the $suffix.

     # On Unix returns ("baz", "/foo/bar/", ".txt")
     fileparse("/foo/bar/baz.txt", qr/\.[^.]*/);

If type is non-Unix (see C<fileparse_set_fstype()>) then the pattern
matching for suffix removal is performed case-insensitively, since
those systems are not case-sensitive when opening existing files.

You are guaranteed that C<$directories . $filename . $suffix> will
denote the same location as the original $path.

=cut


sub fileparse {
  my($fullname,@suffices) = @_;

  unless (defined $fullname) {
      require Carp;
      Carp::croak("fileparse(): need a valid pathname");
  }

  my $orig_type = '';
  my($type,$igncase) = ($Fileparse_fstype, $Fileparse_igncase);

  my($taint) = substr($fullname,0,0);  # Is $fullname tainted?

  if ($type eq "VMS" and $fullname =~ m{/} ) {
    # We're doing Unix emulation
    $orig_type = $type;
    $type = 'Unix';
  }

  my($dirpath, $basename);

  if (grep { $type eq $_ } qw(MSDOS DOS MSWin32 Epoc)) {
    ($dirpath,$basename) = ($fullname =~ /^((?:.*[:\\\/])?)(.*)/s);
    $dirpath .= '.\\' unless $dirpath =~ /[\\\/]\z/;
  }
  elsif ($type eq "OS2") {
    ($dirpath,$basename) = ($fullname =~ m#^((?:.*[:\\/])?)(.*)#s);
    $dirpath = './' unless $dirpath;	# Can't be 0
    $dirpath .= '/' unless $dirpath =~ m#[\\/]\z#;
  }
  elsif ($type eq "MacOS") {
    ($dirpath,$basename) = ($fullname =~ /^(.*:)?(.*)/s);
    $dirpath = ':' unless $dirpath;
  }
  elsif ($type eq "AmigaOS") {
    ($dirpath,$basename) = ($fullname =~ /(.*[:\/])?(.*)/s);
    $dirpath = './' unless $dirpath;
  }
  elsif ($type eq 'VMS' ) {
    ($dirpath,$basename) = ($fullname =~ /^(.*[:>\]])?(.*)/s);
    $dirpath ||= '';  # should always be defined
  }
  else { # Default to Unix semantics.
    ($dirpath,$basename) = ($fullname =~ m{^(.*/)?(.*)}s);
    if ($orig_type eq 'VMS' and $fullname =~ m{^(/[^/]+/000000(/|$))(.*)}) {
      # dev:[000000] is top of VMS tree, similar to Unix '/'
      # so strip it off and treat the rest as "normal"
      my $devspec  = $1;
      my $remainder = $3;
      ($dirpath,$basename) = ($remainder =~ m{^(.*/)?(.*)}s);
      $dirpath ||= '';  # should always be defined
      $dirpath = $devspec.$dirpath;
    }
    $dirpath = './' unless $dirpath;
  }
      

  my $tail   = '';
  my $suffix = '';
  if (@suffices) {
    foreach $suffix (@suffices) {
      my $pat = ($igncase ? '(?i)' : '') . "($suffix)\$";
      if ($basename =~ s/$pat//s) {
        $taint .= substr($suffix,0,0);
        $tail = $1 . $tail;
      }
    }
  }

  # Ensure taint is propgated from the path to its pieces.
  $tail .= $taint;
  wantarray ? ($basename .= $taint, $dirpath .= $taint, $tail)
            : ($basename .= $taint);
}



=item C<basename>
X<basename> X<filename>

    my $filename = basename($path);
    my $filename = basename($path, @suffixes);

This function is provided for compatibility with the Unix shell command
C<basename(1)>.  It does B<NOT> always return the file name portion of a
path as you might expect.  To be safe, if you want the file name portion of
a path use C<fileparse()>.

C<basename()> returns the last level of a filepath even if the last
level is clearly directory.  In effect, it is acting like C<pop()> for
paths.  This differs from C<fileparse()>'s behaviour.

    # Both return "bar"
    basename("/foo/bar");
    basename("/foo/bar/");

@suffixes work as in C<fileparse()> except all regex metacharacters are
quoted.

    # These two function calls are equivalent.
    my $filename = basename("/foo/bar/baz.txt",  ".txt");
    my $filename = fileparse("/foo/bar/baz.txt", qr/\Q.txt\E/);

Also note that in order to be compatible with the shell command,
C<basename()> does not strip off a suffix if it is identical to the
remaining characters in the filename.

=cut


sub basename {
  my($path) = shift;

  # From BSD basename(1)
  # The basename utility deletes any prefix ending with the last slash `/'
  # character present in string (after first stripping trailing slashes)
  _strip_trailing_sep($path);

  my($basename, $dirname, $suffix) = fileparse( $path, map("\Q$_\E",@_) );

  # From BSD basename(1)
  # The suffix is not stripped if it is identical to the remaining 
  # characters in string.
  if( length $suffix and !length $basename ) {
      $basename = $suffix;
  }
  
  # Ensure that basename '/' == '/'
  if( !length $basename ) {
      $basename = $dirname;
  }

  return $basename;
}



=item C<dirname>
X<dirname>

This function is provided for compatibility with the Unix shell
command C<dirname(1)> and has inherited some of its quirks.  In spite of
its name it does B<NOT> always return the directory name as you might
expect.  To be safe, if you want the directory name of a path use
C<fileparse()>.

Only on VMS (where there is no ambiguity between the file and directory
portions of a path) and AmigaOS (possibly due to an implementation quirk in
this module) does C<dirname()> work like C<fileparse($path)>, returning just the
$directories.

    # On VMS and AmigaOS
    my $directories = dirname($path);

When using Unix or MSDOS syntax this emulates the C<dirname(1)> shell function
which is subtly different from how C<fileparse()> works.  It returns all but
the last level of a file path even if the last level is clearly a directory.
In effect, it is not returning the directory portion but simply the path one
level up acting like C<chop()> for file paths.

Also unlike C<fileparse()>, C<dirname()> does not include a trailing slash on
its returned path.

    # returns /foo/bar.  fileparse() would return /foo/bar/
    dirname("/foo/bar/baz");

    # also returns /foo/bar despite the fact that baz is clearly a 
    # directory.  fileparse() would return /foo/bar/baz/
    dirname("/foo/bar/baz/");

    # returns '.'.  fileparse() would return 'foo/'
    dirname("foo/");

Under VMS, if there is no directory information in the $path, then the
current default device and directory is used.

=cut


sub dirname {
    my $path = shift;

    my($type) = $Fileparse_fstype;

    if( $type eq 'VMS' and $path =~ m{/} ) {
        # Parse as Unix
        local($File::Basename::Fileparse_fstype) = '';
        return dirname($path);
    }

    my($basename, $dirname) = fileparse($path);

    if ($type eq 'VMS') { 
        $dirname ||= $ENV{DEFAULT};
    }
    elsif ($type eq 'MacOS') {
	if( !length($basename) && $dirname !~ /^[^:]+:\z/) {
            _strip_trailing_sep($dirname);
	    ($basename,$dirname) = fileparse $dirname;
	}
	$dirname .= ":" unless $dirname =~ /:\z/;
    }
    elsif (grep { $type eq $_ } qw(MSDOS DOS MSWin32 OS2)) { 
        _strip_trailing_sep($dirname);
        unless( length($basename) ) {
	    ($basename,$dirname) = fileparse $dirname;
	    _strip_trailing_sep($dirname);
	}
    }
    elsif ($type eq 'AmigaOS') {
        if ( $dirname =~ /:\z/) { return $dirname }
        chop $dirname;
        $dirname =~ s{[^:/]+\z}{} unless length($basename);
    }
    else {
        _strip_trailing_sep($dirname);
        unless( length($basename) ) {
	    ($basename,$dirname) = fileparse $dirname;
	    _strip_trailing_sep($dirname);
	}
    }

    $dirname;
}


# Strip the trailing path separator.
sub _strip_trailing_sep  {
    my $type = $Fileparse_fstype;

    if ($type eq 'MacOS') {
        $_[0] =~ s/([^:]):\z/$1/s;
    }
    elsif (grep { $type eq $_ } qw(MSDOS DOS MSWin32 OS2)) { 
        $_[0] =~ s/([^:])[\\\/]*\z/$1/;
    }
    else {
        $_[0] =~ s{(.)/*\z}{$1}s;
    }
}


=item C<fileparse_set_fstype>
X<filesystem>

  my $type = fileparse_set_fstype();
  my $previous_type = fileparse_set_fstype($type);

Normally File::Basename will assume a file path type native to your current
operating system (ie. /foo/bar style on Unix, \foo\bar on Windows, etc...).
With this function you can override that assumption.

Valid $types are "MacOS", "VMS", "AmigaOS", "OS2", "RISCOS",
"MSWin32", "DOS" (also "MSDOS" for backwards bug compatibility),
"Epoc" and "Unix" (all case-insensitive).  If an unrecognized $type is
given "Unix" will be assumed.

If you've selected VMS syntax, and the file specification you pass to
one of these routines contains a "/", they assume you are using Unix
emulation and apply the Unix syntax rules instead, for that function
call only.

=back

=cut


BEGIN {

my @Ignore_Case = qw(MacOS VMS AmigaOS OS2 RISCOS MSWin32 MSDOS DOS Epoc);
my @Types = (@Ignore_Case, qw(Unix));

sub fileparse_set_fstype {
    my $old = $Fileparse_fstype;

    if (@_) {
        my $new_type = shift;

        $Fileparse_fstype = 'Unix';  # default
        foreach my $type (@Types) {
            $Fileparse_fstype = $type if $new_type =~ /^$type/i;
        }

        $Fileparse_igncase = 
          (grep $Fileparse_fstype eq $_, @Ignore_Case) ? 1 : 0;
    }

    return $old;
}

}


1;


=head1 SEE ALSO

L<dirname(1)>, L<basename(1)>, L<File::Spec>
