package Tie::Array;

use 5.006_001;
use strict;
use Carp;
our $VERSION = '1.03';

# Pod documentation after __END__ below.

sub DESTROY { }
sub EXTEND  { }
sub UNSHIFT { scalar shift->SPLICE(0,0,@_) }
sub SHIFT { shift->SPLICE(0,1) }
sub CLEAR   { shift->STORESIZE(0) }

sub PUSH
{
 my $obj = shift;
 my $i   = $obj->FETCHSIZE;
 $obj->STORE($i++, shift) while (@_);
}

sub POP
{
 my $obj = shift;
 my $newsize = $obj->FETCHSIZE - 1;
 my $val;
 if ($newsize >= 0)
  {
   $val = $obj->FETCH($newsize);
   $obj->STORESIZE($newsize);
  }
 $val;
}

sub SPLICE {
    my $obj = shift;
    my $sz  = $obj->FETCHSIZE;
    my $off = (@_) ? shift : 0;
    $off += $sz if ($off < 0);
    my $len = (@_) ? shift : $sz - $off;
    $len += $sz - $off if $len < 0;
    my @result;
    for (my $i = 0; $i < $len; $i++) {
        push(@result,$obj->FETCH($off+$i));
    }
    $off = $sz if $off > $sz;
    $len -= $off + $len - $sz if $off + $len > $sz;
    if (@_ > $len) {
        # Move items up to make room
        my $d = @_ - $len;
        my $e = $off+$len;
        $obj->EXTEND($sz+$d);
        for (my $i=$sz-1; $i >= $e; $i--) {
            my $val = $obj->FETCH($i);
            $obj->STORE($i+$d,$val);
        }
    }
    elsif (@_ < $len) {
        # Move items down to close the gap
        my $d = $len - @_;
        my $e = $off+$len;
        for (my $i=$off+$len; $i < $sz; $i++) {
            my $val = $obj->FETCH($i);
            $obj->STORE($i-$d,$val);
        }
        $obj->STORESIZE($sz-$d);
    }
    for (my $i=0; $i < @_; $i++) {
        $obj->STORE($off+$i,$_[$i]);
    }
    return wantarray ? @result : pop @result;
}

sub EXISTS {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define an EXISTS method";
}

sub DELETE {
    my $pkg = ref $_[0];
    croak "$pkg doesn't define a DELETE method";
}

package Tie::StdArray;
use vars qw(@ISA);
@ISA = 'Tie::Array';

sub TIEARRAY  { bless [], $_[0] }
sub FETCHSIZE { scalar @{$_[0]} }
sub STORESIZE { $#{$_[0]} = $_[1]-1 }
sub STORE     { $_[0]->[$_[1]] = $_[2] }
sub FETCH     { $_[0]->[$_[1]] }
sub CLEAR     { @{$_[0]} = () }
sub POP       { pop(@{$_[0]}) }
sub PUSH      { my $o = shift; push(@$o,@_) }
sub SHIFT     { shift(@{$_[0]}) }
sub UNSHIFT   { my $o = shift; unshift(@$o,@_) }
sub EXISTS    { exists $_[0]->[$_[1]] }
sub DELETE    { delete $_[0]->[$_[1]] }

sub SPLICE
{
 my $ob  = shift;
 my $sz  = $ob->FETCHSIZE;
 my $off = @_ ? shift : 0;
 $off   += $sz if $off < 0;
 my $len = @_ ? shift : $sz-$off;
 return splice(@$ob,$off,$len,@_);
}

1;

__END__

=head1 NAME

Tie::Array - base class for tied arrays

=head1 SYNOPSIS

    package Tie::NewArray;
    use Tie::Array;
    @ISA = ('Tie::Array');

    # mandatory methods
    sub TIEARRAY { ... }
    sub FETCH { ... }
    sub FETCHSIZE { ... }

    sub STORE { ... }        # mandatory if elements writeable
    sub STORESIZE { ... }    # mandatory if elements can be added/deleted
    sub EXISTS { ... }       # mandatory if exists() expected to work
    sub DELETE { ... }       # mandatory if delete() expected to work

    # optional methods - for efficiency
    sub CLEAR { ... }
    sub PUSH { ... }
    sub POP { ... }
    sub SHIFT { ... }
    sub UNSHIFT { ... }
    sub SPLICE { ... }
    sub EXTEND { ... }
    sub DESTROY { ... }

    package Tie::NewStdArray;
    use Tie::Array;

    @ISA = ('Tie::StdArray');

    # all methods provided by default

    package main;

    $object = tie @somearray,Tie::NewArray;
    $object = tie @somearray,Tie::StdArray;
    $object = tie @somearray,Tie::NewStdArray;



=head1 DESCRIPTION

This module provides methods for array-tying classes. See
L<perltie> for a list of the functions required in order to tie an array
to a package. The basic B<Tie::Array> package provides stub C<DESTROY>,
and C<EXTEND> methods that do nothing, stub C<DELETE> and C<EXISTS>
methods that croak() if the delete() or exists() builtins are ever called
on the tied array, and implementations of C<PUSH>, C<POP>, C<SHIFT>,
C<UNSHIFT>, C<SPLICE> and C<CLEAR> in terms of basic C<FETCH>, C<STORE>,
C<FETCHSIZE>, C<STORESIZE>.

The B<Tie::StdArray> package provides efficient methods required for tied arrays
which are implemented as blessed references to an "inner" perl array.
It inherits from B<Tie::Array>, and should cause tied arrays to behave exactly
like standard arrays, allowing for selective overloading of methods.

For developers wishing to write their own tied arrays, the required methods
are briefly defined below. See the L<perltie> section for more detailed
descriptive, as well as example code:

=over 4

=item TIEARRAY classname, LIST

The class method is invoked by the command C<tie @array, classname>. Associates
an array instance with the specified class. C<LIST> would represent
additional arguments (along the lines of L<AnyDBM_File> and compatriots) needed
to complete the association. The method should return an object of a class which
provides the methods below.

=item STORE this, index, value

Store datum I<value> into I<index> for the tied array associated with
object I<this>. If this makes the array larger then
class's mapping of C<undef> should be returned for new positions.

=item FETCH this, index

Retrieve the datum in I<index> for the tied array associated with
object I<this>.

=item FETCHSIZE this

Returns the total number of items in the tied array associated with
object I<this>. (Equivalent to C<scalar(@array)>).

=item STORESIZE this, count

Sets the total number of items in the tied array associated with
object I<this> to be I<count>. If this makes the array larger then
class's mapping of C<undef> should be returned for new positions.
If the array becomes smaller then entries beyond count should be
deleted.

=item EXTEND this, count

Informative call that array is likely to grow to have I<count> entries.
Can be used to optimize allocation. This method need do nothing.

=item EXISTS this, key

Verify that the element at index I<key> exists in the tied array I<this>.

The B<Tie::Array> implementation is a stub that simply croaks.

=item DELETE this, key

Delete the element at index I<key> from the tied array I<this>.

The B<Tie::Array> implementation is a stub that simply croaks.

=item CLEAR this

Clear (remove, delete, ...) all values from the tied array associated with
object I<this>.

=item DESTROY this

Normal object destructor method.

=item PUSH this, LIST

Append elements of LIST to the array.

=item POP this

Remove last element of the array and return it.

=item SHIFT this

Remove the first element of the array (shifting other elements down)
and return it.

=item UNSHIFT this, LIST

Insert LIST elements at the beginning of the array, moving existing elements
up to make room.

=item SPLICE this, offset, length, LIST

Perform the equivalent of C<splice> on the array.

I<offset> is optional and defaults to zero, negative values count back
from the end of the array.

I<length> is optional and defaults to rest of the array.

I<LIST> may be empty.

Returns a list of the original I<length> elements at I<offset>.

=back

=head1 CAVEATS

There is no support at present for tied @ISA. There is a potential conflict
between magic entries needed to notice setting of @ISA, and those needed to
implement 'tie'.

Very little consideration has been given to the behaviour of tied arrays
when C<$[> is not default value of zero.

=head1 AUTHOR

Nick Ing-Simmons E<lt>nik@tiuk.ti.comE<gt>

=cut
