# Pod::Text::Overstrike -- Convert POD data to formatted overstrike text
# $Id: Overstrike.pm,v 2.0 2004/06/09 04:51:20 eagle Exp $
#
# Created by Joe Smith <Joe.Smith@inwap.com> 30-Nov-2000
#   (based on Pod::Text::Color by Russ Allbery <rra@stanford.edu>)
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# This was written because the output from:
#
#     pod2text Text.pm > plain.txt; less plain.txt
#
# is not as rich as the output from
#
#     pod2man Text.pm | nroff -man > fancy.txt; less fancy.txt
#
# and because both Pod::Text::Color and Pod::Text::Termcap are not device
# independent.

##############################################################################
# Modules and declarations
##############################################################################

package Pod::Text::Overstrike;

require 5.004;

use Pod::Text ();

use strict;
use vars qw(@ISA $VERSION);

@ISA = qw(Pod::Text);

# Don't use the CVS revision as the version, since this module is also in Perl
# core and too many things could munge CVS magic revision strings.  This
# number should ideally be the same as the CVS revision in podlators, however.
$VERSION = 2.00;

##############################################################################
# Overrides
##############################################################################

# Make level one headings bold, overridding any existing formatting.
sub cmd_head1 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $text = $self->strip_format ($text);
    $text =~ s/(.)/$1\b$1/g;
    return $self->SUPER::cmd_head1 ($attrs, $text);
}

# Make level two headings bold, overriding any existing formatting.
sub cmd_head2 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $text = $self->strip_format ($text);
    $text =~ s/(.)/$1\b$1/g;
    return $self->SUPER::cmd_head2 ($attrs, $text);
}

# Make level three headings underscored, overriding any existing formatting.
sub cmd_head3 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $text = $self->strip_format ($text);
    $text =~ s/(.)/_\b$1/g;
    return $self->SUPER::cmd_head3 ($attrs, $text);
}

# Level four headings look like level three headings.
sub cmd_head4 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $text = $self->strip_format ($text);
    $text =~ s/(.)/_\b$1/g;
    return $self->SUPER::cmd_head4 ($attrs, $text);
}

# The common code for handling all headers.  We have to override to avoid
# interpolating twice and because we don't want to honor alt.
sub heading {
    my ($self, $text, $indent, $marker) = @_;
    $self->item ("\n\n") if defined $$self{ITEM};
    $text .= "\n" if $$self{opt_loose};
    my $margin = ' ' x ($$self{opt_margin} + $indent);
    $self->output ($margin . $text . "\n");
    return '';
}

# Fix the various formatting codes.
sub cmd_b { local $_ = $_[0]->strip_format ($_[2]); s/(.)/$1\b$1/g; $_ }
sub cmd_f { local $_ = $_[0]->strip_format ($_[2]); s/(.)/_\b$1/g; $_ }
sub cmd_i { local $_ = $_[0]->strip_format ($_[2]); s/(.)/_\b$1/g; $_ }

# Output any included code in bold.
sub output_code {
    my ($self, $code) = @_;
    $code =~ s/(.)/$1\b$1/g;
    $self->output ($code);
}

# We unfortunately have to override the wrapping code here, since the normal
# wrapping code gets really confused by all the backspaces.
sub wrap {
    my $self = shift;
    local $_ = shift;
    my $output = '';
    my $spaces = ' ' x $$self{MARGIN};
    my $width = $$self{opt_width} - $$self{MARGIN};
    while (length > $width) {
        # This regex represents a single character, that's possibly underlined
        # or in bold (in which case, it's three characters; the character, a
        # backspace, and a character).  Use [^\n] rather than . to protect
        # against odd settings of $*.
        my $char = '(?:[^\n][\b])?[^\n]';
        if (s/^((?>$char){0,$width})(?:\Z|\s+)//) {
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
# Utility functions
##############################################################################

# Strip all of the formatting from a provided string, returning the stripped
# version.
sub strip_format {
    my ($self, $text) = @_;
    $text =~ s/(.)[\b]\1/$1/g;
    $text =~ s/_[\b]//g;
    return $text;
}

##############################################################################
# Module return value and documentation
##############################################################################

1;
__END__

=head1 NAME

Pod::Text::Overstrike - Convert POD data to formatted overstrike text

=head1 SYNOPSIS

    use Pod::Text::Overstrike;
    my $parser = Pod::Text::Overstrike->new (sentence => 0, width => 78);

    # Read POD from STDIN and write to STDOUT.
    $parser->parse_from_filehandle;

    # Read POD from file.pod and write to file.txt.
    $parser->parse_from_file ('file.pod', 'file.txt');

=head1 DESCRIPTION

Pod::Text::Overstrike is a simple subclass of Pod::Text that highlights
output text using overstrike sequences, in a manner similar to nroff.
Characters in bold text are overstruck (character, backspace, character) and
characters in underlined text are converted to overstruck underscores
(underscore, backspace, character).  This format was originally designed for
hardcopy terminals and/or lineprinters, yet is readable on softcopy (CRT)
terminals.

Overstruck text is best viewed by page-at-a-time programs that take
advantage of the terminal's B<stand-out> and I<underline> capabilities, such
as the less program on Unix.

Apart from the overstrike, it in all ways functions like Pod::Text.  See
L<Pod::Text> for details and available options.

=head1 BUGS

Currently, the outermost formatting instruction wins, so for example
underlined text inside a region of bold text is displayed as simply bold.
There may be some better approach possible.

=head1 SEE ALSO

L<Pod::Text>, L<Pod::Simple>

The current version of this module is always available from its web site at
L<http://www.eyrie.org/~eagle/software/podlators/>.  It is also part of the
Perl core distribution as of 5.6.0.

=head1 AUTHOR

Joe Smith <Joe.Smith@inwap.com>, using the framework created by Russ Allbery
<rra@stanford.edu>.

=head1 COPYRIGHT AND LICENSE

Copyright 2000 by Joe Smith <Joe.Smith@inwap.com>.
Copyright 2001, 2004 by Russ Allbery <rra@stanford.edu>.

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

=cut
