package HTTP::Headers::Util;

use strict;
use vars qw($VERSION @ISA @EXPORT_OK);

$VERSION = "5.810";

require Exporter;
@ISA=qw(Exporter);

@EXPORT_OK=qw(split_header_words join_header_words);



sub split_header_words
{
    my(@val) = @_;
    my @res;
    for (@val) {
	my @cur;
	while (length) {
	    if (s/^\s*(=*[^\s=;,]+)//) {  # 'token' or parameter 'attribute'
		push(@cur, $1);
		# a quoted value
		if (s/^\s*=\s*\"([^\"\\]*(?:\\.[^\"\\]*)*)\"//) {
		    my $val = $1;
		    $val =~ s/\\(.)/$1/g;
		    push(@cur, $val);
		# some unquoted value
		}
		elsif (s/^\s*=\s*([^;,\s]*)//) {
		    my $val = $1;
		    $val =~ s/\s+$//;
		    push(@cur, $val);
		# no value, a lone token
		}
		else {
		    push(@cur, undef);
		}
	    }
	    elsif (s/^\s*,//) {
		push(@res, [@cur]) if @cur;
		@cur = ();
	    }
	    elsif (s/^\s*;// || s/^\s+//) {
		# continue
	    }
	    else {
		die "This should not happen: '$_'";
	    }
	}
	push(@res, \@cur) if @cur;
    }
    @res;
}


sub join_header_words
{
    @_ = ([@_]) if @_ && !ref($_[0]);
    my @res;
    for (@_) {
	my @cur = @$_;
	my @attr;
	while (@cur) {
	    my $k = shift @cur;
	    my $v = shift @cur;
	    if (defined $v) {
		if ($v =~ /[\x00-\x20()<>@,;:\\\"\/\[\]?={}\x7F-\xFF]/ || !length($v)) {
		    $v =~ s/([\"\\])/\\$1/g;  # escape " and \
		    $k .= qq(="$v");
		}
		else {
		    # token
		    $k .= "=$v";
		}
	    }
	    push(@attr, $k);
	}
	push(@res, join("; ", @attr)) if @attr;
    }
    join(", ", @res);
}


1;

__END__

=head1 NAME

HTTP::Headers::Util - Header value parsing utility functions

=head1 SYNOPSIS

  use HTTP::Headers::Util qw(split_header_words);
  @values = split_header_words($h->header("Content-Type"));

=head1 DESCRIPTION

This module provides a few functions that helps parsing and
construction of valid HTTP header values.  None of the functions are
exported by default.

The following functions are available:

=over 4


=item split_header_words( @header_values )

This function will parse the header values given as argument into a
list of anonymous arrays containing key/value pairs.  The function
knows how to deal with ",", ";" and "=" as well as quoted values after
"=".  A list of space separated tokens are parsed as if they were
separated by ";".

If the @header_values passed as argument contains multiple values,
then they are treated as if they were a single value separated by
comma ",".

This means that this function is useful for parsing header fields that
follow this syntax (BNF as from the HTTP/1.1 specification, but we relax
the requirement for tokens).

  headers           = #header
  header            = (token | parameter) *( [";"] (token | parameter))

  token             = 1*<any CHAR except CTLs or separators>
  separators        = "(" | ")" | "<" | ">" | "@"
                    | "," | ";" | ":" | "\" | <">
                    | "/" | "[" | "]" | "?" | "="
                    | "{" | "}" | SP | HT

  quoted-string     = ( <"> *(qdtext | quoted-pair ) <"> )
  qdtext            = <any TEXT except <">>
  quoted-pair       = "\" CHAR

  parameter         = attribute "=" value
  attribute         = token
  value             = token | quoted-string

Each I<header> is represented by an anonymous array of key/value
pairs.  The value for a simple token (not part of a parameter) is C<undef>.
Syntactically incorrect headers will not necessary be parsed as you
would want.

This is easier to describe with some examples:

   split_header_words('foo="bar"; port="80,81"; discard, bar=baz');
   split_header_words('text/html; charset="iso-8859-1"');
   split_header_words('Basic realm="\\"foo\\\\bar\\""');

will return

   [foo=>'bar', port=>'80,81', discard=> undef], [bar=>'baz' ]
   ['text/html' => undef, charset => 'iso-8859-1']
   [Basic => undef, realm => "\"foo\\bar\""]

=item join_header_words( @arrays )

This will do the opposite of the conversion done by split_header_words().
It takes a list of anonymous arrays as arguments (or a list of
key/value pairs) and produces a single header value.  Attribute values
are quoted if needed.

Example:

   join_header_words(["text/plain" => undef, charset => "iso-8859/1"]);
   join_header_words("text/plain" => undef, charset => "iso-8859/1");

will both return the string:

   text/plain; charset="iso-8859/1"

=back

=head1 COPYRIGHT

Copyright 1997-1998, Gisle Aas

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

