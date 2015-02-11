# -*- mode: perl; perl-indent-level: 2; -*-
# Memoize.pm
#
# Transparent memoization of idempotent functions
#
# Copyright 1998, 1999, 2000, 2001 M-J. Dominus.
# You may copy and distribute this program under the
# same terms as Perl itself.  If in doubt, 
# write to mjd-perl-memoize+@plover.com for a license.
#
# Version 1.01 $Revision: 1.18 $ $Date: 2001/06/24 17:16:47 $

package Memoize;
$VERSION = '1.01_02';

# Compile-time constants
sub SCALAR () { 0 } 
sub LIST () { 1 } 


#
# Usage memoize(functionname/ref,
#               { NORMALIZER => coderef, INSTALL => name,
#                 LIST_CACHE => descriptor, SCALAR_CACHE => descriptor }
#

use Carp;
use Exporter;
use vars qw($DEBUG);
use Config;                     # Dammit.
@ISA = qw(Exporter);
@EXPORT = qw(memoize);
@EXPORT_OK = qw(unmemoize flush_cache);
use strict;

my %memotable;
my %revmemotable;
my @CONTEXT_TAGS = qw(MERGE TIE MEMORY FAULT HASH);
my %IS_CACHE_TAG = map {($_ => 1)} @CONTEXT_TAGS;

# Raise an error if the user tries to specify one of thesepackage as a
# tie for LIST_CACHE

my %scalar_only = map {($_ => 1)} qw(DB_File GDBM_File SDBM_File ODBM_File NDBM_File);

sub memoize {
  my $fn = shift;
  my %options = @_;
  my $options = \%options;
  
  unless (defined($fn) && 
	  (ref $fn eq 'CODE' || ref $fn eq '')) {
    croak "Usage: memoize 'functionname'|coderef {OPTIONS}";
  }

  my $uppack = caller;		# TCL me Elmo!
  my $cref;			# Code reference to original function
  my $name = (ref $fn ? undef : $fn);

  # Convert function names to code references
  $cref = &_make_cref($fn, $uppack);

  # Locate function prototype, if any
  my $proto = prototype $cref;
  if (defined $proto) { $proto = "($proto)" }
  else { $proto = "" }

  # I would like to get rid of the eval, but there seems not to be any
  # other way to set the prototype properly.  The switch here for
  # 'usethreads' works around a bug in threadperl having to do with
  # magic goto.  It would be better to fix the bug and use the magic
  # goto version everywhere.
  my $wrapper = 
      $Config{usethreads} 
        ? eval "sub $proto { &_memoizer(\$cref, \@_); }" 
        : eval "sub $proto { unshift \@_, \$cref; goto &_memoizer; }";

  my $normalizer = $options{NORMALIZER};
  if (defined $normalizer  && ! ref $normalizer) {
    $normalizer = _make_cref($normalizer, $uppack);
  }
  
  my $install_name;
  if (defined $options->{INSTALL}) {
    # INSTALL => name
    $install_name = $options->{INSTALL};
  } elsif (! exists $options->{INSTALL}) {
    # No INSTALL option provided; use original name if possible
    $install_name = $name;
  } else {
    # INSTALL => undef  means don't install
  }

  if (defined $install_name) {
    $install_name = $uppack . '::' . $install_name
	unless $install_name =~ /::/;
    no strict;
    local($^W) = 0;	       # ``Subroutine $install_name redefined at ...''
    *{$install_name} = $wrapper; # Install memoized version
  }

  $revmemotable{$wrapper} = "" . $cref; # Turn code ref into hash key

  # These will be the caches
  my %caches;
  for my $context (qw(SCALAR LIST)) {
    # suppress subsequent 'uninitialized value' warnings
    $options{"${context}_CACHE"} ||= ''; 

    my $cache_opt = $options{"${context}_CACHE"};
    my @cache_opt_args;
    if (ref $cache_opt) {
      @cache_opt_args = @$cache_opt;
      $cache_opt = shift @cache_opt_args;
    }
    if ($cache_opt eq 'FAULT') { # no cache
      $caches{$context} = undef;
    } elsif ($cache_opt eq 'HASH') { # user-supplied hash
      my $cache = $cache_opt_args[0];
      my $package = ref(tied %$cache);
      if ($context eq 'LIST' && $scalar_only{$package}) {
        croak("You can't use $package for LIST_CACHE because it can only store scalars");
      }
      $caches{$context} = $cache;
    } elsif ($cache_opt eq '' ||  $IS_CACHE_TAG{$cache_opt}) {
      # default is that we make up an in-memory hash
      $caches{$context} = {};
      # (this might get tied later, or MERGEd away)
    } else {
      croak "Unrecognized option to `${context}_CACHE': `$cache_opt' should be one of (@CONTEXT_TAGS); aborting";
    }
  }

  # Perhaps I should check here that you didn't supply *both* merge
  # options.  But if you did, it does do something reasonable: They
  # both get merged to the same in-memory hash.
  if ($options{SCALAR_CACHE} eq 'MERGE') {
    $caches{SCALAR} = $caches{LIST};
  } elsif ($options{LIST_CACHE} eq 'MERGE') {
    $caches{LIST} = $caches{SCALAR};
  }

  # Now deal with the TIE options
  {
    my $context;
    foreach $context (qw(SCALAR LIST)) {
      # If the relevant option wasn't `TIE', this call does nothing.
      _my_tie($context, $caches{$context}, $options);  # Croaks on failure
    }
  }
  
  # We should put some more stuff in here eventually.
  # We've been saying that for serveral versions now.
  # And you know what?  More stuff keeps going in!
  $memotable{$cref} = 
  {
    O => $options,  # Short keys here for things we need to access frequently
    N => $normalizer,
    U => $cref,
    MEMOIZED => $wrapper,
    PACKAGE => $uppack,
    NAME => $install_name,
    S => $caches{SCALAR},
    L => $caches{LIST},
  };

  $wrapper			# Return just memoized version
}

# This function tries to load a tied hash class and tie the hash to it.
sub _my_tie {
  my ($context, $hash, $options) = @_;
  my $fullopt = $options->{"${context}_CACHE"};

  # We already checked to make sure that this works.
  my $shortopt = (ref $fullopt) ? $fullopt->[0] : $fullopt;
  
  return unless defined $shortopt && $shortopt eq 'TIE';
  carp("TIE option to memoize() is deprecated; use HASH instead")
      if $^W;

  my @args = ref $fullopt ? @$fullopt : ();
  shift @args;
  my $module = shift @args;
  if ($context eq 'LIST' && $scalar_only{$module}) {
    croak("You can't use $module for LIST_CACHE because it can only store scalars");
  }
  my $modulefile = $module . '.pm';
  $modulefile =~ s{::}{/}g;
  eval { require $modulefile };
  if ($@) {
    croak "Memoize: Couldn't load hash tie module `$module': $@; aborting";
  }
  my $rc = (tie %$hash => $module, @args);
  unless ($rc) {
    croak "Memoize: Couldn't tie hash to `$module': $!; aborting";
  }
  1;
}

sub flush_cache {
  my $func = _make_cref($_[0], scalar caller);
  my $info = $memotable{$revmemotable{$func}};
  die "$func not memoized" unless defined $info;
  for my $context (qw(S L)) {
    my $cache = $info->{$context};
    if (tied %$cache && ! (tied %$cache)->can('CLEAR')) {
      my $funcname = defined($info->{NAME}) ? 
          "function $info->{NAME}" : "anonymous function $func";
      my $context = {S => 'scalar', L => 'list'}->{$context};
      croak "Tied cache hash for $context-context $funcname does not support flushing";
    } else {
      %$cache = ();
    }
  }
}

# This is the function that manages the memo tables.
sub _memoizer {
  my $orig = shift;		# stringized version of ref to original func.
  my $info = $memotable{$orig};
  my $normalizer = $info->{N};
  
  my $argstr;
  my $context = (wantarray() ? LIST : SCALAR);

  if (defined $normalizer) { 
    no strict;
    if ($context == SCALAR) {
      $argstr = &{$normalizer}(@_);
    } elsif ($context == LIST) {
      ($argstr) = &{$normalizer}(@_);
    } else {
      croak "Internal error \#41; context was neither LIST nor SCALAR\n";
    }
  } else {                      # Default normalizer
    local $^W = 0;
    $argstr = join chr(28),@_;  
  }

  if ($context == SCALAR) {
    my $cache = $info->{S};
    _crap_out($info->{NAME}, 'scalar') unless $cache;
    if (exists $cache->{$argstr}) { 
      return $cache->{$argstr};
    } else {
      my $val = &{$info->{U}}(@_);
      # Scalars are considered to be lists; store appropriately
      if ($info->{O}{SCALAR_CACHE} eq 'MERGE') {
	$cache->{$argstr} = [$val];
      } else {
	$cache->{$argstr} = $val;
      }
      $val;
    }
  } elsif ($context == LIST) {
    my $cache = $info->{L};
    _crap_out($info->{NAME}, 'list') unless $cache;
    if (exists $cache->{$argstr}) {
      my $val = $cache->{$argstr};
      # If LISTCONTEXT=>MERGE, then the function never returns lists,
      # so we have a scalar value cached, so just return it straightaway:
      return ($val) if $info->{O}{LIST_CACHE} eq 'MERGE';
      # Maybe in a later version we can use a faster test.

      # Otherwise, we cached an array containing the returned list:
      return @$val;
    } else {
        my @q = &{$info->{U}}(@_);
        $cache->{$argstr} = $info->{O}{LIST_CACHE} eq 'MERGE' ? $q [0] : \@q;
        @q;
    }
  } else {
    croak "Internal error \#42; context was neither LIST nor SCALAR\n";
  }
}

sub unmemoize {
  my $f = shift;
  my $uppack = caller;
  my $cref = _make_cref($f, $uppack);

  unless (exists $revmemotable{$cref}) {
    croak "Could not unmemoize function `$f', because it was not memoized to begin with";
  }
  
  my $tabent = $memotable{$revmemotable{$cref}};
  unless (defined $tabent) {
    croak "Could not figure out how to unmemoize function `$f'";
  }
  my $name = $tabent->{NAME};
  if (defined $name) {
    no strict;
    local($^W) = 0;	       # ``Subroutine $install_name redefined at ...''
    *{$name} = $tabent->{U}; # Replace with original function
  }
  undef $memotable{$revmemotable{$cref}};
  undef $revmemotable{$cref};

  # This removes the last reference to the (possibly tied) memo tables
  # my ($old_function, $memotabs) = @{$tabent}{'U','S','L'};
  # undef $tabent; 

#  # Untie the memo tables if they were tied.
#  my $i;
#  for $i (0,1) {
#    if (tied %{$memotabs->[$i]}) {
#      warn "Untying hash #$i\n";
#      untie %{$memotabs->[$i]};
#    }
#  }

  $tabent->{U};
}

sub _make_cref {
  my $fn = shift;
  my $uppack = shift;
  my $cref;
  my $name;

  if (ref $fn eq 'CODE') {
    $cref = $fn;
  } elsif (! ref $fn) {
    if ($fn =~ /::/) {
      $name = $fn;
    } else {
      $name = $uppack . '::' . $fn;
    }
    no strict;
    if (defined $name and !defined(&$name)) {
      croak "Cannot operate on nonexistent function `$fn'";
    }
#    $cref = \&$name;
    $cref = *{$name}{CODE};
  } else {
    my $parent = (caller(1))[3]; # Function that called _make_cref
    croak "Usage: argument 1 to `$parent' must be a function name or reference.\n";
  }
  $DEBUG and warn "${name}($fn) => $cref in _make_cref\n";
  $cref;
}

sub _crap_out {
  my ($funcname, $context) = @_;
  if (defined $funcname) {
    croak "Function `$funcname' called in forbidden $context context; faulting";
  } else {
    croak "Anonymous function called in forbidden $context context; faulting";
  }
}

1;





=head1 NAME

Memoize - Make functions faster by trading space for time

=head1 SYNOPSIS

        # This is the documentation for Memoize 1.01
	use Memoize;
	memoize('slow_function');
	slow_function(arguments);    # Is faster than it was before


This is normally all you need to know.  However, many options are available:

	memoize(function, options...);

Options include:

	NORMALIZER => function
	INSTALL => new_name

	SCALAR_CACHE => 'MEMORY'
        SCALAR_CACHE => ['HASH', \%cache_hash ]
	SCALAR_CACHE => 'FAULT'
	SCALAR_CACHE => 'MERGE'

	LIST_CACHE => 'MEMORY'
        LIST_CACHE => ['HASH', \%cache_hash ]
	LIST_CACHE => 'FAULT'
	LIST_CACHE => 'MERGE'

=head1 DESCRIPTION

`Memoizing' a function makes it faster by trading space for time.  It
does this by caching the return values of the function in a table.
If you call the function again with the same arguments, C<memoize>
jumps in and gives you the value out of the table, instead of letting
the function compute the value all over again.

Here is an extreme example.  Consider the Fibonacci sequence, defined
by the following function:

	# Compute Fibonacci numbers
	sub fib {
	  my $n = shift;
	  return $n if $n < 2;
	  fib($n-1) + fib($n-2);
	}

This function is very slow.  Why?  To compute fib(14), it first wants
to compute fib(13) and fib(12), and add the results.  But to compute
fib(13), it first has to compute fib(12) and fib(11), and then it
comes back and computes fib(12) all over again even though the answer
is the same.  And both of the times that it wants to compute fib(12),
it has to compute fib(11) from scratch, and then it has to do it
again each time it wants to compute fib(13).  This function does so
much recomputing of old results that it takes a really long time to
run---fib(14) makes 1,200 extra recursive calls to itself, to compute
and recompute things that it already computed.

This function is a good candidate for memoization.  If you memoize the
`fib' function above, it will compute fib(14) exactly once, the first
time it needs to, and then save the result in a table.  Then if you
ask for fib(14) again, it gives you the result out of the table.
While computing fib(14), instead of computing fib(12) twice, it does
it once; the second time it needs the value it gets it from the table.
It doesn't compute fib(11) four times; it computes it once, getting it
from the table the next three times.  Instead of making 1,200
recursive calls to `fib', it makes 15.  This makes the function about
150 times faster.

You could do the memoization yourself, by rewriting the function, like
this:

	# Compute Fibonacci numbers, memoized version
	{ my @fib;
  	  sub fib {
	    my $n = shift;
	    return $fib[$n] if defined $fib[$n];
	    return $fib[$n] = $n if $n < 2;
	    $fib[$n] = fib($n-1) + fib($n-2);
	  }
        }

Or you could use this module, like this:

	use Memoize;
	memoize('fib');

	# Rest of the fib function just like the original version.

This makes it easy to turn memoizing on and off.

Here's an even simpler example: I wrote a simple ray tracer; the
program would look in a certain direction, figure out what it was
looking at, and then convert the `color' value (typically a string
like `red') of that object to a red, green, and blue pixel value, like
this:

    for ($direction = 0; $direction < 300; $direction++) {
      # Figure out which object is in direction $direction
      $color = $object->{color};
      ($r, $g, $b) = @{&ColorToRGB($color)};
      ...
    }

Since there are relatively few objects in a picture, there are only a
few colors, which get looked up over and over again.  Memoizing
C<ColorToRGB> sped up the program by several percent.

=head1 DETAILS

This module exports exactly one function, C<memoize>.  The rest of the
functions in this package are None of Your Business.

You should say

	memoize(function)

where C<function> is the name of the function you want to memoize, or
a reference to it.  C<memoize> returns a reference to the new,
memoized version of the function, or C<undef> on a non-fatal error.
At present, there are no non-fatal errors, but there might be some in
the future.

If C<function> was the name of a function, then C<memoize> hides the
old version and installs the new memoized version under the old name,
so that C<&function(...)> actually invokes the memoized version.

=head1 OPTIONS

There are some optional options you can pass to C<memoize> to change
the way it behaves a little.  To supply options, invoke C<memoize>
like this:

	memoize(function, NORMALIZER => function,
			  INSTALL => newname,
                          SCALAR_CACHE => option,
	                  LIST_CACHE => option
			 );

Each of these options is optional; you can include some, all, or none
of them.

=head2 INSTALL

If you supply a function name with C<INSTALL>, memoize will install
the new, memoized version of the function under the name you give.
For example, 

	memoize('fib', INSTALL => 'fastfib')

installs the memoized version of C<fib> as C<fastfib>; without the
C<INSTALL> option it would have replaced the old C<fib> with the
memoized version.  

To prevent C<memoize> from installing the memoized version anywhere, use
C<INSTALL =E<gt> undef>.

=head2 NORMALIZER

Suppose your function looks like this:

	# Typical call: f('aha!', A => 11, B => 12);
	sub f {
	  my $a = shift;
	  my %hash = @_;
	  $hash{B} ||= 2;  # B defaults to 2
	  $hash{C} ||= 7;  # C defaults to 7

	  # Do something with $a, %hash
	}

Now, the following calls to your function are all completely equivalent:

	f(OUCH);
	f(OUCH, B => 2);
	f(OUCH, C => 7);
	f(OUCH, B => 2, C => 7);
	f(OUCH, C => 7, B => 2);
	(etc.)

However, unless you tell C<Memoize> that these calls are equivalent,
it will not know that, and it will compute the values for these
invocations of your function separately, and store them separately.

To prevent this, supply a C<NORMALIZER> function that turns the
program arguments into a string in a way that equivalent arguments
turn into the same string.  A C<NORMALIZER> function for C<f> above
might look like this:

	sub normalize_f {
	  my $a = shift;
	  my %hash = @_;
	  $hash{B} ||= 2;
	  $hash{C} ||= 7;

	  join(',', $a, map ($_ => $hash{$_}) sort keys %hash);
	}

Each of the argument lists above comes out of the C<normalize_f>
function looking exactly the same, like this:

	OUCH,B,2,C,7

You would tell C<Memoize> to use this normalizer this way:

	memoize('f', NORMALIZER => 'normalize_f');

C<memoize> knows that if the normalized version of the arguments is
the same for two argument lists, then it can safely look up the value
that it computed for one argument list and return it as the result of
calling the function with the other argument list, even if the
argument lists look different.

The default normalizer just concatenates the arguments with character
28 in between.  (In ASCII, this is called FS or control-\.)  This
always works correctly for functions with only one string argument,
and also when the arguments never contain character 28.  However, it
can confuse certain argument lists:

	normalizer("a\034", "b")
	normalizer("a", "\034b")
	normalizer("a\034\034b")

for example.

Since hash keys are strings, the default normalizer will not
distinguish between C<undef> and the empty string.  It also won't work
when the function's arguments are references.  For example, consider a
function C<g> which gets two arguments: A number, and a reference to
an array of numbers:

	g(13, [1,2,3,4,5,6,7]);

The default normalizer will turn this into something like
C<"13\034ARRAY(0x436c1f)">.  That would be all right, except that a
subsequent array of numbers might be stored at a different location
even though it contains the same data.  If this happens, C<Memoize>
will think that the arguments are different, even though they are
equivalent.  In this case, a normalizer like this is appropriate:

	sub normalize { join ' ', $_[0], @{$_[1]} }

For the example above, this produces the key "13 1 2 3 4 5 6 7".

Another use for normalizers is when the function depends on data other
than those in its arguments.  Suppose you have a function which
returns a value which depends on the current hour of the day:

	sub on_duty {
          my ($problem_type) = @_;
	  my $hour = (localtime)[2];
          open my $fh, "$DIR/$problem_type" or die...;
          my $line;
          while ($hour-- > 0)
            $line = <$fh>;
          } 
	  return $line;
	}

At 10:23, this function generates the 10th line of a data file; at
3:45 PM it generates the 15th line instead.  By default, C<Memoize>
will only see the $problem_type argument.  To fix this, include the
current hour in the normalizer:

        sub normalize { join ' ', (localtime)[2], @_ }

The calling context of the function (scalar or list context) is
propagated to the normalizer.  This means that if the memoized
function will treat its arguments differently in list context than it
would in scalar context, you can have the normalizer function select
its behavior based on the results of C<wantarray>.  Even if called in
a list context, a normalizer should still return a single string.

=head2 C<SCALAR_CACHE>, C<LIST_CACHE>

Normally, C<Memoize> caches your function's return values into an
ordinary Perl hash variable.  However, you might like to have the
values cached on the disk, so that they persist from one run of your
program to the next, or you might like to associate some other
interesting semantics with the cached values.

There's a slight complication under the hood of C<Memoize>: There are
actually I<two> caches, one for scalar values and one for list values.
When your function is called in scalar context, its return value is
cached in one hash, and when your function is called in list context,
its value is cached in the other hash.  You can control the caching
behavior of both contexts independently with these options.

The argument to C<LIST_CACHE> or C<SCALAR_CACHE> must either be one of
the following four strings:

	MEMORY
	FAULT
	MERGE
        HASH

or else it must be a reference to a list whose first element is one of
these four strings, such as C<[HASH, arguments...]>.

=over 4

=item C<MEMORY>

C<MEMORY> means that return values from the function will be cached in
an ordinary Perl hash variable.  The hash variable will not persist
after the program exits.  This is the default.

=item C<HASH>

C<HASH> allows you to specify that a particular hash that you supply
will be used as the cache.  You can tie this hash beforehand to give
it any behavior you want.

A tied hash can have any semantics at all.  It is typically tied to an
on-disk database, so that cached values are stored in the database and
retrieved from it again when needed, and the disk file typically
persists after your program has exited.  See C<perltie> for more
complete details about C<tie>.

A typical example is:

        use DB_File;
        tie my %cache => 'DB_File', $filename, O_RDWR|O_CREAT, 0666;
        memoize 'function', SCALAR_CACHE => [HASH => \%cache];

This has the effect of storing the cache in a C<DB_File> database
whose name is in C<$filename>.  The cache will persist after the
program has exited.  Next time the program runs, it will find the
cache already populated from the previous run of the program.  Or you
can forcibly populate the cache by constructing a batch program that
runs in the background and populates the cache file.  Then when you
come to run your real program the memoized function will be fast
because all its results have been precomputed.

=item C<TIE>

This option is no longer supported.  It is still documented only to
aid in the debugging of old programs that use it.  Old programs should
be converted to use the C<HASH> option instead.

        memoize ... [TIE, PACKAGE, ARGS...]

is merely a shortcut for

        require PACKAGE;
	{ my %cache;
          tie %cache, PACKAGE, ARGS...;
	}
        memoize ... [HASH => \%cache];

=item C<FAULT>

C<FAULT> means that you never expect to call the function in scalar
(or list) context, and that if C<Memoize> detects such a call, it
should abort the program.  The error message is one of

	`foo' function called in forbidden list context at line ...
	`foo' function called in forbidden scalar context at line ...

=item C<MERGE>

C<MERGE> normally means the function does not distinguish between list
and sclar context, and that return values in both contexts should be
stored together.  C<LIST_CACHE =E<gt> MERGE> means that list context
return values should be stored in the same hash that is used for
scalar context returns, and C<SCALAR_CACHE =E<gt> MERGE> means the
same, mutatis mutandis.  It is an error to specify C<MERGE> for both,
but it probably does something useful.

Consider this function:

	sub pi { 3; }

Normally, the following code will result in two calls to C<pi>:

    $x = pi();
    ($y) = pi();
    $z = pi();

The first call caches the value C<3> in the scalar cache; the second
caches the list C<(3)> in the list cache.  The third call doesn't call
the real C<pi> function; it gets the value from the scalar cache.

Obviously, the second call to C<pi> is a waste of time, and storing
its return value is a waste of space.  Specifying C<LIST_CACHE =E<gt>
MERGE> will make C<memoize> use the same cache for scalar and list
context return values, so that the second call uses the scalar cache
that was populated by the first call.  C<pi> ends up being called only
once, and both subsequent calls return C<3> from the cache, regardless
of the calling context.

Another use for C<MERGE> is when you want both kinds of return values
stored in the same disk file; this saves you from having to deal with
two disk files instead of one.  You can use a normalizer function to
keep the two sets of return values separate.  For example:

        tie my %cache => 'MLDBM', 'DB_File', $filename, ...;

	memoize 'myfunc',
	  NORMALIZER => 'n',
	  SCALAR_CACHE => [HASH => \%cache],
	  LIST_CACHE => MERGE,
	;

	sub n {
	  my $context = wantarray() ? 'L' : 'S';
	  # ... now compute the hash key from the arguments ...
	  $hashkey = "$context:$hashkey";
	}

This normalizer function will store scalar context return values in
the disk file under keys that begin with C<S:>, and list context
return values under keys that begin with C<L:>.

=back

=head1 OTHER FACILITIES

=head2 C<unmemoize>

There's an C<unmemoize> function that you can import if you want to.
Why would you want to?  Here's an example: Suppose you have your cache
tied to a DBM file, and you want to make sure that the cache is
written out to disk if someone interrupts the program.  If the program
exits normally, this will happen anyway, but if someone types
control-C or something then the program will terminate immediately
without synchronizing the database.  So what you can do instead is

    $SIG{INT} = sub { unmemoize 'function' };

C<unmemoize> accepts a reference to, or the name of a previously
memoized function, and undoes whatever it did to provide the memoized
version in the first place, including making the name refer to the
unmemoized version if appropriate.  It returns a reference to the
unmemoized version of the function.

If you ask it to unmemoize a function that was never memoized, it
croaks.

=head2 C<flush_cache>

C<flush_cache(function)> will flush out the caches, discarding I<all>
the cached data.  The argument may be a function name or a reference
to a function.  For finer control over when data is discarded or
expired, see the documentation for C<Memoize::Expire>, included in
this package.

Note that if the cache is a tied hash, C<flush_cache> will attempt to
invoke the C<CLEAR> method on the hash.  If there is no C<CLEAR>
method, this will cause a run-time error.

An alternative approach to cache flushing is to use the C<HASH> option
(see above) to request that C<Memoize> use a particular hash variable
as its cache.  Then you can examine or modify the hash at any time in
any way you desire.  You may flush the cache by using C<%hash = ()>. 

=head1 CAVEATS

Memoization is not a cure-all:

=over 4

=item *

Do not memoize a function whose behavior depends on program
state other than its own arguments, such as global variables, the time
of day, or file input.  These functions will not produce correct
results when memoized.  For a particularly easy example:

	sub f {
	  time;
	}

This function takes no arguments, and as far as C<Memoize> is
concerned, it always returns the same result.  C<Memoize> is wrong, of
course, and the memoized version of this function will call C<time> once
to get the current time, and it will return that same time
every time you call it after that.

=item *

Do not memoize a function with side effects.

	sub f {
	  my ($a, $b) = @_;
          my $s = $a + $b;
	  print "$a + $b = $s.\n";
	}

This function accepts two arguments, adds them, and prints their sum.
Its return value is the numuber of characters it printed, but you
probably didn't care about that.  But C<Memoize> doesn't understand
that.  If you memoize this function, you will get the result you
expect the first time you ask it to print the sum of 2 and 3, but
subsequent calls will return 1 (the return value of
C<print>) without actually printing anything.

=item *

Do not memoize a function that returns a data structure that is
modified by its caller.

Consider these functions:  C<getusers> returns a list of users somehow,
and then C<main> throws away the first user on the list and prints the
rest:

	sub main {
	  my $userlist = getusers();
	  shift @$userlist;
	  foreach $u (@$userlist) {
	    print "User $u\n";
	  }
	}

	sub getusers {
	  my @users;
	  # Do something to get a list of users;
	  \@users;  # Return reference to list.
	}

If you memoize C<getusers> here, it will work right exactly once.  The
reference to the users list will be stored in the memo table.  C<main>
will discard the first element from the referenced list.  The next
time you invoke C<main>, C<Memoize> will not call C<getusers>; it will
just return the same reference to the same list it got last time.  But
this time the list has already had its head removed; C<main> will
erroneously remove another element from it.  The list will get shorter
and shorter every time you call C<main>.

Similarly, this:

	$u1 = getusers();    
	$u2 = getusers();    
	pop @$u1;

will modify $u2 as well as $u1, because both variables are references
to the same array.  Had C<getusers> not been memoized, $u1 and $u2
would have referred to different arrays.

=item * 

Do not memoize a very simple function.

Recently someone mentioned to me that the Memoize module made his
program run slower instead of faster.  It turned out that he was
memoizing the following function:

    sub square {
      $_[0] * $_[0];
    }

I pointed out that C<Memoize> uses a hash, and that looking up a
number in the hash is necessarily going to take a lot longer than a
single multiplication.  There really is no way to speed up the
C<square> function.

Memoization is not magical.

=back

=head1 PERSISTENT CACHE SUPPORT

You can tie the cache tables to any sort of tied hash that you want
to, as long as it supports C<TIEHASH>, C<FETCH>, C<STORE>, and
C<EXISTS>.  For example,

        tie my %cache => 'GDBM_File', $filename, O_RDWR|O_CREAT, 0666;
        memoize 'function', SCALAR_CACHE => [HASH => \%cache];

works just fine.  For some storage methods, you need a little glue.

C<SDBM_File> doesn't supply an C<EXISTS> method, so included in this
package is a glue module called C<Memoize::SDBM_File> which does
provide one.  Use this instead of plain C<SDBM_File> to store your
cache table on disk in an C<SDBM_File> database:

        tie my %cache => 'Memoize::SDBM_File', $filename, O_RDWR|O_CREAT, 0666;
        memoize 'function', SCALAR_CACHE => [HASH => \%cache];

C<NDBM_File> has the same problem and the same solution.  (Use
C<Memoize::NDBM_File instead of plain NDBM_File.>)

C<Storable> isn't a tied hash class at all.  You can use it to store a
hash to disk and retrieve it again, but you can't modify the hash while
it's on the disk.  So if you want to store your cache table in a
C<Storable> database, use C<Memoize::Storable>, which puts a hashlike
front-end onto C<Storable>.  The hash table is actually kept in
memory, and is loaded from your C<Storable> file at the time you
memoize the function, and stored back at the time you unmemoize the
function (or when your program exits):

        tie my %cache => 'Memoize::Storable', $filename;
	memoize 'function', SCALAR_CACHE => [HASH => \%cache];

        tie my %cache => 'Memoize::Storable', $filename, 'nstore';
	memoize 'function', SCALAR_CACHE => [HASH => \%cache];

Include the `nstore' option to have the C<Storable> database written
in `network order'.  (See L<Storable> for more details about this.)

The C<flush_cache()> function will raise a run-time error unless the
tied package provides a C<CLEAR> method.

=head1 EXPIRATION SUPPORT

See Memoize::Expire, which is a plug-in module that adds expiration
functionality to Memoize.  If you don't like the kinds of policies
that Memoize::Expire implements, it is easy to write your own plug-in
module to implement whatever policy you desire.  Memoize comes with
several examples.  An expiration manager that implements a LRU policy
is available on CPAN as Memoize::ExpireLRU.

=head1 BUGS

The test suite is much better, but always needs improvement.

There is some problem with the way C<goto &f> works under threaded
Perl, perhaps because of the lexical scoping of C<@_>.  This is a bug
in Perl, and until it is resolved, memoized functions will see a
slightly different C<caller()> and will perform a little more slowly
on threaded perls than unthreaded perls.

Some versions of C<DB_File> won't let you store data under a key of
length 0.  That means that if you have a function C<f> which you
memoized and the cache is in a C<DB_File> database, then the value of
C<f()> (C<f> called with no arguments) will not be memoized.  If this
is a big problem, you can supply a normalizer function that prepends
C<"x"> to every key.

=head1 MAILING LIST

To join a very low-traffic mailing list for announcements about
C<Memoize>, send an empty note to C<mjd-perl-memoize-request@plover.com>.

=head1 AUTHOR

Mark-Jason Dominus (C<mjd-perl-memoize+@plover.com>), Plover Systems co.

See the C<Memoize.pm> Page at http://www.plover.com/~mjd/perl/Memoize/
for news and upgrades.  Near this page, at
http://www.plover.com/~mjd/perl/MiniMemoize/ there is an article about
memoization and about the internals of Memoize that appeared in The
Perl Journal, issue #13.  (This article is also included in the
Memoize distribution as `article.html'.)

The author's book I<Higher Order Perl> (2005, ISBN 1558607013, published
by Morgan Kaufmann) discusses memoization (and many other fascinating
topics) in tremendous detail. It will also be available on-line for free.
For more information, visit http://perl.plover.com/book/ .

To join a mailing list for announcements about C<Memoize>, send an
empty message to C<mjd-perl-memoize-request@plover.com>.  This mailing
list is for announcements only and has extremely low traffic---about
two messages per year.

=head1 COPYRIGHT AND LICENSE

Copyright 1998, 1999, 2000, 2001  by Mark Jason Dominus

This library is free software; you may redistribute it and/or modify
it under the same terms as Perl itself.

=head1 THANK YOU

Many thanks to Jonathan Roy for bug reports and suggestions, to
Michael Schwern for other bug reports and patches, to Mike Cariaso for
helping me to figure out the Right Thing to Do About Expiration, to
Joshua Gerth, Joshua Chamas, Jonathan Roy (again), Mark D. Anderson,
and Andrew Johnson for more suggestions about expiration, to Brent
Powers for the Memoize::ExpireLRU module, to Ariel Scolnicov for
delightful messages about the Fibonacci function, to Dion Almaer for
thought-provoking suggestions about the default normalizer, to Walt
Mankowski and Kurt Starsinic for much help investigating problems
under threaded Perl, to Alex Dudkevich for reporting the bug in
prototyped functions and for checking my patch, to Tony Bass for many
helpful suggestions, to Jonathan Roy (again) for finding a use for
C<unmemoize()>, to Philippe Verdret for enlightening discussion of
C<Hook::PrePostCall>, to Nat Torkington for advice I ignored, to Chris
Nandor for portability advice, to Randal Schwartz for suggesting the
'C<flush_cache> function, and to Jenda Krynicky for being a light in
the world.

Special thanks to Jarkko Hietaniemi, the 5.8.0 pumpking, for including
this module in the core and for his patient and helpful guidance
during the integration process.

=cut
