package IPC::Run3::ProfLogReader;

$VERSION = 0.038;

=head1 NAME

IPC::Run3::ProfLogReader -  read and process a ProfLogger file

=head1 SYNOPSIS

 use IPC::Run3::ProfLogReader;

 my $reader = IPC::Run3::ProfLogReader->new; ## use "run3.out"
 my $reader = IPC::Run3::ProfLogReader->new( Source => $fn );

 my $profiler = IPC::Run3::ProfPP;   ## For example
 my $reader   = IPC::Run3::ProfLogReader->new( ..., Handler => $p );

 $reader->read;
 $eaderr->read_all;

=head1 DESCRIPTION

Reads a log file.  Use the filename "-" to read from STDIN.

=cut

use strict;

=head1 METHODS

=head2 C<< IPC::Run3::ProfLogReader->new( ... ) >>

=cut

sub new {
    my $class = ref $_[0] ? ref shift : shift;
    my $self = bless { @_ }, $class;
    
    $self->{Source} = "run3.out"
        unless defined $self->{Source} && length $self->{Source};

    my $source = $self->{Source};

    if ( ref $source eq "GLOB" || UNIVERSAL::isa( $source, "IO::Handle" ) ) {
        $self->{FH} = $source;
    }
    elsif ( $source eq "-" ) {
        $self->{FH} = \*STDIN;
    }
    else {
        open PROFILE, "<$self->{Source}" or die "$!: $self->{Source}\n";
        $self->{FH} = *PROFILE{IO};
    }
    return $self;
}


=head2 C<< $reader->set_handler( $handler ) >>

=cut

sub set_handler { $_[0]->{Handler} = $_[1] }

=head2 C<< $reader->get_handler() >>

=cut

sub get_handler { $_[0]->{Handler} }

=head2 C<< $reader->read() >>

=cut

sub read {
    my $self = shift;

    my $fh = $self->{FH};
    my @ln = split / /, <$fh>;

    return 0 unless @ln;
    return 1 unless $self->{Handler};

    chomp $ln[-1];

    ## Ignore blank and comment lines.
    return 1 if @ln == 1 && ! length $ln[0] || 0 == index $ln[0], "#";

    if ( $ln[0] eq "\\app_call" ) {
        shift @ln;
        my @times = split /,/, pop @ln;
        $self->{Handler}->app_call(
            [
                map {
                    s/\\\\/\\/g;
                    s/\\_/ /g;
                    $_;
                } @ln
            ],
            @times
        );
    }
    elsif ( $ln[0] eq "\\app_exit" ) {
        shift @ln;
        $self->{Handler}->app_exit( pop @ln, @ln );
    }
    else {
        my @times = split /,/, pop @ln;
        $self->{Handler}->run_exit(
            [
                map {
                    s/\\\\/\\/g;
                    s/\\_/ /g;
                    $_;
                } @ln
            ],
            @times
        );
    }

    return 1;
}


=head2 C<< $reader->read_all() >>

This method reads until there is nothing left to read, and then returns true.

=cut

sub read_all {
    my $self = shift;

    1 while $self->read;

    return 1;
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
