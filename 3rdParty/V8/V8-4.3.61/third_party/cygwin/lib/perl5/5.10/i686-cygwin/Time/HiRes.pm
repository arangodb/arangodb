package Time::HiRes;

use strict;
use vars qw($VERSION $XS_VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

require Exporter;
require DynaLoader;

@ISA = qw(Exporter DynaLoader);

@EXPORT = qw( );
@EXPORT_OK = qw (usleep sleep ualarm alarm gettimeofday time tv_interval
		 getitimer setitimer nanosleep clock_gettime clock_getres
		 clock clock_nanosleep
		 CLOCK_HIGHRES CLOCK_MONOTONIC CLOCK_PROCESS_CPUTIME_ID
		 CLOCK_REALTIME CLOCK_SOFTTIME CLOCK_THREAD_CPUTIME_ID
		 CLOCK_TIMEOFDAY CLOCKS_PER_SEC
		 ITIMER_REAL ITIMER_VIRTUAL ITIMER_PROF ITIMER_REALPROF
		 TIMER_ABSTIME
		 d_usleep d_ualarm d_gettimeofday d_getitimer d_setitimer
		 d_nanosleep d_clock_gettime d_clock_getres
		 d_clock d_clock_nanosleep
		 stat
		);

$VERSION = '1.9715';
$XS_VERSION = $VERSION;
$VERSION = eval $VERSION;

sub AUTOLOAD {
    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    # print "AUTOLOAD: constname = $constname ($AUTOLOAD)\n";
    die "&Time::HiRes::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    # print "AUTOLOAD: error = $error, val = $val\n";
    if ($error) {
        my (undef,$file,$line) = caller;
        die "$error at $file line $line.\n";
    }
    {
	no strict 'refs';
	*$AUTOLOAD = sub { $val };
    }
    goto &$AUTOLOAD;
}

sub import {
    my $this = shift;
    for my $i (@_) {
	if (($i eq 'clock_getres'    && !&d_clock_getres)    ||
	    ($i eq 'clock_gettime'   && !&d_clock_gettime)   ||
	    ($i eq 'clock_nanosleep' && !&d_clock_nanosleep) ||
	    ($i eq 'clock'           && !&d_clock)           ||
	    ($i eq 'nanosleep'       && !&d_nanosleep)       ||
	    ($i eq 'usleep'          && !&d_usleep)          ||
	    ($i eq 'ualarm'          && !&d_ualarm)) {
	    require Carp;
	    Carp::croak("Time::HiRes::$i(): unimplemented in this platform");
	}
    }
    Time::HiRes->export_to_level(1, $this, @_);
}

bootstrap Time::HiRes;

# Preloaded methods go here.

sub tv_interval {
    # probably could have been done in C
    my ($a, $b) = @_;
    $b = [gettimeofday()] unless defined($b);
    (${$b}[0] - ${$a}[0]) + ((${$b}[1] - ${$a}[1]) / 1_000_000);
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Time::HiRes - High resolution alarm, sleep, gettimeofday, interval timers

=head1 SYNOPSIS

  use Time::HiRes qw( usleep ualarm gettimeofday tv_interval nanosleep
		      clock_gettime clock_getres clock_nanosleep clock
                      stat );

  usleep ($microseconds);
  nanosleep ($nanoseconds);

  ualarm ($microseconds);
  ualarm ($microseconds, $interval_microseconds);

  $t0 = [gettimeofday];
  ($seconds, $microseconds) = gettimeofday;

  $elapsed = tv_interval ( $t0, [$seconds, $microseconds]);
  $elapsed = tv_interval ( $t0, [gettimeofday]);
  $elapsed = tv_interval ( $t0 );

  use Time::HiRes qw ( time alarm sleep );

  $now_fractions = time;
  sleep ($floating_seconds);
  alarm ($floating_seconds);
  alarm ($floating_seconds, $floating_interval);

  use Time::HiRes qw( setitimer getitimer );

  setitimer ($which, $floating_seconds, $floating_interval );
  getitimer ($which);

  use Time::HiRes qw( clock_gettime clock_getres clock_nanosleep
		      ITIMER_REAL ITIMER_VIRTUAL ITIMER_PROF ITIMER_REALPROF );

  $realtime   = clock_gettime(CLOCK_REALTIME);
  $resolution = clock_getres(CLOCK_REALTIME);

  clock_nanosleep(CLOCK_REALTIME, 1.5e9);
  clock_nanosleep(CLOCK_REALTIME, time()*1e9 + 10e9, TIMER_ABSTIME);

  my $ticktock = clock();

  use Time::HiRes qw( stat );

  my @stat = stat("file");
  my @stat = stat(FH);

=head1 DESCRIPTION

The C<Time::HiRes> module implements a Perl interface to the
C<usleep>, C<nanosleep>, C<ualarm>, C<gettimeofday>, and
C<setitimer>/C<getitimer> system calls, in other words, high
resolution time and timers. See the L</EXAMPLES> section below and the
test scripts for usage; see your system documentation for the
description of the underlying C<nanosleep> or C<usleep>, C<ualarm>,
C<gettimeofday>, and C<setitimer>/C<getitimer> calls.

If your system lacks C<gettimeofday()> or an emulation of it you don't
get C<gettimeofday()> or the one-argument form of C<tv_interval()>.
If your system lacks all of C<nanosleep()>, C<usleep()>,
C<select()>, and C<poll>, you don't get C<Time::HiRes::usleep()>,
C<Time::HiRes::nanosleep()>, or C<Time::HiRes::sleep()>.
If your system lacks both C<ualarm()> and C<setitimer()> you don't get
C<Time::HiRes::ualarm()> or C<Time::HiRes::alarm()>.

If you try to import an unimplemented function in the C<use> statement
it will fail at compile time.

If your subsecond sleeping is implemented with C<nanosleep()> instead
of C<usleep()>, you can mix subsecond sleeping with signals since
C<nanosleep()> does not use signals.  This, however, is not portable,
and you should first check for the truth value of
C<&Time::HiRes::d_nanosleep> to see whether you have nanosleep, and
then carefully read your C<nanosleep()> C API documentation for any
peculiarities.

If you are using C<nanosleep> for something else than mixing sleeping
with signals, give some thought to whether Perl is the tool you should
be using for work requiring nanosecond accuracies.

Remember that unless you are working on a I<hard realtime> system,
any clocks and timers will be imprecise, especially so if you are working
in a pre-emptive multiuser system.  Understand the difference between
I<wallclock time> and process time (in UNIX-like systems the sum of
I<user> and I<system> times).  Any attempt to sleep for X seconds will
most probably end up sleeping B<more> than that, but don't be surpised
if you end up sleeping slightly B<less>.

The following functions can be imported from this module.
No functions are exported by default.

=over 4

=item gettimeofday ()

In array context returns a two-element array with the seconds and
microseconds since the epoch.  In scalar context returns floating
seconds like C<Time::HiRes::time()> (see below).

=item usleep ( $useconds )

Sleeps for the number of microseconds (millionths of a second)
specified.  Returns the number of microseconds actually slept.
Can sleep for more than one second, unlike the C<usleep> system call.
Can also sleep for zero seconds, which often works like a I<thread yield>.
See also C<Time::HiRes::usleep()>, C<Time::HiRes::sleep()>, and
C<Time::HiRes::clock_nanosleep()>.

Do not expect usleep() to be exact down to one microsecond.

=item nanosleep ( $nanoseconds )

Sleeps for the number of nanoseconds (1e9ths of a second) specified.
Returns the number of nanoseconds actually slept (accurate only to
microseconds, the nearest thousand of them).  Can sleep for more than
one second.  Can also sleep for zero seconds, which often works like
a I<thread yield>.  See also C<Time::HiRes::sleep()>,
C<Time::HiRes::usleep()>, and C<Time::HiRes::clock_nanosleep()>.

Do not expect nanosleep() to be exact down to one nanosecond.
Getting even accuracy of one thousand nanoseconds is good.

=item ualarm ( $useconds [, $interval_useconds ] )

Issues a C<ualarm> call; the C<$interval_useconds> is optional and
will be zero if unspecified, resulting in C<alarm>-like behaviour.

Returns the remaining time in the alarm in microseconds, or C<undef>
if an error occurred.

ualarm(0) will cancel an outstanding ualarm().

Note that the interaction between alarms and sleeps is unspecified.

=item tv_interval 

tv_interval ( $ref_to_gettimeofday [, $ref_to_later_gettimeofday] )

Returns the floating seconds between the two times, which should have
been returned by C<gettimeofday()>. If the second argument is omitted,
then the current time is used.

=item time ()

Returns a floating seconds since the epoch. This function can be
imported, resulting in a nice drop-in replacement for the C<time>
provided with core Perl; see the L</EXAMPLES> below.

B<NOTE 1>: This higher resolution timer can return values either less
or more than the core C<time()>, depending on whether your platform
rounds the higher resolution timer values up, down, or to the nearest second
to get the core C<time()>, but naturally the difference should be never
more than half a second.  See also L</clock_getres>, if available
in your system.

B<NOTE 2>: Since Sunday, September 9th, 2001 at 01:46:40 AM GMT, when
the C<time()> seconds since epoch rolled over to 1_000_000_000, the
default floating point format of Perl and the seconds since epoch have
conspired to produce an apparent bug: if you print the value of
C<Time::HiRes::time()> you seem to be getting only five decimals, not
six as promised (microseconds).  Not to worry, the microseconds are
there (assuming your platform supports such granularity in the first
place).  What is going on is that the default floating point format of
Perl only outputs 15 digits.  In this case that means ten digits
before the decimal separator and five after.  To see the microseconds
you can use either C<printf>/C<sprintf> with C<"%.6f">, or the
C<gettimeofday()> function in list context, which will give you the
seconds and microseconds as two separate values.

=item sleep ( $floating_seconds )

Sleeps for the specified amount of seconds.  Returns the number of
seconds actually slept (a floating point value).  This function can
be imported, resulting in a nice drop-in replacement for the C<sleep>
provided with perl, see the L</EXAMPLES> below.

Note that the interaction between alarms and sleeps is unspecified.

=item alarm ( $floating_seconds [, $interval_floating_seconds ] )

The C<SIGALRM> signal is sent after the specified number of seconds.
Implemented using C<setitimer()> if available, C<ualarm()> if not.
The C<$interval_floating_seconds> argument is optional and will be
zero if unspecified, resulting in C<alarm()>-like behaviour.  This
function can be imported, resulting in a nice drop-in replacement for
the C<alarm> provided with perl, see the L</EXAMPLES> below.

Returns the remaining time in the alarm in seconds, or C<undef>
if an error occurred.

B<NOTE 1>: With some combinations of operating systems and Perl
releases C<SIGALRM> restarts C<select()>, instead of interrupting it.
This means that an C<alarm()> followed by a C<select()> may together
take the sum of the times specified for the the C<alarm()> and the
C<select()>, not just the time of the C<alarm()>.

Note that the interaction between alarms and sleeps is unspecified.

=item setitimer ( $which, $floating_seconds [, $interval_floating_seconds ] )

Start up an interval timer: after a certain time, a signal ($which) arrives,
and more signals may keep arriving at certain intervals.  To disable
an "itimer", use C<$floating_seconds> of zero.  If the
C<$interval_floating_seconds> is set to zero (or unspecified), the
timer is disabled B<after> the next delivered signal.

Use of interval timers may interfere with C<alarm()>, C<sleep()>,
and C<usleep()>.  In standard-speak the "interaction is unspecified",
which means that I<anything> may happen: it may work, it may not.

In scalar context, the remaining time in the timer is returned.

In list context, both the remaining time and the interval are returned.

There are usually three or four interval timers (signals) available: the
C<$which> can be C<ITIMER_REAL>, C<ITIMER_VIRTUAL>, C<ITIMER_PROF>, or
C<ITIMER_REALPROF>.  Note that which ones are available depends: true
UNIX platforms usually have the first three, but (for example) Win32
and Cygwin have only C<ITIMER_REAL>, and only Solaris seems to have
C<ITIMER_REALPROF> (which is used to profile multithreaded programs).

C<ITIMER_REAL> results in C<alarm()>-like behaviour.  Time is counted in
I<real time>; that is, wallclock time.  C<SIGALRM> is delivered when
the timer expires.

C<ITIMER_VIRTUAL> counts time in (process) I<virtual time>; that is,
only when the process is running.  In multiprocessor/user/CPU systems
this may be more or less than real or wallclock time.  (This time is
also known as the I<user time>.)  C<SIGVTALRM> is delivered when the
timer expires.

C<ITIMER_PROF> counts time when either the process virtual time or when
the operating system is running on behalf of the process (such as I/O).
(This time is also known as the I<system time>.)  (The sum of user
time and system time is known as the I<CPU time>.)  C<SIGPROF> is
delivered when the timer expires.  C<SIGPROF> can interrupt system calls.

The semantics of interval timers for multithreaded programs are
system-specific, and some systems may support additional interval
timers.  For example, it is unspecified which thread gets the signals.
See your C<setitimer()> documentation.

=item getitimer ( $which )

Return the remaining time in the interval timer specified by C<$which>.

In scalar context, the remaining time is returned.

In list context, both the remaining time and the interval are returned.
The interval is always what you put in using C<setitimer()>.

=item clock_gettime ( $which )

Return as seconds the current value of the POSIX high resolution timer
specified by C<$which>.  All implementations that support POSIX high
resolution timers are supposed to support at least the C<$which> value
of C<CLOCK_REALTIME>, which is supposed to return results close to the
results of C<gettimeofday>, or the number of seconds since 00:00:00:00
January 1, 1970 Greenwich Mean Time (GMT).  Do not assume that
CLOCK_REALTIME is zero, it might be one, or something else.
Another potentially useful (but not available everywhere) value is
C<CLOCK_MONOTONIC>, which guarantees a monotonically increasing time
value (unlike time(), which can be adjusted).  See your system
documentation for other possibly supported values.

=item clock_getres ( $which )

Return as seconds the resolution of the POSIX high resolution timer
specified by C<$which>.  All implementations that support POSIX high
resolution timers are supposed to support at least the C<$which> value
of C<CLOCK_REALTIME>, see L</clock_gettime>.

=item clock_nanosleep ( $which, $nanoseconds, $flags = 0)

Sleeps for the number of nanoseconds (1e9ths of a second) specified.
Returns the number of nanoseconds actually slept.  The $which is the
"clock id", as with clock_gettime() and clock_getres().  The flags
default to zero but C<TIMER_ABSTIME> can specified (must be exported
explicitly) which means that C<$nanoseconds> is not a time interval
(as is the default) but instead an absolute time.  Can sleep for more
than one second.  Can also sleep for zero seconds, which often works
like a I<thread yield>.  See also C<Time::HiRes::sleep()>,
C<Time::HiRes::usleep()>, and C<Time::HiRes::nanosleep()>.

Do not expect clock_nanosleep() to be exact down to one nanosecond.
Getting even accuracy of one thousand nanoseconds is good.

=item clock()

Return as seconds the I<process time> (user + system time) spent by
the process since the first call to clock() (the definition is B<not>
"since the start of the process", though if you are lucky these times
may be quite close to each other, depending on the system).  What this
means is that you probably need to store the result of your first call
to clock(), and subtract that value from the following results of clock().

The time returned also includes the process times of the terminated
child processes for which wait() has been executed.  This value is
somewhat like the second value returned by the times() of core Perl,
but not necessarily identical.  Note that due to backward
compatibility limitations the returned value may wrap around at about
2147 seconds or at about 36 minutes.

=item stat

=item stat FH

=item stat EXPR

As L<perlfunc/stat> but with the access/modify/change file timestamps
in subsecond resolution, if the operating system and the filesystem
both support such timestamps.  To override the standard stat():

    use Time::HiRes qw(stat);

Test for the value of &Time::HiRes::d_hires_stat to find out whether
the operating system supports subsecond file timestamps: a value
larger than zero means yes. There are unfortunately no easy
ways to find out whether the filesystem supports such timestamps.
UNIX filesystems often do; NTFS does; FAT doesn't (FAT timestamp
granularity is B<two> seconds).

A zero return value of &Time::HiRes::d_hires_stat means that
Time::HiRes::stat is a no-op passthrough for CORE::stat(),
and therefore the timestamps will stay integers.  The same
thing will happen if the filesystem does not do subsecond timestamps,
even if the &Time::HiRes::d_hires_stat is non-zero.

In any case do not expect nanosecond resolution, or even a microsecond
resolution.  Also note that the modify/access timestamps might have
different resolutions, and that they need not be synchronized, e.g.
if the operations are

    write
    stat # t1
    read
    stat # t2

the access time stamp from t2 need not be greater-than the modify
time stamp from t1: it may be equal or I<less>.

=back

=head1 EXAMPLES

  use Time::HiRes qw(usleep ualarm gettimeofday tv_interval);

  $microseconds = 750_000;
  usleep($microseconds);

  # signal alarm in 2.5s & every .1s thereafter
  ualarm(2_500_000, 100_000);
  # cancel that ualarm
  ualarm(0);

  # get seconds and microseconds since the epoch
  ($s, $usec) = gettimeofday();

  # measure elapsed time 
  # (could also do by subtracting 2 gettimeofday return values)
  $t0 = [gettimeofday];
  # do bunch of stuff here
  $t1 = [gettimeofday];
  # do more stuff here
  $t0_t1 = tv_interval $t0, $t1;

  $elapsed = tv_interval ($t0, [gettimeofday]);
  $elapsed = tv_interval ($t0);	# equivalent code

  #
  # replacements for time, alarm and sleep that know about
  # floating seconds
  #
  use Time::HiRes;
  $now_fractions = Time::HiRes::time;
  Time::HiRes::sleep (2.5);
  Time::HiRes::alarm (10.6666666);

  use Time::HiRes qw ( time alarm sleep );
  $now_fractions = time;
  sleep (2.5);
  alarm (10.6666666);

  # Arm an interval timer to go off first at 10 seconds and
  # after that every 2.5 seconds, in process virtual time

  use Time::HiRes qw ( setitimer ITIMER_VIRTUAL time );

  $SIG{VTALRM} = sub { print time, "\n" };
  setitimer(ITIMER_VIRTUAL, 10, 2.5);

  use Time::HiRes qw( clock_gettime clock_getres CLOCK_REALTIME );
  # Read the POSIX high resolution timer.
  my $high = clock_getres(CLOCK_REALTIME);
  # But how accurate we can be, really?
  my $reso = clock_getres(CLOCK_REALTIME);

  use Time::HiRes qw( clock_nanosleep TIMER_ABSTIME );
  clock_nanosleep(CLOCK_REALTIME, 1e6);
  clock_nanosleep(CLOCK_REALTIME, 2e9, TIMER_ABSTIME);

  use Time::HiRes qw( clock );
  my $clock0 = clock();
  ... # Do something.
  my $clock1 = clock();
  my $clockd = $clock1 - $clock0;

  use Time::HiRes qw( stat );
  my ($atime, $mtime, $ctime) = (stat("istics"))[8, 9, 10];

=head1 C API

In addition to the perl API described above, a C API is available for
extension writers.  The following C functions are available in the
modglobal hash:

  name             C prototype
  ---------------  ----------------------
  Time::NVtime     double (*)()
  Time::U2time     void (*)(pTHX_ UV ret[2])

Both functions return equivalent information (like C<gettimeofday>)
but with different representations.  The names C<NVtime> and C<U2time>
were selected mainly because they are operating system independent.
(C<gettimeofday> is Unix-centric, though some platforms like Win32 and
VMS have emulations for it.)

Here is an example of using C<NVtime> from C:

  double (*myNVtime)(); /* Returns -1 on failure. */
  SV **svp = hv_fetch(PL_modglobal, "Time::NVtime", 12, 0);
  if (!svp)         croak("Time::HiRes is required");
  if (!SvIOK(*svp)) croak("Time::NVtime isn't a function pointer");
  myNVtime = INT2PTR(double(*)(), SvIV(*svp));
  printf("The current time is: %f\n", (*myNVtime)());

=head1 DIAGNOSTICS

=head2 useconds or interval more than ...

In ualarm() you tried to use number of microseconds or interval (also
in microseconds) more than 1_000_000 and setitimer() is not available
in your system to emulate that case.

=head2 negative time not invented yet

You tried to use a negative time argument.

=head2 internal error: useconds < 0 (unsigned ... signed ...)

Something went horribly wrong-- the number of microseconds that cannot
become negative just became negative.  Maybe your compiler is broken?

=head2 useconds or uinterval equal to or more than 1000000

In some platforms it is not possible to get an alarm with subsecond
resolution and later than one second.

=head2 unimplemented in this platform

Some calls simply aren't available, real or emulated, on every platform.

=head1 CAVEATS

Notice that the core C<time()> maybe rounding rather than truncating.
What this means is that the core C<time()> may be reporting the time
as one second later than C<gettimeofday()> and C<Time::HiRes::time()>.

Adjusting the system clock (either manually or by services like ntp)
may cause problems, especially for long running programs that assume
a monotonously increasing time (note that all platforms do not adjust
time as gracefully as UNIX ntp does).  For example in Win32 (and derived
platforms like Cygwin and MinGW) the Time::HiRes::time() may temporarily
drift off from the system clock (and the original time())  by up to 0.5
seconds. Time::HiRes will notice this eventually and recalibrate.
Note that since Time::HiRes 1.77 the clock_gettime(CLOCK_MONOTONIC)
might help in this (in case your system supports CLOCK_MONOTONIC).

=head1 SEE ALSO

Perl modules L<BSD::Resource>, L<Time::TAI64>.

Your system documentation for C<clock>, C<clock_gettime>,
C<clock_getres>, C<clock_nanosleep>, C<clock_settime>, C<getitimer>,
C<gettimeofday>, C<setitimer>, C<sleep>, C<stat>, C<ualarm>.

=head1 AUTHORS

D. Wegscheid <wegscd@whirlpool.com>
R. Schertler <roderick@argon.org>
J. Hietaniemi <jhi@iki.fi>
G. Aas <gisle@aas.no>

=head1 COPYRIGHT AND LICENSE

Copyright (c) 1996-2002 Douglas E. Wegscheid.  All rights reserved.

Copyright (c) 2002, 2003, 2004, 2005, 2006, 2007, 2008 Jarkko Hietaniemi.
All rights reserved.

This program is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

=cut
