package URI::_query;

use strict;
use URI ();
use URI::Escape qw(uri_unescape);

sub query
{
    my $self = shift;
    $$self =~ m,^([^?\#]*)(?:\?([^\#]*))?(.*)$,s or die;

    if (@_) {
	my $q = shift;
	$$self = $1;
	if (defined $q) {
	    $q =~ s/([^$URI::uric])/ URI::Escape::escape_char($1)/ego;
	    $$self .= "?$q";
	}
	$$self .= $3;
    }
    $2;
}

# Handle ...?foo=bar&bar=foo type of query
sub query_form {
    my $self = shift;
    my $old = $self->query;
    if (@_) {
        # Try to set query string
	my @new = @_;
	if (@new == 1) {
	    my $n = $new[0];
	    if (ref($n) eq "ARRAY") {
		@new = @$n;
	    }
	    elsif (ref($n) eq "HASH") {
		@new = %$n;
	    }
	}
        my @query;
        while (my($key,$vals) = splice(@new, 0, 2)) {
            $key = '' unless defined $key;
	    $key =~ s/([;\/?:@&=+,\$\[\]%])/ URI::Escape::escape_char($1)/eg;
	    $key =~ s/ /+/g;
	    $vals = [ref($vals) eq "ARRAY" ? @$vals : $vals];
            for my $val (@$vals) {
                $val = '' unless defined $val;
		$val =~ s/([;\/?:@&=+,\$\[\]%])/ URI::Escape::escape_char($1)/eg;
                $val =~ s/ /+/g;
                push(@query, "$key=$val");
            }
        }
        $self->query(@query ? join('&', @query) : undef);
    }
    return if !defined($old) || !length($old) || !defined(wantarray);
    return unless $old =~ /=/; # not a form
    map { s/\+/ /g; uri_unescape($_) }
         map { /=/ ? split(/=/, $_, 2) : ($_ => '')} split(/&/, $old);
}

# Handle ...?dog+bones type of query
sub query_keywords
{
    my $self = shift;
    my $old = $self->query;
    if (@_) {
        # Try to set query string
	my @copy = @_;
	@copy = @{$copy[0]} if @copy == 1 && ref($copy[0]) eq "ARRAY";
	for (@copy) { s/([;\/?:@&=+,\$\[\]%])/ URI::Escape::escape_char($1)/eg; }
	$self->query(@copy ? join('+', @copy) : undef);
    }
    return if !defined($old) || !defined(wantarray);
    return if $old =~ /=/;  # not keywords, but a form
    map { uri_unescape($_) } split(/\+/, $old, -1);
}

# Some URI::URL compatibility stuff
*equery = \&query;

1;
