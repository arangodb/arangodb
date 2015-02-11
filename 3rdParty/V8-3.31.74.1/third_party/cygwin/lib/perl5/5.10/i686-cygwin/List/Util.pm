# List::Util.pm
#
# Copyright (c) 1997-2006 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package List::Util;

use strict;
use vars qw(@ISA @EXPORT_OK $VERSION $XS_VERSION $TESTING_PERL_ONLY);
require Exporter;

@ISA        = qw(Exporter);
@EXPORT_OK  = qw(first min max minstr maxstr reduce sum shuffle);
$VERSION    = "1.19";
$XS_VERSION = $VERSION;
$VERSION    = eval $VERSION;

eval {
  # PERL_DL_NONLAZY must be false, or any errors in loading will just
  # cause the perl code to be tested
  local $ENV{PERL_DL_NONLAZY} = 0 if $ENV{PERL_DL_NONLAZY};
  eval {
    require XSLoader;
    XSLoader::load('List::Util', $XS_VERSION);
    1;
  } or do {
    require DynaLoader;
    local @ISA = qw(DynaLoader);
    bootstrap List::Util $XS_VERSION;
  };
} unless $TESTING_PERL_ONLY;


# This code is only compiled if the XS did not load
# of for perl < 5.6.0

if (!defined &reduce) {
eval <<'ESQ' 

sub reduce (&@) {
  my $code = shift;
  no strict 'refs';

  return shift unless @_ > 1;

  use vars qw($a $b);

  my $caller = caller;
  local(*{$caller."::a"}) = \my $a;
  local(*{$caller."::b"}) = \my $b;

  $a = shift;
  foreach (@_) {
    $b = $_;
    $a = &{$code}();
  }

  $a;
}

sub first (&@) {
  my $code = shift;

  foreach (@_) {
    return $_ if &{$code}();
  }

  undef;
}

ESQ
}

# This code is only compiled if the XS did not load
eval <<'ESQ' if !defined &sum;

use vars qw($a $b);

sub sum (@) { reduce { $a + $b } @_ }

sub min (@) { reduce { $a < $b ? $a : $b } @_ }

sub max (@) { reduce { $a > $b ? $a : $b } @_ }

sub minstr (@) { reduce { $a lt $b ? $a : $b } @_ }

sub maxstr (@) { reduce { $a gt $b ? $a : $b } @_ }

sub shuffle (@) {
  my @a=\(@_);
  my $n;
  my $i=@_;
  map {
    $n = rand($i--);
    (${$a[$n]}, $a[$n] = $a[$i])[0];
  } @_;
}

ESQ

1;

__END__

=head1 NAME

List::Util - A selection of general-utility list subroutines

=head1 SYNOPSIS

    use List::Util qw(first max maxstr min minstr reduce shuffle sum);

=head1 DESCRIPTION

C<List::Util> contains a selection of subroutines that people have
expressed would be nice to have in the perl core, but the usage would
not really be high enough to warrant the use of a keyword, and the size
so small such that being individual extensions would be wasteful.

By default C<List::Util> does not export any subroutines. The
subroutines defined are

=over 4

=item first BLOCK LIST

Similar to C<grep> in that it evaluates BLOCK setting C<$_> to each element
of LIST in turn. C<first> returns the first element where the result from
BLOCK is a true value. If BLOCK never returns true or LIST was empty then
C<undef> is returned.

    $foo = first { defined($_) } @list    # first defined value in @list
    $foo = first { $_ > $value } @list    # first value in @list which
                                          # is greater than $value

This function could be implemented using C<reduce> like this

    $foo = reduce { defined($a) ? $a : wanted($b) ? $b : undef } undef, @list

for example wanted() could be defined() which would return the first
defined value in @list

=item max LIST

Returns the entry in the list with the highest numerical value. If the
list is empty then C<undef> is returned.

    $foo = max 1..10                # 10
    $foo = max 3,9,12               # 12
    $foo = max @bar, @baz           # whatever

This function could be implemented using C<reduce> like this

    $foo = reduce { $a > $b ? $a : $b } 1..10

=item maxstr LIST

Similar to C<max>, but treats all the entries in the list as strings
and returns the highest string as defined by the C<gt> operator.
If the list is empty then C<undef> is returned.

    $foo = maxstr 'A'..'Z'          # 'Z'
    $foo = maxstr "hello","world"   # "world"
    $foo = maxstr @bar, @baz        # whatever

This function could be implemented using C<reduce> like this

    $foo = reduce { $a gt $b ? $a : $b } 'A'..'Z'

=item min LIST

Similar to C<max> but returns the entry in the list with the lowest
numerical value. If the list is empty then C<undef> is returned.

    $foo = min 1..10                # 1
    $foo = min 3,9,12               # 3
    $foo = min @bar, @baz           # whatever

This function could be implemented using C<reduce> like this

    $foo = reduce { $a < $b ? $a : $b } 1..10

=item minstr LIST

Similar to C<min>, but treats all the entries in the list as strings
and returns the lowest string as defined by the C<lt> operator.
If the list is empty then C<undef> is returned.

    $foo = minstr 'A'..'Z'          # 'A'
    $foo = minstr "hello","world"   # "hello"
    $foo = minstr @bar, @baz        # whatever

This function could be implemented using C<reduce> like this

    $foo = reduce { $a lt $b ? $a : $b } 'A'..'Z'

=item reduce BLOCK LIST

Reduces LIST by calling BLOCK, in a scalar context, multiple times,
setting C<$a> and C<$b> each time. The first call will be with C<$a>
and C<$b> set to the first two elements of the list, subsequent
calls will be done by setting C<$a> to the result of the previous
call and C<$b> to the next element in the list.

Returns the result of the last call to BLOCK. If LIST is empty then
C<undef> is returned. If LIST only contains one element then that
element is returned and BLOCK is not executed.

    $foo = reduce { $a < $b ? $a : $b } 1..10       # min
    $foo = reduce { $a lt $b ? $a : $b } 'aa'..'zz' # minstr
    $foo = reduce { $a + $b } 1 .. 10               # sum
    $foo = reduce { $a . $b } @bar                  # concat

=item shuffle LIST

Returns the elements of LIST in a random order

    @cards = shuffle 0..51      # 0..51 in a random order

=item sum LIST

Returns the sum of all the elements in LIST. If LIST is empty then
C<undef> is returned.

    $foo = sum 1..10                # 55
    $foo = sum 3,9,12               # 24
    $foo = sum @bar, @baz           # whatever

This function could be implemented using C<reduce> like this

    $foo = reduce { $a + $b } 1..10

=back

=head1 KNOWN BUGS

With perl versions prior to 5.005 there are some cases where reduce
will return an incorrect result. This will show up as test 7 of
reduce.t failing.

=head1 SUGGESTED ADDITIONS

The following are additions that have been requested, but I have been reluctant
to add due to them being very simple to implement in perl

  # One argument is true

  sub any { $_ && return 1 for @_; 0 }

  # All arguments are true

  sub all { $_ || return 0 for @_; 1 }

  # All arguments are false

  sub none { $_ && return 0 for @_; 1 }

  # One argument is false

  sub notall { $_ || return 1 for @_; 0 }

  # How many elements are true

  sub true { scalar grep { $_ } @_ }

  # How many elements are false

  sub false { scalar grep { !$_ } @_ }

=head1 SEE ALSO

L<Scalar::Util>, L<List::MoreUtils>

=head1 COPYRIGHT

Copyright (c) 1997-2006 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
