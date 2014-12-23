
require 5;
package Pod::Simple::DumpAsXML;
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
  
  print $fh   '  ' x ($_[0]{'indent'} || 0),  "<", $_[1];

  foreach my $key (sort keys %{$_[2]}) {
    unless($key =~ m/^~/s) {
      next if $key eq 'start_line' and $_[0]{'hide_line_numbers'};
      _xml_escape($value = $_[2]{$key});
      print $fh ' ', $key, '="', $value, '"';
    }
  }


  print $fh ">\n";
  $_[0]{'indent'}++;
  return;
}

sub _handle_text {
  DEBUG and print "== \"$_[1]\"\n";
  if(length $_[1]) {
    my $indent = '  ' x $_[0]{'indent'};
    my $text = $_[1];
    _xml_escape($text);
    $text =~  # A not-totally-brilliant wrapping algorithm:
      s/(
         [^\n]{55}         # Snare some characters from a line
         [^\n\ ]{0,50}     #  and finish any current word
        )
        \x20{1,10}(?!\n)   # capture some spaces not at line-end
       /$1\n$indent/gx     # => line-break here
    ;
    
    print {$_[0]{'output_fh'}} $indent, $text, "\n";
  }
  return;
}

sub _handle_element_end {
  DEBUG and print "-- $_[1]\n";
  print {$_[0]{'output_fh'}}
   '  ' x --$_[0]{'indent'}, "</", $_[1], ">\n";
  return;
}

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

sub _xml_escape {
  foreach my $x (@_) {
    # Escape things very cautiously:
    $x =~ s/([^-\n\t !\#\$\%\(\)\*\+,\.\~\/\:\;=\?\@\[\\\]\^_\`\{\|\}abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789])/'&#'.(ord($1)).';'/eg;
    # Yes, stipulate the list without a range, so that this can work right on
    #  all charsets that this module happens to run under.
    # Altho, hmm, what about that ord?  Presumably that won't work right
    #  under non-ASCII charsets.  Something should be done about that.
  }
  return;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
1;

__END__

=head1 NAME

Pod::Simple::DumpAsXML -- turn Pod into XML

=head1 SYNOPSIS

  perl -MPod::Simple::DumpAsXML -e \
   "exit Pod::Simple::DumpAsXML->filter(shift)->any_errata_seen" \
   thingy.pod

=head1 DESCRIPTION

Pod::Simple::DumpAsXML is a subclass of L<Pod::Simple> that parses Pod
and turns it into indented and wrapped XML.  This class is of
interest to people writing Pod formatters based on Pod::Simple.

Pod::Simple::DumpAsXML inherits methods from
L<Pod::Simple>.


=head1 SEE ALSO

L<Pod::Simple::XMLOutStream> is rather like this class.
Pod::Simple::XMLOutStream's output is space-padded in a way
that's better for sending to an XML processor (that is, it has
no ignoreable whitespace). But
Pod::Simple::DumpAsXML's output is much more human-readable, being
(more-or-less) one token per line, with line-wrapping.

L<Pod::Simple::DumpAsText> is rather like this class,
except that it doesn't dump with XML syntax.  Try them and see
which one you like best!

L<Pod::Simple>, L<Pod::Simple::DumpAsXML>

The older libraries L<Pod::PXML>, L<Pod::XML>, L<Pod::SAX>


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

