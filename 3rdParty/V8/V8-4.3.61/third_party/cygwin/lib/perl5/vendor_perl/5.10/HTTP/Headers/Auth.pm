package HTTP::Headers::Auth;

use strict;
use vars qw($VERSION);
$VERSION = "5.810";

use HTTP::Headers;

package HTTP::Headers;

BEGIN {
    # we provide a new (and better) implementations below
    undef(&www_authenticate);
    undef(&proxy_authenticate);
}

require HTTP::Headers::Util;

sub _parse_authenticate
{
    my @ret;
    for (HTTP::Headers::Util::split_header_words(@_)) {
	if (!defined($_->[1])) {
	    # this is a new auth scheme
	    push(@ret, lc(shift @$_) => {});
	    shift @$_;
	}
	if (@ret) {
	    # this a new parameter pair for the last auth scheme
	    while (@$_) {
		my $k = lc(shift @$_);
		my $v = shift @$_;
	        $ret[-1]{$k} = $v;
	    }
	}
	else {
	    # something wrong, parameter pair without any scheme seen
	    # IGNORE
	}
    }
    @ret;
}

sub _authenticate
{
    my $self = shift;
    my $header = shift;
    my @old = $self->_header($header);
    if (@_) {
	$self->remove_header($header);
	my @new = @_;
	while (@new) {
	    my $a_scheme = shift(@new);
	    if ($a_scheme =~ /\s/) {
		# assume complete valid value, pass it through
		$self->push_header($header, $a_scheme);
	    }
	    else {
		my @param;
		if (@new) {
		    my $p = $new[0];
		    if (ref($p) eq "ARRAY") {
			@param = @$p;
			shift(@new);
		    }
		    elsif (ref($p) eq "HASH") {
			@param = %$p;
			shift(@new);
		    }
		}
		my $val = ucfirst(lc($a_scheme));
		if (@param) {
		    my $sep = " ";
		    while (@param) {
			my $k = shift @param;
			my $v = shift @param;
			if ($v =~ /[^0-9a-zA-Z]/ || lc($k) eq "realm") {
			    # must quote the value
			    $v =~ s,([\\\"]),\\$1,g;
			    $v = qq("$v");
			}
			$val .= "$sep$k=$v";
			$sep = ", ";
		    }
		}
		$self->push_header($header, $val);
	    }
	}
    }
    return unless defined wantarray;
    wantarray ? _parse_authenticate(@old) : join(", ", @old);
}


sub www_authenticate    { shift->_authenticate("WWW-Authenticate", @_)   }
sub proxy_authenticate  { shift->_authenticate("Proxy-Authenticate", @_) }

1;
