package IPC::Run3::ProfLogger;

$VERSION = 0.038;

=head1 NAME

IPC::Run3::ProfLogger - write profiling data to a log file

=head1 SYNOPSIS

 use IPC::Run3::ProfLogger;

 my $logger = IPC::Run3::ProfLogger->new;  ## write to "run3.out"
 my $logger = IPC::Run3::ProfLogger->new( Destination => $fn );

 $logger->app_call( \@cmd, $time );

 $logger->run_exit( \@cmd1, @times1 );
 $logger->run_exit( \@cmd1, @times1 );

 $logger->app_exit( $time );

=head1 DESCRIPTION

Used by IPC::Run3 to write a profiling log file.  Does not
generate reports or maintain statistics; its meant to have minimal
overhead.

Its API is compatible with a tiny subset of the other IPC::Run profiling
classes.

=cut

use strict;

=head1 METHODS

=head2 C<< IPC::Run3::ProfLogger->new( ... ) >>

=cut

sub new {
    my $class = ref $_[0] ? ref shift : shift;
    my $self = bless { @_ }, $class;
    
    $self->{Destination} = "run3.out"
        unless defined $self->{Destination} && length $self->{Destination};

    open PROFILE, ">$self->{Destination}"
        or die "$!: $self->{Destination}\n";
    binmode PROFILE;
    $self->{FH} = *PROFILE{IO};

    $self->{times} = [];
    return $self;
}

=head2 C<< $logger->run_exit( ... ) >>

=cut

sub run_exit {
    my $self = shift;
    my $fh = $self->{FH};
    print( $fh
        join(
            " ",
            (
                map {
                    my $s = $_;
                    $s =~ s/\\/\\\\/g;
                    $s =~ s/ /_/g;
                    $s;
                } @{shift()}
            ),
            join(
                ",",
                @{$self->{times}},
                @_,
            ),
        ),
        "\n"
    );
}

=head2 C<< $logger->app_exit( $arg ) >>

=cut

sub app_exit {
    my $self = shift;
    my $fh = $self->{FH};
    print $fh "\\app_exit ", shift, "\n";
}

=head2 C<< $logger->app_call( $t, @args) >>

=cut

sub app_call {
    my $self = shift;
    my $fh = $self->{FH};
    my $t = shift;
    print( $fh
        join(
            " ",
            "\\app_call",
            (
                map {
                    my $s = $_;
                    $s =~ s/\\\\/\\/g;
                    $s =~ s/ /\\_/g;
                    $s;
                } @_
            ),
            $t,
        ),
        "\n"
    );
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
