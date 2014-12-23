package Carp;

our $VERSION = '1.09';
# this file is an utra-lightweight stub. The first time a function is
# called, Carp::Heavy is loaded, and the real short/longmessmess_jmp
# subs are installed

our $MaxEvalLen = 0;
our $Verbose    = 0;
our $CarpLevel  = 0;
our $MaxArgLen  = 64;   # How much of each argument to print. 0 = all.
our $MaxArgNums = 8;    # How many arguments to print. 0 = all.

require Exporter;
our @ISA = ('Exporter');
our @EXPORT = qw(confess croak carp);
our @EXPORT_OK = qw(cluck verbose longmess shortmess);
our @EXPORT_FAIL = qw(verbose);	# hook to enable verbose mode

# if the caller specifies verbose usage ("perl -MCarp=verbose script.pl")
# then the following method will be called by the Exporter which knows
# to do this thanks to @EXPORT_FAIL, above.  $_[1] will contain the word
# 'verbose'.

sub export_fail { shift; $Verbose = shift if $_[0] eq 'verbose'; @_ }

# fixed hooks for stashes to point to
sub longmess  { goto &longmess_jmp }
sub shortmess { goto &shortmess_jmp }
# these two are replaced when Carp::Heavy is loaded
sub longmess_jmp  {
    local($@, $!);
    eval { require Carp::Heavy };
    return $@ if $@;
    goto &longmess_real;
}
sub shortmess_jmp  {
    local($@, $!);
    eval { require Carp::Heavy };
    return $@ if $@;
    goto &shortmess_real;
}

sub croak   { die  shortmess @_ }
sub confess { die  longmess  @_ }
sub carp    { warn shortmess @_ }
sub cluck   { warn longmess  @_ }

1;
__END__

=head1 NAME

carp    - warn of errors (from perspective of caller)

cluck   - warn of errors with stack backtrace
          (not exported by default)

croak   - die of errors (from perspective of caller)

confess - die of errors with stack backtrace

=head1 SYNOPSIS

    use Carp;
    croak "We're outta here!";

    use Carp qw(cluck);
    cluck "This is how we got here!";

=head1 DESCRIPTION

The Carp routines are useful in your own modules because
they act like die() or warn(), but with a message which is more
likely to be useful to a user of your module.  In the case of
cluck, confess, and longmess that context is a summary of every
call in the call-stack.  For a shorter message you can use C<carp>
or C<croak> which report the error as being from where your module
was called.  There is no guarantee that that is where the error
was, but it is a good educated guess.

You can also alter the way the output and logic of C<Carp> works, by
changing some global variables in the C<Carp> namespace. See the
section on C<GLOBAL VARIABLES> below.

Here is a more complete description of how c<carp> and c<croak> work.
What they do is search the call-stack for a function call stack where
they have not been told that there shouldn't be an error.  If every
call is marked safe, they give up and give a full stack backtrace
instead.  In other words they presume that the first likely looking
potential suspect is guilty.  Their rules for telling whether
a call shouldn't generate errors work as follows:

=over 4

=item 1.

Any call from a package to itself is safe.

=item 2.

Packages claim that there won't be errors on calls to or from
packages explicitly marked as safe by inclusion in C<@CARP_NOT>, or
(if that array is empty) C<@ISA>.  The ability to override what
@ISA says is new in 5.8.

=item 3.

The trust in item 2 is transitive.  If A trusts B, and B
trusts C, then A trusts C.  So if you do not override C<@ISA>
with C<@CARP_NOT>, then this trust relationship is identical to,
"inherits from".

=item 4.

Any call from an internal Perl module is safe.  (Nothing keeps
user modules from marking themselves as internal to Perl, but
this practice is discouraged.)

=item 5.

Any call to Perl's warning system (eg Carp itself) is safe.
(This rule is what keeps it from reporting the error at the
point where you call C<carp> or C<croak>.)

=item 6.

C<$Carp::CarpLevel> can be set to skip a fixed number of additional
call levels.  Using this is not recommended because it is very
difficult to get it to behave correctly.

=back

=head2 Forcing a Stack Trace

As a debugging aid, you can force Carp to treat a croak as a confess
and a carp as a cluck across I<all> modules. In other words, force a
detailed stack trace to be given.  This can be very helpful when trying
to understand why, or from where, a warning or error is being generated.

This feature is enabled by 'importing' the non-existent symbol
'verbose'. You would typically enable it by saying

    perl -MCarp=verbose script.pl

or by including the string C<-MCarp=verbose> in the PERL5OPT
environment variable.

Alternately, you can set the global variable C<$Carp::Verbose> to true.
See the C<GLOBAL VARIABLES> section below.

=head1 GLOBAL VARIABLES

=head2 $Carp::MaxEvalLen

This variable determines how many characters of a string-eval are to
be shown in the output. Use a value of C<0> to show all text.

Defaults to C<0>.

=head2 $Carp::MaxArgLen

This variable determines how many characters of each argument to a
function to print. Use a value of C<0> to show the full length of the
argument.

Defaults to C<64>.

=head2 $Carp::MaxArgNums

This variable determines how many arguments to each function to show.
Use a value of C<0> to show all arguments to a function call.

Defaults to C<8>.

=head2 $Carp::Verbose

This variable makes C<carp> and C<cluck> generate stack backtraces
just like C<cluck> and C<confess>.  This is how C<use Carp 'verbose'>
is implemented internally.

Defaults to C<0>.

=head2 %Carp::Internal

This says what packages are internal to Perl.  C<Carp> will never
report an error as being from a line in a package that is internal to
Perl.  For example:

    $Carp::Internal{ __PACKAGE__ }++;
    # time passes...
    sub foo { ... or confess("whatever") };

would give a full stack backtrace starting from the first caller
outside of __PACKAGE__.  (Unless that package was also internal to
Perl.)

=head2 %Carp::CarpInternal

This says which packages are internal to Perl's warning system.  For
generating a full stack backtrace this is the same as being internal
to Perl, the stack backtrace will not start inside packages that are
listed in C<%Carp::CarpInternal>.  But it is slightly different for
the summary message generated by C<carp> or C<croak>.  There errors
will not be reported on any lines that are calling packages in
C<%Carp::CarpInternal>.

For example C<Carp> itself is listed in C<%Carp::CarpInternal>.
Therefore the full stack backtrace from C<confess> will not start
inside of C<Carp>, and the short message from calling C<croak> is
not placed on the line where C<croak> was called.

=head2 $Carp::CarpLevel

This variable determines how many additional call frames are to be
skipped that would not otherwise be when reporting where an error
occurred on a call to one of C<Carp>'s functions.  It is fairly easy
to count these call frames on calls that generate a full stack
backtrace.  However it is much harder to do this accounting for calls
that generate a short message.  Usually people skip too many call
frames.  If they are lucky they skip enough that C<Carp> goes all of
the way through the call stack, realizes that something is wrong, and
then generates a full stack backtrace.  If they are unlucky then the
error is reported from somewhere misleading very high in the call
stack.

Therefore it is best to avoid C<$Carp::CarpLevel>.  Instead use
C<@CARP_NOT>, C<%Carp::Internal> and %Carp::CarpInternal>.

Defaults to C<0>.

=head1 BUGS

The Carp routines don't handle exception objects currently.
If called with a first argument that is a reference, they simply
call die() or warn(), as appropriate.

