package sigtrap;

=head1 NAME

sigtrap - Perl pragma to enable simple signal handling

=cut

use Carp;

$VERSION = 1.04;
$Verbose ||= 0;

sub import {
    my $pkg = shift;
    my $handler = \&handler_traceback;
    my $saw_sig = 0;
    my $untrapped = 0;
    local $_;

  Arg_loop:
    while (@_) {
	$_ = shift;
	if (/^[A-Z][A-Z0-9]*$/) {
	    $saw_sig++;
	    unless ($untrapped and $SIG{$_} and $SIG{$_} ne 'DEFAULT') {
		print "Installing handler $handler for $_\n" if $Verbose;
		$SIG{$_} = $handler;
	    }
	}
	elsif ($_ eq 'normal-signals') {
	    unshift @_, grep(exists $SIG{$_}, qw(HUP INT PIPE TERM));
	}
	elsif ($_ eq 'error-signals') {
	    unshift @_, grep(exists $SIG{$_},
			     qw(ABRT BUS EMT FPE ILL QUIT SEGV SYS TRAP));
	}
	elsif ($_ eq 'old-interface-signals') {
	    unshift @_,
	    grep(exists $SIG{$_},
		 qw(ABRT BUS EMT FPE ILL PIPE QUIT SEGV SYS TERM TRAP));
	}
    	elsif ($_ eq 'stack-trace') {
	    $handler = \&handler_traceback;
	}
	elsif ($_ eq 'die') {
	    $handler = \&handler_die;
	}
	elsif ($_ eq 'handler') {
	    @_ or croak "No argument specified after 'handler'";
	    $handler = shift;
	    unless (ref $handler or $handler eq 'IGNORE'
			or $handler eq 'DEFAULT') {
    	    	require Symbol;
		$handler = Symbol::qualify($handler, (caller)[0]);
	    }
	}
	elsif ($_ eq 'untrapped') {
	    $untrapped = 1;
	}
	elsif ($_ eq 'any') {
	    $untrapped = 0;
	}
	elsif ($_ =~ /^\d/) {
	    $VERSION >= $_ or croak "sigtrap.pm version $_ required,"
		    	    	    	. " but this is only version $VERSION";
	}
	else {
	    croak "Unrecognized argument $_";
	}
    }
    unless ($saw_sig) {
	@_ = qw(old-interface-signals);
	goto Arg_loop;
    }
}

sub handler_die {
    croak "Caught a SIG$_[0]";
}

sub handler_traceback {
    package DB;		# To get subroutine args.
    $SIG{'ABRT'} = DEFAULT;
    kill 'ABRT', $$ if $panic++;
    syswrite(STDERR, 'Caught a SIG', 12);
    syswrite(STDERR, $_[0], length($_[0]));
    syswrite(STDERR, ' at ', 4);
    ($pack,$file,$line) = caller;
    syswrite(STDERR, $file, length($file));
    syswrite(STDERR, ' line ', 6);
    syswrite(STDERR, $line, length($line));
    syswrite(STDERR, "\n", 1);

    # Now go for broke.
    for ($i = 1; ($p,$f,$l,$s,$h,$w,$e,$r) = caller($i); $i++) {
        @a = ();
	for (@args) {
	    s/([\'\\])/\\$1/g;
	    s/([^\0]*)/'$1'/
	      unless /^(?: -?[\d.]+ | \*[\w:]* )$/x;
	    s/([\200-\377])/sprintf("M-%c",ord($1)&0177)/eg;
	    s/([\0-\37\177])/sprintf("^%c",ord($1)^64)/eg;
	    push(@a, $_);
	}
	$w = $w ? '@ = ' : '$ = ';
	$a = $h ? '(' . join(', ', @a) . ')' : '';
	$e =~ s/\n\s*\;\s*\Z// if $e;
	$e =~ s/[\\\']/\\$1/g if $e;
	if ($r) {
	    $s = "require '$e'";
	} elsif (defined $r) {
	    $s = "eval '$e'";
	} elsif ($s eq '(eval)') {
	    $s = "eval {...}";
	}
	$f = "file `$f'" unless $f eq '-e';
	$mess = "$w$s$a called from $f line $l\n";
	syswrite(STDERR, $mess, length($mess));
    }
    kill 'ABRT', $$;
}

1;

__END__

=head1 SYNOPSIS

    use sigtrap;
    use sigtrap qw(stack-trace old-interface-signals);	# equivalent
    use sigtrap qw(BUS SEGV PIPE ABRT);
    use sigtrap qw(die INT QUIT);
    use sigtrap qw(die normal-signals);
    use sigtrap qw(die untrapped normal-signals);
    use sigtrap qw(die untrapped normal-signals
		    stack-trace any error-signals);
    use sigtrap 'handler' => \&my_handler, 'normal-signals';
    use sigtrap qw(handler my_handler normal-signals
    	    	    stack-trace error-signals);

=head1 DESCRIPTION

The B<sigtrap> pragma is a simple interface to installing signal
handlers.  You can have it install one of two handlers supplied by
B<sigtrap> itself (one which provides a Perl stack trace and one which
simply C<die()>s), or alternately you can supply your own handler for it
to install.  It can be told only to install a handler for signals which
are either untrapped or ignored.  It has a couple of lists of signals to
trap, plus you can supply your own list of signals.

The arguments passed to the C<use> statement which invokes B<sigtrap>
are processed in order.  When a signal name or the name of one of
B<sigtrap>'s signal lists is encountered a handler is immediately
installed, when an option is encountered it affects subsequently
installed handlers.

=head1 OPTIONS

=head2 SIGNAL HANDLERS

These options affect which handler will be used for subsequently
installed signals.

=over 4

=item B<stack-trace>

The handler used for subsequently installed signals outputs a Perl stack
trace to STDERR and then tries to dump core.  This is the default signal
handler.

=item B<die>

The handler used for subsequently installed signals calls C<die>
(actually C<croak>) with a message indicating which signal was caught.

=item B<handler> I<your-handler>

I<your-handler> will be used as the handler for subsequently installed
signals.  I<your-handler> can be any value which is valid as an
assignment to an element of C<%SIG>. See L<perlvar> for examples of
handler functions.

=back

=head2 SIGNAL LISTS

B<sigtrap> has a few built-in lists of signals to trap.  They are:

=over 4

=item B<normal-signals>

These are the signals which a program might normally expect to encounter
and which by default cause it to terminate.  They are HUP, INT, PIPE and
TERM.

=item B<error-signals>

These signals usually indicate a serious problem with the Perl
interpreter or with your script.  They are ABRT, BUS, EMT, FPE, ILL,
QUIT, SEGV, SYS and TRAP.

=item B<old-interface-signals>

These are the signals which were trapped by default by the old
B<sigtrap> interface, they are ABRT, BUS, EMT, FPE, ILL, PIPE, QUIT,
SEGV, SYS, TERM, and TRAP.  If no signals or signals lists are passed to
B<sigtrap>, this list is used.

=back

For each of these three lists, the collection of signals set to be
trapped is checked before trapping; if your architecture does not
implement a particular signal, it will not be trapped but rather
silently ignored.

=head2 OTHER

=over 4

=item B<untrapped>

This token tells B<sigtrap> to install handlers only for subsequently
listed signals which aren't already trapped or ignored.

=item B<any>

This token tells B<sigtrap> to install handlers for all subsequently
listed signals.  This is the default behavior.

=item I<signal>

Any argument which looks like a signal name (that is,
C</^[A-Z][A-Z0-9]*$/>) indicates that B<sigtrap> should install a
handler for that name.

=item I<number>

Require that at least version I<number> of B<sigtrap> is being used.

=back

=head1 EXAMPLES

Provide a stack trace for the old-interface-signals:

    use sigtrap;

Ditto:

    use sigtrap qw(stack-trace old-interface-signals);

Provide a stack trace on the 4 listed signals only:

    use sigtrap qw(BUS SEGV PIPE ABRT);

Die on INT or QUIT:

    use sigtrap qw(die INT QUIT);

Die on HUP, INT, PIPE or TERM:

    use sigtrap qw(die normal-signals);

Die on HUP, INT, PIPE or TERM, except don't change the behavior for
signals which are already trapped or ignored:

    use sigtrap qw(die untrapped normal-signals);

Die on receipt one of an of the B<normal-signals> which is currently
B<untrapped>, provide a stack trace on receipt of B<any> of the
B<error-signals>:

    use sigtrap qw(die untrapped normal-signals
		    stack-trace any error-signals);

Install my_handler() as the handler for the B<normal-signals>:

    use sigtrap 'handler', \&my_handler, 'normal-signals';

Install my_handler() as the handler for the normal-signals, provide a
Perl stack trace on receipt of one of the error-signals:

    use sigtrap qw(handler my_handler normal-signals
    	    	    stack-trace error-signals);

=cut
