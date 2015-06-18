#
# Kill all instances of a process by pattern-matching the command-line
#
# (c) 2000 by Aaron Sherman, see documentation, below for details.

package Proc::Killall;

require Exporter;
use Carp;
use Proc::ProcessTable;
use Config;
use strict;
use vars qw(@EXPORT @EXPORT_OK @ISA $VERSION);

@EXPORT=qw(killall);
@EXPORT_OK=qw(killall);
@ISA=qw(Exporter);

$VERSION='1.0';
sub VERSION {$VERSION}

# Private function for checking to see if a signal identifier is
# valid.
sub is_sig {
	my $sig = shift;
	if (defined($sig)) {
		if ($sig =~ /^-?(\d+)/) {
			my $n = $1;
			my @sigs = split ' ', $Config{sig_num};
			return grep {$_ == $n} @sigs;
		} elsif ($sig =~ /^[A-Z][A-Z0-9]+$/) {
			my @sigs = split ' ', $Config{sig_name};
			return grep {$_ eq $sig} @sigs;
		} else {
			return 0;
		}
	} else {
		return 0;
	}
}

# usage: killall(signal, pattern)
# return: number of procs killed
sub killall {
	croak("Usage: killall(signal, pattern)") unless @_==2;
	my $signal = shift;
	my $pat = shift;
	my $self = shift;
	$self = 0 unless defined $self;
	my $nkilled = 0;
	croak("killall: Unsupported signal: $signal") unless is_sig($signal);
	my $t = new Proc::ProcessTable;
	my $BANG = undef;
	foreach my $p (@{$t->table}) {
	  my $cmndline = $p->{cmndline} || $p->{fname};
	  if ($cmndline =~ /$pat/) {
			next unless $p->pid != $$ || $self;
			if (kill $signal, $p->pid) {
				$nkilled++;
			} else {
				$BANG = $!;
			}
		}
	}
	$! = $BANG if defined $BANG;
	return $nkilled;
}

1;

__END__

=head1 NAME

killall - Kill all instances of a process by pattern matching the command-line

=head1 SYNOPSIS

	use Proc::Killall;

	killall('HUP', 'xterm'); # SIGHUP all xterms
	killall('KILL', '^netscape$'); # SIGKILL to "netscape"

=head1 DESCRIPTION

This module provides one function, C<killall()>, which takes two parameters:
a signal name or number (see C<kill()>) and a process pattern. This pattern
is matched against the process' command-line as the C<ps> command would
show it (C<ps> is not used internally, instead a package called
C<Proc::ProcessTable> is used).

C<killall> searches the process table and sends that signal to all processes
which match the pattern. The return value is the number of processes that
were succesfully signaled. If any kills failed, the C<$!> variable
will be set based on that last one that failed (even if a successful kill
happened afterward).

=head1 AUTHOR

Written in 2000 by Aaron Sherman E<lt>ajs@ajs.comE<gt>

C<Proc::Killall> is copyright 2000 by Aaron Sherman, and may be
distributed under the same terms as Perl itself.

=head1 PREREQUISITES

C<Proc::ProcessTable> is required for C<Proc::Killall> to function.

=head1 SEE ALSO

L<perl>, L<perlfunc>, L<perlvar>, L<Proc::ProcessTable>
