package Thread::Semaphore;

use strict;
use warnings;

our $VERSION = '2.08';

use threads::shared;
use Scalar::Util 1.10 qw(looks_like_number);

# Create a new semaphore optionally with specified count (count defaults to 1)
sub new {
    my $class = shift;
    my $val :shared = @_ ? shift : 1;
    if (!defined($val) ||
        ! looks_like_number($val) ||
        (int($val) != $val))
    {
        require Carp;
        $val = 'undef' if (! defined($val));
        Carp::croak("Semaphore initializer is not an integer: $val");
    }
    return bless(\$val, $class);
}

# Decrement a semaphore's count (decrement amount defaults to 1)
sub down {
    my $sema = shift;
    lock($$sema);
    my $dec = @_ ? shift : 1;
    if (! defined($dec) ||
        ! looks_like_number($dec) ||
        (int($dec) != $dec) ||
        ($dec < 1))
    {
        require Carp;
        $dec = 'undef' if (! defined($dec));
        Carp::croak("Semaphore decrement is not a positive integer: $dec");
    }
    cond_wait($$sema) until ($$sema >= $dec);
    $$sema -= $dec;
}

# Increment a semaphore's count (increment amount defaults to 1)
sub up {
    my $sema = shift;
    lock($$sema);
    my $inc = @_ ? shift : 1;
    if (! defined($inc) ||
        ! looks_like_number($inc) ||
        (int($inc) != $inc) ||
        ($inc < 1))
    {
        require Carp;
        $inc = 'undef' if (! defined($inc));
        Carp::croak("Semaphore increment is not a positive integer: $inc");
    }
    ($$sema += $inc) > 0 and cond_broadcast($$sema);
}

1;

=head1 NAME

Thread::Semaphore - Thread-safe semaphores

=head1 VERSION

This document describes Thread::Semaphore version 2.08

=head1 SYNOPSIS

    use Thread::Semaphore;
    my $s = Thread::Semaphore->new();
    $s->down();   # Also known as the semaphore P operation.
    # The guarded section is here
    $s->up();     # Also known as the semaphore V operation.

    # The default semaphore value is 1
    my $s = Thread::Semaphore-new($initial_value);
    $s->down($down_value);
    $s->up($up_value);

=head1 DESCRIPTION

Semaphores provide a mechanism to regulate access to resources.  Unlike
locks, semaphores aren't tied to particular scalars, and so may be used to
control access to anything you care to use them for.

Semaphores don't limit their values to zero and one, so they can be used to
control access to some resource that there may be more than one of (e.g.,
filehandles).  Increment and decrement amounts aren't fixed at one either,
so threads can reserve or return multiple resources at once.

=head1 METHODS

=over 8

=item ->new()

=item ->new(NUMBER)

C<new> creates a new semaphore, and initializes its count to the specified
number (which must be an integer).  If no number is specified, the
semaphore's count defaults to 1.

=item ->down()

=item ->down(NUMBER)

The C<down> method decreases the semaphore's count by the specified number
(which must be an integer >= 1), or by one if no number is specified.

If the semaphore's count would drop below zero, this method will block
until such time as the semaphore's count is greater than or equal to the
amount you're C<down>ing the semaphore's count by.

This is the semaphore "P operation" (the name derives from the Dutch
word "pak", which means "capture" -- the semaphore operations were
named by the late Dijkstra, who was Dutch).

=item ->up()

=item ->up(NUMBER)

The C<up> method increases the semaphore's count by the number specified
(which must be an integer >= 1), or by one if no number is specified.

This will unblock any thread that is blocked trying to C<down> the
semaphore if the C<up> raises the semaphore's count above the amount that
the C<down> is trying to decrement it by.  For example, if three threads
are blocked trying to C<down> a semaphore by one, and another thread C<up>s
the semaphore by two, then two of the blocked threads (which two is
indeterminate) will become unblocked.

This is the semaphore "V operation" (the name derives from the Dutch
word "vrij", which means "release").

=back

=head1 NOTES

Semaphores created by L<Thread::Semaphore> can be used in both threaded and
non-threaded applications.  This allows you to write modules and packages
that potentially make use of semaphores, and that will function in either
environment.

=head1 SEE ALSO

Thread::Semaphore Discussion Forum on CPAN:
L<http://www.cpanforum.com/dist/Thread-Semaphore>

Annotated POD for Thread::Semaphore:
L<http://annocpan.org/~JDHEDDEN/Thread-Semaphore-2.08/lib/Thread/Semaphore.pm>

Source repository:
L<http://code.google.com/p/thread-semaphore/>

L<threads>, L<threads::shared>

=head1 MAINTAINER

Jerry D. Hedden, S<E<lt>jdhedden AT cpan DOT orgE<gt>>

=head1 LICENSE

This program is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut
