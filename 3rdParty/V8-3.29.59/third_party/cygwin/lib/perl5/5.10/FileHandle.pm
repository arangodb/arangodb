package FileHandle;

use 5.006;
use strict;
our($VERSION, @ISA, @EXPORT, @EXPORT_OK);

$VERSION = "2.01";

require IO::File;
@ISA = qw(IO::File);

@EXPORT = qw(_IOFBF _IOLBF _IONBF);

@EXPORT_OK = qw(
    pipe

    autoflush
    output_field_separator
    output_record_separator
    input_record_separator
    input_line_number
    format_page_number
    format_lines_per_page
    format_lines_left
    format_name
    format_top_name
    format_line_break_characters
    format_formfeed

    print
    printf
    getline
    getlines
);

#
# Everything we're willing to export, we must first import.
#
import IO::Handle grep { !defined(&$_) } @EXPORT, @EXPORT_OK;

#
# Some people call "FileHandle::function", so all the functions
# that were in the old FileHandle class must be imported, too.
#
{
    no strict 'refs';

    my %import = (
	'IO::Handle' =>
	    [qw(DESTROY new_from_fd fdopen close fileno getc ungetc gets
		eof flush error clearerr setbuf setvbuf _open_mode_string)],
	'IO::Seekable' =>
	    [qw(seek tell getpos setpos)],
	'IO::File' =>
	    [qw(new new_tmpfile open)]
    );
    for my $pkg (keys %import) {
	for my $func (@{$import{$pkg}}) {
	    my $c = *{"${pkg}::$func"}{CODE}
		or die "${pkg}::$func missing";
	    *$func = $c;
	}
    }
}

#
# Specialized importer for Fcntl magic.
#
sub import {
    my $pkg = shift;
    my $callpkg = caller;
    require Exporter;
    Exporter::export($pkg, $callpkg, @_);

    #
    # If the Fcntl extension is available,
    #  export its constants.
    #
    eval {
	require Fcntl;
	Exporter::export('Fcntl', $callpkg);
    };
}

################################################
# This is the only exported function we define;
# the rest come from other classes.
#

sub pipe {
    my $r = new IO::Handle;
    my $w = new IO::Handle;
    CORE::pipe($r, $w) or return undef;
    ($r, $w);
}

# Rebless standard file handles
bless *STDIN{IO},  "FileHandle" if ref *STDIN{IO}  eq "IO::Handle";
bless *STDOUT{IO}, "FileHandle" if ref *STDOUT{IO} eq "IO::Handle";
bless *STDERR{IO}, "FileHandle" if ref *STDERR{IO} eq "IO::Handle";

1;

__END__

=head1 NAME

FileHandle - supply object methods for filehandles

=head1 SYNOPSIS

    use FileHandle;

    $fh = new FileHandle;
    if ($fh->open("< file")) {
        print <$fh>;
        $fh->close;
    }

    $fh = new FileHandle "> FOO";
    if (defined $fh) {
        print $fh "bar\n";
        $fh->close;
    }

    $fh = new FileHandle "file", "r";
    if (defined $fh) {
        print <$fh>;
        undef $fh;       # automatically closes the file
    }

    $fh = new FileHandle "file", O_WRONLY|O_APPEND;
    if (defined $fh) {
        print $fh "corge\n";
        undef $fh;       # automatically closes the file
    }

    $pos = $fh->getpos;
    $fh->setpos($pos);

    $fh->setvbuf($buffer_var, _IOLBF, 1024);

    ($readfh, $writefh) = FileHandle::pipe;

    autoflush STDOUT 1;

=head1 DESCRIPTION

NOTE: This class is now a front-end to the IO::* classes.

C<FileHandle::new> creates a C<FileHandle>, which is a reference to a
newly created symbol (see the C<Symbol> package).  If it receives any
parameters, they are passed to C<FileHandle::open>; if the open fails,
the C<FileHandle> object is destroyed.  Otherwise, it is returned to
the caller.

C<FileHandle::new_from_fd> creates a C<FileHandle> like C<new> does.
It requires two parameters, which are passed to C<FileHandle::fdopen>;
if the fdopen fails, the C<FileHandle> object is destroyed.
Otherwise, it is returned to the caller.

C<FileHandle::open> accepts one parameter or two.  With one parameter,
it is just a front end for the built-in C<open> function.  With two
parameters, the first parameter is a filename that may include
whitespace or other special characters, and the second parameter is
the open mode, optionally followed by a file permission value.

If C<FileHandle::open> receives a Perl mode string (">", "+<", etc.)
or a POSIX fopen() mode string ("w", "r+", etc.), it uses the basic
Perl C<open> operator.

If C<FileHandle::open> is given a numeric mode, it passes that mode
and the optional permissions value to the Perl C<sysopen> operator.
For convenience, C<FileHandle::import> tries to import the O_XXX
constants from the Fcntl module.  If dynamic loading is not available,
this may fail, but the rest of FileHandle will still work.

C<FileHandle::fdopen> is like C<open> except that its first parameter
is not a filename but rather a file handle name, a FileHandle object,
or a file descriptor number.

If the C functions fgetpos() and fsetpos() are available, then
C<FileHandle::getpos> returns an opaque value that represents the
current position of the FileHandle, and C<FileHandle::setpos> uses
that value to return to a previously visited position.

If the C function setvbuf() is available, then C<FileHandle::setvbuf>
sets the buffering policy for the FileHandle.  The calling sequence
for the Perl function is the same as its C counterpart, including the
macros C<_IOFBF>, C<_IOLBF>, and C<_IONBF>, except that the buffer
parameter specifies a scalar variable to use as a buffer.  WARNING: A
variable used as a buffer by C<FileHandle::setvbuf> must not be
modified in any way until the FileHandle is closed or until
C<FileHandle::setvbuf> is called again, or memory corruption may
result!

See L<perlfunc> for complete descriptions of each of the following
supported C<FileHandle> methods, which are just front ends for the
corresponding built-in functions:

    close
    fileno
    getc
    gets
    eof
    clearerr
    seek
    tell

See L<perlvar> for complete descriptions of each of the following
supported C<FileHandle> methods:

    autoflush
    output_field_separator
    output_record_separator
    input_record_separator
    input_line_number
    format_page_number
    format_lines_per_page
    format_lines_left
    format_name
    format_top_name
    format_line_break_characters
    format_formfeed

Furthermore, for doing normal I/O you might need these:

=over 4

=item $fh->print

See L<perlfunc/print>.

=item $fh->printf

See L<perlfunc/printf>.

=item $fh->getline

This works like <$fh> described in L<perlop/"I/O Operators">
except that it's more readable and can be safely called in a
list context but still returns just one line.

=item $fh->getlines

This works like <$fh> when called in a list context to
read all the remaining lines in a file, except that it's more readable.
It will also croak() if accidentally called in a scalar context.

=back

There are many other functions available since FileHandle is descended
from IO::File, IO::Seekable, and IO::Handle.  Please see those
respective pages for documentation on more functions.

=head1 SEE ALSO

The B<IO> extension,
L<perlfunc>, 
L<perlop/"I/O Operators">.

=cut
