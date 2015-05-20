# Pod::Text::Termcap -- Convert POD data to ASCII text with format escapes.
# $Id: Termcap.pm,v 2.3 2006-01-25 23:56:54 eagle Exp $
#
# Copyright 1999, 2001, 2002, 2004, 2006 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# This is a simple subclass of Pod::Text that overrides a few key methods to
# output the right termcap escape sequences for formatted text on the current
# terminal type.

##############################################################################
# Modules and declarations
##############################################################################

package Pod::Text::Termcap;

require 5.004;

use Pod::Text ();
use POSIX ();
use Term::Cap;

use strict;
use vars qw(@ISA $VERSION);

@ISA = qw(Pod::Text);

# Don't use the CVS revision as the version, since this module is also in Perl
# core and too many things could munge CVS magic revision strings.  This
# number should ideally be the same as the CVS revision in podlators, however.
$VERSION = 2.03;

##############################################################################
# Overrides
##############################################################################

# In the initialization method, grab our terminal characteristics as well as
# do all the stuff we normally do.
sub new {
    my ($self, @args) = @_;
    my ($ospeed, $term, $termios);
    $self = $self->SUPER::new (@args);

    # $ENV{HOME} is usually not set on Windows.  The default Term::Cap path
    # may not work on Solaris.
    my $home = exists $ENV{HOME} ? "$ENV{HOME}/.termcap:" : '';
    $ENV{TERMPATH} = $home . '/etc/termcap:/usr/share/misc/termcap'
                           . ':/usr/share/lib/termcap';

    # Fall back on a hard-coded terminal speed if POSIX::Termios isn't
    # available (such as on VMS).
    eval { $termios = POSIX::Termios->new };
    if ($@) {
        $ospeed = 9600;
    } else {
        $termios->getattr;
        $ospeed = $termios->getospeed || 9600;
    }

    # Fall back on the ANSI escape sequences if Term::Cap doesn't work.
    eval { $term = Tgetent Term::Cap { TERM => undef, OSPEED => $ospeed } };
    $$self{BOLD} = $$term{_md} || "\e[1m";
    $$self{UNDL} = $$term{_us} || "\e[4m";
    $$self{NORM} = $$term{_me} || "\e[m";

    unless (defined $$self{width}) {
        $$self{opt_width} = $ENV{COLUMNS} || $$term{_co} || 80;
        $$self{opt_width} -= 2;
    }

    return $self;
}

# Make level one headings bold.
sub cmd_head1 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $self->SUPER::cmd_head1 ($attrs, "$$self{BOLD}$text$$self{NORM}");
}

# Make level two headings bold.
sub cmd_head2 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $self->SUPER::cmd_head2 ($attrs, "$$self{BOLD}$text$$self{NORM}");
}

# Fix up B<> and I<>.  Note that we intentionally don't do F<>.
sub cmd_b { my $self = shift; return "$$self{BOLD}$_[1]$$self{NORM}" }
sub cmd_i { my $self = shift; return "$$self{UNDL}$_[1]$$self{NORM}" }

# Output any included code in bold.
sub output_code {
    my ($self, $code) = @_;
    $self->output ($$self{BOLD} . $code . $$self{NORM});
}

# Override the wrapping code to igore the special sequences.
sub wrap {
    my $self = shift;
    local $_ = shift;
    my $output = '';
    my $spaces = ' ' x $$self{MARGIN};
    my $width = $$self{opt_width} - $$self{MARGIN};

    # $codes matches a single special sequence.  $char matches any number of
    # special sequences preceeding a single character other than a newline.
    # We have to do $shortchar and $longchar in variables because the
    # construct ${char}{0,$width} didn't do the right thing until Perl 5.8.x.
    my $codes = "(?:\Q$$self{BOLD}\E|\Q$$self{UNDL}\E|\Q$$self{NORM}\E)";
    my $char = "(?:$codes*[^\\n])";
    my $shortchar = $char . "{0,$width}";
    my $longchar = $char . "{$width}";
    while (length > $width) {
        if (s/^($shortchar)\s+// || s/^($longchar)//) {
            $output .= $spaces . $1 . "\n";
        } else {
            last;
        }
    }
    $output .= $spaces . $_;
    $output =~ s/\s+$/\n\n/;
    return $output;
}

##############################################################################
# Module return value and documentation
##############################################################################

1;
__END__

=head1 NAME

Pod::Text::Termcap - Convert POD data to ASCII text with format escapes

=head1 SYNOPSIS

    use Pod::Text::Termcap;
    my $parser = Pod::Text::Termcap->new (sentence => 0, width => 78);

    # Read POD from STDIN and write to STDOUT.
    $parser->parse_from_filehandle;

    # Read POD from file.pod and write to file.txt.
    $parser->parse_from_file ('file.pod', 'file.txt');

=head1 DESCRIPTION

Pod::Text::Termcap is a simple subclass of Pod::Text that highlights output
text using the correct termcap escape sequences for the current terminal.
Apart from the format codes, it in all ways functions like Pod::Text.  See
L<Pod::Text> for details and available options.

=head1 NOTES

This module uses Term::Cap to retrieve the formatting escape sequences for
the current terminal, and falls back on the ECMA-48 (the same in this
regard as ANSI X3.64 and ISO 6429, the escape codes also used by DEC VT100
terminals) if the bold, underline, and reset codes aren't set in the
termcap information.

=head1 SEE ALSO

L<Pod::Text>, L<Pod::Simple>, L<Term::Cap>

The current version of this module is always available from its web site at
L<http://www.eyrie.org/~eagle/software/podlators/>.  It is also part of the
Perl core distribution as of 5.6.0.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>.

=head1 COPYRIGHT AND LICENSE

Copyright 1999, 2001, 2002, 2004, 2006 by Russ Allbery <rra@stanford.edu>.

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

=cut
