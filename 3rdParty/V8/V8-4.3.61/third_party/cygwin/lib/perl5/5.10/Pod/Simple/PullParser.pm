
require 5;
package Pod::Simple::PullParser;
$VERSION = '2.02';
use Pod::Simple ();
BEGIN {@ISA = ('Pod::Simple')}

use strict;
use Carp ();

use Pod::Simple::PullParserStartToken;
use Pod::Simple::PullParserEndToken;
use Pod::Simple::PullParserTextToken;

BEGIN { *DEBUG = \&Pod::Simple::DEBUG unless defined &DEBUG }

__PACKAGE__->_accessorize(
  'source_fh',         # the filehandle we're reading from
  'source_scalar_ref', # the scalarref we're reading from
  'source_arrayref',   # the arrayref we're reading from
);

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#
#  And here is how we implement a pull-parser on top of a push-parser...

sub filter {
  my($self, $source) = @_;
  $self = $self->new unless ref $self;

  $source = *STDIN{IO} unless defined $source;
  $self->set_source($source);
  $self->output_fh(*STDOUT{IO});

  $self->run; # define run() in a subclass if you want to use filter()!
  return $self;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub parse_string_document {
  my $this = shift;
  $this->set_source(\ $_[0]);
  $this->run;
}

sub parse_file {
  my($this, $filename) = @_;
  $this->set_source($filename);
  $this->run;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
#  In case anyone tries to use them:

sub run {
  use Carp ();
  if( __PACKAGE__ eq ref($_[0]) || $_[0]) { # I'm not being subclassed!
    Carp::croak "You can call run() only on subclasses of "
     . __PACKAGE__;
  } else {
    Carp::croak join '',
      "You can't call run() because ",
      ref($_[0]) || $_[0], " didn't define a run() method";
  }
}

sub parse_lines {
  use Carp ();
  Carp::croak "Use set_source with ", __PACKAGE__,
    " and subclasses, not parse_lines";
}

sub parse_line {
  use Carp ();
  Carp::croak "Use set_source with ", __PACKAGE__,
    " and subclasses, not parse_line";
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub new {
  my $class = shift;
  my $self = $class->SUPER::new(@_);
  die "Couldn't construct for $class" unless $self;

  $self->{'token_buffer'} ||= [];
  $self->{'start_token_class'} ||= 'Pod::Simple::PullParserStartToken';
  $self->{'text_token_class'}  ||= 'Pod::Simple::PullParserTextToken';
  $self->{'end_token_class'}   ||= 'Pod::Simple::PullParserEndToken';

  DEBUG > 1 and print "New pullparser object: $self\n";

  return $self;
}

# ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~

sub get_token {
  my $self = shift;
  DEBUG > 1 and print "\nget_token starting up on $self.\n";
  DEBUG > 2 and print " Items in token-buffer (",
   scalar( @{ $self->{'token_buffer'} } ) ,
   ") :\n", map(
     "    " . $_->dump . "\n", @{ $self->{'token_buffer'} }
   ),
   @{ $self->{'token_buffer'} } ? '' : '       (no tokens)',
   "\n"
  ;

  until( @{ $self->{'token_buffer'} } ) {
    DEBUG > 3 and print "I need to get something into my empty token buffer...\n";
    if($self->{'source_dead'}) {
      DEBUG and print "$self 's source is dead.\n";
      push @{ $self->{'token_buffer'} }, undef;
    } elsif(exists $self->{'source_fh'}) {
      my @lines;
      my $fh = $self->{'source_fh'}
       || Carp::croak('You have to call set_source before you can call get_token');
       
      DEBUG and print "$self 's source is filehandle $fh.\n";
      # Read those many lines at a time
      for(my $i = Pod::Simple::MANY_LINES; $i--;) {
        DEBUG > 3 and print " Fetching a line from source filehandle $fh...\n";
        local $/ = $Pod::Simple::NL;
        push @lines, scalar(<$fh>); # readline
        DEBUG > 3 and print "  Line is: ",
          defined($lines[-1]) ? $lines[-1] : "<undef>\n";
        unless( defined $lines[-1] ) {
          DEBUG and print "That's it for that source fh!  Killing.\n";
          delete $self->{'source_fh'}; # so it can be GC'd
          last;
        }
         # but pass thru the undef, which will set source_dead to true

        # TODO: look to see if $lines[-1] is =encoding, and if so,
        # do horribly magic things

      }
      
      if(DEBUG > 8) {
        print "* I've gotten ", scalar(@lines), " lines:\n";
        foreach my $l (@lines) {
          if(defined $l) {
            print "  line {$l}\n";
          } else {
            print "  line undef\n";
          }
        }
        print "* end of ", scalar(@lines), " lines\n";
      }

      $self->SUPER::parse_lines(@lines);
      
    } elsif(exists $self->{'source_arrayref'}) {
      DEBUG and print "$self 's source is arrayref $self->{'source_arrayref'}, with ",
       scalar(@{$self->{'source_arrayref'}}), " items left in it.\n";

      DEBUG > 3 and print "  Fetching ", Pod::Simple::MANY_LINES, " lines.\n";
      $self->SUPER::parse_lines(
        splice @{ $self->{'source_arrayref'} },
        0,
        Pod::Simple::MANY_LINES
      );
      unless( @{ $self->{'source_arrayref'} } ) {
        DEBUG and print "That's it for that source arrayref!  Killing.\n";
        $self->SUPER::parse_lines(undef);
        delete $self->{'source_arrayref'}; # so it can be GC'd
      }
       # to make sure that an undef is always sent to signal end-of-stream

    } elsif(exists $self->{'source_scalar_ref'}) {

      DEBUG and print "$self 's source is scalarref $self->{'source_scalar_ref'}, with ",
        length(${ $self->{'source_scalar_ref'} }) -
        (pos(${ $self->{'source_scalar_ref'} }) || 0),
        " characters left to parse.\n";

      DEBUG > 3 and print " Fetching a line from source-string...\n";
      if( ${ $self->{'source_scalar_ref'} } =~
        m/([^\n\r]*)((?:\r?\n)?)/g
      ) {
        #print(">> $1\n"),
        $self->SUPER::parse_lines($1)
         if length($1) or length($2)
          or pos(     ${ $self->{'source_scalar_ref'} })
           != length( ${ $self->{'source_scalar_ref'} });
         # I.e., unless it's a zero-length "empty line" at the very
         #  end of "foo\nbar\n" (i.e., between the \n and the EOS).
      } else { # that's the end.  Byebye
        $self->SUPER::parse_lines(undef);
        delete $self->{'source_scalar_ref'};
        DEBUG and print "That's it for that source scalarref!  Killing.\n";
      }

      
    } else {
      die "What source??";
    }
  }
  DEBUG and print "get_token about to return ",
   Pod::Simple::pretty( @{$self->{'token_buffer'}}
     ? $self->{'token_buffer'}[-1] : undef
   ), "\n";
  return shift @{$self->{'token_buffer'}}; # that's an undef if empty
}

use UNIVERSAL ();
sub unget_token {
  my $self = shift;
  DEBUG and print "Ungetting ", scalar(@_), " tokens: ",
   @_ ? "@_\n" : "().\n";
  foreach my $t (@_) {
    Carp::croak "Can't unget that, because it's not a token -- it's undef!"
     unless defined $t;
    Carp::croak "Can't unget $t, because it's not a token -- it's a string!"
     unless ref $t;
    Carp::croak "Can't unget $t, because it's not a token object!"
     unless UNIVERSAL::can($t, 'type');
  }
  
  unshift @{$self->{'token_buffer'}}, @_;
  DEBUG > 1 and print "Token buffer now has ",
   scalar(@{$self->{'token_buffer'}}), " items in it.\n";
  return;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

# $self->{'source_filename'} = $source;

sub set_source {
  my $self = shift @_;
  return $self->{'source_fh'} unless @_;
  my $handle;
  if(!defined $_[0]) {
    Carp::croak("Can't use empty-string as a source for set_source");
  } elsif(ref(\( $_[0] )) eq 'GLOB') {
    $self->{'source_filename'} = '' . ($handle = $_[0]);
    DEBUG and print "$self 's source is glob $_[0]\n";
    # and fall thru   
  } elsif(ref( $_[0] ) eq 'SCALAR') {
    $self->{'source_scalar_ref'} = $_[0];
    DEBUG and print "$self 's source is scalar ref $_[0]\n";
    return;
  } elsif(ref( $_[0] ) eq 'ARRAY') {
    $self->{'source_arrayref'} = $_[0];
    DEBUG and print "$self 's source is array ref $_[0]\n";
    return;
  } elsif(ref $_[0]) {
    $self->{'source_filename'} = '' . ($handle = $_[0]);
    DEBUG and print "$self 's source is fh-obj $_[0]\n";
  } elsif(!length $_[0]) {
    Carp::croak("Can't use empty-string as a source for set_source");
  } else {  # It's a filename!
    DEBUG and print "$self 's source is filename $_[0]\n";
    {
      local *PODSOURCE;
      open(PODSOURCE, "<$_[0]") || Carp::croak "Can't open $_[0]: $!";
      $handle = *PODSOURCE{IO};
    }
    $self->{'source_filename'} = $_[0];
    DEBUG and print "  Its name is $_[0].\n";

    # TODO: file-discipline things here!
  }

  $self->{'source_fh'} = $handle;
  DEBUG and print "  Its handle is $handle\n";
  return 1;
}

# ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~ ~

sub get_title_short {  shift->get_short_title(@_)  } # alias

sub get_short_title {
  my $title = shift->get_title(@_);
  $title = $1 if $title =~ m/^(\S{1,60})\s+--?\s+./s;
    # turn "Foo::Bar -- bars for your foo" into "Foo::Bar"
  return $title;
}

sub get_title       { shift->_get_titled_section(
  'NAME', max_token => 50, desperate => 1, @_)
}
sub get_version     { shift->_get_titled_section(
   'VERSION',
    max_token => 400,
    accept_verbatim => 1,
    max_content_length => 3_000,
   @_,
  );
}
sub get_description { shift->_get_titled_section(
   'DESCRIPTION',
    max_token => 400,
    max_content_length => 3_000,
   @_,
) }

sub get_authors     { shift->get_author(@_) }  # a harmless alias

sub get_author      {
  my $this = shift;
  # Max_token is so high because these are
  #  typically at the end of the document:
  $this->_get_titled_section('AUTHOR' , max_token => 10_000, @_) ||
  $this->_get_titled_section('AUTHORS', max_token => 10_000, @_);
}

#--------------------------------------------------------------------------

sub _get_titled_section {
  # Based on a get_title originally contributed by Graham Barr
  my($self, $titlename, %options) = (@_);
  
  my $max_token            = delete $options{'max_token'};
  my $desperate_for_title  = delete $options{'desperate'};
  my $accept_verbatim      = delete $options{'accept_verbatim'};
  my $max_content_length   = delete $options{'max_content_length'};
  $max_content_length = 120 unless defined $max_content_length;

  Carp::croak( "Unknown " . ((1 == keys %options) ? "option: " : "options: ")
    . join " ", map "[$_]", sort keys %options
  )
   if keys %options;

  my %content_containers;
  $content_containers{'Para'} = 1;
  if($accept_verbatim) {
    $content_containers{'Verbatim'} = 1;
    $content_containers{'VerbatimFormatted'} = 1;
  }

  my $token_count = 0;
  my $title;
  my @to_unget;
  my $state = 0;
  my $depth = 0;

  Carp::croak "What kind of titlename is \"$titlename\"?!" unless
   defined $titlename and $titlename =~ m/^[A-Z ]{1,60}$/s; #sanity
  my $titlename_re = quotemeta($titlename);

  my $head1_text_content;
  my $para_text_content;

  while(
    ++$token_count <= ($max_token || 1_000_000)
    and defined(my $token = $self->get_token)
  ) {
    push @to_unget, $token;

    if ($state == 0) { # seeking =head1
      if( $token->is_start and $token->tagname eq 'head1' ) {
        DEBUG and print "  Found head1.  Seeking content...\n";
        ++$state;
        $head1_text_content = '';
      }
    }

    elsif($state == 1) { # accumulating text until end of head1
      if( $token->is_text ) {
        DEBUG and print "   Adding \"", $token->text, "\" to head1-content.\n";
        $head1_text_content .= $token->text;
      } elsif( $token->is_end and $token->tagname eq 'head1' ) {
        DEBUG and print "  Found end of head1.  Considering content...\n";
        if($head1_text_content eq $titlename
          or $head1_text_content =~ m/\($titlename_re\)/s
          # We accept "=head1 Nomen Modularis (NAME)" for sake of i18n
        ) {
          DEBUG and print "  Yup, it was $titlename.  Seeking next para-content...\n";
          ++$state;
        } elsif(
          $desperate_for_title
           # if we're so desperate we'll take the first
           #  =head1's content as a title
          and $head1_text_content =~ m/\S/
          and $head1_text_content !~ m/^[ A-Z]+$/s
          and $head1_text_content !~
            m/\((?:
             NAME | TITLE | VERSION | AUTHORS? | DESCRIPTION | SYNOPSIS
             | COPYRIGHT | LICENSE | NOTES? | FUNCTIONS? | METHODS?
             | CAVEATS? | BUGS? | SEE\ ALSO | SWITCHES | ENVIRONMENT
            )\)/sx
            # avoid accepting things like =head1 Thingy Thongy (DESCRIPTION)
          and ($max_content_length
            ? (length($head1_text_content) <= $max_content_length) # sanity
            : 1)
        ) {
          DEBUG and print "  It looks titular: \"$head1_text_content\".\n",
            "\n  Using that.\n";
          $title = $head1_text_content;
          last;
        } else {
          --$state;
          DEBUG and print "  Didn't look titular ($head1_text_content).\n",
            "\n  Dropping back to seeking-head1-content mode...\n";
        }
      }
    }
    
    elsif($state == 2) {
      # seeking start of para (which must immediately follow)
      if($token->is_start and $content_containers{ $token->tagname }) {
        DEBUG and print "  Found start of Para.  Accumulating content...\n";
        $para_text_content = '';
        ++$state;
      } else {
        DEBUG and print
         "  Didn't see an immediately subsequent start-Para.  Reseeking H1\n";
        $state = 0;
      }
    }
    
    elsif($state == 3) {
      # accumulating text until end of Para
      if( $token->is_text ) {
        DEBUG and print "   Adding \"", $token->text, "\" to para-content.\n";
        $para_text_content .= $token->text;
        # and keep looking
        
      } elsif( $token->is_end and $content_containers{ $token->tagname } ) {
        DEBUG and print "  Found end of Para.  Considering content: ",
          $para_text_content, "\n";

        if( $para_text_content =~ m/\S/
          and ($max_content_length
           ? (length($para_text_content) <= $max_content_length)
           : 1)
        ) {
          # Some minimal sanity constraints, I think.
          DEBUG and print "  It looks contentworthy, I guess.  Using it.\n";
          $title = $para_text_content;
          last;
        } else {
          DEBUG and print "  Doesn't look at all contentworthy!\n  Giving up.\n";
          undef $title;
          last;
        }
      }
    }
    
    else {
      die "IMPOSSIBLE STATE $state!\n";  # should never happen
    }
    
  }
  
  # Put it all back!
  $self->unget_token(@to_unget);
  
  if(DEBUG) {
    if(defined $title) { print "  Returing title <$title>\n" }
    else { print "Returning title <>\n" }
  }
  
  return '' unless defined $title;
  $title =~ s/^\s+//;
  return $title;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#
#  Methods that actually do work at parse-time:

sub _handle_element_start {
  my $self = shift;   # leaving ($element_name, $attr_hash_r)
  DEBUG > 2 and print "++ $_[0] (", map("<$_> ", %{$_[1]}), ")\n";
  
  push @{ $self->{'token_buffer'} },
       $self->{'start_token_class'}->new(@_);
  return;
}

sub _handle_text {
  my $self = shift;   # leaving ($text)
  DEBUG > 2 and print "== $_[0]\n";
  push @{ $self->{'token_buffer'} },
       $self->{'text_token_class'}->new(@_);
  return;
}

sub _handle_element_end {
  my $self = shift;   # leaving ($element_name);
  DEBUG > 2 and print "-- $_[0]\n";
  push @{ $self->{'token_buffer'} }, 
       $self->{'end_token_class'}->new(@_);
  return;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

1;


__END__

=head1 NAME

Pod::Simple::PullParser -- a pull-parser interface to parsing Pod

=head1 SYNOPSIS

 my $parser = SomePodProcessor->new;
 $parser->set_source( "whatever.pod" );
 $parser->run;

Or:

 my $parser = SomePodProcessor->new;
 $parser->set_source( $some_filehandle_object );
 $parser->run;

Or:

 my $parser = SomePodProcessor->new;
 $parser->set_source( \$document_source );
 $parser->run;

Or:

 my $parser = SomePodProcessor->new;
 $parser->set_source( \@document_lines );
 $parser->run;

And elsewhere:

 require 5;
 package SomePodProcessor;
 use strict;
 use base qw(Pod::Simple::PullParser);
 
 sub run {
   my $self = shift;
  Token:
   while(my $token = $self->get_token) {
     ...process each token...
   }
 }

=head1 DESCRIPTION

This class is for using Pod::Simple to build a Pod processor -- but
one that uses an interface based on a stream of token objects,
instead of based on events.

This is a subclass of L<Pod::Simple> and inherits all its methods.

A subclass of Pod::Simple::PullParser should define a C<run> method
that calls C<< $token = $parser->get_token >> to pull tokens.

See the source for Pod::Simple::RTF for an example of a formatter
that uses Pod::Simple::PullParser.

=head1 METHODS

=over

=item my $token = $parser->get_token

This returns the next token object (which will be of a subclass of
L<Pod::Simple::PullParserToken>), or undef if the parser-stream has hit
the end of the document.

=item $parser->unget_token( $token )

=item $parser->unget_token( $token1, $token2, ... )

This restores the token object(s) to the front of the parser stream.

=back

The source has to be set before you can parse anything.  The lowest-level
way is to call C<set_source>:

=over

=item $parser->set_source( $filename )

=item $parser->set_source( $filehandle_object )

=item $parser->set_source( \$document_source )

=item $parser->set_source( \@document_lines )

=back

Or you can call these methods, which Pod::Simple::PullParser has defined
to work just like Pod::Simple's same-named methods:

=over

=item $parser->parse_file(...)

=item $parser->parse_string_document(...)

=item $parser->filter(...)

=item $parser->parse_from_file(...)

=back

For those to work, the Pod-processing subclass of
Pod::Simple::PullParser has to have defined a $parser->run method --
so it is advised that all Pod::Simple::PullParser subclasses do so.
See the Synopsis above, or the source for Pod::Simple::RTF.

Authors of formatter subclasses might find these methods useful to
call on a parser object that you haven't started pulling tokens
from yet:

=over

=item my $title_string = $parser->get_title

This tries to get the title string out of $parser, by getting some tokens,
and scanning them for the title, and then ungetting them so that you can
process the token-stream from the beginning.

For example, suppose you have a document that starts out:

  =head1 NAME
  
  Hoo::Boy::Wowza -- Stuff B<wow> yeah!

$parser->get_title on that document will return "Hoo::Boy::Wowza --
Stuff wow yeah!".

In cases where get_title can't find the title, it will return empty-string
("").

=item my $title_string = $parser->get_short_title

This is just like get_title, except that it returns just the modulename, if
the title seems to be of the form "SomeModuleName -- description".

For example, suppose you have a document that starts out:

  =head1 NAME
  
  Hoo::Boy::Wowza -- Stuff B<wow> yeah!

then $parser->get_short_title on that document will return
"Hoo::Boy::Wowza".

But if the document starts out:

  =head1 NAME
  
  Hooboy, stuff B<wow> yeah!

then $parser->get_short_title on that document will return "Hooboy,
stuff wow yeah!".

If the title can't be found, then get_short_title returns empty-string
("").

=item $author_name   = $parser->get_author

This works like get_title except that it returns the contents of the
"=head1 AUTHOR\n\nParagraph...\n" section, assuming that that section
isn't terribly long.

(This method tolerates "AUTHORS" instead of "AUTHOR" too.)

=item $description_name = $parser->get_description

This works like get_title except that it returns the contents of the
"=head1 PARAGRAPH\n\nParagraph...\n" section, assuming that that section
isn't terribly long.

=item $version_block = $parser->get_version

This works like get_title except that it returns the contents of
the "=head1 VERSION\n\n[BIG BLOCK]\n" block.  Note that this does NOT
return the module's C<$VERSION>!!


=back

=head1 NOTE

You don't actually I<have> to define a C<run> method.  If you're
writing a Pod-formatter class, you should define a C<run> just so
that users can call C<parse_file> etc, but you don't I<have> to.

And if you're not writing a formatter class, but are instead just
writing a program that does something simple with a Pod::PullParser
object (and not an object of a subclass), then there's no reason to
bother subclassing to add a C<run> method.

=head1 SEE ALSO

L<Pod::Simple>

L<Pod::Simple::PullParserToken> -- and its subclasses
L<Pod::Simple::PullParserStartToken>,
L<Pod::Simple::PullParserTextToken>, and
L<Pod::Simple::PullParserEndToken>.

L<HTML::TokeParser>, which inspired this.

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



JUNK:

sub _old_get_title {  # some witchery in here
  my $self = $_[0];
  my $title;
  my @to_unget;

  while(1) {
    push @to_unget, $self->get_token;
    unless(defined $to_unget[-1]) { # whoops, short doc!
      pop @to_unget;
      last;
    }

    DEBUG and print "-Got token ", $to_unget[-1]->dump, "\n";

    (DEBUG and print "Too much in the buffer.\n"),
     last if @to_unget > 25; # sanity
    
    my $pattern = '';
    if( #$to_unget[-1]->type eq 'end'
        #and $to_unget[-1]->tagname eq 'Para'
        #and
        ($pattern = join('',
         map {;
            ($_->type eq 'start') ? ("<" . $_->tagname .">")
          : ($_->type eq 'end'  ) ? ("</". $_->tagname .">")
          : ($_->type eq 'text' ) ? ($_->text =~ m<^([A-Z]+)$>s ? $1 : 'X')
          : "BLORP"
         } @to_unget
       )) =~ m{<head1>NAME</head1><Para>(X|</?[BCIFLS]>)+</Para>$}s
    ) {
      # Whee, it fits the pattern
      DEBUG and print "Seems to match =head1 NAME pattern.\n";
      $title = '';
      foreach my $t (reverse @to_unget) {
        last if $t->type eq 'start' and $t->tagname eq 'Para';
        $title = $t->text . $title if $t->type eq 'text';
      }
      undef $title if $title =~ m<^\s*$>; # make sure it's contentful!
      last;

    } elsif ($pattern =~ m{<head(\d)>(.+)</head\d>$}
      and !( $1 eq '1' and $2 eq 'NAME' )
    ) {
      # Well, it fits a fallback pattern
      DEBUG and print "Seems to match NAMEless pattern.\n";
      $title = '';
      foreach my $t (reverse @to_unget) {
        last if $t->type eq 'start' and $t->tagname =~ m/^head\d$/s;
        $title = $t->text . $title if $t->type eq 'text';
      }
      undef $title if $title =~ m<^\s*$>; # make sure it's contentful!
      last;
      
    } else {
      DEBUG and $pattern and print "Leading pattern: $pattern\n";
    }
  }
  
  # Put it all back:
  $self->unget_token(@to_unget);
  
  if(DEBUG) {
    if(defined $title) { print "  Returing title <$title>\n" }
    else { print "Returning title <>\n" }
  }
  
  return '' unless defined $title;
  return $title;
}

