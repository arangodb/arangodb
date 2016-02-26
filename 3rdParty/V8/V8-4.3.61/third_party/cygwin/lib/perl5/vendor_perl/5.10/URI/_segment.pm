package URI::_segment;

# Represents a generic path_segment so that it can be treated as
# a string too.

use strict;
use URI::Escape qw(uri_unescape);

use overload '""' => sub { $_[0]->[0] },
             fallback => 1;

sub new
{
    my $class = shift;
    my @segment = split(';', shift, -1);
    $segment[0] = uri_unescape($segment[0]);
    bless \@segment, $class;
}

1;
