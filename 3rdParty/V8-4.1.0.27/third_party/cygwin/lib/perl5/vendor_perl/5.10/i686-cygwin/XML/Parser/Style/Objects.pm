# $Id: Objects.pm,v 1.1 2003/08/18 20:20:51 matt Exp $

package XML::Parser::Style::Objects;
use strict;

sub Init {
  my $expat = shift;
  $expat->{Lists} = [];
  $expat->{Curlist} = $expat->{Tree} = [];
}

sub Start {
  my $expat = shift;
  my $tag = shift;
  my $newlist = [ ];
  my $class = "${$expat}{Pkg}::$tag";
  my $newobj = bless { @_, Kids => $newlist }, $class;
  push @{ $expat->{Lists} }, $expat->{Curlist};
  push @{ $expat->{Curlist} }, $newobj;
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
  my $class = "${$expat}{Pkg}::Characters";
  my $clist = $expat->{Curlist};
  my $pos = $#$clist;
  
  if ($pos >= 0 and ref($clist->[$pos]) eq $class) {
    $clist->[$pos]->{Text} .= $text;
  } else {
    push @$clist, bless { Text => $text }, $class;
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

XML::Parser::Style::Objects

=head1 SYNOPSIS

  use XML::Parser;
  my $p = XML::Parser->new(Style => 'Objects', Pkg => 'MyNode');
  my $tree = $p->parsefile('foo.xml');

=head1 DESCRIPTION

This module implements XML::Parser's Objects style parser.

This is similar to the Tree style, except that a hash object is created for
each element. The corresponding object will be in the class whose name
is created by appending "::" and the element name to the package set with
the Pkg option. Non-markup text will be in the ::Characters class. The
contents of the corresponding object will be in an anonymous array that
is the value of the Kids property for that object.

=head1 SEE ALSO

L<XML::Parser::Style::Tree>

=cut