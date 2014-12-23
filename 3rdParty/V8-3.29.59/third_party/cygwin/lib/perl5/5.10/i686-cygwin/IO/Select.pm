# IO::Select.pm
#
# Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package IO::Select;

use     strict;
use warnings::register;
use     vars qw($VERSION @ISA);
require Exporter;

$VERSION = "1.17";

@ISA = qw(Exporter); # This is only so we can do version checking

sub VEC_BITS () {0}
sub FD_COUNT () {1}
sub FIRST_FD () {2}

sub new
{
 my $self = shift;
 my $type = ref($self) || $self;

 my $vec = bless [undef,0], $type;

 $vec->add(@_)
    if @_;

 $vec;
}

sub add
{
 shift->_update('add', @_);
}


sub remove
{
 shift->_update('remove', @_);
}


sub exists
{
 my $vec = shift;
 my $fno = $vec->_fileno(shift);
 return undef unless defined $fno;
 $vec->[$fno + FIRST_FD];
}


sub _fileno
{
 my($self, $f) = @_;
 return unless defined $f;
 $f = $f->[0] if ref($f) eq 'ARRAY';
 ($f =~ /^\d+$/) ? $f : fileno($f);
}

sub _update
{
 my $vec = shift;
 my $add = shift eq 'add';

 my $bits = $vec->[VEC_BITS];
 $bits = '' unless defined $bits;

 my $count = 0;
 my $f;
 foreach $f (@_)
  {
   my $fn = $vec->_fileno($f);
   next unless defined $fn;
   my $i = $fn + FIRST_FD;
   if ($add) {
     if (defined $vec->[$i]) {
	 $vec->[$i] = $f;  # if array rest might be different, so we update
	 next;
     }
     $vec->[FD_COUNT]++;
     vec($bits, $fn, 1) = 1;
     $vec->[$i] = $f;
   } else {      # remove
     next unless defined $vec->[$i];
     $vec->[FD_COUNT]--;
     vec($bits, $fn, 1) = 0;
     $vec->[$i] = undef;
   }
   $count++;
  }
 $vec->[VEC_BITS] = $vec->[FD_COUNT] ? $bits : undef;
 $count;
}

sub can_read
{
 my $vec = shift;
 my $timeout = shift;
 my $r = $vec->[VEC_BITS];

 defined($r) && (select($r,undef,undef,$timeout) > 0)
    ? handles($vec, $r)
    : ();
}

sub can_write
{
 my $vec = shift;
 my $timeout = shift;
 my $w = $vec->[VEC_BITS];

 defined($w) && (select(undef,$w,undef,$timeout) > 0)
    ? handles($vec, $w)
    : ();
}

sub has_exception
{
 my $vec = shift;
 my $timeout = shift;
 my $e = $vec->[VEC_BITS];

 defined($e) && (select(undef,undef,$e,$timeout) > 0)
    ? handles($vec, $e)
    : ();
}

sub has_error
{
 warnings::warn("Call to deprecated method 'has_error', use 'has_exception'")
	if warnings::enabled();
 goto &has_exception;
}

sub count
{
 my $vec = shift;
 $vec->[FD_COUNT];
}

sub bits
{
 my $vec = shift;
 $vec->[VEC_BITS];
}

sub as_string  # for debugging
{
 my $vec = shift;
 my $str = ref($vec) . ": ";
 my $bits = $vec->bits;
 my $count = $vec->count;
 $str .= defined($bits) ? unpack("b*", $bits) : "undef";
 $str .= " $count";
 my @handles = @$vec;
 splice(@handles, 0, FIRST_FD);
 for (@handles) {
     $str .= " " . (defined($_) ? "$_" : "-");
 }
 $str;
}

sub _max
{
 my($a,$b,$c) = @_;
 $a > $b
    ? $a > $c
        ? $a
        : $c
    : $b > $c
        ? $b
        : $c;
}

sub select
{
 shift
   if defined $_[0] && !ref($_[0]);

 my($r,$w,$e,$t) = @_;
 my @result = ();

 my $rb = defined $r ? $r->[VEC_BITS] : undef;
 my $wb = defined $w ? $w->[VEC_BITS] : undef;
 my $eb = defined $e ? $e->[VEC_BITS] : undef;

 if(select($rb,$wb,$eb,$t) > 0)
  {
   my @r = ();
   my @w = ();
   my @e = ();
   my $i = _max(defined $r ? scalar(@$r)-1 : 0,
                defined $w ? scalar(@$w)-1 : 0,
                defined $e ? scalar(@$e)-1 : 0);

   for( ; $i >= FIRST_FD ; $i--)
    {
     my $j = $i - FIRST_FD;
     push(@r, $r->[$i])
        if defined $rb && defined $r->[$i] && vec($rb, $j, 1);
     push(@w, $w->[$i])
        if defined $wb && defined $w->[$i] && vec($wb, $j, 1);
     push(@e, $e->[$i])
        if defined $eb && defined $e->[$i] && vec($eb, $j, 1);
    }

   @result = (\@r, \@w, \@e);
  }
 @result;
}


sub handles
{
 my $vec = shift;
 my $bits = shift;
 my @h = ();
 my $i;
 my $max = scalar(@$vec) - 1;

 for ($i = FIRST_FD; $i <= $max; $i++)
  {
   next unless defined $vec->[$i];
   push(@h, $vec->[$i])
      if !defined($bits) || vec($bits, $i - FIRST_FD, 1);
  }
 
 @h;
}

1;
__END__

=head1 NAME

IO::Select - OO interface to the select system call

=head1 SYNOPSIS

    use IO::Select;

    $s = IO::Select->new();

    $s->add(\*STDIN);
    $s->add($some_handle);

    @ready = $s->can_read($timeout);

    @ready = IO::Select->new(@handles)->can_read(0);

=head1 DESCRIPTION

The C<IO::Select> package implements an object approach to the system C<select>
function call. It allows the user to see what IO handles, see L<IO::Handle>,
are ready for reading, writing or have an exception pending.

=head1 CONSTRUCTOR

=over 4

=item new ( [ HANDLES ] )

The constructor creates a new object and optionally initialises it with a set
of handles.

=back

=head1 METHODS

=over 4

=item add ( HANDLES )

Add the list of handles to the C<IO::Select> object. It is these values that
will be returned when an event occurs. C<IO::Select> keeps these values in a
cache which is indexed by the C<fileno> of the handle, so if more than one
handle with the same C<fileno> is specified then only the last one is cached.

Each handle can be an C<IO::Handle> object, an integer or an array
reference where the first element is an C<IO::Handle> or an integer.

=item remove ( HANDLES )

Remove all the given handles from the object. This method also works
by the C<fileno> of the handles. So the exact handles that were added
need not be passed, just handles that have an equivalent C<fileno>

=item exists ( HANDLE )

Returns a true value (actually the handle itself) if it is present.
Returns undef otherwise.

=item handles

Return an array of all registered handles.

=item can_read ( [ TIMEOUT ] )

Return an array of handles that are ready for reading. C<TIMEOUT> is
the maximum amount of time to wait before returning an empty list, in
seconds, possibly fractional. If C<TIMEOUT> is not given and any
handles are registered then the call will block.

=item can_write ( [ TIMEOUT ] )

Same as C<can_read> except check for handles that can be written to.

=item has_exception ( [ TIMEOUT ] )

Same as C<can_read> except check for handles that have an exception
condition, for example pending out-of-band data.

=item count ()

Returns the number of handles that the object will check for when
one of the C<can_> methods is called or the object is passed to
the C<select> static method.

=item bits()

Return the bit string suitable as argument to the core select() call.

=item select ( READ, WRITE, EXCEPTION [, TIMEOUT ] )

C<select> is a static method, that is you call it with the package name
like C<new>. C<READ>, C<WRITE> and C<EXCEPTION> are either C<undef> or
C<IO::Select> objects. C<TIMEOUT> is optional and has the same effect as
for the core select call.

The result will be an array of 3 elements, each a reference to an array
which will hold the handles that are ready for reading, writing and have
exceptions respectively. Upon error an empty list is returned.

=back

=head1 EXAMPLE

Here is a short example which shows how C<IO::Select> could be used
to write a server which communicates with several sockets while also
listening for more connections on a listen socket

    use IO::Select;
    use IO::Socket;

    $lsn = new IO::Socket::INET(Listen => 1, LocalPort => 8080);
    $sel = new IO::Select( $lsn );

    while(@ready = $sel->can_read) {
        foreach $fh (@ready) {
            if($fh == $lsn) {
                # Create a new socket
                $new = $lsn->accept;
                $sel->add($new);
            }
            else {
                # Process socket

                # Maybe we have finished with the socket
                $sel->remove($fh);
                $fh->close;
            }
        }
    }

=head1 AUTHOR

Graham Barr. Currently maintained by the Perl Porters.  Please report all
bugs to <perl5-porters@perl.org>.

=head1 COPYRIGHT

Copyright (c) 1997-8 Graham Barr <gbarr@pobox.com>. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut

