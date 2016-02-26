
require 5;
package Pod::Simple;
use strict;
use Carp ();
BEGIN           { *DEBUG = sub () {0} unless defined &DEBUG }
use integer;
use Pod::Escapes 1.03 ();
use Pod::Simple::LinkSection ();
use Pod::Simple::BlackBox ();
#use utf8;

use vars qw(
  $VERSION @ISA
  @Known_formatting_codes  @Known_directives
  %Known_formatting_codes  %Known_directives
  $NL
);

@ISA = ('Pod::Simple::BlackBox');
$VERSION = '3.05';

@Known_formatting_codes = qw(I B C L E F S X Z); 
%Known_formatting_codes = map(($_=>1), @Known_formatting_codes);
@Known_directives       = qw(head1 head2 head3 head4 item over back); 
%Known_directives       = map(($_=>'Plain'), @Known_directives);
$NL = $/ unless defined $NL;

#-----------------------------------------------------------------------------
# Set up some constants:

BEGIN {
  if(defined &ASCII)    { }
  elsif(chr(65) eq 'A') { *ASCII = sub () {1}  }
  else                  { *ASCII = sub () {''} }

  unless(defined &MANY_LINES) { *MANY_LINES = sub () {20} }
  DEBUG > 4 and print "MANY_LINES is ", MANY_LINES(), "\n";
  unless(MANY_LINES() >= 1) {
    die "MANY_LINES is too small (", MANY_LINES(), ")!\nAborting";
  }
  if(defined &UNICODE) { }
  elsif($] >= 5.008)   { *UNICODE = sub() {1}  }
  else                 { *UNICODE = sub() {''} }
}
if(DEBUG > 2) {
  print "# We are ", ASCII ? '' : 'not ', "in ASCII-land\n";
  print "# We are under a Unicode-safe Perl.\n";
}

# Design note:
# This is a parser for Pod.  It is not a parser for the set of Pod-like
#  languages which happens to contain Pod -- it is just for Pod, plus possibly
#  some extensions.

# @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @
#@ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @ @
#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

__PACKAGE__->_accessorize(
  'nbsp_for_S',        # Whether to map S<...>'s to \xA0 characters
  'source_filename',   # Filename of the source, for use in warnings
  'source_dead',       # Whether to consider this parser's source dead

  'output_fh',         # The filehandle we're writing to, if applicable.
                       # Used only in some derived classes.

  'hide_line_numbers', # For some dumping subclasses: whether to pointedly
                       # suppress the start_line attribute
                      
  'line_count',        # the current line number
  'pod_para_count',    # count of pod paragraphs seen so far

  'no_whining',        # whether to suppress whining
  'no_errata_section', # whether to suppress the errata section
  'complain_stderr',   # whether to complain to stderr

  'doc_has_started',   # whether we've fired the open-Document event yet

  'bare_output',       # For some subclasses: whether to prepend
                       #  header-code and postpend footer-code

  'fullstop_space_harden', # Whether to turn ".  " into ".[nbsp] ";

  'nix_X_codes',       # whether to ignore X<...> codes
  'merge_text',        # whether to avoid breaking a single piece of
                       #  text up into several events

  'preserve_whitespace', # whether to try to keep whitespace as-is

 'content_seen',      # whether we've seen any real Pod content
 'errors_seen',       # TODO: document.  whether we've seen any errors (fatal or not)

 'codes_in_verbatim', # for PseudoPod extensions

 'code_handler',      # coderef to call when a code (non-pod) line is seen
 'cut_handler',       # coderef to call when a =cut line is seen
 #Called like:
 # $code_handler->($line, $self->{'line_count'}, $self) if $code_handler;
 #  $cut_handler->($line, $self->{'line_count'}, $self) if $cut_handler;
  
);

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub any_errata_seen {  # good for using as an exit() value...
  return shift->{'errors_seen'} || 0;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
# Pull in some functions that, for some reason, I expect to see here too:
BEGIN {
  *pretty        = \&Pod::Simple::BlackBox::pretty;
  *stringify_lol = \&Pod::Simple::BlackBox::stringify_lol;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub version_report {
  my $class = ref($_[0]) || $_[0];
  if($class eq __PACKAGE__) {
    return "$class $VERSION";
  } else {
    my $v = $class->VERSION;
    return "$class $v (" . __PACKAGE__ . " $VERSION)";
  }
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

#sub curr_open { # read-only list accessor
#  return @{ $_[0]{'curr_open'} || return() };
#}
#sub _curr_open_listref { $_[0]{'curr_open'} ||= [] }


sub output_string {
  # Works by faking out output_fh.  Simplifies our code.
  #
  my $this = shift;
  return $this->{'output_string'} unless @_;  # GET.
  
  require Pod::Simple::TiedOutFH;
  my $x = (defined($_[0]) and ref($_[0])) ? $_[0] : \( $_[0] );
  $$x = '' unless defined $$x;
  DEBUG > 4 and print "# Output string set to $x ($$x)\n";
  $this->{'output_fh'} = Pod::Simple::TiedOutFH->handle_on($_[0]);
  return
    $this->{'output_string'} = $_[0];
    #${ ${ $this->{'output_fh'} } };
}

sub abandon_output_string { $_[0]->abandon_output_fh; delete $_[0]{'output_string'} }
sub abandon_output_fh     { $_[0]->output_fh(undef) }
# These don't delete the string or close the FH -- they just delete our
#  references to it/them.
# TODO: document these

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub new {
  # takes no parameters
  my $class = ref($_[0]) || $_[0];
  #Carp::croak(__PACKAGE__ . " is a virtual base class -- see perldoc "
  #  . __PACKAGE__ );
  return bless {
    'accept_codes'      => { map( ($_=>$_), @Known_formatting_codes ) },
    'accept_directives' => { %Known_directives },
    'accept_targets'    => {},
  }, $class;
}



# TODO: an option for whether to interpolate E<...>'s, or just resolve to codes.

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub _handle_element_start {     # OVERRIDE IN DERIVED CLASS
  my($self, $element_name, $attr_hash_r) = @_;
  return;
}

sub _handle_element_end {       # OVERRIDE IN DERIVED CLASS
  my($self, $element_name) = @_;
  return;
}

sub _handle_text          {     # OVERRIDE IN DERIVED CLASS
  my($self, $text) = @_;
  return;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#
# And now directives (not targets)

sub accept_directive_as_verbatim  { shift->_accept_directives('Verbatim', @_) }
sub accept_directive_as_data      { shift->_accept_directives('Data',     @_) }
sub accept_directive_as_processed { shift->_accept_directives('Plain',    @_) }

sub _accept_directives {
  my($this, $type) = splice @_,0,2;
  foreach my $d (@_) {
    next unless defined $d and length $d;
    Carp::croak "\"$d\" isn't a valid directive name"
     unless $d =~ m/^[a-zA-Z][a-zA-Z0-9]*$/s;
    Carp::croak "\"$d\" is already a reserved Pod directive name"
     if exists $Known_directives{$d};
    $this->{'accept_directives'}{$d} = $type;
    DEBUG > 2 and print "Learning to accept \"=$d\" as directive of type $type\n";
  }
  DEBUG > 6 and print "$this\'s accept_directives : ",
   pretty($this->{'accept_directives'}), "\n";
  
  return sort keys %{ $this->{'accept_directives'} } if wantarray;
  return;
}

#--------------------------------------------------------------------------
# TODO: document these:

sub unaccept_directive { shift->unaccept_directives(@_) };

sub unaccept_directives {
  my $this = shift;
  foreach my $d (@_) {
    next unless defined $d and length $d;
    Carp::croak "\"$d\" isn't a valid directive name"
     unless $d =~ m/^[a-zA-Z][a-zA-Z0-9]*$/s;
    Carp::croak "But you must accept \"$d\" directives -- it's a builtin!"
     if exists $Known_directives{$d};
    delete $this->{'accept_directives'}{$d};
    DEBUG > 2 and print "OK, won't accept \"=$d\" as directive.\n";
  }
  return sort keys %{ $this->{'accept_directives'} } if wantarray;
  return
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#
# And now targets (not directives)

sub accept_target         { shift->accept_targets(@_)         } # alias
sub accept_target_as_text { shift->accept_targets_as_text(@_) } # alias


sub accept_targets         { shift->_accept_targets('1', @_) }

sub accept_targets_as_text { shift->_accept_targets('force_resolve', @_) }
 # forces them to be processed, even when there's no ":".

sub _accept_targets {
  my($this, $type) = splice @_,0,2;
  foreach my $t (@_) {
    next unless defined $t and length $t;
    # TODO: enforce some limitations on what a target name can be?
    $this->{'accept_targets'}{$t} = $type;
    DEBUG > 2 and print "Learning to accept \"$t\" as target of type $type\n";
  }    
  return sort keys %{ $this->{'accept_targets'} } if wantarray;
  return;
}

#--------------------------------------------------------------------------
sub unaccept_target         { shift->unaccept_targets(@_) }

sub unaccept_targets {
  my $this = shift;
  foreach my $t (@_) {
    next unless defined $t and length $t;
    # TODO: enforce some limitations on what a target name can be?
    delete $this->{'accept_targets'}{$t};
    DEBUG > 2 and print "OK, won't accept \"$t\" as target.\n";
  }    
  return sort keys %{ $this->{'accept_targets'} } if wantarray;
  return;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#
# And now codes (not targets or directives)

sub accept_code { shift->accept_codes(@_) } # alias

sub accept_codes {  # Add some codes
  my $this = shift;
  
  foreach my $new_code (@_) {
    next unless defined $new_code and length $new_code;
    if(ASCII) {
      # A good-enough check that it's good as an XML Name symbol:
      Carp::croak "\"$new_code\" isn't a valid element name"
        if $new_code =~
          m/[\x00-\x2C\x2F\x39\x3B-\x40\x5B-\x5E\x60\x7B-\x7F]/
            # Characters under 0x80 that aren't legal in an XML Name.
        or $new_code =~ m/^[-\.0-9]/s
        or $new_code =~ m/:[-\.0-9]/s;
            # The legal under-0x80 Name characters that 
            #  an XML Name still can't start with.
    }
    
    $this->{'accept_codes'}{$new_code} = $new_code;
    
    # Yes, map to itself -- just so that when we
    #  see "=extend W [whatever] thatelementname", we say that W maps
    #  to whatever $this->{accept_codes}{thatelementname} is,
    #  i.e., "thatelementname".  Then when we go re-mapping,
    #  a "W" in the treelet turns into "thatelementname".  We only
    #  remap once.
    # If we say we accept "W", then a "W" in the treelet simply turns
    #  into "W".
  }
  
  return;
}

#--------------------------------------------------------------------------
sub unaccept_code { shift->unaccept_codes(@_) }

sub unaccept_codes { # remove some codes
  my $this = shift;
  
  foreach my $new_code (@_) {
    next unless defined $new_code and length $new_code;
    if(ASCII) {
      # A good-enough check that it's good as an XML Name symbol:
      Carp::croak "\"$new_code\" isn't a valid element name"
        if $new_code =~
          m/[\x00-\x2C\x2F\x39\x3B-\x40\x5B-\x5E\x60\x7B-\x7F]/
            # Characters under 0x80 that aren't legal in an XML Name.
        or $new_code =~ m/^[-\.0-9]/s
        or $new_code =~ m/:[-\.0-9]/s;
            # The legal under-0x80 Name characters that 
            #  an XML Name still can't start with.
    }
    
    Carp::croak "But you must accept \"$new_code\" codes -- it's a builtin!"
     if grep $new_code eq $_, @Known_formatting_codes;

    delete $this->{'accept_codes'}{$new_code};

    DEBUG > 2 and print "OK, won't accept the code $new_code<...>.\n";
  }
  
  return;
}


#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub parse_string_document {
  my $self = shift;
  my @lines;
  foreach my $line_group (@_) {
    next unless defined $line_group and length $line_group;
    pos($line_group) = 0;
    while($line_group =~
      m/([^\n\r]*)((?:\r?\n)?)/g
    ) {
      #print(">> $1\n"),
      $self->parse_lines($1)
       if length($1) or length($2)
        or pos($line_group) != length($line_group);
       # I.e., unless it's a zero-length "empty line" at the very
       #  end of "foo\nbar\n" (i.e., between the \n and the EOS).
    }
  }
  $self->parse_lines(undef); # to signal EOF
  return $self;
}

sub _init_fh_source {
  my($self, $source) = @_;

  #DEBUG > 1 and print "Declaring $source as :raw for starters\n";
  #$self->_apply_binmode($source, ':raw');
  #binmode($source, ":raw");

  return;
}

#:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.
#

sub parse_file {
  my($self, $source) = (@_);

  if(!defined $source) {
    Carp::croak("Can't use empty-string as a source for parse_file");
  } elsif(ref(\$source) eq 'GLOB') {
    $self->{'source_filename'} = '' . ($source);
  } elsif(ref $source) {
    $self->{'source_filename'} = '' . ($source);
  } elsif(!length $source) {
    Carp::croak("Can't use empty-string as a source for parse_file");
  } else {
    {
      local *PODSOURCE;
      open(PODSOURCE, "<$source") || Carp::croak("Can't open $source: $!");
      $self->{'source_filename'} = $source;
      $source = *PODSOURCE{IO};
    }
    $self->_init_fh_source($source);
  }
  # By here, $source is a FH.

  $self->{'source_fh'} = $source;
  
  my($i, @lines);
  until( $self->{'source_dead'} ) {
    splice @lines;
    for($i = MANY_LINES; $i--;) {  # read those many lines at a time
      local $/ = $NL;
      push @lines, scalar(<$source>);  # readline
      last unless defined $lines[-1];
       # but pass thru the undef, which will set source_dead to true
    }
    $self->parse_lines(@lines);
  }
  delete($self->{'source_fh'}); # so it can be GC'd
  return $self;
}

#:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.

sub parse_from_file {
  # An emulation of Pod::Parser's interface, for the sake of Perldoc.
  # Basically just a wrapper around parse_file.

  my($self, $source, $to) = @_;
  $self = $self->new unless ref($self); # so we tolerate being a class method
  
  if(!defined $source)             { $source = *STDIN{IO}
  } elsif(ref(\$source) eq 'GLOB') { # stet
  } elsif(ref($source)           ) { # stet
  } elsif(!length $source
     or $source eq '-' or $source =~ m/^<&(STDIN|0)$/i
  ) { 
    $source = *STDIN{IO};
  }

  if(!defined $to) {             $self->output_fh( *STDOUT{IO}   );
  } elsif(ref(\$to) eq 'GLOB') { $self->output_fh( $to );
  } elsif(ref($to)) {            $self->output_fh( $to );
  } elsif(!length $to
     or $to eq '-' or $to =~ m/^>&?(?:STDOUT|1)$/i
  ) {
    $self->output_fh( *STDOUT{IO} );
  } else {
    require Symbol;
    my $out_fh = Symbol::gensym();
    DEBUG and print "Write-opening to $to\n";
    open($out_fh, ">$to")  or  Carp::croak "Can't write-open $to: $!";
    binmode($out_fh)
     if $self->can('write_with_binmode') and $self->write_with_binmode;
    $self->output_fh($out_fh);
  }

  return $self->parse_file($source);
}

#-----------------------------------------------------------------------------

sub whine {
  #my($self,$line,$complaint) = @_;
  my $self = shift(@_);
  ++$self->{'errors_seen'};
  if($self->{'no_whining'}) {
    DEBUG > 9 and print "Discarding complaint (at line $_[0]) $_[1]\n because no_whining is on.\n";
    return;
  }
  return $self->_complain_warn(@_) if $self->{'complain_stderr'};
  return $self->_complain_errata(@_);
}

sub scream {    # like whine, but not suppressable
  #my($self,$line,$complaint) = @_;
  my $self = shift(@_);
  ++$self->{'errors_seen'};
  return $self->_complain_warn(@_) if $self->{'complain_stderr'};
  return $self->_complain_errata(@_);
}

sub _complain_warn {
  my($self,$line,$complaint) = @_;
  return printf STDERR "%s around line %s: %s\n",
    $self->{'source_filename'} || 'Pod input', $line, $complaint;
}

sub _complain_errata {
  my($self,$line,$complaint) = @_;
  if( $self->{'no_errata_section'} ) {
    DEBUG > 9 and print "Discarding erratum (at line $line) $complaint\n because no_errata_section is on.\n";
  } else {
    DEBUG > 9 and print "Queuing erratum (at line $line) $complaint\n";
    push @{$self->{'errata'}{$line}}, $complaint
      # for a report to be generated later!
  }
  return 1;
}

#@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

sub _get_initial_item_type {
  # A hack-wrapper here for when you have like "=over\n\n=item 456\n\n"
  my($self, $para) = @_;
  return $para->[1]{'~type'}  if $para->[1]{'~type'};

  return $para->[1]{'~type'} = 'text'
   if join("\n", @{$para}[2 .. $#$para]) =~ m/^\s*(\d+)\.?\s*$/s and $1 ne '1';
  # Else fall thru to the general case:
  return $self->_get_item_type($para);
}



sub _get_item_type {       # mutates the item!!
  my($self, $para) = @_;
  return $para->[1]{'~type'} if $para->[1]{'~type'};


  # Otherwise we haven't yet been to this node.  Maybe alter it...
  
  my $content = join "\n", @{$para}[2 .. $#$para];

  if($content =~ m/^\s*\*\s*$/s or $content =~ m/^\s*$/s) {
    # Like: "=item *", "=item   *   ", "=item"
    splice @$para, 2; # so it ends up just being ['=item', { attrhash } ]
    $para->[1]{'~orig_content'} = $content;
    return $para->[1]{'~type'} = 'bullet';

  } elsif($content =~ m/^\s*\*\s+(.+)/s) {  # tolerance
  
    # Like: "=item * Foo bar baz";
    $para->[1]{'~orig_content'}      = $content;
    $para->[1]{'~_freaky_para_hack'} = $1;
    DEBUG > 2 and print " Tolerating $$para[2] as =item *\\n\\n$1\n";
    splice @$para, 2; # so it ends up just being ['=item', { attrhash } ]
    return $para->[1]{'~type'} = 'bullet';

  } elsif($content =~ m/^\s*(\d+)\.?\s*$/s) {
    # Like: "=item 1.", "=item    123412"
    
    $para->[1]{'~orig_content'} = $content;
    $para->[1]{'number'} = $1;  # Yes, stores the number there!

    splice @$para, 2; # so it ends up just being ['=item', { attrhash } ]
    return $para->[1]{'~type'} = 'number';
    
  } else {
    # It's anything else.
    return $para->[1]{'~type'} = 'text';

  }
}

#-----------------------------------------------------------------------------

sub _make_treelet {
  my $self = shift;  # and ($para, $start_line)
  my $treelet;
  if(!@_) {
    return [''];
  } if(ref $_[0] and ref $_[0][0] and $_[0][0][0] eq '~Top') {
    # Hack so we can pass in fake-o pre-cooked paragraphs:
    #  just have the first line be a reference to a ['~Top', {}, ...]
    # We use this feechure in gen_errata and stuff.

    DEBUG and print "Applying precooked treelet hack to $_[0][0]\n";
    $treelet = $_[0][0];
    splice @$treelet, 0, 2;  # lop the top off
    return $treelet;
  } else {
    $treelet = $self->_treelet_from_formatting_codes(@_);
  }
  
  if( $self->_remap_sequences($treelet) ) {
    $self->_treat_Zs($treelet);  # Might as well nix these first
    $self->_treat_Ls($treelet);  # L has to precede E and S
    $self->_treat_Es($treelet);
    $self->_treat_Ss($treelet);  # S has to come after E

    $self->_wrap_up($treelet); # Nix X's and merge texties
    
  } else {
    DEBUG and print "Formatless treelet gets fast-tracked.\n";
     # Very common case!
  }
  
  splice @$treelet, 0, 2;  # lop the top off

  return $treelet;
}

#:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.

sub _wrap_up {
  my($self, @stack) = @_;
  my $nixx  = $self->{'nix_X_codes'};
  my $merge = $self->{'merge_text' };
  return unless $nixx or $merge;

  DEBUG > 2 and print "\nStarting _wrap_up traversal.\n",
   $merge ? (" Merge mode on\n") : (),
   $nixx  ? (" Nix-X mode on\n") : (),
  ;    
  

  my($i, $treelet);
  while($treelet = shift @stack) {
    DEBUG > 3 and print " Considering children of this $treelet->[0] node...\n";
    for($i = 2; $i < @$treelet; ++$i) { # iterate over children
      DEBUG > 3 and print " Considering child at $i ", pretty($treelet->[$i]), "\n";
      if($nixx and ref $treelet->[$i] and $treelet->[$i][0] eq 'X') {
        DEBUG > 3 and print "   Nixing X node at $i\n";
        splice(@$treelet, $i, 1); # just nix this node (and its descendants)
        # no need to back-update the counter just yet
        redo;

      } elsif($merge and $i != 2 and  # non-initial
         !ref $treelet->[$i] and !ref $treelet->[$i - 1]
      ) {
        DEBUG > 3 and print "   Merging ", $i-1,
         ":[$treelet->[$i-1]] and $i\:[$treelet->[$i]]\n";
        $treelet->[$i-1] .= ( splice(@$treelet, $i, 1) )[0];
        DEBUG > 4 and print "    Now: ", $i-1, ":[$treelet->[$i-1]]\n";
        --$i;
        next; 
        # since we just pulled the possibly last node out from under
        #  ourselves, we can't just redo()

      } elsif( ref $treelet->[$i] ) {
        DEBUG > 4 and print "  Enqueuing ", pretty($treelet->[$i]), " for traversal.\n";
        push @stack, $treelet->[$i];

        if($treelet->[$i][0] eq 'L') {
          my $thing;
          foreach my $attrname ('section', 'to') {        
            if(defined($thing = $treelet->[$i][1]{$attrname}) and ref $thing) {
              unshift @stack, $thing;
              DEBUG > 4 and print "  +Enqueuing ",
               pretty( $treelet->[$i][1]{$attrname} ),
               " as an attribute value to tweak.\n";
            }
          }
        }
      }
    }
  }
  DEBUG > 2 and print "End of _wrap_up traversal.\n\n";

  return;
}

#:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.

sub _remap_sequences {
  my($self,@stack) = @_;
  
  if(@stack == 1 and @{ $stack[0] } == 3 and !ref $stack[0][2]) {
    # VERY common case: abort it.
    DEBUG and print "Skipping _remap_sequences: formatless treelet.\n";
    return 0;
  }
  
  my $map = ($self->{'accept_codes'} || die "NO accept_codes in $self?!?");

  my $start_line = $stack[0][1]{'start_line'};
  DEBUG > 2 and printf
   "\nAbout to start _remap_sequences on treelet from line %s.\n",
   $start_line || '[?]'
  ;
  DEBUG > 3 and print " Map: ",
    join('; ', map "$_=" . (
        ref($map->{$_}) ? join(",", @{$map->{$_}}) : $map->{$_}
      ),
      sort keys %$map ),
    ("B~C~E~F~I~L~S~X~Z" eq join '~', sort keys %$map)
     ? "  (all normal)\n" : "\n"
  ;

  # A recursive algorithm implemented iteratively!  Whee!
  
  my($is, $was, $i, $treelet); # scratch
  while($treelet = shift @stack) {
    DEBUG > 3 and print " Considering children of this $treelet->[0] node...\n";
    for($i = 2; $i < @$treelet; ++$i) { # iterate over children
      next unless ref $treelet->[$i];  # text nodes are uninteresting
      
      DEBUG > 4 and print "  Noting child $i : $treelet->[$i][0]<...>\n";
      
      $is = $treelet->[$i][0] = $map->{ $was = $treelet->[$i][0] };
      if( DEBUG > 3 ) {
        if(!defined $is) {
          print "   Code $was<> is UNKNOWN!\n";
        } elsif($is eq $was) {
          DEBUG > 4 and print "   Code $was<> stays the same.\n";
        } else  {
          print "   Code $was<> maps to ",
           ref($is)
            ? ( "tags ", map("$_<", @$is), '...', map('>', @$is), "\n" )
            : "tag $is<...>.\n";
        }
      }
      
      if(!defined $is) {
        $self->whine($start_line, "Deleting unknown formatting code $was<>");
        $is = $treelet->[$i][0] = '1';  # But saving the children!
        # I could also insert a leading "$was<" and tailing ">" as
        # children of this node, but something about that seems icky.
      }
      if(ref $is) {
        my @dynasty = @$is;
        DEBUG > 4 and print "    Renaming $was node to $dynasty[-1]\n"; 
        $treelet->[$i][0] = pop @dynasty;
        my $nugget;
        while(@dynasty) {
          DEBUG > 4 and printf
           "    Grafting a new %s node between %s and %s\n",
           $dynasty[-1], $treelet->[0], $treelet->[$i][0], 
          ;
          
          #$nugget = ;
          splice @$treelet, $i, 1, [pop(@dynasty), {}, $treelet->[$i]];
            # relace node with a new parent
        }
      } elsif($is eq '0') {
        splice(@$treelet, $i, 1); # just nix this node (and its descendants)
        --$i;  # back-update the counter
      } elsif($is eq '1') {
        splice(@$treelet, $i, 1 # replace this node with its children!
          => splice @{ $treelet->[$i] },2
              # (not catching its first two (non-child) items)
        );
        --$i;  # back up for new stuff
      } else {
        # otherwise it's unremarkable
        unshift @stack, $treelet->[$i];  # just recurse
      }
    }
  }
  
  DEBUG > 2 and print "End of _remap_sequences traversal.\n\n";

  if(@_ == 2 and @{ $_[1] } == 3 and !ref $_[1][2]) {
    DEBUG and print "Noting that the treelet is now formatless.\n";
    return 0;
  }
  return 1;
}

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

sub _ponder_extend {

  # "Go to an extreme, move back to a more comfortable place"
  #  -- /Oblique Strategies/,  Brian Eno and Peter Schmidt
  
  my($self, $para) = @_;
  my $content = join ' ', splice @$para, 2;
  $content =~ s/^\s+//s;
  $content =~ s/\s+$//s;

  DEBUG > 2 and print "Ogling extensor: =extend $content\n";

  if($content =~
    m/^
      (\S+)         # 1 : new item
      \s+
      (\S+)         # 2 : fallback(s)
      (?:\s+(\S+))? # 3 : element name(s)
      \s*
      $
    /xs
  ) {
    my $new_letter = $1;
    my $fallbacks_one = $2;
    my $elements_one;
    $elements_one = defined($3) ? $3 : $1;

    DEBUG > 2 and print "Extensor has good syntax.\n";

    unless($new_letter =~ m/^[A-Z]$/s or $new_letter) {
      DEBUG > 2 and print " $new_letter isn't a valid thing to entend.\n";
      $self->whine(
        $para->[1]{'start_line'},
        "You can extend only formatting codes A-Z, not like \"$new_letter\""
      );
      return;
    }
    
    if(grep $new_letter eq $_, @Known_formatting_codes) {
      DEBUG > 2 and print " $new_letter isn't a good thing to extend, because known.\n";
      $self->whine(
        $para->[1]{'start_line'},
        "You can't extend an established code like \"$new_letter\""
      );
      
      #TODO: or allow if last bit is same?
      
      return;
    }

    unless($fallbacks_one =~ m/^[A-Z](,[A-Z])*$/s  # like "B", "M,I", etc.
      or $fallbacks_one eq '0' or $fallbacks_one eq '1'
    ) {
      $self->whine(
        $para->[1]{'start_line'},
        "Format for second =extend parameter must be like"
        . " M or 1 or 0 or M,N or M,N,O but you have it like "
        . $fallbacks_one
      );
      return;
    }
    
    unless($elements_one =~ m/^[^ ,]+(,[^ ,]+)*$/s) { # like "B", "M,I", etc.
      $self->whine(
        $para->[1]{'start_line'},
        "Format for third =extend parameter: like foo or bar,Baz,qu:ux but not like "
        . $elements_one
      );
      return;
    }

    my @fallbacks  = split ',', $fallbacks_one,  -1;
    my @elements   = split ',', $elements_one, -1;

    foreach my $f (@fallbacks) {
      next if exists $Known_formatting_codes{$f} or $f eq '0' or $f eq '1';
      DEBUG > 2 and print "  Can't fall back on unknown code $f\n";
      $self->whine(
        $para->[1]{'start_line'},
        "Can't use unknown formatting code '$f' as a fallback for '$new_letter'"
      );
      return;
    }

    DEBUG > 3 and printf "Extensor: Fallbacks <%s> Elements <%s>.\n",
     @fallbacks, @elements;

    my $canonical_form;
    foreach my $e (@elements) {
      if(exists $self->{'accept_codes'}{$e}) {
        DEBUG > 1 and print " Mapping '$new_letter' to known extension '$e'\n";
        $canonical_form = $e;
        last; # first acceptable elementname wins!
      } else {
        DEBUG > 1 and print " Can't map '$new_letter' to unknown extension '$e'\n";
      }
    }


    if( defined $canonical_form ) {
      # We found a good N => elementname mapping
      $self->{'accept_codes'}{$new_letter} = $canonical_form;
      DEBUG > 2 and print
       "Extensor maps $new_letter => known element $canonical_form.\n";
    } else {
      # We have to use the fallback(s), which might be '0', or '1'.
      $self->{'accept_codes'}{$new_letter}
        = (@fallbacks == 1) ? $fallbacks[0] : \@fallbacks;
      DEBUG > 2 and print
       "Extensor maps $new_letter => fallbacks @fallbacks.\n";
    }

  } else {
    DEBUG > 2 and print "Extensor has bad syntax.\n";
    $self->whine(
      $para->[1]{'start_line'},
      "Unknown =extend syntax: $content"
    )
  }
  return;
}


#:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.:.

sub _treat_Zs {  # Nix Z<...>'s
  my($self,@stack) = @_;

  my($i, $treelet);
  my $start_line = $stack[0][1]{'start_line'};

  # A recursive algorithm implemented iteratively!  Whee!

  while($treelet = shift @stack) {
    for($i = 2; $i < @$treelet; ++$i) { # iterate over children
      next unless ref $treelet->[$i];  # text nodes are uninteresting
      unless($treelet->[$i][0] eq 'Z') {
        unshift @stack, $treelet->[$i]; # recurse
        next;
      }
        
      DEBUG > 1 and print "Nixing Z node @{$treelet->[$i]}\n";
        
      # bitch UNLESS it's empty
      unless(  @{$treelet->[$i]} == 2
           or (@{$treelet->[$i]} == 3 and $treelet->[$i][2] eq '')
      ) {
        $self->whine( $start_line, "A non-empty Z<>" );
      }      # but kill it anyway
        
      splice(@$treelet, $i, 1); # thereby just nix this node.
      --$i;
        
    }
  }
  
  return;
}

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

# Quoting perlpodspec:

# In parsing an L<...> code, Pod parsers must distinguish at least four
# attributes:

############# Not used.  Expressed via the element children plus
#############  the value of the "content-implicit" flag.
# First:
# The link-text. If there is none, this must be undef. (E.g., in "L<Perl
# Functions|perlfunc>", the link-text is "Perl Functions". In
# "L<Time::HiRes>" and even "L<|Time::HiRes>", there is no link text. Note
# that link text may contain formatting.)
# 

############# The element children
# Second:
# The possibly inferred link-text -- i.e., if there was no real link text,
# then this is the text that we'll infer in its place. (E.g., for
# "L<Getopt::Std>", the inferred link text is "Getopt::Std".)
#

############# The "to" attribute (which might be text, or a treelet)
# Third:
# The name or URL, or undef if none. (E.g., in "L<Perl
# Functions|perlfunc>", the name -- also sometimes called the page -- is
# "perlfunc". In "L</CAVEATS>", the name is undef.)
# 

############# The "section" attribute (which might be next, or a treelet)
# Fourth:
# The section (AKA "item" in older perlpods), or undef if none. E.g., in
# Getopt::Std/DESCRIPTION, "DESCRIPTION" is the section. (Note that this
# is not the same as a manpage section like the "5" in "man 5 crontab".
# "Section Foo" in the Pod sense means the part of the text that's
# introduced by the heading or item whose text is "Foo".)
# 
# Pod parsers may also note additional attributes including:
#

############# The "type" attribute.
# Fifth:
# A flag for whether item 3 (if present) is a URL (like
# "http://lists.perl.org" is), in which case there should be no section
# attribute; a Pod name (like "perldoc" and "Getopt::Std" are); or
# possibly a man page name (like "crontab(5)" is).
#

############# Not implemented, I guess.
# Sixth:
# The raw original L<...> content, before text is split on "|", "/", etc,
# and before E<...> codes are expanded.


# For L<...> codes without a "name|" part, only E<...> and Z<> codes may
# occur -- no other formatting codes. That is, authors should not use
# "L<B<Foo::Bar>>".
#
# Note, however, that formatting codes and Z<>'s can occur in any and all
# parts of an L<...> (i.e., in name, section, text, and url).

sub _treat_Ls {  # Process our dear dear friends, the L<...> sequences

  # L<name>
  # L<name/"sec"> or L<name/sec>
  # L</"sec"> or L</sec> or L<"sec">
  # L<text|name>
  # L<text|name/"sec"> or L<text|name/sec>
  # L<text|/"sec"> or L<text|/sec> or L<text|"sec">
  # L<scheme:...>

  my($self,@stack) = @_;

  my($i, $treelet);
  my $start_line = $stack[0][1]{'start_line'};

  # A recursive algorithm implemented iteratively!  Whee!

  while($treelet = shift @stack) {
    for(my $i = 2; $i < @$treelet; ++$i) {
      # iterate over children of current tree node
      next unless ref $treelet->[$i];  # text nodes are uninteresting
      unless($treelet->[$i][0] eq 'L') {
        unshift @stack, $treelet->[$i]; # recurse
        next;
      }
      
      
      # By here, $treelet->[$i] is definitely an L node
      DEBUG > 1 and print "Ogling L node $treelet->[$i]\n";
        
      # bitch if it's empty
      if(  @{$treelet->[$i]} == 2
       or (@{$treelet->[$i]} == 3 and $treelet->[$i][2] eq '')
      ) {
        $self->whine( $start_line, "An empty L<>" );
        $treelet->[$i] = 'L<>';  # just make it a text node
        next;  # and move on
      }
     
      # Catch URLs:
      # URLs can, alas, contain E<...> sequences, so we can't /assume/
      #  that this is one text node.  But it has to START with one text
      #  node...
      if(! ref $treelet->[$i][2] and
        $treelet->[$i][2] =~ m/^\w+:[^:\s]\S*$/s
      ) {
        $treelet->[$i][1]{'type'} = 'url';
        $treelet->[$i][1]{'content-implicit'} = 'yes';

        # TODO: deal with rel: URLs here?

        if( 3 == @{ $treelet->[$i] } ) {
          # But if it IS just one text node (most common case)
          DEBUG > 1 and printf qq{Catching "%s as " as ho-hum L<URL> link.\n},
            $treelet->[$i][2]
          ;
          $treelet->[$i][1]{'to'} = Pod::Simple::LinkSection->new(
            $treelet->[$i][2]
          );                   # its own treelet
        } else {
          # It's a URL but complex (like "L<foo:bazE<123>bar>").  Feh.
          #$treelet->[$i][1]{'to'} = [ @{$treelet->[$i]} ];
          #splice @{ $treelet->[$i][1]{'to'} }, 0,2;
          #DEBUG > 1 and printf qq{Catching "%s as " as complex L<URL> link.\n},
          #  join '~', @{$treelet->[$i][1]{'to'  }};
          
          $treelet->[$i][1]{'to'} = Pod::Simple::LinkSection->new(
            $treelet->[$i]  # yes, clone the whole content as a treelet
          );
          $treelet->[$i][1]{'to'}[0] = ''; # set the copy's tagname to nil
          die "SANITY FAILURE" if $treelet->[0] eq ''; # should never happen!
          DEBUG > 1 and print
           qq{Catching "$treelet->[$i][1]{'to'}" as a complex L<URL> link.\n};
        }

        next; # and move on
      }
      
      
      # Catch some very simple and/or common cases
      if(@{$treelet->[$i]} == 3 and ! ref $treelet->[$i][2]) {
        my $it = $treelet->[$i][2];
        if($it =~ m/^[-a-zA-Z0-9]+\([-a-zA-Z0-9]+\)$/s) { # man sections
          # Hopefully neither too broad nor too restrictive a RE
          DEBUG > 1 and print "Catching \"$it\" as manpage link.\n";
          $treelet->[$i][1]{'type'} = 'man';
          # This's the only place where man links can get made.
          $treelet->[$i][1]{'content-implicit'} = 'yes';
          $treelet->[$i][1]{'to'  } =
            Pod::Simple::LinkSection->new( $it ); # treelet!

          next;
        }
        if($it =~ m/^[^\/\|,\$\%\@\ \"\<\>\:\#\&\*\{\}\[\]\(\)]+(\:\:[^\/\|,\$\%\@\ \"\<\>\:\#\&\*\{\}\[\]\(\)]+)*$/s) {
          # Extremely forgiving idea of what constitutes a bare
          #  modulename link like L<Foo::Bar> or even L<Thing::1.0::Docs::Tralala>
          DEBUG > 1 and print "Catching \"$it\" as ho-hum L<Modulename> link.\n";
          $treelet->[$i][1]{'type'} = 'pod';
          $treelet->[$i][1]{'content-implicit'} = 'yes';
          $treelet->[$i][1]{'to'  } =
            Pod::Simple::LinkSection->new( $it ); # treelet!
          next;
        }
        # else fall thru...
      }
      
      

      # ...Uhoh, here's the real L<...> parsing stuff...
      # "With the ill behavior, with the ill behavior, with the ill behavior..."

      DEBUG > 1 and print "Running a real parse on this non-trivial L\n";
      
      
      my $link_text; # set to an arrayref if found
      my $ell = $treelet->[$i];
      my @ell_content = @$ell;
      splice @ell_content,0,2; # Knock off the 'L' and {} bits

      DEBUG > 3 and print " Ell content to start: ",
       pretty(@ell_content), "\n";


      # Look for the "|" -- only in CHILDREN (not all underlings!)
      # Like L<I like the strictness|strict>
      DEBUG > 3 and
         print "  Peering at L content for a '|' ...\n";
      for(my $j = 0; $j < @ell_content; ++$j) {
        next if ref $ell_content[$j];
        DEBUG > 3 and
         print "    Peering at L-content text bit \"$ell_content[$j]\" for a '|'.\n";

        if($ell_content[$j] =~ m/^([^\|]*)\|(.*)$/s) {
          my @link_text = ($1);   # might be 0-length
          $ell_content[$j] = $2;  # might be 0-length

          DEBUG > 3 and
           print "     FOUND a '|' in it.  Splitting into [$1] + [$2]\n";

          unshift @link_text, splice @ell_content, 0, $j;
            # leaving only things at J and after
          @ell_content =  grep ref($_)||length($_), @ell_content ;
          $link_text   = [grep ref($_)||length($_), @link_text  ];
          DEBUG > 3 and printf
           "  So link text is %s\n  and remaining ell content is %s\n",
            pretty($link_text), pretty(@ell_content);
          last;
        }
      }
      
      
      # Now look for the "/" -- only in CHILDREN (not all underlings!)
      # And afterward, anything left in @ell_content will be the raw name
      # Like L<Foo::Bar/Object Methods>
      my $section_name;  # set to arrayref if found
      DEBUG > 3 and print "  Peering at L-content for a '/' ...\n";
      for(my $j = 0; $j < @ell_content; ++$j) {
        next if ref $ell_content[$j];
        DEBUG > 3 and
         print "    Peering at L-content text bit \"$ell_content[$j]\" for a '/'.\n";

        if($ell_content[$j] =~ m/^([^\/]*)\/(.*)$/s) {
          my @section_name = ($2); # might be 0-length
          $ell_content[$j] =  $1;  # might be 0-length

          DEBUG > 3 and
           print "     FOUND a '/' in it.",
             "  Splitting to page [...$1] + section [$2...]\n";

          push @section_name, splice @ell_content, 1+$j;
            # leaving only things before and including J
          
          @ell_content  = grep ref($_)||length($_), @ell_content  ;
          @section_name = grep ref($_)||length($_), @section_name ;

          # Turn L<.../"foo"> into L<.../foo>
          if(@section_name
            and !ref($section_name[0]) and !ref($section_name[-1])
            and $section_name[ 0] =~ m/^\"/s
            and $section_name[-1] =~ m/\"$/s
            and !( # catch weird degenerate case of L<"> !
              @section_name == 1 and $section_name[0] eq '"'
            )
          ) {
            $section_name[ 0] =~ s/^\"//s;
            $section_name[-1] =~ s/\"$//s;
            DEBUG > 3 and
             print "     Quotes removed: ", pretty(@section_name), "\n";
          } else {
            DEBUG > 3 and
             print "     No need to remove quotes in ", pretty(@section_name), "\n";
          }

          $section_name = \@section_name;
          last;
        }
      }

      # Turn L<"Foo Bar"> into L</Foo Bar>
      if(!$section_name and @ell_content
         and !ref($ell_content[0]) and !ref($ell_content[-1])
         and $ell_content[ 0] =~ m/^\"/s
         and $ell_content[-1] =~ m/\"$/s
         and !( # catch weird degenerate case of L<"> !
           @ell_content == 1 and $ell_content[0] eq '"'
         )
      ) {
        $section_name = [splice @ell_content];
        $section_name->[ 0] =~ s/^\"//s;
        $section_name->[-1] =~ s/\"$//s;
      }

      # Turn L<Foo Bar> into L</Foo Bar>.
      if(!$section_name and !$link_text and @ell_content
         and grep !ref($_) && m/ /s, @ell_content
      ) {
        $section_name = [splice @ell_content];
        # That's support for the now-deprecated syntax.
        # (Maybe generate a warning eventually?)
        # Note that it deliberately won't work on L<...|Foo Bar>
      }


      # Now make up the link_text
      # L<Foo>     -> L<Foo|Foo>
      # L</Bar>    -> L<"Bar"|Bar>
      # L<Foo/Bar> -> L<"Bar" in Foo/Foo>
      unless($link_text) {
        $ell->[1]{'content-implicit'} = 'yes';
        $link_text = [];
        push @$link_text, '"', @$section_name, '"' if $section_name;

        if(@ell_content) {
          $link_text->[-1] .= ' in ' if $section_name;
          push @$link_text, @ell_content;
        }
      }


      # And the E resolver will have to deal with all our treeletty things:

      if(@ell_content == 1 and !ref($ell_content[0])
         and $ell_content[0] =~ m/^[-a-zA-Z0-9]+\([-a-zA-Z0-9]+\)$/s
      ) {
        $ell->[1]{'type'}    = 'man';
        DEBUG > 3 and print "Considering this ($ell_content[0]) a man link.\n";
      } else {
        $ell->[1]{'type'}    = 'pod';
        DEBUG > 3 and print "Considering this a pod link (not man or url).\n";
      }

      if( defined $section_name ) {
        $ell->[1]{'section'} = Pod::Simple::LinkSection->new(
          ['', {}, @$section_name]
        );
        DEBUG > 3 and print "L-section content: ", pretty($ell->[1]{'section'}), "\n";
      }

      if( @ell_content ) {
        $ell->[1]{'to'} = Pod::Simple::LinkSection->new(
          ['', {}, @ell_content]
        );
        DEBUG > 3 and print "L-to content: ", pretty($ell->[1]{'to'}), "\n";
      }
      
      # And update children to be the link-text:
      @$ell = (@$ell[0,1], defined($link_text) ? splice(@$link_text) : '');
      
      DEBUG > 2 and print "End of L-parsing for this node $treelet->[$i]\n";

      unshift @stack, $treelet->[$i]; # might as well recurse
    }
  }

  return;
}

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

sub _treat_Es {
  my($self,@stack) = @_;

  my($i, $treelet, $content, $replacer, $charnum);
  my $start_line = $stack[0][1]{'start_line'};

  # A recursive algorithm implemented iteratively!  Whee!


  # Has frightening side effects on L nodes' attributes.

  #my @ells_to_tweak;

  while($treelet = shift @stack) {
    for(my $i = 2; $i < @$treelet; ++$i) { # iterate over children
      next unless ref $treelet->[$i];  # text nodes are uninteresting
      if($treelet->[$i][0] eq 'L') {
        # SPECIAL STUFF for semi-processed L<>'s
        
        my $thing;
        foreach my $attrname ('section', 'to') {        
          if(defined($thing = $treelet->[$i][1]{$attrname}) and ref $thing) {
            unshift @stack, $thing;
            DEBUG > 2 and print "  Enqueuing ",
             pretty( $treelet->[$i][1]{$attrname} ),
             " as an attribute value to tweak.\n";
          }
        }
        
        unshift @stack, $treelet->[$i]; # recurse
        next;
      } elsif($treelet->[$i][0] ne 'E') {
        unshift @stack, $treelet->[$i]; # recurse
        next;
      }
      
      DEBUG > 1 and print "Ogling E node ", pretty($treelet->[$i]), "\n";

      # bitch if it's empty
      if(  @{$treelet->[$i]} == 2
       or (@{$treelet->[$i]} == 3 and $treelet->[$i][2] eq '')
      ) {
        $self->whine( $start_line, "An empty E<>" );
        $treelet->[$i] = 'E<>'; # splice in a literal
        next;
      }
        
      # bitch if content is weird
      unless(@{$treelet->[$i]} == 3 and !ref($content = $treelet->[$i][2])) {
        $self->whine( $start_line, "An E<...> surrounding strange content" );
        $replacer = $treelet->[$i]; # scratch
        splice(@$treelet, $i, 1,   # fake out a literal
          'E<',
          splice(@$replacer,2), # promote its content
          '>'
        );
        # Don't need to do --$i, as the 'E<' we just added isn't interesting.
        next;
      }

      DEBUG > 1 and print "Ogling E<$content>\n";

      $charnum  = Pod::Escapes::e2charnum($content);
      DEBUG > 1 and print " Considering E<$content> with char ",
        defined($charnum) ? $charnum : "undef", ".\n";

      if(!defined( $charnum )) {
        DEBUG > 1 and print "I don't know how to deal with E<$content>.\n";
        $self->whine( $start_line, "Unknown E content in E<$content>" );
        $replacer = "E<$content>"; # better than nothing
      } elsif($charnum >= 255 and !UNICODE) {
        $replacer = ASCII ? "\xA4" : "?";
        DEBUG > 1 and print "This Perl version can't handle ", 
          "E<$content> (chr $charnum), so replacing with $replacer\n";
      } else {
        $replacer = Pod::Escapes::e2char($content);
        DEBUG > 1 and print " Replacing E<$content> with $replacer\n";
      }

      splice(@$treelet, $i, 1, $replacer); # no need to back up $i, tho
    }
  }

  return;
}


# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .

sub _treat_Ss {
  my($self,$treelet) = @_;
  
  _change_S_to_nbsp($treelet,0) if $self->{'nbsp_for_S'};

  # TODO: or a change_nbsp_to_S
  #  Normalizing nbsp's to S is harder: for each text node, make S content
  #  out of anything matching m/([^ \xA0]*(?:\xA0+[^ \xA0]*)+)/


  return;
}


sub _change_S_to_nbsp { #  a recursive function
  # Sanely assumes that the top node in the excursion won't be an S node.
  my($treelet, $in_s) = @_;
  
  my $is_s = ('S' eq $treelet->[0]);
  $in_s ||= $is_s; # So in_s is on either by this being an S element,
                   #  or by an ancestor being an S element.

  for(my $i = 2; $i < @$treelet; ++$i) {
    if(ref $treelet->[$i]) {
      if( _change_S_to_nbsp( $treelet->[$i], $in_s ) ) {
        my $to_pull_up = $treelet->[$i];
        splice @$to_pull_up,0,2;   # ...leaving just its content
        splice @$treelet, $i, 1, @$to_pull_up;  # Pull up content
        $i +=  @$to_pull_up - 1;   # Make $i skip the pulled-up stuff
      }
    } else {
      $treelet->[$i] =~ s/\s/\xA0/g if ASCII and $in_s;
       # (If not in ASCIIland, we can't assume that \xA0 == nbsp.)
       
       # Note that if you apply nbsp_for_S to text, and so turn
       # "foo S<bar baz> quux" into "foo bar&#160;faz quux", you
       # end up with something that fails to say "and don't hyphenate
       # any part of 'bar baz'".  However, hyphenation is such a vexing
       # problem anyway, that most Pod renderers just don't render it
       # at all.  But if you do want to implement hyphenation, I guess
       # that you'd better have nbsp_for_S off.
    }
  }

  return $is_s;
}

#-----------------------------------------------------------------------------

sub _accessorize {  # A simple-minded method-maker
  no strict 'refs';
  foreach my $attrname (@_) {
    next if $attrname =~ m/::/; # a hack
    *{caller() . '::' . $attrname} = sub {
      use strict;
      $Carp::CarpLevel = 1,  Carp::croak(
       "Accessor usage: \$obj->$attrname() or \$obj->$attrname(\$new_value)"
      ) unless (@_ == 1 or @_ == 2) and ref $_[0];
      (@_ == 1) ?  $_[0]->{$attrname}
                : ($_[0]->{$attrname} = $_[1]);
    };
  }
  # Ya know, they say accessories make the ensemble!
  return;
}

# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
# . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
#=============================================================================

sub filter {
  my($class, $source) = @_;
  my $new = $class->new;
  $new->output_fh(*STDOUT{IO});
  
  if(ref($source || '') eq 'SCALAR') {
    $new->parse_string_document( $$source );
  } elsif(ref($source)) {  # it's a file handle
    $new->parse_file($source);
  } else {  # it's a filename
    $new->parse_file($source);
  }
  
  return $new;
}


#-----------------------------------------------------------------------------

sub _out {
  # For use in testing: Class->_out($source)
  #  returns the transformation of $source
  
  my $class = shift(@_);

  my $mutor = shift(@_) if @_ and ref($_[0] || '') eq 'CODE';

  DEBUG and print "\n\n", '#' x 76,
   "\nAbout to parse source: {{\n$_[0]\n}}\n\n";
  
  
  my $parser = $class->new;
  $parser->hide_line_numbers(1);

  my $out = '';
  $parser->output_string( \$out );
  DEBUG and print " _out to ", \$out, "\n";
  
  $mutor->($parser) if $mutor;

  $parser->parse_string_document( $_[0] );
  # use Data::Dumper; print Dumper($parser), "\n";
  return $out;
}


sub _duo {
  # For use in testing: Class->_duo($source1, $source2)
  #  returns the parse trees of $source1 and $source2.
  # Good in things like: &ok( Class->duo(... , ...) );
  
  my $class = shift(@_);
  
  Carp::croak "But $class->_duo is useful only in list context!"
   unless wantarray;

  my $mutor = shift(@_) if @_ and ref($_[0] || '') eq 'CODE';

  Carp::croak "But $class->_duo takes two parameters, not: @_"
   unless @_ == 2;

  my(@out);
  
  while( @_ ) {
    my $parser = $class->new;

    push @out, '';
    $parser->output_string( \( $out[-1] ) );

    DEBUG and print " _duo out to ", $parser->output_string(),
      " = $parser->{'output_string'}\n";

    $parser->hide_line_numbers(1);
    $mutor->($parser) if $mutor;
    $parser->parse_string_document( shift( @_ ) );
    # use Data::Dumper; print Dumper($parser), "\n";
  }

  return @out;
}



#-----------------------------------------------------------------------------
1;
__END__

TODO:
A start_formatting_code and end_formatting_code methods, which in the
base class call start_L, end_L, start_C, end_C, etc., if they are
defined.

have the POD FORMATTING ERRORS section note the localtime, and the
version of Pod::Simple.

option to delete all E<shy>s?
option to scream if under-0x20 literals are found in the input, or
under-E<32> E codes are found in the tree. And ditto \x7f-\x9f

Option to turn highbit characters into their compromised form? (applies
to E parsing too)

TODO: BOM/encoding things.

TODO: ascii-compat things in the XML classes?

