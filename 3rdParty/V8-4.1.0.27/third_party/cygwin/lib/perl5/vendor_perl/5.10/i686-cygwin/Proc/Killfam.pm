$Proc::Killfam::VERSION = '1.0';

package Proc::Killfam;

use Exporter;
use base qw/Exporter/;
use subs qw/get_pids/;
use vars qw/@EXPORT @EXPORT_OK $ppt_OK/;
use strict;

@EXPORT = qw/killfam/;
@EXPORT_OK = qw/killfam/;

BEGIN {
    $ppt_OK = 1;
    eval "require Proc::ProcessTable";
    if ($@) {
	$ppt_OK = 0;
	warn "Proc::ProcessTable missing, can't kill sub-children.";
    }
}

sub killfam {

    my($signal, @pids) = @_;

    if ($ppt_OK) {
	my $pt = Proc::ProcessTable->new;
	my(@procs) =  @{$pt->table};
	my(@kids) = get_pids \@procs, @pids;
	@pids = (@pids, @kids);
    }

    kill $signal, @pids;

} # end killfam

sub get_pids {

    my($procs, @kids) = @_;

    my @pids;
    foreach my $kid (@kids) {
	foreach my $proc (@$procs) {
	    if ($proc->ppid == $kid) {
		my $pid = $proc->pid;
		push @pids, $pid, get_pids $procs, $pid;
	    } 
	}
    }
    @pids;

} # end get_pids

1;

__END__

=head1 NAME

Proc::Killfam - kill a list of pids, and all their sub-children

=head1 SYNOPSIS

 use Proc::Killfam;
 killfam $signal, @pids;

=head1 DESCRIPTION

B<killfam> accepts the same arguments as the Perl builtin B<kill> command,
but, additionally, recursively searches the process table for children and
kills them as well.

=head1 EXAMPLE

B<killfam 'TERM', ($pid1, $pid2, @more_pids)>;

=head1 KEYWORDS

kill, signal

=cut

