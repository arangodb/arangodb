package IPC::Run3::ProfPP;

$VERSION = 0.000_1;

=head1 NAME

IPC::Run3::ProfPP - Generate reports from IPC::Run3 profiling data

=head1 SYNOPSIS

=head1 DESCRIPTION

Used by IPC::Run3 and/or run3profpp to print out profiling reports for
human readers.  Use other classes for extracting data in other ways.

The output methods are plain text, override these (see the source for
now) to provide other formats.

This class generates reports on each run3_exit() and app_exit() call.

=cut

require IPC::Run3::ProfReporter;
@ISA = qw( IPC::Run3::ProfReporter );

use strict;
use POSIX qw( floor );

=head1 METHODS

=head2 C<< IPC::Run3::ProfPP->new() >>

Returns a new profile reporting object.

=cut

sub _emit { shift; warn @_ }

sub _t {
    sprintf "%10.6f secs", @_;
}

sub _r {
    my ( $num, $denom ) = @_;
    return () unless $denom;
    sprintf "%10.6f", $num / $denom;
}

sub _pct {
    my ( $num, $denom ) = @_;
    return () unless $denom;
    sprintf  " (%3d%%)", floor( 100 * $num / $denom + 0.5 );
}

=head2 C<< $profpp->handle_app_call() >>

=cut

sub handle_app_call {
    my $self = shift;
    $self->_emit("IPC::Run3 parent: ",
        join( " ", @{$self->get_app_cmd} ),
        "\n",
    );

    $self->{NeedNL} = 1;
}

=head2 C<< $profpp->handle_app_exit() >>

=cut

sub handle_app_exit {
    my $self = shift;

    $self->_emit("\n") if $self->{NeedNL} && $self->{NeedNL} != 1;

    $self->_emit( "IPC::Run3 total elapsed:             ",
        _t( $self->get_app_cumulative_time ),
        "\n");
    $self->_emit( "IPC::Run3 calls to run3():    ",
        sprintf( "%10d", $self->get_run_count ),
        "\n");
    $self->_emit( "IPC::Run3 total spent in run3():     ",
        _t( $self->get_run_cumulative_time ),
        _pct( $self->get_run_cumulative_time, $self->get_app_cumulative_time ),
        ", ",
        _r( $self->get_run_cumulative_time, $self->get_run_count ),
        " per call",
        "\n");
    my $exclusive = 
        $self->get_app_cumulative_time - $self->get_run_cumulative_time;
    $self->_emit( "IPC::Run3 total spent not in run3(): ",
        _t( $exclusive ),
        _pct( $exclusive, $self->get_app_cumulative_time ),
        "\n");
    $self->_emit( "IPC::Run3 total spent in children:   ",
        _t( $self->get_sys_cumulative_time ),
        _pct( $self->get_sys_cumulative_time, $self->get_app_cumulative_time ),
        ", ",
        _r( $self->get_sys_cumulative_time, $self->get_run_count ),
        " per call",
        "\n");
    my $overhead =
        $self->get_run_cumulative_time - $self->get_sys_cumulative_time;
    $self->_emit( "IPC::Run3 total overhead:            ",
        _t( $overhead ),
        _pct(
            $overhead,
            $self->get_sys_cumulative_time
        ),
        ", ",
        _r( $overhead, $self->get_run_count ),
        " per call",
        "\n");
}

=head2 C<< $profpp->handle_run_exit() >>

=cut

sub handle_run_exit {
    my $self = shift;
    my $overhead = $self->get_run_time - $self->get_sys_time;

    $self->_emit("\n") if $self->{NeedNL} && $self->{NeedNL} != 2;
    $self->{NeedNL} = 3;

    $self->_emit( "IPC::Run3 child: ",
        join( " ", @{$self->get_run_cmd} ),
        "\n");
    $self->_emit( "IPC::Run3 run3()  : ", _t( $self->get_run_time ), "\n",
         "IPC::Run3 child   : ", _t( $self->get_sys_time ), "\n",
         "IPC::Run3 overhead: ", _t( $overhead ),
             _pct( $overhead, $self->get_sys_time ),
             "\n");
}

=head1 LIMITATIONS

=head1 COPYRIGHT

    Copyright 2003, R. Barrie Slaymaker, Jr., All Rights Reserved

=head1 LICENSE

You may use this module under the terms of the BSD, Artistic, or GPL licenses,
any version.

=head1 AUTHOR

Barrie Slaymaker E<lt>barries@slaysys.comE<gt>

=cut

1;
