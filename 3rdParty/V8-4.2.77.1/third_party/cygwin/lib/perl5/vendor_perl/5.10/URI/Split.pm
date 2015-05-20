package URI::Split;

use strict;

use vars qw(@ISA @EXPORT_OK);
require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(uri_split uri_join);

use URI::Escape ();

sub uri_split {
     return $_[0] =~ m,(?:([^:/?#]+):)?(?://([^/?#]*))?([^?#]*)(?:\?([^#]*))?(?:#(.*))?,;
}

sub uri_join {
    my($scheme, $auth, $path, $query, $frag) = @_;
    my $uri = defined($scheme) ? "$scheme:" : "";
    $path = "" unless defined $path;
    if (defined $auth) {
	$auth =~ s,([/?\#]), URI::Escape::escape_char($1),eg;
	$uri .= "//$auth";
	$path = "/$path" if length($path) && $path !~ m,^/,;
    }
    elsif ($path =~ m,^//,) {
	$uri .= "//";  # XXX force empty auth
    }
    unless (length $uri) {
	$path =~ s,(:), URI::Escape::escape_char($1),e while $path =~ m,^[^:/?\#]+:,;
    }
    $path =~ s,([?\#]), URI::Escape::escape_char($1),eg;
    $uri .= $path;
    if (defined $query) {
	$query =~ s,(\#), URI::Escape::escape_char($1),eg;
	$uri .= "?$query";
    }
    $uri .= "#$frag" if defined $frag;
    $uri;
}

1;

__END__

=head1 NAME

URI::Split - Parse and compose URI strings

=head1 SYNOPSIS

 use URI::Split qw(uri_split uri_join);
 ($scheme, $auth, $path, $query, $frag) = uri_split($uri);
 $uri = uri_join($scheme, $auth, $path, $query, $frag);

=head1 DESCRIPTION

Provides functions to parse and compose URI
strings.  The following functions are provided:

=over

=item ($scheme, $auth, $path, $query, $frag) = uri_split($uri)

Breaks up a URI string into its component
parts.  An C<undef> value is returned for those parts that are not
present.  The $path part is always present (but can be the empty
string) and is thus never returned as C<undef>.

No sensible value is returned if this function is called in a scalar
context.

=item $uri = uri_join($scheme, $auth, $path, $query, $frag)

Puts together a URI string from its parts.
Missing parts are signaled by passing C<undef> for the corresponding
argument.

Minimal escaping is applied to parts that contain reserved chars
that would confuse a parser.  For instance, any occurrence of '?' or '#'
in $path is always escaped, as it would otherwise be parsed back
as a query or fragment.

=back

=head1 SEE ALSO

L<URI>, L<URI::Escape>

=head1 COPYRIGHT

Copyright 2003, Gisle Aas

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
