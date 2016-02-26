# $Id: Stream.pm,v 1.1 2003/07/27 16:07:49 matt Exp $

package XML::Parser::Style::Stream;
use strict;

# This style invented by Tim Bray <tbray@textuality.com>

sub Init {
  no strict 'refs';
  my $expat = shift;
  $expat->{Text} = '';
  my $sub = $expat->{Pkg} ."::StartDocument";
  &$sub($expat)
    if defined(&$sub);
}

sub Start {
  no strict 'refs';
  my $expat = shift;
  my $type = shift;
  
  doText($expat);
  $_ = "<$type";
  
  %_ = @_;
  while (@_) {
    $_ .= ' ' . shift() . '="' . shift() . '"';
  }
  $_ .= '>';
  
  my $sub = $expat->{Pkg} . "::StartTag";
  if (defined(&$sub)) {
    &$sub($expat, $type);
  } else {
    print;
  }
}

sub End {
  no strict 'refs';
  my $expat = shift;
  my $type = shift;
  
  # Set right context for Text handler
  push(@{$expat->{Context}}, $type);
  doText($expat);
  pop(@{$expat->{Context}});
  
  $_ = "</$type>";
  
  my $sub = $expat->{Pkg} . "::EndTag";
  if (defined(&$sub)) {
    &$sub($expat, $type);
  } else {
    print;
  }
}

sub Char {
  my $expat = shift;
  $expat->{Text} .= shift;
}

sub Proc {
  no strict 'refs';
  my $expat = shift;
  my $target = shift;
  my $text = shift;
  
  doText($expat);

  $_ = "<?$target $text?>";
  
  my $sub = $expat->{Pkg} . "::PI";
  if (defined(&$sub)) {
    &$sub($expat, $target, $text);
  } else {
    print;
  }
}

sub Final {
  no strict 'refs';
  my $expat = shift;
  my $sub = $expat->{Pkg} . "::EndDocument";
  &$sub($expat)
    if defined(&$sub);
}

sub doText {
  no strict 'refs';
  my $expat = shift;
  $_ = $expat->{Text};
  
  if (length($_)) {
    my $sub = $expat->{Pkg} . "::Text";
    if (defined(&$sub)) {
      &$sub($expat);
    } else {
      print;
    }
    
    $expat->{Text} = '';
  }
}

1;
__END__

=head1 NAME

XML::Parser::Style::Stream - Stream style for XML::Parser

=head1 SYNOPSIS

  use XML::Parser;
  my $p = XML::Parser->new(Style => 'Stream', Pkg => 'MySubs');
  $p->parsefile('foo.xml');
  
  {
    package MySubs;
    
    sub StartTag {
      my ($e, $name) = @_;
      # do something with start tags
    }
    
    sub EndTag {
      my ($e, $name) = @_;
      # do something with end tags
    }
    
    sub Characters {
      my ($e, $data) = @_;
      # do something with text nodes
    }
  }

=head1 DESCRIPTION

This style uses the Pkg option to find subs in a given package to call for each event.
If none of the subs that this
style looks for is there, then the effect of parsing with this style is
to print a canonical copy of the document without comments or declarations.
All the subs receive as their 1st parameter the Expat instance for the
document they're parsing.

It looks for the following routines:

=over 4

=item * StartDocument

Called at the start of the parse .

=item * StartTag

Called for every start tag with a second parameter of the element type. The $_
variable will contain a copy of the tag and the %_ variable will contain
attribute values supplied for that element.

=item * EndTag

Called for every end tag with a second parameter of the element type. The $_
variable will contain a copy of the end tag.

=item * Text

Called just before start or end tags with accumulated non-markup text in
the $_ variable.

=item * PI

Called for processing instructions. The $_ variable will contain a copy of
the PI and the target and data are sent as 2nd and 3rd parameters
respectively.

=item * EndDocument

Called at conclusion of the parse.

=back

=cut