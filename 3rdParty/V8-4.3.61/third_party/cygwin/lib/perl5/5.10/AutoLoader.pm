package AutoLoader;

use strict;
use 5.006_001;

our($VERSION, $AUTOLOAD);

my $is_dosish;
my $is_epoc;
my $is_vms;
my $is_macos;

BEGIN {
    $is_dosish = $^O eq 'dos' || $^O eq 'os2' || $^O eq 'MSWin32' || $^O eq 'NetWare';
    $is_epoc = $^O eq 'epoc';
    $is_vms = $^O eq 'VMS';
    $is_macos = $^O eq 'MacOS';
    $VERSION = '5.66';
}

AUTOLOAD {
    my $sub = $AUTOLOAD;
    my $filename = AutoLoader::find_filename( $sub );

    my $save = $@;
    local $!; # Do not munge the value. 
    eval { local $SIG{__DIE__}; require $filename };
    if ($@) {
	if (substr($sub,-9) eq '::DESTROY') {
	    no strict 'refs';
	    *$sub = sub {};
	    $@ = undef;
	} elsif ($@ =~ /^Can't locate/) {
	    # The load might just have failed because the filename was too
	    # long for some old SVR3 systems which treat long names as errors.
	    # If we can successfully truncate a long name then it's worth a go.
	    # There is a slight risk that we could pick up the wrong file here
	    # but autosplit should have warned about that when splitting.
	    if ($filename =~ s/(\w{12,})\.al$/substr($1,0,11).".al"/e){
		eval { local $SIG{__DIE__}; require $filename };
	    }
	}
	if ($@){
	    $@ =~ s/ at .*\n//;
	    my $error = $@;
	    require Carp;
	    Carp::croak($error);
	}
    }
    $@ = $save;
    goto &$sub;
}

sub find_filename {
    my $sub = shift;
    my $filename;
    # Braces used to preserve $1 et al.
    {
	# Try to find the autoloaded file from the package-qualified
	# name of the sub. e.g., if the sub needed is
	# Getopt::Long::GetOptions(), then $INC{Getopt/Long.pm} is
	# something like '/usr/lib/perl5/Getopt/Long.pm', and the
	# autoload file is '/usr/lib/perl5/auto/Getopt/Long/GetOptions.al'.
	#
	# However, if @INC is a relative path, this might not work.  If,
	# for example, @INC = ('lib'), then $INC{Getopt/Long.pm} is
	# 'lib/Getopt/Long.pm', and we want to require
	# 'auto/Getopt/Long/GetOptions.al' (without the leading 'lib').
	# In this case, we simple prepend the 'auto/' and let the
	# C<require> take care of the searching for us.

	my ($pkg,$func) = ($sub =~ /(.*)::([^:]+)$/);
	$pkg =~ s#::#/#g;
	if (defined($filename = $INC{"$pkg.pm"})) {
	    if ($is_macos) {
		$pkg =~ tr#/#:#;
		$filename = undef
		  unless $filename =~ s#^(.*)$pkg\.pm\z#$1auto:$pkg:$func.al#s;
	    } else {
		$filename = undef
		  unless $filename =~ s#^(.*)$pkg\.pm\z#$1auto/$pkg/$func.al#s;
	    }

	    # if the file exists, then make sure that it is a
	    # a fully anchored path (i.e either '/usr/lib/auto/foo/bar.al',
	    # or './lib/auto/foo/bar.al'.  This avoids C<require> searching
	    # (and failing) to find the 'lib/auto/foo/bar.al' because it
	    # looked for 'lib/lib/auto/foo/bar.al', given @INC = ('lib').

	    if (defined $filename and -r $filename) {
		unless ($filename =~ m|^/|s) {
		    if ($is_dosish) {
			unless ($filename =~ m{^([a-z]:)?[\\/]}is) {
			    if ($^O ne 'NetWare') {
				$filename = "./$filename";
			    } else {
				$filename = "$filename";
			    }
			}
		    }
		    elsif ($is_epoc) {
			unless ($filename =~ m{^([a-z?]:)?[\\/]}is) {
			     $filename = "./$filename";
			}
		    }
		    elsif ($is_vms) {
			# XXX todo by VMSmiths
			$filename = "./$filename";
		    }
		    elsif (!$is_macos) {
			$filename = "./$filename";
		    }
		}
	    }
	    else {
		$filename = undef;
	    }
	}
	unless (defined $filename) {
	    # let C<require> do the searching
	    $filename = "auto/$sub.al";
	    $filename =~ s#::#/#g;
	}
    }
    return $filename;
}

sub import {
    my $pkg = shift;
    my $callpkg = caller;

    #
    # Export symbols, but not by accident of inheritance.
    #

    if ($pkg eq 'AutoLoader') {
	if ( @_ and $_[0] =~ /^&?AUTOLOAD$/ ) {
	    no strict 'refs';
	    *{ $callpkg . '::AUTOLOAD' } = \&AUTOLOAD;
	}
    }

    #
    # Try to find the autosplit index file.  Eg., if the call package
    # is POSIX, then $INC{POSIX.pm} is something like
    # '/usr/local/lib/perl5/POSIX.pm', and the autosplit index file is in
    # '/usr/local/lib/perl5/auto/POSIX/autosplit.ix', so we require that.
    #
    # However, if @INC is a relative path, this might not work.  If,
    # for example, @INC = ('lib'), then
    # $INC{POSIX.pm} is 'lib/POSIX.pm', and we want to require
    # 'auto/POSIX/autosplit.ix' (without the leading 'lib').
    #

    (my $calldir = $callpkg) =~ s#::#/#g;
    my $path = $INC{$calldir . '.pm'};
    if (defined($path)) {
	# Try absolute path name.
	if ($is_macos) {
	    (my $malldir = $calldir) =~ tr#/#:#;
	    $path =~ s#^(.*)$malldir\.pm\z#$1auto:$malldir:autosplit.ix#s;
	} else {
	    $path =~ s#^(.*)$calldir\.pm\z#$1auto/$calldir/autosplit.ix#;
	}

	eval { require $path; };
	# If that failed, try relative path with normal @INC searching.
	if ($@) {
	    $path ="auto/$calldir/autosplit.ix";
	    eval { require $path; };
	}
	if ($@) {
	    my $error = $@;
	    require Carp;
	    Carp::carp($error);
	}
    } 
}

sub unimport {
    my $callpkg = caller;

    no strict 'refs';

    for my $exported (qw( AUTOLOAD )) {
	my $symname = $callpkg . '::' . $exported;
	undef *{ $symname } if \&{ $symname } == \&{ $exported };
	*{ $symname } = \&{ $symname };
    }
}

1;

__END__

=head1 NAME

AutoLoader - load subroutines only on demand

=head1 SYNOPSIS

    package Foo;
    use AutoLoader 'AUTOLOAD';   # import the default AUTOLOAD subroutine

    package Bar;
    use AutoLoader;              # don't import AUTOLOAD, define our own
    sub AUTOLOAD {
        ...
        $AutoLoader::AUTOLOAD = "...";
        goto &AutoLoader::AUTOLOAD;
    }

=head1 DESCRIPTION

The B<AutoLoader> module works with the B<AutoSplit> module and the
C<__END__> token to defer the loading of some subroutines until they are
used rather than loading them all at once.

To use B<AutoLoader>, the author of a module has to place the
definitions of subroutines to be autoloaded after an C<__END__> token.
(See L<perldata>.)  The B<AutoSplit> module can then be run manually to
extract the definitions into individual files F<auto/funcname.al>.

B<AutoLoader> implements an AUTOLOAD subroutine.  When an undefined
subroutine in is called in a client module of B<AutoLoader>,
B<AutoLoader>'s AUTOLOAD subroutine attempts to locate the subroutine in a
file with a name related to the location of the file from which the
client module was read.  As an example, if F<POSIX.pm> is located in
F</usr/local/lib/perl5/POSIX.pm>, B<AutoLoader> will look for perl
subroutines B<POSIX> in F</usr/local/lib/perl5/auto/POSIX/*.al>, where
the C<.al> file has the same name as the subroutine, sans package.  If
such a file exists, AUTOLOAD will read and evaluate it,
thus (presumably) defining the needed subroutine.  AUTOLOAD will then
C<goto> the newly defined subroutine.

Once this process completes for a given function, it is defined, so
future calls to the subroutine will bypass the AUTOLOAD mechanism.

=head2 Subroutine Stubs

In order for object method lookup and/or prototype checking to operate
correctly even when methods have not yet been defined it is necessary to
"forward declare" each subroutine (as in C<sub NAME;>).  See
L<perlsub/"SYNOPSIS">.  Such forward declaration creates "subroutine
stubs", which are place holders with no code.

The AutoSplit and B<AutoLoader> modules automate the creation of forward
declarations.  The AutoSplit module creates an 'index' file containing
forward declarations of all the AutoSplit subroutines.  When the
AutoLoader module is 'use'd it loads these declarations into its callers
package.

Because of this mechanism it is important that B<AutoLoader> is always
C<use>d and not C<require>d.

=head2 Using B<AutoLoader>'s AUTOLOAD Subroutine

In order to use B<AutoLoader>'s AUTOLOAD subroutine you I<must>
explicitly import it:

    use AutoLoader 'AUTOLOAD';

=head2 Overriding B<AutoLoader>'s AUTOLOAD Subroutine

Some modules, mainly extensions, provide their own AUTOLOAD subroutines.
They typically need to check for some special cases (such as constants)
and then fallback to B<AutoLoader>'s AUTOLOAD for the rest.

Such modules should I<not> import B<AutoLoader>'s AUTOLOAD subroutine.
Instead, they should define their own AUTOLOAD subroutines along these
lines:

    use AutoLoader;
    use Carp;

    sub AUTOLOAD {
        my $sub = $AUTOLOAD;
        (my $constname = $sub) =~ s/.*:://;
        my $val = constant($constname, @_ ? $_[0] : 0);
        if ($! != 0) {
            if ($! =~ /Invalid/ || $!{EINVAL}) {
                $AutoLoader::AUTOLOAD = $sub;
                goto &AutoLoader::AUTOLOAD;
            }
            else {
                croak "Your vendor has not defined constant $constname";
            }
        }
        *$sub = sub { $val }; # same as: eval "sub $sub { $val }";
        goto &$sub;
    }

If any module's own AUTOLOAD subroutine has no need to fallback to the
AutoLoader's AUTOLOAD subroutine (because it doesn't have any AutoSplit
subroutines), then that module should not use B<AutoLoader> at all.

=head2 Package Lexicals

Package lexicals declared with C<my> in the main block of a package
using B<AutoLoader> will not be visible to auto-loaded subroutines, due to
the fact that the given scope ends at the C<__END__> marker.  A module
using such variables as package globals will not work properly under the
B<AutoLoader>.

The C<vars> pragma (see L<perlmod/"vars">) may be used in such
situations as an alternative to explicitly qualifying all globals with
the package namespace.  Variables pre-declared with this pragma will be
visible to any autoloaded routines (but will not be invisible outside
the package, unfortunately).

=head2 Not Using AutoLoader

You can stop using AutoLoader by simply

	no AutoLoader;

=head2 B<AutoLoader> vs. B<SelfLoader>

The B<AutoLoader> is similar in purpose to B<SelfLoader>: both delay the
loading of subroutines.

B<SelfLoader> uses the C<__DATA__> marker rather than C<__END__>.
While this avoids the use of a hierarchy of disk files and the
associated open/close for each routine loaded, B<SelfLoader> suffers a
startup speed disadvantage in the one-time parsing of the lines after
C<__DATA__>, after which routines are cached.  B<SelfLoader> can also
handle multiple packages in a file.

B<AutoLoader> only reads code as it is requested, and in many cases
should be faster, but requires a mechanism like B<AutoSplit> be used to
create the individual files.  L<ExtUtils::MakeMaker> will invoke
B<AutoSplit> automatically if B<AutoLoader> is used in a module source
file.

=head1 CAVEATS

AutoLoaders prior to Perl 5.002 had a slightly different interface.  Any
old modules which use B<AutoLoader> should be changed to the new calling
style.  Typically this just means changing a require to a use, adding
the explicit C<'AUTOLOAD'> import if needed, and removing B<AutoLoader>
from C<@ISA>.

On systems with restrictions on file name length, the file corresponding
to a subroutine may have a shorter name that the routine itself.  This
can lead to conflicting file names.  The I<AutoSplit> package warns of
these potential conflicts when used to split a module.

AutoLoader may fail to find the autosplit files (or even find the wrong
ones) in cases where C<@INC> contains relative paths, B<and> the program
does C<chdir>.

=head1 SEE ALSO

L<SelfLoader> - an autoloader that doesn't use external files.

=head1 AUTHOR

C<AutoLoader> is maintained by the perl5-porters. Please direct
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
        2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007, 2008
        by Larry Wall and others
    
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
