
require 5;
package Pod::Perldoc::BaseTo;
use strict;
use warnings;

sub is_pageable        { '' }
sub write_with_binmode {  1 }

sub output_extension   { 'txt' }  # override in subclass!

# sub new { my $self = shift; ...  }
# sub parse_from_file( my($class, $in, $out) = ...; ... }

#sub new { return bless {}, ref($_[0]) || $_[0] }

sub _perldoc_elem {
  my($self, $name) = splice @_,0,2;
  if(@_) {
    $self->{$name} = $_[0];
  } else {
    $self->{$name};
  }
}


1;

