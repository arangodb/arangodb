package Pod::Plainer;
use strict;
use Pod::Parser;
our @ISA = qw(Pod::Parser);
our $VERSION = '0.01';

our %E = qw( < lt > gt );
 
sub escape_ltgt {
    (undef, my $text) = @_;
    $text =~ s/([<>])/E<$E{$1}>/g;
    $text 
} 

sub simple_delimiters {
    (undef, my $seq) = @_;
    $seq -> left_delimiter( '<' ); 
    $seq -> right_delimiter( '>' );  
    $seq;
}

sub textblock {
    my($parser,$text,$line) = @_;
    print {$parser->output_handle()}
	$parser->parse_text(
	    { -expand_text => q(escape_ltgt),
	      -expand_seq => q(simple_delimiters) },
	    $text, $line ) -> raw_text(); 
}

1;

__END__

=head1 NAME

Pod::Plainer - Perl extension for converting Pod to old style Pod.

=head1 SYNOPSIS

  use Pod::Plainer;

  my $parser = Pod::Plainer -> new ();
  $parser -> parse_from_filehandle(\*STDIN);

=head1 DESCRIPTION

Pod::Plainer uses Pod::Parser which takes Pod with the (new)
'CE<lt>E<lt> .. E<gt>E<gt>' constructs
and returns the old(er) style with just 'CE<lt>E<gt>';
'<' and '>' are replaced by 'EE<lt>ltE<gt>' and 'EE<lt>gtE<gt>'.

This can be used to pre-process Pod before using tools which do not
recognise the new style Pods.

=head2 EXPORT

None by default.

=head1 AUTHOR

Robin Barker, rmb1@cise.npl.co.uk

=head1 SEE ALSO

See L<Pod::Parser>.

=cut

