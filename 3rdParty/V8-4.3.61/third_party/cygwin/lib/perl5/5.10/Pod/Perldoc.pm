
require 5;
use 5.006;  # we use some open(X, "<", $y) syntax 
package Pod::Perldoc;
use strict;
use warnings;
use Config '%Config';

use Fcntl;    # for sysopen
use File::Spec::Functions qw(catfile catdir splitdir);

use vars qw($VERSION @Pagers $Bindir $Pod2man
  $Temp_Files_Created $Temp_File_Lifetime
);
$VERSION = '3.14_04';
#..........................................................................

BEGIN {  # Make a DEBUG constant very first thing...
  unless(defined &DEBUG) {
    if(($ENV{'PERLDOCDEBUG'} || '') =~ m/^(\d+)/) { # untaint
      eval("sub DEBUG () {$1}");
      die "WHAT? Couldn't eval-up a DEBUG constant!? $@" if $@;
    } else {
      *DEBUG = sub () {0};
    }
  }
}

use Pod::Perldoc::GetOptsOO; # uses the DEBUG.

#..........................................................................

sub TRUE  () {1}
sub FALSE () {return}

BEGIN {
 *IS_VMS     = $^O eq 'VMS'     ? \&TRUE : \&FALSE unless defined &IS_VMS;
 *IS_MSWin32 = $^O eq 'MSWin32' ? \&TRUE : \&FALSE unless defined &IS_MSWin32;
 *IS_Dos     = $^O eq 'dos'     ? \&TRUE : \&FALSE unless defined &IS_Dos;
 *IS_OS2     = $^O eq 'os2'     ? \&TRUE : \&FALSE unless defined &IS_OS2;
 *IS_Cygwin  = $^O eq 'cygwin'  ? \&TRUE : \&FALSE unless defined &IS_Cygwin;
 *IS_Linux   = $^O eq 'linux'   ? \&TRUE : \&FALSE unless defined &IS_Linux;
 *IS_HPUX    = $^O =~ m/hpux/   ? \&TRUE : \&FALSE unless defined &IS_HPUX;
}

$Temp_File_Lifetime ||= 60 * 60 * 24 * 5;
  # If it's older than five days, it's quite unlikely
  #  that anyone's still looking at it!!
  # (Currently used only by the MSWin cleanup routine)


#..........................................................................
{ my $pager = $Config{'pager'};
  push @Pagers, $pager if -x (split /\s+/, $pager)[0] or IS_VMS;
}
$Bindir  = $Config{'scriptdirexp'};
$Pod2man = "pod2man" . ( $Config{'versiononly'} ? $Config{'version'} : '' );

# End of class-init stuff
#
###########################################################################
#
# Option accessors...

foreach my $subname (map "opt_$_", split '', q{mhlvriFfXqnTdUL}) {
  no strict 'refs';
  *$subname = do{ use strict 'refs';  sub () { shift->_elem($subname, @_) } };
}

# And these are so that GetOptsOO knows they take options:
sub opt_f_with { shift->_elem('opt_f', @_) }
sub opt_q_with { shift->_elem('opt_q', @_) }
sub opt_d_with { shift->_elem('opt_d', @_) }
sub opt_L_with { shift->_elem('opt_L', @_) }

sub opt_w_with { # Specify an option for the formatter subclass
  my($self, $value) = @_;
  if($value =~ m/^([-_a-zA-Z][-_a-zA-Z0-9]*)(?:[=\:](.*?))?$/s) {
    my $option = $1;
    my $option_value = defined($2) ? $2 : "TRUE";
    $option =~ tr/\-/_/s;  # tolerate "foo-bar" for "foo_bar"
    $self->add_formatter_option( $option, $option_value );
  } else {
    warn "\"$value\" isn't a good formatter option name.  I'm ignoring it!\n";
  }
  return;
}

sub opt_M_with { # specify formatter class name(s)
  my($self, $classes) = @_;
  return unless defined $classes and length $classes;
  DEBUG > 4 and print "Considering new formatter classes -M$classes\n";
  my @classes_to_add;
  foreach my $classname (split m/[,;]+/s, $classes) {
    next unless $classname =~ m/\S/;
    if( $classname =~ m/^(\w+(::\w+)+)$/s ) {
      # A mildly restrictive concept of what modulenames are valid.
      push @classes_to_add, $1; # untaint
    } else {
      warn "\"$classname\" isn't a valid classname.  Ignoring.\n";
    }
  }
  
  unshift @{ $self->{'formatter_classes'} }, @classes_to_add;
  
  DEBUG > 3 and print(
    "Adding @classes_to_add to the list of formatter classes, "
    . "making them @{ $self->{'formatter_classes'} }.\n"
  );
  
  return;
}

sub opt_V { # report version and exit
  print join '',
    "Perldoc v$VERSION, under perl v$] for $^O",

    (defined(&Win32::BuildNumber) and defined &Win32::BuildNumber())
     ? (" (win32 build ", &Win32::BuildNumber(), ")") : (),
    
    (chr(65) eq 'A') ? () : " (non-ASCII)",
    
    "\n",
  ;
  exit;
}

sub opt_t { # choose plaintext as output format
  my $self = shift;
  $self->opt_o_with('text')  if @_ and $_[0];
  return $self->_elem('opt_t', @_);
}

sub opt_u { # choose raw pod as output format
  my $self = shift;
  $self->opt_o_with('pod')  if @_ and $_[0];
  return $self->_elem('opt_u', @_);
}

sub opt_n_with {
  # choose man as the output format, and specify the proggy to run
  my $self = shift;
  $self->opt_o_with('man')  if @_ and $_[0];
  $self->_elem('opt_n', @_);
}

sub opt_o_with { # "o" for output format
  my($self, $rest) = @_;
  return unless defined $rest and length $rest;
  if($rest =~ m/^(\w+)$/s) {
    $rest = $1; #untaint
  } else {
    warn "\"$rest\" isn't a valid output format.  Skipping.\n";
    return;
  }
  
  $self->aside("Noting \"$rest\" as desired output format...\n");
  
  # Figure out what class(es) that could actually mean...
  
  my @classes;
  foreach my $prefix ("Pod::Perldoc::To", "Pod::Simple::", "Pod::") {
    # Messy but smart:
    foreach my $stem (
      $rest,  # Yes, try it first with the given capitalization
      "\L$rest", "\L\u$rest", "\U$rest" # And then try variations

    ) {
      push @classes, $prefix . $stem;
      #print "Considering $prefix$stem\n";
    }
    
    # Tidier, but misses too much:
    #push @classes, $prefix . ucfirst(lc($rest));
  }
  $self->opt_M_with( join ";", @classes );
  return;
}

###########################################################################
# % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %

sub run {  # to be called by the "perldoc" executable
  my $class = shift;
  if(DEBUG > 3) {
    print "Parameters to $class\->run:\n";
    my @x = @_;
    while(@x) {
      $x[1] = '<undef>'  unless defined $x[1];
      $x[1] = "@{$x[1]}" if ref( $x[1] ) eq 'ARRAY';
      print "  [$x[0]] => [$x[1]]\n";
      splice @x,0,2;
    }
    print "\n";
  }
  return $class -> new(@_) -> process() || 0;
}

# % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % % %
###########################################################################

sub new {  # yeah, nothing fancy
  my $class = shift;
  my $new = bless {@_}, (ref($class) || $class);
  DEBUG > 1 and print "New $class object $new\n";
  $new->init();
  $new;
}

#..........................................................................

sub aside {  # If we're in -v or DEBUG mode, say this.
  my $self = shift;
  if( DEBUG or $self->opt_v ) {
    my $out = join( '',
      DEBUG ? do {
        my $callsub = (caller(1))[3];
        my $package = quotemeta(__PACKAGE__ . '::');
        $callsub =~ s/^$package/'/os;
         # the o is justified, as $package really won't change.
        $callsub . ": ";
      } : '',
      @_,
    );
    if(DEBUG) { print $out } else { print STDERR $out }
  }
  return;
}

#..........................................................................

sub usage {
  my $self = shift;
  warn "@_\n" if @_;
  
  # Erase evidence of previous errors (if any), so exit status is simple.
  $! = 0;
  
  die <<EOF;
perldoc [options] PageName|ModuleName|ProgramName...
perldoc [options] -f BuiltinFunction
perldoc [options] -q FAQRegex

Options:
    -h   Display this help message
    -V   report version
    -r   Recursive search (slow)
    -i   Ignore case
    -t   Display pod using pod2text instead of pod2man and nroff
             (-t is the default on win32 unless -n is specified)
    -u   Display unformatted pod text
    -m   Display module's file in its entirety
    -n   Specify replacement for nroff
    -l   Display the module's file name
    -F   Arguments are file names, not modules
    -v   Verbosely describe what's going on
    -T   Send output to STDOUT without any pager
    -d output_filename_to_send_to
    -o output_format_name
    -M FormatterModuleNameToUse
    -w formatter_option:option_value
    -L translation_code   Choose doc translation (if any)
    -X   use index if present (looks for pod.idx at $Config{archlib})
    -q   Search the text of questions (not answers) in perlfaq[1-9]

PageName|ModuleName...
         is the name of a piece of documentation that you want to look at. You
         may either give a descriptive name of the page (as in the case of
         `perlfunc') the name of a module, either like `Term::Info' or like
         `Term/Info', or the name of a program, like `perldoc'.

BuiltinFunction
         is the name of a perl function.  Will extract documentation from
         `perlfunc'.

FAQRegex
         is a regex. Will search perlfaq[1-9] for and extract any
         questions that match.

Any switches in the PERLDOC environment variable will be used before the
command line arguments.  The optional pod index file contains a list of
filenames, one per line.
                                                       [Perldoc v$VERSION]
EOF

}

#..........................................................................

sub usage_brief {
  my $me = $0;		# Editing $0 is unportable

  $me =~ s,.*[/\\],,; # get basename
  
  die <<"EOUSAGE";
Usage: $me [-h] [-V] [-r] [-i] [-v] [-t] [-u] [-m] [-n nroffer_program] [-l] [-T] [-d output_filename] [-o output_format] [-M FormatterModuleNameToUse] [-w formatter_option:option_value] [-L translation_code] [-F] [-X] PageName|ModuleName|ProgramName
       $me -f PerlFunc
       $me -q FAQKeywords

The -h option prints more help.  Also try "perldoc perldoc" to get
acquainted with the system.                        [Perldoc v$VERSION]
EOUSAGE

}

#..........................................................................

sub pagers { @{ shift->{'pagers'} } } 

#..........................................................................

sub _elem {  # handy scalar meta-accessor: shift->_elem("foo", @_)
  if(@_ > 2) { return  $_[0]{ $_[1] } = $_[2]  }
  else       { return  $_[0]{ $_[1] }          }
}
#..........................................................................
###########################################################################
#
# Init formatter switches, and start it off with __bindir and all that
# other stuff that ToMan.pm needs.
#

sub init {
  my $self = shift;

  # Make sure creat()s are neither too much nor too little
  eval { umask(0077) };   # doubtless someone has no mask

  $self->{'args'}              ||= \@ARGV;
  $self->{'found'}             ||= [];
  $self->{'temp_file_list'}    ||= [];
  
  
  $self->{'target'} = undef;

  $self->init_formatter_class_list;

  $self->{'pagers' } = [@Pagers] unless exists $self->{'pagers'};
  $self->{'bindir' } = $Bindir   unless exists $self->{'bindir'};
  $self->{'pod2man'} = $Pod2man  unless exists $self->{'pod2man'};

  push @{ $self->{'formatter_switches'} = [] }, (
   # Yeah, we could use a hashref, but maybe there's some class where options
   # have to be ordered; so we'll use an arrayref.

     [ '__bindir'  => $self->{'bindir' } ],
     [ '__pod2man' => $self->{'pod2man'} ],
  );

  DEBUG > 3 and printf "Formatter switches now: [%s]\n",
   join ' ', map "[@$_]", @{ $self->{'formatter_switches'} };

  $self->{'translators'} = [];
  $self->{'extra_search_dirs'} = [];

  return;
}

#..........................................................................

sub init_formatter_class_list {
  my $self = shift;
  $self->{'formatter_classes'} ||= [];

  # Remember, no switches have been read yet, when
  # we've started this routine.

  $self->opt_M_with('Pod::Perldoc::ToPod');   # the always-there fallthru
  $self->opt_o_with('text');
  $self->opt_o_with('man') unless IS_MSWin32 || IS_Dos
       || !($ENV{TERM} && (
              ($ENV{TERM} || '') !~ /dumb|emacs|none|unknown/i
           ));

  return;
}

#..........................................................................

sub process {
    # if this ever returns, its retval will be used for exit(RETVAL)

    my $self = shift;
    DEBUG > 1 and print "  Beginning process.\n";
    DEBUG > 1 and print "  Args: @{$self->{'args'}}\n\n";
    if(DEBUG > 3) {
        print "Object contents:\n";
        my @x = %$self;
        while(@x) {
            $x[1] = '<undef>'  unless defined $x[1];
            $x[1] = "@{$x[1]}" if ref( $x[1] ) eq 'ARRAY';
            print "  [$x[0]] => [$x[1]]\n";
            splice @x,0,2;
        }
        print "\n";
    }

    # TODO: make it deal with being invoked as various different things
    #  such as perlfaq".
  
    return $self->usage_brief  unless  @{ $self->{'args'} };
    $self->pagers_guessing;
    $self->options_reading;
    $self->aside(sprintf "$0 => %s v%s\n", ref($self), $self->VERSION);
    $self->drop_privs_maybe;
    $self->options_processing;
    
    # Hm, we have @pages and @found, but we only really act on one
    # file per call, with the exception of the opt_q hack, and with
    # -l things

    $self->aside("\n");

    my @pages;
    $self->{'pages'} = \@pages;
    if(    $self->opt_f) { @pages = ("perlfunc")               }
    elsif( $self->opt_q) { @pages = ("perlfaq1" .. "perlfaq9") }
    else                 { @pages = @{$self->{'args'}};
                           # @pages = __FILE__
                           #  if @pages == 1 and $pages[0] eq 'perldoc';
                         }

    return $self->usage_brief  unless  @pages;

    $self->find_good_formatter_class();
    $self->formatter_sanity_check();

    $self->maybe_diddle_INC();
      # for when we're apparently in a module or extension directory
    
    my @found = $self->grand_search_init(\@pages);
    exit (IS_VMS ? 98962 : 1) unless @found;
    
    if ($self->opt_l) {
        DEBUG and print "We're in -l mode, so byebye after this:\n";
        print join("\n", @found), "\n";
        return;
    }

    $self->tweak_found_pathnames(\@found);
    $self->assert_closing_stdout;
    return $self->page_module_file(@found)  if  $self->opt_m;
    DEBUG > 2 and print "Found: [@found]\n";

    return $self->render_and_page(\@found);
}

#..........................................................................
{

my( %class_seen, %class_loaded );
sub find_good_formatter_class {
  my $self = $_[0];
  my @class_list = @{ $self->{'formatter_classes'} || [] };
  die "WHAT?  Nothing in the formatter class list!?" unless @class_list;
  
  my $good_class_found;
  foreach my $c (@class_list) {
    DEBUG > 4 and print "Trying to load $c...\n";
    if($class_loaded{$c}) {
      DEBUG > 4 and print "OK, the already-loaded $c it is!\n";
      $good_class_found = $c;
      last;
    }
    
    if($class_seen{$c}) {
      DEBUG > 4 and print
       "I've tried $c before, and it's no good.  Skipping.\n";
      next;
    }
    
    $class_seen{$c} = 1;
    
    if( $c->can('parse_from_file') ) {
      DEBUG > 4 and print
       "Interesting, the formatter class $c is already loaded!\n";
      
    } elsif(
      (IS_VMS or IS_MSWin32 or IS_Dos or IS_OS2)
       # the alway case-insensitive fs's
      and $class_seen{lc("~$c")}++
    ) {
      DEBUG > 4 and print
       "We already used something quite like \"\L$c\E\", so no point using $c\n";
      # This avoids redefining the package.
    } else {
      DEBUG > 4 and print "Trying to eval 'require $c'...\n";

      local $^W = $^W;
      if(DEBUG() or $self->opt_v) {
        # feh, let 'em see it
      } else {
        $^W = 0;
        # The average user just has no reason to be seeing
        #  $^W-suppressable warnings from the the require!
      }

      eval "require $c";
      if($@) {
        DEBUG > 4 and print "Couldn't load $c: $!\n";
        next;
      }
    }
    
    if( $c->can('parse_from_file') ) {
      DEBUG > 4 and print "Settling on $c\n";
      my $v = $c->VERSION;
      $v = ( defined $v and length $v ) ? " version $v" : '';
      $self->aside("Formatter class $c$v successfully loaded!\n");
      $good_class_found = $c;
      last;
    } else {
      DEBUG > 4 and print "Class $c isn't a formatter?!  Skipping.\n";
    }
  }
  
  die "Can't find any loadable formatter class in @class_list?!\nAborting"
    unless $good_class_found;
  
  $self->{'formatter_class'} = $good_class_found;
  $self->aside("Will format with the class $good_class_found\n");
  
  return;
}

}
#..........................................................................

sub formatter_sanity_check {
  my $self = shift;
  my $formatter_class = $self->{'formatter_class'}
   || die "NO FORMATTER CLASS YET!?";
  
  if(!$self->opt_T # so -T can FORCE sending to STDOUT
    and $formatter_class->can('is_pageable')
    and !$formatter_class->is_pageable
    and !$formatter_class->can('page_for_perldoc')
  ) {
    my $ext =
     ($formatter_class->can('output_extension')
       && $formatter_class->output_extension
     ) || '';
    $ext = ".$ext" if length $ext;
    
    die
       "When using Perldoc to format with $formatter_class, you have to\n"
     . "specify -T or -dsomefile$ext\n"
     . "See `perldoc perldoc' for more information on those switches.\n"
    ;
  }
}

#..........................................................................

sub render_and_page {
    my($self, $found_list) = @_;
    
    $self->maybe_generate_dynamic_pod($found_list);

    my($out, $formatter) = $self->render_findings($found_list);
    
    if($self->opt_d) {
      printf "Perldoc (%s) output saved to %s\n",
        $self->{'formatter_class'} || ref($self),
        $out;
      print "But notice that it's 0 bytes long!\n" unless -s $out;
      
      
    } elsif(  # Allow the formatter to "page" itself, if it wants.
      $formatter->can('page_for_perldoc')
      and do {
        $self->aside("Going to call $formatter\->page_for_perldoc(\"$out\")\n");
        if( $formatter->page_for_perldoc($out, $self) ) {
          $self->aside("page_for_perldoc returned true, so NOT paging with $self.\n");
          1;
        } else {
          $self->aside("page_for_perldoc returned false, so paging with $self instead.\n");
          '';
        }
      }
    ) {
      # Do nothing, since the formatter has "paged" it for itself.
    
    } else {
      # Page it normally (internally)
      
      if( -s $out ) {  # Usual case:
        $self->page($out, $self->{'output_to_stdout'}, $self->pagers);
        
      } else {
        # Odd case:
        $self->aside("Skipping $out (from $$found_list[0] "
         . "via $$self{'formatter_class'}) as it is 0-length.\n");
         
        push @{ $self->{'temp_file_list'} }, $out;
        $self->unlink_if_temp_file($out);
      }
    }
    
    $self->after_rendering();  # any extra cleanup or whatever
    
    return;
}

#..........................................................................

sub options_reading {
    my $self = shift;
    
    if( defined $ENV{"PERLDOC"} and length $ENV{"PERLDOC"} ) {
      require Text::ParseWords;
      $self->aside("Noting env PERLDOC setting of $ENV{'PERLDOC'}\n");
      # Yes, appends to the beginning
      unshift @{ $self->{'args'} },
        Text::ParseWords::shellwords( $ENV{"PERLDOC"} )
      ;
      DEBUG > 1 and print "  Args now: @{$self->{'args'}}\n\n";
    } else {
      DEBUG > 1 and print "  Okay, no PERLDOC setting in ENV.\n";
    }

    DEBUG > 1
     and print "  Args right before switch processing: @{$self->{'args'}}\n";

    Pod::Perldoc::GetOptsOO::getopts( $self, $self->{'args'}, 'YES' )
     or return $self->usage;

    DEBUG > 1
     and print "  Args after switch processing: @{$self->{'args'}}\n";

    return $self->usage if $self->opt_h;
  
    return;
}

#..........................................................................

sub options_processing {
    my $self = shift;
    
    if ($self->opt_X) {
        my $podidx = "$Config{'archlib'}/pod.idx";
        $podidx = "" unless -f $podidx && -r _ && -M _ <= 7;
        $self->{'podidx'} = $podidx;
    }

    $self->{'output_to_stdout'} = 1  if  $self->opt_T or ! -t STDOUT;

    $self->options_sanity;

    $self->opt_n("nroff") unless $self->opt_n;
    $self->add_formatter_option( '__nroffer' => $self->opt_n );

    # Adjust for using translation packages
    $self->add_translator($self->opt_L) if $self->opt_L;

    return;
}

#..........................................................................

sub options_sanity {
    my $self = shift;

    # The opts-counting stuff interacts quite badly with
    # the $ENV{"PERLDOC"} stuff.  I.e., if I have $ENV{"PERLDOC"}
    # set to -t, and I specify -u on the command line, I don't want
    # to be hectored at that -u and -t don't make sense together.

    #my $opts = grep $_ && 1, # yes, the count of the set ones
    #  $self->opt_t, $self->opt_u, $self->opt_m, $self->opt_l
    #;
    #
    #$self->usage("only one of -t, -u, -m or -l") if $opts > 1;
    
    
    # Any sanity-checking need doing here?
    
    # But does not make sense to set either -f or -q in $ENV{"PERLDOC"} 
    if( $self->opt_f or $self->opt_q ) { 
	$self->usage("Only one of -f -or -q") if $self->opt_f and $self->opt_q;
	warn 
	    "Perldoc is only really meant for reading one word at a time.\n",
	    "So these parameters are being ignored: ",
	    join(' ', @{$self->{'args'}}),
	    "\n"
		if @{$self->{'args'}}
    }
    return;
}

#..........................................................................

sub grand_search_init {
    my($self, $pages, @found) = @_;

    foreach (@$pages) {
        if ($self->{'podidx'} && open(PODIDX, $self->{'podidx'})) {
            my $searchfor = catfile split '::', $_;
            $self->aside( "Searching for '$searchfor' in $self->{'podidx'}\n" );
            local $_;
            while (<PODIDX>) {
                chomp;
                push(@found, $_) if m,/$searchfor(?:\.(?:pod|pm))?\z,i;
            }
            close(PODIDX)            or die "Can't close $$self{'podidx'}: $!";
            next;
        }

        $self->aside( "Searching for $_\n" );

        if ($self->opt_F) {
            next unless -r;
            push @found, $_ if $self->opt_m or $self->containspod($_);
            next;
        }

        my @searchdirs;

        # prepend extra search directories (including language specific)
        push @searchdirs, @{ $self->{'extra_search_dirs'} };

        # We must look both in @INC for library modules and in $bindir
        # for executables, like h2xs or perldoc itself.
        push @searchdirs, ($self->{'bindir'}, @INC);
        unless ($self->opt_m) {
            if (IS_VMS) {
                my($i,$trn);
                for ($i = 0; $trn = $ENV{'DCL$PATH;'.$i}; $i++) {
                    push(@searchdirs,$trn);
                }
                push(@searchdirs,'perl_root:[lib.pod]')  # installed pods
            }
            else {
                push(@searchdirs, grep(-d, split($Config{path_sep},
                                                 $ENV{'PATH'})));
            }
        }
        my @files = $self->searchfor(0,$_,@searchdirs);
        if (@files) {
            $self->aside( "Found as @files\n" );
        }
        else {
            # no match, try recursive search
            @searchdirs = grep(!/^\.\z/s,@INC);
            @files= $self->searchfor(1,$_,@searchdirs) if $self->opt_r;
            if (@files) {
                $self->aside( "Loosely found as @files\n" );
            }
            else {
                print STDERR "No " .
                    ($self->opt_m ? "module" : "documentation") . " found for \"$_\".\n";
                if ( @{ $self->{'found'} } ) {
                    print STDERR "However, try\n";
                    for my $dir (@{ $self->{'found'} }) {
                        opendir(DIR, $dir) or die "opendir $dir: $!";
                        while (my $file = readdir(DIR)) {
                            next if ($file =~ /^\./s);
                            $file =~ s/\.(pm|pod)\z//;  # XXX: badfs
                            print STDERR "\tperldoc $_\::$file\n";
                        }
                        closedir(DIR)    or die "closedir $dir: $!";
                    }
                }
            }
        }
        push(@found,@files);
    }
    return @found;
}

#..........................................................................

sub maybe_generate_dynamic_pod {
    my($self, $found_things) = @_;
    my @dynamic_pod;
    
    $self->search_perlfunc($found_things, \@dynamic_pod)  if  $self->opt_f;
    
    $self->search_perlfaqs($found_things, \@dynamic_pod)  if  $self->opt_q;

    if( ! $self->opt_f and ! $self->opt_q ) {
        DEBUG > 4 and print "That's a non-dynamic pod search.\n";
    } elsif ( @dynamic_pod ) {
        $self->aside("Hm, I found some Pod from that search!\n");
        my ($buffd, $buffer) = $self->new_tempfile('pod', 'dyn');
        
        push @{ $self->{'temp_file_list'} }, $buffer;
         # I.e., it MIGHT be deleted at the end.
        
	my $in_list = $self->opt_f;

        print $buffd "=over 8\n\n" if $in_list;
        print $buffd @dynamic_pod  or die "Can't print $buffer: $!";
        print $buffd "=back\n"     if $in_list;

        close $buffd        or die "Can't close $buffer: $!";
        
        @$found_things = $buffer;
          # Yes, so found_things never has more than one thing in
          #  it, by time we leave here
        
        $self->add_formatter_option('__filter_nroff' => 1);

    } else {
        @$found_things = ();
        $self->aside("I found no Pod from that search!\n");
    }

    return;
}

#..........................................................................

sub add_formatter_option { # $self->add_formatter_option('key' => 'value');
  my $self = shift;
  push @{ $self->{'formatter_switches'} }, [ @_ ] if @_;

  DEBUG > 3 and printf "Formatter switches now: [%s]\n",
   join ' ', map "[@$_]", @{ $self->{'formatter_switches'} };
  
  return;
}

#.........................................................................

sub new_translator { # $tr = $self->new_translator($lang);
    my $self = shift;
    my $lang = shift;

    my $pack = 'POD2::' . uc($lang);
    eval "require $pack";
    if ( !$@ && $pack->can('new') ) {
	return $pack->new();
    }

    eval { require POD2::Base };
    return if $@;
    
    return POD2::Base->new({ lang => $lang });
}

#.........................................................................

sub add_translator { # $self->add_translator($lang);
    my $self = shift;
    for my $lang (@_) {
        my $tr = $self->new_translator($lang);
        if ( defined $tr ) {
            push @{ $self->{'translators'} }, $tr;
            push @{ $self->{'extra_search_dirs'} }, $tr->pod_dirs;

            $self->aside( "translator for '$lang' loaded\n" );
        } else {
            # non-installed or bad translator package
            warn "Perldoc cannot load translator package for '$lang': ignored\n";
        }

    }
    return;
}

#..........................................................................

sub search_perlfunc {
    my($self, $found_things, $pod) = @_;

    DEBUG > 2 and print "Search: @$found_things\n";

    my $perlfunc = shift @$found_things;
    open(PFUNC, "<", $perlfunc)               # "Funk is its own reward"
        or die("Can't open $perlfunc: $!");

    # Functions like -r, -e, etc. are listed under `-X'.
    my $search_re = ($self->opt_f =~ /^-[rwxoRWXOeszfdlpSbctugkTBMAC]$/)
                        ? '(?:I<)?-X' : quotemeta($self->opt_f) ;

    DEBUG > 2 and
     print "Going to perlfunc-scan for $search_re in $perlfunc\n";

    my $re = 'Alphabetical Listing of Perl Functions';
    if ( $self->opt_L ) {
        my $tr = $self->{'translators'}->[0];
        $re =  $tr->search_perlfunc_re if $tr->can('search_perlfunc_re');
    }

    # Skip introduction
    local $_;
    while (<PFUNC>) {
        last if /^=head2 $re/;
    }

    # Look for our function
    my $found = 0;
    my $inlist = 0;
    while (<PFUNC>) {  # "The Mothership Connection is here!"
        if ( m/^=item\s+$search_re\b/ )  {
            $found = 1;
        }
        elsif (/^=item/) {
            last if $found > 1 and not $inlist;
        }
        next unless $found;
        if (/^=over/) {
            ++$inlist;
        }
        elsif (/^=back/) {
            --$inlist;
        }
        push @$pod, $_;
        ++$found if /^\w/;        # found descriptive text
    }
    if (!@$pod) {
        die sprintf
          "No documentation for perl function `%s' found\n",
          $self->opt_f
        ;
    }
    close PFUNC                or die "Can't open $perlfunc: $!";

    return;
}

#..........................................................................

sub search_perlfaqs {
    my( $self, $found_things, $pod) = @_;

    my $found = 0;
    my %found_in;
    my $search_key = $self->opt_q;
    
    my $rx = eval { qr/$search_key/ }
     or die <<EOD;
Invalid regular expression '$search_key' given as -q pattern:
$@
Did you mean \\Q$search_key ?

EOD

    local $_;
    foreach my $file (@$found_things) {
        die "invalid file spec: $!" if $file =~ /[<>|]/;
        open(INFAQ, "<", $file)  # XXX 5.6ism
         or die "Can't read-open $file: $!\nAborting";
        while (<INFAQ>) {
            if ( m/^=head2\s+.*(?:$search_key)/i ) {
                $found = 1;
                push @$pod, "=head1 Found in $file\n\n" unless $found_in{$file}++;
            }
            elsif (/^=head[12]/) {
                $found = 0;
            }
            next unless $found;
            push @$pod, $_;
        }
        close(INFAQ);
    }
    die("No documentation for perl FAQ keyword `$search_key' found\n")
     unless @$pod;

    return;
}


#..........................................................................

sub render_findings {
  # Return the filename to open

  my($self, $found_things) = @_;

  my $formatter_class = $self->{'formatter_class'}
   || die "No formatter class set!?";
  my $formatter = $formatter_class->can('new')
    ? $formatter_class->new
    : $formatter_class
  ;

  if(! @$found_things) {
    die "Nothing found?!";
    # should have been caught before here
  } elsif(@$found_things > 1) {
    warn 
     "Perldoc is only really meant for reading one document at a time.\n",
     "So these parameters are being ignored: ",
     join(' ', @$found_things[1 .. $#$found_things] ),
     "\n"
  }

  my $file = $found_things->[0];
  
  DEBUG > 3 and printf "Formatter switches now: [%s]\n",
   join ' ', map "[@$_]", @{ $self->{'formatter_switches'} };

  # Set formatter options:
  if( ref $formatter ) {
    foreach my $f (@{ $self->{'formatter_switches'} || [] }) {
      my($switch, $value, $silent_fail) = @$f;
      if( $formatter->can($switch) ) {
        eval { $formatter->$switch( defined($value) ? $value : () ) };
        warn "Got an error when setting $formatter_class\->$switch:\n$@\n"
         if $@;
      } else {
        if( $silent_fail or $switch =~ m/^__/s ) {
          DEBUG > 2 and print "Formatter $formatter_class doesn't support $switch\n";
        } else {
          warn "$formatter_class doesn't recognize the $switch switch.\n";
        }
      }
    }
  }
  
  $self->{'output_is_binary'} =
    $formatter->can('write_with_binmode') && $formatter->write_with_binmode;

  my ($out_fh, $out) = $self->new_output_file(
    ( $formatter->can('output_extension') && $formatter->output_extension )
     || undef,
    $self->useful_filename_bit,
  );

  # Now, finally, do the formatting!
  {
    local $^W = $^W;
    if(DEBUG() or $self->opt_v) {
      # feh, let 'em see it
    } else {
      $^W = 0;
      # The average user just has no reason to be seeing
      #  $^W-suppressable warnings from the formatting!
    }
          
    eval {  $formatter->parse_from_file( $file, $out_fh )  };
  }
  
  warn "Error while formatting with $formatter_class:\n $@\n" if $@;
  DEBUG > 2 and print "Back from formatting with $formatter_class\n";

  close $out_fh 
   or warn "Can't close $out: $!\n(Did $formatter already close it?)";
  sleep 0; sleep 0; sleep 0;
   # Give the system a few timeslices to meditate on the fact
   # that the output file does in fact exist and is closed.
  
  $self->unlink_if_temp_file($file);

  unless( -s $out ) {
    if( $formatter->can( 'if_zero_length' ) ) {
      # Basically this is just a hook for Pod::Simple::Checker; since
      # what other class could /happily/ format an input file with Pod
      # as a 0-length output file?
      $formatter->if_zero_length( $file, $out, $out_fh );
    } else {
      warn "Got a 0-length file from $$found_things[0] via $formatter_class!?\n"
    }
  }

  DEBUG and print "Finished writing to $out.\n";
  return($out, $formatter) if wantarray;
  return $out;
}

#..........................................................................

sub unlink_if_temp_file {
  # Unlink the specified file IFF it's in the list of temp files.
  # Really only used in the case of -f / -q things when we can
  #  throw away the dynamically generated source pod file once
  #  we've formatted it.
  #
  my($self, $file) = @_;
  return unless defined $file and length $file;
  
  my $temp_file_list = $self->{'temp_file_list'} || return;
  if(grep $_ eq $file, @$temp_file_list) {
    $self->aside("Unlinking $file\n");
    unlink($file) or warn "Odd, couldn't unlink $file: $!";
  } else {
    DEBUG > 1 and print "$file isn't a temp file, so not unlinking.\n";
  }
  return;
}

#..........................................................................

sub MSWin_temp_cleanup {

  # Nothing particularly MSWin-specific in here, but I don't know if any
  # other OS needs its temp dir policed like MSWin does!
 
  my $self = shift;

  my $tempdir = $ENV{'TEMP'};
  return unless defined $tempdir and length $tempdir
   and -e $tempdir and -d _ and -w _;

  $self->aside(
   "Considering whether any old files of mine in $tempdir need unlinking.\n"
  );

  opendir(TMPDIR, $tempdir) || return;
  my @to_unlink;
  
  my $limit = time() - $Temp_File_Lifetime;
  
  DEBUG > 5 and printf "Looking for things pre-dating %s (%x)\n",
   ($limit) x 2;
  
  my $filespec;
  
  while(defined($filespec = readdir(TMPDIR))) {
    if(
     $filespec =~ m{^perldoc_[a-zA-Z0-9]+_T([a-fA-F0-9]{7,})_[a-fA-F0-9]{3,}}s
    ) {
      if( hex($1) < $limit ) {
        push @to_unlink, "$tempdir/$filespec";
        $self->aside( "Will unlink my old temp file $to_unlink[-1]\n" );
      } else {
        DEBUG > 5 and
         printf "  $tempdir/$filespec is too recent (after %x)\n", $limit;
      }
    } else {
      DEBUG > 5 and
       print "  $tempdir/$filespec doesn't look like a perldoc temp file.\n";
    }
  }
  closedir(TMPDIR);
  $self->aside(sprintf "Unlinked %s items of mine in %s\n",
    scalar(unlink(@to_unlink)),
    $tempdir
  );
  return;
}

#  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .  .

sub MSWin_perldoc_tempfile {
  my($self, $suffix, $infix) = @_;

  my $tempdir = $ENV{'TEMP'};
  return unless defined $tempdir and length $tempdir
   and -e $tempdir and -d _ and -w _;

  my $spec;
  
  do {
    $spec = sprintf "%s\\perldoc_%s_T%x_%x%02x.%s", # used also in MSWin_temp_cleanup
      # Yes, we embed the create-time in the filename!
      $tempdir,
      $infix || 'x',
      time(),
      $$,
      defined( &Win32::GetTickCount )
        ? (Win32::GetTickCount() & 0xff)
        : int(rand 256)
       # Under MSWin, $$ values get reused quickly!  So if we ran
       # perldoc foo and then perldoc bar before there was time for
       # time() to increment time."_$$" would likely be the same
       # for each process!  So we tack on the tick count's lower
       # bits (or, in a pinch, rand)
      ,
      $suffix || 'txt';
    ;
  } while( -e $spec );

  my $counter = 0;
  
  while($counter < 50) {
    my $fh;
    # If we are running before perl5.6.0, we can't autovivify
    if ($] < 5.006) {
      require Symbol;
      $fh = Symbol::gensym();
    }
    DEBUG > 3 and print "About to try making temp file $spec\n";
    return($fh, $spec) if open($fh, ">", $spec);    # XXX 5.6ism
    $self->aside("Can't create temp file $spec: $!\n");
  }

  $self->aside("Giving up on making a temp file!\n");
  die "Can't make a tempfile!?";
}

#..........................................................................


sub after_rendering {
  my $self = $_[0];
  $self->after_rendering_VMS     if IS_VMS;
  $self->after_rendering_MSWin32 if IS_MSWin32;
  $self->after_rendering_Dos     if IS_Dos;
  $self->after_rendering_OS2     if IS_OS2;
  return;
}

sub after_rendering_VMS      { return }
sub after_rendering_Dos      { return }
sub after_rendering_OS2      { return }

sub after_rendering_MSWin32  {
  shift->MSWin_temp_cleanup() if $Temp_Files_Created;
}

#..........................................................................
#	:	:	:	:	:	:	:	:	:
#..........................................................................


sub minus_f_nocase {   # i.e., do like -f, but without regard to case

     my($self, $dir, $file) = @_;
     my $path = catfile($dir,$file);
     return $path if -f $path and -r _;

     if(!$self->opt_i
        or IS_VMS or IS_MSWin32
        or IS_Dos or IS_OS2
     ) {
        # On a case-forgiving file system, or if case is important,
	#  that is it, all we can do.
	warn "Ignored $path: unreadable\n" if -f _;
	return '';
     }
     
     local *DIR;
     my @p = ($dir);
     my($p,$cip);
     foreach $p (splitdir $file){
	my $try = catfile @p, $p;
        $self->aside("Scrutinizing $try...\n");
	stat $try;
 	if (-d _) {
 	    push @p, $p;
	    if ( $p eq $self->{'target'} ) {
		my $tmp_path = catfile @p;
		my $path_f = 0;
		for (@{ $self->{'found'} }) {
		    $path_f = 1 if $_ eq $tmp_path;
		}
		push (@{ $self->{'found'} }, $tmp_path) unless $path_f;
		$self->aside( "Found as $tmp_path but directory\n" );
	    }
 	}
	elsif (-f _ && -r _) {
 	    return $try;
 	}
	elsif (-f _) {
	    warn "Ignored $try: unreadable\n";
 	}
	elsif (-d catdir(@p)) {  # at least we see the containing directory!
 	    my $found = 0;
 	    my $lcp = lc $p;
 	    my $p_dirspec = catdir(@p);
 	    opendir DIR, $p_dirspec  or die "opendir $p_dirspec: $!";
 	    while(defined( $cip = readdir(DIR) )) {
 		if (lc $cip eq $lcp){
 		    $found++;
 		    last; # XXX stop at the first? what if there's others?
 		}
 	    }
 	    closedir DIR  or die "closedir $p_dirspec: $!";
 	    return "" unless $found;

 	    push @p, $cip;
 	    my $p_filespec = catfile(@p);
 	    return $p_filespec if -f $p_filespec and -r _;
	    warn "Ignored $p_filespec: unreadable\n" if -f _;
 	}
     }
     return "";
}

#..........................................................................

sub pagers_guessing {
    my $self = shift;

    my @pagers;
    push @pagers, $self->pagers;
    $self->{'pagers'} = \@pagers;

    if (IS_MSWin32) {
        push @pagers, qw( more< less notepad );
        unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
    }
    elsif (IS_VMS) {
        push @pagers, qw( most more less type/page );
    }
    elsif (IS_Dos) {
        push @pagers, qw( less.exe more.com< );
        unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
    }
    else {
        if (IS_OS2) {
          unshift @pagers, 'less', 'cmd /c more <';
        }
        push @pagers, qw( more less pg view cat );
        unshift @pagers, $ENV{PAGER}  if $ENV{PAGER};
    }

    if (IS_Cygwin) {
        if (($pagers[0] eq 'less') || ($pagers[0] eq '/usr/bin/less')) {
            unshift @pagers, '/usr/bin/less -isrR';
        }
    }

    unshift @pagers, $ENV{PERLDOC_PAGER} if $ENV{PERLDOC_PAGER};
    
    return;   
}

#..........................................................................

sub page_module_file {
    my($self, @found) = @_;

    # Security note:
    # Don't ever just pass this off to anything like MSWin's "start.exe",
    # since we might be calling on a .pl file, and we wouldn't want that
    # to actually /execute/ the file that we just want to page thru!
    # Also a consideration if one were to use a web browser as a pager;
    # doing so could trigger the browser's MIME mapping for whatever
    # it thinks .pm/.pl/whatever is.  Probably just a (useless and
    # annoying) "Save as..." dialog, but potentially executing the file
    # in question -- particularly in the case of MSIE and it's, ahem,
    # occasionally hazy distinction between OS-local extension
    # associations, and browser-specific MIME mappings.

    if ($self->{'output_to_stdout'}) {
        $self->aside("Sending unpaged output to STDOUT.\n");
	local $_;
	my $any_error = 0;
        foreach my $output (@found) {
	    unless( open(TMP, "<", $output) ) {    # XXX 5.6ism
	      warn("Can't open $output: $!");
	      $any_error = 1;
	      next;
	    }
	    while (<TMP>) {
	        print or die "Can't print to stdout: $!";
	    } 
	    close TMP  or die "Can't close while $output: $!";
	    $self->unlink_if_temp_file($output);
	}
	return $any_error; # successful
    }

    foreach my $pager ( $self->pagers ) {
        $self->aside("About to try calling $pager @found\n");
        if (system($pager, @found) == 0) {
            $self->aside("Yay, it worked.\n");
            return 0;
        }
        $self->aside("That didn't work.\n");
        
        # Odd -- when it fails, under Win32, this seems to neither
        #  return with a fail nor return with a success!!
        #  That's discouraging!
    }

    $self->aside(
      sprintf "Can't manage to find a way to page [%s] via pagers [%s]\n",
      join(' ', @found),
      join(' ', $self->pagers),
    );
    
    if (IS_VMS) { 
        DEBUG > 1 and print "Bailing out in a VMSish way.\n";
        eval q{
            use vmsish qw(status exit); 
            exit $?;
            1;
        } or die;
    }
    
    return 1;
      # i.e., an UNSUCCESSFUL return value!
}

#..........................................................................

sub check_file {
    my($self, $dir, $file) = @_;
    
    unless( ref $self ) {
      # Should never get called:
      $Carp::Verbose = 1;
      require Carp;
      Carp::croak( join '',
        "Crazy ", __PACKAGE__, " error:\n",
        "check_file must be an object_method!\n",
        "Aborting"
      );
    }
    
    if(length $dir and not -d $dir) {
      DEBUG > 3 and print "  No dir $dir -- skipping.\n";
      return "";
    }
    
    if ($self->opt_m) {
	return $self->minus_f_nocase($dir,$file);
    }
    
    else {
	my $path = $self->minus_f_nocase($dir,$file);
        if( length $path and $self->containspod($path) ) {
            DEBUG > 3 and print
              "  The file $path indeed looks promising!\n";
            return $path;
        }
    }
    DEBUG > 3 and print "  No good: $file in $dir\n";
    
    return "";
}

#..........................................................................

sub containspod {
    my($self, $file, $readit) = @_;
    return 1 if !$readit && $file =~ /\.pod\z/i;


    #  Under cygwin the /usr/bin/perl is legal executable, but
    #  you cannot open a file with that name. It must be spelled
    #  out as "/usr/bin/perl.exe".
    #
    #  The following if-case under cygwin prevents error
    #
    #     $ perldoc perl
    #     Cannot open /usr/bin/perl: no such file or directory
    #
    #  This would work though
    #
    #     $ perldoc perl.pod

    if ( IS_Cygwin  and  -x $file  and  -f "$file.exe" )
    {
        warn "Cygwin $file.exe search skipped\n"  if DEBUG or $self->opt_v;
        return 0;
    }

    local($_);
    open(TEST,"<", $file) 	or die "Can't open $file: $!";   # XXX 5.6ism
    while (<TEST>) {
	if (/^=head/) {
	    close(TEST) 	or die "Can't close $file: $!";
	    return 1;
	}
    }
    close(TEST) 		or die "Can't close $file: $!";
    return 0;
}

#..........................................................................

sub maybe_diddle_INC {
  my $self = shift;
  
  # Does this look like a module or extension directory?
  
  if (-f "Makefile.PL" || -f "Build.PL") {

    # Add "." and "lib" to @INC (if they exist)
    eval q{ use lib qw(. lib); 1; } or die;

    # don't add if superuser
    if ($< && $> && -d "blib") {   # don't be looking too hard now!
      eval q{ use blib; 1 };
      warn $@ if $@ && $self->opt_v;
    }
  }
  
  return;
}

#..........................................................................

sub new_output_file {
  my $self = shift;
  my $outspec = $self->opt_d;  # Yes, -d overrides all else!
                               # So don't call this twice per format-job!
  
  return $self->new_tempfile(@_) unless defined $outspec and length $outspec;

  # Otherwise open a write-handle on opt_d!f

  my $fh;
  # If we are running before perl5.6.0, we can't autovivify
  if ($] < 5.006) {
    require Symbol;
    $fh = Symbol::gensym();
  }
  DEBUG > 3 and print "About to try writing to specified output file $outspec\n";
  die "Can't write-open $outspec: $!"
   unless open($fh, ">", $outspec); # XXX 5.6ism
  
  DEBUG > 3 and print "Successfully opened $outspec\n";
  binmode($fh) if $self->{'output_is_binary'};
  return($fh, $outspec);
}

#..........................................................................

sub useful_filename_bit {
  # This tries to provide a meaningful bit of text to do with the query,
  # such as can be used in naming the file -- since if we're going to be
  # opening windows on temp files (as a "pager" may well do!) then it's
  # better if the temp file's name (which may well be used as the window
  # title) isn't ALL just random garbage!
  # In other words "perldoc_LWPSimple_2371981429" is a better temp file
  # name than "perldoc_2371981429".  So this routine is what tries to
  # provide the "LWPSimple" bit.
  #
  my $self = shift;
  my $pages = $self->{'pages'} || return undef;
  return undef unless @$pages;
  
  my $chunk = $pages->[0];
  return undef unless defined $chunk;
  $chunk =~ s/:://g;
  $chunk =~ s/\.\w+$//g; # strip any extension
  if( $chunk =~ m/([^\#\\:\/\$]+)$/s ) { # get basename, if it's a file
    $chunk = $1;
  } else {
    return undef;
  }
  $chunk =~ s/[^a-zA-Z0-9]+//g; # leave ONLY a-zA-Z0-9 things!
  $chunk = substr($chunk, -10) if length($chunk) > 10;
  return $chunk;
}

#..........................................................................

sub new_tempfile {    # $self->new_tempfile( [$suffix, [$infix] ] )
  my $self = shift;

  ++$Temp_Files_Created;

  if( IS_MSWin32 ) {
    my @out = $self->MSWin_perldoc_tempfile(@_);
    return @out if @out;
    # otherwise fall thru to the normal stuff below...
  }
  
  require File::Temp;
  return File::Temp::tempfile(UNLINK => 1);
}

#..........................................................................

sub page {  # apply a pager to the output file
    my ($self, $output, $output_to_stdout, @pagers) = @_;
    if ($output_to_stdout) {
        $self->aside("Sending unpaged output to STDOUT.\n");
	open(TMP, "<", $output)  or  die "Can't open $output: $!"; # XXX 5.6ism
	local $_;
	while (<TMP>) {
	    print or die "Can't print to stdout: $!";
	} 
	close TMP  or die "Can't close while $output: $!";
	$self->unlink_if_temp_file($output);
    } else {
        # On VMS, quoting prevents logical expansion, and temp files with no
        # extension get the wrong default extension (such as .LIS for TYPE)

        $output = VMS::Filespec::rmsexpand($output, '.') if IS_VMS;

        $output =~ s{/}{\\}g if IS_MSWin32 || IS_Dos;
          # Altho "/" under MSWin is in theory good as a pathsep,
          #  many many corners of the OS don't like it.  So we
          #  have to force it to be "\" to make everyone happy.

        foreach my $pager (@pagers) {
            $self->aside("About to try calling $pager $output\n");
            if (IS_VMS) {
                last if system("$pager $output") == 0;
            } else {
	        last if system("$pager \"$output\"") == 0;
            }
	}
    }
    return;
}

#..........................................................................

sub searchfor {
    my($self, $recurse,$s,@dirs) = @_;
    $s =~ s!::!/!g;
    $s = VMS::Filespec::unixify($s) if IS_VMS;
    return $s if -f $s && $self->containspod($s);
    $self->aside( "Looking for $s in @dirs\n" );
    my $ret;
    my $i;
    my $dir;
    $self->{'target'} = (splitdir $s)[-1];  # XXX: why not use File::Basename?
    for ($i=0; $i<@dirs; $i++) {
	$dir = $dirs[$i];
	next unless -d $dir;
	($dir = VMS::Filespec::unixpath($dir)) =~ s!/\z!! if IS_VMS;
	if (       (! $self->opt_m && ( $ret = $self->check_file($dir,"$s.pod")))
		or ( $ret = $self->check_file($dir,"$s.pm"))
		or ( $ret = $self->check_file($dir,$s))
		or ( IS_VMS and
		     $ret = $self->check_file($dir,"$s.com"))
		or ( IS_OS2 and
		     $ret = $self->check_file($dir,"$s.cmd"))
		or ( (IS_MSWin32 or IS_Dos or IS_OS2) and
		     $ret = $self->check_file($dir,"$s.bat"))
		or ( $ret = $self->check_file("$dir/pod","$s.pod"))
		or ( $ret = $self->check_file("$dir/pod",$s))
		or ( $ret = $self->check_file("$dir/pods","$s.pod"))
		or ( $ret = $self->check_file("$dir/pods",$s))
	) {
	    DEBUG > 1 and print "  Found $ret\n";
	    return $ret;
	}

	if ($recurse) {
	    opendir(D,$dir)	or die "Can't opendir $dir: $!";
	    my @newdirs = map catfile($dir, $_), grep {
		not /^\.\.?\z/s and
		not /^auto\z/s  and   # save time! don't search auto dirs
		-d  catfile($dir, $_)
	    } readdir D;
	    closedir(D)		or die "Can't closedir $dir: $!";
	    next unless @newdirs;
	    # what a wicked map!
	    @newdirs = map((s/\.dir\z//,$_)[1],@newdirs) if IS_VMS;
	    $self->aside( "Also looking in @newdirs\n" );
	    push(@dirs,@newdirs);
	}
    }
    return ();
}

#..........................................................................
{
  my $already_asserted;
  sub assert_closing_stdout {
    my $self = shift;

    return if $already_asserted;

    eval  q~ END { close(STDOUT) || die "Can't close STDOUT: $!" } ~;
     # What for? to let the pager know that nothing more will come?
  
    die $@ if $@;
    $already_asserted = 1;
    return;
  }
}

#..........................................................................

sub tweak_found_pathnames {
  my($self, $found) = @_;
  if (IS_MSWin32) {
    foreach (@$found) { s,/,\\,g }
  }
  return;
}

#..........................................................................
#	:	:	:	:	:	:	:	:	:
#..........................................................................

sub am_taint_checking {
    my $self = shift;
    die "NO ENVIRONMENT?!?!" unless keys %ENV; # reset iterator along the way
    my($k,$v) = each %ENV;
    return is_tainted($v);  
}

#..........................................................................

sub is_tainted { # just a function
    my $arg  = shift;
    my $nada = substr($arg, 0, 0);  # zero-length!
    local $@;  # preserve the caller's version of $@
    eval { eval "# $nada" };
    return length($@) != 0;
}

#..........................................................................

sub drop_privs_maybe {
    my $self = shift;
    
    # Attempt to drop privs if we should be tainting and aren't
    if (!(IS_VMS || IS_MSWin32 || IS_Dos
          || IS_OS2
         )
        && ($> == 0 || $< == 0)
        && !$self->am_taint_checking()
    ) {
        my $id = eval { getpwnam("nobody") };
        $id = eval { getpwnam("nouser") } unless defined $id;
        $id = -2 unless defined $id;
            #
            # According to Stevens' APUE and various
            # (BSD, Solaris, HP-UX) man pages, setting
            # the real uid first and effective uid second
            # is the way to go if one wants to drop privileges,
            # because if one changes into an effective uid of
            # non-zero, one cannot change the real uid any more.
            #
            # Actually, it gets even messier.  There is
            # a third uid, called the saved uid, and as
            # long as that is zero, one can get back to
            # uid of zero.  Setting the real-effective *twice*
            # helps in *most* systems (FreeBSD and Solaris)
            # but apparently in HP-UX even this doesn't help:
            # the saved uid stays zero (apparently the only way
            # in HP-UX to change saved uid is to call setuid()
            # when the effective uid is zero).
            #
        eval {
            $< = $id; # real uid
            $> = $id; # effective uid
            $< = $id; # real uid
            $> = $id; # effective uid
        };
        if( !$@ && $< && $> ) {
          DEBUG and print "OK, I dropped privileges.\n";
        } elsif( $self->opt_U ) {
          DEBUG and print "Couldn't drop privileges, but in -U mode, so feh."
        } else {
          DEBUG and print "Hm, couldn't drop privileges.  Ah well.\n";
          # We used to die here; but that seemed pointless.
        }
    }
    return;
}

#..........................................................................

1;

__END__

# See "perldoc perldoc" for basic details.
#
# Perldoc -- look up a piece of documentation in .pod format that
# is embedded in the perl installation tree.
# 
#~~~~~~
#
# See ChangeLog in CPAN dist for Pod::Perldoc for later notes.
#
# Version 3.01: Sun Nov 10 21:38:09 MST 2002
#       Sean M. Burke <sburke@cpan.org>
#       Massive refactoring and code-tidying.
#       Now it's a module(-family)!
#       Formatter-specific stuff pulled out into Pod::Perldoc::To(Whatever).pm
#       Added -T, -d, -o, -M, -w.
#       Added some improved MSWin funk.
#
#~~~~~~
#
# Version 2.05: Sat Oct 12 16:09:00 CEST 2002
#	Hugo van der Sanden <hv@crypt.org>
#	Made -U the default, based on patch from Simon Cozens
# Version 2.04: Sun Aug 18 13:27:12 BST 2002
#	Randy W. Sims <RandyS@ThePierianSpring.org>
#	allow -n to enable nroff under Win32
# Version 2.03: Sun Apr 23 16:56:34 BST 2000
#	Hugo van der Sanden <hv@crypt.org>
#	don't die when 'use blib' fails
# Version 2.02: Mon Mar 13 18:03:04 MST 2000
#       Tom Christiansen <tchrist@perl.com>
#	Added -U insecurity option
# Version 2.01: Sat Mar 11 15:22:33 MST 2000 
#       Tom Christiansen <tchrist@perl.com>, querulously.
#       Security and correctness patches.
#       What a twisted bit of distasteful spaghetti code.
# Version 2.0: ????
#
#~~~~~~
#
# Version 1.15: Tue Aug 24 01:50:20 EST 1999
#       Charles Wilson <cwilson@ece.gatech.edu>
#	changed /pod/ directory to /pods/ for cygwin
#         to support cygwin/win32
# Version 1.14: Wed Jul 15 01:50:20 EST 1998
#       Robin Barker <rmb1@cise.npl.co.uk>
#	-strict, -w cleanups
# Version 1.13: Fri Feb 27 16:20:50 EST 1997
#       Gurusamy Sarathy <gsar@activestate.com>
#	-doc tweaks for -F and -X options
# Version 1.12: Sat Apr 12 22:41:09 EST 1997
#       Gurusamy Sarathy <gsar@activestate.com>
#	-various fixes for win32
# Version 1.11: Tue Dec 26 09:54:33 EST 1995
#       Kenneth Albanowski <kjahds@kjahds.com>
#   -added Charles Bailey's further VMS patches, and -u switch
#   -added -t switch, with pod2text support
#
# Version 1.10: Thu Nov  9 07:23:47 EST 1995
#		Kenneth Albanowski <kjahds@kjahds.com>
#	-added VMS support
#	-added better error recognition (on no found pages, just exit. On
#	 missing nroff/pod2man, just display raw pod.)
#	-added recursive/case-insensitive matching (thanks, Andreas). This
#	 slows things down a bit, unfortunately. Give a precise name, and
#	 it'll run faster.
#
# Version 1.01:	Tue May 30 14:47:34 EDT 1995
#		Andy Dougherty  <doughera@lafcol.lafayette.edu>
#   -added pod documentation.
#   -added PATH searching.
#   -added searching pod/ subdirectory (mainly to pick up perlfunc.pod
#    and friends.
#
#~~~~~~~
#
# TODO:
#
#	Cache the directories read during sloppy match
#       (To disk, or just in-memory?)
#
#       Backport this to perl 5.005?
#
#       Implement at least part of the "perlman" interface described
#       in Programming Perl 3e?
