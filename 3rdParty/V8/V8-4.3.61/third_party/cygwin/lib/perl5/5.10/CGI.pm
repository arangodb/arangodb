package CGI;
require 5.004;
use Carp 'croak';

# See the bottom of this file for the POD documentation.  Search for the
# string '=head'.

# You can run this file through either pod2man or pod2html to produce pretty
# documentation in manual or html file format (these utilities are part of the
# Perl 5 distribution).

# Copyright 1995-1998 Lincoln D. Stein.  All rights reserved.
# It may be used and modified freely, but I do request that this copyright
# notice remain attached to the file.  You may modify this module as you 
# wish, but if you redistribute a modified version, please attach a note
# listing the modifications you have made.

# The most recent version and complete docs are available at:
#   http://stein.cshl.org/WWW/software/CGI/

$CGI::revision = '$Id: CGI.pm,v 1.251 2008/04/23 13:08:23 lstein Exp $';
$CGI::VERSION='3.37';

# HARD-CODED LOCATION FOR FILE UPLOAD TEMPORARY FILES.
# UNCOMMENT THIS ONLY IF YOU KNOW WHAT YOU'RE DOING.
# $CGITempFile::TMPDIRECTORY = '/usr/tmp';
use CGI::Util qw(rearrange make_attributes unescape escape expires ebcdic2ascii ascii2ebcdic);

#use constant XHTML_DTD => ['-//W3C//DTD XHTML Basic 1.0//EN',
#                           'http://www.w3.org/TR/xhtml-basic/xhtml-basic10.dtd'];

use constant XHTML_DTD => ['-//W3C//DTD XHTML 1.0 Transitional//EN',
                           'http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd'];

{
  local $^W = 0;
  $TAINTED = substr("$0$^X",0,0);
}

$MOD_PERL            = 0; # no mod_perl by default

#global settings
$POST_MAX            = -1; # no limit to uploaded files
$DISABLE_UPLOADS     = 0;

@SAVED_SYMBOLS = ();


# >>>>> Here are some globals that you might want to adjust <<<<<<
sub initialize_globals {
    # Set this to 1 to enable copious autoloader debugging messages
    $AUTOLOAD_DEBUG = 0;

    # Set this to 1 to generate XTML-compatible output
    $XHTML = 1;

    # Change this to the preferred DTD to print in start_html()
    # or use default_dtd('text of DTD to use');
    $DEFAULT_DTD = [ '-//W3C//DTD HTML 4.01 Transitional//EN',
		     'http://www.w3.org/TR/html4/loose.dtd' ] ;

    # Set this to 1 to enable NOSTICKY scripts
    # or: 
    #    1) use CGI qw(-nosticky)
    #    2) $CGI::nosticky(1)
    $NOSTICKY = 0;

    # Set this to 1 to enable NPH scripts
    # or: 
    #    1) use CGI qw(-nph)
    #    2) CGI::nph(1)
    #    3) print header(-nph=>1)
    $NPH = 0;

    # Set this to 1 to enable debugging from @ARGV
    # Set to 2 to enable debugging from STDIN
    $DEBUG = 1;

    # Set this to 1 to make the temporary files created
    # during file uploads safe from prying eyes
    # or do...
    #    1) use CGI qw(:private_tempfiles)
    #    2) CGI::private_tempfiles(1);
    $PRIVATE_TEMPFILES = 0;

    # Set this to 1 to generate automatic tab indexes
    $TABINDEX = 0;

    # Set this to 1 to cause files uploaded in multipart documents
    # to be closed, instead of caching the file handle
    # or:
    #    1) use CGI qw(:close_upload_files)
    #    2) $CGI::close_upload_files(1);
    # Uploads with many files run out of file handles.
    # Also, for performance, since the file is already on disk,
    # it can just be renamed, instead of read and written.
    $CLOSE_UPLOAD_FILES = 0;

    # Automatically determined -- don't change
    $EBCDIC = 0;

    # Change this to 1 to suppress redundant HTTP headers
    $HEADERS_ONCE = 0;

    # separate the name=value pairs by semicolons rather than ampersands
    $USE_PARAM_SEMICOLONS = 1;

    # Do not include undefined params parsed from query string
    # use CGI qw(-no_undef_params);
    $NO_UNDEF_PARAMS = 0;

    # return everything as utf-8
    $PARAM_UTF8      = 0;

    # Other globals that you shouldn't worry about.
    undef $Q;
    $BEEN_THERE = 0;
    $DTD_PUBLIC_IDENTIFIER = "";
    undef @QUERY_PARAM;
    undef %EXPORT;
    undef $QUERY_CHARSET;
    undef %QUERY_FIELDNAMES;
    undef %QUERY_TMPFILES;

    # prevent complaints by mod_perl
    1;
}

# ------------------ START OF THE LIBRARY ------------

*end_form = \&endform;

# make mod_perlhappy
initialize_globals();

# FIGURE OUT THE OS WE'RE RUNNING UNDER
# Some systems support the $^O variable.  If not
# available then require() the Config library
unless ($OS) {
    unless ($OS = $^O) {
	require Config;
	$OS = $Config::Config{'osname'};
    }
}
if ($OS =~ /^MSWin/i) {
  $OS = 'WINDOWS';
} elsif ($OS =~ /^VMS/i) {
  $OS = 'VMS';
} elsif ($OS =~ /^dos/i) {
  $OS = 'DOS';
} elsif ($OS =~ /^MacOS/i) {
    $OS = 'MACINTOSH';
} elsif ($OS =~ /^os2/i) {
    $OS = 'OS2';
} elsif ($OS =~ /^epoc/i) {
    $OS = 'EPOC';
} elsif ($OS =~ /^cygwin/i) {
    $OS = 'CYGWIN';
} else {
    $OS = 'UNIX';
}

# Some OS logic.  Binary mode enabled on DOS, NT and VMS
$needs_binmode = $OS=~/^(WINDOWS|DOS|OS2|MSWin|CYGWIN)/;

# This is the default class for the CGI object to use when all else fails.
$DefaultClass = 'CGI' unless defined $CGI::DefaultClass;

# This is where to look for autoloaded routines.
$AutoloadClass = $DefaultClass unless defined $CGI::AutoloadClass;

# The path separator is a slash, backslash or semicolon, depending
# on the paltform.
$SL = {
     UNIX    => '/',  OS2 => '\\', EPOC      => '/', CYGWIN => '/',
     WINDOWS => '\\', DOS => '\\', MACINTOSH => ':', VMS    => '/'
    }->{$OS};

# This no longer seems to be necessary
# Turn on NPH scripts by default when running under IIS server!
# $NPH++ if defined($ENV{'SERVER_SOFTWARE'}) && $ENV{'SERVER_SOFTWARE'}=~/IIS/;
$IIS++ if defined($ENV{'SERVER_SOFTWARE'}) && $ENV{'SERVER_SOFTWARE'}=~/IIS/;

# Turn on special checking for Doug MacEachern's modperl
if (exists $ENV{MOD_PERL}) {
  # mod_perl handlers may run system() on scripts using CGI.pm;
  # Make sure so we don't get fooled by inherited $ENV{MOD_PERL}
  if (exists $ENV{MOD_PERL_API_VERSION} && $ENV{MOD_PERL_API_VERSION} == 2) {
    $MOD_PERL = 2;
    require Apache2::Response;
    require Apache2::RequestRec;
    require Apache2::RequestUtil;
    require Apache2::RequestIO;
    require APR::Pool;
  } else {
    $MOD_PERL = 1;
    require Apache;
  }
}

# Turn on special checking for ActiveState's PerlEx
$PERLEX++ if defined($ENV{'GATEWAY_INTERFACE'}) && $ENV{'GATEWAY_INTERFACE'} =~ /^CGI-PerlEx/;

# Define the CRLF sequence.  I can't use a simple "\r\n" because the meaning
# of "\n" is different on different OS's (sometimes it generates CRLF, sometimes LF
# and sometimes CR).  The most popular VMS web server
# doesn't accept CRLF -- instead it wants a LR.  EBCDIC machines don't
# use ASCII, so \015\012 means something different.  I find this all 
# really annoying.
$EBCDIC = "\t" ne "\011";
if ($OS eq 'VMS') {
  $CRLF = "\n";
} elsif ($EBCDIC) {
  $CRLF= "\r\n";
} else {
  $CRLF = "\015\012";
}

if ($needs_binmode) {
    $CGI::DefaultClass->binmode(\*main::STDOUT);
    $CGI::DefaultClass->binmode(\*main::STDIN);
    $CGI::DefaultClass->binmode(\*main::STDERR);
}

%EXPORT_TAGS = (
		':html2'=>['h1'..'h6',qw/p br hr ol ul li dl dt dd menu code var strong em
			   tt u i b blockquote pre img a address cite samp dfn html head
			   base body Link nextid title meta kbd start_html end_html
			   input Select option comment charset escapeHTML/],
		':html3'=>[qw/div table caption th td TR Tr sup Sub strike applet Param 
			   embed basefont style span layer ilayer font frameset frame script small big Area Map/],
                ':html4'=>[qw/abbr acronym bdo col colgroup del fieldset iframe
                            ins label legend noframes noscript object optgroup Q 
                            thead tbody tfoot/], 
		':netscape'=>[qw/blink fontsize center/],
		':form'=>[qw/textfield textarea filefield password_field hidden checkbox checkbox_group 
			  submit reset defaults radio_group popup_menu button autoEscape
			  scrolling_list image_button start_form end_form startform endform
			  start_multipart_form end_multipart_form isindex tmpFileName uploadInfo URL_ENCODED MULTIPART/],
		':cgi'=>[qw/param upload path_info path_translated request_uri url self_url script_name 
			 cookie Dump
			 raw_cookie request_method query_string Accept user_agent remote_host content_type
			 remote_addr referer server_name server_software server_port server_protocol virtual_port
			 virtual_host remote_ident auth_type http append
			 save_parameters restore_parameters param_fetch
			 remote_user user_name header redirect import_names put 
			 Delete Delete_all url_param cgi_error/],
		':ssl' => [qw/https/],
		':cgi-lib' => [qw/ReadParse PrintHeader HtmlTop HtmlBot SplitParam Vars/],
		':html' => [qw/:html2 :html3 :html4 :netscape/],
		':standard' => [qw/:html2 :html3 :html4 :form :cgi/],
		':push' => [qw/multipart_init multipart_start multipart_end multipart_final/],
		':all' => [qw/:html2 :html3 :netscape :form :cgi :internal :html4/]
		);

# Custom 'can' method for both autoloaded and non-autoloaded subroutines.
# Author: Cees Hek <cees@sitesuite.com.au>

sub can {
	my($class, $method) = @_;

	# See if UNIVERSAL::can finds it.

	if (my $func = $class -> SUPER::can($method) ){
		return $func;
	}

	# Try to compile the function.

	eval {
		# _compile looks at $AUTOLOAD for the function name.

		local $AUTOLOAD = join "::", $class, $method;
		&_compile;
	};

	# Now that the function is loaded (if it exists)
	# just use UNIVERSAL::can again to do the work.

	return $class -> SUPER::can($method);
}

# to import symbols into caller
sub import {
    my $self = shift;

    # This causes modules to clash.
    undef %EXPORT_OK;
    undef %EXPORT;

    $self->_setup_symbols(@_);
    my ($callpack, $callfile, $callline) = caller;

    # To allow overriding, search through the packages
    # Till we find one in which the correct subroutine is defined.
    my @packages = ($self,@{"$self\:\:ISA"});
    foreach $sym (keys %EXPORT) {
	my $pck;
	my $def = ${"$self\:\:AutoloadClass"} || $DefaultClass;
	foreach $pck (@packages) {
	    if (defined(&{"$pck\:\:$sym"})) {
		$def = $pck;
		last;
	    }
	}
	*{"${callpack}::$sym"} = \&{"$def\:\:$sym"};
    }
}

sub compile {
    my $pack = shift;
    $pack->_setup_symbols('-compile',@_);
}

sub expand_tags {
    my($tag) = @_;
    return ("start_$1","end_$1") if $tag=~/^(?:\*|start_|end_)(.+)/;
    my(@r);
    return ($tag) unless $EXPORT_TAGS{$tag};
    foreach (@{$EXPORT_TAGS{$tag}}) {
	push(@r,&expand_tags($_));
    }
    return @r;
}

#### Method: new
# The new routine.  This will check the current environment
# for an existing query string, and initialize itself, if so.
####
sub new {
  my($class,@initializer) = @_;
  my $self = {};

  bless $self,ref $class || $class || $DefaultClass;

  # always use a tempfile
  $self->{'use_tempfile'} = 1;

  if (ref($initializer[0])
      && (UNIVERSAL::isa($initializer[0],'Apache')
	  ||
	  UNIVERSAL::isa($initializer[0],'Apache2::RequestRec')
	 )) {
    $self->r(shift @initializer);
  }
 if (ref($initializer[0]) 
     && (UNIVERSAL::isa($initializer[0],'CODE'))) {
    $self->upload_hook(shift @initializer, shift @initializer);
    $self->{'use_tempfile'} = shift @initializer if (@initializer > 0);
  }
  if ($MOD_PERL) {
    if ($MOD_PERL == 1) {
      $self->r(Apache->request) unless $self->r;
      my $r = $self->r;
      $r->register_cleanup(\&CGI::_reset_globals);
      $self->_setup_symbols(@SAVED_SYMBOLS) if @SAVED_SYMBOLS;
    }
    else {
      # XXX: once we have the new API
      # will do a real PerlOptions -SetupEnv check
      $self->r(Apache2::RequestUtil->request) unless $self->r;
      my $r = $self->r;
      $r->subprocess_env unless exists $ENV{REQUEST_METHOD};
      $r->pool->cleanup_register(\&CGI::_reset_globals);
      $self->_setup_symbols(@SAVED_SYMBOLS) if @SAVED_SYMBOLS;
    }
    undef $NPH;
  }
  $self->_reset_globals if $PERLEX;
  $self->init(@initializer);
  return $self;
}

# We provide a DESTROY method so that we can ensure that
# temporary files are closed (via Fh->DESTROY) before they
# are unlinked (via CGITempFile->DESTROY) because it is not
# possible to unlink an open file on Win32. We explicitly
# call DESTROY on each, rather than just undefing them and
# letting Perl DESTROY them by garbage collection, in case the
# user is still holding any reference to them as well.
sub DESTROY {
  my $self = shift;
  if ($OS eq 'WINDOWS') {
    foreach my $href (values %{$self->{'.tmpfiles'}}) {
      $href->{hndl}->DESTROY if defined $href->{hndl};
      $href->{name}->DESTROY if defined $href->{name};
    }
  }
}

sub r {
  my $self = shift;
  my $r = $self->{'.r'};
  $self->{'.r'} = shift if @_;
  $r;
}

sub upload_hook {
  my $self;
  if (ref $_[0] eq 'CODE') {
    $CGI::Q = $self = $CGI::DefaultClass->new(@_);
  } else {
    $self = shift;
  }
  my ($hook,$data,$use_tempfile) = @_;
  $self->{'.upload_hook'} = $hook;
  $self->{'.upload_data'} = $data;
  $self->{'use_tempfile'} = $use_tempfile if defined $use_tempfile;
}

#### Method: param
# Returns the value(s)of a named parameter.
# If invoked in a list context, returns the
# entire list.  Otherwise returns the first
# member of the list.
# If name is not provided, return a list of all
# the known parameters names available.
# If more than one argument is provided, the
# second and subsequent arguments are used to
# set the value of the parameter.
####
sub param {
    my($self,@p) = self_or_default(@_);
    return $self->all_parameters unless @p;
    my($name,$value,@other);

    # For compatibility between old calling style and use_named_parameters() style, 
    # we have to special case for a single parameter present.
    if (@p > 1) {
	($name,$value,@other) = rearrange([NAME,[DEFAULT,VALUE,VALUES]],@p);
	my(@values);

	if (substr($p[0],0,1) eq '-') {
	    @values = defined($value) ? (ref($value) && ref($value) eq 'ARRAY' ? @{$value} : $value) : ();
	} else {
	    foreach ($value,@other) {
		push(@values,$_) if defined($_);
	    }
	}
	# If values is provided, then we set it.
	if (@values or defined $value) {
	    $self->add_parameter($name);
	    $self->{$name}=[@values];
	}
    } else {
	$name = $p[0];
    }

    return unless defined($name) && $self->{$name};

    my @result = @{$self->{$name}};

    if ($PARAM_UTF8) {
      eval "require Encode; 1;" unless Encode->can('decode'); # bring in these functions
      @result = map {ref $_ ? $_ : Encode::decode(utf8=>$_) } @result;
    }

    return wantarray ?  @result : $result[0];
}

sub self_or_default {
    return @_ if defined($_[0]) && (!ref($_[0])) &&($_[0] eq 'CGI');
    unless (defined($_[0]) && 
	    (ref($_[0]) eq 'CGI' || UNIVERSAL::isa($_[0],'CGI')) # slightly optimized for common case
	    ) {
	$Q = $CGI::DefaultClass->new unless defined($Q);
	unshift(@_,$Q);
    }
    return wantarray ? @_ : $Q;
}

sub self_or_CGI {
    local $^W=0;                # prevent a warning
    if (defined($_[0]) &&
	(substr(ref($_[0]),0,3) eq 'CGI' 
	 || UNIVERSAL::isa($_[0],'CGI'))) {
	return @_;
    } else {
	return ($DefaultClass,@_);
    }
}

########################################
# THESE METHODS ARE MORE OR LESS PRIVATE
# GO TO THE __DATA__ SECTION TO SEE MORE
# PUBLIC METHODS
########################################

# Initialize the query object from the environment.
# If a parameter list is found, this object will be set
# to an associative array in which parameter names are keys
# and the values are stored as lists
# If a keyword list is found, this method creates a bogus
# parameter list with the single parameter 'keywords'.

sub init {
  my $self = shift;
  my($query_string,$meth,$content_length,$fh,@lines) = ('','','','');

  my $is_xforms;

  my $initializer = shift;  # for backward compatibility
  local($/) = "\n";

    # set autoescaping on by default
    $self->{'escape'} = 1;

    # if we get called more than once, we want to initialize
    # ourselves from the original query (which may be gone
    # if it was read from STDIN originally.)
    if (defined(@QUERY_PARAM) && !defined($initializer)) {
        for my $name (@QUERY_PARAM) {
            my $val = $QUERY_PARAM{$name}; # always an arrayref;
            $self->param('-name'=>$name,'-value'=> $val);
            if (defined $val and ref $val eq 'ARRAY') {
                for my $fh (grep {defined(fileno($_))} @$val) {
                   seek($fh,0,0); # reset the filehandle.  
                }

            }
        }
        $self->charset($QUERY_CHARSET);
        $self->{'.fieldnames'} = {%QUERY_FIELDNAMES};
        $self->{'.tmpfiles'}   = {%QUERY_TMPFILES};
        return;
    }

    $meth=$ENV{'REQUEST_METHOD'} if defined($ENV{'REQUEST_METHOD'});
    $content_length = defined($ENV{'CONTENT_LENGTH'}) ? $ENV{'CONTENT_LENGTH'} : 0;

    $fh = to_filehandle($initializer) if $initializer;

    # set charset to the safe ISO-8859-1
    $self->charset('ISO-8859-1');

  METHOD: {

      # avoid unreasonably large postings
      if (($POST_MAX > 0) && ($content_length > $POST_MAX)) {
	#discard the post, unread
	$self->cgi_error("413 Request entity too large");
	last METHOD;
      }

      # Process multipart postings, but only if the initializer is
      # not defined.
      if ($meth eq 'POST'
	  && defined($ENV{'CONTENT_TYPE'})
	  && $ENV{'CONTENT_TYPE'}=~m|^multipart/form-data|
	  && !defined($initializer)
	  ) {
	  my($boundary) = $ENV{'CONTENT_TYPE'} =~ /boundary=\"?([^\";,]+)\"?/;
	  $self->read_multipart($boundary,$content_length);
	  last METHOD;
      } 

      # Process XForms postings. We know that we have XForms in the
      # following cases:
      # method eq 'POST' && content-type eq 'application/xml'
      # method eq 'POST' && content-type =~ /multipart\/related.+start=/
      # There are more cases, actually, but for now, we don't support other
      # methods for XForm posts.
      # In a XForm POST, the QUERY_STRING is parsed normally.
      # If the content-type is 'application/xml', we just set the param
      # XForms:Model (referring to the xml syntax) param containing the
      # unparsed XML data.
      # In the case of multipart/related we set XForms:Model as above, but
      # the other parts are available as uploads with the Content-ID as the
      # the key.
      # See the URL below for XForms specs on this issue.
      # http://www.w3.org/TR/2006/REC-xforms-20060314/slice11.html#submit-options
      if ($meth eq 'POST' && defined($ENV{'CONTENT_TYPE'})) {
              if ($ENV{'CONTENT_TYPE'} eq 'application/xml') {
                      my($param) = 'XForms:Model';
                      my($value) = '';
                      $self->add_parameter($param);
                      $self->read_from_client(\$value,$content_length,0)
                        if $content_length > 0;
                      push (@{$self->{$param}},$value);
                      $is_xforms = 1;
              } elsif ($ENV{'CONTENT_TYPE'} =~ /multipart\/related.+boundary=\"?([^\";,]+)\"?.+start=\"?\<?([^\"\>]+)\>?\"?/) {
                      my($boundary,$start) = ($1,$2);
                      my($param) = 'XForms:Model';
                      $self->add_parameter($param);
                      my($value) = $self->read_multipart_related($start,$boundary,$content_length,0);
                      push (@{$self->{$param}},$value);
                      if ($MOD_PERL) {
                              $query_string = $self->r->args;
                      } else {
                              $query_string = $ENV{'QUERY_STRING'} if defined $ENV{'QUERY_STRING'};
                              $query_string ||= $ENV{'REDIRECT_QUERY_STRING'} if defined $ENV{'REDIRECT_QUERY_STRING'};
                      }
                      $is_xforms = 1;
              }
      }


      # If initializer is defined, then read parameters
      # from it.
      if (!$is_xforms && defined($initializer)) {
	  if (UNIVERSAL::isa($initializer,'CGI')) {
	      $query_string = $initializer->query_string;
	      last METHOD;
	  }
	  if (ref($initializer) && ref($initializer) eq 'HASH') {
	      foreach (keys %$initializer) {
		  $self->param('-name'=>$_,'-value'=>$initializer->{$_});
	      }
	      last METHOD;
	  }

          if (defined($fh) && ($fh ne '')) {
              while (<$fh>) {
                  chomp;
                  last if /^=/;
                  push(@lines,$_);
              }
              # massage back into standard format
              if ("@lines" =~ /=/) {
                  $query_string=join("&",@lines);
              } else {
                  $query_string=join("+",@lines);
              }
              last METHOD;
          }

	  # last chance -- treat it as a string
	  $initializer = $$initializer if ref($initializer) eq 'SCALAR';
	  $query_string = $initializer;

	  last METHOD;
      }

      # If method is GET or HEAD, fetch the query from
      # the environment.
      if ($is_xforms || $meth=~/^(GET|HEAD)$/) {
	  if ($MOD_PERL) {
	    $query_string = $self->r->args;
	  } else {
	      $query_string = $ENV{'QUERY_STRING'} if defined $ENV{'QUERY_STRING'};
	      $query_string ||= $ENV{'REDIRECT_QUERY_STRING'} if defined $ENV{'REDIRECT_QUERY_STRING'};
	  }
	  last METHOD;
      }

      if ($meth eq 'POST' || $meth eq 'PUT') {
	  $self->read_from_client(\$query_string,$content_length,0)
	      if $content_length > 0;
	  # Some people want to have their cake and eat it too!
	  # Uncomment this line to have the contents of the query string
	  # APPENDED to the POST data.
	  # $query_string .= (length($query_string) ? '&' : '') . $ENV{'QUERY_STRING'} if defined $ENV{'QUERY_STRING'};
	  last METHOD;
      }

      # If $meth is not of GET, POST or HEAD, assume we're being debugged offline.
      # Check the command line and then the standard input for data.
      # We use the shellwords package in order to behave the way that
      # UN*X programmers expect.
      if ($DEBUG)
      {
          my $cmdline_ret = read_from_cmdline();
          $query_string = $cmdline_ret->{'query_string'};
          if (defined($cmdline_ret->{'subpath'}))
          {
              $self->path_info($cmdline_ret->{'subpath'});
          }
      }
  }

# YL: Begin Change for XML handler 10/19/2001
    if (!$is_xforms && ($meth eq 'POST' || $meth eq 'PUT')
        && defined($ENV{'CONTENT_TYPE'})
        && $ENV{'CONTENT_TYPE'} !~ m|^application/x-www-form-urlencoded|
	&& $ENV{'CONTENT_TYPE'} !~ m|^multipart/form-data| ) {
        my($param) = $meth . 'DATA' ;
        $self->add_parameter($param) ;
      push (@{$self->{$param}},$query_string);
      undef $query_string ;
    }
# YL: End Change for XML handler 10/19/2001

    # We now have the query string in hand.  We do slightly
    # different things for keyword lists and parameter lists.
    if (defined $query_string && length $query_string) {
	if ($query_string =~ /[&=;]/) {
	    $self->parse_params($query_string);
	} else {
	    $self->add_parameter('keywords');
	    $self->{'keywords'} = [$self->parse_keywordlist($query_string)];
	}
    }

    # Special case.  Erase everything if there is a field named
    # .defaults.
    if ($self->param('.defaults')) {
      $self->delete_all();
    }

    # Associative array containing our defined fieldnames
    $self->{'.fieldnames'} = {};
    foreach ($self->param('.cgifields')) {
	$self->{'.fieldnames'}->{$_}++;
    }
    
    # Clear out our default submission button flag if present
    $self->delete('.submit');
    $self->delete('.cgifields');

    $self->save_request unless defined $initializer;
}

# FUNCTIONS TO OVERRIDE:
# Turn a string into a filehandle
sub to_filehandle {
    my $thingy = shift;
    return undef unless $thingy;
    return $thingy if UNIVERSAL::isa($thingy,'GLOB');
    return $thingy if UNIVERSAL::isa($thingy,'FileHandle');
    if (!ref($thingy)) {
	my $caller = 1;
	while (my $package = caller($caller++)) {
	    my($tmp) = $thingy=~/[\':]/ ? $thingy : "$package\:\:$thingy"; 
	    return $tmp if defined(fileno($tmp));
	}
    }
    return undef;
}

# send output to the browser
sub put {
    my($self,@p) = self_or_default(@_);
    $self->print(@p);
}

# print to standard output (for overriding in mod_perl)
sub print {
    shift;
    CORE::print(@_);
}

# get/set last cgi_error
sub cgi_error {
    my ($self,$err) = self_or_default(@_);
    $self->{'.cgi_error'} = $err if defined $err;
    return $self->{'.cgi_error'};
}

sub save_request {
    my($self) = @_;
    # We're going to play with the package globals now so that if we get called
    # again, we initialize ourselves in exactly the same way.  This allows
    # us to have several of these objects.
    @QUERY_PARAM = $self->param; # save list of parameters
    foreach (@QUERY_PARAM) {
      next unless defined $_;
      $QUERY_PARAM{$_}=$self->{$_};
    }
    $QUERY_CHARSET = $self->charset;
    %QUERY_FIELDNAMES = %{$self->{'.fieldnames'}};
    %QUERY_TMPFILES   = %{ $self->{'.tmpfiles'} || {} };
}

sub parse_params {
    my($self,$tosplit) = @_;
    my(@pairs) = split(/[&;]/,$tosplit);
    my($param,$value);
    foreach (@pairs) {
	($param,$value) = split('=',$_,2);
	next unless defined $param;
	next if $NO_UNDEF_PARAMS and not defined $value;
	$value = '' unless defined $value;
	$param = unescape($param);
	$value = unescape($value);
	$self->add_parameter($param);
	push (@{$self->{$param}},$value);
    }
}

sub add_parameter {
    my($self,$param)=@_;
    return unless defined $param;
    push (@{$self->{'.parameters'}},$param) 
	unless defined($self->{$param});
}

sub all_parameters {
    my $self = shift;
    return () unless defined($self) && $self->{'.parameters'};
    return () unless @{$self->{'.parameters'}};
    return @{$self->{'.parameters'}};
}

# put a filehandle into binary mode (DOS)
sub binmode {
    return unless defined($_[1]) && defined fileno($_[1]);
    CORE::binmode($_[1]);
}

sub _make_tag_func {
    my ($self,$tagname) = @_;
    my $func = qq(
	sub $tagname {
         my (\$q,\$a,\@rest) = self_or_default(\@_);
         my(\$attr) = '';
	 if (ref(\$a) && ref(\$a) eq 'HASH') {
	    my(\@attr) = make_attributes(\$a,\$q->{'escape'});
	    \$attr = " \@attr" if \@attr;
	  } else {
	    unshift \@rest,\$a if defined \$a;
	  }
	);
    if ($tagname=~/start_(\w+)/i) {
	$func .= qq! return "<\L$1\E\$attr>";} !;
    } elsif ($tagname=~/end_(\w+)/i) {
	$func .= qq! return "<\L/$1\E>"; } !;
    } else {
	$func .= qq#
	    return \$XHTML ? "\L<$tagname\E\$attr />" : "\L<$tagname\E\$attr>" unless \@rest;
	    my(\$tag,\$untag) = ("\L<$tagname\E\$attr>","\L</$tagname>\E");
	    my \@result = map { "\$tag\$_\$untag" } 
                              (ref(\$rest[0]) eq 'ARRAY') ? \@{\$rest[0]} : "\@rest";
	    return "\@result";
            }#;
    }
return $func;
}

sub AUTOLOAD {
    print STDERR "CGI::AUTOLOAD for $AUTOLOAD\n" if $CGI::AUTOLOAD_DEBUG;
    my $func = &_compile;
    goto &$func;
}

sub _compile {
    my($func) = $AUTOLOAD;
    my($pack,$func_name);
    {
	local($1,$2); # this fixes an obscure variable suicide problem.
	$func=~/(.+)::([^:]+)$/;
	($pack,$func_name) = ($1,$2);
	$pack=~s/::SUPER$//;	# fix another obscure problem
	$pack = ${"$pack\:\:AutoloadClass"} || $CGI::DefaultClass
	    unless defined(${"$pack\:\:AUTOLOADED_ROUTINES"});

        my($sub) = \%{"$pack\:\:SUBS"};
        unless (%$sub) {
	   my($auto) = \${"$pack\:\:AUTOLOADED_ROUTINES"};
	   local ($@,$!);
	   eval "package $pack; $$auto";
	   croak("$AUTOLOAD: $@") if $@;
           $$auto = '';  # Free the unneeded storage (but don't undef it!!!)
       }
       my($code) = $sub->{$func_name};

       $code = "sub $AUTOLOAD { }" if (!$code and $func_name eq 'DESTROY');
       if (!$code) {
	   (my $base = $func_name) =~ s/^(start_|end_)//i;
	   if ($EXPORT{':any'} || 
	       $EXPORT{'-any'} ||
	       $EXPORT{$base} || 
	       (%EXPORT_OK || grep(++$EXPORT_OK{$_},&expand_tags(':html')))
	           && $EXPORT_OK{$base}) {
	       $code = $CGI::DefaultClass->_make_tag_func($func_name);
	   }
       }
       croak("Undefined subroutine $AUTOLOAD\n") unless $code;
       local ($@,$!);
       eval "package $pack; $code";
       if ($@) {
	   $@ =~ s/ at .*\n//;
	   croak("$AUTOLOAD: $@");
       }
    }       
    CORE::delete($sub->{$func_name});  #free storage
    return "$pack\:\:$func_name";
}

sub _selected {
  my $self = shift;
  my $value = shift;
  return '' unless $value;
  return $XHTML ? qq(selected="selected" ) : qq(selected );
}

sub _checked {
  my $self = shift;
  my $value = shift;
  return '' unless $value;
  return $XHTML ? qq(checked="checked" ) : qq(checked );
}

sub _reset_globals { initialize_globals(); }

sub _setup_symbols {
    my $self = shift;
    my $compile = 0;

    # to avoid reexporting unwanted variables
    undef %EXPORT;

    foreach (@_) {
	$HEADERS_ONCE++,         next if /^[:-]unique_headers$/;
	$NPH++,                  next if /^[:-]nph$/;
	$NOSTICKY++,             next if /^[:-]nosticky$/;
	$DEBUG=0,                next if /^[:-]no_?[Dd]ebug$/;
	$DEBUG=2,                next if /^[:-][Dd]ebug$/;
	$USE_PARAM_SEMICOLONS++, next if /^[:-]newstyle_urls$/;
	$PARAM_UTF8++,           next if /^[:-]utf8$/;
	$XHTML++,                next if /^[:-]xhtml$/;
	$XHTML=0,                next if /^[:-]no_?xhtml$/;
	$USE_PARAM_SEMICOLONS=0, next if /^[:-]oldstyle_urls$/;
	$PRIVATE_TEMPFILES++,    next if /^[:-]private_tempfiles$/;
	$TABINDEX++,             next if /^[:-]tabindex$/;
	$CLOSE_UPLOAD_FILES++,   next if /^[:-]close_upload_files$/;
	$EXPORT{$_}++,           next if /^[:-]any$/;
	$compile++,              next if /^[:-]compile$/;
	$NO_UNDEF_PARAMS++,      next if /^[:-]no_undef_params$/;
	
	# This is probably extremely evil code -- to be deleted some day.
	if (/^[-]autoload$/) {
	    my($pkg) = caller(1);
	    *{"${pkg}::AUTOLOAD"} = sub { 
		my($routine) = $AUTOLOAD;
		$routine =~ s/^.*::/CGI::/;
		&$routine;
	    };
	    next;
	}

	foreach (&expand_tags($_)) {
	    tr/a-zA-Z0-9_//cd;  # don't allow weird function names
	    $EXPORT{$_}++;
	}
    }
    _compile_all(keys %EXPORT) if $compile;
    @SAVED_SYMBOLS = @_;
}

sub charset {
  my ($self,$charset) = self_or_default(@_);
  $self->{'.charset'} = $charset if defined $charset;
  $self->{'.charset'};
}

sub element_id {
  my ($self,$new_value) = self_or_default(@_);
  $self->{'.elid'} = $new_value if defined $new_value;
  sprintf('%010d',$self->{'.elid'}++);
}

sub element_tab {
  my ($self,$new_value) = self_or_default(@_);
  $self->{'.etab'} ||= 1;
  $self->{'.etab'} = $new_value if defined $new_value;
  my $tab = $self->{'.etab'}++;
  return '' unless $TABINDEX or defined $new_value;
  return qq(tabindex="$tab" );
}

###############################################################################
################# THESE FUNCTIONS ARE AUTOLOADED ON DEMAND ####################
###############################################################################
$AUTOLOADED_ROUTINES = '';      # get rid of -w warning
$AUTOLOADED_ROUTINES=<<'END_OF_AUTOLOAD';

%SUBS = (

'URL_ENCODED'=> <<'END_OF_FUNC',
sub URL_ENCODED { 'application/x-www-form-urlencoded'; }
END_OF_FUNC

'MULTIPART' => <<'END_OF_FUNC',
sub MULTIPART {  'multipart/form-data'; }
END_OF_FUNC

'SERVER_PUSH' => <<'END_OF_FUNC',
sub SERVER_PUSH { 'multipart/x-mixed-replace;boundary="' . shift() . '"'; }
END_OF_FUNC

'new_MultipartBuffer' => <<'END_OF_FUNC',
# Create a new multipart buffer
sub new_MultipartBuffer {
    my($self,$boundary,$length) = @_;
    return MultipartBuffer->new($self,$boundary,$length);
}
END_OF_FUNC

'read_from_client' => <<'END_OF_FUNC',
# Read data from a file handle
sub read_from_client {
    my($self, $buff, $len, $offset) = @_;
    local $^W=0;                # prevent a warning
    return $MOD_PERL
        ? $self->r->read($$buff, $len, $offset)
        : read(\*STDIN, $$buff, $len, $offset);
}
END_OF_FUNC

'delete' => <<'END_OF_FUNC',
#### Method: delete
# Deletes the named parameter entirely.
####
sub delete {
    my($self,@p) = self_or_default(@_);
    my(@names) = rearrange([NAME],@p);
    my @to_delete = ref($names[0]) eq 'ARRAY' ? @$names[0] : @names;
    my %to_delete;
    foreach my $name (@to_delete)
    {
        CORE::delete $self->{$name};
        CORE::delete $self->{'.fieldnames'}->{$name};
        $to_delete{$name}++;
    }
    @{$self->{'.parameters'}}=grep { !exists($to_delete{$_}) } $self->param();
    return;
}
END_OF_FUNC

#### Method: import_names
# Import all parameters into the given namespace.
# Assumes namespace 'Q' if not specified
####
'import_names' => <<'END_OF_FUNC',
sub import_names {
    my($self,$namespace,$delete) = self_or_default(@_);
    $namespace = 'Q' unless defined($namespace);
    die "Can't import names into \"main\"\n" if \%{"${namespace}::"} == \%::;
    if ($delete || $MOD_PERL || exists $ENV{'FCGI_ROLE'}) {
	# can anyone find an easier way to do this?
	foreach (keys %{"${namespace}::"}) {
	    local *symbol = "${namespace}::${_}";
	    undef $symbol;
	    undef @symbol;
	    undef %symbol;
	}
    }
    my($param,@value,$var);
    foreach $param ($self->param) {
	# protect against silly names
	($var = $param)=~tr/a-zA-Z0-9_/_/c;
	$var =~ s/^(?=\d)/_/;
	local *symbol = "${namespace}::$var";
	@value = $self->param($param);
	@symbol = @value;
	$symbol = $value[0];
    }
}
END_OF_FUNC

#### Method: keywords
# Keywords acts a bit differently.  Calling it in a list context
# returns the list of keywords.  
# Calling it in a scalar context gives you the size of the list.
####
'keywords' => <<'END_OF_FUNC',
sub keywords {
    my($self,@values) = self_or_default(@_);
    # If values is provided, then we set it.
    $self->{'keywords'}=[@values] if @values;
    my(@result) = defined($self->{'keywords'}) ? @{$self->{'keywords'}} : ();
    @result;
}
END_OF_FUNC

# These are some tie() interfaces for compatibility
# with Steve Brenner's cgi-lib.pl routines
'Vars' => <<'END_OF_FUNC',
sub Vars {
    my $q = shift;
    my %in;
    tie(%in,CGI,$q);
    return %in if wantarray;
    return \%in;
}
END_OF_FUNC

# These are some tie() interfaces for compatibility
# with Steve Brenner's cgi-lib.pl routines
'ReadParse' => <<'END_OF_FUNC',
sub ReadParse {
    local(*in);
    if (@_) {
	*in = $_[0];
    } else {
	my $pkg = caller();
	*in=*{"${pkg}::in"};
    }
    tie(%in,CGI);
    return scalar(keys %in);
}
END_OF_FUNC

'PrintHeader' => <<'END_OF_FUNC',
sub PrintHeader {
    my($self) = self_or_default(@_);
    return $self->header();
}
END_OF_FUNC

'HtmlTop' => <<'END_OF_FUNC',
sub HtmlTop {
    my($self,@p) = self_or_default(@_);
    return $self->start_html(@p);
}
END_OF_FUNC

'HtmlBot' => <<'END_OF_FUNC',
sub HtmlBot {
    my($self,@p) = self_or_default(@_);
    return $self->end_html(@p);
}
END_OF_FUNC

'SplitParam' => <<'END_OF_FUNC',
sub SplitParam {
    my ($param) = @_;
    my (@params) = split ("\0", $param);
    return (wantarray ? @params : $params[0]);
}
END_OF_FUNC

'MethGet' => <<'END_OF_FUNC',
sub MethGet {
    return request_method() eq 'GET';
}
END_OF_FUNC

'MethPost' => <<'END_OF_FUNC',
sub MethPost {
    return request_method() eq 'POST';
}
END_OF_FUNC

'TIEHASH' => <<'END_OF_FUNC',
sub TIEHASH {
    my $class = shift;
    my $arg   = $_[0];
    if (ref($arg) && UNIVERSAL::isa($arg,'CGI')) {
       return $arg;
    }
    return $Q ||= $class->new(@_);
}
END_OF_FUNC

'STORE' => <<'END_OF_FUNC',
sub STORE {
    my $self = shift;
    my $tag  = shift;
    my $vals = shift;
    my @vals = index($vals,"\0")!=-1 ? split("\0",$vals) : $vals;
    $self->param(-name=>$tag,-value=>\@vals);
}
END_OF_FUNC

'FETCH' => <<'END_OF_FUNC',
sub FETCH {
    return $_[0] if $_[1] eq 'CGI';
    return undef unless defined $_[0]->param($_[1]);
    return join("\0",$_[0]->param($_[1]));
}
END_OF_FUNC

'FIRSTKEY' => <<'END_OF_FUNC',
sub FIRSTKEY {
    $_[0]->{'.iterator'}=0;
    $_[0]->{'.parameters'}->[$_[0]->{'.iterator'}++];
}
END_OF_FUNC

'NEXTKEY' => <<'END_OF_FUNC',
sub NEXTKEY {
    $_[0]->{'.parameters'}->[$_[0]->{'.iterator'}++];
}
END_OF_FUNC

'EXISTS' => <<'END_OF_FUNC',
sub EXISTS {
    exists $_[0]->{$_[1]};
}
END_OF_FUNC

'DELETE' => <<'END_OF_FUNC',
sub DELETE {
    $_[0]->delete($_[1]);
}
END_OF_FUNC

'CLEAR' => <<'END_OF_FUNC',
sub CLEAR {
    %{$_[0]}=();
}
####
END_OF_FUNC

####
# Append a new value to an existing query
####
'append' => <<'EOF',
sub append {
    my($self,@p) = self_or_default(@_);
    my($name,$value) = rearrange([NAME,[VALUE,VALUES]],@p);
    my(@values) = defined($value) ? (ref($value) ? @{$value} : $value) : ();
    if (@values) {
	$self->add_parameter($name);
	push(@{$self->{$name}},@values);
    }
    return $self->param($name);
}
EOF

#### Method: delete_all
# Delete all parameters
####
'delete_all' => <<'EOF',
sub delete_all {
    my($self) = self_or_default(@_);
    my @param = $self->param();
    $self->delete(@param);
}
EOF

'Delete' => <<'EOF',
sub Delete {
    my($self,@p) = self_or_default(@_);
    $self->delete(@p);
}
EOF

'Delete_all' => <<'EOF',
sub Delete_all {
    my($self,@p) = self_or_default(@_);
    $self->delete_all(@p);
}
EOF

#### Method: autoescape
# If you want to turn off the autoescaping features,
# call this method with undef as the argument
'autoEscape' => <<'END_OF_FUNC',
sub autoEscape {
    my($self,$escape) = self_or_default(@_);
    my $d = $self->{'escape'};
    $self->{'escape'} = $escape;
    $d;
}
END_OF_FUNC


#### Method: version
# Return the current version
####
'version' => <<'END_OF_FUNC',
sub version {
    return $VERSION;
}
END_OF_FUNC

#### Method: url_param
# Return a parameter in the QUERY_STRING, regardless of
# whether this was a POST or a GET
####
'url_param' => <<'END_OF_FUNC',
sub url_param {
    my ($self,@p) = self_or_default(@_);
    my $name = shift(@p);
    return undef unless exists($ENV{QUERY_STRING});
    unless (exists($self->{'.url_param'})) {
	$self->{'.url_param'}={}; # empty hash
	if ($ENV{QUERY_STRING} =~ /=/) {
	    my(@pairs) = split(/[&;]/,$ENV{QUERY_STRING});
	    my($param,$value);
	    foreach (@pairs) {
		($param,$value) = split('=',$_,2);
		$param = unescape($param);
		$value = unescape($value);
		push(@{$self->{'.url_param'}->{$param}},$value);
	    }
	} else {
	    $self->{'.url_param'}->{'keywords'} = [$self->parse_keywordlist($ENV{QUERY_STRING})];
	}
    }
    return keys %{$self->{'.url_param'}} unless defined($name);
    return () unless $self->{'.url_param'}->{$name};
    return wantarray ? @{$self->{'.url_param'}->{$name}}
                     : $self->{'.url_param'}->{$name}->[0];
}
END_OF_FUNC

#### Method: Dump
# Returns a string in which all the known parameter/value 
# pairs are represented as nested lists, mainly for the purposes 
# of debugging.
####
'Dump' => <<'END_OF_FUNC',
sub Dump {
    my($self) = self_or_default(@_);
    my($param,$value,@result);
    return '<ul></ul>' unless $self->param;
    push(@result,"<ul>");
    foreach $param ($self->param) {
	my($name)=$self->escapeHTML($param);
	push(@result,"<li><strong>$param</strong></li>");
	push(@result,"<ul>");
	foreach $value ($self->param($param)) {
	    $value = $self->escapeHTML($value);
            $value =~ s/\n/<br \/>\n/g;
	    push(@result,"<li>$value</li>");
	}
	push(@result,"</ul>");
    }
    push(@result,"</ul>");
    return join("\n",@result);
}
END_OF_FUNC

#### Method as_string
#
# synonym for "dump"
####
'as_string' => <<'END_OF_FUNC',
sub as_string {
    &Dump(@_);
}
END_OF_FUNC

#### Method: save
# Write values out to a filehandle in such a way that they can
# be reinitialized by the filehandle form of the new() method
####
'save' => <<'END_OF_FUNC',
sub save {
    my($self,$filehandle) = self_or_default(@_);
    $filehandle = to_filehandle($filehandle);
    my($param);
    local($,) = '';  # set print field separator back to a sane value
    local($\) = '';  # set output line separator to a sane value
    foreach $param ($self->param) {
	my($escaped_param) = escape($param);
	my($value);
	foreach $value ($self->param($param)) {
	    print $filehandle "$escaped_param=",escape("$value"),"\n";
	}
    }
    foreach (keys %{$self->{'.fieldnames'}}) {
          print $filehandle ".cgifields=",escape("$_"),"\n";
    }
    print $filehandle "=\n";    # end of record
}
END_OF_FUNC


#### Method: save_parameters
# An alias for save() that is a better name for exportation.
# Only intended to be used with the function (non-OO) interface.
####
'save_parameters' => <<'END_OF_FUNC',
sub save_parameters {
    my $fh = shift;
    return save(to_filehandle($fh));
}
END_OF_FUNC

#### Method: restore_parameters
# A way to restore CGI parameters from an initializer.
# Only intended to be used with the function (non-OO) interface.
####
'restore_parameters' => <<'END_OF_FUNC',
sub restore_parameters {
    $Q = $CGI::DefaultClass->new(@_);
}
END_OF_FUNC

#### Method: multipart_init
# Return a Content-Type: style header for server-push
# This has to be NPH on most web servers, and it is advisable to set $| = 1
#
# Many thanks to Ed Jordan <ed@fidalgo.net> for this
# contribution, updated by Andrew Benham (adsb@bigfoot.com)
####
'multipart_init' => <<'END_OF_FUNC',
sub multipart_init {
    my($self,@p) = self_or_default(@_);
    my($boundary,@other) = rearrange([BOUNDARY],@p);
    $boundary = $boundary || '------- =_aaaaaaaaaa0';
    $self->{'separator'} = "$CRLF--$boundary$CRLF";
    $self->{'final_separator'} = "$CRLF--$boundary--$CRLF";
    $type = SERVER_PUSH($boundary);
    return $self->header(
	-nph => 0,
	-type => $type,
	(map { split "=", $_, 2 } @other),
    ) . "WARNING: YOUR BROWSER DOESN'T SUPPORT THIS SERVER-PUSH TECHNOLOGY." . $self->multipart_end;
}
END_OF_FUNC


#### Method: multipart_start
# Return a Content-Type: style header for server-push, start of section
#
# Many thanks to Ed Jordan <ed@fidalgo.net> for this
# contribution, updated by Andrew Benham (adsb@bigfoot.com)
####
'multipart_start' => <<'END_OF_FUNC',
sub multipart_start {
    my(@header);
    my($self,@p) = self_or_default(@_);
    my($type,@other) = rearrange([TYPE],@p);
    $type = $type || 'text/html';
    push(@header,"Content-Type: $type");

    # rearrange() was designed for the HTML portion, so we
    # need to fix it up a little.
    foreach (@other) {
        # Don't use \s because of perl bug 21951
        next unless my($header,$value) = /([^ \r\n\t=]+)=\"?(.+?)\"?$/;
	($_ = $header) =~ s/^(\w)(.*)/$1 . lc ($2) . ': '.$self->unescapeHTML($value)/e;
    }
    push(@header,@other);
    my $header = join($CRLF,@header)."${CRLF}${CRLF}";
    return $header;
}
END_OF_FUNC


#### Method: multipart_end
# Return a MIME boundary separator for server-push, end of section
#
# Many thanks to Ed Jordan <ed@fidalgo.net> for this
# contribution
####
'multipart_end' => <<'END_OF_FUNC',
sub multipart_end {
    my($self,@p) = self_or_default(@_);
    return $self->{'separator'};
}
END_OF_FUNC


#### Method: multipart_final
# Return a MIME boundary separator for server-push, end of all sections
#
# Contributed by Andrew Benham (adsb@bigfoot.com)
####
'multipart_final' => <<'END_OF_FUNC',
sub multipart_final {
    my($self,@p) = self_or_default(@_);
    return $self->{'final_separator'} . "WARNING: YOUR BROWSER DOESN'T SUPPORT THIS SERVER-PUSH TECHNOLOGY." . $CRLF;
}
END_OF_FUNC


#### Method: header
# Return a Content-Type: style header
#
####
'header' => <<'END_OF_FUNC',
sub header {
    my($self,@p) = self_or_default(@_);
    my(@header);

    return "" if $self->{'.header_printed'}++ and $HEADERS_ONCE;

    my($type,$status,$cookie,$target,$expires,$nph,$charset,$attachment,$p3p,@other) = 
	rearrange([['TYPE','CONTENT_TYPE','CONTENT-TYPE'],
			    'STATUS',['COOKIE','COOKIES'],'TARGET',
                            'EXPIRES','NPH','CHARSET',
                            'ATTACHMENT','P3P'],@p);

    $nph     ||= $NPH;

    $type ||= 'text/html' unless defined($type);

    if (defined $charset) {
      $self->charset($charset);
    } else {
      $charset = $self->charset if $type =~ /^text\//;
    }
   $charset ||= '';

    # rearrange() was designed for the HTML portion, so we
    # need to fix it up a little.
    foreach (@other) {
        # Don't use \s because of perl bug 21951
        next unless my($header,$value) = /([^ \r\n\t=]+)=\"?(.+?)\"?$/;
        ($_ = $header) =~ s/^(\w)(.*)/"\u$1\L$2" . ': '.$self->unescapeHTML($value)/e;
    }

    $type .= "; charset=$charset"
      if     $type ne ''
         and $type !~ /\bcharset\b/
         and defined $charset
         and $charset ne '';

    # Maybe future compatibility.  Maybe not.
    my $protocol = $ENV{SERVER_PROTOCOL} || 'HTTP/1.0';
    push(@header,$protocol . ' ' . ($status || '200 OK')) if $nph;
    push(@header,"Server: " . &server_software()) if $nph;

    push(@header,"Status: $status") if $status;
    push(@header,"Window-Target: $target") if $target;
    if ($p3p) {
       $p3p = join ' ',@$p3p if ref($p3p) eq 'ARRAY';
       push(@header,qq(P3P: policyref="/w3c/p3p.xml", CP="$p3p"));
    }
    # push all the cookies -- there may be several
    if ($cookie) {
	my(@cookie) = ref($cookie) && ref($cookie) eq 'ARRAY' ? @{$cookie} : $cookie;
	foreach (@cookie) {
            my $cs = UNIVERSAL::isa($_,'CGI::Cookie') ? $_->as_string : $_;
	    push(@header,"Set-Cookie: $cs") if $cs ne '';
	}
    }
    # if the user indicates an expiration time, then we need
    # both an Expires and a Date header (so that the browser is
    # uses OUR clock)
    push(@header,"Expires: " . expires($expires,'http'))
	if $expires;
    push(@header,"Date: " . expires(0,'http')) if $expires || $cookie || $nph;
    push(@header,"Pragma: no-cache") if $self->cache();
    push(@header,"Content-Disposition: attachment; filename=\"$attachment\"") if $attachment;
    push(@header,map {ucfirst $_} @other);
    push(@header,"Content-Type: $type") if $type ne '';
    my $header = join($CRLF,@header)."${CRLF}${CRLF}";
    if (($MOD_PERL >= 1) && !$nph) {
        $self->r->send_cgi_header($header);
        return '';
    }
    return $header;
}
END_OF_FUNC


#### Method: cache
# Control whether header() will produce the no-cache
# Pragma directive.
####
'cache' => <<'END_OF_FUNC',
sub cache {
    my($self,$new_value) = self_or_default(@_);
    $new_value = '' unless $new_value;
    if ($new_value ne '') {
	$self->{'cache'} = $new_value;
    }
    return $self->{'cache'};
}
END_OF_FUNC


#### Method: redirect
# Return a Location: style header
#
####
'redirect' => <<'END_OF_FUNC',
sub redirect {
    my($self,@p) = self_or_default(@_);
    my($url,$target,$status,$cookie,$nph,@other) = 
         rearrange([[LOCATION,URI,URL],TARGET,STATUS,['COOKIE','COOKIES'],NPH],@p);
    $status = '302 Found' unless defined $status;
    $url ||= $self->self_url;
    my(@o);
    foreach (@other) { tr/\"//d; push(@o,split("=",$_,2)); }
    unshift(@o,
	 '-Status'  => $status,
	 '-Location'=> $url,
	 '-nph'     => $nph);
    unshift(@o,'-Target'=>$target) if $target;
    unshift(@o,'-Type'=>'');
    my @unescaped;
    unshift(@unescaped,'-Cookie'=>$cookie) if $cookie;
    return $self->header((map {$self->unescapeHTML($_)} @o),@unescaped);
}
END_OF_FUNC


#### Method: start_html
# Canned HTML header
#
# Parameters:
# $title -> (optional) The title for this HTML document (-title)
# $author -> (optional) e-mail address of the author (-author)
# $base -> (optional) if set to true, will enter the BASE address of this document
#          for resolving relative references (-base) 
# $xbase -> (optional) alternative base at some remote location (-xbase)
# $target -> (optional) target window to load all links into (-target)
# $script -> (option) Javascript code (-script)
# $no_script -> (option) Javascript <noscript> tag (-noscript)
# $meta -> (optional) Meta information tags
# $head -> (optional) any other elements you'd like to incorporate into the <head> tag
#           (a scalar or array ref)
# $style -> (optional) reference to an external style sheet
# @other -> (optional) any other named parameters you'd like to incorporate into
#           the <body> tag.
####
'start_html' => <<'END_OF_FUNC',
sub start_html {
    my($self,@p) = &self_or_default(@_);
    my($title,$author,$base,$xbase,$script,$noscript,
        $target,$meta,$head,$style,$dtd,$lang,$encoding,$declare_xml,@other) = 
	rearrange([TITLE,AUTHOR,BASE,XBASE,SCRIPT,NOSCRIPT,TARGET,
                   META,HEAD,STYLE,DTD,LANG,ENCODING,DECLARE_XML],@p);

    $self->element_id(0);
    $self->element_tab(0);

    $encoding = lc($self->charset) unless defined $encoding;

    # Need to sort out the DTD before it's okay to call escapeHTML().
    my(@result,$xml_dtd);
    if ($dtd) {
        if (defined(ref($dtd)) and (ref($dtd) eq 'ARRAY')) {
            $dtd = $DEFAULT_DTD unless $dtd->[0] =~ m|^-//|;
        } else {
            $dtd = $DEFAULT_DTD unless $dtd =~ m|^-//|;
        }
    } else {
        $dtd = $XHTML ? XHTML_DTD : $DEFAULT_DTD;
    }

    $xml_dtd++ if ref($dtd) eq 'ARRAY' && $dtd->[0] =~ /\bXHTML\b/i;
    $xml_dtd++ if ref($dtd) eq '' && $dtd =~ /\bXHTML\b/i;
    push @result,qq(<?xml version="1.0" encoding="$encoding"?>) if $xml_dtd && $declare_xml;

    if (ref($dtd) && ref($dtd) eq 'ARRAY') {
        push(@result,qq(<!DOCTYPE html\n\tPUBLIC "$dtd->[0]"\n\t "$dtd->[1]">));
	$DTD_PUBLIC_IDENTIFIER = $dtd->[0];
    } else {
        push(@result,qq(<!DOCTYPE html\n\tPUBLIC "$dtd">));
	$DTD_PUBLIC_IDENTIFIER = $dtd;
    }

    # Now that we know whether we're using the HTML 3.2 DTD or not, it's okay to
    # call escapeHTML().  Strangely enough, the title needs to be escaped as
    # HTML while the author needs to be escaped as a URL.
    $title = $self->escapeHTML($title || 'Untitled Document');
    $author = $self->escape($author);

    if ($DTD_PUBLIC_IDENTIFIER =~ /[^X]HTML (2\.0|3\.2)/i) {
	$lang = "" unless defined $lang;
	$XHTML = 0;
    }
    else {
	$lang = 'en-US' unless defined $lang;
    }

    my $lang_bits = $lang ne '' ? qq( lang="$lang" xml:lang="$lang") : '';
    my $meta_bits = qq(<meta http-equiv="Content-Type" content="text/html; charset=$encoding" />) 
                    if $XHTML && $encoding && !$declare_xml;

    push(@result,$XHTML ? qq(<html xmlns="http://www.w3.org/1999/xhtml"$lang_bits>\n<head>\n<title>$title</title>)
                        : ($lang ? qq(<html lang="$lang">) : "<html>")
	                  . "<head><title>$title</title>");
	if (defined $author) {
    push(@result,$XHTML ? "<link rev=\"made\" href=\"mailto:$author\" />"
			: "<link rev=\"made\" href=\"mailto:$author\">");
	}

    if ($base || $xbase || $target) {
	my $href = $xbase || $self->url('-path'=>1);
	my $t = $target ? qq/ target="$target"/ : '';
	push(@result,$XHTML ? qq(<base href="$href"$t />) : qq(<base href="$href"$t>));
    }

    if ($meta && ref($meta) && (ref($meta) eq 'HASH')) {
	foreach (keys %$meta) { push(@result,$XHTML ? qq(<meta name="$_" content="$meta->{$_}" />) 
			: qq(<meta name="$_" content="$meta->{$_}">)); }
    }

    push(@result,ref($head) ? @$head : $head) if $head;

    # handle the infrequently-used -style and -script parameters
    push(@result,$self->_style($style))   if defined $style;
    push(@result,$self->_script($script)) if defined $script;
    push(@result,$meta_bits)              if defined $meta_bits;

    # handle -noscript parameter
    push(@result,<<END) if $noscript;
<noscript>
$noscript
</noscript>
END
    ;
    my($other) = @other ? " @other" : '';
    push(@result,"</head>\n<body$other>\n");
    return join("\n",@result);
}
END_OF_FUNC

### Method: _style
# internal method for generating a CSS style section
####
'_style' => <<'END_OF_FUNC',
sub _style {
    my ($self,$style) = @_;
    my (@result);

    my $type = 'text/css';
    my $rel  = 'stylesheet';


    my $cdata_start = $XHTML ? "\n<!--/* <![CDATA[ */" : "\n<!-- ";
    my $cdata_end   = $XHTML ? "\n/* ]]> */-->\n" : " -->\n";

    my @s = ref($style) eq 'ARRAY' ? @$style : $style;
    my $other = '';

    for my $s (@s) {
      if (ref($s)) {
       my($src,$code,$verbatim,$stype,$alternate,$foo,@other) =
           rearrange([qw(SRC CODE VERBATIM TYPE ALTERNATE FOO)],
                      ('-foo'=>'bar',
                       ref($s) eq 'ARRAY' ? @$s : %$s));
       my $type = defined $stype ? $stype : 'text/css';
       my $rel  = $alternate ? 'alternate stylesheet' : 'stylesheet';
       $other = "@other" if @other;

       if (ref($src) eq "ARRAY") # Check to see if the $src variable is an array reference
       { # If it is, push a LINK tag for each one
           foreach $src (@$src)
         {
           push(@result,$XHTML ? qq(<link rel="$rel" type="$type" href="$src" $other/>)
                             : qq(<link rel="$rel" type="$type" href="$src"$other>)) if $src;
         }
       }
       else
       { # Otherwise, push the single -src, if it exists.
         push(@result,$XHTML ? qq(<link rel="$rel" type="$type" href="$src" $other/>)
                             : qq(<link rel="$rel" type="$type" href="$src"$other>)
              ) if $src;
        }
     if ($verbatim) {
           my @v = ref($verbatim) eq 'ARRAY' ? @$verbatim : $verbatim;
           push(@result, "<style type=\"text/css\">\n$_\n</style>") foreach @v;
      }
      my @c = ref($code) eq 'ARRAY' ? @$code : $code if $code;
      push(@result,style({'type'=>$type},"$cdata_start\n$_\n$cdata_end")) foreach @c;

      } else {
           my $src = $s;
           push(@result,$XHTML ? qq(<link rel="$rel" type="$type" href="$src" $other/>)
                               : qq(<link rel="$rel" type="$type" href="$src"$other>));
      }
    }
    @result;
}
END_OF_FUNC

'_script' => <<'END_OF_FUNC',
sub _script {
    my ($self,$script) = @_;
    my (@result);

    my (@scripts) = ref($script) eq 'ARRAY' ? @$script : ($script);
    foreach $script (@scripts) {
	my($src,$code,$language);
	if (ref($script)) { # script is a hash
	    ($src,$code,$type) =
		rearrange(['SRC','CODE',['LANGUAGE','TYPE']],
				 '-foo'=>'bar',	# a trick to allow the '-' to be omitted
				 ref($script) eq 'ARRAY' ? @$script : %$script);
            $type ||= 'text/javascript';
            unless ($type =~ m!\w+/\w+!) {
                $type =~ s/[\d.]+$//;
                $type = "text/$type";
            }
	} else {
	    ($src,$code,$type) = ('',$script, 'text/javascript');
	}

    my $comment = '//';  # javascript by default
    $comment = '#' if $type=~/perl|tcl/i;
    $comment = "'" if $type=~/vbscript/i;

    my ($cdata_start,$cdata_end);
    if ($XHTML) {
       $cdata_start    = "$comment<![CDATA[\n";
       $cdata_end     .= "\n$comment]]>";
    } else {
       $cdata_start  =  "\n<!-- Hide script\n";
       $cdata_end    = $comment;
       $cdata_end   .= " End script hiding -->\n";
   }
     my(@satts);
     push(@satts,'src'=>$src) if $src;
     push(@satts,'type'=>$type);
     $code = $cdata_start . $code . $cdata_end if defined $code;
     push(@result,$self->script({@satts},$code || ''));
    }
    @result;
}
END_OF_FUNC

#### Method: end_html
# End an HTML document.
# Trivial method for completeness.  Just returns "</body>"
####
'end_html' => <<'END_OF_FUNC',
sub end_html {
    return "\n</body>\n</html>";
}
END_OF_FUNC


################################
# METHODS USED IN BUILDING FORMS
################################

#### Method: isindex
# Just prints out the isindex tag.
# Parameters:
#  $action -> optional URL of script to run
# Returns:
#   A string containing a <isindex> tag
'isindex' => <<'END_OF_FUNC',
sub isindex {
    my($self,@p) = self_or_default(@_);
    my($action,@other) = rearrange([ACTION],@p);
    $action = qq/ action="$action"/ if $action;
    my($other) = @other ? " @other" : '';
    return $XHTML ? "<isindex$action$other />" : "<isindex$action$other>";
}
END_OF_FUNC


#### Method: startform
# Start a form
# Parameters:
#   $method -> optional submission method to use (GET or POST)
#   $action -> optional URL of script to run
#   $enctype ->encoding to use (URL_ENCODED or MULTIPART)
'startform' => <<'END_OF_FUNC',
sub startform {
    my($self,@p) = self_or_default(@_);

    my($method,$action,$enctype,@other) = 
	rearrange([METHOD,ACTION,ENCTYPE],@p);

    $method  = $self->escapeHTML(lc($method || 'post'));
    $enctype = $self->escapeHTML($enctype || &URL_ENCODED);
    if (defined $action) {
       $action = $self->escapeHTML($action);
    }
    else {
       $action = $self->escapeHTML($self->request_uri || $self->self_url);
    }
    $action = qq(action="$action");
    my($other) = @other ? " @other" : '';
    $self->{'.parametersToAdd'}={};
    return qq/<form method="$method" $action enctype="$enctype"$other>\n/;
}
END_OF_FUNC


#### Method: start_form
# synonym for startform
'start_form' => <<'END_OF_FUNC',
sub start_form {
    $XHTML ? &start_multipart_form : &startform;
}
END_OF_FUNC

'end_multipart_form' => <<'END_OF_FUNC',
sub end_multipart_form {
    &endform;
}
END_OF_FUNC

#### Method: start_multipart_form
# synonym for startform
'start_multipart_form' => <<'END_OF_FUNC',
sub start_multipart_form {
    my($self,@p) = self_or_default(@_);
    if (defined($p[0]) && substr($p[0],0,1) eq '-') {
      return $self->startform(-enctype=>&MULTIPART,@p);
    } else {
	my($method,$action,@other) = 
	    rearrange([METHOD,ACTION],@p);
	return $self->startform($method,$action,&MULTIPART,@other);
    }
}
END_OF_FUNC


#### Method: endform
# End a form
'endform' => <<'END_OF_FUNC',
sub endform {
    my($self,@p) = self_or_default(@_);
    if ( $NOSTICKY ) {
    return wantarray ? ("</form>") : "\n</form>";
    } else {
      if (my @fields = $self->get_fields) {
         return wantarray ? ("<div>",@fields,"</div>","</form>")
                          : "<div>".(join '',@fields)."</div>\n</form>";
      } else {
         return "</form>";
      }
    }
}
END_OF_FUNC


'_textfield' => <<'END_OF_FUNC',
sub _textfield {
    my($self,$tag,@p) = self_or_default(@_);
    my($name,$default,$size,$maxlength,$override,$tabindex,@other) = 
	rearrange([NAME,[DEFAULT,VALUE,VALUES],SIZE,MAXLENGTH,[OVERRIDE,FORCE],TABINDEX],@p);

    my $current = $override ? $default : 
	(defined($self->param($name)) ? $self->param($name) : $default);

    $current = defined($current) ? $self->escapeHTML($current,1) : '';
    $name = defined($name) ? $self->escapeHTML($name) : '';
    my($s) = defined($size) ? qq/ size="$size"/ : '';
    my($m) = defined($maxlength) ? qq/ maxlength="$maxlength"/ : '';
    my($other) = @other ? " @other" : '';
    # this entered at cristy's request to fix problems with file upload fields
    # and WebTV -- not sure it won't break stuff
    my($value) = $current ne '' ? qq(value="$current") : '';
    $tabindex = $self->element_tab($tabindex);
    return $XHTML ? qq(<input type="$tag" name="$name" $tabindex$value$s$m$other />) 
                  : qq(<input type="$tag" name="$name" $value$s$m$other>);
}
END_OF_FUNC

#### Method: textfield
# Parameters:
#   $name -> Name of the text field
#   $default -> Optional default value of the field if not
#                already defined.
#   $size ->  Optional width of field in characaters.
#   $maxlength -> Optional maximum number of characters.
# Returns:
#   A string containing a <input type="text"> field
#
'textfield' => <<'END_OF_FUNC',
sub textfield {
    my($self,@p) = self_or_default(@_);
    $self->_textfield('text',@p);
}
END_OF_FUNC


#### Method: filefield
# Parameters:
#   $name -> Name of the file upload field
#   $size ->  Optional width of field in characaters.
#   $maxlength -> Optional maximum number of characters.
# Returns:
#   A string containing a <input type="file"> field
#
'filefield' => <<'END_OF_FUNC',
sub filefield {
    my($self,@p) = self_or_default(@_);
    $self->_textfield('file',@p);
}
END_OF_FUNC


#### Method: password
# Create a "secret password" entry field
# Parameters:
#   $name -> Name of the field
#   $default -> Optional default value of the field if not
#                already defined.
#   $size ->  Optional width of field in characters.
#   $maxlength -> Optional maximum characters that can be entered.
# Returns:
#   A string containing a <input type="password"> field
#
'password_field' => <<'END_OF_FUNC',
sub password_field {
    my ($self,@p) = self_or_default(@_);
    $self->_textfield('password',@p);
}
END_OF_FUNC

#### Method: textarea
# Parameters:
#   $name -> Name of the text field
#   $default -> Optional default value of the field if not
#                already defined.
#   $rows ->  Optional number of rows in text area
#   $columns -> Optional number of columns in text area
# Returns:
#   A string containing a <textarea></textarea> tag
#
'textarea' => <<'END_OF_FUNC',
sub textarea {
    my($self,@p) = self_or_default(@_);
    my($name,$default,$rows,$cols,$override,$tabindex,@other) =
	rearrange([NAME,[DEFAULT,VALUE],ROWS,[COLS,COLUMNS],[OVERRIDE,FORCE],TABINDEX],@p);

    my($current)= $override ? $default :
	(defined($self->param($name)) ? $self->param($name) : $default);

    $name = defined($name) ? $self->escapeHTML($name) : '';
    $current = defined($current) ? $self->escapeHTML($current) : '';
    my($r) = $rows ? qq/ rows="$rows"/ : '';
    my($c) = $cols ? qq/ cols="$cols"/ : '';
    my($other) = @other ? " @other" : '';
    $tabindex = $self->element_tab($tabindex);
    return qq{<textarea name="$name" $tabindex$r$c$other>$current</textarea>};
}
END_OF_FUNC


#### Method: button
# Create a javascript button.
# Parameters:
#   $name ->  (optional) Name for the button. (-name)
#   $value -> (optional) Value of the button when selected (and visible name) (-value)
#   $onclick -> (optional) Text of the JavaScript to run when the button is
#                clicked.
# Returns:
#   A string containing a <input type="button"> tag
####
'button' => <<'END_OF_FUNC',
sub button {
    my($self,@p) = self_or_default(@_);

    my($label,$value,$script,$tabindex,@other) = rearrange([NAME,[VALUE,LABEL],
						            [ONCLICK,SCRIPT],TABINDEX],@p);

    $label=$self->escapeHTML($label);
    $value=$self->escapeHTML($value,1);
    $script=$self->escapeHTML($script);

    my($name) = '';
    $name = qq/ name="$label"/ if $label;
    $value = $value || $label;
    my($val) = '';
    $val = qq/ value="$value"/ if $value;
    $script = qq/ onclick="$script"/ if $script;
    my($other) = @other ? " @other" : '';
    $tabindex = $self->element_tab($tabindex);
    return $XHTML ? qq(<input type="button" $tabindex$name$val$script$other />)
                  : qq(<input type="button"$name$val$script$other>);
}
END_OF_FUNC


#### Method: submit
# Create a "submit query" button.
# Parameters:
#   $name ->  (optional) Name for the button.
#   $value -> (optional) Value of the button when selected (also doubles as label).
#   $label -> (optional) Label printed on the button(also doubles as the value).
# Returns:
#   A string containing a <input type="submit"> tag
####
'submit' => <<'END_OF_FUNC',
sub submit {
    my($self,@p) = self_or_default(@_);

    my($label,$value,$tabindex,@other) = rearrange([NAME,[VALUE,LABEL],TABINDEX],@p);

    $label=$self->escapeHTML($label);
    $value=$self->escapeHTML($value,1);

    my $name = $NOSTICKY ? '' : 'name=".submit" ';
    $name = qq/name="$label" / if defined($label);
    $value = defined($value) ? $value : $label;
    my $val = '';
    $val = qq/value="$value" / if defined($value);
    $tabindex = $self->element_tab($tabindex);
    my($other) = @other ? "@other " : '';
    return $XHTML ? qq(<input type="submit" $tabindex$name$val$other/>)
                  : qq(<input type="submit" $name$val$other>);
}
END_OF_FUNC


#### Method: reset
# Create a "reset" button.
# Parameters:
#   $name -> (optional) Name for the button.
# Returns:
#   A string containing a <input type="reset"> tag
####
'reset' => <<'END_OF_FUNC',
sub reset {
    my($self,@p) = self_or_default(@_);
    my($label,$value,$tabindex,@other) = rearrange(['NAME',['VALUE','LABEL'],TABINDEX],@p);
    $label=$self->escapeHTML($label);
    $value=$self->escapeHTML($value,1);
    my ($name) = ' name=".reset"';
    $name = qq/ name="$label"/ if defined($label);
    $value = defined($value) ? $value : $label;
    my($val) = '';
    $val = qq/ value="$value"/ if defined($value);
    my($other) = @other ? " @other" : '';
    $tabindex = $self->element_tab($tabindex);
    return $XHTML ? qq(<input type="reset" $tabindex$name$val$other />)
                  : qq(<input type="reset"$name$val$other>);
}
END_OF_FUNC


#### Method: defaults
# Create a "defaults" button.
# Parameters:
#   $name -> (optional) Name for the button.
# Returns:
#   A string containing a <input type="submit" name=".defaults"> tag
#
# Note: this button has a special meaning to the initialization script,
# and tells it to ERASE the current query string so that your defaults
# are used again!
####
'defaults' => <<'END_OF_FUNC',
sub defaults {
    my($self,@p) = self_or_default(@_);

    my($label,$tabindex,@other) = rearrange([[NAME,VALUE],TABINDEX],@p);

    $label=$self->escapeHTML($label,1);
    $label = $label || "Defaults";
    my($value) = qq/ value="$label"/;
    my($other) = @other ? " @other" : '';
    $tabindex = $self->element_tab($tabindex);
    return $XHTML ? qq(<input type="submit" name=".defaults" $tabindex$value$other />)
                  : qq/<input type="submit" NAME=".defaults"$value$other>/;
}
END_OF_FUNC


#### Method: comment
# Create an HTML <!-- comment -->
# Parameters: a string
'comment' => <<'END_OF_FUNC',
sub comment {
    my($self,@p) = self_or_CGI(@_);
    return "<!-- @p -->";
}
END_OF_FUNC

#### Method: checkbox
# Create a checkbox that is not logically linked to any others.
# The field value is "on" when the button is checked.
# Parameters:
#   $name -> Name of the checkbox
#   $checked -> (optional) turned on by default if true
#   $value -> (optional) value of the checkbox, 'on' by default
#   $label -> (optional) a user-readable label printed next to the box.
#             Otherwise the checkbox name is used.
# Returns:
#   A string containing a <input type="checkbox"> field
####
'checkbox' => <<'END_OF_FUNC',
sub checkbox {
    my($self,@p) = self_or_default(@_);

    my($name,$checked,$value,$label,$labelattributes,$override,$tabindex,@other) =
       rearrange([NAME,[CHECKED,SELECTED,ON],VALUE,LABEL,LABELATTRIBUTES,
                   [OVERRIDE,FORCE],TABINDEX],@p);

    $value = defined $value ? $value : 'on';

    if (!$override && ($self->{'.fieldnames'}->{$name} || 
		       defined $self->param($name))) {
	$checked = grep($_ eq $value,$self->param($name)) ? $self->_checked(1) : '';
    } else {
	$checked = $self->_checked($checked);
    }
    my($the_label) = defined $label ? $label : $name;
    $name = $self->escapeHTML($name);
    $value = $self->escapeHTML($value,1);
    $the_label = $self->escapeHTML($the_label);
    my($other) = @other ? "@other " : '';
    $tabindex = $self->element_tab($tabindex);
    $self->register_parameter($name);
    return $XHTML ? CGI::label($labelattributes,
                    qq{<input type="checkbox" name="$name" value="$value" $tabindex$checked$other/>$the_label})
                  : qq{<input type="checkbox" name="$name" value="$value"$checked$other>$the_label};
}
END_OF_FUNC



# Escape HTML -- used internally
'escapeHTML' => <<'END_OF_FUNC',
sub escapeHTML {
         # hack to work around  earlier hacks
         push @_,$_[0] if @_==1 && $_[0] eq 'CGI';
         my ($self,$toencode,$newlinestoo) = CGI::self_or_default(@_);
         return undef unless defined($toencode);
         return $toencode if ref($self) && !$self->{'escape'};
         $toencode =~ s{&}{&amp;}gso;
         $toencode =~ s{<}{&lt;}gso;
         $toencode =~ s{>}{&gt;}gso;
	 if ($DTD_PUBLIC_IDENTIFIER =~ /[^X]HTML 3\.2/i) {
	     # $quot; was accidentally omitted from the HTML 3.2 DTD -- see
	     # <http://validator.w3.org/docs/errors.html#bad-entity> /
	     # <http://lists.w3.org/Archives/Public/www-html/1997Mar/0003.html>.
	     $toencode =~ s{"}{&#34;}gso;
         }
         else {
	     $toencode =~ s{"}{&quot;}gso;
         }
         # Handle bug in some browsers with Latin charsets
         if ($self->{'.charset'} &&
             (uc($self->{'.charset'}) eq 'ISO-8859-1' ||
              uc($self->{'.charset'}) eq 'WINDOWS-1252'))
         {
                $toencode =~ s{'}{&#39;}gso;
                $toencode =~ s{\x8b}{&#8249;}gso;
                $toencode =~ s{\x9b}{&#8250;}gso;
                if (defined $newlinestoo && $newlinestoo) {
                     $toencode =~ s{\012}{&#10;}gso;
                     $toencode =~ s{\015}{&#13;}gso;
                }
         }
         return $toencode;
}
END_OF_FUNC

# unescape HTML -- used internally
'unescapeHTML' => <<'END_OF_FUNC',
sub unescapeHTML {
    # hack to work around  earlier hacks
    push @_,$_[0] if @_==1 && $_[0] eq 'CGI';
    my ($self,$string) = CGI::self_or_default(@_);
    return undef unless defined($string);
    my $latin = defined $self->{'.charset'} ? $self->{'.charset'} =~ /^(ISO-8859-1|WINDOWS-1252)$/i
                                            : 1;
    # thanks to Randal Schwartz for the correct solution to this one
    $string=~ s[&(.*?);]{
	local $_ = $1;
	/^amp$/i	? "&" :
	/^quot$/i	? '"' :
        /^gt$/i		? ">" :
	/^lt$/i		? "<" :
	/^#(\d+)$/ && $latin	     ? chr($1) :
	/^#x([0-9a-f]+)$/i && $latin ? chr(hex($1)) :
	$_
	}gex;
    return $string;
}
END_OF_FUNC

# Internal procedure - don't use
'_tableize' => <<'END_OF_FUNC',
sub _tableize {
    my($rows,$columns,$rowheaders,$colheaders,@elements) = @_;
    my @rowheaders = $rowheaders ? @$rowheaders : ();
    my @colheaders = $colheaders ? @$colheaders : ();
    my($result);

    if (defined($columns)) {
	$rows = int(0.99 + @elements/$columns) unless defined($rows);
    }
    if (defined($rows)) {
	$columns = int(0.99 + @elements/$rows) unless defined($columns);
    }

    # rearrange into a pretty table
    $result = "<table>";
    my($row,$column);
    unshift(@colheaders,'') if @colheaders && @rowheaders;
    $result .= "<tr>" if @colheaders;
    foreach (@colheaders) {
	$result .= "<th>$_</th>";
    }
    for ($row=0;$row<$rows;$row++) {
	$result .= "<tr>";
	$result .= "<th>$rowheaders[$row]</th>" if @rowheaders;
	for ($column=0;$column<$columns;$column++) {
	    $result .= "<td>" . $elements[$column*$rows + $row] . "</td>"
		if defined($elements[$column*$rows + $row]);
	}
	$result .= "</tr>";
    }
    $result .= "</table>";
    return $result;
}
END_OF_FUNC


#### Method: radio_group
# Create a list of logically-linked radio buttons.
# Parameters:
#   $name -> Common name for all the buttons.
#   $values -> A pointer to a regular array containing the
#             values for each button in the group.
#   $default -> (optional) Value of the button to turn on by default.  Pass '-'
#               to turn _nothing_ on.
#   $linebreak -> (optional) Set to true to place linebreaks
#             between the buttons.
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   An ARRAY containing a series of <input type="radio"> fields
####
'radio_group' => <<'END_OF_FUNC',
sub radio_group {
    my($self,@p) = self_or_default(@_);
   $self->_box_group('radio',@p);
}
END_OF_FUNC

#### Method: checkbox_group
# Create a list of logically-linked checkboxes.
# Parameters:
#   $name -> Common name for all the check boxes
#   $values -> A pointer to a regular array containing the
#             values for each checkbox in the group.
#   $defaults -> (optional)
#             1. If a pointer to a regular array of checkbox values,
#             then this will be used to decide which
#             checkboxes to turn on by default.
#             2. If a scalar, will be assumed to hold the
#             value of a single checkbox in the group to turn on. 
#   $linebreak -> (optional) Set to true to place linebreaks
#             between the buttons.
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   An ARRAY containing a series of <input type="checkbox"> fields
####

'checkbox_group' => <<'END_OF_FUNC',
sub checkbox_group {
    my($self,@p) = self_or_default(@_);
   $self->_box_group('checkbox',@p);
}
END_OF_FUNC

'_box_group' => <<'END_OF_FUNC',
sub _box_group {
    my $self     = shift;
    my $box_type = shift;

    my($name,$values,$defaults,$linebreak,$labels,$labelattributes,
       $attributes,$rows,$columns,$rowheaders,$colheaders,
       $override,$nolabels,$tabindex,$disabled,@other) =
        rearrange([NAME,[VALUES,VALUE],[DEFAULT,DEFAULTS],LINEBREAK,LABELS,LABELATTRIBUTES,
                       ATTRIBUTES,ROWS,[COLUMNS,COLS],[ROWHEADERS,ROWHEADER],[COLHEADERS,COLHEADER],
                       [OVERRIDE,FORCE],NOLABELS,TABINDEX,DISABLED
                  ],@_);


    my($result,$checked,@elements,@values);

    @values = $self->_set_values_and_labels($values,\$labels,$name);
    my %checked = $self->previous_or_default($name,$defaults,$override);

    # If no check array is specified, check the first by default
    $checked{$values[0]}++ if $box_type eq 'radio' && !%checked;

    $name=$self->escapeHTML($name);

    my %tabs = ();
    if ($TABINDEX && $tabindex) {
      if (!ref $tabindex) {
          $self->element_tab($tabindex);
      } elsif (ref $tabindex eq 'ARRAY') {
          %tabs = map {$_=>$self->element_tab} @$tabindex;
      } elsif (ref $tabindex eq 'HASH') {
          %tabs = %$tabindex;
      }
    }
    %tabs = map {$_=>$self->element_tab} @values unless %tabs;
    my $other = @other ? "@other " : '';
    my $radio_checked;

    # for disabling groups of radio/checkbox buttons
    my %disabled;
    foreach (@{$disabled}) {
   	$disabled{$_}=1;
    }

    foreach (@values) {
    	 my $disable="";
	 if ($disabled{$_}) {
		$disable="disabled='1'";
	 }

        my $checkit = $self->_checked($box_type eq 'radio' ? ($checked{$_} && !$radio_checked++)
                                                           : $checked{$_});
	my($break);
	if ($linebreak) {
          $break = $XHTML ? "<br />" : "<br>";
	}
	else {
	  $break = '';
	}
	my($label)='';
	unless (defined($nolabels) && $nolabels) {
	    $label = $_;
	    $label = $labels->{$_} if defined($labels) && defined($labels->{$_});
	    $label = $self->escapeHTML($label,1);
            $label = "<span style=\"color:gray\">$label</span>" if $disabled{$_};
	}
        my $attribs = $self->_set_attributes($_, $attributes);
        my $tab     = $tabs{$_};
	$_=$self->escapeHTML($_);

        if ($XHTML) {
           push @elements,
              CGI::label($labelattributes,
                   qq(<input type="$box_type" name="$name" value="$_" $checkit$other$tab$attribs$disable/>$label)).${break};
        } else {
            push(@elements,qq/<input type="$box_type" name="$name" value="$_"$checkit$other$tab$attribs$disable>${label}${break}/);
        }
    }
    $self->register_parameter($name);
    return wantarray ? @elements : "@elements"
           unless defined($columns) || defined($rows);
    return _tableize($rows,$columns,$rowheaders,$colheaders,@elements);
}
END_OF_FUNC


#### Method: popup_menu
# Create a popup menu.
# Parameters:
#   $name -> Name for all the menu
#   $values -> A pointer to a regular array containing the
#             text of each menu item.
#   $default -> (optional) Default item to display
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   A string containing the definition of a popup menu.
####
'popup_menu' => <<'END_OF_FUNC',
sub popup_menu {
    my($self,@p) = self_or_default(@_);

    my($name,$values,$default,$labels,$attributes,$override,$tabindex,@other) =
       rearrange([NAME,[VALUES,VALUE],[DEFAULT,DEFAULTS],LABELS,
       ATTRIBUTES,[OVERRIDE,FORCE],TABINDEX],@p);
    my($result,$selected);

    if (!$override && defined($self->param($name))) {
	$selected = $self->param($name);
    } else {
	$selected = $default;
    }
    $name=$self->escapeHTML($name);
    my($other) = @other ? " @other" : '';

    my(@values);
    @values = $self->_set_values_and_labels($values,\$labels,$name);
    $tabindex = $self->element_tab($tabindex);
    $result = qq/<select name="$name" $tabindex$other>\n/;
    foreach (@values) {
        if (/<optgroup/) {
            foreach (split(/\n/)) {
                my $selectit = $XHTML ? 'selected="selected"' : 'selected';
                s/(value="$selected")/$selectit $1/ if defined $selected;
                $result .= "$_\n";
            }
        }
        else {
          my $attribs = $self->_set_attributes($_, $attributes);
	  my($selectit) = defined($selected) ? $self->_selected($selected eq $_) : '';
	  my($label) = $_;
	  $label = $labels->{$_} if defined($labels) && defined($labels->{$_});
	  my($value) = $self->escapeHTML($_);
	  $label=$self->escapeHTML($label,1);
          $result .= "<option${attribs} ${selectit}value=\"$value\">$label</option>\n";
        }
    }

    $result .= "</select>";
    return $result;
}
END_OF_FUNC


#### Method: optgroup
# Create a optgroup.
# Parameters:
#   $name -> Label for the group
#   $values -> A pointer to a regular array containing the
#              values for each option line in the group.
#   $labels -> (optional)
#              A pointer to an associative array of labels to print next to each item
#              in the form $label{'value'}="Long explanatory label".
#              Otherwise the provided values are used as the labels.
#   $labeled -> (optional)
#               A true value indicates the value should be used as the label attribute
#               in the option elements.
#               The label attribute specifies the option label presented to the user.
#               This defaults to the content of the <option> element, but the label
#               attribute allows authors to more easily use optgroup without sacrificing
#               compatibility with browsers that do not support option groups.
#   $novals -> (optional)
#              A true value indicates to suppress the val attribute in the option elements
# Returns:
#   A string containing the definition of an option group.
####
'optgroup' => <<'END_OF_FUNC',
sub optgroup {
    my($self,@p) = self_or_default(@_);
    my($name,$values,$attributes,$labeled,$noval,$labels,@other)
        = rearrange([NAME,[VALUES,VALUE],ATTRIBUTES,LABELED,NOVALS,LABELS],@p);

    my($result,@values);
    @values = $self->_set_values_and_labels($values,\$labels,$name,$labeled,$novals);
    my($other) = @other ? " @other" : '';

    $name=$self->escapeHTML($name);
    $result = qq/<optgroup label="$name"$other>\n/;
    foreach (@values) {
        if (/<optgroup/) {
            foreach (split(/\n/)) {
                my $selectit = $XHTML ? 'selected="selected"' : 'selected';
                s/(value="$selected")/$selectit $1/ if defined $selected;
                $result .= "$_\n";
            }
        }
        else {
            my $attribs = $self->_set_attributes($_, $attributes);
            my($label) = $_;
            $label = $labels->{$_} if defined($labels) && defined($labels->{$_});
            $label=$self->escapeHTML($label);
            my($value)=$self->escapeHTML($_,1);
            $result .= $labeled ? $novals ? "<option$attribs label=\"$value\">$label</option>\n"
                                          : "<option$attribs label=\"$value\" value=\"$value\">$label</option>\n"
                                : $novals ? "<option$attribs>$label</option>\n"
                                          : "<option$attribs value=\"$value\">$label</option>\n";
        }
    }
    $result .= "</optgroup>";
    return $result;
}
END_OF_FUNC


#### Method: scrolling_list
# Create a scrolling list.
# Parameters:
#   $name -> name for the list
#   $values -> A pointer to a regular array containing the
#             values for each option line in the list.
#   $defaults -> (optional)
#             1. If a pointer to a regular array of options,
#             then this will be used to decide which
#             lines to turn on by default.
#             2. Otherwise holds the value of the single line to turn on.
#   $size -> (optional) Size of the list.
#   $multiple -> (optional) If set, allow multiple selections.
#   $labels -> (optional)
#             A pointer to an associative array of labels to print next to each checkbox
#             in the form $label{'value'}="Long explanatory label".
#             Otherwise the provided values are used as the labels.
# Returns:
#   A string containing the definition of a scrolling list.
####
'scrolling_list' => <<'END_OF_FUNC',
sub scrolling_list {
    my($self,@p) = self_or_default(@_);
    my($name,$values,$defaults,$size,$multiple,$labels,$attributes,$override,$tabindex,@other)
	= rearrange([NAME,[VALUES,VALUE],[DEFAULTS,DEFAULT],
          SIZE,MULTIPLE,LABELS,ATTRIBUTES,[OVERRIDE,FORCE],TABINDEX],@p);

    my($result,@values);
    @values = $self->_set_values_and_labels($values,\$labels,$name);

    $size = $size || scalar(@values);

    my(%selected) = $self->previous_or_default($name,$defaults,$override);

    my($is_multiple) = $multiple ? qq/ multiple="multiple"/ : '';
    my($has_size) = $size ? qq/ size="$size"/: '';
    my($other) = @other ? " @other" : '';

    $name=$self->escapeHTML($name);
    $tabindex = $self->element_tab($tabindex);
    $result = qq/<select name="$name" $tabindex$has_size$is_multiple$other>\n/;
    foreach (@values) {
	my($selectit) = $self->_selected($selected{$_});
	my($label) = $_;
	$label = $labels->{$_} if defined($labels) && defined($labels->{$_});
	$label=$self->escapeHTML($label);
	my($value)=$self->escapeHTML($_,1);
        my $attribs = $self->_set_attributes($_, $attributes);
        $result .= "<option ${selectit}${attribs}value=\"$value\">$label</option>\n";
    }
    $result .= "</select>";
    $self->register_parameter($name);
    return $result;
}
END_OF_FUNC


#### Method: hidden
# Parameters:
#   $name -> Name of the hidden field
#   @default -> (optional) Initial values of field (may be an array)
#      or
#   $default->[initial values of field]
# Returns:
#   A string containing a <input type="hidden" name="name" value="value">
####
'hidden' => <<'END_OF_FUNC',
sub hidden {
    my($self,@p) = self_or_default(@_);

    # this is the one place where we departed from our standard
    # calling scheme, so we have to special-case (darn)
    my(@result,@value);
    my($name,$default,$override,@other) = 
	rearrange([NAME,[DEFAULT,VALUE,VALUES],[OVERRIDE,FORCE]],@p);

    my $do_override = 0;
    if ( ref($p[0]) || substr($p[0],0,1) eq '-') {
	@value = ref($default) ? @{$default} : $default;
	$do_override = $override;
    } else {
	foreach ($default,$override,@other) {
	    push(@value,$_) if defined($_);
	}
    }

    # use previous values if override is not set
    my @prev = $self->param($name);
    @value = @prev if !$do_override && @prev;

    $name=$self->escapeHTML($name);
    foreach (@value) {
	$_ = defined($_) ? $self->escapeHTML($_,1) : '';
	push @result,$XHTML ? qq(<input type="hidden" name="$name" value="$_" @other />)
                            : qq(<input type="hidden" name="$name" value="$_" @other>);
    }
    return wantarray ? @result : join('',@result);
}
END_OF_FUNC


#### Method: image_button
# Parameters:
#   $name -> Name of the button
#   $src ->  URL of the image source
#   $align -> Alignment style (TOP, BOTTOM or MIDDLE)
# Returns:
#   A string containing a <input type="image" name="name" src="url" align="alignment">
####
'image_button' => <<'END_OF_FUNC',
sub image_button {
    my($self,@p) = self_or_default(@_);

    my($name,$src,$alignment,@other) =
	rearrange([NAME,SRC,ALIGN],@p);

    my($align) = $alignment ? " align=\L\"$alignment\"" : '';
    my($other) = @other ? " @other" : '';
    $name=$self->escapeHTML($name);
    return $XHTML ? qq(<input type="image" name="$name" src="$src"$align$other />)
                  : qq/<input type="image" name="$name" src="$src"$align$other>/;
}
END_OF_FUNC


#### Method: self_url
# Returns a URL containing the current script and all its
# param/value pairs arranged as a query.  You can use this
# to create a link that, when selected, will reinvoke the
# script with all its state information preserved.
####
'self_url' => <<'END_OF_FUNC',
sub self_url {
    my($self,@p) = self_or_default(@_);
    return $self->url('-path_info'=>1,'-query'=>1,'-full'=>1,@p);
}
END_OF_FUNC


# This is provided as a synonym to self_url() for people unfortunate
# enough to have incorporated it into their programs already!
'state' => <<'END_OF_FUNC',
sub state {
    &self_url;
}
END_OF_FUNC


#### Method: url
# Like self_url, but doesn't return the query string part of
# the URL.
####
'url' => <<'END_OF_FUNC',
sub url {
    my($self,@p) = self_or_default(@_);
    my ($relative,$absolute,$full,$path_info,$query,$base,$rewrite) = 
	rearrange(['RELATIVE','ABSOLUTE','FULL',['PATH','PATH_INFO'],['QUERY','QUERY_STRING'],'BASE','REWRITE'],@p);
    my $url  = '';
    $full++      if $base || !($relative || $absolute);
    $rewrite++   unless defined $rewrite;

    my $path        =  $self->path_info;
    my $script_name =  $self->script_name;
    my $request_uri =  unescape($self->request_uri) || '';
    my $query_str   =  $self->query_string;

    my $rewrite_in_use = $request_uri && $request_uri !~ /^\Q$script_name/;
    undef $path if $rewrite_in_use && $rewrite;  # path not valid when rewriting active

    my $uri         =  $rewrite && $request_uri ? $request_uri : $script_name;
    $uri            =~ s/\?.*$//;                                 # remove query string
    $uri            =~ s/\Q$path\E$//      if defined $path;      # remove path

    if ($full) {
	my $protocol = $self->protocol();
	$url = "$protocol://";
	my $vh = http('x_forwarded_host') || http('host') || '';
        $vh =~ s/\:\d+$//;  # some clients add the port number (incorrectly). Get rid of it.
	if ($vh) {
	    $url .= $vh;
	} else {
	    $url .= server_name();
	}
        my $port = $self->server_port;
	$url .= ":" . $port
	  unless (lc($protocol) eq 'http'  && $port == 80)
		|| (lc($protocol) eq 'https' && $port == 443);
        return $url if $base;
	$url .= $uri;
    } elsif ($relative) {
	($url) = $uri =~ m!([^/]+)$!;
    } elsif ($absolute) {
	$url = $uri;
    }

    $url .= $path         if $path_info and defined $path;
    $url .= "?$query_str" if $query     and $query_str ne '';
    $url ||= '';
    $url =~ s/([^a-zA-Z0-9_.%;&?\/\\:+=~-])/sprintf("%%%02X",ord($1))/eg;
    return $url;
}

END_OF_FUNC

#### Method: cookie
# Set or read a cookie from the specified name.
# Cookie can then be passed to header().
# Usual rules apply to the stickiness of -value.
#  Parameters:
#   -name -> name for this cookie (optional)
#   -value -> value of this cookie (scalar, array or hash) 
#   -path -> paths for which this cookie is valid (optional)
#   -domain -> internet domain in which this cookie is valid (optional)
#   -secure -> if true, cookie only passed through secure channel (optional)
#   -expires -> expiry date in format Wdy, DD-Mon-YYYY HH:MM:SS GMT (optional)
####
'cookie' => <<'END_OF_FUNC',
sub cookie {
    my($self,@p) = self_or_default(@_);
    my($name,$value,$path,$domain,$secure,$expires,$httponly) =
	rearrange([NAME,[VALUE,VALUES],PATH,DOMAIN,SECURE,EXPIRES,HTTPONLY],@p);

    require CGI::Cookie;

    # if no value is supplied, then we retrieve the
    # value of the cookie, if any.  For efficiency, we cache the parsed
    # cookies in our state variables.
    unless ( defined($value) ) {
	$self->{'.cookies'} = CGI::Cookie->fetch
	    unless $self->{'.cookies'};

	# If no name is supplied, then retrieve the names of all our cookies.
	return () unless $self->{'.cookies'};
	return keys %{$self->{'.cookies'}} unless $name;
	return () unless $self->{'.cookies'}->{$name};
	return $self->{'.cookies'}->{$name}->value if defined($name) && $name ne '';
    }

    # If we get here, we're creating a new cookie
    return undef unless defined($name) && $name ne '';	# this is an error

    my @param;
    push(@param,'-name'=>$name);
    push(@param,'-value'=>$value);
    push(@param,'-domain'=>$domain) if $domain;
    push(@param,'-path'=>$path) if $path;
    push(@param,'-expires'=>$expires) if $expires;
    push(@param,'-secure'=>$secure) if $secure;
    push(@param,'-httponly'=>$httponly) if $httponly;

    return new CGI::Cookie(@param);
}
END_OF_FUNC

'parse_keywordlist' => <<'END_OF_FUNC',
sub parse_keywordlist {
    my($self,$tosplit) = @_;
    $tosplit = unescape($tosplit); # unescape the keywords
    $tosplit=~tr/+/ /;          # pluses to spaces
    my(@keywords) = split(/\s+/,$tosplit);
    return @keywords;
}
END_OF_FUNC

'param_fetch' => <<'END_OF_FUNC',
sub param_fetch {
    my($self,@p) = self_or_default(@_);
    my($name) = rearrange([NAME],@p);
    unless (exists($self->{$name})) {
	$self->add_parameter($name);
	$self->{$name} = [];
    }
    
    return $self->{$name};
}
END_OF_FUNC

###############################################
# OTHER INFORMATION PROVIDED BY THE ENVIRONMENT
###############################################

#### Method: path_info
# Return the extra virtual path information provided
# after the URL (if any)
####
'path_info' => <<'END_OF_FUNC',
sub path_info {
    my ($self,$info) = self_or_default(@_);
    if (defined($info)) {
	$info = "/$info" if $info ne '' &&  substr($info,0,1) ne '/';
	$self->{'.path_info'} = $info;
    } elsif (! defined($self->{'.path_info'}) ) {
        my (undef,$path_info) = $self->_name_and_path_from_env;
	$self->{'.path_info'} = $path_info || '';
    }
    return $self->{'.path_info'};
}
END_OF_FUNC

# WE USE THIS TO COMPENSATE FOR A BUG IN APACHE 2 PRESENT AT LEAST UP THROUGH 2.0.54
'_name_and_path_from_env' => <<'END_OF_FUNC',
sub _name_and_path_from_env {
   my $self = shift;
   my $raw_script_name = $ENV{SCRIPT_NAME} || '';
   my $raw_path_info   = $ENV{PATH_INFO}   || '';
   my $uri             = unescape($self->request_uri) || '';

   my $protected    = quotemeta($raw_path_info);
   $raw_script_name =~ s/$protected$//;

   my @uri_double_slashes  = $uri =~ m^(/{2,}?)^g;
   my @path_double_slashes = "$raw_script_name $raw_path_info" =~ m^(/{2,}?)^g;

   my $apache_bug      = @uri_double_slashes != @path_double_slashes;
   return ($raw_script_name,$raw_path_info) unless $apache_bug;

   my $path_info_search = quotemeta($raw_path_info);
   $path_info_search    =~ s!/!/+!g;
   if ($uri =~ m/^(.+)($path_info_search)/) {
       return ($1,$2);
   } else {
       return ($raw_script_name,$raw_path_info);
   }
}
END_OF_FUNC


#### Method: request_method
# Returns 'POST', 'GET', 'PUT' or 'HEAD'
####
'request_method' => <<'END_OF_FUNC',
sub request_method {
    return $ENV{'REQUEST_METHOD'};
}
END_OF_FUNC

#### Method: content_type
# Returns the content_type string
####
'content_type' => <<'END_OF_FUNC',
sub content_type {
    return $ENV{'CONTENT_TYPE'};
}
END_OF_FUNC

#### Method: path_translated
# Return the physical path information provided
# by the URL (if any)
####
'path_translated' => <<'END_OF_FUNC',
sub path_translated {
    return $ENV{'PATH_TRANSLATED'};
}
END_OF_FUNC


#### Method: request_uri
# Return the literal request URI
####
'request_uri' => <<'END_OF_FUNC',
sub request_uri {
    return $ENV{'REQUEST_URI'};
}
END_OF_FUNC


#### Method: query_string
# Synthesize a query string from our current
# parameters
####
'query_string' => <<'END_OF_FUNC',
sub query_string {
    my($self) = self_or_default(@_);
    my($param,$value,@pairs);
    foreach $param ($self->param) {
	my($eparam) = escape($param);
	foreach $value ($self->param($param)) {
	    $value = escape($value);
            next unless defined $value;
	    push(@pairs,"$eparam=$value");
	}
    }
    foreach (keys %{$self->{'.fieldnames'}}) {
      push(@pairs,".cgifields=".escape("$_"));
    }
    return join($USE_PARAM_SEMICOLONS ? ';' : '&',@pairs);
}
END_OF_FUNC


#### Method: accept
# Without parameters, returns an array of the
# MIME types the browser accepts.
# With a single parameter equal to a MIME
# type, will return undef if the browser won't
# accept it, 1 if the browser accepts it but
# doesn't give a preference, or a floating point
# value between 0.0 and 1.0 if the browser
# declares a quantitative score for it.
# This handles MIME type globs correctly.
####
'Accept' => <<'END_OF_FUNC',
sub Accept {
    my($self,$search) = self_or_CGI(@_);
    my(%prefs,$type,$pref,$pat);
    
    my(@accept) = split(',',$self->http('accept'));

    foreach (@accept) {
	($pref) = /q=(\d\.\d+|\d+)/;
	($type) = m#(\S+/[^;]+)#;
	next unless $type;
	$prefs{$type}=$pref || 1;
    }

    return keys %prefs unless $search;
    
    # if a search type is provided, we may need to
    # perform a pattern matching operation.
    # The MIME types use a glob mechanism, which
    # is easily translated into a perl pattern match

    # First return the preference for directly supported
    # types:
    return $prefs{$search} if $prefs{$search};

    # Didn't get it, so try pattern matching.
    foreach (keys %prefs) {
	next unless /\*/;       # not a pattern match
	($pat = $_) =~ s/([^\w*])/\\$1/g; # escape meta characters
	$pat =~ s/\*/.*/g; # turn it into a pattern
	return $prefs{$_} if $search=~/$pat/;
    }
}
END_OF_FUNC


#### Method: user_agent
# If called with no parameters, returns the user agent.
# If called with one parameter, does a pattern match (case
# insensitive) on the user agent.
####
'user_agent' => <<'END_OF_FUNC',
sub user_agent {
    my($self,$match)=self_or_CGI(@_);
    return $self->http('user_agent') unless $match;
    return $self->http('user_agent') =~ /$match/i;
}
END_OF_FUNC


#### Method: raw_cookie
# Returns the magic cookies for the session.
# The cookies are not parsed or altered in any way, i.e.
# cookies are returned exactly as given in the HTTP
# headers.  If a cookie name is given, only that cookie's
# value is returned, otherwise the entire raw cookie
# is returned.
####
'raw_cookie' => <<'END_OF_FUNC',
sub raw_cookie {
    my($self,$key) = self_or_CGI(@_);

    require CGI::Cookie;

    if (defined($key)) {
	$self->{'.raw_cookies'} = CGI::Cookie->raw_fetch
	    unless $self->{'.raw_cookies'};

	return () unless $self->{'.raw_cookies'};
	return () unless $self->{'.raw_cookies'}->{$key};
	return $self->{'.raw_cookies'}->{$key};
    }
    return $self->http('cookie') || $ENV{'COOKIE'} || '';
}
END_OF_FUNC

#### Method: virtual_host
# Return the name of the virtual_host, which
# is not always the same as the server
######
'virtual_host' => <<'END_OF_FUNC',
sub virtual_host {
    my $vh = http('x_forwarded_host') || http('host') || server_name();
    $vh =~ s/:\d+$//;		# get rid of port number
    return $vh;
}
END_OF_FUNC

#### Method: remote_host
# Return the name of the remote host, or its IP
# address if unavailable.  If this variable isn't
# defined, it returns "localhost" for debugging
# purposes.
####
'remote_host' => <<'END_OF_FUNC',
sub remote_host {
    return $ENV{'REMOTE_HOST'} || $ENV{'REMOTE_ADDR'} 
    || 'localhost';
}
END_OF_FUNC


#### Method: remote_addr
# Return the IP addr of the remote host.
####
'remote_addr' => <<'END_OF_FUNC',
sub remote_addr {
    return $ENV{'REMOTE_ADDR'} || '127.0.0.1';
}
END_OF_FUNC


#### Method: script_name
# Return the partial URL to this script for
# self-referencing scripts.  Also see
# self_url(), which returns a URL with all state information
# preserved.
####
'script_name' => <<'END_OF_FUNC',
sub script_name {
    my ($self,@p) = self_or_default(@_);
    if (@p) {
        $self->{'.script_name'} = shift @p;
    } elsif (!exists $self->{'.script_name'}) {
        my ($script_name,$path_info) = $self->_name_and_path_from_env();
        $self->{'.script_name'} = $script_name;
    }
    return $self->{'.script_name'};
}
END_OF_FUNC


#### Method: referer
# Return the HTTP_REFERER: useful for generating
# a GO BACK button.
####
'referer' => <<'END_OF_FUNC',
sub referer {
    my($self) = self_or_CGI(@_);
    return $self->http('referer');
}
END_OF_FUNC


#### Method: server_name
# Return the name of the server
####
'server_name' => <<'END_OF_FUNC',
sub server_name {
    return $ENV{'SERVER_NAME'} || 'localhost';
}
END_OF_FUNC

#### Method: server_software
# Return the name of the server software
####
'server_software' => <<'END_OF_FUNC',
sub server_software {
    return $ENV{'SERVER_SOFTWARE'} || 'cmdline';
}
END_OF_FUNC

#### Method: virtual_port
# Return the server port, taking virtual hosts into account
####
'virtual_port' => <<'END_OF_FUNC',
sub virtual_port {
    my($self) = self_or_default(@_);
    my $vh = $self->http('x_forwarded_host') || $self->http('host');
    my $protocol = $self->protocol;
    if ($vh) {
        return ($vh =~ /:(\d+)$/)[0] || ($protocol eq 'https' ? 443 : 80);
    } else {
        return $self->server_port();
    }
}
END_OF_FUNC

#### Method: server_port
# Return the tcp/ip port the server is running on
####
'server_port' => <<'END_OF_FUNC',
sub server_port {
    return $ENV{'SERVER_PORT'} || 80; # for debugging
}
END_OF_FUNC

#### Method: server_protocol
# Return the protocol (usually HTTP/1.0)
####
'server_protocol' => <<'END_OF_FUNC',
sub server_protocol {
    return $ENV{'SERVER_PROTOCOL'} || 'HTTP/1.0'; # for debugging
}
END_OF_FUNC

#### Method: http
# Return the value of an HTTP variable, or
# the list of variables if none provided
####
'http' => <<'END_OF_FUNC',
sub http {
    my ($self,$parameter) = self_or_CGI(@_);
    return $ENV{$parameter} if $parameter=~/^HTTP/;
    $parameter =~ tr/-/_/;
    return $ENV{"HTTP_\U$parameter\E"} if $parameter;
    my(@p);
    foreach (keys %ENV) {
	push(@p,$_) if /^HTTP/;
    }
    return @p;
}
END_OF_FUNC

#### Method: https
# Return the value of HTTPS
####
'https' => <<'END_OF_FUNC',
sub https {
    local($^W)=0;
    my ($self,$parameter) = self_or_CGI(@_);
    return $ENV{HTTPS} unless $parameter;
    return $ENV{$parameter} if $parameter=~/^HTTPS/;
    $parameter =~ tr/-/_/;
    return $ENV{"HTTPS_\U$parameter\E"} if $parameter;
    my(@p);
    foreach (keys %ENV) {
	push(@p,$_) if /^HTTPS/;
    }
    return @p;
}
END_OF_FUNC

#### Method: protocol
# Return the protocol (http or https currently)
####
'protocol' => <<'END_OF_FUNC',
sub protocol {
    local($^W)=0;
    my $self = shift;
    return 'https' if uc($self->https()) eq 'ON'; 
    return 'https' if $self->server_port == 443;
    my $prot = $self->server_protocol;
    my($protocol,$version) = split('/',$prot);
    return "\L$protocol\E";
}
END_OF_FUNC

#### Method: remote_ident
# Return the identity of the remote user
# (but only if his host is running identd)
####
'remote_ident' => <<'END_OF_FUNC',
sub remote_ident {
    return $ENV{'REMOTE_IDENT'};
}
END_OF_FUNC


#### Method: auth_type
# Return the type of use verification/authorization in use, if any.
####
'auth_type' => <<'END_OF_FUNC',
sub auth_type {
    return $ENV{'AUTH_TYPE'};
}
END_OF_FUNC


#### Method: remote_user
# Return the authorization name used for user
# verification.
####
'remote_user' => <<'END_OF_FUNC',
sub remote_user {
    return $ENV{'REMOTE_USER'};
}
END_OF_FUNC


#### Method: user_name
# Try to return the remote user's name by hook or by
# crook
####
'user_name' => <<'END_OF_FUNC',
sub user_name {
    my ($self) = self_or_CGI(@_);
    return $self->http('from') || $ENV{'REMOTE_IDENT'} || $ENV{'REMOTE_USER'};
}
END_OF_FUNC

#### Method: nosticky
# Set or return the NOSTICKY global flag
####
'nosticky' => <<'END_OF_FUNC',
sub nosticky {
    my ($self,$param) = self_or_CGI(@_);
    $CGI::NOSTICKY = $param if defined($param);
    return $CGI::NOSTICKY;
}
END_OF_FUNC

#### Method: nph
# Set or return the NPH global flag
####
'nph' => <<'END_OF_FUNC',
sub nph {
    my ($self,$param) = self_or_CGI(@_);
    $CGI::NPH = $param if defined($param);
    return $CGI::NPH;
}
END_OF_FUNC

#### Method: private_tempfiles
# Set or return the private_tempfiles global flag
####
'private_tempfiles' => <<'END_OF_FUNC',
sub private_tempfiles {
    my ($self,$param) = self_or_CGI(@_);
    $CGI::PRIVATE_TEMPFILES = $param if defined($param);
    return $CGI::PRIVATE_TEMPFILES;
}
END_OF_FUNC
#### Method: close_upload_files
# Set or return the close_upload_files global flag
####
'close_upload_files' => <<'END_OF_FUNC',
sub close_upload_files {
    my ($self,$param) = self_or_CGI(@_);
    $CGI::CLOSE_UPLOAD_FILES = $param if defined($param);
    return $CGI::CLOSE_UPLOAD_FILES;
}
END_OF_FUNC


#### Method: default_dtd
# Set or return the default_dtd global
####
'default_dtd' => <<'END_OF_FUNC',
sub default_dtd {
    my ($self,$param,$param2) = self_or_CGI(@_);
    if (defined $param2 && defined $param) {
        $CGI::DEFAULT_DTD = [ $param, $param2 ];
    } elsif (defined $param) {
        $CGI::DEFAULT_DTD = $param;
    }
    return $CGI::DEFAULT_DTD;
}
END_OF_FUNC

# -------------- really private subroutines -----------------
'previous_or_default' => <<'END_OF_FUNC',
sub previous_or_default {
    my($self,$name,$defaults,$override) = @_;
    my(%selected);

    if (!$override && ($self->{'.fieldnames'}->{$name} || 
		       defined($self->param($name)) ) ) {
	$selected{$_}++ for $self->param($name);
    } elsif (defined($defaults) && ref($defaults) && 
	     (ref($defaults) eq 'ARRAY')) {
	$selected{$_}++ for @{$defaults};
    } else {
	$selected{$defaults}++ if defined($defaults);
    }

    return %selected;
}
END_OF_FUNC

'register_parameter' => <<'END_OF_FUNC',
sub register_parameter {
    my($self,$param) = @_;
    $self->{'.parametersToAdd'}->{$param}++;
}
END_OF_FUNC

'get_fields' => <<'END_OF_FUNC',
sub get_fields {
    my($self) = @_;
    return $self->CGI::hidden('-name'=>'.cgifields',
			      '-values'=>[keys %{$self->{'.parametersToAdd'}}],
			      '-override'=>1);
}
END_OF_FUNC

'read_from_cmdline' => <<'END_OF_FUNC',
sub read_from_cmdline {
    my($input,@words);
    my($query_string);
    my($subpath);
    if ($DEBUG && @ARGV) {
	@words = @ARGV;
    } elsif ($DEBUG > 1) {
	require "shellwords.pl";
	print STDERR "(offline mode: enter name=value pairs on standard input; press ^D or ^Z when done)\n";
	chomp(@lines = <STDIN>); # remove newlines
	$input = join(" ",@lines);
	@words = &shellwords($input);    
    }
    foreach (@words) {
	s/\\=/%3D/g;
	s/\\&/%26/g;	    
    }

    if ("@words"=~/=/) {
	$query_string = join('&',@words);
    } else {
	$query_string = join('+',@words);
    }
    if ($query_string =~ /^(.*?)\?(.*)$/)
    {
        $query_string = $2;
        $subpath = $1;
    }
    return { 'query_string' => $query_string, 'subpath' => $subpath };
}
END_OF_FUNC

#####
# subroutine: read_multipart
#
# Read multipart data and store it into our parameters.
# An interesting feature is that if any of the parts is a file, we
# create a temporary file and open up a filehandle on it so that the
# caller can read from it if necessary.
#####
'read_multipart' => <<'END_OF_FUNC',
sub read_multipart {
    my($self,$boundary,$length) = @_;
    my($buffer) = $self->new_MultipartBuffer($boundary,$length);
    return unless $buffer;
    my(%header,$body);
    my $filenumber = 0;
    while (!$buffer->eof) {
	%header = $buffer->readHeader;

	unless (%header) {
	    $self->cgi_error("400 Bad request (malformed multipart POST)");
	    return;
	}

	my($param)= $header{'Content-Disposition'}=~/ name="([^"]*)"/;
        $param .= $TAINTED;

        # See RFC 1867, 2183, 2045
        # NB: File content will be loaded into memory should
        # content-disposition parsing fail.
        my ($filename) = $header{'Content-Disposition'}
	               =~/ filename=(("[^"]*")|([a-z\d!\#'\*\+,\.^_\`\{\}\|\~]*))/i;
        $filename =~ s/^"([^"]*)"$/$1/;
	# Test for Opera's multiple upload feature
	my($multipart) = ( defined( $header{'Content-Type'} ) &&
		$header{'Content-Type'} =~ /multipart\/mixed/ ) ?
		1 : 0;

	# add this parameter to our list
	$self->add_parameter($param);

	# If no filename specified, then just read the data and assign it
	# to our parameter list.
	if ( ( !defined($filename) || $filename eq '' ) && !$multipart ) {
	    my($value) = $buffer->readBody;
            $value .= $TAINTED;
	    push(@{$self->{$param}},$value);
	    next;
	}

	my ($tmpfile,$tmp,$filehandle);
      UPLOADS: {
	  # If we get here, then we are dealing with a potentially large
	  # uploaded form.  Save the data to a temporary file, then open
	  # the file for reading.

	  # skip the file if uploads disabled
	  if ($DISABLE_UPLOADS) {
	      while (defined($data = $buffer->read)) { }
	      last UPLOADS;
	  }

	  # set the filename to some recognizable value
          if ( ( !defined($filename) || $filename eq '' ) && $multipart ) {
              $filename = "multipart/mixed";
          }

	  # choose a relatively unpredictable tmpfile sequence number
          my $seqno = unpack("%16C*",join('',localtime,grep {defined $_} values %ENV));
          for (my $cnt=10;$cnt>0;$cnt--) {
	    next unless $tmpfile = new CGITempFile($seqno);
	    $tmp = $tmpfile->as_string;
	    last if defined($filehandle = Fh->new($filename,$tmp,$PRIVATE_TEMPFILES));
            $seqno += int rand(100);
          }
          die "CGI open of tmpfile: $!\n" unless defined $filehandle;
	  $CGI::DefaultClass->binmode($filehandle) if $CGI::needs_binmode 
                     && defined fileno($filehandle);

	  # if this is an multipart/mixed attachment, save the header
	  # together with the body for later parsing with an external
	  # MIME parser module
	  if ( $multipart ) {
	      foreach ( keys %header ) {
		  print $filehandle "$_: $header{$_}${CRLF}";
	      }
	      print $filehandle "${CRLF}";
	  }

	  my ($data);
	  local($\) = '';
          my $totalbytes = 0;
          while (defined($data = $buffer->read)) {
              if (defined $self->{'.upload_hook'})
               {
                  $totalbytes += length($data);
                   &{$self->{'.upload_hook'}}($filename ,$data, $totalbytes, $self->{'.upload_data'});
              }
              print $filehandle $data if ($self->{'use_tempfile'});
          }

	  # back up to beginning of file
	  seek($filehandle,0,0);

      ## Close the filehandle if requested this allows a multipart MIME
      ## upload to contain many files, and we won't die due to too many
      ## open file handles. The user can access the files using the hash
      ## below.
      close $filehandle if $CLOSE_UPLOAD_FILES;
	  $CGI::DefaultClass->binmode($filehandle) if $CGI::needs_binmode;

	  # Save some information about the uploaded file where we can get
	  # at it later.
	  # Use the typeglob as the key, as this is guaranteed to be
	  # unique for each filehandle.  Don't use the file descriptor as
	  # this will be re-used for each filehandle if the
	  # close_upload_files feature is used.
	  $self->{'.tmpfiles'}->{$$filehandle}= {
              hndl => $filehandle,
	      name => $tmpfile,
	      info => {%header},
	  };
	  push(@{$self->{$param}},$filehandle);
      }
    }
}
END_OF_FUNC

#####
# subroutine: read_multipart_related
#
# Read multipart/related data and store it into our parameters.  The
# first parameter sets the start of the data. The part identified by
# this Content-ID will not be stored as a file upload, but will be
# returned by this method.  All other parts will be available as file
# uploads accessible by their Content-ID
#####
'read_multipart_related' => <<'END_OF_FUNC',
sub read_multipart_related {
    my($self,$start,$boundary,$length) = @_;
    my($buffer) = $self->new_MultipartBuffer($boundary,$length);
    return unless $buffer;
    my(%header,$body);
    my $filenumber = 0;
    my $returnvalue;
    while (!$buffer->eof) {
	%header = $buffer->readHeader;

	unless (%header) {
	    $self->cgi_error("400 Bad request (malformed multipart POST)");
	    return;
	}

	my($param) = $header{'Content-ID'}=~/\<([^\>]*)\>/;
        $param .= $TAINTED;

	# If this is the start part, then just read the data and assign it
	# to our return variable.
	if ( $param eq $start ) {
	    $returnvalue = $buffer->readBody;
            $returnvalue .= $TAINTED;
	    next;
	}

	# add this parameter to our list
	$self->add_parameter($param);

	my ($tmpfile,$tmp,$filehandle);
      UPLOADS: {
	  # If we get here, then we are dealing with a potentially large
	  # uploaded form.  Save the data to a temporary file, then open
	  # the file for reading.

	  # skip the file if uploads disabled
	  if ($DISABLE_UPLOADS) {
	      while (defined($data = $buffer->read)) { }
	      last UPLOADS;
	  }

	  # choose a relatively unpredictable tmpfile sequence number
          my $seqno = unpack("%16C*",join('',localtime,grep {defined $_} values %ENV));
          for (my $cnt=10;$cnt>0;$cnt--) {
	    next unless $tmpfile = new CGITempFile($seqno);
	    $tmp = $tmpfile->as_string;
	    last if defined($filehandle = Fh->new($param,$tmp,$PRIVATE_TEMPFILES));
            $seqno += int rand(100);
          }
          die "CGI open of tmpfile: $!\n" unless defined $filehandle;
	  $CGI::DefaultClass->binmode($filehandle) if $CGI::needs_binmode 
                     && defined fileno($filehandle);

	  my ($data);
	  local($\) = '';
          my $totalbytes;
          while (defined($data = $buffer->read)) {
              if (defined $self->{'.upload_hook'})
               {
                  $totalbytes += length($data);
                   &{$self->{'.upload_hook'}}($param ,$data, $totalbytes, $self->{'.upload_data'});
              }
              print $filehandle $data if ($self->{'use_tempfile'});
          }

	  # back up to beginning of file
	  seek($filehandle,0,0);

      ## Close the filehandle if requested this allows a multipart MIME
      ## upload to contain many files, and we won't die due to too many
      ## open file handles. The user can access the files using the hash
      ## below.
      close $filehandle if $CLOSE_UPLOAD_FILES;
	  $CGI::DefaultClass->binmode($filehandle) if $CGI::needs_binmode;

	  # Save some information about the uploaded file where we can get
	  # at it later.
	  # Use the typeglob as the key, as this is guaranteed to be
	  # unique for each filehandle.  Don't use the file descriptor as
	  # this will be re-used for each filehandle if the
	  # close_upload_files feature is used.
	  $self->{'.tmpfiles'}->{$$filehandle}= {
              hndl => $filehandle,
	      name => $tmpfile,
	      info => {%header},
	  };
	  push(@{$self->{$param}},$filehandle);
      }
    }
    return $returnvalue;
}
END_OF_FUNC


'upload' =><<'END_OF_FUNC',
sub upload {
    my($self,$param_name) = self_or_default(@_);
    my @param = grep {ref($_) && defined(fileno($_))} $self->param($param_name);
    return unless @param;
    return wantarray ? @param : $param[0];
}
END_OF_FUNC

'tmpFileName' => <<'END_OF_FUNC',
sub tmpFileName {
    my($self,$filename) = self_or_default(@_);
    return $self->{'.tmpfiles'}->{$$filename}->{name} ?
	$self->{'.tmpfiles'}->{$$filename}->{name}->as_string
	    : '';
}
END_OF_FUNC

'uploadInfo' => <<'END_OF_FUNC',
sub uploadInfo {
    my($self,$filename) = self_or_default(@_);
    return $self->{'.tmpfiles'}->{$$filename}->{info};
}
END_OF_FUNC

# internal routine, don't use
'_set_values_and_labels' => <<'END_OF_FUNC',
sub _set_values_and_labels {
    my $self = shift;
    my ($v,$l,$n) = @_;
    $$l = $v if ref($v) eq 'HASH' && !ref($$l);
    return $self->param($n) if !defined($v);
    return $v if !ref($v);
    return ref($v) eq 'HASH' ? keys %$v : @$v;
}
END_OF_FUNC

# internal routine, don't use
'_set_attributes' => <<'END_OF_FUNC',
sub _set_attributes {
    my $self = shift;
    my($element, $attributes) = @_;
    return '' unless defined($attributes->{$element});
    $attribs = ' ';
    foreach my $attrib (keys %{$attributes->{$element}}) {
        (my $clean_attrib = $attrib) =~ s/^-//;
        $attribs .= "@{[lc($clean_attrib)]}=\"$attributes->{$element}{$attrib}\" ";
    }
    $attribs =~ s/ $//;
    return $attribs;
}
END_OF_FUNC

'_compile_all' => <<'END_OF_FUNC',
sub _compile_all {
    foreach (@_) {
	next if defined(&$_);
	$AUTOLOAD = "CGI::$_";
	_compile();
    }
}
END_OF_FUNC

);
END_OF_AUTOLOAD
;

#########################################################
# Globals and stubs for other packages that we use.
#########################################################

################### Fh -- lightweight filehandle ###############
package Fh;
use overload 
    '""'  => \&asString,
    'cmp' => \&compare,
    'fallback'=>1;

$FH='fh00000';

*Fh::AUTOLOAD = \&CGI::AUTOLOAD;

sub DESTROY {
    my $self = shift;
    close $self;
}

$AUTOLOADED_ROUTINES = '';      # prevent -w error
$AUTOLOADED_ROUTINES=<<'END_OF_AUTOLOAD';
%SUBS =  (
'asString' => <<'END_OF_FUNC',
sub asString {
    my $self = shift;
    # get rid of package name
    (my $i = $$self) =~ s/^\*(\w+::fh\d{5})+//; 
    $i =~ s/%(..)/ chr(hex($1)) /eg;
    return $i.$CGI::TAINTED;
# BEGIN DEAD CODE
# This was an extremely clever patch that allowed "use strict refs".
# Unfortunately it relied on another bug that caused leaky file descriptors.
# The underlying bug has been fixed, so this no longer works.  However
# "strict refs" still works for some reason.
#    my $self = shift;
#    return ${*{$self}{SCALAR}};
# END DEAD CODE
}
END_OF_FUNC

'compare' => <<'END_OF_FUNC',
sub compare {
    my $self = shift;
    my $value = shift;
    return "$self" cmp $value;
}
END_OF_FUNC

'new'  => <<'END_OF_FUNC',
sub new {
    my($pack,$name,$file,$delete) = @_;
    _setup_symbols(@SAVED_SYMBOLS) if @SAVED_SYMBOLS;
    require Fcntl unless defined &Fcntl::O_RDWR;
    (my $safename = $name) =~ s/([':%])/ sprintf '%%%02X', ord $1 /eg;
    my $fv = ++$FH . $safename;
    my $ref = \*{"Fh::$fv"};
    $file =~ m!^([a-zA-Z0-9_\+ \'\":/.\$\\-]+)$! || return;
    my $safe = $1;
    sysopen($ref,$safe,Fcntl::O_RDWR()|Fcntl::O_CREAT()|Fcntl::O_EXCL(),0600) || return;
    unlink($safe) if $delete;
    CORE::delete $Fh::{$fv};
    return bless $ref,$pack;
}
END_OF_FUNC

);
END_OF_AUTOLOAD

######################## MultipartBuffer ####################
package MultipartBuffer;

use constant DEBUG => 0;

# how many bytes to read at a time.  We use
# a 4K buffer by default.
$INITIAL_FILLUNIT = 1024 * 4;
$TIMEOUT = 240*60;       # 4 hour timeout for big files
$SPIN_LOOP_MAX = 2000;  # bug fix for some Netscape servers
$CRLF=$CGI::CRLF;

#reuse the autoload function
*MultipartBuffer::AUTOLOAD = \&CGI::AUTOLOAD;

# avoid autoloader warnings
sub DESTROY {}

###############################################################################
################# THESE FUNCTIONS ARE AUTOLOADED ON DEMAND ####################
###############################################################################
$AUTOLOADED_ROUTINES = '';      # prevent -w error
$AUTOLOADED_ROUTINES=<<'END_OF_AUTOLOAD';
%SUBS =  (

'new' => <<'END_OF_FUNC',
sub new {
    my($package,$interface,$boundary,$length) = @_;
    $FILLUNIT = $INITIAL_FILLUNIT;
    $CGI::DefaultClass->binmode($IN); # if $CGI::needs_binmode;  # just do it always

    # If the user types garbage into the file upload field,
    # then Netscape passes NOTHING to the server (not good).
    # We may hang on this read in that case. So we implement
    # a read timeout.  If nothing is ready to read
    # by then, we return.

    # Netscape seems to be a little bit unreliable
    # about providing boundary strings.
    my $boundary_read = 0;
    if ($boundary) {

	# Under the MIME spec, the boundary consists of the 
	# characters "--" PLUS the Boundary string

	# BUG: IE 3.01 on the Macintosh uses just the boundary -- not
	# the two extra hyphens.  We do a special case here on the user-agent!!!!
	$boundary = "--$boundary" unless CGI::user_agent('MSIE\s+3\.0[12];\s*Mac|DreamPassport');

    } else { # otherwise we find it ourselves
	my($old);
	($old,$/) = ($/,$CRLF); # read a CRLF-delimited line
	$boundary = <STDIN>;      # BUG: This won't work correctly under mod_perl
	$length -= length($boundary);
	chomp($boundary);               # remove the CRLF
	$/ = $old;                      # restore old line separator
        $boundary_read++;
    }

    my $self = {LENGTH=>$length,
		CHUNKED=>!$length,
		BOUNDARY=>$boundary,
		INTERFACE=>$interface,
		BUFFER=>'',
	    };

    $FILLUNIT = length($boundary)
	if length($boundary) > $FILLUNIT;

    my $retval = bless $self,ref $package || $package;

    # Read the preamble and the topmost (boundary) line plus the CRLF.
    unless ($boundary_read) {
      while ($self->read(0)) { }
    }
    die "Malformed multipart POST: data truncated\n" if $self->eof;

    return $retval;
}
END_OF_FUNC

'readHeader' => <<'END_OF_FUNC',
sub readHeader {
    my($self) = @_;
    my($end);
    my($ok) = 0;
    my($bad) = 0;

    local($CRLF) = "\015\012" if $CGI::OS eq 'VMS' || $CGI::EBCDIC;

    do {
	$self->fillBuffer($FILLUNIT);
	$ok++ if ($end = index($self->{BUFFER},"${CRLF}${CRLF}")) >= 0;
	$ok++ if $self->{BUFFER} eq '';
	$bad++ if !$ok && $self->{LENGTH} <= 0;
	# this was a bad idea
	# $FILLUNIT *= 2 if length($self->{BUFFER}) >= $FILLUNIT; 
    } until $ok || $bad;
    return () if $bad;

    #EBCDIC NOTE: translate header into EBCDIC, but watch out for continuation lines!

    my($header) = substr($self->{BUFFER},0,$end+2);
    substr($self->{BUFFER},0,$end+4) = '';
    my %return;

    if ($CGI::EBCDIC) {
      warn "untranslated header=$header\n" if DEBUG;
      $header = CGI::Util::ascii2ebcdic($header);
      warn "translated header=$header\n" if DEBUG;
    }

    # See RFC 2045 Appendix A and RFC 822 sections 3.4.8
    #   (Folding Long Header Fields), 3.4.3 (Comments)
    #   and 3.4.5 (Quoted-Strings).

    my $token = '[-\w!\#$%&\'*+.^_\`|{}~]';
    $header=~s/$CRLF\s+/ /og;		# merge continuation lines

    while ($header=~/($token+):\s+([^$CRLF]*)/mgox) {
        my ($field_name,$field_value) = ($1,$2);
	$field_name =~ s/\b(\w)/uc($1)/eg; #canonicalize
	$return{$field_name}=$field_value;
    }
    return %return;
}
END_OF_FUNC

# This reads and returns the body as a single scalar value.
'readBody' => <<'END_OF_FUNC',
sub readBody {
    my($self) = @_;
    my($data);
    my($returnval)='';

    #EBCDIC NOTE: want to translate returnval into EBCDIC HERE

    while (defined($data = $self->read)) {
	$returnval .= $data;
    }

    if ($CGI::EBCDIC) {
      warn "untranslated body=$returnval\n" if DEBUG;
      $returnval = CGI::Util::ascii2ebcdic($returnval);
      warn "translated body=$returnval\n"   if DEBUG;
    }
    return $returnval;
}
END_OF_FUNC

# This will read $bytes or until the boundary is hit, whichever happens
# first.  After the boundary is hit, we return undef.  The next read will
# skip over the boundary and begin reading again;
'read' => <<'END_OF_FUNC',
sub read {
    my($self,$bytes) = @_;

    # default number of bytes to read
    $bytes = $bytes || $FILLUNIT;

    # Fill up our internal buffer in such a way that the boundary
    # is never split between reads.
    $self->fillBuffer($bytes);

    my $boundary_start = $CGI::EBCDIC ? CGI::Util::ebcdic2ascii($self->{BOUNDARY})      : $self->{BOUNDARY};
    my $boundary_end   = $CGI::EBCDIC ? CGI::Util::ebcdic2ascii($self->{BOUNDARY}.'--') : $self->{BOUNDARY}.'--';

    # Find the boundary in the buffer (it may not be there).
    my $start = index($self->{BUFFER},$boundary_start);

    warn "boundary=$self->{BOUNDARY} length=$self->{LENGTH} start=$start\n" if DEBUG;

    # protect against malformed multipart POST operations
    die "Malformed multipart POST\n" unless $self->{CHUNKED} || ($start >= 0 || $self->{LENGTH} > 0);

    #EBCDIC NOTE: want to translate boundary search into ASCII here.

    # If the boundary begins the data, then skip past it
    # and return undef.
    if ($start == 0) {

	# clear us out completely if we've hit the last boundary.
	if (index($self->{BUFFER},$boundary_end)==0) {
	    $self->{BUFFER}='';
	    $self->{LENGTH}=0;
	    return undef;
	}

	# just remove the boundary.
	substr($self->{BUFFER},0,length($boundary_start))='';
        $self->{BUFFER} =~ s/^\012\015?//;
	return undef;
    }

    my $bytesToReturn;
    if ($start > 0) {           # read up to the boundary
        $bytesToReturn = $start-2 > $bytes ? $bytes : $start;
    } else {    # read the requested number of bytes
	# leave enough bytes in the buffer to allow us to read
	# the boundary.  Thanks to Kevin Hendrick for finding
	# this one.
	$bytesToReturn = $bytes - (length($boundary_start)+1);
    }

    my $returnval=substr($self->{BUFFER},0,$bytesToReturn);
    substr($self->{BUFFER},0,$bytesToReturn)='';
    
    # If we hit the boundary, remove the CRLF from the end.
    return ($bytesToReturn==$start)
           ? substr($returnval,0,-2) : $returnval;
}
END_OF_FUNC


# This fills up our internal buffer in such a way that the
# boundary is never split between reads
'fillBuffer' => <<'END_OF_FUNC',
sub fillBuffer {
    my($self,$bytes) = @_;
    return unless $self->{CHUNKED} || $self->{LENGTH};

    my($boundaryLength) = length($self->{BOUNDARY});
    my($bufferLength) = length($self->{BUFFER});
    my($bytesToRead) = $bytes - $bufferLength + $boundaryLength + 2;
    $bytesToRead = $self->{LENGTH} if !$self->{CHUNKED} && $self->{LENGTH} < $bytesToRead;

    # Try to read some data.  We may hang here if the browser is screwed up.
    my $bytesRead = $self->{INTERFACE}->read_from_client(\$self->{BUFFER},
							 $bytesToRead,
							 $bufferLength);
    warn "bytesToRead=$bytesToRead, bufferLength=$bufferLength, buffer=$self->{BUFFER}\n" if DEBUG;
    $self->{BUFFER} = '' unless defined $self->{BUFFER};

    # An apparent bug in the Apache server causes the read()
    # to return zero bytes repeatedly without blocking if the
    # remote user aborts during a file transfer.  I don't know how
    # they manage this, but the workaround is to abort if we get
    # more than SPIN_LOOP_MAX consecutive zero reads.
    if ($bytesRead <= 0) {
	die  "CGI.pm: Server closed socket during multipart read (client aborted?).\n"
	    if ($self->{ZERO_LOOP_COUNTER}++ >= $SPIN_LOOP_MAX);
    } else {
	$self->{ZERO_LOOP_COUNTER}=0;
    }

    $self->{LENGTH} -= $bytesRead if !$self->{CHUNKED} && $bytesRead;
}
END_OF_FUNC


# Return true when we've finished reading
'eof' => <<'END_OF_FUNC'
sub eof {
    my($self) = @_;
    return 1 if (length($self->{BUFFER}) == 0)
		 && ($self->{LENGTH} <= 0);
    undef;
}
END_OF_FUNC

);
END_OF_AUTOLOAD

####################################################################################
################################## TEMPORARY FILES #################################
####################################################################################
package CGITempFile;

sub find_tempdir {
  $SL = $CGI::SL;
  $MAC = $CGI::OS eq 'MACINTOSH';
  my ($vol) = $MAC ? MacPerl::Volumes() =~ /:(.*)/ : "";
  unless (defined $TMPDIRECTORY) {
    @TEMP=("${SL}usr${SL}tmp","${SL}var${SL}tmp",
	   "C:${SL}temp","${SL}tmp","${SL}temp",
	   "${vol}${SL}Temporary Items",
           "${SL}WWW_ROOT", "${SL}SYS\$SCRATCH",
	   "C:${SL}system${SL}temp");
    unshift(@TEMP,$ENV{'TMPDIR'}) if defined $ENV{'TMPDIR'};

    # this feature was supposed to provide per-user tmpfiles, but
    # it is problematic.
    #    unshift(@TEMP,(getpwuid($<))[7].'/tmp') if $CGI::OS eq 'UNIX';
    # Rob: getpwuid() is unfortunately UNIX specific. On brain dead OS'es this
    #    : can generate a 'getpwuid() not implemented' exception, even though
    #    : it's never called.  Found under DOS/Win with the DJGPP perl port.
    #    : Refer to getpwuid() only at run-time if we're fortunate and have  UNIX.
    # unshift(@TEMP,(eval {(getpwuid($>))[7]}).'/tmp') if $CGI::OS eq 'UNIX' and $> != 0;

    foreach (@TEMP) {
      do {$TMPDIRECTORY = $_; last} if -d $_ && -w _;
    }
  }
  $TMPDIRECTORY  = $MAC ? "" : "." unless $TMPDIRECTORY;
}

find_tempdir();

$MAXTRIES = 5000;

# cute feature, but overload implementation broke it
# %OVERLOAD = ('""'=>'as_string');
*CGITempFile::AUTOLOAD = \&CGI::AUTOLOAD;

sub DESTROY {
    my($self) = @_;
    $$self =~ m!^([a-zA-Z0-9_ \'\":/.\$\\-]+)$! || return;
    my $safe = $1;             # untaint operation
    unlink $safe;              # get rid of the file
}

###############################################################################
################# THESE FUNCTIONS ARE AUTOLOADED ON DEMAND ####################
###############################################################################
$AUTOLOADED_ROUTINES = '';      # prevent -w error
$AUTOLOADED_ROUTINES=<<'END_OF_AUTOLOAD';
%SUBS = (

'new' => <<'END_OF_FUNC',
sub new {
    my($package,$sequence) = @_;
    my $filename;
    find_tempdir() unless -w $TMPDIRECTORY;
    for (my $i = 0; $i < $MAXTRIES; $i++) {
	last if ! -f ($filename = sprintf("\%s${SL}CGItemp%d", $TMPDIRECTORY, $sequence++));
    }
    # check that it is a more-or-less valid filename
    return unless $filename =~ m!^([a-zA-Z0-9_\+ \'\":/.\$\\-]+)$!;
    # this used to untaint, now it doesn't
    # $filename = $1;
    return bless \$filename;
}
END_OF_FUNC

'as_string' => <<'END_OF_FUNC'
sub as_string {
    my($self) = @_;
    return $$self;
}
END_OF_FUNC

);
END_OF_AUTOLOAD

package CGI;

# We get a whole bunch of warnings about "possibly uninitialized variables"
# when running with the -w switch.  Touch them all once to get rid of the
# warnings.  This is ugly and I hate it.
if ($^W) {
    $CGI::CGI = '';
    $CGI::CGI=<<EOF;
    $CGI::VERSION;
    $MultipartBuffer::SPIN_LOOP_MAX;
    $MultipartBuffer::CRLF;
    $MultipartBuffer::TIMEOUT;
    $MultipartBuffer::INITIAL_FILLUNIT;
EOF
    ;
}

1;

__END__

=head1 NAME

CGI - Simple Common Gateway Interface Class

=head1 SYNOPSIS

  # CGI script that creates a fill-out form
  # and echoes back its values.

  use CGI qw/:standard/;
  print header,
        start_html('A Simple Example'),
        h1('A Simple Example'),
        start_form,
        "What's your name? ",textfield('name'),p,
        "What's the combination?", p,
        checkbox_group(-name=>'words',
		       -values=>['eenie','meenie','minie','moe'],
		       -defaults=>['eenie','minie']), p,
        "What's your favorite color? ",
        popup_menu(-name=>'color',
	           -values=>['red','green','blue','chartreuse']),p,
        submit,
        end_form,
        hr;

   if (param()) {
       my $name      = param('name');
       my $keywords  = join ', ',param('words');
       my $color     = param('color');
       print "Your name is",em(escapeHTML($name)),p,
	     "The keywords are: ",em(escapeHTML($keywords)),p,
	     "Your favorite color is ",em(escapeHTML($color)),
	     hr;
   }

   print end_html;

=head1 ABSTRACT

This perl library uses perl5 objects to make it easy to create Web
fill-out forms and parse their contents.  This package defines CGI
objects, entities that contain the values of the current query string
and other state variables.  Using a CGI object's methods, you can
examine keywords and parameters passed to your script, and create
forms whose initial values are taken from the current query (thereby
preserving state information).  The module provides shortcut functions
that produce boilerplate HTML, reducing typing and coding errors. It
also provides functionality for some of the more advanced features of
CGI scripting, including support for file uploads, cookies, cascading
style sheets, server push, and frames.

CGI.pm also provides a simple function-oriented programming style for
those who don't need its object-oriented features.

The current version of CGI.pm is available at

  http://www.genome.wi.mit.edu/ftp/pub/software/WWW/cgi_docs.html
  ftp://ftp-genome.wi.mit.edu/pub/software/WWW/

=head1 DESCRIPTION

=head2 PROGRAMMING STYLE

There are two styles of programming with CGI.pm, an object-oriented
style and a function-oriented style.  In the object-oriented style you
create one or more CGI objects and then use object methods to create
the various elements of the page.  Each CGI object starts out with the
list of named parameters that were passed to your CGI script by the
server.  You can modify the objects, save them to a file or database
and recreate them.  Because each object corresponds to the "state" of
the CGI script, and because each object's parameter list is
independent of the others, this allows you to save the state of the
script and restore it later.

For example, using the object oriented style, here is how you create
a simple "Hello World" HTML page:

   #!/usr/local/bin/perl -w
   use CGI;                             # load CGI routines
   $q = new CGI;                        # create new CGI object
   print $q->header,                    # create the HTTP header
         $q->start_html('hello world'), # start the HTML
         $q->h1('hello world'),         # level 1 header
         $q->end_html;                  # end the HTML

In the function-oriented style, there is one default CGI object that
you rarely deal with directly.  Instead you just call functions to
retrieve CGI parameters, create HTML tags, manage cookies, and so
on.  This provides you with a cleaner programming interface, but
limits you to using one CGI object at a time.  The following example
prints the same page, but uses the function-oriented interface.
The main differences are that we now need to import a set of functions
into our name space (usually the "standard" functions), and we don't
need to create the CGI object.

   #!/usr/local/bin/perl
   use CGI qw/:standard/;           # load standard CGI routines
   print header,                    # create the HTTP header
         start_html('hello world'), # start the HTML
         h1('hello world'),         # level 1 header
         end_html;                  # end the HTML

The examples in this document mainly use the object-oriented style.
See HOW TO IMPORT FUNCTIONS for important information on
function-oriented programming in CGI.pm

=head2 CALLING CGI.PM ROUTINES

Most CGI.pm routines accept several arguments, sometimes as many as 20
optional ones!  To simplify this interface, all routines use a named
argument calling style that looks like this:

   print $q->header(-type=>'image/gif',-expires=>'+3d');

Each argument name is preceded by a dash.  Neither case nor order
matters in the argument list.  -type, -Type, and -TYPE are all
acceptable.  In fact, only the first argument needs to begin with a
dash.  If a dash is present in the first argument, CGI.pm assumes
dashes for the subsequent ones.

Several routines are commonly called with just one argument.  In the
case of these routines you can provide the single argument without an
argument name.  header() happens to be one of these routines.  In this
case, the single argument is the document type.

   print $q->header('text/html');

Other such routines are documented below.

Sometimes named arguments expect a scalar, sometimes a reference to an
array, and sometimes a reference to a hash.  Often, you can pass any
type of argument and the routine will do whatever is most appropriate.
For example, the param() routine is used to set a CGI parameter to a
single or a multi-valued value.  The two cases are shown below:

   $q->param(-name=>'veggie',-value=>'tomato');
   $q->param(-name=>'veggie',-value=>['tomato','tomahto','potato','potahto']);

A large number of routines in CGI.pm actually aren't specifically
defined in the module, but are generated automatically as needed.
These are the "HTML shortcuts," routines that generate HTML tags for
use in dynamically-generated pages.  HTML tags have both attributes
(the attribute="value" pairs within the tag itself) and contents (the
part between the opening and closing pairs.)  To distinguish between
attributes and contents, CGI.pm uses the convention of passing HTML
attributes as a hash reference as the first argument, and the
contents, if any, as any subsequent arguments.  It works out like
this:

   Code                           Generated HTML
   ----                           --------------
   h1()                           <h1>
   h1('some','contents');         <h1>some contents</h1>
   h1({-align=>left});            <h1 align="LEFT">
   h1({-align=>left},'contents'); <h1 align="LEFT">contents</h1>

HTML tags are described in more detail later.

Many newcomers to CGI.pm are puzzled by the difference between the
calling conventions for the HTML shortcuts, which require curly braces
around the HTML tag attributes, and the calling conventions for other
routines, which manage to generate attributes without the curly
brackets.  Don't be confused.  As a convenience the curly braces are
optional in all but the HTML shortcuts.  If you like, you can use
curly braces when calling any routine that takes named arguments.  For
example:

   print $q->header( {-type=>'image/gif',-expires=>'+3d'} );

If you use the B<-w> switch, you will be warned that some CGI.pm argument
names conflict with built-in Perl functions.  The most frequent of
these is the -values argument, used to create multi-valued menus,
radio button clusters and the like.  To get around this warning, you
have several choices:

=over 4

=item 1.

Use another name for the argument, if one is available. 
For example, -value is an alias for -values.

=item 2.

Change the capitalization, e.g. -Values

=item 3.

Put quotes around the argument name, e.g. '-values'

=back

Many routines will do something useful with a named argument that it
doesn't recognize.  For example, you can produce non-standard HTTP
header fields by providing them as named arguments:

  print $q->header(-type  =>  'text/html',
                   -cost  =>  'Three smackers',
                   -annoyance_level => 'high',
                   -complaints_to   => 'bit bucket');

This will produce the following nonstandard HTTP header:

   HTTP/1.0 200 OK
   Cost: Three smackers
   Annoyance-level: high
   Complaints-to: bit bucket
   Content-type: text/html

Notice the way that underscores are translated automatically into
hyphens.  HTML-generating routines perform a different type of
translation. 

This feature allows you to keep up with the rapidly changing HTTP and
HTML "standards".

=head2 CREATING A NEW QUERY OBJECT (OBJECT-ORIENTED STYLE):

     $query = new CGI;

This will parse the input (from both POST and GET methods) and store
it into a perl5 object called $query. 

Any filehandles from file uploads will have their position reset to 
the beginning of the file. 

=head2 CREATING A NEW QUERY OBJECT FROM AN INPUT FILE

     $query = new CGI(INPUTFILE);

If you provide a file handle to the new() method, it will read
parameters from the file (or STDIN, or whatever).  The file can be in
any of the forms describing below under debugging (i.e. a series of
newline delimited TAG=VALUE pairs will work).  Conveniently, this type
of file is created by the save() method (see below).  Multiple records
can be saved and restored.

Perl purists will be pleased to know that this syntax accepts
references to file handles, or even references to filehandle globs,
which is the "official" way to pass a filehandle:

    $query = new CGI(\*STDIN);

You can also initialize the CGI object with a FileHandle or IO::File
object.

If you are using the function-oriented interface and want to
initialize CGI state from a file handle, the way to do this is with
B<restore_parameters()>.  This will (re)initialize the
default CGI object from the indicated file handle.

    open (IN,"test.in") || die;
    restore_parameters(IN);
    close IN;

You can also initialize the query object from an associative array
reference:

    $query = new CGI( {'dinosaur'=>'barney',
		       'song'=>'I love you',
		       'friends'=>[qw/Jessica George Nancy/]}
		    );

or from a properly formatted, URL-escaped query string:

    $query = new CGI('dinosaur=barney&color=purple');

or from a previously existing CGI object (currently this clones the
parameter list, but none of the other object-specific fields, such as
autoescaping):

    $old_query = new CGI;
    $new_query = new CGI($old_query);

To create an empty query, initialize it from an empty string or hash:

   $empty_query = new CGI("");

       -or-

   $empty_query = new CGI({});

=head2 FETCHING A LIST OF KEYWORDS FROM THE QUERY:

     @keywords = $query->keywords

If the script was invoked as the result of an <ISINDEX> search, the
parsed keywords can be obtained as an array using the keywords() method.

=head2 FETCHING THE NAMES OF ALL THE PARAMETERS PASSED TO YOUR SCRIPT:

     @names = $query->param

If the script was invoked with a parameter list
(e.g. "name1=value1&name2=value2&name3=value3"), the param() method
will return the parameter names as a list.  If the script was invoked
as an <ISINDEX> script and contains a string without ampersands
(e.g. "value1+value2+value3") , there will be a single parameter named
"keywords" containing the "+"-delimited keywords.

NOTE: As of version 1.5, the array of parameter names returned will
be in the same order as they were submitted by the browser.
Usually this order is the same as the order in which the 
parameters are defined in the form (however, this isn't part
of the spec, and so isn't guaranteed).

=head2 FETCHING THE VALUE OR VALUES OF A SINGLE NAMED PARAMETER:

    @values = $query->param('foo');

	      -or-

    $value = $query->param('foo');

Pass the param() method a single argument to fetch the value of the
named parameter. If the parameter is multivalued (e.g. from multiple
selections in a scrolling list), you can ask to receive an array.  Otherwise
the method will return a single value.

If a value is not given in the query string, as in the queries
"name1=&name2=" or "name1&name2", it will be returned as an empty
string.  This feature is new in 2.63.


If the parameter does not exist at all, then param() will return undef
in a scalar context, and the empty list in a list context.


=head2 SETTING THE VALUE(S) OF A NAMED PARAMETER:

    $query->param('foo','an','array','of','values');

This sets the value for the named parameter 'foo' to an array of
values.  This is one way to change the value of a field AFTER
the script has been invoked once before.  (Another way is with
the -override parameter accepted by all methods that generate
form elements.)

param() also recognizes a named parameter style of calling described
in more detail later:

    $query->param(-name=>'foo',-values=>['an','array','of','values']);

			      -or-

    $query->param(-name=>'foo',-value=>'the value');

=head2 APPENDING ADDITIONAL VALUES TO A NAMED PARAMETER:

   $query->append(-name=>'foo',-values=>['yet','more','values']);

This adds a value or list of values to the named parameter.  The
values are appended to the end of the parameter if it already exists.
Otherwise the parameter is created.  Note that this method only
recognizes the named argument calling syntax.

=head2 IMPORTING ALL PARAMETERS INTO A NAMESPACE:

   $query->import_names('R');

This creates a series of variables in the 'R' namespace.  For example,
$R::foo, @R:foo.  For keyword lists, a variable @R::keywords will appear.
If no namespace is given, this method will assume 'Q'.
WARNING:  don't import anything into 'main'; this is a major security
risk!!!!

NOTE 1: Variable names are transformed as necessary into legal Perl
variable names.  All non-legal characters are transformed into
underscores.  If you need to keep the original names, you should use
the param() method instead to access CGI variables by name.

NOTE 2: In older versions, this method was called B<import()>.  As of version 2.20, 
this name has been removed completely to avoid conflict with the built-in
Perl module B<import> operator.

=head2 DELETING A PARAMETER COMPLETELY:

    $query->delete('foo','bar','baz');

This completely clears a list of parameters.  It sometimes useful for
resetting parameters that you don't want passed down between script
invocations.

If you are using the function call interface, use "Delete()" instead
to avoid conflicts with Perl's built-in delete operator.

=head2 DELETING ALL PARAMETERS:

   $query->delete_all();

This clears the CGI object completely.  It might be useful to ensure
that all the defaults are taken when you create a fill-out form.

Use Delete_all() instead if you are using the function call interface.

=head2 HANDLING NON-URLENCODED ARGUMENTS


If POSTed data is not of type application/x-www-form-urlencoded or
multipart/form-data, then the POSTed data will not be processed, but
instead be returned as-is in a parameter named POSTDATA.  To retrieve
it, use code like this:

   my $data = $query->param('POSTDATA');

Likewise if PUTed data can be retrieved with code like this:

   my $data = $query->param('PUTDATA');

(If you don't know what the preceding means, don't worry about it.  It
only affects people trying to use CGI for XML processing and other
specialized tasks.)


=head2 DIRECT ACCESS TO THE PARAMETER LIST:

   $q->param_fetch('address')->[1] = '1313 Mockingbird Lane';
   unshift @{$q->param_fetch(-name=>'address')},'George Munster';

If you need access to the parameter list in a way that isn't covered
by the methods above, you can obtain a direct reference to it by
calling the B<param_fetch()> method with the name of the .  This
will return an array reference to the named parameters, which you then
can manipulate in any way you like.

You can also use a named argument style using the B<-name> argument.

=head2 FETCHING THE PARAMETER LIST AS A HASH:

    $params = $q->Vars;
    print $params->{'address'};
    @foo = split("\0",$params->{'foo'});
    %params = $q->Vars;

    use CGI ':cgi-lib';
    $params = Vars;

Many people want to fetch the entire parameter list as a hash in which
the keys are the names of the CGI parameters, and the values are the
parameters' values.  The Vars() method does this.  Called in a scalar
context, it returns the parameter list as a tied hash reference.
Changing a key changes the value of the parameter in the underlying
CGI parameter list.  Called in a list context, it returns the
parameter list as an ordinary hash.  This allows you to read the
contents of the parameter list, but not to change it.

When using this, the thing you must watch out for are multivalued CGI
parameters.  Because a hash cannot distinguish between scalar and
list context, multivalued parameters will be returned as a packed
string, separated by the "\0" (null) character.  You must split this
packed string in order to get at the individual values.  This is the
convention introduced long ago by Steve Brenner in his cgi-lib.pl
module for Perl version 4.

If you wish to use Vars() as a function, import the I<:cgi-lib> set of
function calls (also see the section on CGI-LIB compatibility).

=head2 SAVING THE STATE OF THE SCRIPT TO A FILE:

    $query->save(\*FILEHANDLE)

This will write the current state of the form to the provided
filehandle.  You can read it back in by providing a filehandle
to the new() method.  Note that the filehandle can be a file, a pipe,
or whatever!

The format of the saved file is:

	NAME1=VALUE1
	NAME1=VALUE1'
	NAME2=VALUE2
	NAME3=VALUE3
	=

Both name and value are URL escaped.  Multi-valued CGI parameters are
represented as repeated names.  A session record is delimited by a
single = symbol.  You can write out multiple records and read them
back in with several calls to B<new>.  You can do this across several
sessions by opening the file in append mode, allowing you to create
primitive guest books, or to keep a history of users' queries.  Here's
a short example of creating multiple session records:

   use CGI;

   open (OUT,">>test.out") || die;
   $records = 5;
   foreach (0..$records) {
       my $q = new CGI;
       $q->param(-name=>'counter',-value=>$_);
       $q->save(\*OUT);
   }
   close OUT;

   # reopen for reading
   open (IN,"test.out") || die;
   while (!eof(IN)) {
       my $q = new CGI(\*IN);
       print $q->param('counter'),"\n";
   }

The file format used for save/restore is identical to that used by the
Whitehead Genome Center's data exchange format "Boulderio", and can be
manipulated and even databased using Boulderio utilities.  See

  http://stein.cshl.org/boulder/

for further details.

If you wish to use this method from the function-oriented (non-OO)
interface, the exported name for this method is B<save_parameters()>.

=head2 RETRIEVING CGI ERRORS

Errors can occur while processing user input, particularly when
processing uploaded files.  When these errors occur, CGI will stop
processing and return an empty parameter list.  You can test for
the existence and nature of errors using the I<cgi_error()> function.
The error messages are formatted as HTTP status codes. You can either
incorporate the error text into an HTML page, or use it as the value
of the HTTP status:

    my $error = $q->cgi_error;
    if ($error) {
	print $q->header(-status=>$error),
	      $q->start_html('Problems'),
              $q->h2('Request not processed'),
	      $q->strong($error);
        exit 0;
    }

When using the function-oriented interface (see the next section),
errors may only occur the first time you call I<param()>. Be ready
for this!

=head2 USING THE FUNCTION-ORIENTED INTERFACE

To use the function-oriented interface, you must specify which CGI.pm
routines or sets of routines to import into your script's namespace.
There is a small overhead associated with this importation, but it
isn't much.

   use CGI <list of methods>;

The listed methods will be imported into the current package; you can
call them directly without creating a CGI object first.  This example
shows how to import the B<param()> and B<header()>
methods, and then use them directly:

   use CGI 'param','header';
   print header('text/plain');
   $zipcode = param('zipcode');

More frequently, you'll import common sets of functions by referring
to the groups by name.  All function sets are preceded with a ":"
character as in ":html3" (for tags defined in the HTML 3 standard).

Here is a list of the function sets you can import:

=over 4

=item B<:cgi>

Import all CGI-handling methods, such as B<param()>, B<path_info()>
and the like.

=item B<:form>

Import all fill-out form generating methods, such as B<textfield()>.

=item B<:html2>

Import all methods that generate HTML 2.0 standard elements.

=item B<:html3>

Import all methods that generate HTML 3.0 elements (such as
<table>, <super> and <sub>).

=item B<:html4>

Import all methods that generate HTML 4 elements (such as
<abbrev>, <acronym> and <thead>).

=item B<:netscape>

Import all methods that generate Netscape-specific HTML extensions.

=item B<:html>

Import all HTML-generating shortcuts (i.e. 'html2' + 'html3' +
'netscape')...

=item B<:standard>

Import "standard" features, 'html2', 'html3', 'html4', 'form' and 'cgi'.

=item B<:all>

Import all the available methods.  For the full list, see the CGI.pm
code, where the variable %EXPORT_TAGS is defined.

=back

If you import a function name that is not part of CGI.pm, the module
will treat it as a new HTML tag and generate the appropriate
subroutine.  You can then use it like any other HTML tag.  This is to
provide for the rapidly-evolving HTML "standard."  For example, say
Microsoft comes out with a new tag called <gradient> (which causes the
user's desktop to be flooded with a rotating gradient fill until his
machine reboots).  You don't need to wait for a new version of CGI.pm
to start using it immediately:

   use CGI qw/:standard :html3 gradient/;
   print gradient({-start=>'red',-end=>'blue'});

Note that in the interests of execution speed CGI.pm does B<not> use
the standard L<Exporter> syntax for specifying load symbols.  This may
change in the future.

If you import any of the state-maintaining CGI or form-generating
methods, a default CGI object will be created and initialized
automatically the first time you use any of the methods that require
one to be present.  This includes B<param()>, B<textfield()>,
B<submit()> and the like.  (If you need direct access to the CGI
object, you can find it in the global variable B<$CGI::Q>).  By
importing CGI.pm methods, you can create visually elegant scripts:

   use CGI qw/:standard/;
   print 
       header,
       start_html('Simple Script'),
       h1('Simple Script'),
       start_form,
       "What's your name? ",textfield('name'),p,
       "What's the combination?",
       checkbox_group(-name=>'words',
		      -values=>['eenie','meenie','minie','moe'],
		      -defaults=>['eenie','moe']),p,
       "What's your favorite color?",
       popup_menu(-name=>'color',
		  -values=>['red','green','blue','chartreuse']),p,
       submit,
       end_form,
       hr,"\n";

    if (param) {
       print 
	   "Your name is ",em(param('name')),p,
	   "The keywords are: ",em(join(", ",param('words'))),p,
	   "Your favorite color is ",em(param('color')),".\n";
    }
    print end_html;

=head2 PRAGMAS

In addition to the function sets, there are a number of pragmas that
you can import.  Pragmas, which are always preceded by a hyphen,
change the way that CGI.pm functions in various ways.  Pragmas,
function sets, and individual functions can all be imported in the
same use() line.  For example, the following use statement imports the
standard set of functions and enables debugging mode (pragma
-debug):

   use CGI qw/:standard -debug/;

The current list of pragmas is as follows:

=over 4

=item -any

When you I<use CGI -any>, then any method that the query object
doesn't recognize will be interpreted as a new HTML tag.  This allows
you to support the next I<ad hoc> Netscape or Microsoft HTML
extension.  This lets you go wild with new and unsupported tags:

   use CGI qw(-any);
   $q=new CGI;
   print $q->gradient({speed=>'fast',start=>'red',end=>'blue'});

Since using <cite>any</cite> causes any mistyped method name
to be interpreted as an HTML tag, use it with care or not at
all.

=item -compile

This causes the indicated autoloaded methods to be compiled up front,
rather than deferred to later.  This is useful for scripts that run
for an extended period of time under FastCGI or mod_perl, and for
those destined to be crunched by Malcolm Beattie's Perl compiler.  Use
it in conjunction with the methods or method families you plan to use.

   use CGI qw(-compile :standard :html3);

or even

   use CGI qw(-compile :all);

Note that using the -compile pragma in this way will always have
the effect of importing the compiled functions into the current
namespace.  If you want to compile without importing use the
compile() method instead:

   use CGI();
   CGI->compile();

This is particularly useful in a mod_perl environment, in which you
might want to precompile all CGI routines in a startup script, and
then import the functions individually in each mod_perl script.

=item -nosticky

By default the CGI module implements a state-preserving behavior
called "sticky" fields.  The way this works is that if you are
regenerating a form, the methods that generate the form field values
will interrogate param() to see if similarly-named parameters are
present in the query string. If they find a like-named parameter, they
will use it to set their default values.

Sometimes this isn't what you want.  The B<-nosticky> pragma prevents
this behavior.  You can also selectively change the sticky behavior in
each element that you generate.

=item -tabindex

Automatically add tab index attributes to each form field. With this
option turned off, you can still add tab indexes manually by passing a
-tabindex option to each field-generating method.

=item -no_undef_params

This keeps CGI.pm from including undef params in the parameter list.

=item -no_xhtml

By default, CGI.pm versions 2.69 and higher emit XHTML
(http://www.w3.org/TR/xhtml1/).  The -no_xhtml pragma disables this
feature.  Thanks to Michalis Kabrianis <kabrianis@hellug.gr> for this
feature.

If start_html()'s -dtd parameter specifies an HTML 2.0 or 3.2 DTD, 
XHTML will automatically be disabled without needing to use this 
pragma.

=item -utf8

This makes CGI.pm treat all parameters as UTF-8 strings. Use this with
care, as it will interfere with the processing of binary uploads. It
is better to manually select which fields are expected to return utf-8
strings and convert them using code like this:

 use Encode;
 my $arg = decode utf8=>param('foo');

=item -nph

This makes CGI.pm produce a header appropriate for an NPH (no
parsed header) script.  You may need to do other things as well
to tell the server that the script is NPH.  See the discussion
of NPH scripts below.

=item -newstyle_urls

Separate the name=value pairs in CGI parameter query strings with
semicolons rather than ampersands.  For example:

   ?name=fred;age=24;favorite_color=3

Semicolon-delimited query strings are always accepted, but will not be
emitted by self_url() and query_string() unless the -newstyle_urls
pragma is specified.

This became the default in version 2.64.

=item -oldstyle_urls

Separate the name=value pairs in CGI parameter query strings with
ampersands rather than semicolons.  This is no longer the default.

=item -autoload

This overrides the autoloader so that any function in your program
that is not recognized is referred to CGI.pm for possible evaluation.
This allows you to use all the CGI.pm functions without adding them to
your symbol table, which is of concern for mod_perl users who are
worried about memory consumption.  I<Warning:> when
I<-autoload> is in effect, you cannot use "poetry mode"
(functions without the parenthesis).  Use I<hr()> rather
than I<hr>, or add something like I<use subs qw/hr p header/> 
to the top of your script.

=item -no_debug

This turns off the command-line processing features.  If you want to
run a CGI.pm script from the command line to produce HTML, and you
don't want it to read CGI parameters from the command line or STDIN,
then use this pragma:

   use CGI qw(-no_debug :standard);

=item -debug

This turns on full debugging.  In addition to reading CGI arguments
from the command-line processing, CGI.pm will pause and try to read
arguments from STDIN, producing the message "(offline mode: enter
name=value pairs on standard input)" features.

See the section on debugging for more details.

=item -private_tempfiles

CGI.pm can process uploaded file. Ordinarily it spools the uploaded
file to a temporary directory, then deletes the file when done.
However, this opens the risk of eavesdropping as described in the file
upload section.  Another CGI script author could peek at this data
during the upload, even if it is confidential information. On Unix
systems, the -private_tempfiles pragma will cause the temporary file
to be unlinked as soon as it is opened and before any data is written
into it, reducing, but not eliminating the risk of eavesdropping
(there is still a potential race condition).  To make life harder for
the attacker, the program chooses tempfile names by calculating a 32
bit checksum of the incoming HTTP headers.

To ensure that the temporary file cannot be read by other CGI scripts,
use suEXEC or a CGI wrapper program to run your script.  The temporary
file is created with mode 0600 (neither world nor group readable).

The temporary directory is selected using the following algorithm:

    1. if the current user (e.g. "nobody") has a directory named
    "tmp" in its home directory, use that (Unix systems only).

    2. if the environment variable TMPDIR exists, use the location
    indicated.

    3. Otherwise try the locations /usr/tmp, /var/tmp, C:\temp,
    /tmp, /temp, ::Temporary Items, and \WWW_ROOT.

Each of these locations is checked that it is a directory and is
writable.  If not, the algorithm tries the next choice.

=back

=head2 SPECIAL FORMS FOR IMPORTING HTML-TAG FUNCTIONS

Many of the methods generate HTML tags.  As described below, tag
functions automatically generate both the opening and closing tags.
For example:

  print h1('Level 1 Header');

produces

  <h1>Level 1 Header</h1>

There will be some times when you want to produce the start and end
tags yourself.  In this case, you can use the form start_I<tag_name>
and end_I<tag_name>, as in:

  print start_h1,'Level 1 Header',end_h1;

With a few exceptions (described below), start_I<tag_name> and
end_I<tag_name> functions are not generated automatically when you
I<use CGI>.  However, you can specify the tags you want to generate
I<start/end> functions for by putting an asterisk in front of their
name, or, alternatively, requesting either "start_I<tag_name>" or
"end_I<tag_name>" in the import list.

Example:

  use CGI qw/:standard *table start_ul/;

In this example, the following functions are generated in addition to
the standard ones:

=over 4

=item 1. start_table() (generates a <table> tag)

=item 2. end_table() (generates a </table> tag)

=item 3. start_ul() (generates a <ul> tag)

=item 4. end_ul() (generates a </ul> tag)

=back

=head1 GENERATING DYNAMIC DOCUMENTS

Most of CGI.pm's functions deal with creating documents on the fly.
Generally you will produce the HTTP header first, followed by the
document itself.  CGI.pm provides functions for generating HTTP
headers of various types as well as for generating HTML.  For creating
GIF images, see the GD.pm module.

Each of these functions produces a fragment of HTML or HTTP which you
can print out directly so that it displays in the browser window,
append to a string, or save to a file for later use.

=head2 CREATING A STANDARD HTTP HEADER:

Normally the first thing you will do in any CGI script is print out an
HTTP header.  This tells the browser what type of document to expect,
and gives other optional information, such as the language, expiration
date, and whether to cache the document.  The header can also be
manipulated for special purposes, such as server push and pay per view
pages.

	print header;

	     -or-

	print header('image/gif');

	     -or-

	print header('text/html','204 No response');

	     -or-

	print header(-type=>'image/gif',
			     -nph=>1,
			     -status=>'402 Payment required',
			     -expires=>'+3d',
			     -cookie=>$cookie,
                             -charset=>'utf-7',
                             -attachment=>'foo.gif',
			     -Cost=>'$2.00');

header() returns the Content-type: header.  You can provide your own
MIME type if you choose, otherwise it defaults to text/html.  An
optional second parameter specifies the status code and a human-readable
message.  For example, you can specify 204, "No response" to create a
script that tells the browser to do nothing at all.

The last example shows the named argument style for passing arguments
to the CGI methods using named parameters.  Recognized parameters are
B<-type>, B<-status>, B<-expires>, and B<-cookie>.  Any other named
parameters will be stripped of their initial hyphens and turned into
header fields, allowing you to specify any HTTP header you desire.
Internal underscores will be turned into hyphens:

    print header(-Content_length=>3002);

Most browsers will not cache the output from CGI scripts.  Every time
the browser reloads the page, the script is invoked anew.  You can
change this behavior with the B<-expires> parameter.  When you specify
an absolute or relative expiration interval with this parameter, some
browsers and proxy servers will cache the script's output until the
indicated expiration date.  The following forms are all valid for the
-expires field:

	+30s                              30 seconds from now
	+10m                              ten minutes from now
	+1h                               one hour from now
	-1d                               yesterday (i.e. "ASAP!")
	now                               immediately
	+3M                               in three months
	+10y                              in ten years time
	Thursday, 25-Apr-1999 00:40:33 GMT  at the indicated time & date

The B<-cookie> parameter generates a header that tells the browser to provide
a "magic cookie" during all subsequent transactions with your script.
Netscape cookies have a special format that includes interesting attributes
such as expiration time.  Use the cookie() method to create and retrieve
session cookies.

The B<-nph> parameter, if set to a true value, will issue the correct
headers to work with a NPH (no-parse-header) script.  This is important
to use with certain servers that expect all their scripts to be NPH.

The B<-charset> parameter can be used to control the character set
sent to the browser.  If not provided, defaults to ISO-8859-1.  As a
side effect, this sets the charset() method as well.

The B<-attachment> parameter can be used to turn the page into an
attachment.  Instead of displaying the page, some browsers will prompt
the user to save it to disk.  The value of the argument is the
suggested name for the saved file.  In order for this to work, you may
have to set the B<-type> to "application/octet-stream".

The B<-p3p> parameter will add a P3P tag to the outgoing header.  The
parameter can be an arrayref or a space-delimited string of P3P tags.
For example:

   print header(-p3p=>[qw(CAO DSP LAW CURa)]);
   print header(-p3p=>'CAO DSP LAW CURa');

In either case, the outgoing header will be formatted as:

  P3P: policyref="/w3c/p3p.xml" cp="CAO DSP LAW CURa"

=head2 GENERATING A REDIRECTION HEADER

   print redirect('http://somewhere.else/in/movie/land');

Sometimes you don't want to produce a document yourself, but simply
redirect the browser elsewhere, perhaps choosing a URL based on the
time of day or the identity of the user.  

The redirect() function redirects the browser to a different URL.  If
you use redirection like this, you should B<not> print out a header as
well.

You should always use full URLs (including the http: or ftp: part) in
redirection requests.  Relative URLs will not work correctly.

You can also use named arguments:

    print redirect(-uri=>'http://somewhere.else/in/movie/land',
			   -nph=>1,
                           -status=>301);

The B<-nph> parameter, if set to a true value, will issue the correct
headers to work with a NPH (no-parse-header) script.  This is important
to use with certain servers, such as Microsoft IIS, which
expect all their scripts to be NPH.

The B<-status> parameter will set the status of the redirect.  HTTP
defines three different possible redirection status codes:

     301 Moved Permanently
     302 Found
     303 See Other

The default if not specified is 302, which means "moved temporarily."
You may change the status to another status code if you wish.  Be
advised that changing the status to anything other than 301, 302 or
303 will probably break redirection.

=head2 CREATING THE HTML DOCUMENT HEADER

   print start_html(-title=>'Secrets of the Pyramids',
			    -author=>'fred@capricorn.org',
			    -base=>'true',
			    -target=>'_blank',
			    -meta=>{'keywords'=>'pharaoh secret mummy',
				    'copyright'=>'copyright 1996 King Tut'},
			    -style=>{'src'=>'/styles/style1.css'},
			    -BGCOLOR=>'blue');

After creating the HTTP header, most CGI scripts will start writing
out an HTML document.  The start_html() routine creates the top of the
page, along with a lot of optional information that controls the
page's appearance and behavior.

This method returns a canned HTML header and the opening <body> tag.
All parameters are optional.  In the named parameter form, recognized
parameters are -title, -author, -base, -xbase, -dtd, -lang and -target
(see below for the explanation).  Any additional parameters you
provide, such as the Netscape unofficial BGCOLOR attribute, are added
to the <body> tag.  Additional parameters must be proceeded by a
hyphen.

The argument B<-xbase> allows you to provide an HREF for the <base> tag
different from the current location, as in

    -xbase=>"http://home.mcom.com/"

All relative links will be interpreted relative to this tag.

The argument B<-target> allows you to provide a default target frame
for all the links and fill-out forms on the page.  B<This is a
non-standard HTTP feature which only works with Netscape browsers!>
See the Netscape documentation on frames for details of how to
manipulate this.

    -target=>"answer_window"

All relative links will be interpreted relative to this tag.
You add arbitrary meta information to the header with the B<-meta>
argument.  This argument expects a reference to an associative array
containing name/value pairs of meta information.  These will be turned
into a series of header <meta> tags that look something like this:

    <meta name="keywords" content="pharaoh secret mummy">
    <meta name="description" content="copyright 1996 King Tut">

To create an HTTP-EQUIV type of <meta> tag, use B<-head>, described
below.

The B<-style> argument is used to incorporate cascading stylesheets
into your code.  See the section on CASCADING STYLESHEETS for more
information.

The B<-lang> argument is used to incorporate a language attribute into
the <html> tag.  For example:

    print $q->start_html(-lang=>'fr-CA');

The default if not specified is "en-US" for US English, unless the 
-dtd parameter specifies an HTML 2.0 or 3.2 DTD, in which case the
lang attribute is left off.  You can force the lang attribute to left
off in other cases by passing an empty string (-lang=>'').

The B<-encoding> argument can be used to specify the character set for
XHTML.  It defaults to iso-8859-1 if not specified.

The B<-declare_xml> argument, when used in conjunction with XHTML,
will put a <?xml> declaration at the top of the HTML header. The sole
purpose of this declaration is to declare the character set
encoding. In the absence of -declare_xml, the output HTML will contain
a <meta> tag that specifies the encoding, allowing the HTML to pass
most validators.  The default for -declare_xml is false.

You can place other arbitrary HTML elements to the <head> section with the
B<-head> tag.  For example, to place the rarely-used <link> element in the
head section, use this:

    print start_html(-head=>Link({-rel=>'next',
		                  -href=>'http://www.capricorn.com/s2.html'}));

To incorporate multiple HTML elements into the <head> section, just pass an
array reference:

    print start_html(-head=>[ 
                             Link({-rel=>'next',
				   -href=>'http://www.capricorn.com/s2.html'}),
		             Link({-rel=>'previous',
				   -href=>'http://www.capricorn.com/s1.html'})
			     ]
		     );

And here's how to create an HTTP-EQUIV <meta> tag:

      print start_html(-head=>meta({-http_equiv => 'Content-Type',
                                    -content    => 'text/html'}))


JAVASCRIPTING: The B<-script>, B<-noScript>, B<-onLoad>,
B<-onMouseOver>, B<-onMouseOut> and B<-onUnload> parameters are used
to add Netscape JavaScript calls to your pages.  B<-script> should
point to a block of text containing JavaScript function definitions.
This block will be placed within a <script> block inside the HTML (not
HTTP) header.  The block is placed in the header in order to give your
page a fighting chance of having all its JavaScript functions in place
even if the user presses the stop button before the page has loaded
completely.  CGI.pm attempts to format the script in such a way that
JavaScript-naive browsers will not choke on the code: unfortunately
there are some browsers, such as Chimera for Unix, that get confused
by it nevertheless.

The B<-onLoad> and B<-onUnload> parameters point to fragments of JavaScript
code to execute when the page is respectively opened and closed by the
browser.  Usually these parameters are calls to functions defined in the
B<-script> field:

      $query = new CGI;
      print header;
      $JSCRIPT=<<END;
      // Ask a silly question
      function riddle_me_this() {
	 var r = prompt("What walks on four legs in the morning, " +
		       "two legs in the afternoon, " +
		       "and three legs in the evening?");
	 response(r);
      }
      // Get a silly answer
      function response(answer) {
	 if (answer == "man")
	    alert("Right you are!");
	 else
	    alert("Wrong!  Guess again.");
      }
      END
      print start_html(-title=>'The Riddle of the Sphinx',
			       -script=>$JSCRIPT);

Use the B<-noScript> parameter to pass some HTML text that will be displayed on 
browsers that do not have JavaScript (or browsers where JavaScript is turned
off).

The <script> tag, has several attributes including "type" and src.
The latter is particularly interesting, as it allows you to keep the
JavaScript code in a file or CGI script rather than cluttering up each
page with the source.  To use these attributes pass a HASH reference
in the B<-script> parameter containing one or more of -type, -src, or
-code:

    print $q->start_html(-title=>'The Riddle of the Sphinx',
			 -script=>{-type=>'JAVASCRIPT',
                                   -src=>'/javascript/sphinx.js'}
			 );

    print $q->(-title=>'The Riddle of the Sphinx',
	       -script=>{-type=>'PERLSCRIPT',
			 -code=>'print "hello world!\n;"'}
	       );


A final feature allows you to incorporate multiple <script> sections into the
header.  Just pass the list of script sections as an array reference.
this allows you to specify different source files for different dialects
of JavaScript.  Example:

     print $q->start_html(-title=>'The Riddle of the Sphinx',
                          -script=>[
                                    { -type => 'text/javascript',
                                      -src      => '/javascript/utilities10.js'
                                    },
                                    { -type => 'text/javascript',
                                      -src      => '/javascript/utilities11.js'
                                    },
                                    { -type => 'text/jscript',
                                      -src      => '/javascript/utilities12.js'
                                    },
                                    { -type => 'text/ecmascript',
                                      -src      => '/javascript/utilities219.js'
                                    }
                                 ]
                             );

The option "-language" is a synonym for -type, and is supported for
backwad compatibility.

The old-style positional parameters are as follows:

=over 4

=item B<Parameters:>

=item 1.

The title

=item 2.

The author's e-mail address (will create a <link rev="MADE"> tag if present

=item 3.

A 'true' flag if you want to include a <base> tag in the header.  This
helps resolve relative addresses to absolute ones when the document is moved, 
but makes the document hierarchy non-portable.  Use with care!

=item 4, 5, 6...

Any other parameters you want to include in the <body> tag.  This is a good
place to put Netscape extensions, such as colors and wallpaper patterns.

=back

=head2 ENDING THE HTML DOCUMENT:

	print end_html

This ends an HTML document by printing the </body></html> tags.

=head2 CREATING A SELF-REFERENCING URL THAT PRESERVES STATE INFORMATION:

    $myself = self_url;
    print q(<a href="$myself">I'm talking to myself.</a>);

self_url() will return a URL, that, when selected, will reinvoke
this script with all its state information intact.  This is most
useful when you want to jump around within the document using
internal anchors but you don't want to disrupt the current contents
of the form(s).  Something like this will do the trick.

     $myself = self_url;
     print "<a href=\"$myself#table1\">See table 1</a>";
     print "<a href=\"$myself#table2\">See table 2</a>";
     print "<a href=\"$myself#yourself\">See for yourself</a>";

If you want more control over what's returned, using the B<url()>
method instead.

You can also retrieve the unprocessed query string with query_string():

    $the_string = query_string;

=head2 OBTAINING THE SCRIPT'S URL

    $full_url      = url();
    $full_url      = url(-full=>1);  #alternative syntax
    $relative_url  = url(-relative=>1);
    $absolute_url  = url(-absolute=>1);
    $url_with_path = url(-path_info=>1);
    $url_with_path_and_query = url(-path_info=>1,-query=>1);
    $netloc        = url(-base => 1);

B<url()> returns the script's URL in a variety of formats.  Called
without any arguments, it returns the full form of the URL, including
host name and port number

    http://your.host.com/path/to/script.cgi

You can modify this format with the following named arguments:

=over 4

=item B<-absolute>

If true, produce an absolute URL, e.g.

    /path/to/script.cgi

=item B<-relative>

Produce a relative URL.  This is useful if you want to reinvoke your
script with different parameters. For example:

    script.cgi

=item B<-full>

Produce the full URL, exactly as if called without any arguments.
This overrides the -relative and -absolute arguments.

=item B<-path> (B<-path_info>)

Append the additional path information to the URL.  This can be
combined with B<-full>, B<-absolute> or B<-relative>.  B<-path_info>
is provided as a synonym.

=item B<-query> (B<-query_string>)

Append the query string to the URL.  This can be combined with
B<-full>, B<-absolute> or B<-relative>.  B<-query_string> is provided
as a synonym.

=item B<-base>

Generate just the protocol and net location, as in http://www.foo.com:8000

=item B<-rewrite>

If Apache's mod_rewrite is turned on, then the script name and path
info probably won't match the request that the user sent. Set
-rewrite=>1 (default) to return URLs that match what the user sent
(the original request URI). Set -rewrite=>0 to return URLs that match
the URL after mod_rewrite's rules have run. Because the additional
path information only makes sense in the context of the rewritten URL,
-rewrite is set to false when you request path info in the URL.

=back

=head2 MIXING POST AND URL PARAMETERS

   $color = url_param('color');

It is possible for a script to receive CGI parameters in the URL as
well as in the fill-out form by creating a form that POSTs to a URL
containing a query string (a "?" mark followed by arguments).  The
B<param()> method will always return the contents of the POSTed
fill-out form, ignoring the URL's query string.  To retrieve URL
parameters, call the B<url_param()> method.  Use it in the same way as
B<param()>.  The main difference is that it allows you to read the
parameters, but not set them.


Under no circumstances will the contents of the URL query string
interfere with similarly-named CGI parameters in POSTed forms.  If you
try to mix a URL query string with a form submitted with the GET
method, the results will not be what you expect.

=head1 CREATING STANDARD HTML ELEMENTS:

CGI.pm defines general HTML shortcut methods for most, if not all of
the HTML 3 and HTML 4 tags.  HTML shortcuts are named after a single
HTML element and return a fragment of HTML text that you can then
print or manipulate as you like.  Each shortcut returns a fragment of
HTML code that you can append to a string, save to a file, or, most
commonly, print out so that it displays in the browser window.

This example shows how to use the HTML methods:

   print $q->blockquote(
		     "Many years ago on the island of",
		     $q->a({href=>"http://crete.org/"},"Crete"),
		     "there lived a Minotaur named",
		     $q->strong("Fred."),
		    ),
       $q->hr;

This results in the following HTML code (extra newlines have been
added for readability):

   <blockquote>
   Many years ago on the island of
   <a href="http://crete.org/">Crete</a> there lived
   a minotaur named <strong>Fred.</strong> 
   </blockquote>
   <hr>

If you find the syntax for calling the HTML shortcuts awkward, you can
import them into your namespace and dispense with the object syntax
completely (see the next section for more details):

   use CGI ':standard';
   print blockquote(
      "Many years ago on the island of",
      a({href=>"http://crete.org/"},"Crete"),
      "there lived a minotaur named",
      strong("Fred."),
      ),
      hr;

=head2 PROVIDING ARGUMENTS TO HTML SHORTCUTS

The HTML methods will accept zero, one or multiple arguments.  If you
provide no arguments, you get a single tag:

   print hr;  	#  <hr>

If you provide one or more string arguments, they are concatenated
together with spaces and placed between opening and closing tags:

   print h1("Chapter","1"); # <h1>Chapter 1</h1>"

If the first argument is an associative array reference, then the keys
and values of the associative array become the HTML tag's attributes:

   print a({-href=>'fred.html',-target=>'_new'},
      "Open a new frame");

	    <a href="fred.html",target="_new">Open a new frame</a>

You may dispense with the dashes in front of the attribute names if
you prefer:

   print img {src=>'fred.gif',align=>'LEFT'};

	   <img align="LEFT" src="fred.gif">

Sometimes an HTML tag attribute has no argument.  For example, ordered
lists can be marked as COMPACT.  The syntax for this is an argument that
that points to an undef string:

   print ol({compact=>undef},li('one'),li('two'),li('three'));

Prior to CGI.pm version 2.41, providing an empty ('') string as an
attribute argument was the same as providing undef.  However, this has
changed in order to accommodate those who want to create tags of the form 
<img alt="">.  The difference is shown in these two pieces of code:

   CODE                   RESULT
   img({alt=>undef})      <img alt>
   img({alt=>''})         <img alt="">

=head2 THE DISTRIBUTIVE PROPERTY OF HTML SHORTCUTS

One of the cool features of the HTML shortcuts is that they are
distributive.  If you give them an argument consisting of a
B<reference> to a list, the tag will be distributed across each
element of the list.  For example, here's one way to make an ordered
list:

   print ul(
             li({-type=>'disc'},['Sneezy','Doc','Sleepy','Happy'])
           );

This example will result in HTML output that looks like this:

   <ul>
     <li type="disc">Sneezy</li>
     <li type="disc">Doc</li>
     <li type="disc">Sleepy</li>
     <li type="disc">Happy</li>
   </ul>

This is extremely useful for creating tables.  For example:

   print table({-border=>undef},
           caption('When Should You Eat Your Vegetables?'),
           Tr({-align=>CENTER,-valign=>TOP},
           [
              th(['Vegetable', 'Breakfast','Lunch','Dinner']),
              td(['Tomatoes' , 'no', 'yes', 'yes']),
              td(['Broccoli' , 'no', 'no',  'yes']),
              td(['Onions'   , 'yes','yes', 'yes'])
           ]
           )
        );

=head2 HTML SHORTCUTS AND LIST INTERPOLATION

Consider this bit of code:

   print blockquote(em('Hi'),'mom!'));

It will ordinarily return the string that you probably expect, namely:

   <blockquote><em>Hi</em> mom!</blockquote>

Note the space between the element "Hi" and the element "mom!".
CGI.pm puts the extra space there using array interpolation, which is
controlled by the magic $" variable.  Sometimes this extra space is
not what you want, for example, when you are trying to align a series
of images.  In this case, you can simply change the value of $" to an
empty string.

   {
      local($") = '';
      print blockquote(em('Hi'),'mom!'));
    }

I suggest you put the code in a block as shown here.  Otherwise the
change to $" will affect all subsequent code until you explicitly
reset it.

=head2 NON-STANDARD HTML SHORTCUTS

A few HTML tags don't follow the standard pattern for various
reasons.  

B<comment()> generates an HTML comment (<!-- comment -->).  Call it
like

    print comment('here is my comment');

Because of conflicts with built-in Perl functions, the following functions
begin with initial caps:

    Select
    Tr
    Link
    Delete
    Accept
    Sub

In addition, start_html(), end_html(), start_form(), end_form(),
start_multipart_form() and all the fill-out form tags are special.
See their respective sections.

=head2 AUTOESCAPING HTML

By default, all HTML that is emitted by the form-generating functions
is passed through a function called escapeHTML():

=over 4

=item $escaped_string = escapeHTML("unescaped string");

Escape HTML formatting characters in a string.

=back

Provided that you have specified a character set of ISO-8859-1 (the
default), the standard HTML escaping rules will be used.  The "<"
character becomes "&lt;", ">" becomes "&gt;", "&" becomes "&amp;", and
the quote character becomes "&quot;".  In addition, the hexadecimal
0x8b and 0x9b characters, which some browsers incorrectly interpret
as the left and right angle-bracket characters, are replaced by their
numeric character entities ("&#8249" and "&#8250;").  If you manually change
the charset, either by calling the charset() method explicitly or by
passing a -charset argument to header(), then B<all> characters will
be replaced by their numeric entities, since CGI.pm has no lookup
table for all the possible encodings.

The automatic escaping does not apply to other shortcuts, such as
h1().  You should call escapeHTML() yourself on untrusted data in
order to protect your pages against nasty tricks that people may enter
into guestbooks, etc..  To change the character set, use charset().
To turn autoescaping off completely, use autoEscape(0):

=over 4

=item $charset = charset([$charset]);

Get or set the current character set.

=item $flag = autoEscape([$flag]);

Get or set the value of the autoescape flag.

=back

=head2 PRETTY-PRINTING HTML

By default, all the HTML produced by these functions comes out as one
long line without carriage returns or indentation. This is yuck, but
it does reduce the size of the documents by 10-20%.  To get
pretty-printed output, please use L<CGI::Pretty>, a subclass
contributed by Brian Paulsen.

=head1 CREATING FILL-OUT FORMS:

I<General note>  The various form-creating methods all return strings
to the caller, containing the tag or tags that will create the requested
form element.  You are responsible for actually printing out these strings.
It's set up this way so that you can place formatting tags
around the form elements.

I<Another note> The default values that you specify for the forms are only
used the B<first> time the script is invoked (when there is no query
string).  On subsequent invocations of the script (when there is a query
string), the former values are used even if they are blank.  

If you want to change the value of a field from its previous value, you have two
choices:

(1) call the param() method to set it.

(2) use the -override (alias -force) parameter (a new feature in version 2.15).
This forces the default value to be used, regardless of the previous value:

   print textfield(-name=>'field_name',
			   -default=>'starting value',
			   -override=>1,
			   -size=>50,
			   -maxlength=>80);

I<Yet another note> By default, the text and labels of form elements are
escaped according to HTML rules.  This means that you can safely use
"<CLICK ME>" as the label for a button.  However, it also interferes with
your ability to incorporate special HTML character sequences, such as &Aacute;,
into your fields.  If you wish to turn off automatic escaping, call the
autoEscape() method with a false value immediately after creating the CGI object:

   $query = new CGI;
   autoEscape(undef);

I<A Lurking Trap!> Some of the form-element generating methods return
multiple tags.  In a scalar context, the tags will be concatenated
together with spaces, or whatever is the current value of the $"
global.  In a list context, the methods will return a list of
elements, allowing you to modify them if you wish.  Usually you will
not notice this behavior, but beware of this:

    printf("%s\n",end_form())

end_form() produces several tags, and only the first of them will be
printed because the format only expects one value.

<p>


=head2 CREATING AN ISINDEX TAG

   print isindex(-action=>$action);

	 -or-

   print isindex($action);

Prints out an <isindex> tag.  Not very exciting.  The parameter
-action specifies the URL of the script to process the query.  The
default is to process the query with the current script.

=head2 STARTING AND ENDING A FORM

    print start_form(-method=>$method,
		    -action=>$action,
		    -enctype=>$encoding);
      <... various form stuff ...>
    print endform;

	-or-

    print start_form($method,$action,$encoding);
      <... various form stuff ...>
    print endform;

start_form() will return a <form> tag with the optional method,
action and form encoding that you specify.  The defaults are:

    method: POST
    action: this script
    enctype: application/x-www-form-urlencoded

endform() returns the closing </form> tag.  

Start_form()'s enctype argument tells the browser how to package the various
fields of the form before sending the form to the server.  Two
values are possible:

B<Note:> This method was previously named startform(), and startform()
is still recognized as an alias.

=over 4

=item B<application/x-www-form-urlencoded>

This is the older type of encoding used by all browsers prior to
Netscape 2.0.  It is compatible with many CGI scripts and is
suitable for short fields containing text data.  For your
convenience, CGI.pm stores the name of this encoding
type in B<&CGI::URL_ENCODED>.

=item B<multipart/form-data>

This is the newer type of encoding introduced by Netscape 2.0.
It is suitable for forms that contain very large fields or that
are intended for transferring binary data.  Most importantly,
it enables the "file upload" feature of Netscape 2.0 forms.  For
your convenience, CGI.pm stores the name of this encoding type
in B<&CGI::MULTIPART>

Forms that use this type of encoding are not easily interpreted
by CGI scripts unless they use CGI.pm or another library designed
to handle them.

If XHTML is activated (the default), then forms will be automatically
created using this type of encoding.

=back

For compatibility, the start_form() method uses the older form of
encoding by default.  If you want to use the newer form of encoding
by default, you can call B<start_multipart_form()> instead of
B<start_form()>.

JAVASCRIPTING: The B<-name> and B<-onSubmit> parameters are provided
for use with JavaScript.  The -name parameter gives the
form a name so that it can be identified and manipulated by
JavaScript functions.  -onSubmit should point to a JavaScript
function that will be executed just before the form is submitted to your
server.  You can use this opportunity to check the contents of the form 
for consistency and completeness.  If you find something wrong, you
can put up an alert box or maybe fix things up yourself.  You can 
abort the submission by returning false from this function.  

Usually the bulk of JavaScript functions are defined in a <script>
block in the HTML header and -onSubmit points to one of these function
call.  See start_html() for details.

=head2 FORM ELEMENTS

After starting a form, you will typically create one or more
textfields, popup menus, radio groups and other form elements.  Each
of these elements takes a standard set of named arguments.  Some
elements also have optional arguments.  The standard arguments are as
follows:

=over 4

=item B<-name>

The name of the field. After submission this name can be used to
retrieve the field's value using the param() method.

=item B<-value>, B<-values>

The initial value of the field which will be returned to the script
after form submission.  Some form elements, such as text fields, take
a single scalar -value argument. Others, such as popup menus, take a
reference to an array of values. The two arguments are synonyms.

=item B<-tabindex>

A numeric value that sets the order in which the form element receives
focus when the user presses the tab key. Elements with lower values
receive focus first.

=item B<-id>

A string identifier that can be used to identify this element to
JavaScript and DHTML.

=item B<-override>

A boolean, which, if true, forces the element to take on the value
specified by B<-value>, overriding the sticky behavior described
earlier for the B<-no_sticky> pragma.

=item B<-onChange>, B<-onFocus>, B<-onBlur>, B<-onMouseOver>, B<-onMouseOut>, B<-onSelect>

These are used to assign JavaScript event handlers. See the
JavaScripting section for more details.

=back

Other common arguments are described in the next section. In addition
to these, all attributes described in the HTML specifications are
supported.

=head2 CREATING A TEXT FIELD

    print textfield(-name=>'field_name',
		    -value=>'starting value',
		    -size=>50,
		    -maxlength=>80);
	-or-

    print textfield('field_name','starting value',50,80);

textfield() will return a text input field. 

=over 4

=item B<Parameters>

=item 1.

The first parameter is the required name for the field (-name). 

=item 2.

The optional second parameter is the default starting value for the field
contents (-value, formerly known as -default).

=item 3.

The optional third parameter is the size of the field in
      characters (-size).

=item 4.

The optional fourth parameter is the maximum number of characters the
      field will accept (-maxlength).

=back

As with all these methods, the field will be initialized with its 
previous contents from earlier invocations of the script.
When the form is processed, the value of the text field can be
retrieved with:

       $value = param('foo');

If you want to reset it from its initial value after the script has been
called once, you can do so like this:

       param('foo',"I'm taking over this value!");

=head2 CREATING A BIG TEXT FIELD

   print textarea(-name=>'foo',
			  -default=>'starting value',
			  -rows=>10,
			  -columns=>50);

	-or

   print textarea('foo','starting value',10,50);

textarea() is just like textfield, but it allows you to specify
rows and columns for a multiline text entry box.  You can provide
a starting value for the field, which can be long and contain
multiple lines.

=head2 CREATING A PASSWORD FIELD

   print password_field(-name=>'secret',
				-value=>'starting value',
				-size=>50,
				-maxlength=>80);
	-or-

   print password_field('secret','starting value',50,80);

password_field() is identical to textfield(), except that its contents 
will be starred out on the web page.

=head2 CREATING A FILE UPLOAD FIELD

    print filefield(-name=>'uploaded_file',
			    -default=>'starting value',
			    -size=>50,
			    -maxlength=>80);
	-or-

    print filefield('uploaded_file','starting value',50,80);

filefield() will return a file upload field for Netscape 2.0 browsers.
In order to take full advantage of this I<you must use the new 
multipart encoding scheme> for the form.  You can do this either
by calling B<start_form()> with an encoding type of B<&CGI::MULTIPART>,
or by calling the new method B<start_multipart_form()> instead of
vanilla B<start_form()>.

=over 4

=item B<Parameters>

=item 1.

The first parameter is the required name for the field (-name).  

=item 2.

The optional second parameter is the starting value for the field contents
to be used as the default file name (-default).

For security reasons, browsers don't pay any attention to this field,
and so the starting value will always be blank.  Worse, the field
loses its "sticky" behavior and forgets its previous contents.  The
starting value field is called for in the HTML specification, however,
and possibly some browser will eventually provide support for it.

=item 3.

The optional third parameter is the size of the field in
characters (-size).

=item 4.

The optional fourth parameter is the maximum number of characters the
field will accept (-maxlength).

=back

When the form is processed, you can retrieve the entered filename
by calling param():

       $filename = param('uploaded_file');

Different browsers will return slightly different things for the
name.  Some browsers return the filename only.  Others return the full
path to the file, using the path conventions of the user's machine.
Regardless, the name returned is always the name of the file on the
I<user's> machine, and is unrelated to the name of the temporary file
that CGI.pm creates during upload spooling (see below).

The filename returned is also a file handle.  You can read the contents
of the file using standard Perl file reading calls:

	# Read a text file and print it out
	while (<$filename>) {
	   print;
	}

	# Copy a binary file to somewhere safe
	open (OUTFILE,">>/usr/local/web/users/feedback");
	while ($bytesread=read($filename,$buffer,1024)) {
	   print OUTFILE $buffer;
	}

However, there are problems with the dual nature of the upload fields.
If you C<use strict>, then Perl will complain when you try to use a
string as a filehandle.  You can get around this by placing the file
reading code in a block containing the C<no strict> pragma.  More
seriously, it is possible for the remote user to type garbage into the
upload field, in which case what you get from param() is not a
filehandle at all, but a string.

To be safe, use the I<upload()> function (new in version 2.47).  When
called with the name of an upload field, I<upload()> returns a
filehandle, or undef if the parameter is not a valid filehandle.

     $fh = upload('uploaded_file');
     while (<$fh>) {
	   print;
     }

In an list context, upload() will return an array of filehandles.
This makes it possible to create forms that use the same name for
multiple upload fields.

This is the recommended idiom.

For robust code, consider reseting the file handle position to beginning of the
file. Inside of larger frameworks, other code may have already used the query
object and changed the filehandle postion:

  seek($fh,0,0); # reset postion to beginning of file.

When a file is uploaded the browser usually sends along some
information along with it in the format of headers.  The information
usually includes the MIME content type.  Future browsers may send
other information as well (such as modification date and size). To
retrieve this information, call uploadInfo().  It returns a reference to
an associative array containing all the document headers.

       $filename = param('uploaded_file');
       $type = uploadInfo($filename)->{'Content-Type'};
       unless ($type eq 'text/html') {
	  die "HTML FILES ONLY!";
       }

If you are using a machine that recognizes "text" and "binary" data
modes, be sure to understand when and how to use them (see the Camel book).  
Otherwise you may find that binary files are corrupted during file
uploads.

There are occasionally problems involving parsing the uploaded file.
This usually happens when the user presses "Stop" before the upload is
finished.  In this case, CGI.pm will return undef for the name of the
uploaded file and set I<cgi_error()> to the string "400 Bad request
(malformed multipart POST)".  This error message is designed so that
you can incorporate it into a status code to be sent to the browser.
Example:

   $file = upload('uploaded_file');
   if (!$file && cgi_error) {
      print header(-status=>cgi_error);
      exit 0;
   }

You are free to create a custom HTML page to complain about the error,
if you wish.

You can set up a callback that will be called whenever a file upload
is being read during the form processing. This is much like the
UPLOAD_HOOK facility available in Apache::Request, with the exception
that the first argument to the callback is an Apache::Upload object,
here it's the remote filename.

 $q = CGI->new(\&hook [,$data [,$use_tempfile]]);

 sub hook
 {
        my ($filename, $buffer, $bytes_read, $data) = @_;
        print  "Read $bytes_read bytes of $filename\n";         
 }

The $data field is optional; it lets you pass configuration
information (e.g. a database handle) to your hook callback.

The $use_tempfile field is a flag that lets you turn on and off
CGI.pm's use of a temporary disk-based file during file upload. If you
set this to a FALSE value (default true) then param('uploaded_file')
will no longer work, and the only way to get at the uploaded data is
via the hook you provide.

If using the function-oriented interface, call the CGI::upload_hook()
method before calling param() or any other CGI functions:

  CGI::upload_hook(\&hook [,$data [,$use_tempfile]]);

This method is not exported by default.  You will have to import it
explicitly if you wish to use it without the CGI:: prefix.

If you are using CGI.pm on a Windows platform and find that binary
files get slightly larger when uploaded but that text files remain the
same, then you have forgotten to activate binary mode on the output
filehandle.  Be sure to call binmode() on any handle that you create
to write the uploaded file to disk.

JAVASCRIPTING: The B<-onChange>, B<-onFocus>, B<-onBlur>,
B<-onMouseOver>, B<-onMouseOut> and B<-onSelect> parameters are
recognized.  See textfield() for details.

=head2 CREATING A POPUP MENU

   print popup_menu('menu_name',
			    ['eenie','meenie','minie'],
			    'meenie');

      -or-

   %labels = ('eenie'=>'your first choice',
	      'meenie'=>'your second choice',
	      'minie'=>'your third choice');
   %attributes = ('eenie'=>{'class'=>'class of first choice'});
   print popup_menu('menu_name',
			    ['eenie','meenie','minie'],
          'meenie',\%labels,\%attributes);

	-or (named parameter style)-

   print popup_menu(-name=>'menu_name',
			    -values=>['eenie','meenie','minie'],
			    -default=>'meenie',
          -labels=>\%labels,
          -attributes=>\%attributes);

popup_menu() creates a menu.

=over 4

=item 1.

The required first argument is the menu's name (-name).

=item 2.

The required second argument (-values) is an array B<reference>
containing the list of menu items in the menu.  You can pass the
method an anonymous array, as shown in the example, or a reference to
a named array, such as "\@foo".

=item 3.

The optional third parameter (-default) is the name of the default
menu choice.  If not specified, the first item will be the default.
The values of the previous choice will be maintained across queries.

=item 4.

The optional fourth parameter (-labels) is provided for people who
want to use different values for the user-visible label inside the
popup menu and the value returned to your script.  It's a pointer to an
associative array relating menu values to user-visible labels.  If you
leave this parameter blank, the menu values will be displayed by
default.  (You can also leave a label undefined if you want to).

=item 5.

The optional fifth parameter (-attributes) is provided to assign
any of the common HTML attributes to an individual menu item. It's
a pointer to an associative array relating menu values to another
associative array with the attribute's name as the key and the
attribute's value as the value.

=back

When the form is processed, the selected value of the popup menu can
be retrieved using:

      $popup_menu_value = param('menu_name');

=head2 CREATING AN OPTION GROUP

Named parameter style

  print popup_menu(-name=>'menu_name',
                  -values=>[qw/eenie meenie minie/,
                            optgroup(-name=>'optgroup_name',
                                             -values => ['moe','catch'],
                                             -attributes=>{'catch'=>{'class'=>'red'}})],
                  -labels=>{'eenie'=>'one',
                            'meenie'=>'two',
                            'minie'=>'three'},
                  -default=>'meenie');

  Old style
  print popup_menu('menu_name',
                  ['eenie','meenie','minie',
                   optgroup('optgroup_name', ['moe', 'catch'],
                                   {'catch'=>{'class'=>'red'}})],'meenie',
                  {'eenie'=>'one','meenie'=>'two','minie'=>'three'});

optgroup() creates an option group within a popup menu.

=over 4

=item 1.

The required first argument (B<-name>) is the label attribute of the
optgroup and is B<not> inserted in the parameter list of the query.

=item 2.

The required second argument (B<-values>)  is an array reference
containing the list of menu items in the menu.  You can pass the
method an anonymous array, as shown in the example, or a reference
to a named array, such as \@foo.  If you pass a HASH reference,
the keys will be used for the menu values, and the values will be
used for the menu labels (see -labels below).

=item 3.

The optional third parameter (B<-labels>) allows you to pass a reference
to an associative array containing user-visible labels for one or more
of the menu items.  You can use this when you want the user to see one
menu string, but have the browser return your program a different one.
If you don't specify this, the value string will be used instead
("eenie", "meenie" and "minie" in this example).  This is equivalent
to using a hash reference for the -values parameter.

=item 4.

An optional fourth parameter (B<-labeled>) can be set to a true value
and indicates that the values should be used as the label attribute
for each option element within the optgroup.

=item 5.

An optional fifth parameter (-novals) can be set to a true value and
indicates to suppress the val attribute in each option element within
the optgroup.

See the discussion on optgroup at W3C
(http://www.w3.org/TR/REC-html40/interact/forms.html#edef-OPTGROUP)
for details.

=item 6.

An optional sixth parameter (-attributes) is provided to assign
any of the common HTML attributes to an individual menu item. It's
a pointer to an associative array relating menu values to another
associative array with the attribute's name as the key and the
attribute's value as the value.

=back

=head2 CREATING A SCROLLING LIST

   print scrolling_list('list_name',
				['eenie','meenie','minie','moe'],
        ['eenie','moe'],5,'true',{'moe'=>{'class'=>'red'}});
      -or-

   print scrolling_list('list_name',
				['eenie','meenie','minie','moe'],
				['eenie','moe'],5,'true',
        \%labels,%attributes);

	-or-

   print scrolling_list(-name=>'list_name',
				-values=>['eenie','meenie','minie','moe'],
				-default=>['eenie','moe'],
				-size=>5,
				-multiple=>'true',
        -labels=>\%labels,
        -attributes=>\%attributes);

scrolling_list() creates a scrolling list.  

=over 4

=item B<Parameters:>

=item 1.

The first and second arguments are the list name (-name) and values
(-values).  As in the popup menu, the second argument should be an
array reference.

=item 2.

The optional third argument (-default) can be either a reference to a
list containing the values to be selected by default, or can be a
single value to select.  If this argument is missing or undefined,
then nothing is selected when the list first appears.  In the named
parameter version, you can use the synonym "-defaults" for this
parameter.

=item 3.

The optional fourth argument is the size of the list (-size).

=item 4.

The optional fifth argument can be set to true to allow multiple
simultaneous selections (-multiple).  Otherwise only one selection
will be allowed at a time.

=item 5.

The optional sixth argument is a pointer to an associative array
containing long user-visible labels for the list items (-labels).
If not provided, the values will be displayed.

=item 6.

The optional sixth parameter (-attributes) is provided to assign
any of the common HTML attributes to an individual menu item. It's
a pointer to an associative array relating menu values to another
associative array with the attribute's name as the key and the
attribute's value as the value.

When this form is processed, all selected list items will be returned as
a list under the parameter name 'list_name'.  The values of the
selected items can be retrieved with:

      @selected = param('list_name');

=back

=head2 CREATING A GROUP OF RELATED CHECKBOXES

   print checkbox_group(-name=>'group_name',
				-values=>['eenie','meenie','minie','moe'],
				-default=>['eenie','moe'],
				-linebreak=>'true',
                                -disabled => ['moe'],
        -labels=>\%labels,
        -attributes=>\%attributes);

   print checkbox_group('group_name',
				['eenie','meenie','minie','moe'],
        ['eenie','moe'],'true',\%labels,
        {'moe'=>{'class'=>'red'}});

   HTML3-COMPATIBLE BROWSERS ONLY:

   print checkbox_group(-name=>'group_name',
				-values=>['eenie','meenie','minie','moe'],
				-rows=2,-columns=>2);


checkbox_group() creates a list of checkboxes that are related
by the same name.

=over 4

=item B<Parameters:>

=item 1.

The first and second arguments are the checkbox name and values,
respectively (-name and -values).  As in the popup menu, the second
argument should be an array reference.  These values are used for the
user-readable labels printed next to the checkboxes as well as for the
values passed to your script in the query string.

=item 2.

The optional third argument (-default) can be either a reference to a
list containing the values to be checked by default, or can be a
single value to checked.  If this argument is missing or undefined,
then nothing is selected when the list first appears.

=item 3.

The optional fourth argument (-linebreak) can be set to true to place
line breaks between the checkboxes so that they appear as a vertical
list.  Otherwise, they will be strung together on a horizontal line.

=back


The optional b<-labels> argument is a pointer to an associative array
relating the checkbox values to the user-visible labels that will be
printed next to them.  If not provided, the values will be used as the
default.


The optional parameters B<-rows>, and B<-columns> cause
checkbox_group() to return an HTML3 compatible table containing the
checkbox group formatted with the specified number of rows and
columns.  You can provide just the -columns parameter if you wish;
checkbox_group will calculate the correct number of rows for you.

The option b<-disabled> takes an array of checkbox values and disables
them by greying them out (this may not be supported by all browsers).

The optional B<-attributes> argument is provided to assign any of the
common HTML attributes to an individual menu item. It's a pointer to
an associative array relating menu values to another associative array
with the attribute's name as the key and the attribute's value as the
value.

The optional B<-tabindex> argument can be used to control the order in which
radio buttons receive focus when the user presses the tab button.  If
passed a scalar numeric value, the first element in the group will
receive this tab index and subsequent elements will be incremented by
one.  If given a reference to an array of radio button values, then
the indexes will be jiggered so that the order specified in the array
will correspond to the tab order.  You can also pass a reference to a
hash in which the hash keys are the radio button values and the values
are the tab indexes of each button.  Examples:

  -tabindex => 100    #  this group starts at index 100 and counts up
  -tabindex => ['moe','minie','eenie','meenie']  # tab in this order
  -tabindex => {meenie=>100,moe=>101,minie=>102,eenie=>200} # tab in this order

The optional B<-labelattributes> argument will contain attributes
attached to the <label> element that surrounds each button.

When the form is processed, all checked boxes will be returned as
a list under the parameter name 'group_name'.  The values of the
"on" checkboxes can be retrieved with:

      @turned_on = param('group_name');

The value returned by checkbox_group() is actually an array of button
elements.  You can capture them and use them within tables, lists,
or in other creative ways:

    @h = checkbox_group(-name=>'group_name',-values=>\@values);
    &use_in_creative_way(@h);

=head2 CREATING A STANDALONE CHECKBOX

    print checkbox(-name=>'checkbox_name',
			   -checked=>1,
			   -value=>'ON',
			   -label=>'CLICK ME');

	-or-

    print checkbox('checkbox_name','checked','ON','CLICK ME');

checkbox() is used to create an isolated checkbox that isn't logically
related to any others.

=over 4

=item B<Parameters:>

=item 1.

The first parameter is the required name for the checkbox (-name).  It
will also be used for the user-readable label printed next to the
checkbox.

=item 2.

The optional second parameter (-checked) specifies that the checkbox
is turned on by default.  Synonyms are -selected and -on.

=item 3.

The optional third parameter (-value) specifies the value of the
checkbox when it is checked.  If not provided, the word "on" is
assumed.

=item 4.

The optional fourth parameter (-label) is the user-readable label to
be attached to the checkbox.  If not provided, the checkbox name is
used.

=back

The value of the checkbox can be retrieved using:

    $turned_on = param('checkbox_name');

=head2 CREATING A RADIO BUTTON GROUP

   print radio_group(-name=>'group_name',
			     -values=>['eenie','meenie','minie'],
			     -default=>'meenie',
			     -linebreak=>'true',
           -labels=>\%labels,
           -attributes=>\%attributes);

	-or-

   print radio_group('group_name',['eenie','meenie','minie'],
            'meenie','true',\%labels,\%attributes);


   HTML3-COMPATIBLE BROWSERS ONLY:

   print radio_group(-name=>'group_name',
			     -values=>['eenie','meenie','minie','moe'],
			     -rows=2,-columns=>2);

radio_group() creates a set of logically-related radio buttons
(turning one member of the group on turns the others off)

=over 4

=item B<Parameters:>

=item 1.

The first argument is the name of the group and is required (-name).

=item 2.

The second argument (-values) is the list of values for the radio
buttons.  The values and the labels that appear on the page are
identical.  Pass an array I<reference> in the second argument, either
using an anonymous array, as shown, or by referencing a named array as
in "\@foo".

=item 3.

The optional third parameter (-default) is the name of the default
button to turn on. If not specified, the first item will be the
default.  You can provide a nonexistent button name, such as "-" to
start up with no buttons selected.

=item 4.

The optional fourth parameter (-linebreak) can be set to 'true' to put
line breaks between the buttons, creating a vertical list.

=item 5.

The optional fifth parameter (-labels) is a pointer to an associative
array relating the radio button values to user-visible labels to be
used in the display.  If not provided, the values themselves are
displayed.

=back


All modern browsers can take advantage of the optional parameters
B<-rows>, and B<-columns>.  These parameters cause radio_group() to
return an HTML3 compatible table containing the radio group formatted
with the specified number of rows and columns.  You can provide just
the -columns parameter if you wish; radio_group will calculate the
correct number of rows for you.

To include row and column headings in the returned table, you
can use the B<-rowheaders> and B<-colheaders> parameters.  Both
of these accept a pointer to an array of headings to use.
The headings are just decorative.  They don't reorganize the
interpretation of the radio buttons -- they're still a single named
unit.

The optional B<-tabindex> argument can be used to control the order in which
radio buttons receive focus when the user presses the tab button.  If
passed a scalar numeric value, the first element in the group will
receive this tab index and subsequent elements will be incremented by
one.  If given a reference to an array of radio button values, then
the indexes will be jiggered so that the order specified in the array
will correspond to the tab order.  You can also pass a reference to a
hash in which the hash keys are the radio button values and the values
are the tab indexes of each button.  Examples:

  -tabindex => 100    #  this group starts at index 100 and counts up
  -tabindex => ['moe','minie','eenie','meenie']  # tab in this order
  -tabindex => {meenie=>100,moe=>101,minie=>102,eenie=>200} # tab in this order


The optional B<-attributes> argument is provided to assign any of the
common HTML attributes to an individual menu item. It's a pointer to
an associative array relating menu values to another associative array
with the attribute's name as the key and the attribute's value as the
value.

The optional B<-labelattributes> argument will contain attributes
attached to the <label> element that surrounds each button.

When the form is processed, the selected radio button can
be retrieved using:

      $which_radio_button = param('group_name');

The value returned by radio_group() is actually an array of button
elements.  You can capture them and use them within tables, lists,
or in other creative ways:

    @h = radio_group(-name=>'group_name',-values=>\@values);
    &use_in_creative_way(@h);

=head2 CREATING A SUBMIT BUTTON 

   print submit(-name=>'button_name',
			-value=>'value');

	-or-

   print submit('button_name','value');

submit() will create the query submission button.  Every form
should have one of these.

=over 4

=item B<Parameters:>

=item 1.

The first argument (-name) is optional.  You can give the button a
name if you have several submission buttons in your form and you want
to distinguish between them.  

=item 2.

The second argument (-value) is also optional.  This gives the button
a value that will be passed to your script in the query string. The
name will also be used as the user-visible label.

=item 3.

You can use -label as an alias for -value.  I always get confused
about which of -name and -value changes the user-visible label on the
button.

=back

You can figure out which button was pressed by using different
values for each one:

     $which_one = param('button_name');

=head2 CREATING A RESET BUTTON

   print reset

reset() creates the "reset" button.  Note that it restores the
form to its value from the last time the script was called, 
NOT necessarily to the defaults.

Note that this conflicts with the Perl reset() built-in.  Use
CORE::reset() to get the original reset function.

=head2 CREATING A DEFAULT BUTTON

   print defaults('button_label')

defaults() creates a button that, when invoked, will cause the
form to be completely reset to its defaults, wiping out all the
changes the user ever made.

=head2 CREATING A HIDDEN FIELD

	print hidden(-name=>'hidden_name',
			     -default=>['value1','value2'...]);

		-or-

	print hidden('hidden_name','value1','value2'...);

hidden() produces a text field that can't be seen by the user.  It
is useful for passing state variable information from one invocation
of the script to the next.

=over 4

=item B<Parameters:>

=item 1.

The first argument is required and specifies the name of this
field (-name).

=item 2.  

The second argument is also required and specifies its value
(-default).  In the named parameter style of calling, you can provide
a single value here or a reference to a whole list

=back

Fetch the value of a hidden field this way:

     $hidden_value = param('hidden_name');

Note, that just like all the other form elements, the value of a
hidden field is "sticky".  If you want to replace a hidden field with
some other values after the script has been called once you'll have to
do it manually:

     param('hidden_name','new','values','here');

=head2 CREATING A CLICKABLE IMAGE BUTTON

     print image_button(-name=>'button_name',
				-src=>'/source/URL',
				-align=>'MIDDLE');      

	-or-

     print image_button('button_name','/source/URL','MIDDLE');

image_button() produces a clickable image.  When it's clicked on the
position of the click is returned to your script as "button_name.x"
and "button_name.y", where "button_name" is the name you've assigned
to it.

=over 4

=item B<Parameters:>

=item 1.

The first argument (-name) is required and specifies the name of this
field.

=item 2.

The second argument (-src) is also required and specifies the URL

=item 3.
The third option (-align, optional) is an alignment type, and may be
TOP, BOTTOM or MIDDLE

=back

Fetch the value of the button this way:
     $x = param('button_name.x');
     $y = param('button_name.y');

=head2 CREATING A JAVASCRIPT ACTION BUTTON

     print button(-name=>'button_name',
			  -value=>'user visible label',
			  -onClick=>"do_something()");

	-or-

     print button('button_name',"do_something()");

button() produces a button that is compatible with Netscape 2.0's
JavaScript.  When it's pressed the fragment of JavaScript code
pointed to by the B<-onClick> parameter will be executed.  On
non-Netscape browsers this form element will probably not even
display.

=head1 HTTP COOKIES

Netscape browsers versions 1.1 and higher, and all versions of
Internet Explorer, support a so-called "cookie" designed to help
maintain state within a browser session.  CGI.pm has several methods
that support cookies.

A cookie is a name=value pair much like the named parameters in a CGI
query string.  CGI scripts create one or more cookies and send
them to the browser in the HTTP header.  The browser maintains a list
of cookies that belong to a particular Web server, and returns them
to the CGI script during subsequent interactions.

In addition to the required name=value pair, each cookie has several
optional attributes:

=over 4

=item 1. an expiration time

This is a time/date string (in a special GMT format) that indicates
when a cookie expires.  The cookie will be saved and returned to your
script until this expiration date is reached if the user exits
the browser and restarts it.  If an expiration date isn't specified, the cookie
will remain active until the user quits the browser.

=item 2. a domain

This is a partial or complete domain name for which the cookie is 
valid.  The browser will return the cookie to any host that matches
the partial domain name.  For example, if you specify a domain name
of ".capricorn.com", then the browser will return the cookie to
Web servers running on any of the machines "www.capricorn.com", 
"www2.capricorn.com", "feckless.capricorn.com", etc.  Domain names
must contain at least two periods to prevent attempts to match
on top level domains like ".edu".  If no domain is specified, then
the browser will only return the cookie to servers on the host the
cookie originated from.

=item 3. a path

If you provide a cookie path attribute, the browser will check it
against your script's URL before returning the cookie.  For example,
if you specify the path "/cgi-bin", then the cookie will be returned
to each of the scripts "/cgi-bin/tally.pl", "/cgi-bin/order.pl",
and "/cgi-bin/customer_service/complain.pl", but not to the script
"/cgi-private/site_admin.pl".  By default, path is set to "/", which
causes the cookie to be sent to any CGI script on your site.

=item 4. a "secure" flag

If the "secure" attribute is set, the cookie will only be sent to your
script if the CGI request is occurring on a secure channel, such as SSL.

=back

The interface to HTTP cookies is the B<cookie()> method:

    $cookie = cookie(-name=>'sessionID',
			     -value=>'xyzzy',
			     -expires=>'+1h',
			     -path=>'/cgi-bin/database',
			     -domain=>'.capricorn.org',
			     -secure=>1);
    print header(-cookie=>$cookie);

B<cookie()> creates a new cookie.  Its parameters include:

=over 4

=item B<-name>

The name of the cookie (required).  This can be any string at all.
Although browsers limit their cookie names to non-whitespace
alphanumeric characters, CGI.pm removes this restriction by escaping
and unescaping cookies behind the scenes.

=item B<-value>

The value of the cookie.  This can be any scalar value,
array reference, or even associative array reference.  For example,
you can store an entire associative array into a cookie this way:

	$cookie=cookie(-name=>'family information',
			       -value=>\%childrens_ages);

=item B<-path>

The optional partial path for which this cookie will be valid, as described
above.

=item B<-domain>

The optional partial domain for which this cookie will be valid, as described
above.

=item B<-expires>

The optional expiration date for this cookie.  The format is as described 
in the section on the B<header()> method:

	"+1h"  one hour from now

=item B<-secure>

If set to true, this cookie will only be used within a secure
SSL session.

=back

The cookie created by cookie() must be incorporated into the HTTP
header within the string returned by the header() method:

        use CGI ':standard';
	print header(-cookie=>$my_cookie);

To create multiple cookies, give header() an array reference:

	$cookie1 = cookie(-name=>'riddle_name',
				  -value=>"The Sphynx's Question");
	$cookie2 = cookie(-name=>'answers',
				  -value=>\%answers);
	print header(-cookie=>[$cookie1,$cookie2]);

To retrieve a cookie, request it by name by calling cookie() method
without the B<-value> parameter. This example uses the object-oriented
form:

	use CGI;
	$query = new CGI;
	$riddle = $query->cookie('riddle_name');
        %answers = $query->cookie('answers');

Cookies created with a single scalar value, such as the "riddle_name"
cookie, will be returned in that form.  Cookies with array and hash
values can also be retrieved.

The cookie and CGI namespaces are separate.  If you have a parameter
named 'answers' and a cookie named 'answers', the values retrieved by
param() and cookie() are independent of each other.  However, it's
simple to turn a CGI parameter into a cookie, and vice-versa:

   # turn a CGI parameter into a cookie
   $c=cookie(-name=>'answers',-value=>[param('answers')]);
   # vice-versa
   param(-name=>'answers',-value=>[cookie('answers')]);

If you call cookie() without any parameters, it will return a list of
the names of all cookies passed to your script:

  @cookies = cookie();

See the B<cookie.cgi> example script for some ideas on how to use
cookies effectively.

=head1 WORKING WITH FRAMES

It's possible for CGI.pm scripts to write into several browser panels
and windows using the HTML 4 frame mechanism.  There are three
techniques for defining new frames programmatically:

=over 4

=item 1. Create a <Frameset> document

After writing out the HTTP header, instead of creating a standard
HTML document using the start_html() call, create a <frameset> 
document that defines the frames on the page.  Specify your script(s)
(with appropriate parameters) as the SRC for each of the frames.

There is no specific support for creating <frameset> sections 
in CGI.pm, but the HTML is very simple to write.  See the frame
documentation in Netscape's home pages for details 

  http://wp.netscape.com/assist/net_sites/frames.html

=item 2. Specify the destination for the document in the HTTP header

You may provide a B<-target> parameter to the header() method:

    print header(-target=>'ResultsWindow');

This will tell the browser to load the output of your script into the
frame named "ResultsWindow".  If a frame of that name doesn't already
exist, the browser will pop up a new window and load your script's
document into that.  There are a number of magic names that you can
use for targets.  See the frame documents on Netscape's home pages for
details.

=item 3. Specify the destination for the document in the <form> tag

You can specify the frame to load in the FORM tag itself.  With
CGI.pm it looks like this:

    print start_form(-target=>'ResultsWindow');

When your script is reinvoked by the form, its output will be loaded
into the frame named "ResultsWindow".  If one doesn't already exist
a new window will be created.

=back

The script "frameset.cgi" in the examples directory shows one way to
create pages in which the fill-out form and the response live in
side-by-side frames.

=head1 SUPPORT FOR JAVASCRIPT

Netscape versions 2.0 and higher incorporate an interpreted language
called JavaScript. Internet Explorer, 3.0 and higher, supports a
closely-related dialect called JScript. JavaScript isn't the same as
Java, and certainly isn't at all the same as Perl, which is a great
pity. JavaScript allows you to programmatically change the contents of
fill-out forms, create new windows, and pop up dialog box from within
Netscape itself. From the point of view of CGI scripting, JavaScript
is quite useful for validating fill-out forms prior to submitting
them.

You'll need to know JavaScript in order to use it. There are many good
sources in bookstores and on the web.

The usual way to use JavaScript is to define a set of functions in a
<SCRIPT> block inside the HTML header and then to register event
handlers in the various elements of the page. Events include such
things as the mouse passing over a form element, a button being
clicked, the contents of a text field changing, or a form being
submitted. When an event occurs that involves an element that has
registered an event handler, its associated JavaScript code gets
called.

The elements that can register event handlers include the <BODY> of an
HTML document, hypertext links, all the various elements of a fill-out
form, and the form itself. There are a large number of events, and
each applies only to the elements for which it is relevant. Here is a
partial list:

=over 4

=item B<onLoad>

The browser is loading the current document. Valid in:

     + The HTML <BODY> section only.

=item B<onUnload>

The browser is closing the current page or frame. Valid for:

     + The HTML <BODY> section only.

=item B<onSubmit>

The user has pressed the submit button of a form. This event happens
just before the form is submitted, and your function can return a
value of false in order to abort the submission.  Valid for:

     + Forms only.

=item B<onClick>

The mouse has clicked on an item in a fill-out form. Valid for:

     + Buttons (including submit, reset, and image buttons)
     + Checkboxes
     + Radio buttons

=item B<onChange>

The user has changed the contents of a field. Valid for:

     + Text fields
     + Text areas
     + Password fields
     + File fields
     + Popup Menus
     + Scrolling lists

=item B<onFocus>

The user has selected a field to work with. Valid for:

     + Text fields
     + Text areas
     + Password fields
     + File fields
     + Popup Menus
     + Scrolling lists

=item B<onBlur>

The user has deselected a field (gone to work somewhere else).  Valid
for:

     + Text fields
     + Text areas
     + Password fields
     + File fields
     + Popup Menus
     + Scrolling lists

=item B<onSelect>

The user has changed the part of a text field that is selected.  Valid
for:

     + Text fields
     + Text areas
     + Password fields
     + File fields

=item B<onMouseOver>

The mouse has moved over an element.

     + Text fields
     + Text areas
     + Password fields
     + File fields
     + Popup Menus
     + Scrolling lists

=item B<onMouseOut>

The mouse has moved off an element.

     + Text fields
     + Text areas
     + Password fields
     + File fields
     + Popup Menus
     + Scrolling lists

=back

In order to register a JavaScript event handler with an HTML element,
just use the event name as a parameter when you call the corresponding
CGI method. For example, to have your validateAge() JavaScript code
executed every time the textfield named "age" changes, generate the
field like this: 

 print textfield(-name=>'age',-onChange=>"validateAge(this)");

This example assumes that you've already declared the validateAge()
function by incorporating it into a <SCRIPT> block. The CGI.pm
start_html() method provides a convenient way to create this section.

Similarly, you can create a form that checks itself over for
consistency and alerts the user if some essential value is missing by
creating it this way: 
  print startform(-onSubmit=>"validateMe(this)");

See the javascript.cgi script for a demonstration of how this all
works.


=head1 LIMITED SUPPORT FOR CASCADING STYLE SHEETS

CGI.pm has limited support for HTML3's cascading style sheets (css).
To incorporate a stylesheet into your document, pass the
start_html() method a B<-style> parameter.  The value of this
parameter may be a scalar, in which case it is treated as the source
URL for the stylesheet, or it may be a hash reference.  In the latter
case you should provide the hash with one or more of B<-src> or
B<-code>.  B<-src> points to a URL where an externally-defined
stylesheet can be found.  B<-code> points to a scalar value to be
incorporated into a <style> section.  Style definitions in B<-code>
override similarly-named ones in B<-src>, hence the name "cascading."

You may also specify the type of the stylesheet by adding the optional
B<-type> parameter to the hash pointed to by B<-style>.  If not
specified, the style defaults to 'text/css'.

To refer to a style within the body of your document, add the
B<-class> parameter to any HTML element:

    print h1({-class=>'Fancy'},'Welcome to the Party');

Or define styles on the fly with the B<-style> parameter:

    print h1({-style=>'Color: red;'},'Welcome to Hell');

You may also use the new B<span()> element to apply a style to a
section of text:

    print span({-style=>'Color: red;'},
	       h1('Welcome to Hell'),
	       "Where did that handbasket get to?"
	       );

Note that you must import the ":html3" definitions to have the
B<span()> method available.  Here's a quick and dirty example of using
CSS's.  See the CSS specification at
http://www.w3.org/pub/WWW/TR/Wd-css-1.html for more information.

    use CGI qw/:standard :html3/;

    #here's a stylesheet incorporated directly into the page
    $newStyle=<<END;
    <!-- 
    P.Tip {
	margin-right: 50pt;
	margin-left: 50pt;
        color: red;
    }
    P.Alert {
	font-size: 30pt;
        font-family: sans-serif;
      color: red;
    }
    -->
    END
    print header();
    print start_html( -title=>'CGI with Style',
		      -style=>{-src=>'http://www.capricorn.com/style/st1.css',
		               -code=>$newStyle}
	             );
    print h1('CGI with Style'),
          p({-class=>'Tip'},
	    "Better read the cascading style sheet spec before playing with this!"),
          span({-style=>'color: magenta'},
	       "Look Mom, no hands!",
	       p(),
	       "Whooo wee!"
	       );
    print end_html;

Pass an array reference to B<-code> or B<-src> in order to incorporate
multiple stylesheets into your document.

Should you wish to incorporate a verbatim stylesheet that includes
arbitrary formatting in the header, you may pass a -verbatim tag to
the -style hash, as follows:

print start_html (-style  =>  {-verbatim => '@import url("/server-common/css/'.$cssFile.'");',
                  -src    =>  '/server-common/css/core.css'});


This will generate an HTML header that contains this:

 <link rel="stylesheet" type="text/css"  href="/server-common/css/core.css">
   <style type="text/css">
   @import url("/server-common/css/main.css");
   </style>

Any additional arguments passed in the -style value will be
incorporated into the <link> tag.  For example:

 start_html(-style=>{-src=>['/styles/print.css','/styles/layout.css'],
			  -media => 'all'});

This will give:

 <link rel="stylesheet" type="text/css" href="/styles/print.css" media="all"/>
 <link rel="stylesheet" type="text/css" href="/styles/layout.css" media="all"/>

<p>

To make more complicated <link> tags, use the Link() function
and pass it to start_html() in the -head argument, as in:

  @h = (Link({-rel=>'stylesheet',-type=>'text/css',-src=>'/ss/ss.css',-media=>'all'}),
        Link({-rel=>'stylesheet',-type=>'text/css',-src=>'/ss/fred.css',-media=>'paper'}));
  print start_html({-head=>\@h})

To create primary and  "alternate" stylesheet, use the B<-alternate> option:

 start_html(-style=>{-src=>[
                           {-src=>'/styles/print.css'},
			   {-src=>'/styles/alt.css',-alternate=>1}
                           ]
		    });

=head1 DEBUGGING

If you are running the script from the command line or in the perl
debugger, you can pass the script a list of keywords or
parameter=value pairs on the command line or from standard input (you
don't have to worry about tricking your script into reading from
environment variables).  You can pass keywords like this:

    your_script.pl keyword1 keyword2 keyword3

or this:

   your_script.pl keyword1+keyword2+keyword3

or this:

    your_script.pl name1=value1 name2=value2

or this:

    your_script.pl name1=value1&name2=value2

To turn off this feature, use the -no_debug pragma.

To test the POST method, you may enable full debugging with the -debug
pragma.  This will allow you to feed newline-delimited name=value
pairs to the script on standard input.

When debugging, you can use quotes and backslashes to escape 
characters in the familiar shell manner, letting you place
spaces and other funny characters in your parameter=value
pairs:

   your_script.pl "name1='I am a long value'" "name2=two\ words"

Finally, you can set the path info for the script by prefixing the first
name/value parameter with the path followed by a question mark (?):

    your_script.pl /your/path/here?name1=value1&name2=value2

=head2 DUMPING OUT ALL THE NAME/VALUE PAIRS

The Dump() method produces a string consisting of all the query's
name/value pairs formatted nicely as a nested list.  This is useful
for debugging purposes:

    print Dump


Produces something that looks like:

    <ul>
    <li>name1
	<ul>
	<li>value1
	<li>value2
	</ul>
    <li>name2
	<ul>
	<li>value1
	</ul>
    </ul>

As a shortcut, you can interpolate the entire CGI object into a string
and it will be replaced with the a nice HTML dump shown above:

    $query=new CGI;
    print "<h2>Current Values</h2> $query\n";

=head1 FETCHING ENVIRONMENT VARIABLES

Some of the more useful environment variables can be fetched
through this interface.  The methods are as follows:

=over 4

=item B<Accept()>

Return a list of MIME types that the remote browser accepts. If you
give this method a single argument corresponding to a MIME type, as in
Accept('text/html'), it will return a floating point value
corresponding to the browser's preference for this type from 0.0
(don't want) to 1.0.  Glob types (e.g. text/*) in the browser's accept
list are handled correctly.

Note that the capitalization changed between version 2.43 and 2.44 in
order to avoid conflict with Perl's accept() function.

=item B<raw_cookie()>

Returns the HTTP_COOKIE variable, an HTTP extension implemented by
Netscape browsers version 1.1 and higher, and all versions of Internet
Explorer.  Cookies have a special format, and this method call just
returns the raw form (?cookie dough).  See cookie() for ways of
setting and retrieving cooked cookies.

Called with no parameters, raw_cookie() returns the packed cookie
structure.  You can separate it into individual cookies by splitting
on the character sequence "; ".  Called with the name of a cookie,
retrieves the B<unescaped> form of the cookie.  You can use the
regular cookie() method to get the names, or use the raw_fetch()
method from the CGI::Cookie module.

=item B<user_agent()>

Returns the HTTP_USER_AGENT variable.  If you give
this method a single argument, it will attempt to
pattern match on it, allowing you to do something
like user_agent(netscape);

=item B<path_info()>

Returns additional path information from the script URL.
E.G. fetching /cgi-bin/your_script/additional/stuff will result in
path_info() returning "/additional/stuff".

NOTE: The Microsoft Internet Information Server
is broken with respect to additional path information.  If
you use the Perl DLL library, the IIS server will attempt to
execute the additional path information as a Perl script.
If you use the ordinary file associations mapping, the
path information will be present in the environment, 
but incorrect.  The best thing to do is to avoid using additional
path information in CGI scripts destined for use with IIS.

=item B<path_translated()>

As per path_info() but returns the additional
path information translated into a physical path, e.g.
"/usr/local/etc/httpd/htdocs/additional/stuff".

The Microsoft IIS is broken with respect to the translated
path as well.

=item B<remote_host()>

Returns either the remote host name or IP address.
if the former is unavailable.

=item B<script_name()>
Return the script name as a partial URL, for self-refering
scripts.

=item B<referer()>

Return the URL of the page the browser was viewing
prior to fetching your script.  Not available for all
browsers.

=item B<auth_type ()>

Return the authorization/verification method in use for this
script, if any.

=item B<server_name ()>

Returns the name of the server, usually the machine's host
name.

=item B<virtual_host ()>

When using virtual hosts, returns the name of the host that
the browser attempted to contact

=item B<server_port ()>

Return the port that the server is listening on.

=item B<virtual_port ()>

Like server_port() except that it takes virtual hosts into account.
Use this when running with virtual hosts.

=item B<server_software ()>

Returns the server software and version number.

=item B<remote_user ()>

Return the authorization/verification name used for user
verification, if this script is protected.

=item B<user_name ()>

Attempt to obtain the remote user's name, using a variety of different
techniques.  This only works with older browsers such as Mosaic.
Newer browsers do not report the user name for privacy reasons!

=item B<request_method()>

Returns the method used to access your script, usually
one of 'POST', 'GET' or 'HEAD'.

=item B<content_type()>

Returns the content_type of data submitted in a POST, generally 
multipart/form-data or application/x-www-form-urlencoded

=item B<http()>

Called with no arguments returns the list of HTTP environment
variables, including such things as HTTP_USER_AGENT,
HTTP_ACCEPT_LANGUAGE, and HTTP_ACCEPT_CHARSET, corresponding to the
like-named HTTP header fields in the request.  Called with the name of
an HTTP header field, returns its value.  Capitalization and the use
of hyphens versus underscores are not significant.

For example, all three of these examples are equivalent:

   $requested_language = http('Accept-language');
   $requested_language = http('Accept_language');
   $requested_language = http('HTTP_ACCEPT_LANGUAGE');

=item B<https()>

The same as I<http()>, but operates on the HTTPS environment variables
present when the SSL protocol is in effect.  Can be used to determine
whether SSL is turned on.

=back

=head1 USING NPH SCRIPTS

NPH, or "no-parsed-header", scripts bypass the server completely by
sending the complete HTTP header directly to the browser.  This has
slight performance benefits, but is of most use for taking advantage
of HTTP extensions that are not directly supported by your server,
such as server push and PICS headers.

Servers use a variety of conventions for designating CGI scripts as
NPH.  Many Unix servers look at the beginning of the script's name for
the prefix "nph-".  The Macintosh WebSTAR server and Microsoft's
Internet Information Server, in contrast, try to decide whether a
program is an NPH script by examining the first line of script output.


CGI.pm supports NPH scripts with a special NPH mode.  When in this
mode, CGI.pm will output the necessary extra header information when
the header() and redirect() methods are
called.

The Microsoft Internet Information Server requires NPH mode.  As of
version 2.30, CGI.pm will automatically detect when the script is
running under IIS and put itself into this mode.  You do not need to
do this manually, although it won't hurt anything if you do.  However,
note that if you have applied Service Pack 6, much of the
functionality of NPH scripts, including the ability to redirect while
setting a cookie, b<do not work at all> on IIS without a special patch
from Microsoft.  See
http://support.microsoft.com/support/kb/articles/Q280/3/41.ASP:
Non-Parsed Headers Stripped From CGI Applications That Have nph-
Prefix in Name.

=over 4

=item In the B<use> statement 

Simply add the "-nph" pragmato the list of symbols to be imported into
your script:

      use CGI qw(:standard -nph)

=item By calling the B<nph()> method:

Call B<nph()> with a non-zero parameter at any point after using CGI.pm in your program.

      CGI->nph(1)

=item By using B<-nph> parameters

in the B<header()> and B<redirect()>  statements:

      print header(-nph=>1);

=back

=head1 Server Push

CGI.pm provides four simple functions for producing multipart
documents of the type needed to implement server push.  These
functions were graciously provided by Ed Jordan <ed@fidalgo.net>.  To
import these into your namespace, you must import the ":push" set.
You are also advised to put the script into NPH mode and to set $| to
1 to avoid buffering problems.

Here is a simple script that demonstrates server push:

  #!/usr/local/bin/perl
  use CGI qw/:push -nph/;
  $| = 1;
  print multipart_init(-boundary=>'----here we go!');
  foreach (0 .. 4) {
      print multipart_start(-type=>'text/plain'),
            "The current time is ",scalar(localtime),"\n";
      if ($_ < 4) {
              print multipart_end;
      } else {
              print multipart_final;
      }
      sleep 1;
  }

This script initializes server push by calling B<multipart_init()>.
It then enters a loop in which it begins a new multipart section by
calling B<multipart_start()>, prints the current local time,
and ends a multipart section with B<multipart_end()>.  It then sleeps
a second, and begins again. On the final iteration, it ends the
multipart section with B<multipart_final()> rather than with
B<multipart_end()>.

=over 4

=item multipart_init()

  multipart_init(-boundary=>$boundary);

Initialize the multipart system.  The -boundary argument specifies
what MIME boundary string to use to separate parts of the document.
If not provided, CGI.pm chooses a reasonable boundary for you.

=item multipart_start()

  multipart_start(-type=>$type)

Start a new part of the multipart document using the specified MIME
type.  If not specified, text/html is assumed.

=item multipart_end()

  multipart_end()

End a part.  You must remember to call multipart_end() once for each
multipart_start(), except at the end of the last part of the multipart
document when multipart_final() should be called instead of multipart_end().

=item multipart_final()

  multipart_final()

End all parts.  You should call multipart_final() rather than
multipart_end() at the end of the last part of the multipart document.

=back

Users interested in server push applications should also have a look
at the CGI::Push module.

Only Netscape Navigator supports server push.  Internet Explorer
browsers do not.

=head1 Avoiding Denial of Service Attacks

A potential problem with CGI.pm is that, by default, it attempts to
process form POSTings no matter how large they are.  A wily hacker
could attack your site by sending a CGI script a huge POST of many
megabytes.  CGI.pm will attempt to read the entire POST into a
variable, growing hugely in size until it runs out of memory.  While
the script attempts to allocate the memory the system may slow down
dramatically.  This is a form of denial of service attack.

Another possible attack is for the remote user to force CGI.pm to
accept a huge file upload.  CGI.pm will accept the upload and store it
in a temporary directory even if your script doesn't expect to receive
an uploaded file.  CGI.pm will delete the file automatically when it
terminates, but in the meantime the remote user may have filled up the
server's disk space, causing problems for other programs.

The best way to avoid denial of service attacks is to limit the amount
of memory, CPU time and disk space that CGI scripts can use.  Some Web
servers come with built-in facilities to accomplish this. In other
cases, you can use the shell I<limit> or I<ulimit>
commands to put ceilings on CGI resource usage.


CGI.pm also has some simple built-in protections against denial of
service attacks, but you must activate them before you can use them.
These take the form of two global variables in the CGI name space:

=over 4

=item B<$CGI::POST_MAX>

If set to a non-negative integer, this variable puts a ceiling
on the size of POSTings, in bytes.  If CGI.pm detects a POST
that is greater than the ceiling, it will immediately exit with an error
message.  This value will affect both ordinary POSTs and
multipart POSTs, meaning that it limits the maximum size of file
uploads as well.  You should set this to a reasonably high
value, such as 1 megabyte.

=item B<$CGI::DISABLE_UPLOADS>

If set to a non-zero value, this will disable file uploads
completely.  Other fill-out form values will work as usual.

=back

You can use these variables in either of two ways.

=over 4

=item B<1. On a script-by-script basis>

Set the variable at the top of the script, right after the "use" statement:

    use CGI qw/:standard/;
    use CGI::Carp 'fatalsToBrowser';
    $CGI::POST_MAX=1024 * 100;  # max 100K posts
    $CGI::DISABLE_UPLOADS = 1;  # no uploads

=item B<2. Globally for all scripts>

Open up CGI.pm, find the definitions for $POST_MAX and 
$DISABLE_UPLOADS, and set them to the desired values.  You'll 
find them towards the top of the file in a subroutine named 
initialize_globals().

=back

An attempt to send a POST larger than $POST_MAX bytes will cause
I<param()> to return an empty CGI parameter list.  You can test for
this event by checking I<cgi_error()>, either after you create the CGI
object or, if you are using the function-oriented interface, call
<param()> for the first time.  If the POST was intercepted, then
cgi_error() will return the message "413 POST too large".

This error message is actually defined by the HTTP protocol, and is
designed to be returned to the browser as the CGI script's status
 code.  For example:

   $uploaded_file = param('upload');
   if (!$uploaded_file && cgi_error()) {
      print header(-status=>cgi_error());
      exit 0;
   }

However it isn't clear that any browser currently knows what to do
with this status code.  It might be better just to create an
HTML page that warns the user of the problem.

=head1 COMPATIBILITY WITH CGI-LIB.PL

To make it easier to port existing programs that use cgi-lib.pl the
compatibility routine "ReadParse" is provided.  Porting is simple:

OLD VERSION
    require "cgi-lib.pl";
    &ReadParse;
    print "The value of the antique is $in{antique}.\n";

NEW VERSION
    use CGI;
    CGI::ReadParse();
    print "The value of the antique is $in{antique}.\n";

CGI.pm's ReadParse() routine creates a tied variable named %in,
which can be accessed to obtain the query variables.  Like
ReadParse, you can also provide your own variable.  Infrequently
used features of ReadParse, such as the creation of @in and $in 
variables, are not supported.

Once you use ReadParse, you can retrieve the query object itself
this way:

    $q = $in{CGI};
    print textfield(-name=>'wow',
			-value=>'does this really work?');

This allows you to start using the more interesting features
of CGI.pm without rewriting your old scripts from scratch.

=head1 AUTHOR INFORMATION

The GD.pm interface is copyright 1995-2007, Lincoln D. Stein.  It is
distributed under GPL and the Artistic License 2.0.

Address bug reports and comments to: lstein@cshl.org.  When sending
bug reports, please provide the version of CGI.pm, the version of
Perl, the name and version of your Web server, and the name and
version of the operating system you are using.  If the problem is even
remotely browser dependent, please provide information about the
affected browers as well.

=head1 CREDITS

Thanks very much to:

=over 4

=item Matt Heffron (heffron@falstaff.css.beckman.com)

=item James Taylor (james.taylor@srs.gov)

=item Scott Anguish <sanguish@digifix.com>

=item Mike Jewell (mlj3u@virginia.edu)

=item Timothy Shimmin (tes@kbs.citri.edu.au)

=item Joergen Haegg (jh@axis.se)

=item Laurent Delfosse (delfosse@delfosse.com)

=item Richard Resnick (applepi1@aol.com)

=item Craig Bishop (csb@barwonwater.vic.gov.au)

=item Tony Curtis (tc@vcpc.univie.ac.at)

=item Tim Bunce (Tim.Bunce@ig.co.uk)

=item Tom Christiansen (tchrist@convex.com)

=item Andreas Koenig (k@franz.ww.TU-Berlin.DE)

=item Tim MacKenzie (Tim.MacKenzie@fulcrum.com.au)

=item Kevin B. Hendricks (kbhend@dogwood.tyler.wm.edu)

=item Stephen Dahmen (joyfire@inxpress.net)

=item Ed Jordan (ed@fidalgo.net)

=item David Alan Pisoni (david@cnation.com)

=item Doug MacEachern (dougm@opengroup.org)

=item Robin Houston (robin@oneworld.org)

=item ...and many many more...

for suggestions and bug fixes.

=back

=head1 A COMPLETE EXAMPLE OF A SIMPLE FORM-BASED SCRIPT


	#!/usr/local/bin/perl

	use CGI ':standard';

	print header;
	print start_html("Example CGI.pm Form");
	print "<h1> Example CGI.pm Form</h1>\n";
        print_prompt();
	do_work();
	print_tail();
	print end_html;

	sub print_prompt {
	   print start_form;
	   print "<em>What's your name?</em><br>";
	   print textfield('name');
	   print checkbox('Not my real name');

	   print "<p><em>Where can you find English Sparrows?</em><br>";
	   print checkbox_group(
				 -name=>'Sparrow locations',
				 -values=>[England,France,Spain,Asia,Hoboken],
				 -linebreak=>'yes',
				 -defaults=>[England,Asia]);

	   print "<p><em>How far can they fly?</em><br>",
		radio_group(
			-name=>'how far',
			-values=>['10 ft','1 mile','10 miles','real far'],
			-default=>'1 mile');

	   print "<p><em>What's your favorite color?</em>  ";
	   print popup_menu(-name=>'Color',
				    -values=>['black','brown','red','yellow'],
				    -default=>'red');

	   print hidden('Reference','Monty Python and the Holy Grail');

	   print "<p><em>What have you got there?</em><br>";
	   print scrolling_list(
			 -name=>'possessions',
			 -values=>['A Coconut','A Grail','An Icon',
				   'A Sword','A Ticket'],
			 -size=>5,
			 -multiple=>'true');

	   print "<p><em>Any parting comments?</em><br>";
	   print textarea(-name=>'Comments',
				  -rows=>10,
				  -columns=>50);

	   print "<p>",reset;
	   print submit('Action','Shout');
	   print submit('Action','Scream');
	   print endform;
	   print "<hr>\n";
	}

	sub do_work {
	   my(@values,$key);

	   print "<h2>Here are the current settings in this form</h2>";

	   foreach $key (param) {
	      print "<strong>$key</strong> -> ";
	      @values = param($key);
	      print join(", ",@values),"<br>\n";
	  }
	}

	sub print_tail {
	   print <<END;
	<hr>
	<address>Lincoln D. Stein</address><br>
	<a href="/">Home Page</a>
	END
	}

=head1 BUGS

Please report them.

=head1 SEE ALSO

L<CGI::Carp>, L<CGI::Fast>, L<CGI::Pretty>

=cut

