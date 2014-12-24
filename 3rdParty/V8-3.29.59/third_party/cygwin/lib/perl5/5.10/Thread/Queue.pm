package Thread::Queue;

use strict;
use warnings;

our $VERSION = '2.08';

use threads::shared 1.21;
use Scalar::Util 1.10 qw(looks_like_number blessed reftype refaddr);

# Carp errors from threads::shared calls should complain about caller
our @CARP_NOT = ("threads::shared");

# Predeclarations for internal functions
my ($validate_count, $validate_index);

# Create a new queue possibly pre-populated with items
sub new
{
    my $class = shift;
    my @queue :shared = map { shared_clone($_) } @_;
    return bless(\@queue, $class);
}

# Add items to the tail of a queue
sub enqueue
{
    my $queue = shift;
    lock(@$queue);
    push(@$queue, map { shared_clone($_) } @_)
        and cond_signal(@$queue);
}

# Return a count of the number of items on a queue
sub pending
{
    my $queue = shift;
    lock(@$queue);
    return scalar(@$queue);
}

# Return 1 or more items from the head of a queue, blocking if needed
sub dequeue
{
    my $queue = shift;
    lock(@$queue);

    my $count = @_ ? $validate_count->(shift) : 1;

    # Wait for requisite number of items
    cond_wait(@$queue) until (@$queue >= $count);
    cond_signal(@$queue) if (@$queue > $count);

    # Return single item
    return shift(@$queue) if ($count == 1);

    # Return multiple items
    my @items;
    push(@items, shift(@$queue)) for (1..$count);
    return @items;
}

# Return items from the head of a queue with no blocking
sub dequeue_nb
{
    my $queue = shift;
    lock(@$queue);

    my $count = @_ ? $validate_count->(shift) : 1;

    # Return single item
    return shift(@$queue) if ($count == 1);

    # Return multiple items
    my @items;
    for (1..$count) {
        last if (! @$queue);
        push(@items, shift(@$queue));
    }
    return @items;
}

# Return an item without removing it from a queue
sub peek
{
    my $queue = shift;
    lock(@$queue);
    my $index = @_ ? $validate_index->(shift) : 0;
    return $$queue[$index];
}

# Insert items anywhere into a queue
sub insert
{
    my $queue = shift;
    lock(@$queue);

    my $index = $validate_index->(shift);

    return if (! @_);   # Nothing to insert

    # Support negative indices
    if ($index < 0) {
        $index += @$queue;
        if ($index < 0) {
            $index = 0;
        }
    }

    # Dequeue items from $index onward
    my @tmp;
    while (@$queue > $index) {
        unshift(@tmp, pop(@$queue))
    }

    # Add new items to the queue
    push(@$queue, map { shared_clone($_) } @_);

    # Add previous items back onto the queue
    push(@$queue, @tmp);

    # Soup's up
    cond_signal(@$queue);
}

# Remove items from anywhere in a queue
sub extract
{
    my $queue = shift;
    lock(@$queue);

    my $index = @_ ? $validate_index->(shift) : 0;
    my $count = @_ ? $validate_count->(shift) : 1;

    # Support negative indices
    if ($index < 0) {
        $index += @$queue;
        if ($index < 0) {
            $count += $index;
            return if ($count <= 0);            # Beyond the head of the queue
            return $queue->dequeue_nb($count);  # Extract from the head
        }
    }

    # Dequeue items from $index+$count onward
    my @tmp;
    while (@$queue > ($index+$count)) {
        unshift(@tmp, pop(@$queue))
    }

    # Extract desired items
    my @items;
    unshift(@items, pop(@$queue)) while (@$queue > $index);

    # Add back any removed items
    push(@$queue, @tmp);

    # Return single item
    return $items[0] if ($count == 1);

    # Return multiple items
    return @items;
}

### Internal Functions ###

# Check value of the requested index
$validate_index = sub {
    my $index = shift;

    if (! looks_like_number($index) || (int($index) != $index)) {
        require Carp;
        my ($method) = (caller(1))[3];
        $method =~ s/Thread::Queue:://;
        $index = 'undef' if (! defined($index));
        Carp::croak("Invalid 'index' argument ($index) to '$method' method");
    }

    return $index;
};

# Check value of the requested count
$validate_count = sub {
    my $count = shift;

    if ((! looks_like_number($count)) || (int($count) != $count) || ($count < 1)) {
        require Carp;
        my ($method) = (caller(1))[3];
        $method =~ s/Thread::Queue:://;
        $count = 'undef' if (! defined($count));
        Carp::croak("Invalid 'count' argument ($count) to '$method' method");
    }

    return $count;
};

1;

=head1 NAME

Thread::Queue - Thread-safe queues

=head1 VERSION

This document describes Thread::Queue version 2.08

=head1 SYNOPSIS

    use strict;
    use warnings;

    use threads;
    use Thread::Queue;

    my $q = Thread::Queue->new();    # A new empty queue

    # Worker thread
    my $thr = threads->create(sub {
                                while (my $item = $q->dequeue()) {
                                    # Do work on $item
                                }
                             })->detach();

    # Send work to the thread
    $q->enqueue($item1, ...);


    # Count of items in the queue
    my $left = $q->pending();

    # Non-blocking dequeue
    if (defined(my $item = $q->dequeue_nb())) {
        # Work on $item
    }

    # Get the second item in the queue without dequeuing anything
    my $item = $q->peek(1);

    # Insert two items into the queue just behind the head
    $q->insert(1, $item1, $item2);

    # Extract the last two items on the queue
    my ($item1, $item2) = $q->extract(-2, 2);

=head1 DESCRIPTION

This module provides thread-safe FIFO queues that can be accessed safely by
any number of threads.

Any data types supported by L<threads::shared> can be passed via queues:

=over

=item Ordinary scalars

=item Array refs

=item Hash refs

=item Scalar refs

=item Objects based on the above

=back

Ordinary scalars are added to queues as they are.

If not already thread-shared, the other complex data types will be cloned
(recursively, if needed, and including any C<bless>ings and read-only
settings) into thread-shared structures before being placed onto a queue.

For example, the following would cause L<Thread::Queue> to create a empty,
shared array reference via C<&shared([])>, copy the elements 'foo', 'bar'
and 'baz' from C<@ary> into it, and then place that shared reference onto
the queue:

    my @ary = qw/foo bar baz/;
    $q->enqueue(\@ary);

However, for the following, the items are already shared, so their references
are added directly to the queue, and no cloning takes place:

    my @ary :shared = qw/foo bar baz/;
    $q->enqueue(\@ary);

    my $obj = &shared({});
    $$obj{'foo'} = 'bar';
    $$obj{'qux'} = 99;
    bless($obj, 'My::Class');
    $q->enqueue($obj);

See L</"LIMITATIONS"> for caveats related to passing objects via queues.

=head1 QUEUE CREATION

=over

=item ->new()

Creates a new empty queue.

=item ->new(LIST)

Creates a new queue pre-populated with the provided list of items.

=back

=head1 BASIC METHODS

The following methods deal with queues on a FIFO basis.

=over

=item ->enqueue(LIST)

Adds a list of items onto the end of the queue.

=item ->dequeue()

=item ->dequeue(COUNT)

Removes the requested number of items (default is 1) from the head of the
queue, and returns them.  If the queue contains fewer than the requested
number of items, then the thread will be blocked until the requisite number
of items are available (i.e., until other threads <enqueue> more items).

=item ->dequeue_nb()

=item ->dequeue_nb(COUNT)

Removes the requested number of items (default is 1) from the head of the
queue, and returns them.  If the queue contains fewer than the requested
number of items, then it immediately (i.e., non-blocking) returns whatever
items there are on the queue.  If the queue is empty, then C<undef> is
returned.

=item ->pending()

Returns the number of items still in the queue.

=back

=head1 ADVANCED METHODS

The following methods can be used to manipulate items anywhere in a queue.

To prevent the contents of a queue from being modified by another thread
while it is being examined and/or changed, L<lock|threads::shared/"lock
VARIABLE"> the queue inside a local block:

    {
        lock($q);   # Keep other threads from changing the queue's contents
        my $item = $q->peek();
        if ($item ...) {
            ...
        }
    }
    # Queue is now unlocked

=over

=item ->peek()

=item ->peek(INDEX)

Returns an item from the queue without dequeuing anything.  Defaults to the
the head of queue (at index position 0) if no index is specified.  Negative
index values are supported as with L<arrays|perldata/"Subscripts"> (i.e., -1
is the end of the queue, -2 is next to last, and so on).

If no items exists at the specified index (i.e., the queue is empty, or the
index is beyond the number of items on the queue), then C<undef> is returned.

Remember, the returned item is not removed from the queue, so manipulating a
C<peek>ed at reference affects the item on the queue.

=item ->insert(INDEX, LIST)

Adds the list of items to the queue at the specified index position (0
is the head of the list).  Any existing items at and beyond that position are
pushed back past the newly added items:

    $q->enqueue(1, 2, 3, 4);
    $q->insert(1, qw/foo bar/);
    # Queue now contains:  1, foo, bar, 2, 3, 4

Specifying an index position greater than the number of items in the queue
just adds the list to the end.

Negative index positions are supported:

    $q->enqueue(1, 2, 3, 4);
    $q->insert(-2, qw/foo bar/);
    # Queue now contains:  1, 2, foo, bar, 3, 4

Specifying a negative index position greater than the number of items in the
queue adds the list to the head of the queue.

=item ->extract()

=item ->extract(INDEX)

=item ->extract(INDEX, COUNT)

Removes and returns the specified number of items (defaults to 1) from the
specified index position in the queue (0 is the head of the queue).  When
called with no arguments, C<extract> operates the same as C<dequeue_nb>.

This method is non-blocking, and will return only as many items as are
available to fulfill the request:

    $q->enqueue(1, 2, 3, 4);
    my $item  = $q->extract(2)     # Returns 3
                                   # Queue now contains:  1, 2, 4
    my @items = $q->extract(1, 3)  # Returns (2, 4)
                                   # Queue now contains:  1

Specifying an index position greater than the number of items in the
queue results in C<undef> or an empty list being returned.

    $q->enqueue('foo');
    my $nada = $q->extract(3)      # Returns undef
    my @nada = $q->extract(1, 3)   # Returns ()

Negative index positions are supported.  Specifying a negative index position
greater than the number of items in the queue may return items from the head
of the queue (similar to C<dequeue_nb>) if the count overlaps the head of the
queue from the specified position (i.e. if queue size + index + count is
greater than zero):

    $q->enqueue(qw/foo bar baz/);
    my @nada = $q->extract(-6, 2);   # Returns ()         - (3+(-6)+2) <= 0
    my @some = $q->extract(-6, 4);   # Returns (foo)      - (3+(-6)+4) > 0
                                     # Queue now contains:  bar, baz
    my @rest = $q->extract(-3, 4);   # Returns (bar, baz) - (2+(-3)+4) > 0

=back

=head1 NOTES

Queues created by L<Thread::Queue> can be used in both threaded and
non-threaded applications.

=head1 LIMITATIONS

Passing objects on queues may not work if the objects' classes do not support
sharing.  See L<threads::shared/"BUGS AND LIMITATIONS"> for more.

Passing array/hash refs that contain objects may not work for Perl prior to
5.10.0.

=head1 SEE ALSO

Thread::Queue Discussion Forum on CPAN:
L<http://www.cpanforum.com/dist/Thread-Queue>

Annotated POD for Thread::Queue:
L<http://annocpan.org/~JDHEDDEN/Thread-Queue-2.08/lib/Thread/Queue.pm>

Source repository:
L<http://code.google.com/p/thread-queue/>

L<threads>, L<threads::shared>

=head1 MAINTAINER

Jerry D. Hedden, S<E<lt>jdhedden AT cpan DOT orgE<gt>>

=head1 LICENSE

This program is free software; you can redistribute it and/or modify it under
the same terms as Perl itself.

=cut
