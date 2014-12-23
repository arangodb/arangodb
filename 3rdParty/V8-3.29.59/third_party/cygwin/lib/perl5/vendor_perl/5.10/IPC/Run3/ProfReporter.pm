package IPC::Run3::ProfReporter;

$VERSION = 0.000_1;

=head1 NAME

IPC::Run3::ProfReporter - base class for handling profiling data

=head1 SYNOPSIS

=head1 DESCRIPTION

See L<IPC::Run3::ProfPP|IPC::Run3::ProfPP> and for an example subclass.

This class just notes and accumulates times; subclasses use methods like
"handle_app_call", "handle_run_exit" and "handle_app_exit" to emit reports on
it.  The default methods for these handlers are noops.

If run from the command line, a reporter will be created and run on
each logfile given as a command line parameter or on run3.out if none
are given.

This allows reports to be run like:

    perl -MIPC::Run3::ProfPP -e1
    perl -MIPC::Run3::ProfPP -e1 foo.out bar.out

Use "-" to read from STDIN (the log file format is meant to be moderately
greppable):

    grep "^cvs " run3.out perl -MIPC::Run3::ProfPP -e1 -

Use --app to show only application level statistics (ie don't emit
a report section for each command run).

=cut

use strict;

my $loaded_by;

sub import {
    $loaded_by = shift;
}

END {
    my @caller;
    for ( my $i = 0;; ++$i ) {
        my @c = caller $i;
        last unless @c;
        @caller = @c;
    }

    if ( $caller[0] eq "main"
        && $caller[1] eq "-e"
    ) {
        require IPC::Run3::ProfLogReader;
        require Getopt::Long;
        my ( $app, $run );

        Getopt::Long::GetOptions(
            "app" => \$app,
            "run" => \$run,
        );

        $app = 1, $run = 1 unless $app || $run;

        for ( @ARGV ? @ARGV : "" ) {
            my $r = IPC::Run3::ProfLogReader->new(
                Source  => $_,
                Handler => $loaded_by->new(
                    Source => $_,
                    app_report => $app,
                    run_report => $run,
                ),
            );
            $r->read_all;
        }
    }
}

=head1 METHODS

=over

=item C<< IPC::Run3::ProfReporter->new >>

Returns a new profile reporting object.

=cut

sub new {
    my $class = ref $_[0] ? ref shift : shift;
    my $self = bless { @_ }, $class;
    $self->{app_report} = 1, $self->{run_report} = 1
        unless $self->{app_report} || $self->{run_report};

    return $self;
}

=item C<< $reporter->handle_app_call( ... ) >>

=item C<< $reporter->handle_app_exit( ... ) >>

=item C<< $reporter->handle_run_exit( ... ) >>

These methods are called by the handled events (see below).

=cut

sub handle_app_call {}
sub handle_app_exit {}

sub handle_run_exit {}

=item C<< $reporter->app_call(\@cmd, $time) >>

=item C<< $reporter->app_exit($time) >>

=item C<< $reporter->run_exit(@times) >>

   $self->app_call( $time );
   my $time = $self->get_app_call_time;

Sets the time (in floating point seconds) when the application, run3(),
or system() was called or exited.  If no time parameter is passed, uses
IPC::Run3's time routine.

Use get_...() to retrieve these values (and _accum values, too).  This
is a separate method to speed the execution time of the setters just a
bit.

=cut

sub app_call {
    my $self = shift;
    ( $self->{app_cmd}, $self->{app_call_time} ) = @_;
    $self->handle_app_call if $self->{app_report};
}

sub app_exit {
    my $self = shift;
    $self->{app_exit_time} = shift;
    $self->handle_app_exit if $self->{app_report};
}

sub run_exit {
    my $self = shift;
    @{$self}{qw(
        run_cmd run_call_time sys_call_time sys_exit_time run_exit_time
    )} = @_;

    ++$self->{run_count};
    $self->{run_cumulative_time} += $self->get_run_time;
    $self->{sys_cumulative_time} += $self->get_sys_time;
    $self->handle_run_exit if $self->{run_report};
}

=item C<< $reporter->get_run_count() >>

=item C<< $reporter->get_app_call_time() >>

=item C<< $reporter->get_app_exit_time() >>

=item C<< $reporter->get_app_cmd() >>

=item C<< $reporter->get_app_time() >>

=cut

sub get_run_count     { shift->{run_count} }
sub get_app_call_time { shift->{app_call_time} }
sub get_app_exit_time { shift->{app_exit_time} }
sub get_app_cmd       { shift->{app_cmd}       }
sub get_app_time {
    my $self = shift;
    $self->get_app_exit_time - $self->get_app_call_time;
}

=item C<< $reporter->get_app_cumulative_time() >>

=cut

sub get_app_cumulative_time {
    my $self = shift;
    $self->get_app_exit_time - $self->get_app_call_time;
}

=item C<< $reporter->get_run_call_time() >>

=item C<< $reporter->get_run_exit_time() >>

=item C<< $reporter->get_run_time() >>

=cut

sub get_run_call_time { shift->{run_call_time} }
sub get_run_exit_time { shift->{run_exit_time} }
sub get_run_time {
    my $self = shift;
    $self->get_run_exit_time - $self->get_run_call_time;
}

=item C<< $reporter->get_run_cumulative_time() >>

=cut

sub get_run_cumulative_time { shift->{run_cumulative_time} }

=item C<< $reporter->get_sys_call_time() >>

=item C<< $reporter->get_sys_exit_time() >>

=item C<< $reporter->get_sys_time() >>

=cut

sub get_sys_call_time { shift->{sys_call_time} }
sub get_sys_exit_time { shift->{sys_exit_time} }
sub get_sys_time {
    my $self = shift;
    $self->get_sys_exit_time - $self->get_sys_call_time;
}

=item C<< $reporter->get_sys_cumulative_time() >>

=cut

sub get_sys_cumulative_time { shift->{sys_cumulative_time} }

=item C<< $reporter->get_run_cmd() >>

=cut

sub get_run_cmd { shift->{run_cmd} }

=back

=head1 LIMITATIONS

=head1 COPYRIGHT

    Copyright 2003, R. Barrie Slaymaker, Jr., All Rights Reserved

=head1 LICENSE

You may use this module under the terms of the BSD, Artistic, or GPL licenses,
any version.

=head1 AUTHOR

Barrie Slaymaker <barries@slaysys.com>

=cut

1;
