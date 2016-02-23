package URI::QueryParam;

use strict;

sub URI::_query::query_param {
    my $self = shift;
    my @old = $self->query_form;

    if (@_ == 0) {
	# get keys
	my %seen;
	my @keys;
	for (my $i = 0; $i < @old; $i += 2) {
	    push(@keys, $old[$i]) unless $seen{$old[$i]}++;
	}
	return @keys;
    }

    my $key = shift;
    my @i;

    for (my $i = 0; $i < @old; $i += 2) {
	push(@i, $i) if $old[$i] eq $key;
    }

    if (@_) {
	my @new = @old;
	my @new_i = @i;
	my @vals = map { ref($_) eq 'ARRAY' ? @$_ : $_ } @_;
	#print "VALS:@vals [@i]\n";
	while (@new_i > @vals) {
	    #print "REMOVE $new_i[-1]\n";
	    splice(@new, pop(@new_i), 2);
	}
	while (@vals > @new_i) {
	    my $i = @new_i ? $new_i[-1] + 2 : @new;
	    #print "SPLICE $i\n";
	    splice(@new, $i, 0, $key => pop(@vals));
	}
	for (@vals) {
	    #print "SET $new_i[0]\n";
	    $new[shift(@new_i)+1] = $_;
	}

	$self->query_form(\@new);
    }

    return wantarray ? @old[map $_+1, @i] : @i ? $old[$i[0]+1] : undef;
}

sub URI::_query::query_param_append {
    my $self = shift;
    my $key = shift;
    $self->query_form($self->query_form, $key => \@_);  # XXX
    return;
}

sub URI::_query::query_param_delete {
    my $self = shift;
    my $key = shift;
    my @old = $self->query_form;
    my @vals;

    for (my $i = @old - 2; $i >= 0; $i -= 2) {
	next if $old[$i] ne $key;
	push(@vals, (splice(@old, $i, 2))[1]);
    }
    $self->query_form(\@old) if @vals;
    return wantarray ? reverse @vals : $vals[-1];
}

sub URI::_query::query_form_hash {
    my $self = shift;
    my @old = $self->query_form;
    if (@_) {
	$self->query_form(@_ == 1 ? %{shift(@_)} : @_);
    }
    my %hash;
    while (my($k, $v) = splice(@old, 0, 2)) {
	if (exists $hash{$k}) {
	    for ($hash{$k}) {
		$_ = [$_] unless ref($_) eq "ARRAY";
		push(@$_, $v);
	    }
	}
	else {
	    $hash{$k} = $v;
	}
    }
    return \%hash;
}

1;

__END__

=head1 NAME

URI::QueryParam - Additional query methods for URIs

=head1 SYNOPSIS

  use URI;
  use URI::QueryParam;

  $u = URI->new("", "http");
  $u->query_param(foo => 1, 2, 3);
  print $u->query;    # prints foo=1&foo=2&foo=3

  for my $key ($u->query_param) {
      print "$key: ", join(", ", $u->query_param($key)), "\n";
  }

=head1 DESCRIPTION

Loading the C<URI::QueryParam> module adds some extra methods to
URIs that support query methods.  These methods provide an alternative
interface to the $u->query_form data.

The query_param_* methods have deliberately been made identical to the
interface of the corresponding C<CGI.pm> methods.

The following additional methods are made available:

=over

=item @keys = $u->query_param

=item @values = $u->query_param( $key )

=item $first_value = $u->query_param( $key )

=item $u->query_param( $key, $value,... )

If $u->query_param is called with no arguments, it returns all the
distinct parameter keys of the URI.  In a scalar context it returns the
number of distinct keys.

When a $key argument is given, the method returns the parameter values with the
given key.  In a scalar context, only the first parameter value is
returned.

If additional arguments are given, they are used to update successive
parameters with the given key.  If any of the values provided are
array references, then the array is dereferenced to get the actual
values.

=item $u->query_param_append($key, $value,...)

Adds new parameters with the given
key without touching any old parameters with the same key.  It
can be explained as a more efficient version of:

   $u->query_param($key,
                   $u->query_param($key),
                   $value,...);

One difference is that this expression would return the old values
of $key, whereas the query_param_append() method does not.

=item @values = $u->query_param_delete($key)

=item $first_value = $u->query_param_delete($key)

Deletes all key/value pairs with the given key.
The old values are returned.  In a scalar context, only the first value
is returned.

Using the query_param_delete() method is slightly more efficient than
the equivalent:

   $u->query_param($key, []);

=item $hashref = $u->query_form_hash

=item $u->query_form_hash( \%new_form )

Returns a reference to a hash that represents the
query form's key/value pairs.  If a key occurs multiple times, then the hash
value becomes an array reference.

Note that sequence information is lost.  This means that:

   $u->query_form_hash($u->query_form_hash)

is not necessarily a no-op, as it may reorder the key/value pairs.
The values returned by the query_param() method should stay the same
though.

=back

=head1 SEE ALSO

L<URI>, L<CGI>

=head1 COPYRIGHT

Copyright 2002 Gisle Aas.

=cut
