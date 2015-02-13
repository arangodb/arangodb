package ExtUtils::ParseXS;

use 5.006;  # We use /??{}/ in regexes
use Cwd;
use Config;
use File::Basename;
use File::Spec;
use Symbol;

require Exporter;

@ISA = qw(Exporter);
@EXPORT_OK = qw(process_file);

# use strict;  # One of these days...

my(@XSStack);	# Stack of conditionals and INCLUDEs
my($XSS_work_idx, $cpp_next_tmp);

use vars qw($VERSION);
$VERSION = '2.19_01';

use vars qw(%input_expr %output_expr $ProtoUsed @InitFileCode $FH $proto_re $Overload $errors $Fallback
	    $cplusplus $hiertype $WantPrototypes $WantVersionChk $except $WantLineNumbers
	    $WantOptimize $process_inout $process_argtypes @tm
	    $dir $filename $filepathname %IncludedFiles
	    %type_kind %proto_letter
            %targetable $BLOCK_re $lastline $lastline_no
            $Package $Prefix @line @BootCode %args_match %defaults %var_types %arg_list @proto_arg
            $processing_arg_with_types %argtype_seen @outlist %in_out %lengthof
            $proto_in_this_xsub $scope_in_this_xsub $interface $prepush_done $interface_macro $interface_macro_set
            $ProtoThisXSUB $ScopeThisXSUB $xsreturn
            @line_no $ret_type $func_header $orig_args
	   ); # Add these just to get compilation to happen.


sub process_file {
  
  # Allow for $package->process_file(%hash) in the future
  my ($pkg, %args) = @_ % 2 ? @_ : (__PACKAGE__, @_);
  
  $ProtoUsed = exists $args{prototypes};
  
  # Set defaults.
  %args = (
	   # 'C++' => 0, # Doesn't seem to *do* anything...
	   hiertype => 0,
	   except => 0,
	   prototypes => 0,
	   versioncheck => 1,
	   linenumbers => 1,
	   optimize => 1,
	   prototypes => 0,
	   inout => 1,
	   argtypes => 1,
	   typemap => [],
	   output => \*STDOUT,
	   csuffix => '.c',
	   %args,
	  );

  # Global Constants
  
  my ($Is_VMS, $SymSet);
  if ($^O eq 'VMS') {
    $Is_VMS = 1;
    # Establish set of global symbols with max length 28, since xsubpp
    # will later add the 'XS_' prefix.
    require ExtUtils::XSSymSet;
    $SymSet = new ExtUtils::XSSymSet 28;
  }
  @XSStack = ({type => 'none'});
  ($XSS_work_idx, $cpp_next_tmp) = (0, "XSubPPtmpAAAA");
  @InitFileCode = ();
  $FH = Symbol::gensym();
  $proto_re = "[" . quotemeta('\$%&*@;[]') . "]" ;
  $Overload = 0;
  $errors = 0;
  $Fallback = '&PL_sv_undef';

  # Most of the 1500 lines below uses these globals.  We'll have to
  # clean this up sometime, probably.  For now, we just pull them out
  # of %args.  -Ken
  
  $cplusplus = $args{'C++'};
  $hiertype = $args{hiertype};
  $WantPrototypes = $args{prototypes};
  $WantVersionChk = $args{versioncheck};
  $except = $args{except} ? ' TRY' : '';
  $WantLineNumbers = $args{linenumbers};
  $WantOptimize = $args{optimize};
  $process_inout = $args{inout};
  $process_argtypes = $args{argtypes};
  @tm = ref $args{typemap} ? @{$args{typemap}} : ($args{typemap});
  
  for ($args{filename}) {
    die "Missing required parameter 'filename'" unless $_;
    $filepathname = $_;
    ($dir, $filename) = (dirname($_), basename($_));
    $filepathname =~ s/\\/\\\\/g;
    $IncludedFiles{$_}++;
  }
  
  # Open the input file
  open($FH, $args{filename}) or die "cannot open $args{filename}: $!\n";

  # Open the output file if given as a string.  If they provide some
  # other kind of reference, trust them that we can print to it.
  if (not ref $args{output}) {
    open my($fh), "> $args{output}" or die "Can't create $args{output}: $!";
    $args{outfile} = $args{output};
    $args{output} = $fh;
  }

  # Really, we shouldn't have to chdir() or select() in the first
  # place.  For now, just save & restore.
  my $orig_cwd = cwd();
  my $orig_fh = select();
  
  chdir($dir);
  my $pwd = cwd();
  my $csuffix = $args{csuffix};
  
  if ($WantLineNumbers) {
    my $cfile;
    if ( $args{outfile} ) {
      $cfile = $args{outfile};
    } else {
      $cfile = $args{filename};
      $cfile =~ s/\.xs$/$csuffix/i or $cfile .= $csuffix;
    }
    tie(*PSEUDO_STDOUT, 'ExtUtils::ParseXS::CountLines', $cfile, $args{output});
    select PSEUDO_STDOUT;
  } else {
    select $args{output};
  }

  foreach my $typemap (@tm) {
    die "Can't find $typemap in $pwd\n" unless -r $typemap;
  }

  push @tm, standard_typemap_locations();

  foreach my $typemap (@tm) {
    next unless -f $typemap ;
    # skip directories, binary files etc.
    warn("Warning: ignoring non-text typemap file '$typemap'\n"), next
      unless -T $typemap ;
    open(TYPEMAP, $typemap)
      or warn ("Warning: could not open typemap file '$typemap': $!\n"), next;
    my $mode = 'Typemap';
    my $junk = "" ;
    my $current = \$junk;
    while (<TYPEMAP>) {
      next if /^\s*		#/;
        my $line_no = $. + 1;
      if (/^INPUT\s*$/) {
	$mode = 'Input';   $current = \$junk;  next;
      }
      if (/^OUTPUT\s*$/) {
	$mode = 'Output';  $current = \$junk;  next;
      }
      if (/^TYPEMAP\s*$/) {
	$mode = 'Typemap'; $current = \$junk;  next;
      }
      if ($mode eq 'Typemap') {
	chomp;
	my $line = $_ ;
	TrimWhitespace($_) ;
	# skip blank lines and comment lines
	next if /^$/ or /^#/ ;
	my($type,$kind, $proto) = /^\s*(.*?\S)\s+(\S+)\s*($proto_re*)\s*$/ or
	  warn("Warning: File '$typemap' Line $. '$line' TYPEMAP entry needs 2 or 3 columns\n"), next;
	$type = TidyType($type) ;
	$type_kind{$type} = $kind ;
	# prototype defaults to '$'
	$proto = "\$" unless $proto ;
	warn("Warning: File '$typemap' Line $. '$line' Invalid prototype '$proto'\n")
	  unless ValidProtoString($proto) ;
	$proto_letter{$type} = C_string($proto) ;
      } elsif (/^\s/) {
	$$current .= $_;
      } elsif ($mode eq 'Input') {
	s/\s+$//;
	$input_expr{$_} = '';
	$current = \$input_expr{$_};
      } else {
	s/\s+$//;
	$output_expr{$_} = '';
	$current = \$output_expr{$_};
      }
    }
    close(TYPEMAP);
  }

  foreach my $value (values %input_expr) {
    $value =~ s/;*\s+\z//;
    # Move C pre-processor instructions to column 1 to be strictly ANSI
    # conformant. Some pre-processors are fussy about this.
    $value =~ s/^\s+#/#/mg;
  }
  foreach my $value (values %output_expr) {
    # And again.
    $value =~ s/^\s+#/#/mg;
  }

  my ($cast, $size);
  our $bal = qr[(?:(?>[^()]+)|\((??{ $bal })\))*]; # ()-balanced
  $cast = qr[(?:\(\s*SV\s*\*\s*\)\s*)?]; # Optional (SV*) cast
  $size = qr[,\s* (??{ $bal }) ]x; # Third arg (to setpvn)

  foreach my $key (keys %output_expr) {
    BEGIN { $^H |= 0x00200000 }; # Equivalent to: use re 'eval', but hardcoded so we can compile re.xs

    my ($t, $with_size, $arg, $sarg) =
      ($output_expr{$key} =~
       m[^ \s+ sv_set ( [iunp] ) v (n)?	# Type, is_setpvn
	 \s* \( \s* $cast \$arg \s* ,
	 \s* ( (??{ $bal }) )	# Set from
	 ( (??{ $size }) )?	# Possible sizeof set-from
	 \) \s* ; \s* $
	]x);
    $targetable{$key} = [$t, $with_size, $arg, $sarg] if $t;
  }

  my $END = "!End!\n\n";		# "impossible" keyword (multiple newline)

  # Match an XS keyword
  $BLOCK_re= '\s*(' . join('|', qw(
				   REQUIRE BOOT CASE PREINIT INPUT INIT CODE PPCODE OUTPUT
				   CLEANUP ALIAS ATTRS PROTOTYPES PROTOTYPE VERSIONCHECK INCLUDE
				   SCOPE INTERFACE INTERFACE_MACRO C_ARGS POSTCALL OVERLOAD FALLBACK
				  )) . "|$END)\\s*:";

  
  our ($C_group_rex, $C_arg);
  # Group in C (no support for comments or literals)
  $C_group_rex = qr/ [({\[]
		       (?: (?> [^()\[\]{}]+ ) | (??{ $C_group_rex }) )*
		       [)}\]] /x ;
  # Chunk in C without comma at toplevel (no comments):
  $C_arg = qr/ (?: (?> [^()\[\]{},"']+ )
	     |   (??{ $C_group_rex })
	     |   " (?: (?> [^\\"]+ )
		   |   \\.
		   )* "		# String literal
			    |   ' (?: (?> [^\\']+ ) | \\. )* ' # Char literal
	     )* /xs;
  
  # Identify the version of xsubpp used
  print <<EOM ;
/*
 * This file was generated automatically by ExtUtils::ParseXS version $VERSION from the
 * contents of $filename. Do not edit this file, edit $filename instead.
 *
 *	ANY CHANGES MADE HERE WILL BE LOST! 
 *
 */

EOM


  print("#line 1 \"$filepathname\"\n")
    if $WantLineNumbers;

  firstmodule:
  while (<$FH>) {
    if (/^=/) {
      my $podstartline = $.;
      do {
	if (/^=cut\s*$/) {
	  # We can't just write out a /* */ comment, as our embedded
	  # POD might itself be in a comment. We can't put a /**/
	  # comment inside #if 0, as the C standard says that the source
	  # file is decomposed into preprocessing characters in the stage
	  # before preprocessing commands are executed.
	  # I don't want to leave the text as barewords, because the spec
	  # isn't clear whether macros are expanded before or after
	  # preprocessing commands are executed, and someone pathological
	  # may just have defined one of the 3 words as a macro that does
	  # something strange. Multiline strings are illegal in C, so
	  # the "" we write must be a string literal. And they aren't
	  # concatenated until 2 steps later, so we are safe.
	  #     - Nicholas Clark
	  print("#if 0\n  \"Skipped embedded POD.\"\n#endif\n");
	  printf("#line %d \"$filepathname\"\n", $. + 1)
	    if $WantLineNumbers;
	  next firstmodule
	}
	
      } while (<$FH>);
      # At this point $. is at end of file so die won't state the start
      # of the problem, and as we haven't yet read any lines &death won't
      # show the correct line in the message either.
      die ("Error: Unterminated pod in $filename, line $podstartline\n")
	unless $lastline;
    }
    last if ($Package, $Prefix) =
      /^MODULE\s*=\s*[\w:]+(?:\s+PACKAGE\s*=\s*([\w:]+))?(?:\s+PREFIX\s*=\s*(\S+))?\s*$/;
    
    print $_;
  }
  unless (defined $_) {
    warn "Didn't find a 'MODULE ... PACKAGE ... PREFIX' line\n";
    exit 0; # Not a fatal error for the caller process
  }

    print <<"EOF";
#ifndef PERL_UNUSED_VAR
#  define PERL_UNUSED_VAR(var) if (0) var = var
#endif

EOF

  print 'ExtUtils::ParseXS::CountLines'->end_marker, "\n" if $WantLineNumbers;

  $lastline    = $_;
  $lastline_no = $.;

 PARAGRAPH:
  while (fetch_para()) {
    # Print initial preprocessor statements and blank lines
    while (@line && $line[0] !~ /^[^\#]/) {
      my $line = shift(@line);
      print $line, "\n";
      next unless $line =~ /^\#\s*((if)(?:n?def)?|elsif|else|endif)\b/;
      my $statement = $+;
      if ($statement eq 'if') {
	$XSS_work_idx = @XSStack;
	push(@XSStack, {type => 'if'});
      } else {
	death ("Error: `$statement' with no matching `if'")
	  if $XSStack[-1]{type} ne 'if';
	if ($XSStack[-1]{varname}) {
	  push(@InitFileCode, "#endif\n");
	  push(@BootCode,     "#endif");
	}
	
	my(@fns) = keys %{$XSStack[-1]{functions}};
	if ($statement ne 'endif') {
	  # Hide the functions defined in other #if branches, and reset.
	  @{$XSStack[-1]{other_functions}}{@fns} = (1) x @fns;
	  @{$XSStack[-1]}{qw(varname functions)} = ('', {});
	} else {
	  my($tmp) = pop(@XSStack);
	  0 while (--$XSS_work_idx
		   && $XSStack[$XSS_work_idx]{type} ne 'if');
	  # Keep all new defined functions
	  push(@fns, keys %{$tmp->{other_functions}});
	  @{$XSStack[$XSS_work_idx]{functions}}{@fns} = (1) x @fns;
	}
      }
    }
    
    next PARAGRAPH unless @line;
    
    if ($XSS_work_idx && !$XSStack[$XSS_work_idx]{varname}) {
      # We are inside an #if, but have not yet #defined its xsubpp variable.
      print "#define $cpp_next_tmp 1\n\n";
      push(@InitFileCode, "#if $cpp_next_tmp\n");
      push(@BootCode,     "#if $cpp_next_tmp");
      $XSStack[$XSS_work_idx]{varname} = $cpp_next_tmp++;
    }

    death ("Code is not inside a function"
	   ." (maybe last function was ended by a blank line "
	   ." followed by a statement on column one?)")
      if $line[0] =~ /^\s/;
    
    my ($class, $externC, $static, $ellipsis, $wantRETVAL, $RETVAL_no_return);
    my (@fake_INPUT_pre);	# For length(s) generated variables
    my (@fake_INPUT);
    
    # initialize info arrays
    undef(%args_match);
    undef(%var_types);
    undef(%defaults);
    undef(%arg_list) ;
    undef(@proto_arg) ;
    undef($processing_arg_with_types) ;
    undef(%argtype_seen) ;
    undef(@outlist) ;
    undef(%in_out) ;
    undef(%lengthof) ;
    undef($proto_in_this_xsub) ;
    undef($scope_in_this_xsub) ;
    undef($interface);
    undef($prepush_done);
    $interface_macro = 'XSINTERFACE_FUNC' ;
    $interface_macro_set = 'XSINTERFACE_FUNC_SET' ;
    $ProtoThisXSUB = $WantPrototypes ;
    $ScopeThisXSUB = 0;
    $xsreturn = 0;

    $_ = shift(@line);
    while (my $kwd = check_keyword("REQUIRE|PROTOTYPES|FALLBACK|VERSIONCHECK|INCLUDE")) {
      &{"${kwd}_handler"}() ;
      next PARAGRAPH unless @line ;
      $_ = shift(@line);
    }

    if (check_keyword("BOOT")) {
      &check_cpp;
      push (@BootCode, "#line $line_no[@line_no - @line] \"$filepathname\"")
	if $WantLineNumbers && $line[0] !~ /^\s*#\s*line\b/;
      push (@BootCode, @line, "") ;
      next PARAGRAPH ;
    }


    # extract return type, function name and arguments
    ($ret_type) = TidyType($_);
    $RETVAL_no_return = 1 if $ret_type =~ s/^NO_OUTPUT\s+//;

    # Allow one-line ANSI-like declaration
    unshift @line, $2
      if $process_argtypes
	and $ret_type =~ s/^(.*?\w.*?)\s*\b(\w+\s*\(.*)/$1/s;

    # a function definition needs at least 2 lines
    blurt ("Error: Function definition too short '$ret_type'"), next PARAGRAPH
      unless @line ;

    $externC = 1 if $ret_type =~ s/^extern "C"\s+//;
    $static  = 1 if $ret_type =~ s/^static\s+//;

    $func_header = shift(@line);
    blurt ("Error: Cannot parse function definition from '$func_header'"), next PARAGRAPH
      unless $func_header =~ /^(?:([\w:]*)::)?(\w+)\s*\(\s*(.*?)\s*\)\s*(const)?\s*(;\s*)?$/s;

    ($class, $func_name, $orig_args) =  ($1, $2, $3) ;
    $class = "$4 $class" if $4;
    ($pname = $func_name) =~ s/^($Prefix)?/$Packprefix/;
    ($clean_func_name = $func_name) =~ s/^$Prefix//;
    $Full_func_name = "${Packid}_$clean_func_name";
    if ($Is_VMS) {
      $Full_func_name = $SymSet->addsym($Full_func_name);
    }

    # Check for duplicate function definition
    for my $tmp (@XSStack) {
      next unless defined $tmp->{functions}{$Full_func_name};
      Warn("Warning: duplicate function definition '$clean_func_name' detected");
      last;
    }
    $XSStack[$XSS_work_idx]{functions}{$Full_func_name} ++ ;
    %XsubAliases = %XsubAliasValues = %Interfaces = @Attributes = ();
    $DoSetMagic = 1;

    $orig_args =~ s/\\\s*/ /g;	# process line continuations
    my @args;

    my %only_C_inlist;		# Not in the signature of Perl function
    if ($process_argtypes and $orig_args =~ /\S/) {
      my $args = "$orig_args ,";
      if ($args =~ /^( (??{ $C_arg }) , )* $ /x) {
	@args = ($args =~ /\G ( (??{ $C_arg }) ) , /xg);
	for ( @args ) {
	  s/^\s+//;
	  s/\s+$//;
	  my ($arg, $default) = / ( [^=]* ) ( (?: = .* )? ) /x;
	  my ($pre, $name) = ($arg =~ /(.*?) \s*
					     \b ( \w+ | length\( \s*\w+\s* \) )
					     \s* $ /x);
	  next unless defined($pre) && length($pre);
	  my $out_type = '';
	  my $inout_var;
	  if ($process_inout and s/^(IN|IN_OUTLIST|OUTLIST|OUT|IN_OUT)\s+//) {
	    my $type = $1;
	    $out_type = $type if $type ne 'IN';
	    $arg =~ s/^(IN|IN_OUTLIST|OUTLIST|OUT|IN_OUT)\s+//;
	    $pre =~ s/^(IN|IN_OUTLIST|OUTLIST|OUT|IN_OUT)\s+//;
	  }
	  my $islength;
	  if ($name =~ /^length\( \s* (\w+) \s* \)\z/x) {
	    $name = "XSauto_length_of_$1";
	    $islength = 1;
	    die "Default value on length() argument: `$_'"
	      if length $default;
	  }
	  if (length $pre or $islength) { # Has a type
	    if ($islength) {
	      push @fake_INPUT_pre, $arg;
	    } else {
	      push @fake_INPUT, $arg;
	    }
	    # warn "pushing '$arg'\n";
	    $argtype_seen{$name}++;
	    $_ = "$name$default"; # Assigns to @args
	  }
	  $only_C_inlist{$_} = 1 if $out_type eq "OUTLIST" or $islength;
	  push @outlist, $name if $out_type =~ /OUTLIST$/;
	  $in_out{$name} = $out_type if $out_type;
	}
      } else {
	@args = split(/\s*,\s*/, $orig_args);
	Warn("Warning: cannot parse argument list '$orig_args', fallback to split");
      }
    } else {
      @args = split(/\s*,\s*/, $orig_args);
      for (@args) {
	if ($process_inout and s/^(IN|IN_OUTLIST|OUTLIST|IN_OUT|OUT)\s+//) {
	  my $out_type = $1;
	  next if $out_type eq 'IN';
	  $only_C_inlist{$_} = 1 if $out_type eq "OUTLIST";
	  push @outlist, $name if $out_type =~ /OUTLIST$/;
	  $in_out{$_} = $out_type;
	}
      }
    }
    if (defined($class)) {
      my $arg0 = ((defined($static) or $func_name eq 'new')
		  ? "CLASS" : "THIS");
      unshift(@args, $arg0);
      ($report_args = "$arg0, $report_args") =~ s/^\w+, $/$arg0/;
    }
    my $extra_args = 0;
    @args_num = ();
    $num_args = 0;
    my $report_args = '';
    foreach my $i (0 .. $#args) {
      if ($args[$i] =~ s/\.\.\.//) {
	$ellipsis = 1;
	if ($args[$i] eq '' && $i == $#args) {
	  $report_args .= ", ...";
	  pop(@args);
	  last;
	}
      }
      if ($only_C_inlist{$args[$i]}) {
	push @args_num, undef;
      } else {
	push @args_num, ++$num_args;
	$report_args .= ", $args[$i]";
      }
      if ($args[$i] =~ /^([^=]*[^\s=])\s*=\s*(.*)/s) {
	$extra_args++;
	$args[$i] = $1;
	$defaults{$args[$i]} = $2;
	$defaults{$args[$i]} =~ s/"/\\"/g;
      }
      $proto_arg[$i+1] = '$' ;
    }
    $min_args = $num_args - $extra_args;
    $report_args =~ s/"/\\"/g;
    $report_args =~ s/^,\s+//;
    my @func_args = @args;
    shift @func_args if defined($class);

    for (@func_args) {
      s/^/&/ if $in_out{$_};
    }
    $func_args = join(", ", @func_args);
    @args_match{@args} = @args_num;

    $PPCODE = grep(/^\s*PPCODE\s*:/, @line);
    $CODE = grep(/^\s*CODE\s*:/, @line);
    # Detect CODE: blocks which use ST(n)= or XST_m*(n,v)
    #   to set explicit return values.
    $EXPLICIT_RETURN = ($CODE &&
			("@line" =~ /(\bST\s*\([^;]*=) | (\bXST_m\w+\s*\()/x ));
    $ALIAS  = grep(/^\s*ALIAS\s*:/,  @line);
    $INTERFACE  = grep(/^\s*INTERFACE\s*:/,  @line);

    $xsreturn = 1 if $EXPLICIT_RETURN;

    $externC = $externC ? qq[extern "C"] : "";

    # print function header
    print Q(<<"EOF");
#$externC
#XS(XS_${Full_func_name}); /* prototype to pass -Wmissing-prototypes */
#XS(XS_${Full_func_name})
#[[
##ifdef dVAR
#    dVAR; dXSARGS;
##else
#    dXSARGS;
##endif
EOF
    print Q(<<"EOF") if $ALIAS ;
#    dXSI32;
EOF
    print Q(<<"EOF") if $INTERFACE ;
#    dXSFUNCTION($ret_type);
EOF
    if ($ellipsis) {
      $cond = ($min_args ? qq(items < $min_args) : 0);
    } elsif ($min_args == $num_args) {
      $cond = qq(items != $min_args);
    } else {
      $cond = qq(items < $min_args || items > $num_args);
    }

    print Q(<<"EOF") if $except;
#    char errbuf[1024];
#    *errbuf = '\0';
EOF

    if ($ALIAS)
      { print Q(<<"EOF") if $cond }
#    if ($cond)
#       Perl_croak(aTHX_ "Usage: %s(%s)", GvNAME(CvGV(cv)), "$report_args");
EOF
    else
      { print Q(<<"EOF") if $cond }
#    if ($cond)
#       Perl_croak(aTHX_ "Usage: %s(%s)", "$pname", "$report_args");
EOF
    
     # cv doesn't seem to be used, in most cases unless we go in 
     # the if of this else
     print Q(<<"EOF");
#    PERL_UNUSED_VAR(cv); /* -W */
EOF

    #gcc -Wall: if an xsub has PPCODE is used
    #it is possible none of ST, XSRETURN or XSprePUSH macros are used
    #hence `ax' (setup by dXSARGS) is unused
    #XXX: could breakup the dXSARGS; into dSP;dMARK;dITEMS
    #but such a move could break third-party extensions
    print Q(<<"EOF") if $PPCODE;
#    PERL_UNUSED_VAR(ax); /* -Wall */
EOF

    print Q(<<"EOF") if $PPCODE;
#    SP -= items;
EOF

    # Now do a block of some sort.

    $condnum = 0;
    $cond = '';			# last CASE: condidional
    push(@line, "$END:");
    push(@line_no, $line_no[-1]);
    $_ = '';
    &check_cpp;
    while (@line) {
      &CASE_handler if check_keyword("CASE");
      print Q(<<"EOF");
#   $except [[
EOF

      # do initialization of input variables
      $thisdone = 0;
      $retvaldone = 0;
      $deferred = "";
      %arg_list = () ;
      $gotRETVAL = 0;
	
      INPUT_handler() ;
      process_keyword("INPUT|PREINIT|INTERFACE_MACRO|C_ARGS|ALIAS|ATTRS|PROTOTYPE|SCOPE|OVERLOAD") ;

      print Q(<<"EOF") if $ScopeThisXSUB;
#   ENTER;
#   [[
EOF
	
      if (!$thisdone && defined($class)) {
	if (defined($static) or $func_name eq 'new') {
	  print "\tchar *";
	  $var_types{"CLASS"} = "char *";
	  &generate_init("char *", 1, "CLASS");
	}
	else {
	  print "\t$class *";
	  $var_types{"THIS"} = "$class *";
	  &generate_init("$class *", 1, "THIS");
	}
      }
      
      # do code
      if (/^\s*NOT_IMPLEMENTED_YET/) {
	print "\n\tPerl_croak(aTHX_ \"$pname: not implemented yet\");\n";
	$_ = '' ;
      } else {
	if ($ret_type ne "void") {
	  print "\t" . &map_type($ret_type, 'RETVAL') . ";\n"
	    if !$retvaldone;
	  $args_match{"RETVAL"} = 0;
	  $var_types{"RETVAL"} = $ret_type;
	  print "\tdXSTARG;\n"
	    if $WantOptimize and $targetable{$type_kind{$ret_type}};
	}
	
	if (@fake_INPUT or @fake_INPUT_pre) {
	  unshift @line, @fake_INPUT_pre, @fake_INPUT, $_;
	  $_ = "";
	  $processing_arg_with_types = 1;
	  INPUT_handler() ;
	}
	print $deferred;
	
        process_keyword("INIT|ALIAS|ATTRS|PROTOTYPE|INTERFACE_MACRO|INTERFACE|C_ARGS|OVERLOAD") ;
	
	if (check_keyword("PPCODE")) {
	  print_section();
	  death ("PPCODE must be last thing") if @line;
	  print "\tLEAVE;\n" if $ScopeThisXSUB;
	  print "\tPUTBACK;\n\treturn;\n";
	} elsif (check_keyword("CODE")) {
	  print_section() ;
	} elsif (defined($class) and $func_name eq "DESTROY") {
	  print "\n\t";
	  print "delete THIS;\n";
	} else {
	  print "\n\t";
	  if ($ret_type ne "void") {
	    print "RETVAL = ";
	    $wantRETVAL = 1;
	  }
	  if (defined($static)) {
	    if ($func_name eq 'new') {
	      $func_name = "$class";
	    } else {
	      print "${class}::";
	    }
	  } elsif (defined($class)) {
	    if ($func_name eq 'new') {
	      $func_name .= " $class";
	    } else {
	      print "THIS->";
	    }
	  }
	  $func_name =~ s/^\Q$args{'s'}//
	    if exists $args{'s'};
	  $func_name = 'XSFUNCTION' if $interface;
	  print "$func_name($func_args);\n";
	}
      }
      
      # do output variables
      $gotRETVAL = 0;		# 1 if RETVAL seen in OUTPUT section;
      undef $RETVAL_code ;	# code to set RETVAL (from OUTPUT section);
      # $wantRETVAL set if 'RETVAL =' autogenerated
      ($wantRETVAL, $ret_type) = (0, 'void') if $RETVAL_no_return;
      undef %outargs ;
      process_keyword("POSTCALL|OUTPUT|ALIAS|ATTRS|PROTOTYPE|OVERLOAD");
      
      &generate_output($var_types{$_}, $args_match{$_}, $_, $DoSetMagic)
	for grep $in_out{$_} =~ /OUT$/, keys %in_out;
      
      # all OUTPUT done, so now push the return value on the stack
      if ($gotRETVAL && $RETVAL_code) {
	print "\t$RETVAL_code\n";
      } elsif ($gotRETVAL || $wantRETVAL) {
	my $t = $WantOptimize && $targetable{$type_kind{$ret_type}};
	my $var = 'RETVAL';
	my $type = $ret_type;
	
	# 0: type, 1: with_size, 2: how, 3: how_size
	if ($t and not $t->[1] and $t->[0] eq 'p') {
	  # PUSHp corresponds to setpvn.  Treate setpv directly
	  my $what = eval qq("$t->[2]");
	  warn $@ if $@;
	  
	  print "\tsv_setpv(TARG, $what); XSprePUSH; PUSHTARG;\n";
	  $prepush_done = 1;
	}
	elsif ($t) {
	  my $what = eval qq("$t->[2]");
	  warn $@ if $@;
	  
	  my $size = $t->[3];
	  $size = '' unless defined $size;
	  $size = eval qq("$size");
	  warn $@ if $@;
	  print "\tXSprePUSH; PUSH$t->[0]($what$size);\n";
	  $prepush_done = 1;
	}
	else {
	  # RETVAL almost never needs SvSETMAGIC()
	  &generate_output($ret_type, 0, 'RETVAL', 0);
	}
      }
      
      $xsreturn = 1 if $ret_type ne "void";
      my $num = $xsreturn;
      my $c = @outlist;
      print "\tXSprePUSH;" if $c and not $prepush_done;
      print "\tEXTEND(SP,$c);\n" if $c;
      $xsreturn += $c;
      generate_output($var_types{$_}, $num++, $_, 0, 1) for @outlist;
      
      # do cleanup
      process_keyword("CLEANUP|ALIAS|ATTRS|PROTOTYPE|OVERLOAD") ;
      
      print Q(<<"EOF") if $ScopeThisXSUB;
#   ]]
EOF
      print Q(<<"EOF") if $ScopeThisXSUB and not $PPCODE;
#   LEAVE;
EOF
      
      # print function trailer
      print Q(<<"EOF");
#    ]]
EOF
      print Q(<<"EOF") if $except;
#    BEGHANDLERS
#    CATCHALL
#	sprintf(errbuf, "%s: %s\\tpropagated", Xname, Xreason);
#    ENDHANDLERS
EOF
      if (check_keyword("CASE")) {
	blurt ("Error: No `CASE:' at top of function")
	  unless $condnum;
	$_ = "CASE: $_";	# Restore CASE: label
	next;
      }
      last if $_ eq "$END:";
      death(/^$BLOCK_re/o ? "Misplaced `$1:'" : "Junk at end of function");
    }
    
    print Q(<<"EOF") if $except;
#    if (errbuf[0])
#	Perl_croak(aTHX_ errbuf);
EOF
    
    if ($xsreturn) {
      print Q(<<"EOF") unless $PPCODE;
#    XSRETURN($xsreturn);
EOF
    } else {
      print Q(<<"EOF") unless $PPCODE;
#    XSRETURN_EMPTY;
EOF
    }

    print Q(<<"EOF");
#]]
#
EOF

    my $newXS = "newXS" ;
    my $proto = "" ;
    
    # Build the prototype string for the xsub
    if ($ProtoThisXSUB) {
      $newXS = "newXSproto";
      
      if ($ProtoThisXSUB eq 2) {
	# User has specified empty prototype
      }
      elsif ($ProtoThisXSUB eq 1) {
	my $s = ';';
	if ($min_args < $num_args)  {
	  $s = '';
	  $proto_arg[$min_args] .= ";" ;
	}
	push @proto_arg, "$s\@"
	  if $ellipsis ;
	
	$proto = join ("", grep defined, @proto_arg);
      }
      else {
	# User has specified a prototype
	$proto = $ProtoThisXSUB;
      }
      $proto = qq{, "$proto"};
    }
    
    if (%XsubAliases) {
      $XsubAliases{$pname} = 0
	unless defined $XsubAliases{$pname} ;
      while ( ($name, $value) = each %XsubAliases) {
	push(@InitFileCode, Q(<<"EOF"));
#        cv = newXS(\"$name\", XS_$Full_func_name, file);
#        XSANY.any_i32 = $value ;
EOF
	push(@InitFileCode, Q(<<"EOF")) if $proto;
#        sv_setpv((SV*)cv$proto) ;
EOF
      }
    }
    elsif (@Attributes) {
      push(@InitFileCode, Q(<<"EOF"));
#        cv = newXS(\"$pname\", XS_$Full_func_name, file);
#        apply_attrs_string("$Package", cv, "@Attributes", 0);
EOF
    }
    elsif ($interface) {
      while ( ($name, $value) = each %Interfaces) {
	$name = "$Package\::$name" unless $name =~ /::/;
	push(@InitFileCode, Q(<<"EOF"));
#        cv = newXS(\"$name\", XS_$Full_func_name, file);
#        $interface_macro_set(cv,$value) ;
EOF
	push(@InitFileCode, Q(<<"EOF")) if $proto;
#        sv_setpv((SV*)cv$proto) ;
EOF
      }
    }
    else {
      push(@InitFileCode,
	   "        ${newXS}(\"$pname\", XS_$Full_func_name, file$proto);\n");
    }
  }

  if ($Overload) # make it findable with fetchmethod
  {
    print Q(<<"EOF");
#XS(XS_${Packid}_nil); /* prototype to pass -Wmissing-prototypes */
#XS(XS_${Packid}_nil)
#{
#   dXSARGS;
#   XSRETURN_EMPTY;
#}
#
EOF
    unshift(@InitFileCode, <<"MAKE_FETCHMETHOD_WORK");
    /* Making a sub named "${Package}::()" allows the package */
    /* to be findable via fetchmethod(), and causes */
    /* overload::Overloaded("${Package}") to return true. */
    newXS("${Package}::()", XS_${Packid}_nil, file$proto);
MAKE_FETCHMETHOD_WORK
  }

  # print initialization routine

  print Q(<<"EOF");
##ifdef __cplusplus
#extern "C"
##endif
EOF

  print Q(<<"EOF");
#XS(boot_$Module_cname); /* prototype to pass -Wmissing-prototypes */
#XS(boot_$Module_cname)
EOF

  print Q(<<"EOF");
#[[
##ifdef dVAR
#    dVAR; dXSARGS;
##else
#    dXSARGS;
##endif
EOF

  #-Wall: if there is no $Full_func_name there are no xsubs in this .xs
  #so `file' is unused
  print Q(<<"EOF") if $Full_func_name;
#    char* file = __FILE__;
EOF

  print Q("#\n");

  print Q(<<"EOF");
#    PERL_UNUSED_VAR(cv); /* -W */
#    PERL_UNUSED_VAR(items); /* -W */
EOF
    
  print Q(<<"EOF") if $WantVersionChk ;
#    XS_VERSION_BOOTCHECK ;
#
EOF

  print Q(<<"EOF") if defined $XsubAliases or defined $Interfaces ;
#    {
#        CV * cv ;
#
EOF

  print Q(<<"EOF") if ($Overload);
#    /* register the overloading (type 'A') magic */
#    PL_amagic_generation++;
#    /* The magic for overload gets a GV* via gv_fetchmeth as */
#    /* mentioned above, and looks in the SV* slot of it for */
#    /* the "fallback" status. */
#    sv_setsv(
#        get_sv( "${Package}::()", TRUE ),
#        $Fallback
#    );
EOF

  print @InitFileCode;

  print Q(<<"EOF") if defined $XsubAliases or defined $Interfaces ;
#    }
EOF

  if (@BootCode)
  {
    print "\n    /* Initialisation Section */\n\n" ;
    @line = @BootCode;
    print_section();
    print "\n    /* End of Initialisation Section */\n\n" ;
  }

  if ($] >= 5.009) {
    print <<'EOF';
    if (PL_unitcheckav)
         call_list(PL_scopestack_ix, PL_unitcheckav);
EOF
  }

  print Q(<<"EOF");
#    XSRETURN_YES;
#]]
#
EOF

  warn("Please specify prototyping behavior for $filename (see perlxs manual)\n")
    unless $ProtoUsed ;

  chdir($orig_cwd);
  select($orig_fh);
  untie *PSEUDO_STDOUT if tied *PSEUDO_STDOUT;
  close $FH;

  return 1;
}

sub errors { $errors }

sub standard_typemap_locations {
  # Add all the default typemap locations to the search path
  my @tm = qw(typemap);
  
  my $updir = File::Spec->updir;
  foreach my $dir (File::Spec->catdir(($updir) x 1), File::Spec->catdir(($updir) x 2),
		   File::Spec->catdir(($updir) x 3), File::Spec->catdir(($updir) x 4)) {
    
    unshift @tm, File::Spec->catfile($dir, 'typemap');
    unshift @tm, File::Spec->catfile($dir, lib => ExtUtils => 'typemap');
  }
  foreach my $dir (@INC) {
    my $file = File::Spec->catfile($dir, ExtUtils => 'typemap');
    unshift @tm, $file if -e $file;
  }
  return @tm;
}
  
sub TrimWhitespace
{
  $_[0] =~ s/^\s+|\s+$//go ;
}

sub TidyType
  {
    local ($_) = @_ ;

    # rationalise any '*' by joining them into bunches and removing whitespace
    s#\s*(\*+)\s*#$1#g;
    s#(\*+)# $1 #g ;

    # change multiple whitespace into a single space
    s/\s+/ /g ;

    # trim leading & trailing whitespace
    TrimWhitespace($_) ;

    $_ ;
}

# Input:  ($_, @line) == unparsed input.
# Output: ($_, @line) == (rest of line, following lines).
# Return: the matched keyword if found, otherwise 0
sub check_keyword {
	$_ = shift(@line) while !/\S/ && @line;
	s/^(\s*)($_[0])\s*:\s*(?:#.*)?/$1/s && $2;
}

sub print_section {
    # the "do" is required for right semantics
    do { $_ = shift(@line) } while !/\S/ && @line;

    print("#line ", $line_no[@line_no - @line -1], " \"$filepathname\"\n")
	if $WantLineNumbers && !/^\s*#\s*line\b/ && !/^#if XSubPPtmp/;
    for (;  defined($_) && !/^$BLOCK_re/o;  $_ = shift(@line)) {
	print "$_\n";
    }
    print 'ExtUtils::ParseXS::CountLines'->end_marker, "\n" if $WantLineNumbers;
}

sub merge_section {
    my $in = '';

    while (!/\S/ && @line) {
      $_ = shift(@line);
    }

    for (;  defined($_) && !/^$BLOCK_re/o;  $_ = shift(@line)) {
      $in .= "$_\n";
    }
    chomp $in;
    return $in;
  }

sub process_keyword($)
  {
    my($pattern) = @_ ;
    my $kwd ;

    &{"${kwd}_handler"}()
      while $kwd = check_keyword($pattern) ;
  }

sub CASE_handler {
  blurt ("Error: `CASE:' after unconditional `CASE:'")
    if $condnum && $cond eq '';
  $cond = $_;
  TrimWhitespace($cond);
  print "   ", ($condnum++ ? " else" : ""), ($cond ? " if ($cond)\n" : "\n");
  $_ = '' ;
}

sub INPUT_handler {
  for (;  !/^$BLOCK_re/o;  $_ = shift(@line)) {
    last if /^\s*NOT_IMPLEMENTED_YET/;
    next unless /\S/;		# skip blank lines

    TrimWhitespace($_) ;
    my $line = $_ ;

    # remove trailing semicolon if no initialisation
    s/\s*;$//g unless /[=;+].*\S/ ;

    # Process the length(foo) declarations
    if (s/^([^=]*)\blength\(\s*(\w+)\s*\)\s*$/$1 XSauto_length_of_$2=NO_INIT/x) {
      print "\tSTRLEN\tSTRLEN_length_of_$2;\n";
      $lengthof{$2} = $name;
      # $islengthof{$name} = $1;
      $deferred .= "\n\tXSauto_length_of_$2 = STRLEN_length_of_$2;";
    }

    # check for optional initialisation code
    my $var_init = '' ;
    $var_init = $1 if s/\s*([=;+].*)$//s ;
    $var_init =~ s/"/\\"/g;

    s/\s+/ /g;
    my ($var_type, $var_addr, $var_name) = /^(.*?[^&\s])\s*(\&?)\s*\b(\w+)$/s
      or blurt("Error: invalid argument declaration '$line'"), next;

    # Check for duplicate definitions
    blurt ("Error: duplicate definition of argument '$var_name' ignored"), next
      if $arg_list{$var_name}++
	or defined $argtype_seen{$var_name} and not $processing_arg_with_types;

    $thisdone |= $var_name eq "THIS";
    $retvaldone |= $var_name eq "RETVAL";
    $var_types{$var_name} = $var_type;
    # XXXX This check is a safeguard against the unfinished conversion of
    # generate_init().  When generate_init() is fixed,
    # one can use 2-args map_type() unconditionally.
    if ($var_type =~ / \( \s* \* \s* \) /x) {
      # Function pointers are not yet supported with &output_init!
      print "\t" . &map_type($var_type, $var_name);
      $name_printed = 1;
    } else {
      print "\t" . &map_type($var_type);
      $name_printed = 0;
    }
    $var_num = $args_match{$var_name};

    $proto_arg[$var_num] = ProtoString($var_type)
      if $var_num ;
    $func_args =~ s/\b($var_name)\b/&$1/ if $var_addr;
    if ($var_init =~ /^[=;]\s*NO_INIT\s*;?\s*$/
	or $in_out{$var_name} and $in_out{$var_name} =~ /^OUT/
	and $var_init !~ /\S/) {
      if ($name_printed) {
	print ";\n";
      } else {
	print "\t$var_name;\n";
      }
    } elsif ($var_init =~ /\S/) {
      &output_init($var_type, $var_num, $var_name, $var_init, $name_printed);
    } elsif ($var_num) {
      # generate initialization code
      &generate_init($var_type, $var_num, $var_name, $name_printed);
    } else {
      print ";\n";
    }
  }
}

sub OUTPUT_handler {
  for (;  !/^$BLOCK_re/o;  $_ = shift(@line)) {
    next unless /\S/;
    if (/^\s*SETMAGIC\s*:\s*(ENABLE|DISABLE)\s*/) {
      $DoSetMagic = ($1 eq "ENABLE" ? 1 : 0);
      next;
    }
    my ($outarg, $outcode) = /^\s*(\S+)\s*(.*?)\s*$/s ;
    blurt ("Error: duplicate OUTPUT argument '$outarg' ignored"), next
      if $outargs{$outarg} ++ ;
    if (!$gotRETVAL and $outarg eq 'RETVAL') {
      # deal with RETVAL last
      $RETVAL_code = $outcode ;
      $gotRETVAL = 1 ;
      next ;
    }
    blurt ("Error: OUTPUT $outarg not an argument"), next
      unless defined($args_match{$outarg});
    blurt("Error: No input definition for OUTPUT argument '$outarg' - ignored"), next
      unless defined $var_types{$outarg} ;
    $var_num = $args_match{$outarg};
    if ($outcode) {
      print "\t$outcode\n";
      print "\tSvSETMAGIC(ST(" , $var_num-1 , "));\n" if $DoSetMagic;
    } else {
      &generate_output($var_types{$outarg}, $var_num, $outarg, $DoSetMagic);
    }
    delete $in_out{$outarg} 	# No need to auto-OUTPUT
      if exists $in_out{$outarg} and $in_out{$outarg} =~ /OUT$/;
  }
}

sub C_ARGS_handler() {
  my $in = merge_section();

  TrimWhitespace($in);
  $func_args = $in;
}

sub INTERFACE_MACRO_handler() {
  my $in = merge_section();

  TrimWhitespace($in);
  if ($in =~ /\s/) {		# two
    ($interface_macro, $interface_macro_set) = split ' ', $in;
  } else {
    $interface_macro = $in;
    $interface_macro_set = 'UNKNOWN_CVT'; # catch later
  }
  $interface = 1;		# local
  $Interfaces = 1;		# global
}

sub INTERFACE_handler() {
  my $in = merge_section();

  TrimWhitespace($in);

  foreach (split /[\s,]+/, $in) {
    my $name = $_;
    $name =~ s/^$Prefix//;
    $Interfaces{$name} = $_;
  }
  print Q(<<"EOF");
#	XSFUNCTION = $interface_macro($ret_type,cv,XSANY.any_dptr);
EOF
  $interface = 1;		# local
  $Interfaces = 1;		# global
}

sub CLEANUP_handler() { print_section() }
sub PREINIT_handler() { print_section() }
sub POSTCALL_handler() { print_section() }
sub INIT_handler()    { print_section() }

sub GetAliases
  {
    my ($line) = @_ ;
    my ($orig) = $line ;
    my ($alias) ;
    my ($value) ;

    # Parse alias definitions
    # format is
    #    alias = value alias = value ...

    while ($line =~ s/^\s*([\w:]+)\s*=\s*(\w+)\s*//) {
      $alias = $1 ;
      $orig_alias = $alias ;
      $value = $2 ;

      # check for optional package definition in the alias
      $alias = $Packprefix . $alias if $alias !~ /::/ ;

      # check for duplicate alias name & duplicate value
      Warn("Warning: Ignoring duplicate alias '$orig_alias'")
	if defined $XsubAliases{$alias} ;

      Warn("Warning: Aliases '$orig_alias' and '$XsubAliasValues{$value}' have identical values")
	if $XsubAliasValues{$value} ;

      $XsubAliases = 1;
      $XsubAliases{$alias} = $value ;
      $XsubAliasValues{$value} = $orig_alias ;
    }

    blurt("Error: Cannot parse ALIAS definitions from '$orig'")
      if $line ;
  }

sub ATTRS_handler ()
  {
    for (;  !/^$BLOCK_re/o;  $_ = shift(@line)) {
      next unless /\S/;
      TrimWhitespace($_) ;
      push @Attributes, $_;
    }
  }

sub ALIAS_handler ()
  {
    for (;  !/^$BLOCK_re/o;  $_ = shift(@line)) {
      next unless /\S/;
      TrimWhitespace($_) ;
      GetAliases($_) if $_ ;
    }
  }

sub OVERLOAD_handler()
{
  for (;  !/^$BLOCK_re/o;  $_ = shift(@line)) {
    next unless /\S/;
    TrimWhitespace($_) ;
    while ( s/^\s*([\w:"\\)\+\-\*\/\%\<\>\.\&\|\^\!\~\{\}\=]+)\s*//) {
      $Overload = 1 unless $Overload;
      my $overload = "$Package\::(".$1 ;
      push(@InitFileCode,
	   "        newXS(\"$overload\", XS_$Full_func_name, file$proto);\n");
    }
  }  
}

sub FALLBACK_handler()
{
  # the rest of the current line should contain either TRUE, 
  # FALSE or UNDEF
  
  TrimWhitespace($_) ;
  my %map = (
	     TRUE => "&PL_sv_yes", 1 => "&PL_sv_yes",
	     FALSE => "&PL_sv_no", 0 => "&PL_sv_no",
	     UNDEF => "&PL_sv_undef",
	    ) ;
  
  # check for valid FALLBACK value
  death ("Error: FALLBACK: TRUE/FALSE/UNDEF") unless exists $map{uc $_} ;
  
  $Fallback = $map{uc $_} ;
}


sub REQUIRE_handler ()
  {
    # the rest of the current line should contain a version number
    my ($Ver) = $_ ;

    TrimWhitespace($Ver) ;

    death ("Error: REQUIRE expects a version number")
      unless $Ver ;

    # check that the version number is of the form n.n
    death ("Error: REQUIRE: expected a number, got '$Ver'")
      unless $Ver =~ /^\d+(\.\d*)?/ ;

    death ("Error: xsubpp $Ver (or better) required--this is only $VERSION.")
      unless $VERSION >= $Ver ;
  }

sub VERSIONCHECK_handler ()
  {
    # the rest of the current line should contain either ENABLE or
    # DISABLE

    TrimWhitespace($_) ;

    # check for ENABLE/DISABLE
    death ("Error: VERSIONCHECK: ENABLE/DISABLE")
      unless /^(ENABLE|DISABLE)/i ;

    $WantVersionChk = 1 if $1 eq 'ENABLE' ;
    $WantVersionChk = 0 if $1 eq 'DISABLE' ;

  }

sub PROTOTYPE_handler ()
  {
    my $specified ;

    death("Error: Only 1 PROTOTYPE definition allowed per xsub")
      if $proto_in_this_xsub ++ ;

    for (;  !/^$BLOCK_re/o;  $_ = shift(@line)) {
      next unless /\S/;
      $specified = 1 ;
      TrimWhitespace($_) ;
      if ($_ eq 'DISABLE') {
	$ProtoThisXSUB = 0
      } elsif ($_ eq 'ENABLE') {
	$ProtoThisXSUB = 1
      } else {
	# remove any whitespace
	s/\s+//g ;
	death("Error: Invalid prototype '$_'")
	  unless ValidProtoString($_) ;
	$ProtoThisXSUB = C_string($_) ;
      }
    }

    # If no prototype specified, then assume empty prototype ""
    $ProtoThisXSUB = 2 unless $specified ;

    $ProtoUsed = 1 ;

  }

sub SCOPE_handler ()
  {
    death("Error: Only 1 SCOPE declaration allowed per xsub")
      if $scope_in_this_xsub ++ ;

    for (;  !/^$BLOCK_re/o;  $_ = shift(@line)) {
      next unless /\S/;
      TrimWhitespace($_) ;
      if ($_ =~ /^DISABLE/i) {
	$ScopeThisXSUB = 0
      } elsif ($_ =~ /^ENABLE/i) {
	$ScopeThisXSUB = 1
      }
    }

  }

sub PROTOTYPES_handler ()
  {
    # the rest of the current line should contain either ENABLE or
    # DISABLE

    TrimWhitespace($_) ;

    # check for ENABLE/DISABLE
    death ("Error: PROTOTYPES: ENABLE/DISABLE")
      unless /^(ENABLE|DISABLE)/i ;

    $WantPrototypes = 1 if $1 eq 'ENABLE' ;
    $WantPrototypes = 0 if $1 eq 'DISABLE' ;
    $ProtoUsed = 1 ;

  }

sub INCLUDE_handler ()
  {
    # the rest of the current line should contain a valid filename

    TrimWhitespace($_) ;

    death("INCLUDE: filename missing")
      unless $_ ;

    death("INCLUDE: output pipe is illegal")
      if /^\s*\|/ ;

    # simple minded recursion detector
    death("INCLUDE loop detected")
      if $IncludedFiles{$_} ;

    ++ $IncludedFiles{$_} unless /\|\s*$/ ;

    # Save the current file context.
    push(@XSStack, {
		    type		=> 'file',
		    LastLine        => $lastline,
		    LastLineNo      => $lastline_no,
		    Line            => \@line,
		    LineNo          => \@line_no,
		    Filename        => $filename,
		    Filepathname    => $filepathname,
		    Handle          => $FH,
		   }) ;

    $FH = Symbol::gensym();

    # open the new file
    open ($FH, "$_") or death("Cannot open '$_': $!") ;

    print Q(<<"EOF");
#
#/* INCLUDE:  Including '$_' from '$filename' */
#
EOF

    $filepathname = $filename = $_ ;

    # Prime the pump by reading the first
    # non-blank line

    # skip leading blank lines
    while (<$FH>) {
      last unless /^\s*$/ ;
    }

    $lastline = $_ ;
    $lastline_no = $. ;

  }

sub PopFile()
  {
    return 0 unless $XSStack[-1]{type} eq 'file' ;

    my $data     = pop @XSStack ;
    my $ThisFile = $filename ;
    my $isPipe   = ($filename =~ /\|\s*$/) ;

    -- $IncludedFiles{$filename}
      unless $isPipe ;

    close $FH ;

    $FH         = $data->{Handle} ;
    # $filename is the leafname, which for some reason isused for diagnostic
    # messages, whereas $filepathname is the full pathname, and is used for
    # #line directives.
    $filename   = $data->{Filename} ;
    $filepathname = $data->{Filepathname} ;
    $lastline   = $data->{LastLine} ;
    $lastline_no = $data->{LastLineNo} ;
    @line       = @{ $data->{Line} } ;
    @line_no    = @{ $data->{LineNo} } ;

    if ($isPipe and $? ) {
      -- $lastline_no ;
      print STDERR "Error reading from pipe '$ThisFile': $! in $filename, line $lastline_no\n"  ;
      exit 1 ;
    }

    print Q(<<"EOF");
#
#/* INCLUDE: Returning to '$filename' from '$ThisFile' */
#
EOF

    return 1 ;
  }

sub ValidProtoString ($)
  {
    my($string) = @_ ;

    if ( $string =~ /^$proto_re+$/ ) {
      return $string ;
    }

    return 0 ;
  }

sub C_string ($)
  {
    my($string) = @_ ;

    $string =~ s[\\][\\\\]g ;
    $string ;
  }

sub ProtoString ($)
  {
    my ($type) = @_ ;

    $proto_letter{$type} or "\$" ;
  }

sub check_cpp {
  my @cpp = grep(/^\#\s*(?:if|e\w+)/, @line);
  if (@cpp) {
    my ($cpp, $cpplevel);
    for $cpp (@cpp) {
      if ($cpp =~ /^\#\s*if/) {
	$cpplevel++;
      } elsif (!$cpplevel) {
	Warn("Warning: #else/elif/endif without #if in this function");
	print STDERR "    (precede it with a blank line if the matching #if is outside the function)\n"
	  if $XSStack[-1]{type} eq 'if';
	return;
      } elsif ($cpp =~ /^\#\s*endif/) {
	$cpplevel--;
      }
    }
    Warn("Warning: #if without #endif in this function") if $cpplevel;
  }
}


sub Q {
  my($text) = @_;
  $text =~ s/^#//gm;
  $text =~ s/\[\[/{/g;
  $text =~ s/\]\]/}/g;
  $text;
}

# Read next xsub into @line from ($lastline, <$FH>).
sub fetch_para {
  # parse paragraph
  death ("Error: Unterminated `#if/#ifdef/#ifndef'")
    if !defined $lastline && $XSStack[-1]{type} eq 'if';
  @line = ();
  @line_no = () ;
  return PopFile() if !defined $lastline;

  if ($lastline =~
      /^MODULE\s*=\s*([\w:]+)(?:\s+PACKAGE\s*=\s*([\w:]+))?(?:\s+PREFIX\s*=\s*(\S+))?\s*$/) {
    $Module = $1;
    $Package = defined($2) ? $2 : ''; # keep -w happy
    $Prefix  = defined($3) ? $3 : ''; # keep -w happy
    $Prefix = quotemeta $Prefix ;
    ($Module_cname = $Module) =~ s/\W/_/g;
    ($Packid = $Package) =~ tr/:/_/;
    $Packprefix = $Package;
    $Packprefix .= "::" if $Packprefix ne "";
    $lastline = "";
  }

  for (;;) {
    # Skip embedded PODs
    while ($lastline =~ /^=/) {
      while ($lastline = <$FH>) {
	last if ($lastline =~ /^=cut\s*$/);
      }
      death ("Error: Unterminated pod") unless $lastline;
      $lastline = <$FH>;
      chomp $lastline;
      $lastline =~ s/^\s+$//;
    }
    if ($lastline !~ /^\s*#/ ||
	# CPP directives:
	#	ANSI:	if ifdef ifndef elif else endif define undef
	#		line error pragma
	#	gcc:	warning include_next
	#   obj-c:	import
	#   others:	ident (gcc notes that some cpps have this one)
	$lastline =~ /^#[ \t]*(?:(?:if|ifn?def|elif|else|endif|define|undef|pragma|error|warning|line\s+\d+|ident)\b|(?:include(?:_next)?|import)\s*["<].*[>"])/) {
      last if $lastline =~ /^\S/ && @line && $line[-1] eq "";
      push(@line, $lastline);
      push(@line_no, $lastline_no) ;
    }

    # Read next line and continuation lines
    last unless defined($lastline = <$FH>);
    $lastline_no = $.;
    my $tmp_line;
    $lastline .= $tmp_line
      while ($lastline =~ /\\$/ && defined($tmp_line = <$FH>));

    chomp $lastline;
    $lastline =~ s/^\s+$//;
  }
  pop(@line), pop(@line_no) while @line && $line[-1] eq "";
  1;
}

sub output_init {
  local($type, $num, $var, $init, $name_printed) = @_;
  local($arg) = "ST(" . ($num - 1) . ")";

  if (  $init =~ /^=/  ) {
    if ($name_printed) {
      eval qq/print " $init\\n"/;
    } else {
      eval qq/print "\\t$var $init\\n"/;
    }
    warn $@   if  $@;
  } else {
    if (  $init =~ s/^\+//  &&  $num  ) {
      &generate_init($type, $num, $var, $name_printed);
    } elsif ($name_printed) {
      print ";\n";
      $init =~ s/^;//;
    } else {
      eval qq/print "\\t$var;\\n"/;
      warn $@   if  $@;
      $init =~ s/^;//;
    }
    $deferred .= eval qq/"\\n\\t$init\\n"/;
    warn $@   if  $@;
  }
}

sub Warn
  {
    # work out the line number
    my $line_no = $line_no[@line_no - @line -1] ;

    print STDERR "@_ in $filename, line $line_no\n" ;
  }

sub blurt
  {
    Warn @_ ;
    $errors ++
  }

sub death
  {
    Warn @_ ;
    exit 1 ;
  }

sub generate_init {
  local($type, $num, $var) = @_;
  local($arg) = "ST(" . ($num - 1) . ")";
  local($argoff) = $num - 1;
  local($ntype);
  local($tk);

  $type = TidyType($type) ;
  blurt("Error: '$type' not in typemap"), return
    unless defined($type_kind{$type});

  ($ntype = $type) =~ s/\s*\*/Ptr/g;
  ($subtype = $ntype) =~ s/(?:Array)?(?:Ptr)?$//;
  $tk = $type_kind{$type};
  $tk =~ s/OBJ$/REF/ if $func_name =~ /DESTROY$/;
  if ($tk eq 'T_PV' and exists $lengthof{$var}) {
    print "\t$var" unless $name_printed;
    print " = ($type)SvPV($arg, STRLEN_length_of_$var);\n";
    die "default value not supported with length(NAME) supplied"
      if defined $defaults{$var};
    return;
  }
  $type =~ tr/:/_/ unless $hiertype;
  blurt("Error: No INPUT definition for type '$type', typekind '$type_kind{$type}' found"), return
    unless defined $input_expr{$tk} ;
  $expr = $input_expr{$tk};
  if ($expr =~ /DO_ARRAY_ELEM/) {
    blurt("Error: '$subtype' not in typemap"), return
      unless defined($type_kind{$subtype});
    blurt("Error: No INPUT definition for type '$subtype', typekind '$type_kind{$subtype}' found"), return
      unless defined $input_expr{$type_kind{$subtype}} ;
    $subexpr = $input_expr{$type_kind{$subtype}};
    $subexpr =~ s/\$type/\$subtype/g;
    $subexpr =~ s/ntype/subtype/g;
    $subexpr =~ s/\$arg/ST(ix_$var)/g;
    $subexpr =~ s/\n\t/\n\t\t/g;
    $subexpr =~ s/is not of (.*\")/[arg %d] is not of $1, ix_$var + 1/g;
    $subexpr =~ s/\$var/${var}[ix_$var - $argoff]/;
    $expr =~ s/DO_ARRAY_ELEM/$subexpr/;
  }
  if ($expr =~ m#/\*.*scope.*\*/#i) {  # "scope" in C comments
    $ScopeThisXSUB = 1;
  }
  if (defined($defaults{$var})) {
    $expr =~ s/(\t+)/$1    /g;
    $expr =~ s/        /\t/g;
    if ($name_printed) {
      print ";\n";
    } else {
      eval qq/print "\\t$var;\\n"/;
      warn $@   if  $@;
    }
    if ($defaults{$var} eq 'NO_INIT') {
      $deferred .= eval qq/"\\n\\tif (items >= $num) {\\n$expr;\\n\\t}\\n"/;
    } else {
      $deferred .= eval qq/"\\n\\tif (items < $num)\\n\\t    $var = $defaults{$var};\\n\\telse {\\n$expr;\\n\\t}\\n"/;
    }
    warn $@   if  $@;
  } elsif ($ScopeThisXSUB or $expr !~ /^\s*\$var =/) {
    if ($name_printed) {
      print ";\n";
    } else {
      eval qq/print "\\t$var;\\n"/;
      warn $@   if  $@;
    }
    $deferred .= eval qq/"\\n$expr;\\n"/;
    warn $@   if  $@;
  } else {
    die "panic: do not know how to handle this branch for function pointers"
      if $name_printed;
    eval qq/print "$expr;\\n"/;
    warn $@   if  $@;
  }
}

sub generate_output {
  local($type, $num, $var, $do_setmagic, $do_push) = @_;
  local($arg) = "ST(" . ($num - ($num != 0)) . ")";
  local($argoff) = $num - 1;
  local($ntype);

  $type = TidyType($type) ;
  if ($type =~ /^array\(([^,]*),(.*)\)/) {
    print "\t$arg = sv_newmortal();\n";
    print "\tsv_setpvn($arg, (char *)$var, $2 * sizeof($1));\n";
    print "\tSvSETMAGIC($arg);\n" if $do_setmagic;
  } else {
    blurt("Error: '$type' not in typemap"), return
      unless defined($type_kind{$type});
    blurt("Error: No OUTPUT definition for type '$type', typekind '$type_kind{$type}' found"), return
      unless defined $output_expr{$type_kind{$type}} ;
    ($ntype = $type) =~ s/\s*\*/Ptr/g;
    $ntype =~ s/\(\)//g;
    ($subtype = $ntype) =~ s/(?:Array)?(?:Ptr)?$//;
    $expr = $output_expr{$type_kind{$type}};
    if ($expr =~ /DO_ARRAY_ELEM/) {
      blurt("Error: '$subtype' not in typemap"), return
	unless defined($type_kind{$subtype});
      blurt("Error: No OUTPUT definition for type '$subtype', typekind '$type_kind{$subtype}' found"), return
	unless defined $output_expr{$type_kind{$subtype}} ;
      $subexpr = $output_expr{$type_kind{$subtype}};
      $subexpr =~ s/ntype/subtype/g;
      $subexpr =~ s/\$arg/ST(ix_$var)/g;
      $subexpr =~ s/\$var/${var}[ix_$var]/g;
      $subexpr =~ s/\n\t/\n\t\t/g;
      $expr =~ s/DO_ARRAY_ELEM\n/$subexpr/;
      eval "print qq\a$expr\a";
      warn $@   if  $@;
      print "\t\tSvSETMAGIC(ST(ix_$var));\n" if $do_setmagic;
    } elsif ($var eq 'RETVAL') {
      if ($expr =~ /^\t\$arg = new/) {
	# We expect that $arg has refcnt 1, so we need to
	# mortalize it.
	eval "print qq\a$expr\a";
	warn $@   if  $@;
	print "\tsv_2mortal(ST($num));\n";
	print "\tSvSETMAGIC(ST($num));\n" if $do_setmagic;
      } elsif ($expr =~ /^\s*\$arg\s*=/) {
	# We expect that $arg has refcnt >=1, so we need
	# to mortalize it!
	eval "print qq\a$expr\a";
	warn $@   if  $@;
	print "\tsv_2mortal(ST(0));\n";
	print "\tSvSETMAGIC(ST(0));\n" if $do_setmagic;
      } else {
	# Just hope that the entry would safely write it
	# over an already mortalized value. By
	# coincidence, something like $arg = &sv_undef
	# works too.
	print "\tST(0) = sv_newmortal();\n";
	eval "print qq\a$expr\a";
	warn $@   if  $@;
	# new mortals don't have set magic
      }
    } elsif ($do_push) {
      print "\tPUSHs(sv_newmortal());\n";
      $arg = "ST($num)";
      eval "print qq\a$expr\a";
      warn $@   if  $@;
      print "\tSvSETMAGIC($arg);\n" if $do_setmagic;
    } elsif ($arg =~ /^ST\(\d+\)$/) {
      eval "print qq\a$expr\a";
      warn $@   if  $@;
      print "\tSvSETMAGIC($arg);\n" if $do_setmagic;
    }
  }
}

sub map_type {
  my($type, $varname) = @_;
  
  # C++ has :: in types too so skip this
  $type =~ tr/:/_/ unless $hiertype;
  $type =~ s/^array\(([^,]*),(.*)\).*/$1 */s;
  if ($varname) {
    if ($varname && $type =~ / \( \s* \* (?= \s* \) ) /xg) {
      (substr $type, pos $type, 0) = " $varname ";
    } else {
      $type .= "\t$varname";
    }
  }
  $type;
}


#########################################################
package
  ExtUtils::ParseXS::CountLines;
use strict;
use vars qw($SECTION_END_MARKER);

sub TIEHANDLE {
  my ($class, $cfile, $fh) = @_;
  $cfile =~ s/\\/\\\\/g;
  $SECTION_END_MARKER = qq{#line --- "$cfile"};
  
  return bless {buffer => '',
		fh => $fh,
		line_no => 1,
	       }, $class;
}

sub PRINT {
  my $self = shift;
  for (@_) {
    $self->{buffer} .= $_;
    while ($self->{buffer} =~ s/^([^\n]*\n)//) {
      my $line = $1;
      ++ $self->{line_no};
      $line =~ s|^\#line\s+---(?=\s)|#line $self->{line_no}|;
      print {$self->{fh}} $line;
    }
  }
}

sub PRINTF {
  my $self = shift;
  my $fmt = shift;
  $self->PRINT(sprintf($fmt, @_));
}

sub DESTROY {
  # Not necessary if we're careful to end with a "\n"
  my $self = shift;
  print {$self->{fh}} $self->{buffer};
}

sub UNTIE {
  # This sub does nothing, but is neccessary for references to be released.
}

sub end_marker {
  return $SECTION_END_MARKER;
}


1;
__END__

=head1 NAME

ExtUtils::ParseXS - converts Perl XS code into C code

=head1 SYNOPSIS

  use ExtUtils::ParseXS qw(process_file);
  
  process_file( filename => 'foo.xs' );

  process_file( filename => 'foo.xs',
                output => 'bar.c',
                'C++' => 1,
                typemap => 'path/to/typemap',
                hiertype => 1,
                except => 1,
                prototypes => 1,
                versioncheck => 1,
                linenumbers => 1,
                optimize => 1,
                prototypes => 1,
              );
=head1 DESCRIPTION

C<ExtUtils::ParseXS> will compile XS code into C code by embedding the constructs
necessary to let C functions manipulate Perl values and creates the glue
necessary to let Perl access those functions.  The compiler uses typemaps to
determine how to map C function parameters and variables to Perl values.

The compiler will search for typemap files called I<typemap>.  It will use
the following search path to find default typemaps, with the rightmost
typemap taking precedence.

	../../../typemap:../../typemap:../typemap:typemap

=head1 EXPORT

None by default.  C<process_file()> may be exported upon request.


=head1 FUNCTIONS

=over 4

=item process_xs()

This function processes an XS file and sends output to a C file.
Named parameters control how the processing is done.  The following
parameters are accepted:

=over 4

=item B<C++>

Adds C<extern "C"> to the C code.  Default is false.

=item B<hiertype>

Retains C<::> in type names so that C++ hierachical types can be
mapped.  Default is false.

=item B<except>

Adds exception handling stubs to the C code.  Default is false.

=item B<typemap>

Indicates that a user-supplied typemap should take precedence over the
default typemaps.  A single typemap may be specified as a string, or
multiple typemaps can be specified in an array reference, with the
last typemap having the highest precedence.

=item B<prototypes>

Generates prototype code for all xsubs.  Default is false.

=item B<versioncheck>

Makes sure at run time that the object file (derived from the C<.xs>
file) and the C<.pm> files have the same version number.  Default is
true.

=item B<linenumbers>

Adds C<#line> directives to the C output so error messages will look
like they came from the original XS file.  Default is true.

=item B<optimize>

Enables certain optimizations.  The only optimization that is currently
affected is the use of I<target>s by the output C code (see L<perlguts>).
Not optimizing may significantly slow down the generated code, but this is the way
B<xsubpp> of 5.005 and earlier operated.  Default is to optimize.

=item B<inout>

Enable recognition of C<IN>, C<OUT_LIST> and C<INOUT_LIST>
declarations.  Default is true.

=item B<argtypes>

Enable recognition of ANSI-like descriptions of function signature.
Default is true.

=item B<s>

I have no clue what this does.  Strips function prefixes?

=back

=item errors()

This function returns the number of [a certain kind of] errors
encountered during processing of the XS file.

=back

=head1 AUTHOR

Based on xsubpp code, written by Larry Wall.

Maintained by Ken Williams, <ken@mathforum.org>

=head1 COPYRIGHT

Copyright 2002-2003 Ken Williams.  All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

Based on the ExtUtils::xsubpp code by Larry Wall and the Perl 5
Porters, which was released under the same license terms.

=head1 SEE ALSO

L<perl>, ExtUtils::xsubpp, ExtUtils::MakeMaker, L<perlxs>, L<perlxstut>.

=cut
