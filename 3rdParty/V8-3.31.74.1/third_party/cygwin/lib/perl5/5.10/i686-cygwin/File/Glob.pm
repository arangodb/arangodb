package File::Glob;

use strict;
our($VERSION, @ISA, @EXPORT_OK, @EXPORT_FAIL, %EXPORT_TAGS,
    $AUTOLOAD, $DEFAULT_FLAGS);

use XSLoader ();

@ISA = qw(Exporter);

# NOTE: The glob() export is only here for compatibility with 5.6.0.
# csh_glob() should not be used directly, unless you know what you're doing.

@EXPORT_OK   = qw(
    csh_glob
    bsd_glob
    glob
    GLOB_ABEND
    GLOB_ALPHASORT
    GLOB_ALTDIRFUNC
    GLOB_BRACE
    GLOB_CSH
    GLOB_ERR
    GLOB_ERROR
    GLOB_LIMIT
    GLOB_MARK
    GLOB_NOCASE
    GLOB_NOCHECK
    GLOB_NOMAGIC
    GLOB_NOSORT
    GLOB_NOSPACE
    GLOB_QUOTE
    GLOB_TILDE
);

%EXPORT_TAGS = (
    'glob' => [ qw(
        GLOB_ABEND
	GLOB_ALPHASORT
        GLOB_ALTDIRFUNC
        GLOB_BRACE
        GLOB_CSH
        GLOB_ERR
        GLOB_ERROR
        GLOB_LIMIT
        GLOB_MARK
        GLOB_NOCASE
        GLOB_NOCHECK
        GLOB_NOMAGIC
        GLOB_NOSORT
        GLOB_NOSPACE
        GLOB_QUOTE
        GLOB_TILDE
        glob
        bsd_glob
    ) ],
);

$VERSION = '1.06';

sub import {
    require Exporter;
    my $i = 1;
    while ($i < @_) {
	if ($_[$i] =~ /^:(case|nocase|globally)$/) {
	    splice(@_, $i, 1);
	    $DEFAULT_FLAGS &= ~GLOB_NOCASE() if $1 eq 'case';
	    $DEFAULT_FLAGS |= GLOB_NOCASE() if $1 eq 'nocase';
	    if ($1 eq 'globally') {
		local $^W;
		*CORE::GLOBAL::glob = \&File::Glob::csh_glob;
	    }
	    next;
	}
	++$i;
    }
    goto &Exporter::import;
}

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my ($error, $val) = constant($constname);
    if ($error) {
	require Carp;
	Carp::croak($error);
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

XSLoader::load 'File::Glob', $VERSION;

# Preloaded methods go here.

sub GLOB_ERROR {
    return (constant('GLOB_ERROR'))[1];
}

sub GLOB_CSH () {
    GLOB_BRACE()
	| GLOB_NOMAGIC()
	| GLOB_QUOTE()
	| GLOB_TILDE()
	| GLOB_ALPHASORT()
}

$DEFAULT_FLAGS = GLOB_CSH();
if ($^O =~ /^(?:MSWin32|VMS|os2|dos|riscos|MacOS)$/) {
    $DEFAULT_FLAGS |= GLOB_NOCASE();
}

# Autoload methods go after =cut, and are processed by the autosplit program.

sub bsd_glob {
    my ($pat,$flags) = @_;
    $flags = $DEFAULT_FLAGS if @_ < 2;
    return doglob($pat,$flags);
}

# File::Glob::glob() is deprecated because its prototype is different from
# CORE::glob() (use bsd_glob() instead)
sub glob {
    splice @_, 1; # don't pass PL_glob_index as flags!
    goto &bsd_glob;
}

## borrowed heavily from gsar's File::DosGlob
my %iter;
my %entries;

sub csh_glob {
    my $pat = shift;
    my $cxix = shift;
    my @pat;

    # glob without args defaults to $_
    $pat = $_ unless defined $pat;

    # extract patterns
    $pat =~ s/^\s+//;	# Protect against empty elements in
    $pat =~ s/\s+$//;	# things like < *.c> and <*.c >.
			# These alone shouldn't trigger ParseWords.
    if ($pat =~ /\s/) {
        # XXX this is needed for compatibility with the csh
	# implementation in Perl.  Need to support a flag
	# to disable this behavior.
	require Text::ParseWords;
	@pat = Text::ParseWords::parse_line('\s+',0,$pat);
    }

    # assume global context if not provided one
    $cxix = '_G_' unless defined $cxix;
    $iter{$cxix} = 0 unless exists $iter{$cxix};

    # if we're just beginning, do it all first
    if ($iter{$cxix} == 0) {
	if (@pat) {
	    $entries{$cxix} = [ map { doglob($_, $DEFAULT_FLAGS) } @pat ];
	}
	else {
	    $entries{$cxix} = [ doglob($pat, $DEFAULT_FLAGS) ];
	}
    }

    # chuck it all out, quick or slow
    if (wantarray) {
        delete $iter{$cxix};
        return @{delete $entries{$cxix}};
    }
    else {
        if ($iter{$cxix} = scalar @{$entries{$cxix}}) {
            return shift @{$entries{$cxix}};
        }
        else {
            # return undef for EOL
            delete $iter{$cxix};
            delete $entries{$cxix};
            return undef;
        }
    }
}

1;
__END__

=head1 NAME

File::Glob - Perl extension for BSD glob routine

=head1 SYNOPSIS

  use File::Glob ':glob';

  @list = bsd_glob('*.[ch]');
  $homedir = bsd_glob('~gnat', GLOB_TILDE | GLOB_ERR);

  if (GLOB_ERROR) {
    # an error occurred reading $homedir
  }

  ## override the core glob (CORE::glob() does this automatically
  ## by default anyway, since v5.6.0)
  use File::Glob ':globally';
  my @sources = <*.{c,h,y}>;

  ## override the core glob, forcing case sensitivity
  use File::Glob qw(:globally :case);
  my @sources = <*.{c,h,y}>;

  ## override the core glob forcing case insensitivity
  use File::Glob qw(:globally :nocase);
  my @sources = <*.{c,h,y}>;

  ## glob on all files in home directory
  use File::Glob ':globally';
  my @sources = <~gnat/*>;

=head1 DESCRIPTION

The glob angle-bracket operator C<< <> >> is a pathname generator that
implements the rules for file name pattern matching used by Unix-like shells
such as the Bourne shell or C shell.

File::Glob::bsd_glob() implements the FreeBSD glob(3) routine, which is
a superset of the POSIX glob() (described in IEEE Std 1003.2 "POSIX.2").
bsd_glob() takes a mandatory C<pattern> argument, and an optional
C<flags> argument, and returns a list of filenames matching the
pattern, with interpretation of the pattern modified by the C<flags>
variable.

Since v5.6.0, Perl's CORE::glob() is implemented in terms of bsd_glob().
Note that they don't share the same prototype--CORE::glob() only accepts
a single argument.  Due to historical reasons, CORE::glob() will also
split its argument on whitespace, treating it as multiple patterns,
whereas bsd_glob() considers them as one pattern.

=head2 META CHARACTERS

  \       Quote the next metacharacter
  []      Character class
  {}      Multiple pattern
  *       Match any string of characters
  ?       Match any single character
  ~       User name home directory

The metanotation C<a{b,c,d}e> is a shorthand for C<abe ace ade>.  Left to
right order is preserved, with results of matches being sorted separately
at a low level to preserve this order. As a special case C<{>, C<}>, and
C<{}> are passed undisturbed.

=head2 POSIX FLAGS

The POSIX defined flags for bsd_glob() are:

=over 4

=item C<GLOB_ERR>

Force bsd_glob() to return an error when it encounters a directory it
cannot open or read.  Ordinarily bsd_glob() continues to find matches.

=item C<GLOB_LIMIT>

Make bsd_glob() return an error (GLOB_NOSPACE) when the pattern expands
to a size bigger than the system constant C<ARG_MAX> (usually found in
limits.h).  If your system does not define this constant, bsd_glob() uses
C<sysconf(_SC_ARG_MAX)> or C<_POSIX_ARG_MAX> where available (in that
order).  You can inspect these values using the standard C<POSIX>
extension.

=item C<GLOB_MARK>

Each pathname that is a directory that matches the pattern has a slash
appended.

=item C<GLOB_NOCASE>

By default, file names are assumed to be case sensitive; this flag
makes bsd_glob() treat case differences as not significant.

=item C<GLOB_NOCHECK>

If the pattern does not match any pathname, then bsd_glob() returns a list
consisting of only the pattern.  If C<GLOB_QUOTE> is set, its effect
is present in the pattern returned.

=item C<GLOB_NOSORT>

By default, the pathnames are sorted in ascending ASCII order; this
flag prevents that sorting (speeding up bsd_glob()).

=back

The FreeBSD extensions to the POSIX standard are the following flags:

=over 4

=item C<GLOB_BRACE>

Pre-process the string to expand C<{pat,pat,...}> strings like csh(1).
The pattern '{}' is left unexpanded for historical reasons (and csh(1)
does the same thing to ease typing of find(1) patterns).

=item C<GLOB_NOMAGIC>

Same as C<GLOB_NOCHECK> but it only returns the pattern if it does not
contain any of the special characters "*", "?" or "[".  C<NOMAGIC> is
provided to simplify implementing the historic csh(1) globbing
behaviour and should probably not be used anywhere else.

=item C<GLOB_QUOTE>

Use the backslash ('\') character for quoting: every occurrence of a
backslash followed by a character in the pattern is replaced by that
character, avoiding any special interpretation of the character.
(But see below for exceptions on DOSISH systems).

=item C<GLOB_TILDE>

Expand patterns that start with '~' to user name home directories.

=item C<GLOB_CSH>

For convenience, C<GLOB_CSH> is a synonym for
C<GLOB_BRACE | GLOB_NOMAGIC | GLOB_QUOTE | GLOB_TILDE | GLOB_ALPHASORT>.

=back

The POSIX provided C<GLOB_APPEND>, C<GLOB_DOOFFS>, and the FreeBSD
extensions C<GLOB_ALTDIRFUNC>, and C<GLOB_MAGCHAR> flags have not been
implemented in the Perl version because they involve more complex
interaction with the underlying C structures.

The following flag has been added in the Perl implementation for
csh compatibility:

=over 4

=item C<GLOB_ALPHASORT>

If C<GLOB_NOSORT> is not in effect, sort filenames is alphabetical
order (case does not matter) rather than in ASCII order.

=back

=head1 DIAGNOSTICS

bsd_glob() returns a list of matching paths, possibly zero length.  If an
error occurred, &File::Glob::GLOB_ERROR will be non-zero and C<$!> will be
set.  &File::Glob::GLOB_ERROR is guaranteed to be zero if no error occurred,
or one of the following values otherwise:

=over 4

=item C<GLOB_NOSPACE>

An attempt to allocate memory failed.

=item C<GLOB_ABEND>

The glob was stopped because an error was encountered.

=back

In the case where bsd_glob() has found some matching paths, but is
interrupted by an error, it will return a list of filenames B<and>
set &File::Glob::ERROR.

Note that bsd_glob() deviates from POSIX and FreeBSD glob(3) behaviour
by not considering C<ENOENT> and C<ENOTDIR> as errors - bsd_glob() will
continue processing despite those errors, unless the C<GLOB_ERR> flag is
set.

Be aware that all filenames returned from File::Glob are tainted.

=head1 NOTES

=over 4

=item *

If you want to use multiple patterns, e.g. C<bsd_glob("a* b*")>, you should
probably throw them in a set as in C<bsd_glob("{a*,b*}")>.  This is because
the argument to bsd_glob() isn't subjected to parsing by the C shell.
Remember that you can use a backslash to escape things.

=item *

On DOSISH systems, backslash is a valid directory separator character.
In this case, use of backslash as a quoting character (via GLOB_QUOTE)
interferes with the use of backslash as a directory separator. The
best (simplest, most portable) solution is to use forward slashes for
directory separators, and backslashes for quoting. However, this does
not match "normal practice" on these systems. As a concession to user
expectation, therefore, backslashes (under GLOB_QUOTE) only quote the
glob metacharacters '[', ']', '{', '}', '-', '~', and backslash itself.
All other backslashes are passed through unchanged.

=item *

Win32 users should use the real slash.  If you really want to use
backslashes, consider using Sarathy's File::DosGlob, which comes with
the standard Perl distribution.

=item *

Mac OS (Classic) users should note a few differences. Since
Mac OS is not Unix, when the glob code encounters a tilde glob (e.g.
~user) and the C<GLOB_TILDE> flag is used, it simply returns that
pattern without doing any expansion.

Glob on Mac OS is case-insensitive by default (if you don't use any
flags). If you specify any flags at all and still want glob
to be case-insensitive, you must include C<GLOB_NOCASE> in the flags.

The path separator is ':' (aka colon), not '/' (aka slash). Mac OS users
should be careful about specifying relative pathnames. While a full path
always begins with a volume name, a relative pathname should always
begin with a ':'.  If specifying a volume name only, a trailing ':' is
required.

The specification of pathnames in glob patterns adheres to the usual Mac
OS conventions: The path separator is a colon ':', not a slash '/'. A
full path always begins with a volume name. A relative pathname on Mac
OS must always begin with a ':', except when specifying a file or
directory name in the current working directory, where the leading colon
is optional. If specifying a volume name only, a trailing ':' is
required. Due to these rules, a glob like E<lt>*:E<gt> will find all
mounted volumes, while a glob like E<lt>*E<gt> or E<lt>:*E<gt> will find
all files and directories in the current directory.

Note that updirs in the glob pattern are resolved before the matching begins,
i.e. a pattern like "*HD:t?p::a*" will be matched as "*HD:a*". Note also,
that a single trailing ':' in the pattern is ignored (unless it's a volume
name pattern like "*HD:"), i.e. a glob like E<lt>:*:E<gt> will find both
directories I<and> files (and not, as one might expect, only directories).
You can, however, use the C<GLOB_MARK> flag to distinguish (without a file
test) directory names from file names.

If the C<GLOB_MARK> flag is set, all directory paths will have a ':' appended.
Since a directory like 'lib:' is I<not> a valid I<relative> path on Mac OS,
both a leading and a trailing colon will be added, when the directory name in
question doesn't contain any colons (e.g. 'lib' becomes ':lib:').

=back

=head1 SEE ALSO

L<perlfunc/glob>, glob(3)

=head1 AUTHOR

The Perl interface was written by Nathan Torkington E<lt>gnat@frii.comE<gt>,
and is released under the artistic license.  Further modifications were
made by Greg Bacon E<lt>gbacon@cs.uah.eduE<gt>, Gurusamy Sarathy
E<lt>gsar@activestate.comE<gt>, and Thomas Wegner
E<lt>wegner_thomas@yahoo.comE<gt>.  The C glob code has the
following copyright:

    Copyright (c) 1989, 1993 The Regents of the University of California.
    All rights reserved.

    This code is derived from software contributed to Berkeley by
    Guido van Rossum.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.
    3. Neither the name of the University nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
    ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
    DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
    OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
    OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
    SUCH DAMAGE.

=cut
