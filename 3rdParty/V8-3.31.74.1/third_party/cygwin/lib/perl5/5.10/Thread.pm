package Thread;

use strict;
use warnings;
no warnings 'redefine';

our $VERSION = '3.02';
$VERSION = eval $VERSION;

BEGIN {
    use Config;
    if (! $Config{useithreads}) {
        die("This Perl not built to support threads\n");
    }
}

use threads 'yield';
use threads::shared;

require Exporter;
our @ISA = qw(Exporter threads);
our @EXPORT = qw(cond_wait cond_broadcast cond_signal);
our @EXPORT_OK = qw(async yield);

sub async (&;@) { return Thread->new(shift); }

sub done { return ! shift->is_running(); }

sub eval  { die("'eval' not implemented with 'ithreads'\n"); };
sub flags { die("'flags' not implemented with 'ithreads'\n"); };

1;

__END__

=head1 NAME

Thread - Manipulate threads in Perl (for old code only)

=head1 DEPRECATED

The C<Thread> module served as the frontend to the old-style thread model,
called I<5005threads>, that was introduced in release 5.005.  That model was
deprecated, and has been removed in version 5.10.

For old code and interim backwards compatibility, the C<Thread> module has
been reworked to function as a frontend for the new interpreter threads
(I<ithreads>) model.  However, some previous functionality is not available.
Further, the data sharing models between the two thread models are completely
different, and anything to do with data sharing has to be thought differently.
With I<ithreads>, you must explicitly C<share()> variables between the
threads.

You are strongly encouraged to migrate any existing threaded code to the new
model (i.e., use the C<threads> and C<threads::shared> modules) as soon as
possible.

=head1 HISTORY

In Perl 5.005, the thread model was that all data is implicitly shared, and
shared access to data has to be explicitly synchronized.  This model is called
I<5005threads>.

In Perl 5.6, a new model was introduced in which all is was thread local and
shared access to data has to be explicitly declared.  This model is called
I<ithreads>, for "interpreter threads".

In Perl 5.6, the I<ithreads> model was not available as a public API; only as
an internal API that was available for extension writers, and to implement
fork() emulation on Win32 platforms.

In Perl 5.8, the I<ithreads> model became available through the C<threads>
module, and the I<5005threads> model was deprecated.

In Perl 5.10, the I<5005threads> model was removed from the Perl interpreter.

=head1 SYNOPSIS

    use Thread qw(:DEFAULT async yield);

    my $t = Thread->new(\&start_sub, @start_args);

    $result = $t->join;
    $t->detach;

    if ($t->done) {
        $t->join;
    }

    if($t->equal($another_thread)) {
        # ...
    }

    yield();

    my $tid = Thread->self->tid;

    lock($scalar);
    lock(@array);
    lock(%hash);

    my @list = Thread->list;

=head1 DESCRIPTION

The C<Thread> module provides multithreading support for Perl.

=head1 FUNCTIONS

=over 8

=item $thread = Thread->new(\&start_sub)

=item $thread = Thread->new(\&start_sub, LIST)

C<new> starts a new thread of execution in the referenced subroutine. The
optional list is passed as parameters to the subroutine. Execution
continues in both the subroutine and the code after the C<new> call.

C<Thread-&gt;new> returns a thread object representing the newly created
thread.

=item lock VARIABLE

C<lock> places a lock on a variable until the lock goes out of scope.

If the variable is locked by another thread, the C<lock> call will
block until it's available.  C<lock> is recursive, so multiple calls
to C<lock> are safe--the variable will remain locked until the
outermost lock on the variable goes out of scope.

Locks on variables only affect C<lock> calls--they do I<not> affect normal
access to a variable. (Locks on subs are different, and covered in a bit.)
If you really, I<really> want locks to block access, then go ahead and tie
them to something and manage this yourself.  This is done on purpose.
While managing access to variables is a good thing, Perl doesn't force
you out of its living room...

If a container object, such as a hash or array, is locked, all the
elements of that container are not locked. For example, if a thread
does a C<lock @a>, any other thread doing a C<lock($a[12])> won't
block.

Finally, C<lock> will traverse up references exactly I<one> level.
C<lock(\$a)> is equivalent to C<lock($a)>, while C<lock(\\$a)> is not.

=item async BLOCK;

C<async> creates a thread to execute the block immediately following
it.  This block is treated as an anonymous sub, and so must have a
semi-colon after the closing brace. Like C<Thread-&gt;new>, C<async>
returns a thread object.

=item Thread->self

The C<Thread-E<gt>self> function returns a thread object that represents
the thread making the C<Thread-E<gt>self> call.

=item Thread->list

Returns a list of all non-joined, non-detached Thread objects.

=item cond_wait VARIABLE

The C<cond_wait> function takes a B<locked> variable as
a parameter, unlocks the variable, and blocks until another thread
does a C<cond_signal> or C<cond_broadcast> for that same locked
variable. The variable that C<cond_wait> blocked on is relocked
after the C<cond_wait> is satisfied.  If there are multiple threads
C<cond_wait>ing on the same variable, all but one will reblock waiting
to reaquire the lock on the variable.  (So if you're only using
C<cond_wait> for synchronization, give up the lock as soon as
possible.)

=item cond_signal VARIABLE

The C<cond_signal> function takes a locked variable as a parameter and
unblocks one thread that's C<cond_wait>ing on that variable. If more than
one thread is blocked in a C<cond_wait> on that variable, only one (and
which one is indeterminate) will be unblocked.

If there are no threads blocked in a C<cond_wait> on the variable,
the signal is discarded.

=item cond_broadcast VARIABLE

The C<cond_broadcast> function works similarly to C<cond_signal>.
C<cond_broadcast>, though, will unblock B<all> the threads that are
blocked in a C<cond_wait> on the locked variable, rather than only
one.

=item yield

The C<yield> function allows another thread to take control of the
CPU. The exact results are implementation-dependent.

=back

=head1 METHODS

=over 8

=item join

C<join> waits for a thread to end and returns any values the thread
exited with.  C<join> will block until the thread has ended, though
it won't block if the thread has already terminated.

If the thread being C<join>ed C<die>d, the error it died with will
be returned at this time. If you don't want the thread performing
the C<join> to die as well, you should either wrap the C<join> in
an C<eval> or use the C<eval> thread method instead of C<join>.

=item detach

C<detach> tells a thread that it is never going to be joined i.e.
that all traces of its existence can be removed once it stops running.
Errors in detached threads will not be visible anywhere - if you want
to catch them, you should use $SIG{__DIE__} or something like that.

=item equal

C<equal> tests whether two thread objects represent the same thread and
returns true if they do.

=item tid

The C<tid> method returns the tid of a thread. The tid is
a monotonically increasing integer assigned when a thread is
created. The main thread of a program will have a tid of zero,
while subsequent threads will have tids assigned starting with one.

=item done

The C<done> method returns true if the thread you're checking has
finished, and false otherwise.

=back

=head1 DEFUNCT

The following were implemented with I<5005threads>, but are no longer
available with I<ithreads>.

=over 8

=item lock(\&sub)

With 5005threads, you could also C<lock> a sub such that any calls to that sub
from another thread would block until the lock was released.

Also, subroutines could be declared with the C<:locked> attribute which would
serialize access to the subroutine, but allowed different threads
non-simultaneous access.

=item eval

The C<eval> method wrapped an C<eval> around a C<join>, and so waited for a
thread to exit, passing along any values the thread might have returned and
placing any errors into C<$@>.

=item flags

The C<flags> method returned the flags for the thread - an integer value
corresponding to the internal flags for the thread.

=back

=head1 SEE ALSO

L<threads>, L<threads::shared>, L<Thread::Queue>, L<Thread::Semaphore>

=cut
