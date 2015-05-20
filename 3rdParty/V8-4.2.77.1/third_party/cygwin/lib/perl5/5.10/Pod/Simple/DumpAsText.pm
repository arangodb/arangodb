
require 5;
package Pod::Simple::DumpAsText;
$VERSION = '2.02';
use Pod::Simple ();
BEGIN {@ISA = ('Pod::Simple')}

use strict;

use Carp ();

BEGIN { *DEBUG = \&Pod::Simple::DEBUG unless defined &DEBUG }

sub new {
  my $self = shift;
  my $new = $self->SUPER::new(@_);
  $new->{'output_fh'} ||= *STDOUT{IO};
  $new->accept_codes('VerbatimFormatted');
  return $new;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub _handle_element_start {
  # ($self, $element_name, $attr_hash_r)
  my $fh = $_[0]{'output_fh'};
  my($key, $value);
  DEBUG and print "++ $_[1]\n";
  
  print $fh   '  ' x ($_[0]{'indent'} || 0),  "++", $_[1], "\n";
  $_[0]{'indent'}++;
  while(($key,$value) = each %{$_[2]}) {
    unless($key =~ m/^~/s) {
      next if $key eq 'start_line' and $_[0]{'hide_line_numbers'};
      _perly_escape($key);
      _perly_escape($value);
      printf $fh qq{%s \\ "%s" => "%s"\n},
        '  ' x ($_[0]{'indent'} || 0), $key, $value;
    }
  }
  return;
}

sub _handle_text {
  DEBUG and print "== \"$_[1]\"\n";
  
  if(length $_[1]) {
    my $indent = '  ' x $_[0]{'indent'};
    my $text = $_[1];
    _perly_escape($text);
    $text =~  # A not-totally-brilliant wrapping algorithm:
      s/(
         [^\n]{55}         # Snare some characters from a line
         [^\n\ ]{0,50}     #  and finish any current word
        )
        \x20{1,10}(?!\n)   # capture some spaces not at line-end
       /$1"\n$indent . "/gx     # => line-break here
    ;
    
    print {$_[0]{'output_fh'}} $indent, '* "', $text, "\"\n";
  }
  return;
}

sub _handle_element_end {
  DEBUG and print "-- $_[1]\n";
  print {$_[0]{'output_fh'}}
   '  ' x --$_[0]{'indent'}, "--", $_[1], "\n";
  return;
}

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

sub _perly_escape {
  foreach my $x (@_) {
    $x =~ s/([^\x00-\xFF])/sprintf'\x{%X}',ord($1)/eg;
    # Escape things very cautiously:
    $x =~ s/([^-\n\t \&\<\>\'!\#\%\(\)\*\+,\.\/\:\;=\?\~\[\]\^_\`\{\|\}abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789])/sprintf'\x%02X',ord($1)/eg;
  }
  return;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
1;


__END__

=head1 NAME

Pod::Simple::DumpAsText -- dump Pod-parsing events as text

=head1 SYNOPSIS

  perl -MPod::Simple::DumpAsText -e \
   "exit Pod::Simple::DumpAsText->filter(shift)->any_errata_seen" \
   thingy.pod

=head1 DESCRIPTION

This class is for dumping, as text, the events gotten from parsing a Pod
document.  This class is of interest to people writing Pod formatters
based on Pod::Simple. It is useful for seeing exactly what events you
get out of some Pod that you feed in.

This is a subclass of L<Pod::Simple> and inherits all its methods.

=head1 SEE ALSO

L<Pod::Simple::DumpAsXML>

L<Pod::Simple>

=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2002 Sean M. Burke.  All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

=cut

