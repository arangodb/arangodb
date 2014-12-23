use strict;
use warnings;
package Test::Reporter::Transport::File;
use base 'Test::Reporter::Transport';
use vars qw/$VERSION/;
$VERSION = '1.4002';
$VERSION = eval $VERSION;

sub new {
  my ($class, $dir) = @_;

  die "target directory '$dir' doesn't exist or can't be written to"
    unless -d $dir && -w $dir;

  return bless { dir => $dir } => $class;
}

sub send {
    my ($self, $report) = @_;
    $report->dir( $self->{dir} );
    return $report->write();
}

1;

__END__

=head1 NAME

Test::Reporter::Transport::File - File transport for Test::Reporter

=head1 SYNOPSIS

    my $report = Test::Reporter->new(
        transport => 'File',
        transport_args => [ $dir ],
    );

=head1 DESCRIPTION

This module saves a Test::Reporter report to the specified directory (using
the C<write> method from Test::Reporter.

This lets you save reports during offline operation.  The files may later be
uploaded using C<< Test::Reporter->read() >>.

    Test::Reporter->read( $file )->send();

=head1 USAGE

See L<Test::Reporter> and L<Test::Reporter::Transport> for general usage
information.

=head2 Transport Arguments

    $report->transport_args( $dir );

This transport class must have a writeable directory as its argument.

=head1 METHODS

These methods are only for internal use by Test::Reporter.

=head2 new

    my $sender = Test::Reporter::Transport::File->new( $dir ); 
    
The C<new> method is the object constructor.   

=head2 send

    $sender->send( $report );

The C<send> method transmits the report.  

=head1 AUTHOR

=over

=item *

David A. Golden (DAGOLDEN)

=item *

Ricardo Signes (RJBS)

=back

=head1 COPYRIGHT

 Copyright (C) 2008 David A. Golden
 Copyright (C) 2008 Ricardo Signes

 All rights reserved.

=head1 LICENSE

This program is free software; you may redistribute it and/or modify it under
the same terms as Perl itself.

=cut

