# $Id: Subs.pm,v 1.1 2003/07/27 16:07:49 matt Exp $

package XML::Parser::Style::Subs;

sub Start {
  no strict 'refs';
  my $expat = shift;
  my $tag = shift;
  my $sub = $expat->{Pkg} . "::$tag";
  eval { &$sub($expat, $tag, @_) };
}

sub End {
  no strict 'refs';
  my $expat = shift;
  my $tag = shift;
  my $sub = $expat->{Pkg} . "::${tag}_";
  eval { &$sub($expat, $tag) };
}

1;
__END__

=head1 NAME

XML::Parser::Style::Subs

=head1 SYNOPSIS

  use XML::Parser;
  my $p = XML::Parser->new(Style => 'Subs', Pkg => 'MySubs');
  $p->parsefile('foo.xml');
  
  {
    package MySubs;
    
    sub foo {
      # start of foo tag
    }
    
    sub foo_ {
      # end of foo tag
    }
  }

=head1 DESCRIPTION

Each time an element starts, a sub by that name in the package specified
by the Pkg option is called with the same parameters that the Start
handler gets called with.

Each time an element ends, a sub with that name appended with an underscore
("_"), is called with the same parameters that the End handler gets called
with.

Nothing special is returned by parse.

=cut