# Term::ANSIColor -- Color screen output using ANSI escape sequences.
# $Id: ANSIColor.pm 64 2007-03-23 17:58:18Z eagle $
#
# Copyright 1996, 1997, 1998, 2000, 2001, 2002, 2005, 2006
#   by Russ Allbery <rra@stanford.edu> and Zenin
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# Ah, September, when the sysadmins turn colors and fall off the trees....
#                               -- Dave Van Domelen

##############################################################################
# Modules and declarations
##############################################################################

package Term::ANSIColor;
require 5.001;

use strict;
use vars qw($AUTOLOAD $AUTORESET $EACHLINE @ISA @EXPORT @EXPORT_OK
            %EXPORT_TAGS $VERSION %attributes %attributes_r);

use Exporter ();
@ISA         = qw(Exporter);
@EXPORT      = qw(color colored);
@EXPORT_OK   = qw(uncolor);
%EXPORT_TAGS = (constants => [qw(CLEAR RESET BOLD DARK UNDERLINE UNDERSCORE
                                 BLINK REVERSE CONCEALED BLACK RED GREEN
                                 YELLOW BLUE MAGENTA CYAN WHITE ON_BLACK
                                 ON_RED ON_GREEN ON_YELLOW ON_BLUE ON_MAGENTA
                                 ON_CYAN ON_WHITE)]);
Exporter::export_ok_tags ('constants');

$VERSION = '1.12';

##############################################################################
# Internal data structures
##############################################################################

%attributes = ('clear'      => 0,
               'reset'      => 0,
               'bold'       => 1,
               'dark'       => 2,
               'underline'  => 4,
               'underscore' => 4,
               'blink'      => 5,
               'reverse'    => 7,
               'concealed'  => 8,

               'black'      => 30,   'on_black'   => 40,
               'red'        => 31,   'on_red'     => 41,
               'green'      => 32,   'on_green'   => 42,
               'yellow'     => 33,   'on_yellow'  => 43,
               'blue'       => 34,   'on_blue'    => 44,
               'magenta'    => 35,   'on_magenta' => 45,
               'cyan'       => 36,   'on_cyan'    => 46,
               'white'      => 37,   'on_white'   => 47);

# Reverse lookup.  Alphabetically first name for a sequence is preferred.
for (reverse sort keys %attributes) {
    $attributes_r{$attributes{$_}} = $_;
}

##############################################################################
# Implementation (constant form)
##############################################################################

# Time to have fun!  We now want to define the constant subs, which are named
# the same as the attributes above but in all caps.  Each constant sub needs
# to act differently depending on whether $AUTORESET is set.  Without
# autoreset:
#
#     BLUE "text\n"  ==>  "\e[34mtext\n"
#
# If $AUTORESET is set, we should instead get:
#
#     BLUE "text\n"  ==>  "\e[34mtext\n\e[0m"
#
# The sub also needs to handle the case where it has no arguments correctly.
# Maintaining all of this as separate subs would be a major nightmare, as well
# as duplicate the %attributes hash, so instead we define an AUTOLOAD sub to
# define the constant subs on demand.  To do that, we check the name of the
# called sub against the list of attributes, and if it's an all-caps version
# of one of them, we define the sub on the fly and then run it.
#
# If the environment variable ANSI_COLORS_DISABLED is set, turn all of the
# generated subs into pass-through functions that don't add any escape
# sequences.  This is to make it easier to write scripts that also work on
# systems without any ANSI support, like Windows consoles.
sub AUTOLOAD {
    my $enable_colors = !defined $ENV{ANSI_COLORS_DISABLED};
    my $sub;
    ($sub = $AUTOLOAD) =~ s/^.*:://;
    my $attr = $attributes{lc $sub};
    if ($sub =~ /^[A-Z_]+$/ && defined $attr) {
        $attr = $enable_colors ? "\e[" . $attr . 'm' : '';
        eval qq {
            sub $AUTOLOAD {
                if (\$AUTORESET && \@_) {
                    '$attr' . "\@_" . "\e[0m";
                } else {
                    ('$attr' . "\@_");
                }
            }
        };
        goto &$AUTOLOAD;
    } else {
        require Carp;
        Carp::croak ("undefined subroutine &$AUTOLOAD called");
    }
}

##############################################################################
# Implementation (attribute string form)
##############################################################################

# Return the escape code for a given set of color attributes.
sub color {
    return '' if defined $ENV{ANSI_COLORS_DISABLED};
    my @codes = map { split } @_;
    my $attribute = '';
    foreach (@codes) {
        $_ = lc $_;
        unless (defined $attributes{$_}) {
            require Carp;
            Carp::croak ("Invalid attribute name $_");
        }
        $attribute .= $attributes{$_} . ';';
    }
    chop $attribute;
    ($attribute ne '') ? "\e[${attribute}m" : undef;
}

# Return a list of named color attributes for a given set of escape codes.
# Escape sequences can be given with or without enclosing "\e[" and "m".  The
# empty escape sequence '' or "\e[m" gives an empty list of attrs.
sub uncolor {
    my (@nums, @result);
    for (@_) {
        my $escape = $_;
        $escape =~ s/^\e\[//;
        $escape =~ s/m$//;
        unless ($escape =~ /^((?:\d+;)*\d*)$/) {
            require Carp;
            Carp::croak ("Bad escape sequence $_");
        }
        push (@nums, split (/;/, $1));
    }
    for (@nums) {
	$_ += 0; # Strip leading zeroes
	my $name = $attributes_r{$_};
	if (!defined $name) {
	    require Carp;
	    Carp::croak ("No name for escape sequence $_" );
	}
	push (@result, $name);
    }
    @result;
}

# Given a string and a set of attributes, returns the string surrounded by
# escape codes to set those attributes and then clear them at the end of the
# string.  The attributes can be given either as an array ref as the first
# argument or as a list as the second and subsequent arguments.  If $EACHLINE
# is set, insert a reset before each occurrence of the string $EACHLINE and
# the starting attribute code after the string $EACHLINE, so that no attribute
# crosses line delimiters (this is often desirable if the output is to be
# piped to a pager or some other program).
sub colored {
    my ($string, @codes);
    if (ref $_[0]) {
        @codes = @{+shift};
        $string = join ('', @_);
    } else {
        $string = shift;
        @codes = @_;
    }
    return $string if defined $ENV{ANSI_COLORS_DISABLED};
    if (defined $EACHLINE) {
        my $attr = color (@codes);
        join '',
            map { $_ ne $EACHLINE ? $attr . $_ . "\e[0m" : $_ }
                grep { length ($_) > 0 }
                    split (/(\Q$EACHLINE\E)/, $string);
    } else {
        color (@codes) . $string . "\e[0m";
    }
}

##############################################################################
# Module return value and documentation
##############################################################################

# Ensure we evaluate to true.
1;
__END__

=head1 NAME

Term::ANSIColor - Color screen output using ANSI escape sequences

=head1 SYNOPSIS

    use Term::ANSIColor;
    print color 'bold blue';
    print "This text is bold blue.\n";
    print color 'reset';
    print "This text is normal.\n";
    print colored ("Yellow on magenta.", 'yellow on_magenta'), "\n";
    print "This text is normal.\n";
    print colored ['yellow on_magenta'], 'Yellow on magenta.';
    print "\n";

    use Term::ANSIColor qw(uncolor);
    print uncolor '01;31', "\n";

    use Term::ANSIColor qw(:constants);
    print BOLD, BLUE, "This text is in bold blue.\n", RESET;

    use Term::ANSIColor qw(:constants);
    $Term::ANSIColor::AUTORESET = 1;
    print BOLD BLUE "This text is in bold blue.\n";
    print "This text is normal.\n";

=head1 DESCRIPTION

This module has two interfaces, one through color() and colored() and the
other through constants.  It also offers the utility function uncolor(),
which has to be explicitly imported to be used (see L</SYNOPSIS>).

color() takes any number of strings as arguments and considers them to be
space-separated lists of attributes.  It then forms and returns the escape
sequence to set those attributes.  It doesn't print it out, just returns it,
so you'll have to print it yourself if you want to (this is so that you can
save it as a string, pass it to something else, send it to a file handle, or
do anything else with it that you might care to).

uncolor() performs the opposite translation, turning escape sequences
into a list of strings.

The recognized attributes (all of which should be fairly intuitive) are clear,
reset, dark, bold, underline, underscore, blink, reverse, concealed, black,
red, green, yellow, blue, magenta, cyan, white, on_black, on_red, on_green,
on_yellow, on_blue, on_magenta, on_cyan, and on_white.  Case is not
significant.  Underline and underscore are equivalent, as are clear and reset,
so use whichever is the most intuitive to you.  The color alone sets the
foreground color, and on_color sets the background color.

Note that not all attributes are supported by all terminal types, and some
terminals may not support any of these sequences.  Dark, blink, and
concealed in particular are frequently not implemented.

Attributes, once set, last until they are unset (by sending the attribute
"reset").  Be careful to do this, or otherwise your attribute will last
after your script is done running, and people get very annoyed at having
their prompt and typing changed to weird colors.

As an aid to help with this, colored() takes a scalar as the first argument
and any number of attribute strings as the second argument and returns the
scalar wrapped in escape codes so that the attributes will be set as
requested before the string and reset to normal after the string.
Alternately, you can pass a reference to an array as the first argument, and
then the contents of that array will be taken as attributes and color codes
and the remainder of the arguments as text to colorize.

Normally, colored() just puts attribute codes at the beginning and end of
the string, but if you set $Term::ANSIColor::EACHLINE to some string, that
string will be considered the line delimiter and the attribute will be set
at the beginning of each line of the passed string and reset at the end of
each line.  This is often desirable if the output contains newlines and
you're using background colors, since a background color that persists
across a newline is often interpreted by the terminal as providing the
default background color for the next line.  Programs like pagers can also
be confused by attributes that span lines.  Normally you'll want to set
$Term::ANSIColor::EACHLINE to C<"\n"> to use this feature.

Alternately, if you import C<:constants>, you can use the constants CLEAR,
RESET, BOLD, DARK, UNDERLINE, UNDERSCORE, BLINK, REVERSE, CONCEALED, BLACK,
RED, GREEN, YELLOW, BLUE, MAGENTA, CYAN, WHITE, ON_BLACK, ON_RED, ON_GREEN,
ON_YELLOW, ON_BLUE, ON_MAGENTA, ON_CYAN, and ON_WHITE directly.  These are
the same as color('attribute') and can be used if you prefer typing:

    print BOLD BLUE ON_WHITE "Text", RESET, "\n";

to

    print colored ("Text", 'bold blue on_white'), "\n";

(Note that the newline is kept separate to avoid confusing the terminal as
described above since a background color is being used.)

When using the constants, if you don't want to have to remember to add the
C<, RESET> at the end of each print line, you can set
$Term::ANSIColor::AUTORESET to a true value.  Then, the display mode will
automatically be reset if there is no comma after the constant.  In other
words, with that variable set:

    print BOLD BLUE "Text\n";

will reset the display mode afterwards, whereas:

    print BOLD, BLUE, "Text\n";

will not.  If you are using background colors, you will probably want to
print the newline with a separate print statement to avoid confusing the
terminal.

The subroutine interface has the advantage over the constants interface in
that only two subroutines are exported into your namespace, versus
twenty-two in the constants interface.  On the flip side, the constants
interface has the advantage of better compile time error checking, since
misspelled names of colors or attributes in calls to color() and colored()
won't be caught until runtime whereas misspelled names of constants will be
caught at compile time.  So, pollute your namespace with almost two dozen
subroutines that you may not even use that often, or risk a silly bug by
mistyping an attribute.  Your choice, TMTOWTDI after all.

=head1 DIAGNOSTICS

=over 4

=item Bad escape sequence %s

(F) You passed an invalid ANSI escape sequence to uncolor().

=item Bareword "%s" not allowed while "strict subs" in use

(F) You probably mistyped a constant color name such as:

    $Foobar = FOOBAR . "This line should be blue\n";

or:

    @Foobar = FOOBAR, "This line should be blue\n";

This will only show up under use strict (another good reason to run under
use strict).

=item Invalid attribute name %s

(F) You passed an invalid attribute name to either color() or colored().

=item Name "%s" used only once: possible typo

(W) You probably mistyped a constant color name such as:

    print FOOBAR "This text is color FOOBAR\n";

It's probably better to always use commas after constant names in order to
force the next error.

=item No comma allowed after filehandle

(F) You probably mistyped a constant color name such as:

    print FOOBAR, "This text is color FOOBAR\n";

Generating this fatal compile error is one of the main advantages of using
the constants interface, since you'll immediately know if you mistype a
color name.

=item No name for escape sequence %s

(F) The ANSI escape sequence passed to uncolor() contains escapes which
aren't recognized and can't be translated to names.

=back

=head1 ENVIRONMENT

=over 4

=item ANSI_COLORS_DISABLED

If this environment variable is set, all of the functions defined by this
module (color(), colored(), and all of the constants not previously used in
the program) will not output any escape sequences and instead will just
return the empty string or pass through the original text as appropriate.
This is intended to support easy use of scripts using this module on
platforms that don't support ANSI escape sequences.

For it to have its proper effect, this environment variable must be set
before any color constants are used in the program.

=back

=head1 RESTRICTIONS

It would be nice if one could leave off the commas around the constants
entirely and just say:

    print BOLD BLUE ON_WHITE "Text\n" RESET;

but the syntax of Perl doesn't allow this.  You need a comma after the
string.  (Of course, you may consider it a bug that commas between all the
constants aren't required, in which case you may feel free to insert commas
unless you're using $Term::ANSIColor::AUTORESET.)

For easier debugging, you may prefer to always use the commas when not
setting $Term::ANSIColor::AUTORESET so that you'll get a fatal compile error
rather than a warning.

=head1 NOTES

The codes generated by this module are standard terminal control codes,
complying with ECMA-48 and ISO 6429 (generally referred to as "ANSI color"
for the color codes).  The non-color control codes (bold, dark, italic,
underline, and reverse) are part of the earlier ANSI X3.64 standard for
control sequences for video terminals and peripherals.

Note that not all displays are ISO 6429-compliant, or even X3.64-compliant
(or are even attempting to be so).  This module will not work as expected on
displays that do not honor these escape sequences, such as cmd.exe, 4nt.exe,
and command.com under either Windows NT or Windows 2000.  They may just be
ignored, or they may display as an ESC character followed by some apparent
garbage.

Jean Delvare provided the following table of different common terminal
emulators and their support for the various attributes and others have helped
me flesh it out:

              clear    bold     dark    under    blink   reverse  conceal
 ------------------------------------------------------------------------
 xterm         yes      yes      no      yes     bold      yes      yes
 linux         yes      yes      yes    bold      yes      yes      no
 rxvt          yes      yes      no      yes  bold/black   yes      no
 dtterm        yes      yes      yes     yes    reverse    yes      yes
 teraterm      yes    reverse    no      yes    rev/red    yes      no
 aixterm      kinda   normal     no      yes      no       yes      yes
 PuTTY         yes     color     no      yes      no       yes      no
 Windows       yes      no       no      no       no       yes      no
 Cygwin SSH    yes      yes      no     color    color    color     yes
 Mac Terminal  yes      yes      no      yes      yes      yes      yes

Windows is Windows telnet, Cygwin SSH is the OpenSSH implementation under
Cygwin on Windows NT, and Mac Terminal is the Terminal application in Mac OS
X.  Where the entry is other than yes or no, that emulator displays the
given attribute as something else instead.  Note that on an aixterm, clear
doesn't reset colors; you have to explicitly set the colors back to what you
want.  More entries in this table are welcome.

Note that codes 3 (italic), 6 (rapid blink), and 9 (strikethrough) are
specified in ANSI X3.64 and ECMA-048 but are not commonly supported by most
displays and emulators and therefore aren't supported by this module at the
present time.  ECMA-048 also specifies a large number of other attributes,
including a sequence of attributes for font changes, Fraktur characters,
double-underlining, framing, circling, and overlining.  As none of these
attributes are widely supported or useful, they also aren't currently
supported by this module.

=head1 SEE ALSO

ECMA-048 is available on-line (at least at the time of this writing) at
L<http://www.ecma-international.org/publications/standards/ECMA-048.HTM>.

ISO 6429 is available from ISO for a charge; the author of this module does
not own a copy of it.  Since the source material for ISO 6429 was ECMA-048
and the latter is available for free, there seems little reason to obtain
the ISO standard.

The current version of this module is always available from its web site at
L<http://www.eyrie.org/~eagle/software/ansicolor/>.  It is also part of the
Perl core distribution as of 5.6.0.

=head1 AUTHORS

Original idea (using constants) by Zenin, reimplemented using subs by Russ
Allbery <rra@stanford.edu>, and then combined with the original idea by Russ
with input from Zenin.  Russ Allbery now maintains this module.

=head1 COPYRIGHT AND LICENSE

Copyright 1996, 1997, 1998, 2000, 2001, 2002, 2005, 2006 Russ Allbery
<rra@stanford.edu> and Zenin.  This program is free software; you may
redistribute it and/or modify it under the same terms as Perl itself.

=cut
