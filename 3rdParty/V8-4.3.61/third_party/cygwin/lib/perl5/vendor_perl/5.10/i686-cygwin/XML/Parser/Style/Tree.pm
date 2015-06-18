# $Id: Tree.pm,v 1.2 2003/07/31 07:54:51 matt Exp $

package XML::Parser::Style::Tree;
$XML::Parser::Built_In_Styles{Tree} = 1;

sub Init {
  my $expat = shift;
  $expat->{Lists} = [];
  $expat->{Curlist} = $expat->{Tree} = [];
}

sub Start {
  my $expat = shift;
  my $tag = shift;
  my $newlist = [ { @_ } ];
  push @{ $expat->{Lists} }, $expat->{Curlist};
  push @{ $expat->{Curlist} }, $tag => $newlist;
  $expat->{Curlist} = $newlist;
}

sub End {
  my $expat = shift;
  my $tag = shift;
  $expat->{Curlist} = pop @{ $expat->{Lists} };
}

sub Char {
  my $expat = shift;
  my $text = shift;
  my $clist = $expat->{Curlist};
  my $pos = $#$clist;
  
  if ($pos > 0 and $clist->[$pos - 1] eq '0') {
    $clist->[$pos] .= $text;
  } else {
    push @$clist, 0 => $text;
  }
}

sub Final {
  my $expat = shift;
  delete $expat->{Curlist};
  delete $expat->{Lists};
  $expat->{Tree};
}

1;
__END__

=head1 NAME

XML::Parser::Style::Tree

=head1 SYNOPSIS

  use XML::Parser;
  my $p = XML::Parser->new(Style => 'Tree');
  my $tree = $p->parsefile('foo.xml');

=head1 DESCRIPTION

This module implements XML::Parser's Tree style parser.

When parsing a document, C<parse()> will return a parse tree for the
document. Each node in the tree
takes the form of a tag, content pair. Text nodes are represented with
a pseudo-tag of "0" and the string that is their content. For elements,
the content is an array reference. The first item in the array is a
(possibly empty) hash reference containing attributes. The remainder of
the array is a sequence of tag-content pairs representing the content
of the element.

So for example the result of parsing:

  <foo><head id="a">Hello <em>there</em></head><bar>Howdy<ref/></bar>do</foo>

would be:
             Tag   Content
  ==================================================================
  [foo, [{}, head, [{id => "a"}, 0, "Hello ",  em, [{}, 0, "there"]],
              bar, [         {}, 0, "Howdy",  ref, [{}]],
                0, "do"
        ]
  ]

The root document "foo", has 3 children: a "head" element, a "bar"
element and the text "do". After the empty attribute hash, these are
represented in it's contents by 3 tag-content pairs.

=cut
