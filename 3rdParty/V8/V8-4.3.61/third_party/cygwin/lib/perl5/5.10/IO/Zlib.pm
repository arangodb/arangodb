# IO::Zlib.pm
#
# Copyright (c) 1998-2004 Tom Hughes <tom@compton.nu>.
# All rights reserved. This program is free software; you can redistribute
# it and/or modify it under the same terms as Perl itself.

package IO::Zlib;

$VERSION = "1.09";

=head1 NAME

IO::Zlib - IO:: style interface to L<Compress::Zlib>

=head1 SYNOPSIS

With any version of Perl 5 you can use the basic OO interface:

    use IO::Zlib;

    $fh = new IO::Zlib;
    if ($fh->open("file.gz", "rb")) {
        print <$fh>;
        $fh->close;
    }

    $fh = IO::Zlib->new("file.gz", "wb9");
    if (defined $fh) {
        print $fh "bar\n";
        $fh->close;
    }

    $fh = IO::Zlib->new("file.gz", "rb");
    if (defined $fh) {
        print <$fh>;
        undef $fh;       # automatically closes the file
    }

With Perl 5.004 you can also use the TIEHANDLE interface to access
compressed files just like ordinary files:

    use IO::Zlib;

    tie *FILE, 'IO::Zlib', "file.gz", "wb";
    print FILE "line 1\nline2\n";

    tie *FILE, 'IO::Zlib', "file.gz", "rb";
    while (<FILE>) { print "LINE: ", $_ };

=head1 DESCRIPTION

C<IO::Zlib> provides an IO:: style interface to L<Compress::Zlib> and
hence to gzip/zlib compressed files. It provides many of the same methods
as the L<IO::Handle> interface.

Starting from IO::Zlib version 1.02, IO::Zlib can also use an
external F<gzip> command.  The default behaviour is to try to use
an external F<gzip> if no C<Compress::Zlib> can be loaded, unless
explicitly disabled by

    use IO::Zlib qw(:gzip_external 0);

If explicitly enabled by

    use IO::Zlib qw(:gzip_external 1);

then the external F<gzip> is used B<instead> of C<Compress::Zlib>.

=head1 CONSTRUCTOR

=over 4

=item new ( [ARGS] )

Creates an C<IO::Zlib> object. If it receives any parameters, they are
passed to the method C<open>; if the open fails, the object is destroyed.
Otherwise, it is returned to the caller.

=back

=head1 OBJECT METHODS

=over 4

=item open ( FILENAME, MODE )

C<open> takes two arguments. The first is the name of the file to open
and the second is the open mode. The mode can be anything acceptable to
L<Compress::Zlib> and by extension anything acceptable to I<zlib> (that
basically means POSIX fopen() style mode strings plus an optional number
to indicate the compression level).

=item opened

Returns true if the object currently refers to a opened file.

=item close

Close the file associated with the object and disassociate
the file from the handle.
Done automatically on destroy.

=item getc

Return the next character from the file, or undef if none remain.

=item getline

Return the next line from the file, or undef on end of string.
Can safely be called in an array context.
Currently ignores $/ ($INPUT_RECORD_SEPARATOR or $RS when L<English>
is in use) and treats lines as delimited by "\n".

=item getlines

Get all remaining lines from the file.
It will croak() if accidentally called in a scalar context.

=item print ( ARGS... )

Print ARGS to the  file.

=item read ( BUF, NBYTES, [OFFSET] )

Read some bytes from the file.
Returns the number of bytes actually read, 0 on end-of-file, undef on error.

=item eof

Returns true if the handle is currently positioned at end of file?

=item seek ( OFFSET, WHENCE )

Seek to a given position in the stream.
Not yet supported.

=item tell

Return the current position in the stream, as a numeric offset.
Not yet supported.

=item setpos ( POS )

Set the current position, using the opaque value returned by C<getpos()>.
Not yet supported.

=item getpos ( POS )

Return the current position in the string, as an opaque object.
Not yet supported.

=back

=head1 USING THE EXTERNAL GZIP

If the external F<gzip> is used, the following C<open>s are used:

    open(FH, "gzip -dc $filename |")  # for read opens
    open(FH, " | gzip > $filename")   # for write opens

You can modify the 'commands' for example to hardwire
an absolute path by e.g.

    use IO::Zlib ':gzip_read_open'  => '/some/where/gunzip -c %s |';
    use IO::Zlib ':gzip_write_open' => '| /some/where/gzip.exe > %s';

The C<%s> is expanded to be the filename (C<sprintf> is used, so be
careful to escape any other C<%> signs).  The 'commands' are checked
for sanity - they must contain the C<%s>, and the read open must end
with the pipe sign, and the write open must begin with the pipe sign.

=head1 CLASS METHODS

=over 4

=item has_Compress_Zlib

Returns true if C<Compress::Zlib> is available.  Note that this does
not mean that C<Compress::Zlib> is being used: see L</gzip_external>
and L<gzip_used>.

=item gzip_external

Undef if an external F<gzip> B<can> be used if C<Compress::Zlib> is
not available (see L</has_Compress_Zlib>), true if an external F<gzip>
is explicitly used, false if an external F<gzip> must not be used.
See L</gzip_used>.

=item gzip_used

True if an external F<gzip> is being used, false if not.

=item gzip_read_open

Return the 'command' being used for opening a file for reading using an
external F<gzip>.

=item gzip_write_open

Return the 'command' being used for opening a file for writing using an
external F<gzip>.

=back

=head1 DIAGNOSTICS

=over 4

=item IO::Zlib::getlines: must be called in list context

If you want read lines, you must read in list context.

=item IO::Zlib::gzopen_external: mode '...' is illegal

Use only modes 'rb' or 'wb' or /wb[1-9]/.

=item IO::Zlib::import: '...' is illegal

The known import symbols are the C<:gzip_external>, C<:gzip_read_open>,
and C<:gzip_write_open>.  Anything else is not recognized.

=item IO::Zlib::import: ':gzip_external' requires an argument

The C<:gzip_external> requires one boolean argument.

=item IO::Zlib::import: 'gzip_read_open' requires an argument

The C<:gzip_external> requires one string argument.

=item IO::Zlib::import: 'gzip_read' '...' is illegal

The C<:gzip_read_open> argument must end with the pipe sign (|)
and have the C<%s> for the filename.  See L</"USING THE EXTERNAL GZIP">.

=item IO::Zlib::import: 'gzip_write_open' requires an argument

The C<:gzip_external> requires one string argument.

=item IO::Zlib::import: 'gzip_write_open' '...' is illegal

The C<:gzip_write_open> argument must begin with the pipe sign (|)
and have the C<%s> for the filename.  An output redirect (>) is also
often a good idea, depending on your operating system shell syntax.
See L</"USING THE EXTERNAL GZIP">.

=item IO::Zlib::import: no Compress::Zlib and no external gzip

Given that we failed to load C<Compress::Zlib> and that the use of
 an external F<gzip> was disabled, IO::Zlib has not much chance of working.

=item IO::Zlib::open: needs a filename

No filename, no open.

=item IO::Zlib::READ: NBYTES must be specified

We must know how much to read.

=item IO::Zlib::WRITE: too long LENGTH

The LENGTH must be less than or equal to the buffer size.

=back

=head1 SEE ALSO

L<perlfunc>,
L<perlop/"I/O Operators">,
L<IO::Handle>,
L<Compress::Zlib>

=head1 HISTORY

Created by Tom Hughes E<lt>F<tom@compton.nu>E<gt>.

Support for external gzip added by Jarkko Hietaniemi E<lt>F<jhi@iki.fi>E<gt>.

=head1 COPYRIGHT

Copyright (c) 1998-2004 Tom Hughes E<lt>F<tom@compton.nu>E<gt>.
All rights reserved. This program is free software; you can redistribute
it and/or modify it under the same terms as Perl itself.

=cut

require 5.006;

use strict;
use vars qw($VERSION $AUTOLOAD @ISA);

use Carp;
use Fcntl qw(SEEK_SET);

my $has_Compress_Zlib;
my $aliased;

sub has_Compress_Zlib {
    $has_Compress_Zlib;
}

BEGIN {
    eval { require Compress::Zlib };
    $has_Compress_Zlib = $@ ? 0 : 1;
}

use Symbol;
use Tie::Handle;

# These might use some $^O logic.
my $gzip_read_open   = "gzip -dc %s |";
my $gzip_write_open  = "| gzip > %s";

my $gzip_external;
my $gzip_used;

sub gzip_read_open {
    $gzip_read_open;
}

sub gzip_write_open {
    $gzip_write_open;
}

sub gzip_external {
    $gzip_external;
}

sub gzip_used {
    $gzip_used;
}

sub can_gunzip {
    $has_Compress_Zlib || $gzip_external;
}

sub _import {
    my $import = shift;
    while (@_) {
	if ($_[0] eq ':gzip_external') {
	    shift;
	    if (@_) {
		$gzip_external = shift;
	    } else {
		croak "$import: ':gzip_external' requires an argument";
	    }
	}
	elsif ($_[0] eq ':gzip_read_open') {
	    shift;
	    if (@_) {
		$gzip_read_open = shift;
		croak "$import: ':gzip_read_open' '$gzip_read_open' is illegal"
		    unless $gzip_read_open =~ /^.+%s.+\|\s*$/;
	    } else {
		croak "$import: ':gzip_read_open' requires an argument";
	    }
	}
	elsif ($_[0] eq ':gzip_write_open') {
	    shift;
	    if (@_) {
		$gzip_write_open = shift;
		croak "$import: ':gzip_write_open' '$gzip_read_open' is illegal"
		    unless $gzip_write_open =~ /^\s*\|.+%s.*$/;
	    } else {
		croak "$import: ':gzip_write_open' requires an argument";
	    }
	}
	else {
	    last;
	}
    }
    return @_;
}

sub _alias {
    my $import = shift;
    if ((!$has_Compress_Zlib && !defined $gzip_external) || $gzip_external) {
	# The undef *gzopen is really needed only during
	# testing where we eval several 'use IO::Zlib's.
	undef *gzopen;
        *gzopen                 = \&gzopen_external;
        *IO::Handle::gzread     = \&gzread_external;
        *IO::Handle::gzwrite    = \&gzwrite_external;
        *IO::Handle::gzreadline = \&gzreadline_external;
        *IO::Handle::gzeof      = \&gzeof_external;
        *IO::Handle::gzclose    = \&gzclose_external;
	$gzip_used = 1;
    } else {
	croak "$import: no Compress::Zlib and no external gzip"
	    unless $has_Compress_Zlib;
        *gzopen     = \&Compress::Zlib::gzopen;
        *gzread     = \&Compress::Zlib::gzread;
        *gzwrite    = \&Compress::Zlib::gzwrite;
        *gzreadline = \&Compress::Zlib::gzreadline;
        *gzeof      = \&Compress::Zlib::gzeof;
    }
    $aliased = 1;
}

sub import {
    shift;
    my $import = "IO::Zlib::import";
    if (@_) {
	if (_import($import, @_)) {
	    croak "$import: '@_' is illegal";
	}
    }
    _alias($import);
}

@ISA = qw(Tie::Handle);

sub TIEHANDLE
{
    my $class = shift;
    my @args = @_;

    my $self = bless {}, $class;

    return @args ? $self->OPEN(@args) : $self;
}

sub DESTROY
{
}

sub OPEN
{
    my $self = shift;
    my $filename = shift;
    my $mode = shift;

    croak "IO::Zlib::open: needs a filename" unless defined($filename);

    $self->{'file'} = gzopen($filename,$mode);

    return defined($self->{'file'}) ? $self : undef;
}

sub CLOSE
{
    my $self = shift;

    return undef unless defined($self->{'file'});

    my $status = $self->{'file'}->gzclose();

    delete $self->{'file'};

    return ($status == 0) ? 1 : undef;
}

sub READ
{
    my $self = shift;
    my $bufref = \$_[0];
    my $nbytes = $_[1];
    my $offset = $_[2] || 0;

    croak "IO::Zlib::READ: NBYTES must be specified" unless defined($nbytes);

    $$bufref = "" unless defined($$bufref);

    my $bytesread = $self->{'file'}->gzread(substr($$bufref,$offset),$nbytes);

    return undef if $bytesread < 0;

    return $bytesread;
}

sub READLINE
{
    my $self = shift;

    my $line;

    return () if $self->{'file'}->gzreadline($line) <= 0;

    return $line unless wantarray;

    my @lines = $line;

    while ($self->{'file'}->gzreadline($line) > 0)
    {
        push @lines, $line;
    }

    return @lines;
}

sub WRITE
{
    my $self = shift;
    my $buf = shift;
    my $length = shift;
    my $offset = shift;

    croak "IO::Zlib::WRITE: too long LENGTH" unless $offset + $length <= length($buf);

    return $self->{'file'}->gzwrite(substr($buf,$offset,$length));
}

sub EOF
{
    my $self = shift;

    return $self->{'file'}->gzeof();
}

sub FILENO
{
    return undef;
}

sub new
{
    my $class = shift;
    my @args = @_;

    _alias("new", @_) unless $aliased; # Some call new IO::Zlib directly...

    my $self = gensym();

    tie *{$self}, $class, @args;

    return tied(${$self}) ? bless $self, $class : undef;
}

sub getline
{
    my $self = shift;

    return scalar tied(*{$self})->READLINE();
}

sub getlines
{
    my $self = shift;

    croak "IO::Zlib::getlines: must be called in list context"
	unless wantarray;

    return tied(*{$self})->READLINE();
}

sub opened
{
    my $self = shift;

    return defined tied(*{$self})->{'file'};
}

sub AUTOLOAD
{
    my $self = shift;

    $AUTOLOAD =~ s/.*:://;
    $AUTOLOAD =~ tr/a-z/A-Z/;

    return tied(*{$self})->$AUTOLOAD(@_);
}

sub gzopen_external {
    my ($filename, $mode) = @_;
    require IO::Handle;
    my $fh = IO::Handle->new();
    if ($mode =~ /r/) {
	# Because someone will try to read ungzipped files
	# with this we peek and verify the signature.  Yes,
	# this means that we open the file twice (if it is
	# gzipped).
	# Plenty of race conditions exist in this code, but
	# the alternative would be to capture the stderr of
	# gzip and parse it, which would be a portability nightmare.
	if (-e $filename && open($fh, $filename)) {
	    binmode $fh;
	    my $sig;
	    my $rdb = read($fh, $sig, 2);
	    if ($rdb == 2 && $sig eq "\x1F\x8B") {
		my $ropen = sprintf $gzip_read_open, $filename;
		if (open($fh, $ropen)) {
		    binmode $fh;
		    return $fh;
		} else {
		    return undef;
		}
	    }
	    seek($fh, 0, SEEK_SET) or
		die "IO::Zlib: open('$filename', 'r'): seek: $!";
	    return $fh;
	} else {
	    return undef;
	}
    } elsif ($mode =~ /w/) {
	my $level = '';
	$level = "-$1" if $mode =~ /([1-9])/;
	# To maximize portability we would need to open
	# two filehandles here, one for "| gzip $level"
	# and another for "> $filename", and then when
	# writing copy bytes from the first to the second.
	# We are using IO::Handle objects for now, however,
	# and they can only contain one stream at a time.
	my $wopen = sprintf $gzip_write_open, $filename;
	if (open($fh, $wopen)) {
	    $fh->autoflush(1);
	    binmode $fh;
	    return $fh;
	} else {
	    return undef;
	}
    } else {
	croak "IO::Zlib::gzopen_external: mode '$mode' is illegal";
    }
    return undef;
}

sub gzread_external {
    # Use read() instead of syswrite() because people may
    # mix reads and readlines, and we don't want to mess
    # the stdio buffering.  See also gzreadline_external()
    # and gzwrite_external().
    my $nread = read($_[0], $_[1], @_ == 3 ? $_[2] : 4096);
    defined $nread ? $nread : -1;
}

sub gzwrite_external {
    # Using syswrite() is okay (cf. gzread_external())
    # since the bytes leave this process and buffering
    # is therefore not an issue.
    my $nwrote = syswrite($_[0], $_[1]);
    defined $nwrote ? $nwrote : -1;
}

sub gzreadline_external {
    # See the comment in gzread_external().
    $_[1] = readline($_[0]);
    return defined $_[1] ? length($_[1]) : -1;
}

sub gzeof_external {
    return eof($_[0]);
}

sub gzclose_external {
    close($_[0]);
    # I am not entirely certain why this is needed but it seems
    # the above close() always fails (as if the stream would have
    # been already closed - something to do with using external
    # processes via pipes?)
    return 0;
}

1;
