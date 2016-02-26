# Pod::Text::Color -- Convert POD data to formatted color ASCII text
# $Id: Color.pm,v 2.3 2006-01-25 23:56:54 eagle Exp $
#
# Copyright 1999, 2001, 2004, 2006 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# This is just a basic proof of concept.  It should later be modified to make
# better use of color, take options changing what colors are used for what
# text, and the like.

##############################################################################
# Modules and declarations
##############################################################################

package Pod::Text::Color;

require 5.004;

use Pod::Text ();
use Term::ANSIColor qw(colored);

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

# Make level one headings bold.
sub cmd_head1 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $self->SUPER::cmd_head1 ($attrs, colored ($text, 'bold'));
}

# Make level two headings bold.
sub cmd_head2 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\s+$//;
    $self->SUPER::cmd_head2 ($attrs, colored ($text, 'bold'));
}

# Fix the various formatting codes.
sub cmd_b { return colored ($_[2], 'bold')   }
sub cmd_f { return colored ($_[2], 'cyan')   }
sub cmd_i { return colored ($_[2], 'yellow') }

# Output any included code in green.
sub output_code {
    my ($self, $code) = @_;
    $code = colored ($code, 'green');
    $self->output ($code);
}

# We unfortunately have to override the wrapping code here, since the normal
# wrapping code gets really confused by all the escape sequences.
sub wrap {
    my $self = shift;
    local $_ = shift;
    my $output = '';
    my $spaces = ' ' x $$self{MARGIN};
    my $width = $$self{opt_width} - $$self{MARGIN};

    # We have to do $shortchar and $longchar in variables because the
    # construct ${char}{0,$width} didn't do the right thing until Perl 5.8.x.
    my $char = '(?:(?:\e\[[\d;]+m)*[^\n])';
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
    $output;
}

##############################################################################
# Module return value and documentation
##############################################################################

1;
__END__

=head1 NAME

Pod::Text::Color - Convert POD data to formatted color ASCII text

=head1 SYNOPSIS

    use Pod::Text::Color;
    my $parser = Pod::Text::Color->new (sentence => 0, width => 78);

    # Read POD from STDIN and write to STDOUT.
    $parser->parse_from_filehandle;

    # Read POD from file.pod and write to file.txt.
    $parser->parse_from_file ('file.pod', 'file.txt');

=head1 DESCRIPTION

Pod::Text::Color is a simple subclass of Pod::Text that highlights output
text using ANSI color escape sequences.  Apart from the color, it in all
ways functions like Pod::Text.  See L<Pod::Text> for details and available
options.

Term::ANSIColor is used to get colors and therefore must be installed to use
this module.

=head1 BUGS

This is just a basic proof of concept.  It should be seriously expanded to
support configurable coloration via options passed to the constructor, and
B<pod2text> should be taught about those.

=head1 SEE ALSO

L<Pod::Text>, L<Pod::Simple>

The current version of this module is always available from its web site at
L<http://www.eyrie.org/~eagle/software/podlators/>.  It is also part of the
Perl core distribution as of 5.6.0.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>.

=head1 COPYRIGHT AND LICENSE

Copyright 1999, 2001, 2004, 2006 by Russ Allbery <rra@stanford.edu>.

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

=cut
