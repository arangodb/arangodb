package SelfLoader;
use 5.008;
use strict;
our $VERSION = "1.15";

# The following bit of eval-magic is necessary to make this work on
# perls < 5.009005.
use vars qw/$AttrList/;
BEGIN {
  if ($] > 5.009004) {
    eval <<'NEWERPERL';
use 5.009005; # due to new regexp features
# allow checking for valid ': attrlist' attachments
# see also AutoSplit
$AttrList = qr{
    \s* : \s*
    (?:
	# one attribute
	(?> # no backtrack
	    (?! \d) \w+
	    (?<nested> \( (?: [^()]++ | (?&nested)++ )*+ \) ) ?
	)
	(?: \s* : \s* | \s+ (?! :) )
    )*
}x;

NEWERPERL
  }
  else {
    eval <<'OLDERPERL';
# allow checking for valid ': attrlist' attachments
# (we use 'our' rather than 'my' here, due to the rather complex and buggy
# behaviour of lexicals with qr// and (??{$lex}) )
our $nested;
$nested = qr{ \( (?: (?> [^()]+ ) | (??{ $nested }) )* \) }x;
our $one_attr = qr{ (?> (?! \d) \w+ (?:$nested)? ) (?:\s*\:\s*|\s+(?!\:)) }x;
$AttrList = qr{ \s* : \s* (?: $one_attr )* }x;
OLDERPERL
  }
}
use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(AUTOLOAD);
sub Version {$VERSION}
sub DEBUG () { 0 }

my %Cache;      # private cache for all SelfLoader's client packages

# in croak and carp, protect $@ from "require Carp;" RT #40216

sub croak { { local $@; require Carp; } goto &Carp::croak }
sub carp { { local $@; require Carp; } goto &Carp::carp }

AUTOLOAD {
    our $AUTOLOAD;
    print STDERR "SelfLoader::AUTOLOAD for $AUTOLOAD\n" if DEBUG;
    my $SL_code = $Cache{$AUTOLOAD};
    my $save = $@; # evals in both AUTOLOAD and _load_stubs can corrupt $@
    unless ($SL_code) {
        # Maybe this pack had stubs before __DATA__, and never initialized.
        # Or, this maybe an automatic DESTROY method call when none exists.
        $AUTOLOAD =~ m/^(.*)::/;
        SelfLoader->_load_stubs($1) unless exists $Cache{"${1}::<DATA"};
        $SL_code = $Cache{$AUTOLOAD};
        $SL_code = "sub $AUTOLOAD { }"
            if (!$SL_code and $AUTOLOAD =~ m/::DESTROY$/);
        croak "Undefined subroutine $AUTOLOAD" unless $SL_code;
    }
    print STDERR "SelfLoader::AUTOLOAD eval: $SL_code\n" if DEBUG;

    {
	no strict;
	eval $SL_code;
    }
    if ($@) {
        $@ =~ s/ at .*\n//;
        croak $@;
    }
    $@ = $save;
    defined(&$AUTOLOAD) || die "SelfLoader inconsistency error";
    delete $Cache{$AUTOLOAD};
    goto &$AUTOLOAD
}

sub load_stubs { shift->_load_stubs((caller)[0]) }

sub _load_stubs {
    # $endlines is used by Devel::SelfStubber to capture lines after __END__
    my($self, $callpack, $endlines) = @_;
    no strict "refs";
    my $fh = \*{"${callpack}::DATA"};
    use strict;
    my $currpack = $callpack;
    my($line,$name,@lines, @stubs, $protoype);

    print STDERR "SelfLoader::load_stubs($callpack)\n" if DEBUG;
    croak("$callpack doesn't contain an __DATA__ token")
        unless defined fileno($fh);
    # Protect: fork() shares the file pointer between the parent and the kid
    if(sysseek($fh, tell($fh), 0)) {
      open my $nfh, '<&', $fh or croak "reopen: $!";# dup() the fd
      close $fh or die "close: $1";                 # autocloses, but be paranoid
      open $fh, '<&', $nfh or croak "reopen2: $!";  # dup() the fd "back"
      close $nfh or die "close after reopen: $1";   # autocloses, but be paranoid
    }
    $Cache{"${currpack}::<DATA"} = 1;   # indicate package is cached

    local($/) = "\n";
    while(defined($line = <$fh>) and $line !~ m/^__END__/) {
	if ($line =~ m/^\s*sub\s+([\w:]+)\s*((?:\([\\\$\@\%\&\*\;]*\))?(?:$AttrList)?)/) {
            push(@stubs, $self->_add_to_cache($name, $currpack, \@lines, $protoype));
            $protoype = $2;
            @lines = ($line);
            if (index($1,'::') == -1) {         # simple sub name
                $name = "${currpack}::$1";
            } else {                            # sub name with package
                $name = $1;
                $name =~ m/^(.*)::/;
                if (defined(&{"${1}::AUTOLOAD"})) {
                    \&{"${1}::AUTOLOAD"} == \&SelfLoader::AUTOLOAD ||
                        die 'SelfLoader Error: attempt to specify Selfloading',
                            " sub $name in non-selfloading module $1";
                } else {
                    $self->export($1,'AUTOLOAD');
                }
            }
        } elsif ($line =~ m/^package\s+([\w:]+)/) { # A package declared
            push(@stubs, $self->_add_to_cache($name, $currpack, \@lines, $protoype));
            $self->_package_defined($line);
            $name = '';
            @lines = ();
            $currpack = $1;
            $Cache{"${currpack}::<DATA"} = 1;   # indicate package is cached
            if (defined(&{"${1}::AUTOLOAD"})) {
                \&{"${1}::AUTOLOAD"} == \&SelfLoader::AUTOLOAD ||
                    die 'SelfLoader Error: attempt to specify Selfloading',
                        " package $currpack which already has AUTOLOAD";
            } else {
                $self->export($currpack,'AUTOLOAD');
            }
        } else {
            push(@lines,$line);
        }
    }
    if (defined($line) && $line =~ /^__END__/) { # __END__
        unless ($line =~ /^__END__\s*DATA/) {
            if ($endlines) {
                # Devel::SelfStubber would like us to capture the lines after
                # __END__ so it can write out the entire file
                @$endlines = <$fh>;
            }
            close($fh);
        }
    }
    push(@stubs, $self->_add_to_cache($name, $currpack, \@lines, $protoype));
    no strict;
    eval join('', @stubs) if @stubs;
}


sub _add_to_cache {
    my($self,$fullname,$pack,$lines, $protoype) = @_;
    return () unless $fullname;
    carp("Redefining sub $fullname")
      if exists $Cache{$fullname};
    $Cache{$fullname} = join('', "package $pack; ",@$lines);
    print STDERR "SelfLoader cached $fullname: $Cache{$fullname}" if DEBUG;
    # return stub to be eval'd
    defined($protoype) ? "sub $fullname $protoype;" : "sub $fullname;"
}

sub _package_defined {}

1;
__END__

=head1 NAME

SelfLoader - load functions only on demand

=head1 SYNOPSIS

    package FOOBAR;
    use SelfLoader;

    ... (initializing code)

    __DATA__
    sub {....


=head1 DESCRIPTION

This module tells its users that functions in the FOOBAR package are to be
autoloaded from after the C<__DATA__> token.  See also
L<perlsub/"Autoloading">.

=head2 The __DATA__ token

The C<__DATA__> token tells the perl compiler that the perl code
for compilation is finished. Everything after the C<__DATA__> token
is available for reading via the filehandle FOOBAR::DATA,
where FOOBAR is the name of the current package when the C<__DATA__>
token is reached. This works just the same as C<__END__> does in
package 'main', but for other modules data after C<__END__> is not
automatically retrievable, whereas data after C<__DATA__> is.
The C<__DATA__> token is not recognized in versions of perl prior to
5.001m.

Note that it is possible to have C<__DATA__> tokens in the same package
in multiple files, and that the last C<__DATA__> token in a given
package that is encountered by the compiler is the one accessible
by the filehandle. This also applies to C<__END__> and main, i.e. if
the 'main' program has an C<__END__>, but a module 'require'd (_not_ 'use'd)
by that program has a 'package main;' declaration followed by an 'C<__DATA__>',
then the C<DATA> filehandle is set to access the data after the C<__DATA__>
in the module, _not_ the data after the C<__END__> token in the 'main'
program, since the compiler encounters the 'require'd file later.

=head2 SelfLoader autoloading

The B<SelfLoader> works by the user placing the C<__DATA__>
token I<after> perl code which needs to be compiled and
run at 'require' time, but I<before> subroutine declarations
that can be loaded in later - usually because they may never
be called.

The B<SelfLoader> will read from the FOOBAR::DATA filehandle to
load in the data after C<__DATA__>, and load in any subroutine
when it is called. The costs are the one-time parsing of the
data after C<__DATA__>, and a load delay for the _first_
call of any autoloaded function. The benefits (hopefully)
are a speeded up compilation phase, with no need to load
functions which are never used.

The B<SelfLoader> will stop reading from C<__DATA__> if
it encounters the C<__END__> token - just as you would expect.
If the C<__END__> token is present, and is followed by the
token DATA, then the B<SelfLoader> leaves the FOOBAR::DATA
filehandle open on the line after that token.

The B<SelfLoader> exports the C<AUTOLOAD> subroutine to the
package using the B<SelfLoader>, and this loads the called
subroutine when it is first called.

There is no advantage to putting subroutines which will _always_
be called after the C<__DATA__> token.

=head2 Autoloading and package lexicals

A 'my $pack_lexical' statement makes the variable $pack_lexical
local _only_ to the file up to the C<__DATA__> token. Subroutines
declared elsewhere _cannot_ see these types of variables,
just as if you declared subroutines in the package but in another
file, they cannot see these variables.

So specifically, autoloaded functions cannot see package
lexicals (this applies to both the B<SelfLoader> and the Autoloader).
The C<vars> pragma provides an alternative to defining package-level
globals that will be visible to autoloaded routines. See the documentation
on B<vars> in the pragma section of L<perlmod>.

=head2 SelfLoader and AutoLoader

The B<SelfLoader> can replace the AutoLoader - just change 'use AutoLoader'
to 'use SelfLoader' (though note that the B<SelfLoader> exports
the AUTOLOAD function - but if you have your own AUTOLOAD and
are using the AutoLoader too, you probably know what you're doing),
and the C<__END__> token to C<__DATA__>. You will need perl version 5.001m
or later to use this (version 5.001 with all patches up to patch m).

There is no need to inherit from the B<SelfLoader>.

The B<SelfLoader> works similarly to the AutoLoader, but picks up the
subs from after the C<__DATA__> instead of in the 'lib/auto' directory.
There is a maintenance gain in not needing to run AutoSplit on the module
at installation, and a runtime gain in not needing to keep opening and
closing files to load subs. There is a runtime loss in needing
to parse the code after the C<__DATA__>. Details of the B<AutoLoader> and
another view of these distinctions can be found in that module's
documentation.

=head2 __DATA__, __END__, and the FOOBAR::DATA filehandle.

This section is only relevant if you want to use
the C<FOOBAR::DATA> together with the B<SelfLoader>.

Data after the C<__DATA__> token in a module is read using the
FOOBAR::DATA filehandle. C<__END__> can still be used to denote the end
of the C<__DATA__> section if followed by the token DATA - this is supported
by the B<SelfLoader>. The C<FOOBAR::DATA> filehandle is left open if an
C<__END__> followed by a DATA is found, with the filehandle positioned at
the start of the line after the C<__END__> token. If no C<__END__> token is
present, or an C<__END__> token with no DATA token on the same line, then
the filehandle is closed.

The B<SelfLoader> reads from wherever the current
position of the C<FOOBAR::DATA> filehandle is, until the
EOF or C<__END__>. This means that if you want to use
that filehandle (and ONLY if you want to), you should either

1. Put all your subroutine declarations immediately after
the C<__DATA__> token and put your own data after those
declarations, using the C<__END__> token to mark the end
of subroutine declarations. You must also ensure that the B<SelfLoader>
reads first by  calling 'SelfLoader-E<gt>load_stubs();', or by using a
function which is selfloaded;

or

2. You should read the C<FOOBAR::DATA> filehandle first, leaving
the handle open and positioned at the first line of subroutine
declarations.

You could conceivably do both.

=head2 Classes and inherited methods.

For modules which are not classes, this section is not relevant.
This section is only relevant if you have methods which could
be inherited.

A subroutine stub (or forward declaration) looks like

  sub stub;

i.e. it is a subroutine declaration without the body of the
subroutine. For modules which are not classes, there is no real
need for stubs as far as autoloading is concerned.

For modules which ARE classes, and need to handle inherited methods,
stubs are needed to ensure that the method inheritance mechanism works
properly. You can load the stubs into the module at 'require' time, by
adding the statement 'SelfLoader-E<gt>load_stubs();' to the module to do
this.

The alternative is to put the stubs in before the C<__DATA__> token BEFORE
releasing the module, and for this purpose the C<Devel::SelfStubber>
module is available.  However this does require the extra step of ensuring
that the stubs are in the module. If this is done I strongly recommend
that this is done BEFORE releasing the module - it should NOT be done
at install time in general.

=head1 Multiple packages and fully qualified subroutine names

Subroutines in multiple packages within the same file are supported - but you
should note that this requires exporting the C<SelfLoader::AUTOLOAD> to
every package which requires it. This is done automatically by the
B<SelfLoader> when it first loads the subs into the cache, but you should
really specify it in the initialization before the C<__DATA__> by putting
a 'use SelfLoader' statement in each package.

Fully qualified subroutine names are also supported. For example,

   __DATA__
   sub foo::bar {23}
   package baz;
   sub dob {32}

will all be loaded correctly by the B<SelfLoader>, and the B<SelfLoader>
will ensure that the packages 'foo' and 'baz' correctly have the
B<SelfLoader> C<AUTOLOAD> method when the data after C<__DATA__> is first
parsed.

=head1 AUTHOR

C<SelfLoader> is maintained by the perl5-porters. Please direct
any questions to the canonical mailing list. Anything that
is applicable to the CPAN release can be sent to its maintainer,
though.

Author and Maintainer: The Perl5-Porters <perl5-porters@perl.org>

Maintainer of the CPAN release: Steffen Mueller <smueller@cpan.org>

=head1 COPYRIGHT AND LICENSE

This package has been part of the perl core since the first release
of perl5. It has been released separately to CPAN so older installations
can benefit from bug fixes.

This package has the same copyright and license as the perl core:

             Copyright (C) 1993, 1994, 1995, 1996, 1997, 1998, 1999,
        2000, 2001, 2002, 2003, 2004, 2005, 2006 by Larry Wall and others
    
			    All rights reserved.
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of either:
    
	a) the GNU General Public License as published by the Free
	Software Foundation; either version 1, or (at your option) any
	later version, or
    
	b) the "Artistic License" which comes with this Kit.
    
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See either
    the GNU General Public License or the Artistic License for more details.
    
    You should have received a copy of the Artistic License with this
    Kit, in the file named "Artistic".  If not, I'll be glad to provide one.
    
    You should also have received a copy of the GNU General Public License
    along with this program in the file named "Copying". If not, write to the 
    Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
    02111-1307, USA or visit their web page on the internet at
    http://www.gnu.org/copyleft/gpl.html.
    
    For those of you that choose to use the GNU General Public License,
    my interpretation of the GNU General Public License is that no Perl
    script falls under the terms of the GPL unless you explicitly put
    said script under the terms of the GPL yourself.  Furthermore, any
    object code linked with perl does not automatically fall under the
    terms of the GPL, provided such object code only adds definitions
    of subroutines and variables, and does not otherwise impair the
    resulting interpreter from executing any standard Perl script.  I
    consider linking in C subroutines in this manner to be the moral
    equivalent of defining subroutines in the Perl language itself.  You
    may sell such an object file as proprietary provided that you provide
    or offer to provide the Perl source, as specified by the GNU General
    Public License.  (This is merely an alternate way of specifying input
    to the program.)  You may also sell a binary produced by the dumping of
    a running Perl script that belongs to you, provided that you provide or
    offer to provide the Perl source as specified by the GPL.  (The
    fact that a Perl interpreter and your code are in the same binary file
    is, in this case, a form of mere aggregation.)  This is my interpretation
    of the GPL.  If you still have concerns or difficulties understanding
    my intent, feel free to contact me.  Of course, the Artistic License
    spells all this out for your protection, so you may prefer to use that.

=cut
