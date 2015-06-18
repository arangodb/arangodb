# Pod::ParseLink -- Parse an L<> formatting code in POD text.
# $Id: ParseLink.pm,v 1.6 2002/07/15 05:46:00 eagle Exp $
#
# Copyright 2001 by Russ Allbery <rra@stanford.edu>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# This module implements parsing of the text of an L<> formatting code as
# defined in perlpodspec.  It should be suitable for any POD formatter.  It
# exports only one function, parselink(), which returns the five-item parse
# defined in perlpodspec.
#
# Perl core hackers, please note that this module is also separately
# maintained outside of the Perl core as part of the podlators.  Please send
# me any patches at the address above in addition to sending them to the
# standard Perl mailing lists.

##############################################################################
# Modules and declarations
##############################################################################

package Pod::ParseLink;

require 5.004;

use strict;
use vars qw(@EXPORT @ISA $VERSION);

use Exporter;
@ISA    = qw(Exporter);
@EXPORT = qw(parselink);

# Don't use the CVS revision as the version, since this module is also in Perl
# core and too many things could munge CVS magic revision strings.  This
# number should ideally be the same as the CVS revision in podlators, however.
$VERSION = 1.06;


##############################################################################
# Implementation
##############################################################################

# Parse the name and section portion of a link into a name and section.
sub _parse_section {
    my ($link) = @_;
    $link =~ s/^\s+//;
    $link =~ s/\s+$//;

    # If the whole link is enclosed in quotes, interpret it all as a section
    # even if it contains a slash.
    return (undef, $1) if ($link =~ /^"\s*(.*?)\s*"$/);

    # Split into page and section on slash, and then clean up quoting in the
    # section.  If there is no section and the name contains spaces, also
    # guess that it's an old section link.
    my ($page, $section) = split (/\s*\/\s*/, $link, 2);
    $section =~ s/^"\s*(.*?)\s*"$/$1/ if $section;
    if ($page && $page =~ / / && !defined ($section)) {
        $section = $page;
        $page = undef;
    } else {
        $page = undef unless $page;
        $section = undef unless $section;
    }
    return ($page, $section);
}

# Infer link text from the page and section.
sub _infer_text {
    my ($page, $section) = @_;
    my $inferred;
    if ($page && !$section) {
        $inferred = $page;
    } elsif (!$page && $section) {
        $inferred = '"' . $section . '"';
    } elsif ($page && $section) {
        $inferred = '"' . $section . '" in ' . $page;
    }
    return $inferred;
}

# Given the contents of an L<> formatting code, parse it into the link text,
# the possibly inferred link text, the name or URL, the section, and the type
# of link (pod, man, or url).
sub parselink {
    my ($link) = @_;
    $link =~ s/\s+/ /g;
    if ($link =~ /\A\w+:[^:\s]\S*\Z/) {
        return (undef, $link, $link, undef, 'url');
    } else {
        my $text;
        if ($link =~ /\|/) {
            ($text, $link) = split (/\|/, $link, 2);
        }
        my ($name, $section) = _parse_section ($link);
        my $inferred = $text || _infer_text ($name, $section);
        my $type = ($name && $name =~ /\(\S*\)/) ? 'man' : 'pod';
        return ($text, $inferred, $name, $section, $type);
    }
}


##############################################################################
# Module return value and documentation
##############################################################################

# Ensure we evaluate to true.
1;
__END__

=head1 NAME

Pod::ParseLink - Parse an LE<lt>E<gt> formatting code in POD text

=head1 SYNOPSIS

    use Pod::ParseLink;
    my ($text, $inferred, $name, $section, $type) = parselink ($link);

=head1 DESCRIPTION

This module only provides a single function, parselink(), which takes the
text of an LE<lt>E<gt> formatting code and parses it.  It returns the anchor
text for the link (if any was given), the anchor text possibly inferred from
the name and section, the name or URL, the section if any, and the type of
link.  The type will be one of 'url', 'pod', or 'man', indicating a URL, a
link to a POD page, or a link to a Unix manual page.

Parsing is implemented per L<perlpodspec>.  For backward compatibility,
links where there is no section and name contains spaces, or links where the
entirety of the link (except for the anchor text if given) is enclosed in
double-quotes are interpreted as links to a section (LE<lt>/sectionE<gt>).

The inferred anchor text is implemented per L<perlpodspec>:

    L<name>         =>  L<name|name>
    L</section>     =>  L<"section"|/section>
    L<name/section> =>  L<"section" in name|name/section>

The name may contain embedded EE<lt>E<gt> and ZE<lt>E<gt> formatting codes,
and the section, anchor text, and inferred anchor text may contain any
formatting codes.  Any double quotes around the section are removed as part
of the parsing, as is any leading or trailing whitespace.

If the text of the LE<lt>E<gt> escape is entirely enclosed in double quotes,
it's interpreted as a link to a section for backwards compatibility.

No attempt is made to resolve formatting codes.  This must be done after
calling parselink (since EE<lt>E<gt> formatting codes can be used to escape
characters that would otherwise be significant to the parser and resolving
them before parsing would result in an incorrect parse of a formatting code
like:

    L<verticalE<verbar>barE<sol>slash>

which should be interpreted as a link to the C<vertical|bar/slash> POD page
and not as a link to the C<slash> section of the C<bar> POD page with an
anchor text of C<vertical>.  Note that not only the anchor text will need to
have formatting codes expanded, but so will the target of the link (to deal
with EE<lt>E<gt> and ZE<lt>E<gt> formatting codes), and special handling of
the section may be necessary depending on whether the translator wants to
consider markup in sections to be significant when resolving links.  See
L<perlpodspec> for more information.

=head1 SEE ALSO

L<Pod::Parser>

The current version of this module is always available from its web site at
L<http://www.eyrie.org/~eagle/software/podlators/>.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>.

=head1 COPYRIGHT AND LICENSE

Copyright 2001 by Russ Allbery <rra@stanford.edu>.

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

=cut
