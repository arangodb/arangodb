package URI::mailto;  # RFC 2368

require URI;
require URI::_query;
@ISA=qw(URI URI::_query);

use strict;

sub to
{
    my $self = shift;
    my @old = $self->headers;
    if (@_) {
	my @new = @old;
	# get rid of any other to: fields
	for (my $i = 0; $i < @new; $i += 2) {
	    if (lc($new[$i] || '') eq "to") {
		splice(@new, $i, 2);
		redo;
	    }
	}

	my $to = shift;
	$to = "" unless defined $to;
	unshift(@new, "to" => $to);
	$self->headers(@new);
    }
    return unless defined wantarray;

    my @to;
    while (@old) {
	my $h = shift @old;
	my $v = shift @old;
	push(@to, $v) if lc($h) eq "to";
    }
    join(",", @to);
}


sub headers
{
    my $self = shift;

    # The trick is to just treat everything as the query string...
    my $opaque = "to=" . $self->opaque;
    $opaque =~ s/\?/&/;

    if (@_) {
	my @new = @_;

	# strip out any "to" fields
	my @to;
	for (my $i=0; $i < @new; $i += 2) {
	    if (lc($new[$i] || '') eq "to") {
		push(@to, (splice(@new, $i, 2))[1]);  # remove header
		redo;
	    }
	}

	my $new = join(",",@to);
	$new =~ s/%/%25/g;
	$new =~ s/\?/%3F/g;
	$self->opaque($new);
	$self->query_form(@new) if @new;
    }
    return unless defined wantarray;

    # I am lazy today...
    URI->new("mailto:?$opaque")->query_form;
}

1;
