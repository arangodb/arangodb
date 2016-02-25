

require 5;
package Pod::Simple::SimpleTree;
use strict;
use Carp ();
use Pod::Simple ();
use vars qw( $ATTR_PAD @ISA $VERSION $SORT_ATTRS);
$VERSION = '2.02';
BEGIN {
  @ISA = ('Pod::Simple');
  *DEBUG = \&Pod::Simple::DEBUG unless defined &DEBUG;
}

__PACKAGE__->_accessorize(
  'root',   # root of the tree
);

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub _handle_element_start { # self, tagname, attrhash
  DEBUG > 2 and print "Handling $_[1] start-event\n";
  my $x = [$_[1], $_[2]];
  if($_[0]{'_currpos'}) {
    push    @{ $_[0]{'_currpos'}[0] }, $x; # insert in parent's child-list
    unshift @{ $_[0]{'_currpos'} },    $x; # prefix to stack
  } else {
    DEBUG and print " And oo, it gets to be root!\n";
    $_[0]{'_currpos'} = [   $_[0]{'root'} = $x   ];
      # first event!  set to stack, and set as root.
  }
  DEBUG > 3 and print "Stack is now: ",
    join(">", map $_->[0], @{$_[0]{'_currpos'}}), "\n";
  return;
}

sub _handle_element_end { # self, tagname
  DEBUG > 2 and print "Handling $_[1] end-event\n";
  shift @{$_[0]{'_currpos'}};
  DEBUG > 3 and print "Stack is now: ",
    join(">", map $_->[0], @{$_[0]{'_currpos'}}), "\n";
  return;
}

sub _handle_text { # self, text
  DEBUG > 2 and print "Handling $_[1] text-event\n";
  push @{ $_[0]{'_currpos'}[0] }, $_[1];
  return;
}


# A bit of evil from the black box...  please avert your eyes, kind souls.
sub _traverse_treelet_bit {
  DEBUG > 2 and print "Handling $_[1] paragraph event\n";
  my $self = shift;
  push @{ $self->{'_currpos'}[0] }, [@_];
  return;
}
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1;
__END__

=head1 NAME

Pod::Simple::SimpleTree -- parse Pod into a simple parse tree 

=head1 SYNOPSIS

  % cat ptest.pod
  
  =head1 PIE
  
  I like B<pie>!
  
  % perl -MPod::Simple::SimpleTree -MData::Dumper -e \
     "print Dumper(Pod::Simple::SimpleTree->new->parse_file(shift)->root)" \
     ptest.pod
  
  $VAR1 = [
            'Document',
            { 'start_line' => 1 },
            [
              'head1',
              { 'start_line' => 1 },
              'PIE'
            ],
            [
              'Para',
              { 'start_line' => 3 },
              'I like ',
              [
                'B',
                {},
                'pie'
              ],
              '!'
            ]
          ];

=head1 DESCRIPTION

This class is of interest to people writing a Pod processor/formatter.

This class takes Pod and parses it, returning a parse tree made just
of arrayrefs, and hashrefs, and strings.

This is a subclass of L<Pod::Simple> and inherits all its methods.

This class is inspired by XML::Parser's "Tree" parsing-style, although
it doesn't use exactly the same LoL format.

=head1 METHODS

At the end of the parse, call C<< $parser->root >> to get the
tree's top node.

=head1 Tree Contents

Every element node in the parse tree is represented by an arrayref of
the form: C<[ I<elementname>, \%attributes, I<...subnodes...> ]>.
See the example tree dump in the Synopsis, above.

Every text node in the tree is represented by a simple (non-ref)
string scalar.  So you can test C<ref($node)> to see whather you have
an element node or just a text node.

The top node in the tree is C<[ 'Document', \%attributes,
I<...subnodes...> ]>


=head1 SEE ALSO

L<Pod::Simple>

L<perllol>

L<The "Tree" subsubsection in XML::Parser|XML::Parser/"Tree">

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

