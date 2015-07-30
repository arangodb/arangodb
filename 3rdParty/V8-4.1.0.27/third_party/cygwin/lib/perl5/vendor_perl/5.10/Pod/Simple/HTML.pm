
require 5;
package Pod::Simple::HTML;
use strict;
use Pod::Simple::PullParser ();
use vars qw(
  @ISA %Tagmap $Computerese $LamePad $Linearization_Limit $VERSION
  $Perldoc_URL_Prefix $Perldoc_URL_Postfix
  $Title_Prefix $Title_Postfix $HTML_EXTENSION %ToIndex
  $Doctype_decl  $Content_decl
);
@ISA = ('Pod::Simple::PullParser');
$VERSION = '3.03';

use UNIVERSAL ();
BEGIN {
  if(defined &DEBUG) { } # no-op
  elsif( defined &Pod::Simple::DEBUG ) { *DEBUG = \&Pod::Simple::DEBUG }
  else { *DEBUG = sub () {0}; }
}

$Doctype_decl ||= '';  # No.  Just No.  Don't even ask me for it.
 # qq{<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN"
 #    "http://www.w3.org/TR/html4/loose.dtd">\n};

$Content_decl ||=
 q{<meta http-equiv="Content-Type" content="text/html; charset=ISO-8859-1" >};

$HTML_EXTENSION = '.html' unless defined $HTML_EXTENSION;
$Computerese =  "" unless defined $Computerese;
$LamePad = '' unless defined $LamePad;

$Linearization_Limit = 120 unless defined $Linearization_Limit;
 # headings/items longer than that won't get an <a name="...">
$Perldoc_URL_Prefix  = 'http://search.cpan.org/perldoc?'
 unless defined $Perldoc_URL_Prefix;
$Perldoc_URL_Postfix = ''
 unless defined $Perldoc_URL_Postfix;

$Title_Prefix  = '' unless defined $Title_Prefix;
$Title_Postfix = '' unless defined $Title_Postfix;
%ToIndex = map {; $_ => 1 } qw(head1 head2 head3 head4 ); # item-text
  # 'item-text' stuff in the index doesn't quite work, and may
  # not be a good idea anyhow.


__PACKAGE__->_accessorize(
 'perldoc_url_prefix',
   # In turning L<Foo::Bar> into http://whatever/Foo%3a%3aBar, what
   #  to put before the "Foo%3a%3aBar".
   # (for singleton mode only?)
 'perldoc_url_postfix',
   # what to put after "Foo%3a%3aBar" in the URL.  Normally "".

 'batch_mode', # whether we're in batch mode
 'batch_mode_current_level',
    # When in batch mode, how deep the current module is: 1 for "LWP",
    #  2 for "LWP::Procotol", 3 for "LWP::Protocol::GHTTP", etc
    
 'title_prefix',  'title_postfix',
  # What to put before and after the title in the head.
  # Should already be &-escaped
  
 'html_header_before_title',
 'html_header_after_title',
 'html_footer',

 'index', # whether to add an index at the top of each page
    # (actually it's a table-of-contents, but we'll call it an index,
    #  out of apparently longstanding habit)

 'html_css', # URL of CSS file to point to
 'html_javascript', # URL of CSS file to point to

 'force_title',   # should already be &-escaped
 'default_title', # should already be &-escaped
);

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
my @_to_accept;

%Tagmap = (
  'Verbatim'  => "\n<pre$Computerese>",
  '/Verbatim' => "</pre>\n",
  'VerbatimFormatted'  => "\n<pre$Computerese>",
  '/VerbatimFormatted' => "</pre>\n",
  'VerbatimB'  => "<b>",
  '/VerbatimB' => "</b>",
  'VerbatimI'  => "<i>",
  '/VerbatimI' => "</i>",
  'VerbatimBI'  => "<b><i>",
  '/VerbatimBI' => "</i></b>",


  'Data'  => "\n",
  '/Data' => "\n",
  
  'head1' => "\n<h1>",  # And also stick in an <a name="...">
  'head2' => "\n<h2>",  #  ''
  'head3' => "\n<h3>",  #  ''
  'head4' => "\n<h4>",  #  ''
  '/head1' => "</a></h1>\n",
  '/head2' => "</a></h2>\n",
  '/head3' => "</a></h3>\n",
  '/head4' => "</a></h4>\n",

  'X'  => "<!--\n\tINDEX: ",
  '/X' => "\n-->",

  changes(qw(
    Para=p
    B=b I=i
    over-bullet=ul
    over-number=ol
    over-text=dl
    over-block=blockquote
    item-bullet=li
    item-number=li
    item-text=dt
  )),
  changes2(
    map {; m/^([-a-z]+)/s && push @_to_accept, $1; $_ }
    qw[
      sample=samp
      definition=dfn
      kbd=keyboard
      variable=var
      citation=cite
      abbreviation=abbr
      acronym=acronym
      subscript=sub
      superscript=sup
      big=big
      small=small
      underline=u
      strikethrough=s
    ]  # no point in providing a way to get <q>...</q>, I think
  ),
  
  '/item-bullet' => "</li>$LamePad\n",
  '/item-number' => "</li>$LamePad\n",
  '/item-text'   => "</a></dt>$LamePad\n",
  'item-body'    => "\n<dd>",
  '/item-body'   => "</dd>\n",


  'B'      =>  "<b>",                  '/B'     =>  "</b>",
  'I'      =>  "<i>",                  '/I'     =>  "</i>",
  'F'      =>  "<em$Computerese>",     '/F'     =>  "</em>",
  'C'      =>  "<code$Computerese>",   '/C'     =>  "</code>",
  'L'  =>  "<a href='YOU_SHOULD_NEVER_SEE_THIS'>", # ideally never used!
  '/L' =>  "</a>",
);

sub changes {
  return map {; m/^([-_:0-9a-zA-Z]+)=([-_:0-9a-zA-Z]+)$/s
     ? ( $1, => "\n<$2>", "/$1", => "</$2>\n" ) : die "Funky $_"
  } @_;
}
sub changes2 {
  return map {; m/^([-_:0-9a-zA-Z]+)=([-_:0-9a-zA-Z]+)$/s
     ? ( $1, => "<$2>", "/$1", => "</$2>" ) : die "Funky $_"
  } @_;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
sub go { exit Pod::Simple::HTML->parse_from_file(@ARGV) }
 # Just so we can run from the command line.  No options.
 #  For that, use perldoc!
#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub new {
  my $new = shift->SUPER::new(@_);
  #$new->nix_X_codes(1);
  $new->nbsp_for_S(1);
  $new->accept_targets( 'html', 'HTML' );
  $new->accept_codes('VerbatimFormatted');
  $new->accept_codes(@_to_accept);
  DEBUG > 2 and print "To accept: ", join(' ',@_to_accept), "\n";

  $new->perldoc_url_prefix(  $Perldoc_URL_Prefix  );
  $new->perldoc_url_postfix( $Perldoc_URL_Postfix );
  $new->title_prefix(  $Title_Prefix  );
  $new->title_postfix( $Title_Postfix );

  $new->html_header_before_title(
   qq[$Doctype_decl<html><head><title>]
  );
  $new->html_header_after_title( join "\n" =>
    "</title>",
    $Content_decl,
    "</head>\n<body class='pod'>",
    $new->version_tag_comment,
    "<!-- start doc -->\n",
  );
  $new->html_footer( qq[\n<!-- end doc -->\n\n</body></html>\n] );

  $new->{'Tagmap'} = {%Tagmap};
  return $new;
}

sub batch_mode_page_object_init {
  my($self, $batchconvobj, $module, $infile, $outfile, $depth) = @_;
  DEBUG and print "Initting $self\n  for $module\n",
    "  in $infile\n  out $outfile\n  depth $depth\n";
  $self->batch_mode(1);
  $self->batch_mode_current_level($depth);
  return $self;
}

sub run {
  my $self = $_[0];
  return $self->do_middle if $self->bare_output;
  return
   $self->do_beginning && $self->do_middle && $self->do_end;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub do_beginning {
  my $self = $_[0];

  my $title;
  
  if(defined $self->force_title) {
    $title = $self->force_title;
    DEBUG and print "Forcing title to be $title\n";
  } else {
    # Actually try looking for the title in the document:
    $title = $self->get_short_title();
    unless($self->content_seen) {
      DEBUG and print "No content seen in search for title.\n";
      return;
    }
    $self->{'Title'} = $title;

    if(defined $title and $title =~ m/\S/) {
      $title = $self->title_prefix . esc($title) . $self->title_postfix;
    } else {
      $title = $self->default_title;    
      $title = '' unless defined $title;
      DEBUG and print "Title defaults to $title\n";
    }
  }

  
  my $after = $self->html_header_after_title  || '';
  if($self->html_css) {
    my $link =
    $self->html_css =~ m/</
     ? $self->html_css # It's a big blob of markup, let's drop it in
     : sprintf(        # It's just a URL, so let's wrap it up
      qq[<link rel="stylesheet" type="text/css" title="pod_stylesheet" href="%s">\n],
      $self->html_css,
    );
    $after =~ s{(</head>)}{$link\n$1}i;  # otherwise nevermind
  }
  $self->_add_top_anchor(\$after);

  if($self->html_javascript) {
    my $link =
    $self->html_javascript =~ m/</
     ? $self->html_javascript # It's a big blob of markup, let's drop it in
     : sprintf(        # It's just a URL, so let's wrap it up
      qq[<script type="text/javascript" src="%s"></script>\n],
      $self->html_javascript,
    );
    $after =~ s{(</head>)}{$link\n$1}i;  # otherwise nevermind
  }

  print {$self->{'output_fh'}}
    $self->html_header_before_title || '',
    $title, # already escaped
    $after,
  ;

  DEBUG and print "Returning from do_beginning...\n";
  return 1;
}

sub _add_top_anchor {
  my($self, $text_r) = @_;
  unless($$text_r and $$text_r =~ m/name=['"]___top['"]/) { # a hack
    $$text_r .= "<a name='___top' class='dummyTopAnchor' ></a>\n";
  }
  return;
}

sub version_tag_comment {
  my $self = shift;
  return sprintf
   "<!--\n  generated by %s v%s,\n  using %s v%s,\n  under Perl v%s at %s GMT.\n\n %s\n\n-->\n",
   esc(
    ref($self), $self->VERSION(), $ISA[0], $ISA[0]->VERSION(),
    $], scalar(gmtime),
   ), $self->_modnote(),
  ;
}

sub _modnote {
  my $class = ref($_[0]) || $_[0];
  return join "\n   " => grep m/\S/, split "\n",

qq{
If you want to change this HTML document, you probably shouldn't do that
by changing it directly.  Instead, see about changing the calling options
to $class, and/or subclassing $class,
then reconverting this document from the Pod source.
When in doubt, email the author of $class for advice.
See 'perldoc $class' for more info.
};

}

sub do_end {
  my $self = $_[0];
  print {$self->{'output_fh'}}  $self->html_footer || '';
  return 1;
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
# Normally this would just be a call to _do_middle_main_loop -- but we
#  have to do some elaborate things to emit all the content and then
#  summarize it and output it /before/ the content that it's a summary of.

sub do_middle {
  my $self = $_[0];
  return $self->_do_middle_main_loop unless $self->index;

  if( $self->output_string ) {
    # An efficiency hack
    my $out = $self->output_string; #it's a reference to it
    my $sneakytag = "\f\f\e\e\b\bIndex Here\e\e\b\b\f\f\n";
    $$out .= $sneakytag;
    $self->_do_middle_main_loop;
    $sneakytag = quotemeta($sneakytag);
    my $index = $self->index_as_html();
    if( $$out =~ s/$sneakytag/$index/s ) {
      # Expected case
      DEBUG and print "Inserted ", length($index), " bytes of index HTML into $out.\n";
    } else {
      DEBUG and print "Odd, couldn't find where to insert the index in the output!\n";
      # I don't think this should ever happen.
    }
    return 1;
  }

  unless( $self->output_fh ) {
    require Carp;
    Carp::confess("Parser object \$p doesn't seem to have any output object!  I don't know how to deal with that.");
  }

  # If we get here, we're outputting to a FH.  So we need to do some magic.
  # Namely, divert all content to a string, which we output after the index.
  my $fh = $self->output_fh;
  my $content = '';
  {
    # Our horrible bait and switch:
    $self->output_string( \$content );
    $self->_do_middle_main_loop;
    $self->abandon_output_string();
    $self->output_fh($fh);
  }
  print $fh $self->index_as_html();
  print $fh $content;

  return 1;
}

###########################################################################

sub index_as_html {
  my $self = $_[0];
  # This is meant to be called AFTER the input document has been parsed!

  my $points = $self->{'PSHTML_index_points'} || [];
  
  @$points > 1 or return qq[<div class='indexgroupEmpty'></div>\n];
   # There's no point in having a 0-item or 1-item index, I dare say.
  
  my(@out) = qq{\n<div class='indexgroup'>};
  my $level = 0;

  my( $target_level, $previous_tagname, $tagname, $text, $anchorname, $indent);
  foreach my $p (@$points, ['head0', '(end)']) {
    ($tagname, $text) = @$p;
    $anchorname = $self->section_escape($text);
    if( $tagname =~ m{^head(\d+)$} ) {
      $target_level = 0 + $1;
    } else {  # must be some kinda list item
      if($previous_tagname =~ m{^head\d+$} ) {
        $target_level = $level + 1;
      } else {
        $target_level = $level;  # no change needed
      }
    }
    
    # Get to target_level by opening or closing ULs
    while($level > $target_level)
     { --$level; push @out, ("  " x $level) . "</ul>"; }
    while($level < $target_level)
     { ++$level; push @out, ("  " x ($level-1))
       . "<ul   class='indexList indexList$level'>"; }

    $previous_tagname = $tagname;
    next unless $level;
    
    $indent = '  '  x $level;
    push @out, sprintf
      "%s<li class='indexItem indexItem%s'><a href='#%s'>%s</a>",
      $indent, $level, $anchorname, esc($text)
    ;
  }
  push @out, "</div>\n";
  return join "\n", @out;
}

###########################################################################

sub _do_middle_main_loop {
  my $self = $_[0];
  my $fh = $self->{'output_fh'};
  my $tagmap = $self->{'Tagmap'};
  
  my($token, $type, $tagname, $linkto, $linktype);
  my @stack;
  my $dont_wrap = 0;

  while($token = $self->get_token) {

    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if( ($type = $token->type) eq 'start' ) {
      if(($tagname = $token->tagname) eq 'L') {
        $linktype = $token->attr('type') || 'insane';
        
        $linkto = $self->do_link($token);

        if(defined $linkto and length $linkto) {
          esc($linkto);
            #   (Yes, SGML-escaping applies on top of %-escaping!
            #   But it's rarely noticeable in practice.)
          print $fh qq{<a href="$linkto" class="podlink$linktype"\n>};
        } else {
          print $fh "<a>"; # Yes, an 'a' element with no attributes!
        }

      } elsif ($tagname eq 'item-text' or $tagname =~ m/^head\d$/s) {
        print $fh $tagmap->{$tagname} || next;

        my @to_unget;
        while(1) {
          push @to_unget, $self->get_token;
          last if $to_unget[-1]->is_end
              and $to_unget[-1]->tagname eq $tagname;
          
          # TODO: support for X<...>'s found in here?  (maybe hack into linearize_tokens)
        }

        my $name = $self->linearize_tokens(@to_unget);
        
        print $fh "<a ";
        print $fh "class='u' href='#___top' title='click to go to top of document'\n"
         if $tagname =~ m/^head\d$/s;
        
        if(defined $name) {
          my $esc = esc(  $self->section_name_tidy( $name ) );
          print $fh qq[name="$esc"];
          DEBUG and print "Linearized ", scalar(@to_unget),
           " tokens as \"$name\".\n";
          push @{ $self->{'PSHTML_index_points'} }, [$tagname, $name]
           if $ToIndex{ $tagname };
            # Obviously, this discards all formatting codes (saving
            #  just their content), but ahwell.
           
        } else {  # ludicrously long, so nevermind
          DEBUG and print "Linearized ", scalar(@to_unget),
           " tokens, but it was too long, so nevermind.\n";
        }
        print $fh "\n>";
        $self->unget_token(@to_unget);

      } elsif ($tagname eq 'Data') {
        my $next = $self->get_token;
        next unless defined $next;
        unless( $next->type eq 'text' ) {
          $self->unget_token($next);
          next;
        }
        DEBUG and print "    raw text ", $next->text, "\n";
        printf $fh "\n" . $next->text . "\n";
        next;
       
      } else {
        if( $tagname =~ m/^over-/s ) {
          push @stack, '';
        } elsif( $tagname =~ m/^item-/s and @stack and $stack[-1] ) {
          print $fh $stack[-1];
          $stack[-1] = '';
        }
        print $fh $tagmap->{$tagname} || next;
        ++$dont_wrap if $tagname eq 'Verbatim' or $tagname eq "VerbatimFormatted"
          or $tagname eq 'X';
      }

    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    } elsif( $type eq 'end' ) {
      if( ($tagname = $token->tagname) =~ m/^over-/s ) {
        if( my $end = pop @stack ) {
          print $fh $end;
        }
      } elsif( $tagname =~ m/^item-/s and @stack) {
        $stack[-1] = $tagmap->{"/$tagname"};
        if( $tagname eq 'item-text' and defined(my $next = $self->get_token) ) {
          $self->unget_token($next);
          if( $next->type eq 'start' and $next->tagname !~ m/^item-/s ) {
            print $fh $tagmap->{"/item-text"},$tagmap->{"item-body"};
            $stack[-1] = $tagmap->{"/item-body"};
          }
        }
        next;
      }
      print $fh $tagmap->{"/$tagname"} || next;
      --$dont_wrap if $tagname eq 'Verbatim' or $tagname eq 'X';

    # - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    } elsif( $type eq 'text' ) {
      esc($type = $token->text);  # reuse $type, why not
      $type =~ s/([\?\!\"\'\.\,]) /$1\n/g unless $dont_wrap;
      print $fh $type;
    }

  }
  return 1;
}

###########################################################################
#

sub do_link {
  my($self, $token) = @_;
  my $type = $token->attr('type');
  if(!defined $type) {
    $self->whine("Typeless L!?", $token->attr('start_line'));
  } elsif( $type eq 'pod') { return $self->do_pod_link($token);
  } elsif( $type eq 'url') { return $self->do_url_link($token);
  } elsif( $type eq 'man') { return $self->do_man_link($token);
  } else {
    $self->whine("L of unknown type $type!?", $token->attr('start_line'));
  }
  return 'FNORG'; # should never get called
}

# - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

sub do_url_link { return $_[1]->attr('to') }

sub do_man_link { return undef }
 # But subclasses are welcome to override this if they have man
 #  pages somewhere URL-accessible.


sub do_pod_link {
  # And now things get really messy...
  my($self, $link) = @_;
  my $to = $link->attr('to');
  my $section = $link->attr('section');
  return undef unless(  # should never happen
    (defined $to and length $to) or
    (defined $section and length $section)
  );

  $section = $self->section_escape($section)
   if defined $section and length($section .= ''); # (stringify)

  DEBUG and printf "Resolving \"%s\" \"%s\"...\n",
   $to || "(nil)",  $section || "(nil)";
   
  {
    # An early hack:
    my $complete_url = $self->resolve_pod_link_by_table($to, $section);
    if( $complete_url ) {
      DEBUG > 1 and print "resolve_pod_link_by_table(T,S) gives ",
        $complete_url, "\n  (Returning that.)\n";
      return $complete_url;
    } else {
      DEBUG > 4 and print " resolve_pod_link_by_table(T,S)", 
       " didn't return anything interesting.\n";
    }
  }

  if(defined $to and length $to) {
    # Give this routine first hack again
    my $there = $self->resolve_pod_link_by_table($to);
    if(defined $there and length $there) {
      DEBUG > 1
       and print "resolve_pod_link_by_table(T) gives $there\n";
    } else {
      $there = 
        $self->resolve_pod_page_link($to, $section);
         # (I pass it the section value, but I don't see a
         #  particular reason it'd use it.)
      DEBUG > 1 and print "resolve_pod_page_link gives ", $to || "(nil)", "\n";
      unless( defined $there and length $there ) {
        DEBUG and print "Can't resolve $to\n";
        return undef;
      }
      # resolve_pod_page_link returning undef is how it
      #  can signal that it gives up on making a link
    }
    $to = $there;
  }

  #DEBUG and print "So far [", $to||'nil', "] [", $section||'nil', "]\n";

  my $out = (defined $to and length $to) ? $to : '';
  $out .= "#" . $section if defined $section and length $section;
  
  unless(length $out) { # sanity check
    DEBUG and printf "Oddly, couldn't resolve \"%s\" \"%s\"...\n",
     $to || "(nil)",  $section || "(nil)";
    return undef;
  }

  DEBUG and print "Resolved to $out\n";
  return $out;  
}


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

sub section_escape {
  my($self, $section) = @_;
  return $self->section_url_escape(
    $self->section_name_tidy($section)
  );
}

sub section_name_tidy {
  my($self, $section) = @_;
  $section =~ tr/ /_/;
  $section =~ tr/\x00-\x1F\x80-\x9F//d if 'A' eq chr(65); # drop crazy characters
  $section = $self->unicode_escape_url($section);
  $section = '_' unless length $section;
  return $section;
}

sub section_url_escape  { shift->general_url_escape(@_) }
sub pagepath_url_escape { shift->general_url_escape(@_) }

sub general_url_escape {
  my($self, $string) = @_;
 
  $string =~ s/([^\x00-\xFF])/join '', map sprintf('%%%02X',$_), unpack 'C*', $1/eg;
     # express Unicode things as urlencode(utf(orig)).
  
  # A pretty conservative escaping, behoovey even for query components
  #  of a URL (see RFC 2396)
  
  $string =~ s/([^-_\.!~*()abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789])/sprintf('%%%02X',ord($1))/eg;
   # Yes, stipulate the list without a range, so that this can work right on
   #  all charsets that this module happens to run under.
   # Altho, hmm, what about that ord?  Presumably that won't work right
   #  under non-ASCII charsets.  Something should be done
   #  about that, I guess?
  
  return $string;
}

#--------------------------------------------------------------------------
#
# Oh look, a yawning portal to Hell!  Let's play touch football right by it!
#

sub resolve_pod_page_link {
  # resolve_pod_page_link must return a properly escaped URL
  my $self = shift;
  return $self->batch_mode()
   ? $self->resolve_pod_page_link_batch_mode(@_)
   : $self->resolve_pod_page_link_singleton_mode(@_)
  ;
}

sub resolve_pod_page_link_singleton_mode {
  my($self, $it) = @_;
  return undef unless defined $it and length $it;
  my $url = $self->pagepath_url_escape($it);
  
  $url =~ s{::$}{}s; # probably never comes up anyway
  $url =~ s{::}{/}g unless $self->perldoc_url_prefix =~ m/\?/s; # sane DWIM?
  
  return undef unless length $url;
  return $self->perldoc_url_prefix . $url . $self->perldoc_url_postfix;
}

sub resolve_pod_page_link_batch_mode {
  my($self, $to) = @_;
  DEBUG > 1 and print " During batch mode, resolving $to ...\n";
  my @path = grep length($_), split m/::/s, $to, -1;
  unless( @path ) { # sanity
    DEBUG and print "Very odd!  Splitting $to gives (nil)!\n";
    return undef;
  }
  $self->batch_mode_rectify_path(\@path);
  my $out = join('/', map $self->pagepath_url_escape($_), @path)
    . $HTML_EXTENSION;
  DEBUG > 1 and print " => $out\n";
  return $out;
}

sub batch_mode_rectify_path {
  my($self, $pathbits) = @_;
  my $level = $self->batch_mode_current_level;
  $level--; # how many levels up to go to get to the root
  if($level < 1) {
    unshift @$pathbits, '.'; # just to be pretty
  } else {
    unshift @$pathbits, ('..') x $level;
  }
  return;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub resolve_pod_link_by_table {
  # A crazy hack to allow specifying custom L<foo> => URL mappings

  return unless $_[0]->{'podhtml_LOT'};  # An optimizy shortcut

  my($self, $to, $section) = @_;

  # TODO: add a method that actually populates podhtml_LOT from a file?

  if(defined $section) {
    $to = '' unless defined $to and length $to;
    return $self->{'podhtml_LOT'}{"$to#$section"}; # quite possibly undef!
  } else {
    return $self->{'podhtml_LOT'}{$to};            # quite possibly undef!
  }
  return;
}

###########################################################################

sub linearize_tokens {  # self, tokens
  my $self = shift;
  my $out = '';
  
  my $t;
  while($t = shift @_) {
    if(!ref $t or !UNIVERSAL::can($t, 'is_text')) {
      $out .= $t; # a string, or some insane thing
    } elsif($t->is_text) {
      $out .= $t->text;
    } elsif($t->is_start and $t->tag eq 'X') {
      # Ignore until the end of this X<...> sequence:
      my $x_open = 1;
      while($x_open) {
        next if( ($t = shift @_)->is_text );
        if(   $t->is_start and $t->tag eq 'X') { ++$x_open }
        elsif($t->is_end   and $t->tag eq 'X') { --$x_open }
      }
    }
  }
  return undef if length $out > $Linearization_Limit;
  return $out;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub unicode_escape_url {
  my($self, $string) = @_;
  $string =~ s/([^\x00-\xFF])/'('.ord($1).')'/eg;
    #  Turn char 1234 into "(1234)"
  return $string;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
sub esc { # a function.
  if(defined wantarray) {
    if(wantarray) {
      @_ = splice @_; # break aliasing
    } else {
      my $x = shift;
      $x =~ s/([^-\n\t !\#\$\%\(\)\*\+,\.\~\/\:\;=\?\@\[\\\]\^_\`\{\|\}abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789])/'&#'.(ord($1)).';'/eg;
      return $x;
    }
  }
  foreach my $x (@_) {
    # Escape things very cautiously:
    $x =~ s/([^-\n\t !\#\$\%\(\)\*\+,\.\~\/\:\;=\?\@\[\\\]\^_\`\{\|\}abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789])/'&#'.(ord($1)).';'/eg
     if defined $x;
    # Leave out "- so that "--" won't make it thru in X-generated comments
    #  with text in them.

    # Yes, stipulate the list without a range, so that this can work right on
    #  all charsets that this module happens to run under.
    # Altho, hmm, what about that ord?  Presumably that won't work right
    #  under non-ASCII charsets.  Something should be done about that.
  }
  return @_;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

1;
__END__

=head1 NAME

Pod::Simple::HTML - convert Pod to HTML

=head1 SYNOPSIS

  perl -MPod::Simple::HTML -e Pod::Simple::HTML::go thingy.pod


=head1 DESCRIPTION

This class is for making an HTML rendering of a Pod document.

This is a subclass of L<Pod::Simple::PullParser> and inherits all its
methods (and options).

Note that if you want to do a batch conversion of a lot of Pod
documents to HTML, you should see the module L<Pod::Simple::HTMLBatch>.



=head1 CALLING FROM THE COMMAND LINE

TODO

  perl -MPod::Simple::HTML -e Pod::Simple::HTML::go Thing.pod Thing.html



=head1 CALLING FROM PERL

TODO   make a new object, set any options, and use parse_from_file


=head1 METHODS

TODO
all (most?) accessorized methods


=head1 SUBCLASSING

TODO

 can just set any of:  html_css html_javascript title_prefix
  'html_header_before_title',
  'html_header_after_title',
  'html_footer',

maybe override do_pod_link

maybe override do_beginning do_end



=head1 SEE ALSO

L<Pod::Simple>, L<Pod::Simple::HTMLBatch>


TODO: a corpus of sample Pod input and HTML output?  Or common
idioms?



=head1 COPYRIGHT AND DISCLAIMERS

Copyright (c) 2002-2004 Sean M. Burke.  All rights reserved.

This library is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

This program is distributed in the hope that it will be useful, but
without any warranty; without even the implied warranty of
merchantability or fitness for a particular purpose.

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>

=cut

