package threads;

use 5.008;

use strict;
use warnings;

our $VERSION = '1.69';
my $XS_VERSION = $VERSION;
$VERSION = eval $VERSION;

# Verify this Perl supports threads
require Config;
if (! $Config::Config{useithreads}) {
    die("This Perl not built to support threads\n");
}

# Complain if 'threads' is loaded after 'threads::shared'
if ($threads::shared::threads_shared) {
    warn <<'_MSG_';
Warning, threads::shared has already been loaded.  To
enable shared variables, 'use threads' must be called
before threads::shared or any module that uses it.
_MSG_
}

# Declare that we have been loaded
$threads::threads = 1;

# Load the XS code
require XSLoader;
XSLoader::load('threads', $XS_VERSION);


### Export ###

sub import
{
    my $class = shift;   # Not used

    # Exported subroutines
    my @EXPORT = qw(async);

    # Handle args
    while (my $sym = shift) {
        if ($sym =~ /^(?:stack|exit)/i) {
            if (defined(my $arg = shift)) {
                if ($sym =~ /^stack/i) {
                    threads->set_stack_size($arg);
                } else {
                    $threads::thread_exit_only = $arg =~ /^thread/i;
                }
            } else {
                require Carp;
                Carp::croak("threads: Missing argument for option: $sym");
            }

        } elsif ($sym =~ /^str/i) {
            import overload ('""' => \&tid);

        } elsif ($sym =~ /^(?::all|yield)$/) {
            push(@EXPORT, qw(yield));

        } else {
            require Carp;
            Carp::croak("threads: Unknown import option: $sym");
        }
    }

    # Export subroutine names
    my $caller = caller();
    foreach my $sym (@EXPORT) {
        no strict 'refs';
        *{$caller.'::'.$sym} = \&{$sym};
    }

    # Set stack size via environment variable
    if (exists($ENV{'PERL5_ITHREADS_STACK_SIZE'})) {
        threads->set_stack_size($ENV{'PERL5_ITHREADS_STACK_SIZE'});
    }
}


### Methods, etc. ###

# Exit from a thread (only)
sub exit
{
    my ($class, $status) = @_;
    if (! defined($status)) {
        $status = 0;
    }

    # Class method only
    if (ref($class)) {
        require Carp;
        Carp::croak('Usage: threads->exit(status)');
    }

    $class->set_thread_exit_only(1);
    CORE::exit($status);
}

# 'Constant' args for threads->list()
sub threads::all      { }
sub threads::running  { 1 }
sub threads::joinable { 0 }

# 'new' is an alias for 'create'
*new = \&create;

# 'async' is a function alias for the 'threads->create()' method
sub async (&;@)
{
    unshift(@_, 'threads');
    # Use "goto" trick to avoid pad problems from 5.8.1 (fixed in 5.8.2)
    goto &create;
}

# Thread object equality checking
use overload (
    '==' => \&equal,
    '!=' => sub { ! equal(@_) },
    'fallback' => 1
);

1;

__END__

=head1 NAME

threads - Perl interpreter-based threads

=head1 VERSION

This document describes threads version 1.69

=head1 SYNOPSIS

    use threads ('yield',
                 'stack_size' => 64*4096,
                 'exit' => 'threads_only',
                 'stringify');

    sub start_thread {
        my @args = @_;
        print('Thread started: ', join(' ', @args), "\n");
    }
    my $thr = threads->create('start_thread', 'argument');
    $thr->join();

    threads->create(sub { print("I am a thread\n"); })->join();

    my $thr2 = async { foreach (@files) { ... } };
    $thr2->join();
    if (my $err = $thr2->error()) {
        warn("Thread error: $err\n");
    }

    # Invoke thread in list context (implicit) so it can return a list
    my ($thr) = threads->create(sub { return (qw/a b c/); });
    # or specify list context explicitly
    my $thr = threads->create({'context' => 'list'},
                              sub { return (qw/a b c/); });
    my @results = $thr->join();

    $thr->detach();

    # Get a thread's object
    $thr = threads->self();
    $thr = threads->object($tid);

    # Get a thread's ID
    $tid = threads->tid();
    $tid = $thr->tid();
    $tid = "$thr";

    # Give other threads a chance to run
    threads->yield();
    yield();

    # Lists of non-detached threads
    my @threads = threads->list();
    my $thread_count = threads->list();

    my @running = threads->list(threads::running);
    my @joinable = threads->list(threads::joinable);

    # Test thread objects
    if ($thr1 == $thr2) {
        ...
    }

    # Manage thread stack size
    $stack_size = threads->get_stack_size();
    $old_size = threads->set_stack_size(32*4096);

    # Create a thread with a specific context and stack size
    my $thr = threads->create({ 'context'    => 'list',
                                'stack_size' => 32*4096,
                                'exit'       => 'thread_only' },
                              \&foo);

    # Get thread's context
    my $wantarray = $thr->wantarray();

    # Check thread's state
    if ($thr->is_running()) {
        sleep(1);
    }
    if ($thr->is_joinable()) {
        $thr->join();
    }

    # Send a signal to a thread
    $thr->kill('SIGUSR1');

    # Exit a thread
    threads->exit();

=head1 DESCRIPTION

Perl 5.6 introduced something called interpreter threads.  Interpreter threads
are different from I<5005threads> (the thread model of Perl 5.005) by creating
a new Perl interpreter per thread, and not sharing any data or state between
threads by default.

Prior to Perl 5.8, this has only been available to people embedding Perl, and
for emulating fork() on Windows.

The I<threads> API is loosely based on the old Thread.pm API. It is very
important to note that variables are not shared between threads, all variables
are by default thread local.  To use shared variables one must also use
L<threads::shared>:

    use threads;
    use threads::shared;

It is also important to note that you must enable threads by doing C<use
threads> as early as possible in the script itself, and that it is not
possible to enable threading inside an C<eval "">, C<do>, C<require>, or
C<use>.  In particular, if you are intending to share variables with
L<threads::shared>, you must C<use threads> before you C<use threads::shared>.
(C<threads> will emit a warning if you do it the other way around.)

=over

=item $thr = threads->create(FUNCTION, ARGS)

This will create a new thread that will begin execution with the specified
entry point function, and give it the I<ARGS> list as parameters.  It will
return the corresponding threads object, or C<undef> if thread creation failed.

I<FUNCTION> may either be the name of a function, an anonymous subroutine, or
a code ref.

    my $thr = threads->create('func_name', ...);
        # or
    my $thr = threads->create(sub { ... }, ...);
        # or
    my $thr = threads->create(\&func, ...);

The C<-E<gt>new()> method is an alias for C<-E<gt>create()>.

=item $thr->join()

This will wait for the corresponding thread to complete its execution.  When
the thread finishes, C<-E<gt>join()> will return the return value(s) of the
entry point function.

The context (void, scalar or list) for the return value(s) for C<-E<gt>join()>
is determined at the time of thread creation.

    # Create thread in list context (implicit)
    my ($thr1) = threads->create(sub {
                                    my @results = qw(a b c);
                                    return (@results);
                                 });
    #   or (explicit)
    my $thr1 = threads->create({'context' => 'list'},
                               sub {
                                    my @results = qw(a b c);
                                    return (@results);
                               });
    # Retrieve list results from thread
    my @res1 = $thr1->join();

    # Create thread in scalar context (implicit)
    my $thr2 = threads->create(sub {
                                    my $result = 42;
                                    return ($result);
                                 });
    # Retrieve scalar result from thread
    my $res2 = $thr2->join();

    # Create a thread in void context (explicit)
    my $thr3 = threads->create({'void' => 1},
                               sub { print("Hello, world\n"); });
    # Join the thread in void context (i.e., no return value)
    $thr3->join();

See L</"THREAD CONTEXT"> for more details.

If the program exits without all threads having either been joined or
detached, then a warning will be issued.

Calling C<-E<gt>join()> or C<-E<gt>detach()> on an already joined thread will
cause an error to be thrown.

=item $thr->detach()

Makes the thread unjoinable, and causes any eventual return value to be
discarded.  When the program exits, any detached threads that are still
running are silently terminated.

If the program exits without all threads having either been joined or
detached, then a warning will be issued.

Calling C<-E<gt>join()> or C<-E<gt>detach()> on an already detached thread
will cause an error to be thrown.

=item threads->detach()

Class method that allows a thread to detach itself.

=item threads->self()

Class method that allows a thread to obtain its own I<threads> object.

=item $thr->tid()

Returns the ID of the thread.  Thread IDs are unique integers with the main
thread in a program being 0, and incrementing by 1 for every thread created.

=item threads->tid()

Class method that allows a thread to obtain its own ID.

=item "$thr"

If you add the C<stringify> import option to your C<use threads> declaration,
then using a threads object in a string or a string context (e.g., as a hash
key) will cause its ID to be used as the value:

    use threads qw(stringify);

    my $thr = threads->create(...);
    print("Thread $thr started...\n");  # Prints out: Thread 1 started...

=item threads->object($tid)

This will return the I<threads> object for the I<active> thread associated
with the specified thread ID.  Returns C<undef> if there is no thread
associated with the TID, if the thread is joined or detached, if no TID is
specified or if the specified TID is undef.

=item threads->yield()

This is a suggestion to the OS to let this thread yield CPU time to other
threads.  What actually happens is highly dependent upon the underlying
thread implementation.

You may do C<use threads qw(yield)>, and then just use C<yield()> in your
code.

=item threads->list()

=item threads->list(threads::all)

=item threads->list(threads::running)

=item threads->list(threads::joinable)

With no arguments (or using C<threads::all>) and in a list context, returns a
list of all non-joined, non-detached I<threads> objects.  In a scalar context,
returns a count of the same.

With a I<true> argument (using C<threads::running>), returns a list of all
non-joined, non-detached I<threads> objects that are still running.

With a I<false> argument (using C<threads::joinable>), returns a list of all
non-joined, non-detached I<threads> objects that have finished running (i.e.,
for which C<-E<gt>join()> will not I<block>).

=item $thr1->equal($thr2)

Tests if two threads objects are the same thread or not.  This is overloaded
to the more natural forms:

    if ($thr1 == $thr2) {
        print("Threads are the same\n");
    }
    # or
    if ($thr1 != $thr2) {
        print("Threads differ\n");
    }

(Thread comparison is based on thread IDs.)

=item async BLOCK;

C<async> creates a thread to execute the block immediately following
it.  This block is treated as an anonymous subroutine, and so must have a
semicolon after the closing brace.  Like C<threads-E<gt>create()>, C<async>
returns a I<threads> object.

=item $thr->error()

Threads are executed in an C<eval> context.  This method will return C<undef>
if the thread terminates I<normally>.  Otherwise, it returns the value of
C<$@> associated with the thread's execution status in its C<eval> context.

=item $thr->_handle()

This I<private> method returns the memory location of the internal thread
structure associated with a threads object.  For Win32, this is a pointer to
the C<HANDLE> value returned by C<CreateThread> (i.e., C<HANDLE *>); for other
platforms, it is a pointer to the C<pthread_t> structure used in the
C<pthread_create> call (i.e., C<pthread_t *>).

This method is of no use for general Perl threads programming.  Its intent is
to provide other (XS-based) thread modules with the capability to access, and
possibly manipulate, the underlying thread structure associated with a Perl
thread.

=item threads->_handle()

Class method that allows a thread to obtain its own I<handle>.

=back

=head1 EXITING A THREAD

The usual method for terminating a thread is to
L<return()|perlfunc/"return EXPR"> from the entry point function with the
appropriate return value(s).

=over

=item threads->exit()

If needed, a thread can be exited at any time by calling
C<threads-E<gt>exit()>.  This will cause the thread to return C<undef> in a
scalar context, or the empty list in a list context.

When called from the I<main> thread, this behaves the same as C<exit(0)>.

=item threads->exit(status)

When called from a thread, this behaves like C<threads-E<gt>exit()> (i.e., the
exit status code is ignored).

When called from the I<main> thread, this behaves the same as C<exit(status)>.

=item die()

Calling C<die()> in a thread indicates an abnormal exit for the thread.  Any
C<$SIG{__DIE__}> handler in the thread will be called first, and then the
thread will exit with a warning message that will contain any arguments passed
in the C<die()> call.

=item exit(status)

Calling L<exit()|perlfunc/"exit EXPR"> inside a thread causes the whole
application to terminate.  Because of this, the use of C<exit()> inside
threaded code, or in modules that might be used in threaded applications, is
strongly discouraged.

If C<exit()> really is needed, then consider using the following:

    threads->exit() if threads->can('exit');   # Thread friendly
    exit(status);

=item use threads 'exit' => 'threads_only'

This globally overrides the default behavior of calling C<exit()> inside a
thread, and effectively causes such calls to behave the same as
C<threads-E<gt>exit()>.  In other words, with this setting, calling C<exit()>
causes only the thread to terminate.

Because of its global effect, this setting should not be used inside modules
or the like.

The I<main> thread is unaffected by this setting.

=item threads->create({'exit' => 'thread_only'}, ...)

This overrides the default behavior of C<exit()> inside the newly created
thread only.

=item $thr->set_thread_exit_only(boolean)

This can be used to change the I<exit thread only> behavior for a thread after
it has been created.  With a I<true> argument, C<exit()> will cause only the
thread to exit.  With a I<false> argument, C<exit()> will terminate the
application.

The I<main> thread is unaffected by this call.

=item threads->set_thread_exit_only(boolean)

Class method for use inside a thread to change its own behavior for C<exit()>.

The I<main> thread is unaffected by this call.

=back

=head1 THREAD STATE

The following boolean methods are useful in determining the I<state> of a
thread.

=over

=item $thr->is_running()

Returns true if a thread is still running (i.e., if its entry point function
has not yet finished or exited).

=item $thr->is_joinable()

Returns true if the thread has finished running, is not detached and has not
yet been joined.  In other words, the thread is ready to be joined, and a call
to C<$thr-E<gt>join()> will not I<block>.

=item $thr->is_detached()

Returns true if the thread has been detached.

=item threads->is_detached()

Class method that allows a thread to determine whether or not it is detached.

=back

=head1 THREAD CONTEXT

As with subroutines, the type of value returned from a thread's entry point
function may be determined by the thread's I<context>:  list, scalar or void.
The thread's context is determined at thread creation.  This is necessary so
that the context is available to the entry point function via
L<wantarray()|perlfunc/"wantarray">.  The thread may then specify a value of
the appropriate type to be returned from C<-E<gt>join()>.

=head2 Explicit context

Because thread creation and thread joining may occur in different contexts, it
may be desirable to state the context explicitly to the thread's entry point
function.  This may be done by calling C<-E<gt>create()> with a hash reference
as the first argument:

    my $thr = threads->create({'context' => 'list'}, \&foo);
    ...
    my @results = $thr->join();

In the above, the threads object is returned to the parent thread in scalar
context, and the thread's entry point function C<foo> will be called in list
(array) context such that the parent thread can receive a list (array) from
the C<-E<gt>join()> call.  (C<'array'> is synonymous with C<'list'>.)

Similarly, if you need the threads object, but your thread will not be
returning a value (i.e., I<void> context), you would do the following:

    my $thr = threads->create({'context' => 'void'}, \&foo);
    ...
    $thr->join();

The context type may also be used as the I<key> in the hash reference followed
by a I<true> value:

    threads->create({'scalar' => 1}, \&foo);
    ...
    my ($thr) = threads->list();
    my $result = $thr->join();

=head2 Implicit context

If not explicitly stated, the thread's context is implied from the context
of the C<-E<gt>create()> call:

    # Create thread in list context
    my ($thr) = threads->create(...);

    # Create thread in scalar context
    my $thr = threads->create(...);

    # Create thread in void context
    threads->create(...);

=head2 $thr->wantarray()

This returns the thread's context in the same manner as
L<wantarray()|perlfunc/"wantarray">.

=head2 threads->wantarray()

Class method to return the current thread's context.  This returns the same
value as running L<wantarray()|perlfunc/"wantarray"> inside the current
thread's entry point function.

=head1 THREAD STACK SIZE

The default per-thread stack size for different platforms varies
significantly, and is almost always far more than is needed for most
applications.  On Win32, Perl's makefile explicitly sets the default stack to
16 MB; on most other platforms, the system default is used, which again may be
much larger than is needed.

By tuning the stack size to more accurately reflect your application's needs,
you may significantly reduce your application's memory usage, and increase the
number of simultaneously running threads.

Note that on Windows, address space allocation granularity is 64 KB,
therefore, setting the stack smaller than that on Win32 Perl will not save any
more memory.

=over

=item threads->get_stack_size();

Returns the current default per-thread stack size.  The default is zero, which
means the system default stack size is currently in use.

=item $size = $thr->get_stack_size();

Returns the stack size for a particular thread.  A return value of zero
indicates the system default stack size was used for the thread.

=item $old_size = threads->set_stack_size($new_size);

Sets a new default per-thread stack size, and returns the previous setting.

Some platforms have a minimum thread stack size.  Trying to set the stack size
below this value will result in a warning, and the minimum stack size will be
used.

Some Linux platforms have a maximum stack size.  Setting too large of a stack
size will cause thread creation to fail.

If needed, C<$new_size> will be rounded up to the next multiple of the memory
page size (usually 4096 or 8192).

Threads created after the stack size is set will then either call
C<pthread_attr_setstacksize()> I<(for pthreads platforms)>, or supply the
stack size to C<CreateThread()> I<(for Win32 Perl)>.

(Obviously, this call does not affect any currently extant threads.)

=item use threads ('stack_size' => VALUE);

This sets the default per-thread stack size at the start of the application.

=item $ENV{'PERL5_ITHREADS_STACK_SIZE'}

The default per-thread stack size may be set at the start of the application
through the use of the environment variable C<PERL5_ITHREADS_STACK_SIZE>:

    PERL5_ITHREADS_STACK_SIZE=1048576
    export PERL5_ITHREADS_STACK_SIZE
    perl -e'use threads; print(threads->get_stack_size(), "\n")'

This value overrides any C<stack_size> parameter given to C<use threads>.  Its
primary purpose is to permit setting the per-thread stack size for legacy
threaded applications.

=item threads->create({'stack_size' => VALUE}, FUNCTION, ARGS)

To specify a particular stack size for any individual thread, call
C<-E<gt>create()> with a hash reference as the first argument:

    my $thr = threads->create({'stack_size' => 32*4096}, \&foo, @args);

=item $thr2 = $thr1->create(FUNCTION, ARGS)

This creates a new thread (C<$thr2>) that inherits the stack size from an
existing thread (C<$thr1>).  This is shorthand for the following:

    my $stack_size = $thr1->get_stack_size();
    my $thr2 = threads->create({'stack_size' => $stack_size}, FUNCTION, ARGS);

=back

=head1 THREAD SIGNALLING

When safe signals is in effect (the default behavior - see L</"Unsafe signals">
for more details), then signals may be sent and acted upon by individual
threads.

=over 4

=item $thr->kill('SIG...');

Sends the specified signal to the thread.  Signal names and (positive) signal
numbers are the same as those supported by
L<kill()|perlfunc/"kill SIGNAL, LIST">.  For example, 'SIGTERM', 'TERM' and
(depending on the OS) 15 are all valid arguments to C<-E<gt>kill()>.

Returns the thread object to allow for method chaining:

    $thr->kill('SIG...')->join();

=back

Signal handlers need to be set up in the threads for the signals they are
expected to act upon.  Here's an example for I<cancelling> a thread:

    use threads;

    sub thr_func
    {
        # Thread 'cancellation' signal handler
        $SIG{'KILL'} = sub { threads->exit(); };

        ...
    }

    # Create a thread
    my $thr = threads->create('thr_func');

    ...

    # Signal the thread to terminate, and then detach
    # it so that it will get cleaned up automatically
    $thr->kill('KILL')->detach();

Here's another simplistic example that illustrates the use of thread
signalling in conjunction with a semaphore to provide rudimentary I<suspend>
and I<resume> capabilities:

    use threads;
    use Thread::Semaphore;

    sub thr_func
    {
        my $sema = shift;

        # Thread 'suspend/resume' signal handler
        $SIG{'STOP'} = sub {
            $sema->down();      # Thread suspended
            $sema->up();        # Thread resumes
        };

        ...
    }

    # Create a semaphore and pass it to a thread
    my $sema = Thread::Semaphore->new();
    my $thr = threads->create('thr_func', $sema);

    # Suspend the thread
    $sema->down();
    $thr->kill('STOP');

    ...

    # Allow the thread to continue
    $sema->up();

CAVEAT:  The thread signalling capability provided by this module does not
actually send signals via the OS.  It I<emulates> signals at the Perl-level
such that signal handlers are called in the appropriate thread.  For example,
sending C<$thr-E<gt>kill('STOP')> does not actually suspend a thread (or the
whole process), but does cause a C<$SIG{'STOP'}> handler to be called in that
thread (as illustrated above).

As such, signals that would normally not be appropriate to use in the
C<kill()> command (e.g., C<kill('KILL', $$)>) are okay to use with the
C<-E<gt>kill()> method (again, as illustrated above).

Correspondingly, sending a signal to a thread does not disrupt the operation
the thread is currently working on:  The signal will be acted upon after the
current operation has completed.  For instance, if the thread is I<stuck> on
an I/O call, sending it a signal will not cause the I/O call to be interrupted
such that the signal is acted up immediately.

Sending a signal to a terminated thread is ignored.

=head1 WARNINGS

=over 4

=item Perl exited with active threads:

If the program exits without all threads having either been joined or
detached, then this warning will be issued.

NOTE:  If the I<main> thread exits, then this warning cannot be suppressed
using C<no warnings 'threads';> as suggested below.

=item Thread creation failed: pthread_create returned #

See the appropriate I<man> page for C<pthread_create> to determine the actual
cause for the failure.

=item Thread # terminated abnormally: ...

A thread terminated in some manner other than just returning from its entry
point function, or by using C<threads-E<gt>exit()>.  For example, the thread
may have terminated because of an error, or by using C<die>.

=item Using minimum thread stack size of #

Some platforms have a minimum thread stack size.  Trying to set the stack size
below this value will result in the above warning, and the stack size will be
set to the minimum.

=item Thread creation failed: pthread_attr_setstacksize(I<SIZE>) returned 22

The specified I<SIZE> exceeds the system's maximum stack size.  Use a smaller
value for the stack size.

=back

If needed, thread warnings can be suppressed by using:

    no warnings 'threads';

in the appropriate scope.

=head1 ERRORS

=over 4

=item This Perl not built to support threads

The particular copy of Perl that you're trying to use was not built using the
C<useithreads> configuration option.

Having threads support requires all of Perl and all of the XS modules in the
Perl installation to be rebuilt; it is not just a question of adding the
L<threads> module (i.e., threaded and non-threaded Perls are binary
incompatible.)

=item Cannot change stack size of an existing thread

The stack size of currently extant threads cannot be changed, therefore, the
following results in the above error:

    $thr->set_stack_size($size);

=item Cannot signal threads without safe signals

Safe signals must be in effect to use the C<-E<gt>kill()> signalling method.
See L</"Unsafe signals"> for more details.

=item Unrecognized signal name: ...

The particular copy of Perl that you're trying to use does not support the
specified signal being used in a C<-E<gt>kill()> call.

=back

=head1 BUGS AND LIMITATIONS

Before you consider posting a bug report, please consult, and possibly post a
message to the discussion forum to see if what you've encountered is a known
problem.

=over

=item Thread-safe modules

See L<perlmod/"Making your module threadsafe"> when creating modules that may
be used in threaded applications, especially if those modules use non-Perl
data, or XS code.

=item Using non-thread-safe modules

Unfortunately, you may encounter Perl modules that are not I<thread-safe>.
For example, they may crash the Perl interpreter during execution, or may dump
core on termination.  Depending on the module and the requirements of your
application, it may be possible to work around such difficulties.

If the module will only be used inside a thread, you can try loading the
module from inside the thread entry point function using C<require> (and
C<import> if needed):

    sub thr_func
    {
        require Unsafe::Module
        # Unsafe::Module->import(...);

        ....
    }

If the module is needed inside the I<main> thread, try modifying your
application so that the module is loaded (again using C<require> and
C<-E<gt>import()>) after any threads are started, and in such a way that no
other threads are started afterwards.

If the above does not work, or is not adequate for your application, then file
a bug report on L<http://rt.cpan.org/Public/> against the problematic module.

=item Current working directory

On all platforms except MSWin32, the setting for the current working directory
is shared among all threads such that changing it in one thread (e.g., using
C<chdir()>) will affect all the threads in the application.

On MSWin32, each thread maintains its own the current working directory
setting.

=item Environment variables

Currently, on all platforms except MSWin32, all I<system> calls (e.g., using
C<system()> or back-ticks) made from threads use the environment variable
settings from the I<main> thread.  In other words, changes made to C<%ENV> in
a thread will not be visible in I<system> calls made by that thread.

To work around this, set environment variables as part of the I<system> call.
For example:

    my $msg = 'hello';
    system("FOO=$msg; echo \$FOO");   # Outputs 'hello' to STDOUT

On MSWin32, each thread maintains its own set of environment variables.

=item Parent-child threads

On some platforms, it might not be possible to destroy I<parent> threads while
there are still existing I<child> threads.

=item Creating threads inside special blocks

Creating threads inside C<BEGIN>, C<CHECK> or C<INIT> blocks should not be
relied upon.  Depending on the Perl version and the application code, results
may range from success, to (apparently harmless) warnings of leaked scalar, or
all the way up to crashing of the Perl interpreter.

=item Unsafe signals

Since Perl 5.8.0, signals have been made safer in Perl by postponing their
handling until the interpreter is in a I<safe> state.  See
L<perl58delta/"Safe Signals"> and L<perlipc/"Deferred Signals (Safe Signals)">
for more details.

Safe signals is the default behavior, and the old, immediate, unsafe
signalling behavior is only in effect in the following situations:

=over 4

=item * Perl has been built with C<PERL_OLD_SIGNALS> (see C<perl -V>).

=item * The environment variable C<PERL_SIGNALS> is set to C<unsafe> (see L<perlrun/"PERL_SIGNALS">).

=item * The module L<Perl::Unsafe::Signals> is used.

=back

If unsafe signals is in effect, then signal handling is not thread-safe, and
the C<-E<gt>kill()> signalling method cannot be used.

=item Returning closures from threads

Returning closures from threads should not be relied upon.  Depending of the
Perl version and the application code, results may range from success, to
(apparently harmless) warnings of leaked scalar, or all the way up to crashing
of the Perl interpreter.

=item Returning objects from threads

Returning objects from threads does not work.  Depending on the classes
involved, you may be able to work around this by returning a serialized
version of the object (e.g., using L<Data::Dumper> or L<Storable>), and then
reconstituting it in the joining thread.  If you're using Perl 5.10.0 or
later, and if the class supports L<shared objects|threads::shared/"OBJECTS">,
you can pass them via L<shared queues| Thread::Queue>.

=item Perl Bugs and the CPAN Version of L<threads>

Support for threads extends beyond the code in this module (i.e.,
F<threads.pm> and F<threads.xs>), and into the Perl interpreter itself.  Older
versions of Perl contain bugs that may manifest themselves despite using the
latest version of L<threads> from CPAN.  There is no workaround for this other
than upgrading to the latest version of Perl.

Even with the latest version of Perl, it is known that certain constructs
with threads may result in warning messages concerning leaked scalars or
unreferenced scalars.  However, such warnings are harmless, and may safely be
ignored.

You can search for L<threads> related bug reports at
L<http://rt.cpan.org/Public/>.  If needed submit any new bugs, problems,
patches, etc. to: L<http://rt.cpan.org/Public/Dist/Display.html?Name=threads>

=back

=head1 REQUIREMENTS

Perl 5.8.0 or later

=head1 SEE ALSO

L<threads> Discussion Forum on CPAN:
L<http://www.cpanforum.com/dist/threads>

Annotated POD for L<threads>:
L<http://annocpan.org/~JDHEDDEN/threads-1.69/threads.pm>

Source repository:
L<http://code.google.com/p/threads-shared/>

L<threads::shared>, L<perlthrtut>

L<http://www.perl.com/pub/a/2002/06/11/threads.html> and
L<http://www.perl.com/pub/a/2002/09/04/threads.html>

Perl threads mailing list:
L<http://lists.cpan.org/showlist.cgi?name=iThreads>

Stack size discussion:
L<http://www.perlmonks.org/?node_id=532956>

=head1 AUTHOR

Artur Bergman E<lt>sky AT crucially DOT netE<gt>

threads is released under the same license as Perl.

CPAN version produced by Jerry D. Hedden <jdhedden AT cpan DOT org>

=head1 ACKNOWLEDGEMENTS

Richard Soderberg E<lt>perl AT crystalflame DOT netE<gt> -
Helping me out tons, trying to find reasons for races and other weird bugs!

Simon Cozens E<lt>simon AT brecon DOT co DOT ukE<gt> -
Being there to answer zillions of annoying questions

Rocco Caputo E<lt>troc AT netrus DOT netE<gt>

Vipul Ved Prakash E<lt>mail AT vipul DOT netE<gt> -
Helping with debugging

Dean Arnold E<lt>darnold AT presicient DOT comE<gt> -
Stack size API

=cut
