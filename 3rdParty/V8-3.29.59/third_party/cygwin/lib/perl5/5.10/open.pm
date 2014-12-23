package open;
use warnings;

our $VERSION = '1.06';

require 5.008001; # for PerlIO::get_layers()

my $locale_encoding;

sub _get_encname {
    return ($1, Encode::resolve_alias($1)) if $_[0] =~ /^:?encoding\((.+)\)$/;
    return;
}

sub croak {
    require Carp; goto &Carp::croak;
}

sub _drop_oldenc {
    # If by the time we arrive here there already is at the top of the
    # perlio layer stack an encoding identical to what we would like
    # to push via this open pragma, we will pop away the old encoding
    # (+utf8) so that we can push ourselves in place (this is easier
    # than ignoring pushing ourselves because of the way how ${^OPEN}
    # works).  So we are looking for something like
    #
    #   stdio encoding(xxx) utf8
    #
    # in the existing layer stack, and in the new stack chunk for
    #
    #   :encoding(xxx)
    #
    # If we find a match, we pop the old stack (once, since
    # the utf8 is just a flag on the encoding layer)
    my ($h, @new) = @_;
    return unless @new >= 1 && $new[-1] =~ /^:encoding\(.+\)$/;
    my @old = PerlIO::get_layers($h);
    return unless @old >= 3 &&
	          $old[-1] eq 'utf8' &&
                  $old[-2] =~ /^encoding\(.+\)$/;
    require Encode;
    my ($loname, $lcname) = _get_encname($old[-2]);
    unless (defined $lcname) { # Should we trust get_layers()?
	croak("open: Unknown encoding '$loname'");
    }
    my ($voname, $vcname) = _get_encname($new[-1]);
    unless (defined $vcname) {
	croak("open: Unknown encoding '$voname'");
    }
    if ($lcname eq $vcname) {
	binmode($h, ":pop"); # utf8 is part of the encoding layer
    }
}

sub import {
    my ($class,@args) = @_;
    croak("open: needs explicit list of PerlIO layers") unless @args;
    my $std;
    my ($in,$out) = split(/\0/,(${^OPEN} || "\0"), -1);
    while (@args) {
	my $type = shift(@args);
	my $dscp;
	if ($type =~ /^:?(utf8|locale|encoding\(.+\))$/) {
	    $type = 'IO';
	    $dscp = ":$1";
	} elsif ($type eq ':std') {
	    $std = 1;
	    next;
	} else {
	    $dscp = shift(@args) || '';
	}
	my @val;
	foreach my $layer (split(/\s+/,$dscp)) {
            $layer =~ s/^://;
	    if ($layer eq 'locale') {
		require Encode;
		require encoding;
		$locale_encoding = encoding::_get_locale_encoding()
		    unless defined $locale_encoding;
		(warnings::warnif("layer", "Cannot figure out an encoding to use"), last)
		    unless defined $locale_encoding;
                $layer = "encoding($locale_encoding)";
		$std = 1;
	    } else {
		my $target = $layer;		# the layer name itself
		$target =~ s/^(\w+)\(.+\)$/$1/;	# strip parameters

		unless(PerlIO::Layer::->find($target,1)) {
		    warnings::warnif("layer", "Unknown PerlIO layer '$target'");
		}
	    }
	    push(@val,":$layer");
	    if ($layer =~ /^(crlf|raw)$/) {
		$^H{"open_$type"} = $layer;
	    }
	}
	if ($type eq 'IN') {
	    _drop_oldenc(*STDIN, @val);
	    $in  = join(' ', @val);
	}
	elsif ($type eq 'OUT') {
	    _drop_oldenc(*STDOUT, @val);
	    $out = join(' ', @val);
	}
	elsif ($type eq 'IO') {
	    _drop_oldenc(*STDIN,  @val);
	    _drop_oldenc(*STDOUT, @val);
	    $in = $out = join(' ', @val);
	}
	else {
	    croak "Unknown PerlIO layer class '$type'";
	}
    }
    ${^OPEN} = join("\0", $in, $out);
    if ($std) {
	if ($in) {
	    if ($in =~ /:utf8\b/) {
		    binmode(STDIN,  ":utf8");
		} elsif ($in =~ /(\w+\(.+\))/) {
		    binmode(STDIN,  ":$1");
		}
	}
	if ($out) {
	    if ($out =~ /:utf8\b/) {
		binmode(STDOUT,  ":utf8");
		binmode(STDERR,  ":utf8");
	    } elsif ($out =~ /(\w+\(.+\))/) {
		binmode(STDOUT,  ":$1");
		binmode(STDERR,  ":$1");
	    }
	}
    }
}

1;
__END__

=head1 NAME

open - perl pragma to set default PerlIO layers for input and output

=head1 SYNOPSIS

    use open IN  => ":crlf", OUT => ":bytes";
    use open OUT => ':utf8';
    use open IO  => ":encoding(iso-8859-7)";

    use open IO  => ':locale';

    use open ':encoding(utf8)';
    use open ':locale';
    use open ':encoding(iso-8859-7)';

    use open ':std';

=head1 DESCRIPTION

Full-fledged support for I/O layers is now implemented provided
Perl is configured to use PerlIO as its IO system (which is now the
default).

The C<open> pragma serves as one of the interfaces to declare default
"layers" (also known as "disciplines") for all I/O. Any two-argument
open(), readpipe() (aka qx//) and similar operators found within the
lexical scope of this pragma will use the declared defaults.
Even three-argument opens may be affected by this pragma
when they don't specify IO layers in MODE.

With the C<IN> subpragma you can declare the default layers
of input streams, and with the C<OUT> subpragma you can declare
the default layers of output streams.  With the C<IO>  subpragma
you can control both input and output streams simultaneously.

If you have a legacy encoding, you can use the C<:encoding(...)> tag.

If you want to set your encoding layers based on your
locale environment variables, you can use the C<:locale> tag.
For example:

    $ENV{LANG} = 'ru_RU.KOI8-R';
    # the :locale will probe the locale environment variables like LANG
    use open OUT => ':locale';
    open(O, ">koi8");
    print O chr(0x430); # Unicode CYRILLIC SMALL LETTER A = KOI8-R 0xc1
    close O;
    open(I, "<koi8");
    printf "%#x\n", ord(<I>), "\n"; # this should print 0xc1
    close I;

These are equivalent

    use open ':encoding(utf8)';
    use open IO => ':encoding(utf8)';

as are these

    use open ':locale';
    use open IO => ':locale';

and these

    use open ':encoding(iso-8859-7)';
    use open IO => ':encoding(iso-8859-7)';

The matching of encoding names is loose: case does not matter, and
many encodings have several aliases.  See L<Encode::Supported> for
details and the list of supported locales.

When open() is given an explicit list of layers (with the three-arg
syntax), they override the list declared using this pragma.

The C<:std> subpragma on its own has no effect, but if combined with
the C<:utf8> or C<:encoding> subpragmas, it converts the standard
filehandles (STDIN, STDOUT, STDERR) to comply with encoding selected
for input/output handles.  For example, if both input and out are
chosen to be C<:encoding(utf8)>, a C<:std> will mean that STDIN, STDOUT,
and STDERR are also in C<:encoding(utf8)>.  On the other hand, if only
output is chosen to be in C<< :encoding(koi8r) >>, a C<:std> will cause
only the STDOUT and STDERR to be in C<koi8r>.  The C<:locale> subpragma
implicitly turns on C<:std>.

The logic of C<:locale> is described in full in L<encoding>,
but in short it is first trying nl_langinfo(CODESET) and then
guessing from the LC_ALL and LANG locale environment variables.

Directory handles may also support PerlIO layers in the future.

=head1 NONPERLIO FUNCTIONALITY

If Perl is not built to use PerlIO as its IO system then only the two
pseudo-layers C<:bytes> and C<:crlf> are available.

The C<:bytes> layer corresponds to "binary mode" and the C<:crlf>
layer corresponds to "text mode" on platforms that distinguish
between the two modes when opening files (which is many DOS-like
platforms, including Windows).  These two layers are no-ops on
platforms where binmode() is a no-op, but perform their functions
everywhere if PerlIO is enabled.

=head1 IMPLEMENTATION DETAILS

There is a class method in C<PerlIO::Layer> C<find> which is
implemented as XS code.  It is called by C<import> to validate the
layers:

   PerlIO::Layer::->find("perlio")

The return value (if defined) is a Perl object, of class
C<PerlIO::Layer> which is created by the C code in F<perlio.c>.  As
yet there is nothing useful you can do with the object at the perl
level.

=head1 SEE ALSO

L<perlfunc/"binmode">, L<perlfunc/"open">, L<perlunicode>, L<PerlIO>,
L<encoding>

=cut
