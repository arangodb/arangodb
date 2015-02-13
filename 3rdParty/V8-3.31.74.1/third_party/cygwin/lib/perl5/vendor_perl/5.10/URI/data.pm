package URI::data;  # RFC 2397

require URI;
@ISA=qw(URI);

use strict;

use MIME::Base64 qw(encode_base64 decode_base64);
use URI::Escape  qw(uri_unescape);

sub media_type
{
    my $self = shift;
    my $opaque = $self->opaque;
    $opaque =~ /^([^,]*),?/ or die;
    my $old = $1;
    my $base64;
    $base64 = $1 if $old =~ s/(;base64)$//i;
    if (@_) {
	my $new = shift;
	$new = "" unless defined $new;
	$new =~ s/%/%25/g;
	$new =~ s/,/%2C/g;
	$base64 = "" unless defined $base64;
	$opaque =~ s/^[^,]*,?/$new$base64,/;
	$self->opaque($opaque);
    }
    return uri_unescape($old) if $old;  # media_type can't really be "0"
    "text/plain;charset=US-ASCII";      # default type
}

sub data
{
    my $self = shift;
    my($enc, $data) = split(",", $self->opaque, 2);
    unless (defined $data) {
	$data = "";
	$enc  = "" unless defined $enc;
    }
    my $base64 = ($enc =~ /;base64$/i);
    if (@_) {
	$enc =~ s/;base64$//i if $base64;
	my $new = shift;
	$new = "" unless defined $new;
	my $uric_count = _uric_count($new);
	my $urienc_len = $uric_count + (length($new) - $uric_count) * 3;
	my $base64_len = int((length($new)+2) / 3) * 4;
	$base64_len += 7;  # because of ";base64" marker
	if ($base64_len < $urienc_len || $_[0]) {
	    $enc .= ";base64";
	    $new = encode_base64($new, "");
	} else {
	    $new =~ s/%/%25/g;
	}
	$self->opaque("$enc,$new");
    }
    return unless defined wantarray;
    $data = uri_unescape($data);
    return $base64 ? decode_base64($data) : $data;
}

# I could not find a better way to interpolate the tr/// chars from
# a variable.
my $ENC = $URI::uric;
$ENC =~ s/%//;

eval <<EOT; die $@ if $@;
sub _uric_count
{
    \$_[0] =~ tr/$ENC//;
}
EOT

1;

__END__

=head1 NAME

URI::data - URI that contains immediate data

=head1 SYNOPSIS

 use URI;

 $u = URI->new("data:");
 $u->media_type("image/gif");
 $u->data(scalar(`cat camel.gif`));
 print "$u\n";
 open(XV, "|xv -") and print XV $u->data;

=head1 DESCRIPTION

The C<URI::data> class supports C<URI> objects belonging to the I<data>
URI scheme.  The I<data> URI scheme is specified in RFC 2397.  It
allows inclusion of small data items as "immediate" data, as if it had
been included externally.  Examples:

  data:,Perl%20is%20good

  data:image/gif;base64,R0lGODdhIAAgAIAAAAAAAPj8+CwAAAAAI
    AAgAAAClYyPqcu9AJyCjtIKc5w5xP14xgeO2tlY3nWcajmZZdeJcG
    Kxrmimms1KMTa1Wg8UROx4MNUq1HrycMjHT9b6xKxaFLM6VRKzI+p
    KS9XtXpcbdun6uWVxJXA8pNPkdkkxhxc21LZHFOgD2KMoQXa2KMWI
    JtnE2KizVUkYJVZZ1nczBxXlFopZBtoJ2diXGdNUymmJdFMAADs=



C<URI> objects belonging to the data scheme support the common methods
(described in L<URI>) and the following two scheme-specific methods:

=over 4

=item $uri->media_type( [$new_media_type] )

Can be used to get or set the media type specified in the
URI.  If no media type is specified, then the default
C<"text/plain;charset=US-ASCII"> is returned.

=item $uri->data( [$new_data] )

Can be used to get or set the data contained in the URI.
The data is passed unescaped (in binary form).  The decision about
whether to base64 encode the data in the URI is taken automatically,
based on the encoding that produces the shorter URI string.

=back

=head1 SEE ALSO

L<URI>

=head1 COPYRIGHT

Copyright 1995-1998 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
