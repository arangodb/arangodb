# Pod::Man -- Convert POD data to formatted *roff input.
# $Id: Man.pm,v 2.16 2007-11-29 01:35:53 eagle Exp $
#
# Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
#     Russ Allbery <rra@stanford.edu>
# Substantial contributions by Sean Burke <sburke@cpan.org>
#
# This program is free software; you may redistribute it and/or modify it
# under the same terms as Perl itself.
#
# This module translates POD documentation into *roff markup using the man
# macro set, and is intended for converting POD documents written as Unix
# manual pages to manual pages that can be read by the man(1) command.  It is
# a replacement for the pod2man command distributed with versions of Perl
# prior to 5.6.
#
# Perl core hackers, please note that this module is also separately
# maintained outside of the Perl core as part of the podlators.  Please send
# me any patches at the address above in addition to sending them to the
# standard Perl mailing lists.

##############################################################################
# Modules and declarations
##############################################################################

package Pod::Man;

require 5.005;

use strict;
use subs qw(makespace);
use vars qw(@ISA %ESCAPES $PREAMBLE $VERSION);

use Carp qw(croak);
use Pod::Simple ();
use POSIX qw(strftime);

@ISA = qw(Pod::Simple);

# Don't use the CVS revision as the version, since this module is also in Perl
# core and too many things could munge CVS magic revision strings.  This
# number should ideally be the same as the CVS revision in podlators, however.
$VERSION = '2.16';

# Set the debugging level.  If someone has inserted a debug function into this
# class already, use that.  Otherwise, use any Pod::Simple debug function
# that's defined, and failing that, define a debug level of 10.
BEGIN {
    my $parent = defined (&Pod::Simple::DEBUG) ? \&Pod::Simple::DEBUG : undef;
    unless (defined &DEBUG) {
        *DEBUG = $parent || sub () { 10 };
    }
}

# Import the ASCII constant from Pod::Simple.  This is true iff we're in an
# ASCII-based universe (including such things as ISO 8859-1 and UTF-8), and is
# generally only false for EBCDIC.
BEGIN { *ASCII = \&Pod::Simple::ASCII }

# Pretty-print a data structure.  Only used for debugging.
BEGIN { *pretty = \&Pod::Simple::pretty }

##############################################################################
# Object initialization
##############################################################################

# Initialize the object and set various Pod::Simple options that we need.
# Here, we also process any additional options passed to the constructor or
# set up defaults if none were given.  Note that all internal object keys are
# in all-caps, reserving all lower-case object keys for Pod::Simple and user
# arguments.
sub new {
    my $class = shift;
    my $self = $class->SUPER::new;

    # Tell Pod::Simple to handle S<> by automatically inserting &nbsp;.
    $self->nbsp_for_S (1);

    # Tell Pod::Simple to keep whitespace whenever possible.
    if ($self->can ('preserve_whitespace')) {
        $self->preserve_whitespace (1);
    } else {
        $self->fullstop_space_harden (1);
    }

    # The =for and =begin targets that we accept.
    $self->accept_targets (qw/man MAN roff ROFF/);

    # Ensure that contiguous blocks of code are merged together.  Otherwise,
    # some of the guesswork heuristics don't work right.
    $self->merge_text (1);

    # Pod::Simple doesn't do anything useful with our arguments, but we want
    # to put them in our object as hash keys and values.  This could cause
    # problems if we ever clash with Pod::Simple's own internal class
    # variables.
    %$self = (%$self, @_);

    # Initialize various other internal constants based on our arguments.
    $self->init_fonts;
    $self->init_quotes;
    $self->init_page;

    # For right now, default to turning on all of the magic.
    $$self{MAGIC_CPP}       = 1;
    $$self{MAGIC_EMDASH}    = 1;
    $$self{MAGIC_FUNC}      = 1;
    $$self{MAGIC_MANREF}    = 1;
    $$self{MAGIC_SMALLCAPS} = 1;
    $$self{MAGIC_VARS}      = 1;

    return $self;
}

# Translate a font string into an escape.
sub toescape { (length ($_[0]) > 1 ? '\f(' : '\f') . $_[0] }

# Determine which fonts the user wishes to use and store them in the object.
# Regular, italic, bold, and bold-italic are constants, but the fixed width
# fonts may be set by the user.  Sets the internal hash key FONTS which is
# used to map our internal font escapes to actual *roff sequences later.
sub init_fonts {
    my ($self) = @_;

    # Figure out the fixed-width font.  If user-supplied, make sure that they
    # are the right length.
    for (qw/fixed fixedbold fixeditalic fixedbolditalic/) {
        my $font = $$self{$_};
        if (defined ($font) && (length ($font) < 1 || length ($font) > 2)) {
            croak qq(roff font should be 1 or 2 chars, not "$font");
        }
    }

    # Set the default fonts.  We can't be sure portably across different
    # implementations what fixed bold-italic may be called (if it's even
    # available), so default to just bold.
    $$self{fixed}           ||= 'CW';
    $$self{fixedbold}       ||= 'CB';
    $$self{fixeditalic}     ||= 'CI';
    $$self{fixedbolditalic} ||= 'CB';

    # Set up a table of font escapes.  First number is fixed-width, second is
    # bold, third is italic.
    $$self{FONTS} = { '000' => '\fR', '001' => '\fI',
                      '010' => '\fB', '011' => '\f(BI',
                      '100' => toescape ($$self{fixed}),
                      '101' => toescape ($$self{fixeditalic}),
                      '110' => toescape ($$self{fixedbold}),
                      '111' => toescape ($$self{fixedbolditalic}) };
}

# Initialize the quotes that we'll be using for C<> text.  This requires some
# special handling, both to parse the user parameter if given and to make sure
# that the quotes will be safe against *roff.  Sets the internal hash keys
# LQUOTE and RQUOTE.
sub init_quotes {
    my ($self) = (@_);

    $$self{quotes} ||= '"';
    if ($$self{quotes} eq 'none') {
        $$self{LQUOTE} = $$self{RQUOTE} = '';
    } elsif (length ($$self{quotes}) == 1) {
        $$self{LQUOTE} = $$self{RQUOTE} = $$self{quotes};
    } elsif ($$self{quotes} =~ /^(.)(.)$/
             || $$self{quotes} =~ /^(..)(..)$/) {
        $$self{LQUOTE} = $1;
        $$self{RQUOTE} = $2;
    } else {
        croak(qq(Invalid quote specification "$$self{quotes}"))
    }

    # Double the first quote; note that this should not be s///g as two double
    # quotes is represented in *roff as three double quotes, not four.  Weird,
    # I know.
    $$self{LQUOTE} =~ s/\"/\"\"/;
    $$self{RQUOTE} =~ s/\"/\"\"/;
}

# Initialize the page title information and indentation from our arguments.
sub init_page {
    my ($self) = @_;

    # We used to try first to get the version number from a local binary, but
    # we shouldn't need that any more.  Get the version from the running Perl.
    # Work a little magic to handle subversions correctly under both the
    # pre-5.6 and the post-5.6 version numbering schemes.
    my @version = ($] =~ /^(\d+)\.(\d{3})(\d{0,3})$/);
    $version[2] ||= 0;
    $version[2] *= 10 ** (3 - length $version[2]);
    for (@version) { $_ += 0 }
    my $version = join ('.', @version);

    # Set the defaults for page titles and indentation if the user didn't
    # override anything.
    $$self{center} = 'User Contributed Perl Documentation'
        unless defined $$self{center};
    $$self{release} = 'perl v' . $version
        unless defined $$self{release};
    $$self{indent} = 4
        unless defined $$self{indent};

    # Double quotes in things that will be quoted.
    for (qw/center release/) {
        $$self{$_} =~ s/\"/\"\"/g if $$self{$_};
    }
}

##############################################################################
# Core parsing
##############################################################################

# This is the glue that connects the code below with Pod::Simple itself.  The
# goal is to convert the event stream coming from the POD parser into method
# calls to handlers once the complete content of a tag has been seen.  Each
# paragraph or POD command will have textual content associated with it, and
# as soon as all of a paragraph or POD command has been seen, that content
# will be passed in to the corresponding method for handling that type of
# object.  The exceptions are handlers for lists, which have opening tag
# handlers and closing tag handlers that will be called right away.
#
# The internal hash key PENDING is used to store the contents of a tag until
# all of it has been seen.  It holds a stack of open tags, each one
# represented by a tuple of the attributes hash for the tag, formatting
# options for the tag (which are inherited), and the contents of the tag.

# Add a block of text to the contents of the current node, formatting it
# according to the current formatting instructions as we do.
sub _handle_text {
    my ($self, $text) = @_;
    DEBUG > 3 and print "== $text\n";
    my $tag = $$self{PENDING}[-1];
    $$tag[2] .= $self->format_text ($$tag[1], $text);
}

# Given an element name, get the corresponding method name.
sub method_for_element {
    my ($self, $element) = @_;
    $element =~ tr/-/_/;
    $element =~ tr/A-Z/a-z/;
    $element =~ tr/_a-z0-9//cd;
    return $element;
}

# Handle the start of a new element.  If cmd_element is defined, assume that
# we need to collect the entire tree for this element before passing it to the
# element method, and create a new tree into which we'll collect blocks of
# text and nested elements.  Otherwise, if start_element is defined, call it.
sub _handle_element_start {
    my ($self, $element, $attrs) = @_;
    DEBUG > 3 and print "++ $element (<", join ('> <', %$attrs), ">)\n";
    my $method = $self->method_for_element ($element);

    # If we have a command handler, we need to accumulate the contents of the
    # tag before calling it.  Turn off IN_NAME for any command other than
    # <Para> so that IN_NAME isn't still set for the first heading after the
    # NAME heading.
    if ($self->can ("cmd_$method")) {
        DEBUG > 2 and print "<$element> starts saving a tag\n";
        $$self{IN_NAME} = 0 if ($element ne 'Para');

        # How we're going to format embedded text blocks depends on the tag
        # and also depends on our parent tags.  Thankfully, inside tags that
        # turn off guesswork and reformatting, nothing else can turn it back
        # on, so this can be strictly inherited.
        my $formatting = $$self{PENDING}[-1][1];
        $formatting = $self->formatting ($formatting, $element);
        push (@{ $$self{PENDING} }, [ $attrs, $formatting, '' ]);
        DEBUG > 4 and print "Pending: [", pretty ($$self{PENDING}), "]\n";
    } elsif ($self->can ("start_$method")) {
        my $method = 'start_' . $method;
        $self->$method ($attrs, '');
    } else {
        DEBUG > 2 and print "No $method start method, skipping\n";
    }
}

# Handle the end of an element.  If we had a cmd_ method for this element,
# this is where we pass along the tree that we built.  Otherwise, if we have
# an end_ method for the element, call that.
sub _handle_element_end {
    my ($self, $element) = @_;
    DEBUG > 3 and print "-- $element\n";
    my $method = $self->method_for_element ($element);

    # If we have a command handler, pull off the pending text and pass it to
    # the handler along with the saved attribute hash.
    if ($self->can ("cmd_$method")) {
        DEBUG > 2 and print "</$element> stops saving a tag\n";
        my $tag = pop @{ $$self{PENDING} };
        DEBUG > 4 and print "Popped: [", pretty ($tag), "]\n";
        DEBUG > 4 and print "Pending: [", pretty ($$self{PENDING}), "]\n";
        my $method = 'cmd_' . $method;
        my $text = $self->$method ($$tag[0], $$tag[2]);
        if (defined $text) {
            if (@{ $$self{PENDING} } > 1) {
                $$self{PENDING}[-1][2] .= $text;
            } else {
                $self->output ($text);
            }
        }
    } elsif ($self->can ("end_$method")) {
        my $method = 'end_' . $method;
        $self->$method ();
    } else {
        DEBUG > 2 and print "No $method end method, skipping\n";
    }
}

##############################################################################
# General formatting
##############################################################################

# Return formatting instructions for a new block.  Takes the current
# formatting and the new element.  Formatting inherits negatively, in the
# sense that if the parent has turned off guesswork, all child elements should
# leave it off.  We therefore return a copy of the same formatting
# instructions but possibly with more things turned off depending on the
# element.
sub formatting {
    my ($self, $current, $element) = @_;
    my %options;
    if ($current) {
        %options = %$current;
    } else {
        %options = (guesswork => 1, cleanup => 1, convert => 1);
    }
    if ($element eq 'Data') {
        $options{guesswork} = 0;
        $options{cleanup} = 0;
        $options{convert} = 0;
    } elsif ($element eq 'X') {
        $options{guesswork} = 0;
        $options{cleanup} = 0;
    } elsif ($element eq 'Verbatim' || $element eq 'C') {
        $options{guesswork} = 0;
        $options{literal} = 1;
    }
    return \%options;
}

# Format a text block.  Takes a hash of formatting options and the text to
# format.  Currently, the only formatting options are guesswork, cleanup, and
# convert, all of which are boolean.
sub format_text {
    my ($self, $options, $text) = @_;
    my $guesswork = $$options{guesswork} && !$$self{IN_NAME};
    my $cleanup = $$options{cleanup};
    my $convert = $$options{convert};
    my $literal = $$options{literal};

    # Normally we do character translation, but we won't even do that in
    # <Data> blocks.
    if ($convert) {
        if (ASCII) {
            $text =~ s/(\\|[^\x00-\x7F])/$ESCAPES{ord ($1)} || "X"/eg;
        } else {
            $text =~ s/(\\)/$ESCAPES{ord ($1)} || "X"/eg;
        }
    }

    # Cleanup just tidies up a few things, telling *roff that the hyphens are
    # hard and putting a bit of space between consecutive underscores.
    if ($cleanup) {
        $text =~ s/-/\\-/g;
        $text =~ s/_(?=_)/_\\|/g;
    }

    # Ensure that *roff doesn't convert literal quotes to UTF-8 single quotes,
    # but don't mess up our accept escapes.
    if ($literal) {
        $text =~ s/(?<!\\\*)\'/\\*\(Aq/g;
        $text =~ s/(?<!\\\*)\`/\\\`/g;
    }

    # If guesswork is asked for, do that.  This involves more substantial
    # formatting based on various heuristics that may only be appropriate for
    # particular documents.
    if ($guesswork) {
        $text = $self->guesswork ($text);
    }

    return $text;
}

# Handles C<> text, deciding whether to put \*C` around it or not.  This is a
# whole bunch of messy heuristics to try to avoid overquoting, originally from
# Barrie Slaymaker.  This largely duplicates similar code in Pod::Text.
sub quote_literal {
    my $self = shift;
    local $_ = shift;

    # A regex that matches the portion of a variable reference that's the
    # array or hash index, separated out just because we want to use it in
    # several places in the following regex.
    my $index = '(?: \[.*\] | \{.*\} )?';

    # Check for things that we don't want to quote, and if we find any of
    # them, return the string with just a font change and no quoting.
    m{
      ^\s*
      (?:
         ( [\'\`\"] ) .* \1                             # already quoted
       | \\\*\(Aq .* \\\*\(Aq                           # quoted and escaped
       | \\?\` .* ( \' | \\\*\(Aq )                     # `quoted'
       | \$+ [\#^]? \S $index                           # special ($^Foo, $")
       | [\$\@%&*]+ \#? [:\'\w]+ $index                 # plain var or func
       | [\$\@%&*]* [:\'\w]+ (?: -> )? \(\s*[^\s,]\s*\) # 0/1-arg func call
       | [-+]? ( \d[\d.]* | \.\d+ ) (?: [eE][-+]?\d+ )? # a number
       | 0x [a-fA-F\d]+                                 # a hex constant
      )
      \s*\z
     }xso and return '\f(FS' . $_ . '\f(FE';

    # If we didn't return, go ahead and quote the text.
    return '\f(FS\*(C`' . $_ . "\\*(C'\\f(FE";
}

# Takes a text block to perform guesswork on.  Returns the text block with
# formatting codes added.  This is the code that marks up various Perl
# constructs and things commonly used in man pages without requiring the user
# to add any explicit markup, and is applied to all non-literal text.  We're
# guaranteed that the text we're applying guesswork to does not contain any
# *roff formatting codes.  Note that the inserted font sequences must be
# treated later with mapfonts or textmapfonts.
#
# This method is very fragile, both in the regular expressions it uses and in
# the ordering of those modifications.  Care and testing is required when
# modifying it.
sub guesswork {
    my $self = shift;
    local $_ = shift;
    DEBUG > 5 and print "   Guesswork called on [$_]\n";

    # By the time we reach this point, all hypens will be escaped by adding a
    # backslash.  We want to undo that escaping if they're part of regular
    # words and there's only a single dash, since that's a real hyphen that
    # *roff gets to consider a possible break point.  Make sure that a dash
    # after the first character of a word stays non-breaking, however.
    #
    # Note that this is not user-controllable; we pretty much have to do this
    # transformation or *roff will mangle the output in unacceptable ways.
    s{
        ( (?:\G|^|\s) [\(\"]* [a-zA-Z] ) ( \\- )?
        ( (?: [a-zA-Z\']+ \\-)+ )
        ( [a-zA-Z\']+ ) (?= [\)\".?!,;:]* (?:\s|\Z|\\\ ) )
        \b
    } {
        my ($prefix, $hyphen, $main, $suffix) = ($1, $2, $3, $4);
        $hyphen ||= '';
        $main =~ s/\\-/-/g;
        $prefix . $hyphen . $main . $suffix;
    }egx;

    # Translate "--" into a real em-dash if it's used like one.  This means
    # that it's either surrounded by whitespace, it follows a regular word, or
    # it occurs between two regular words.
    if ($$self{MAGIC_EMDASH}) {
        s{          (\s) \\-\\- (\s)                } { $1 . '\*(--' . $2 }egx;
        s{ (\b[a-zA-Z]+) \\-\\- (\s|\Z|[a-zA-Z]+\b) } { $1 . '\*(--' . $2 }egx;
    }

    # Make words in all-caps a little bit smaller; they look better that way.
    # However, we don't want to change Perl code (like @ARGV), nor do we want
    # to fix the MIME in MIME-Version since it looks weird with the
    # full-height V.
    #
    # We change only a string of all caps (2) either at the beginning of the
    # line or following regular punctuation (like quotes) or whitespace (1),
    # and followed by either similar punctuation, an em-dash, or the end of
    # the line (3).
    if ($$self{MAGIC_SMALLCAPS}) {
        s{
            ( ^ | [\s\(\"\'\`\[\{<>] | \\\  )                   # (1)
            ( [A-Z] [A-Z] (?: [/A-Z+:\d_\$&] | \\- )* )         # (2)
            (?= [\s>\}\]\(\)\'\".?!,;] | \\*\(-- | \\\  | $ )   # (3)
        } {
            $1 . '\s-1' . $2 . '\s0'
        }egx;
    }

    # Note that from this point forward, we have to adjust for \s-1 and \s-0
    # strings inserted around things that we've made small-caps if later
    # transforms should work on those strings.

    # Italize functions in the form func(), including functions that are in
    # all capitals, but don't italize if there's anything between the parens.
    # The function must start with an alphabetic character or underscore and
    # then consist of word characters or colons.
    if ($$self{MAGIC_FUNC}) {
        s{
            ( \b | \\s-1 )
            ( [A-Za-z_] ([:\w] | \\s-?[01])+ \(\) )
        } {
            $1 . '\f(IS' . $2 . '\f(IE'
        }egx;
    }

    # Change references to manual pages to put the page name in italics but
    # the number in the regular font, with a thin space between the name and
    # the number.  Only recognize func(n) where func starts with an alphabetic
    # character or underscore and contains only word characters, periods (for
    # configuration file man pages), or colons, and n is a single digit,
    # optionally followed by some number of lowercase letters.  Note that this
    # does not recognize man page references like perl(l) or socket(3SOCKET).
    if ($$self{MAGIC_MANREF}) {
        s{
            ( \b | \\s-1 )
            ( [A-Za-z_] (?:[.:\w] | \\- | \\s-?[01])+ )
            ( \( \d [a-z]* \) )
        } {
            $1 . '\f(IS' . $2 . '\f(IE\|' . $3
        }egx;
    }

    # Convert simple Perl variable references to a fixed-width font.  Be
    # careful not to convert functions, though; there are too many subtleties
    # with them to want to perform this transformation.
    if ($$self{MAGIC_VARS}) {
        s{
           ( ^ | \s+ )
           ( [\$\@%] [\w:]+ )
           (?! \( )
        } {
            $1 . '\f(FS' . $2 . '\f(FE'
        }egx;
    }

    # Fix up double quotes.  Unfortunately, we miss this transformation if the
    # quoted text contains any code with formatting codes and there's not much
    # we can effectively do about that, which makes it somewhat unclear if
    # this is really a good idea.
    s{ \" ([^\"]+) \" } { '\*(L"' . $1 . '\*(R"' }egx;

    # Make C++ into \*(C+, which is a squinched version.
    if ($$self{MAGIC_CPP}) {
        s{ \b C\+\+ } {\\*\(C+}gx;
    }

    # Done.
    DEBUG > 5 and print "   Guesswork returning [$_]\n";
    return $_;
}

##############################################################################
# Output
##############################################################################

# When building up the *roff code, we don't use real *roff fonts.  Instead, we
# embed font codes of the form \f(<font>[SE] where <font> is one of B, I, or
# F, S stands for start, and E stands for end.  This method turns these into
# the right start and end codes.
#
# We add this level of complexity because the old pod2man didn't get code like
# B<someI<thing> else> right; after I<> it switched back to normal text rather
# than bold.  We take care of this by using variables that state whether bold,
# italic, or fixed are turned on as a combined pointer to our current font
# sequence, and set each to the number of current nestings of start tags for
# that font.
#
# \fP changes to the previous font, but only one previous font is kept.  We
# don't know what the outside level font is; normally it's R, but if we're
# inside a heading it could be something else.  So arrange things so that the
# outside font is always the "previous" font and end with \fP instead of \fR.
# Idea from Zack Weinberg.
sub mapfonts {
    my ($self, $text) = @_;
    my ($fixed, $bold, $italic) = (0, 0, 0);
    my %magic = (F => \$fixed, B => \$bold, I => \$italic);
    my $last = '\fR';
    $text =~ s<
        \\f\((.)(.)
    > <
        my $sequence = '';
        my $f;
        if ($last ne '\fR') { $sequence = '\fP' }
        ${ $magic{$1} } += ($2 eq 'S') ? 1 : -1;
        $f = $$self{FONTS}{ ($fixed && 1) . ($bold && 1) . ($italic && 1) };
        if ($f eq $last) {
            '';
        } else {
            if ($f ne '\fR') { $sequence .= $f }
            $last = $f;
            $sequence;
        }
    >gxe;
    return $text;
}

# Unfortunately, there is a bug in Solaris 2.6 nroff (not present in GNU
# groff) where the sequence \fB\fP\f(CW\fP leaves the font set to B rather
# than R, presumably because \f(CW doesn't actually do a font change.  To work
# around this, use a separate textmapfonts for text blocks where the default
# font is always R and only use the smart mapfonts for headings.
sub textmapfonts {
    my ($self, $text) = @_;
    my ($fixed, $bold, $italic) = (0, 0, 0);
    my %magic = (F => \$fixed, B => \$bold, I => \$italic);
    $text =~ s<
        \\f\((.)(.)
    > <
        ${ $magic{$1} } += ($2 eq 'S') ? 1 : -1;
        $$self{FONTS}{ ($fixed && 1) . ($bold && 1) . ($italic && 1) };
    >gxe;
    return $text;
}

# Given a command and a single argument that may or may not contain double
# quotes, handle double-quote formatting for it.  If there are no double
# quotes, just return the command followed by the argument in double quotes.
# If there are double quotes, use an if statement to test for nroff, and for
# nroff output the command followed by the argument in double quotes with
# embedded double quotes doubled.  For other formatters, remap paired double
# quotes to LQUOTE and RQUOTE.
sub switchquotes {
    my ($self, $command, $text, $extra) = @_;
    $text =~ s/\\\*\([LR]\"/\"/g;

    # We also have to deal with \*C` and \*C', which are used to add the
    # quotes around C<> text, since they may expand to " and if they do this
    # confuses the .SH macros and the like no end.  Expand them ourselves.
    # Also separate troff from nroff if there are any fixed-width fonts in use
    # to work around problems with Solaris nroff.
    my $c_is_quote = ($$self{LQUOTE} =~ /\"/) || ($$self{RQUOTE} =~ /\"/);
    my $fixedpat = join '|', @{ $$self{FONTS} }{'100', '101', '110', '111'};
    $fixedpat =~ s/\\/\\\\/g;
    $fixedpat =~ s/\(/\\\(/g;
    if ($text =~ m/\"/ || $text =~ m/$fixedpat/) {
        $text =~ s/\"/\"\"/g;
        my $nroff = $text;
        my $troff = $text;
        $troff =~ s/\"\"([^\"]*)\"\"/\`\`$1\'\'/g;
        if ($c_is_quote and $text =~ m/\\\*\(C[\'\`]/) {
            $nroff =~ s/\\\*\(C\`/$$self{LQUOTE}/g;
            $nroff =~ s/\\\*\(C\'/$$self{RQUOTE}/g;
            $troff =~ s/\\\*\(C[\'\`]//g;
        }
        $nroff = qq("$nroff") . ($extra ? " $extra" : '');
        $troff = qq("$troff") . ($extra ? " $extra" : '');

        # Work around the Solaris nroff bug where \f(CW\fP leaves the font set
        # to Roman rather than the actual previous font when used in headings.
        # troff output may still be broken, but at least we can fix nroff by
        # just switching the font changes to the non-fixed versions.
        $nroff =~ s/\Q$$self{FONTS}{100}\E(.*)\\f[PR]/$1/g;
        $nroff =~ s/\Q$$self{FONTS}{101}\E(.*)\\f([PR])/\\fI$1\\f$2/g;
        $nroff =~ s/\Q$$self{FONTS}{110}\E(.*)\\f([PR])/\\fB$1\\f$2/g;
        $nroff =~ s/\Q$$self{FONTS}{111}\E(.*)\\f([PR])/\\f\(BI$1\\f$2/g;

        # Now finally output the command.  Bother with .ie only if the nroff
        # and troff output aren't the same.
        if ($nroff ne $troff) {
            return ".ie n $command $nroff\n.el $command $troff\n";
        } else {
            return "$command $nroff\n";
        }
    } else {
        $text = qq("$text") . ($extra ? " $extra" : '');
        return "$command $text\n";
    }
}

# Protect leading quotes and periods against interpretation as commands.  Also
# protect anything starting with a backslash, since it could expand or hide
# something that *roff would interpret as a command.  This is overkill, but
# it's much simpler than trying to parse *roff here.
sub protect {
    my ($self, $text) = @_;
    $text =~ s/^([.\'\\])/\\&$1/mg;
    return $text;
}

# Make vertical whitespace if NEEDSPACE is set, appropriate to the indentation
# level the situation.  This function is needed since in *roff one has to
# create vertical whitespace after paragraphs and between some things, but
# other macros create their own whitespace.  Also close out a sequence of
# repeated =items, since calling makespace means we're about to begin the item
# body.
sub makespace {
    my ($self) = @_;
    $self->output (".PD\n") if $$self{ITEMS} > 1;
    $$self{ITEMS} = 0;
    $self->output ($$self{INDENT} > 0 ? ".Sp\n" : ".PP\n")
        if $$self{NEEDSPACE};
}

# Output any pending index entries, and optionally an index entry given as an
# argument.  Support multiple index entries in X<> separated by slashes, and
# strip special escapes from index entries.
sub outindex {
    my ($self, $section, $index) = @_;
    my @entries = map { split m%\s*/\s*% } @{ $$self{INDEX} };
    return unless ($section || @entries);

    # We're about to output all pending entries, so clear our pending queue.
    $$self{INDEX} = [];

    # Build the output.  Regular index entries are marked Xref, and headings
    # pass in their own section.  Undo some *roff formatting on headings.
    my @output;
    if (@entries) {
        push @output, [ 'Xref', join (' ', @entries) ];
    }
    if ($section) {
        $index =~ s/\\-/-/g;
        $index =~ s/\\(?:s-?\d|.\(..|.)//g;
        push @output, [ $section, $index ];
    }

    # Print out the .IX commands.
    for (@output) {
        my ($type, $entry) = @$_;
        $entry =~ s/\"/\"\"/g;
        $self->output (".IX $type " . '"' . $entry . '"' . "\n");
    }
}

# Output some text, without any additional changes.
sub output {
    my ($self, @text) = @_;
    print { $$self{output_fh} } @text;
}

##############################################################################
# Document initialization
##############################################################################

# Handle the start of the document.  Here we handle empty documents, as well
# as setting up our basic macros in a preamble and building the page title.
sub start_document {
    my ($self, $attrs) = @_;
    if ($$attrs{contentless} && !$$self{ALWAYS_EMIT_SOMETHING}) {
        DEBUG and print "Document is contentless\n";
        $$self{CONTENTLESS} = 1;
        return;
    }

    # Determine information for the preamble and then output it.
    my ($name, $section);
    if (defined $$self{name}) {
        $name = $$self{name};
        $section = $$self{section} || 1;
    } else {
        ($name, $section) = $self->devise_title;
    }
    my $date = $$self{date} || $self->devise_date;
    $self->preamble ($name, $section, $date)
        unless $self->bare_output or DEBUG > 9;

    # Initialize a few per-document variables.
    $$self{INDENT}    = 0;      # Current indentation level.
    $$self{INDENTS}   = [];     # Stack of indentations.
    $$self{INDEX}     = [];     # Index keys waiting to be printed.
    $$self{IN_NAME}   = 0;      # Whether processing the NAME section.
    $$self{ITEMS}     = 0;      # The number of consecutive =items.
    $$self{ITEMTYPES} = [];     # Stack of =item types, one per list.
    $$self{SHIFTWAIT} = 0;      # Whether there is a shift waiting.
    $$self{SHIFTS}    = [];     # Stack of .RS shifts.
    $$self{PENDING}   = [[]];   # Pending output.
}

# Handle the end of the document.  This does nothing but print out a final
# comment at the end of the document under debugging.
sub end_document {
    my ($self) = @_;
    return if $self->bare_output;
    return if ($$self{CONTENTLESS} && !$$self{ALWAYS_EMIT_SOMETHING});
    $self->output (q(.\" [End document]) . "\n") if DEBUG;
}

# Try to figure out the name and section from the file name and return them as
# a list, returning an empty name and section 1 if we can't find any better
# information.  Uses File::Basename and File::Spec as necessary.
sub devise_title {
    my ($self) = @_;
    my $name = $self->source_filename || '';
    my $section = $$self{section} || 1;
    $section = 3 if (!$$self{section} && $name =~ /\.pm\z/i);
    $name =~ s/\.p(od|[lm])\z//i;

    # If the section isn't 3, then the name defaults to just the basename of
    # the file.  Otherwise, assume we're dealing with a module.  We want to
    # figure out the full module name from the path to the file, but we don't
    # want to include too much of the path into the module name.  Lose
    # anything up to the first off:
    #
    #     */lib/*perl*/         standard or site_perl module
    #     */*perl*/lib/         from -Dprefix=/opt/perl
    #     */*perl*/             random module hierarchy
    #
    # which works.  Also strip off a leading site, site_perl, or vendor_perl
    # component, any OS-specific component, and any version number component,
    # and strip off an initial component of "lib" or "blib/lib" since that's
    # what ExtUtils::MakeMaker creates.  splitdir requires at least File::Spec
    # 0.8.
    if ($section !~ /^3/) {
        require File::Basename;
        $name = uc File::Basename::basename ($name);
    } else {
        require File::Spec;
        my ($volume, $dirs, $file) = File::Spec->splitpath ($name);
        my @dirs = File::Spec->splitdir ($dirs);
        my $cut = 0;
        my $i;
        for ($i = 0; $i < @dirs; $i++) {
            if ($dirs[$i] =~ /perl/) {
                $cut = $i + 1;
                $cut++ if ($dirs[$i + 1] && $dirs[$i + 1] eq 'lib');
                last;
            }
        }
        if ($cut > 0) {
            splice (@dirs, 0, $cut);
            shift @dirs if ($dirs[0] =~ /^(site|vendor)(_perl)?$/);
            shift @dirs if ($dirs[0] =~ /^[\d.]+$/);
            shift @dirs if ($dirs[0] =~ /^(.*-$^O|$^O-.*|$^O)$/);
        }
        shift @dirs if $dirs[0] eq 'lib';
        splice (@dirs, 0, 2) if ($dirs[0] eq 'blib' && $dirs[1] eq 'lib');

        # Remove empty directories when building the module name; they
        # occur too easily on Unix by doubling slashes.
        $name = join ('::', (grep { $_ ? $_ : () } @dirs), $file);
    }
    return ($name, $section);
}

# Determine the modification date and return that, properly formatted in ISO
# format.  If we can't get the modification date of the input, instead use the
# current time.  Pod::Simple returns a completely unuseful stringified file
# handle as the source_filename for input from a file handle, so we have to
# deal with that as well.
sub devise_date {
    my ($self) = @_;
    my $input = $self->source_filename;
    my $time;
    if ($input) {
        $time = (stat $input)[9] || time;
    } else {
        $time = time;
    }
    return strftime ('%Y-%m-%d', localtime $time);
}

# Print out the preamble and the title.  The meaning of the arguments to .TH
# unfortunately vary by system; some systems consider the fourth argument to
# be a "source" and others use it as a version number.  Generally it's just
# presented as the left-side footer, though, so it doesn't matter too much if
# a particular system gives it another interpretation.
#
# The order of date and release used to be reversed in older versions of this
# module, but this order is correct for both Solaris and Linux.
sub preamble {
    my ($self, $name, $section, $date) = @_;
    my $preamble = $self->preamble_template;

    # Build the index line and make sure that it will be syntactically valid.
    my $index = "$name $section";
    $index =~ s/\"/\"\"/g;

    # If name or section contain spaces, quote them (section really never
    # should, but we may as well be cautious).
    for ($name, $section) {
        if (/\s/) {
            s/\"/\"\"/g;
            $_ = '"' . $_ . '"';
        }
    }

    # Double quotes in date, since it will be quoted.
    $date =~ s/\"/\"\"/g;

    # Substitute into the preamble the configuration options.
    $preamble =~ s/\@CFONT\@/$$self{fixed}/;
    $preamble =~ s/\@LQUOTE\@/$$self{LQUOTE}/;
    $preamble =~ s/\@RQUOTE\@/$$self{RQUOTE}/;
    chomp $preamble;

    # Get the version information.
    my $version = $self->version_report;

    # Finally output everything.
    $self->output (<<"----END OF HEADER----");
.\\" Automatically generated by $version
.\\"
.\\" Standard preamble:
.\\" ========================================================================
$preamble
.\\" ========================================================================
.\\"
.IX Title "$index"
.TH $name $section "$date" "$$self{release}" "$$self{center}"
.\\" For nroff, turn off justification.  Always turn off hyphenation; it makes
.\\" way too many mistakes in technical documents.
.if n .ad l
.nh
----END OF HEADER----
    $self->output (".\\\" [End of preamble]\n") if DEBUG;
}

##############################################################################
# Text blocks
##############################################################################

# Handle a basic block of text.  The only tricky part of this is if this is
# the first paragraph of text after an =over, in which case we have to change
# indentations for *roff.
sub cmd_para {
    my ($self, $attrs, $text) = @_;
    my $line = $$attrs{start_line};

    # Output the paragraph.  We also have to handle =over without =item.  If
    # there's an =over without =item, SHIFTWAIT will be set, and we need to
    # handle creation of the indent here.  Add the shift to SHIFTS so that it
    # will be cleaned up on =back.
    $self->makespace;
    if ($$self{SHIFTWAIT}) {
        $self->output (".RS $$self{INDENT}\n");
        push (@{ $$self{SHIFTS} }, $$self{INDENT});
        $$self{SHIFTWAIT} = 0;
    }

    # Add the line number for debugging, but not in the NAME section just in
    # case the comment would confuse apropos.
    $self->output (".\\\" [At source line $line]\n")
        if defined ($line) && DEBUG && !$$self{IN_NAME};

    # Force exactly one newline at the end and strip unwanted trailing
    # whitespace at the end.
    $text =~ s/\s*$/\n/;

    # Output the paragraph.
    $self->output ($self->protect ($self->textmapfonts ($text)));
    $self->outindex;
    $$self{NEEDSPACE} = 1;
    return '';
}

# Handle a verbatim paragraph.  Put a null token at the beginning of each line
# to protect against commands and wrap in .Vb/.Ve (which we define in our
# prelude).
sub cmd_verbatim {
    my ($self, $attrs, $text) = @_;

    # Ignore an empty verbatim paragraph.
    return unless $text =~ /\S/;

    # Force exactly one newline at the end and strip unwanted trailing
    # whitespace at the end.
    $text =~ s/\s*$/\n/;

    # Get a count of the number of lines before the first blank line, which
    # we'll pass to .Vb as its parameter.  This tells *roff to keep that many
    # lines together.  We don't want to tell *roff to keep huge blocks
    # together.
    my @lines = split (/\n/, $text);
    my $unbroken = 0;
    for (@lines) {
        last if /^\s*$/;
        $unbroken++;
    }
    $unbroken = 10 if ($unbroken > 12 && !$$self{MAGIC_VNOPAGEBREAK_LIMIT});

    # Prepend a null token to each line.
    $text =~ s/^/\\&/gm;

    # Output the results.
    $self->makespace;
    $self->output (".Vb $unbroken\n$text.Ve\n");
    $$self{NEEDSPACE} = 1;
    return '';
}

# Handle literal text (produced by =for and similar constructs).  Just output
# it with the minimum of changes.
sub cmd_data {
    my ($self, $attrs, $text) = @_;
    $text =~ s/^\n+//;
    $text =~ s/\n{0,2}$/\n/;
    $self->output ($text);
    return '';
}

##############################################################################
# Headings
##############################################################################

# Common code for all headings.  This is called before the actual heading is
# output.  It returns the cleaned up heading text (putting the heading all on
# one line) and may do other things, like closing bad =item blocks.
sub heading_common {
    my ($self, $text, $line) = @_;
    $text =~ s/\s+$//;
    $text =~ s/\s*\n\s*/ /g;

    # This should never happen; it means that we have a heading after =item
    # without an intervening =back.  But just in case, handle it anyway.
    if ($$self{ITEMS} > 1) {
        $$self{ITEMS} = 0;
        $self->output (".PD\n");
    }

    # Output the current source line.
    $self->output ( ".\\\" [At source line $line]\n" )
        if defined ($line) && DEBUG;
    return $text;
}

# First level heading.  We can't output .IX in the NAME section due to a bug
# in some versions of catman, so don't output a .IX for that section.  .SH
# already uses small caps, so remove \s0 and \s-1.  Maintain IN_NAME as
# appropriate.
sub cmd_head1 {
    my ($self, $attrs, $text) = @_;
    $text =~ s/\\s-?\d//g;
    $text = $self->heading_common ($text, $$attrs{start_line});
    my $isname = ($text eq 'NAME' || $text =~ /\(NAME\)/);
    $self->output ($self->switchquotes ('.SH', $self->mapfonts ($text)));
    $self->outindex ('Header', $text) unless $isname;
    $$self{NEEDSPACE} = 0;
    $$self{IN_NAME} = $isname;
    return '';
}

# Second level heading.
sub cmd_head2 {
    my ($self, $attrs, $text) = @_;
    $text = $self->heading_common ($text, $$attrs{start_line});
    $self->output ($self->switchquotes ('.Sh', $self->mapfonts ($text)));
    $self->outindex ('Subsection', $text);
    $$self{NEEDSPACE} = 0;
    return '';
}

# Third level heading.  *roff doesn't have this concept, so just put the
# heading in italics as a normal paragraph.
sub cmd_head3 {
    my ($self, $attrs, $text) = @_;
    $text = $self->heading_common ($text, $$attrs{start_line});
    $self->makespace;
    $self->output ($self->textmapfonts ('\f(IS' . $text . '\f(IE') . "\n");
    $self->outindex ('Subsection', $text);
    $$self{NEEDSPACE} = 1;
    return '';
}

# Fourth level heading.  *roff doesn't have this concept, so just put the
# heading as a normal paragraph.
sub cmd_head4 {
    my ($self, $attrs, $text) = @_;
    $text = $self->heading_common ($text, $$attrs{start_line});
    $self->makespace;
    $self->output ($self->textmapfonts ($text) . "\n");
    $self->outindex ('Subsection', $text);
    $$self{NEEDSPACE} = 1;
    return '';
}

##############################################################################
# Formatting codes
##############################################################################

# All of the formatting codes that aren't handled internally by the parser,
# other than L<> and X<>.
sub cmd_b { return '\f(BS' . $_[2] . '\f(BE' }
sub cmd_i { return '\f(IS' . $_[2] . '\f(IE' }
sub cmd_f { return '\f(IS' . $_[2] . '\f(IE' }
sub cmd_c { return $_[0]->quote_literal ($_[2]) }

# Index entries are just added to the pending entries.
sub cmd_x {
    my ($self, $attrs, $text) = @_;
    push (@{ $$self{INDEX} }, $text);
    return '';
}

# Links reduce to the text that we're given, wrapped in angle brackets if it's
# a URL.
sub cmd_l {
    my ($self, $attrs, $text) = @_;
    return $$attrs{type} eq 'url' ? "<$text>" : $text;
}

##############################################################################
# List handling
##############################################################################

# Handle the beginning of an =over block.  Takes the type of the block as the
# first argument, and then the attr hash.  This is called by the handlers for
# the four different types of lists (bullet, number, text, and block).
sub over_common_start {
    my ($self, $type, $attrs) = @_;
    my $line = $$attrs{start_line};
    my $indent = $$attrs{indent};
    DEBUG > 3 and print " Starting =over $type (line $line, indent ",
        ($indent || '?'), "\n";

    # Find the indentation level.
    unless (defined ($indent) && $indent =~ /^[-+]?\d{1,4}\s*$/) {
        $indent = $$self{indent};
    }

    # If we've gotten multiple indentations in a row, we need to emit the
    # pending indentation for the last level that we saw and haven't acted on
    # yet.  SHIFTS is the stack of indentations that we've actually emitted
    # code for.
    if (@{ $$self{SHIFTS} } < @{ $$self{INDENTS} }) {
        $self->output (".RS $$self{INDENT}\n");
        push (@{ $$self{SHIFTS} }, $$self{INDENT});
    }

    # Now, do record-keeping.  INDENTS is a stack of indentations that we've
    # seen so far, and INDENT is the current level of indentation.  ITEMTYPES
    # is a stack of list types that we've seen.
    push (@{ $$self{INDENTS} }, $$self{INDENT});
    push (@{ $$self{ITEMTYPES} }, $type);
    $$self{INDENT} = $indent + 0;
    $$self{SHIFTWAIT} = 1;
}

# End an =over block.  Takes no options other than the class pointer.
# Normally, once we close a block and therefore remove something from INDENTS,
# INDENTS will now be longer than SHIFTS, indicating that we also need to emit
# *roff code to close the indent.  This isn't *always* true, depending on the
# circumstance.  If we're still inside an indentation, we need to emit another
# .RE and then a new .RS to unconfuse *roff.
sub over_common_end {
    my ($self) = @_;
    DEBUG > 3 and print " Ending =over\n";
    $$self{INDENT} = pop @{ $$self{INDENTS} };
    pop @{ $$self{ITEMTYPES} };

    # If we emitted code for that indentation, end it.
    if (@{ $$self{SHIFTS} } > @{ $$self{INDENTS} }) {
        $self->output (".RE\n");
        pop @{ $$self{SHIFTS} };
    }

    # If we're still in an indentation, *roff will have now lost track of the
    # right depth of that indentation, so fix that.
    if (@{ $$self{INDENTS} } > 0) {
        $self->output (".RE\n");
        $self->output (".RS $$self{INDENT}\n");
    }
    $$self{NEEDSPACE} = 1;
    $$self{SHIFTWAIT} = 0;
}

# Dispatch the start and end calls as appropriate.
sub start_over_bullet { my $s = shift; $s->over_common_start ('bullet', @_) }
sub start_over_number { my $s = shift; $s->over_common_start ('number', @_) }
sub start_over_text   { my $s = shift; $s->over_common_start ('text',   @_) }
sub start_over_block  { my $s = shift; $s->over_common_start ('block',  @_) }
sub end_over_bullet { $_[0]->over_common_end }
sub end_over_number { $_[0]->over_common_end }
sub end_over_text   { $_[0]->over_common_end }
sub end_over_block  { $_[0]->over_common_end }

# The common handler for all item commands.  Takes the type of the item, the
# attributes, and then the text of the item.
#
# Emit an index entry for anything that's interesting, but don't emit index
# entries for things like bullets and numbers.  Newlines in an item title are
# turned into spaces since *roff can't handle them embedded.
sub item_common {
    my ($self, $type, $attrs, $text) = @_;
    my $line = $$attrs{start_line};
    DEBUG > 3 and print "  $type item (line $line): $text\n";

    # Clean up the text.  We want to end up with two variables, one ($text)
    # which contains any body text after taking out the item portion, and
    # another ($item) which contains the actual item text.
    $text =~ s/\s+$//;
    my ($item, $index);
    if ($type eq 'bullet') {
        $item = "\\\(bu";
        $text =~ s/\n*$/\n/;
    } elsif ($type eq 'number') {
        $item = $$attrs{number} . '.';
    } else {
        $item = $text;
        $item =~ s/\s*\n\s*/ /g;
        $text = '';
        $index = $item if ($item =~ /\w/);
    }

    # Take care of the indentation.  If shifts and indents are equal, close
    # the top shift, since we're about to create an indentation with .IP.
    # Also output .PD 0 to turn off spacing between items if this item is
    # directly following another one.  We only have to do that once for a
    # whole chain of items so do it for the second item in the change.  Note
    # that makespace is what undoes this.
    if (@{ $$self{SHIFTS} } == @{ $$self{INDENTS} }) {
        $self->output (".RE\n");
        pop @{ $$self{SHIFTS} };
    }
    $self->output (".PD 0\n") if ($$self{ITEMS} == 1);

    # Now, output the item tag itself.
    $item = $self->textmapfonts ($item);
    $self->output ($self->switchquotes ('.IP', $item, $$self{INDENT}));
    $$self{NEEDSPACE} = 0;
    $$self{ITEMS}++;
    $$self{SHIFTWAIT} = 0;

    # If body text for this item was included, go ahead and output that now.
    if ($text) {
        $text =~ s/\s*$/\n/;
        $self->makespace;
        $self->output ($self->protect ($self->textmapfonts ($text)));
        $$self{NEEDSPACE} = 1;
    }
    $self->outindex ($index ? ('Item', $index) : ());
}

# Dispatch the item commands to the appropriate place.
sub cmd_item_bullet { my $self = shift; $self->item_common ('bullet', @_) }
sub cmd_item_number { my $self = shift; $self->item_common ('number', @_) }
sub cmd_item_text   { my $self = shift; $self->item_common ('text',   @_) }
sub cmd_item_block  { my $self = shift; $self->item_common ('block',  @_) }

##############################################################################
# Backward compatibility
##############################################################################

# Reset the underlying Pod::Simple object between calls to parse_from_file so
# that the same object can be reused to convert multiple pages.
sub parse_from_file {
    my $self = shift;
    $self->reinit;

    # Fake the old cutting option to Pod::Parser.  This fiddings with internal
    # Pod::Simple state and is quite ugly; we need a better approach.
    if (ref ($_[0]) eq 'HASH') {
        my $opts = shift @_;
        if (defined ($$opts{-cutting}) && !$$opts{-cutting}) {
            $$self{in_pod} = 1;
            $$self{last_was_blank} = 1;
        }
    }

    # Do the work.
    my $retval = $self->SUPER::parse_from_file (@_);

    # Flush output, since Pod::Simple doesn't do this.  Ideally we should also
    # close the file descriptor if we had to open one, but we can't easily
    # figure this out.
    my $fh = $self->output_fh ();
    my $oldfh = select $fh;
    my $oldflush = $|;
    $| = 1;
    print $fh '';
    $| = $oldflush;
    select $oldfh;
    return $retval;
}

# Pod::Simple failed to provide this backward compatibility function, so
# implement it ourselves.  File handles are one of the inputs that
# parse_from_file supports.
sub parse_from_filehandle {
    my $self = shift;
    $self->parse_from_file (@_);
}

##############################################################################
# Translation tables
##############################################################################

# The following table is adapted from Tom Christiansen's pod2man.  It assumes
# that the standard preamble has already been printed, since that's what
# defines all of the accent marks.  We really want to do something better than
# this when *roff actually supports other character sets itself, since these
# results are pretty poor.
#
# This only works in an ASCII world.  What to do in a non-ASCII world is very
# unclear.
@ESCAPES{0xA0 .. 0xFF} = (
    "\\ ", undef, undef, undef,            undef, undef, undef, undef,
    undef, undef, undef, undef,            undef, "\\%", undef, undef,

    undef, undef, undef, undef,            undef, undef, undef, undef,
    undef, undef, undef, undef,            undef, undef, undef, undef,

    "A\\*`",  "A\\*'", "A\\*^", "A\\*~",   "A\\*:", "A\\*o", "\\*(AE", "C\\*,",
    "E\\*`",  "E\\*'", "E\\*^", "E\\*:",   "I\\*`", "I\\*'", "I\\*^",  "I\\*:",

    "\\*(D-", "N\\*~", "O\\*`", "O\\*'",   "O\\*^", "O\\*~", "O\\*:",  undef,
    "O\\*/",  "U\\*`", "U\\*'", "U\\*^",   "U\\*:", "Y\\*'", "\\*(Th", "\\*8",

    "a\\*`",  "a\\*'", "a\\*^", "a\\*~",   "a\\*:", "a\\*o", "\\*(ae", "c\\*,",
    "e\\*`",  "e\\*'", "e\\*^", "e\\*:",   "i\\*`", "i\\*'", "i\\*^",  "i\\*:",

    "\\*(d-", "n\\*~", "o\\*`", "o\\*'",   "o\\*^", "o\\*~", "o\\*:",  undef,
    "o\\*/" , "u\\*`", "u\\*'", "u\\*^",   "u\\*:", "y\\*'", "\\*(th", "y\\*:",
) if ASCII;

# Make sure that at least this works even outside of ASCII.
$ESCAPES{ord("\\")} = "\\e";

##############################################################################
# Premable
##############################################################################

# The following is the static preamble which starts all *roff output we
# generate.  It's completely static except for the font to use as a
# fixed-width font, which is designed by @CFONT@, and the left and right
# quotes to use for C<> text, designated by @LQOUTE@ and @RQUOTE@.
sub preamble_template {
    return <<'----END OF PREAMBLE----';
.de Sh \" Subsection heading
.br
.if t .Sp
.ne 5
.PP
\fB\\$1\fR
.PP
..
.de Sp \" Vertical space (when we can't use .PP)
.if t .sp .5v
.if n .sp
..
.de Vb \" Begin verbatim text
.ft @CFONT@
.nf
.ne \\$1
..
.de Ve \" End verbatim text
.ft R
.fi
..
.\" Set up some character translations and predefined strings.  \*(-- will
.\" give an unbreakable dash, \*(PI will give pi, \*(L" will give a left
.\" double quote, and \*(R" will give a right double quote.  \*(C+ will
.\" give a nicer C++.  Capital omega is used to do unbreakable dashes and
.\" therefore won't be available.  \*(C` and \*(C' expand to `' in nroff,
.\" nothing in troff, for use with C<>.
.tr \(*W-
.ds C+ C\v'-.1v'\h'-1p'\s-2+\h'-1p'+\s0\v'.1v'\h'-1p'
.ie n \{\
.    ds -- \(*W-
.    ds PI pi
.    if (\n(.H=4u)&(1m=24u) .ds -- \(*W\h'-12u'\(*W\h'-12u'-\" diablo 10 pitch
.    if (\n(.H=4u)&(1m=20u) .ds -- \(*W\h'-12u'\(*W\h'-8u'-\"  diablo 12 pitch
.    ds L" ""
.    ds R" ""
.    ds C` @LQUOTE@
.    ds C' @RQUOTE@
'br\}
.el\{\
.    ds -- \|\(em\|
.    ds PI \(*p
.    ds L" ``
.    ds R" ''
'br\}
.\"
.\" Escape single quotes in literal strings from groff's Unicode transform.
.ie \n(.g .ds Aq \(aq
.el       .ds Aq '
.\"
.\" If the F register is turned on, we'll generate index entries on stderr for
.\" titles (.TH), headers (.SH), subsections (.Sh), items (.Ip), and index
.\" entries marked with X<> in POD.  Of course, you'll have to process the
.\" output yourself in some meaningful fashion.
.ie \nF \{\
.    de IX
.    tm Index:\\$1\t\\n%\t"\\$2"
..
.    nr % 0
.    rr F
.\}
.el \{\
.    de IX
..
.\}
.\"
.\" Accent mark definitions (@(#)ms.acc 1.5 88/02/08 SMI; from UCB 4.2).
.\" Fear.  Run.  Save yourself.  No user-serviceable parts.
.    \" fudge factors for nroff and troff
.if n \{\
.    ds #H 0
.    ds #V .8m
.    ds #F .3m
.    ds #[ \f1
.    ds #] \fP
.\}
.if t \{\
.    ds #H ((1u-(\\\\n(.fu%2u))*.13m)
.    ds #V .6m
.    ds #F 0
.    ds #[ \&
.    ds #] \&
.\}
.    \" simple accents for nroff and troff
.if n \{\
.    ds ' \&
.    ds ` \&
.    ds ^ \&
.    ds , \&
.    ds ~ ~
.    ds /
.\}
.if t \{\
.    ds ' \\k:\h'-(\\n(.wu*8/10-\*(#H)'\'\h"|\\n:u"
.    ds ` \\k:\h'-(\\n(.wu*8/10-\*(#H)'\`\h'|\\n:u'
.    ds ^ \\k:\h'-(\\n(.wu*10/11-\*(#H)'^\h'|\\n:u'
.    ds , \\k:\h'-(\\n(.wu*8/10)',\h'|\\n:u'
.    ds ~ \\k:\h'-(\\n(.wu-\*(#H-.1m)'~\h'|\\n:u'
.    ds / \\k:\h'-(\\n(.wu*8/10-\*(#H)'\z\(sl\h'|\\n:u'
.\}
.    \" troff and (daisy-wheel) nroff accents
.ds : \\k:\h'-(\\n(.wu*8/10-\*(#H+.1m+\*(#F)'\v'-\*(#V'\z.\h'.2m+\*(#F'.\h'|\\n:u'\v'\*(#V'
.ds 8 \h'\*(#H'\(*b\h'-\*(#H'
.ds o \\k:\h'-(\\n(.wu+\w'\(de'u-\*(#H)/2u'\v'-.3n'\*(#[\z\(de\v'.3n'\h'|\\n:u'\*(#]
.ds d- \h'\*(#H'\(pd\h'-\w'~'u'\v'-.25m'\f2\(hy\fP\v'.25m'\h'-\*(#H'
.ds D- D\\k:\h'-\w'D'u'\v'-.11m'\z\(hy\v'.11m'\h'|\\n:u'
.ds th \*(#[\v'.3m'\s+1I\s-1\v'-.3m'\h'-(\w'I'u*2/3)'\s-1o\s+1\*(#]
.ds Th \*(#[\s+2I\s-2\h'-\w'I'u*3/5'\v'-.3m'o\v'.3m'\*(#]
.ds ae a\h'-(\w'a'u*4/10)'e
.ds Ae A\h'-(\w'A'u*4/10)'E
.    \" corrections for vroff
.if v .ds ~ \\k:\h'-(\\n(.wu*9/10-\*(#H)'\s-2\u~\d\s+2\h'|\\n:u'
.if v .ds ^ \\k:\h'-(\\n(.wu*10/11-\*(#H)'\v'-.4m'^\v'.4m'\h'|\\n:u'
.    \" for low resolution devices (crt and lpr)
.if \n(.H>23 .if \n(.V>19 \
\{\
.    ds : e
.    ds 8 ss
.    ds o a
.    ds d- d\h'-1'\(ga
.    ds D- D\h'-1'\(hy
.    ds th \o'bp'
.    ds Th \o'LP'
.    ds ae ae
.    ds Ae AE
.\}
.rm #[ #] #H #V #F C
----END OF PREAMBLE----
#`# for cperl-mode
}

##############################################################################
# Module return value and documentation
##############################################################################

1;
__END__

=head1 NAME

Pod::Man - Convert POD data to formatted *roff input

=head1 SYNOPSIS

    use Pod::Man;
    my $parser = Pod::Man->new (release => $VERSION, section => 8);

    # Read POD from STDIN and write to STDOUT.
    $parser->parse_file (\*STDIN);

    # Read POD from file.pod and write to file.1.
    $parser->parse_from_file ('file.pod', 'file.1');

=head1 DESCRIPTION

Pod::Man is a module to convert documentation in the POD format (the
preferred language for documenting Perl) into *roff input using the man
macro set.  The resulting *roff code is suitable for display on a terminal
using L<nroff(1)>, normally via L<man(1)>, or printing using L<troff(1)>.
It is conventionally invoked using the driver script B<pod2man>, but it can
also be used directly.

As a derived class from Pod::Simple, Pod::Man supports the same methods and
interfaces.  See L<Pod::Simple> for all the details.

new() can take options, in the form of key/value pairs that control the
behavior of the parser.  See below for details.

If no options are given, Pod::Man uses the name of the input file with any
trailing C<.pod>, C<.pm>, or C<.pl> stripped as the man page title, to
section 1 unless the file ended in C<.pm> in which case it defaults to
section 3, to a centered title of "User Contributed Perl Documentation", to
a centered footer of the Perl version it is run with, and to a left-hand
footer of the modification date of its input (or the current date if given
STDIN for input).

Pod::Man assumes that your *roff formatters have a fixed-width font named
CW.  If yours is called something else (like CR), use the C<fixed> option to
specify it.  This generally only matters for troff output for printing.
Similarly, you can set the fonts used for bold, italic, and bold italic
fixed-width output.

Besides the obvious pod conversions, Pod::Man also takes care of formatting
func(), func(3), and simple variable references like $foo or @bar so you
don't have to use code escapes for them; complex expressions like
C<$fred{'stuff'}> will still need to be escaped, though.  It also translates
dashes that aren't used as hyphens into en dashes, makes long dashes--like
this--into proper em dashes, fixes "paired quotes," makes C++ look right,
puts a little space between double underbars, makes ALLCAPS a teeny bit
smaller in B<troff>, and escapes stuff that *roff treats as special so that
you don't have to.

The recognized options to new() are as follows.  All options take a single
argument.

=over 4

=item center

Sets the centered page header to use instead of "User Contributed Perl
Documentation".

=item date

Sets the left-hand footer.  By default, the modification date of the input
file will be used, or the current date if stat() can't find that file (the
case if the input is from STDIN), and the date will be formatted as
YYYY-MM-DD.

=item fixed

The fixed-width font to use for vertabim text and code.  Defaults to CW.
Some systems may want CR instead.  Only matters for B<troff> output.

=item fixedbold

Bold version of the fixed-width font.  Defaults to CB.  Only matters for
B<troff> output.

=item fixeditalic

Italic version of the fixed-width font (actually, something of a misnomer,
since most fixed-width fonts only have an oblique version, not an italic
version).  Defaults to CI.  Only matters for B<troff> output.

=item fixedbolditalic

Bold italic (probably actually oblique) version of the fixed-width font.
Pod::Man doesn't assume you have this, and defaults to CB.  Some systems
(such as Solaris) have this font available as CX.  Only matters for B<troff>
output.

=item name

Set the name of the manual page.  Without this option, the manual name is
set to the uppercased base name of the file being converted unless the
manual section is 3, in which case the path is parsed to see if it is a Perl
module path.  If it is, a path like C<.../lib/Pod/Man.pm> is converted into
a name like C<Pod::Man>.  This option, if given, overrides any automatic
determination of the name.

=item quotes

Sets the quote marks used to surround CE<lt>> text.  If the value is a
single character, it is used as both the left and right quote; if it is two
characters, the first character is used as the left quote and the second as
the right quoted; and if it is four characters, the first two are used as
the left quote and the second two as the right quote.

This may also be set to the special value C<none>, in which case no quote
marks are added around CE<lt>> text (but the font is still changed for troff
output).

=item release

Set the centered footer.  By default, this is the version of Perl you run
Pod::Man under.  Note that some system an macro sets assume that the
centered footer will be a modification date and will prepend something like
"Last modified: "; if this is the case, you may want to set C<release> to
the last modified date and C<date> to the version number.

=item section

Set the section for the C<.TH> macro.  The standard section numbering
convention is to use 1 for user commands, 2 for system calls, 3 for
functions, 4 for devices, 5 for file formats, 6 for games, 7 for
miscellaneous information, and 8 for administrator commands.  There is a lot
of variation here, however; some systems (like Solaris) use 4 for file
formats, 5 for miscellaneous information, and 7 for devices.  Still others
use 1m instead of 8, or some mix of both.  About the only section numbers
that are reliably consistent are 1, 2, and 3.

By default, section 1 will be used unless the file ends in .pm in which case
section 3 will be selected.

=back

The standard Pod::Simple method parse_file() takes one argument naming the
POD file to read from.  By default, the output is sent to STDOUT, but this
can be changed with the output_fd() method.

The standard Pod::Simple method parse_from_file() takes up to two
arguments, the first being the input file to read POD from and the second
being the file to write the formatted output to.

You can also call parse_lines() to parse an array of lines or
parse_string_document() to parse a document already in memory.  To put the
output into a string instead of a file handle, call the output_string()
method.  See L<Pod::Simple> for the specific details.

=head1 DIAGNOSTICS

=over 4

=item roff font should be 1 or 2 chars, not "%s"

(F) You specified a *roff font (using C<fixed>, C<fixedbold>, etc.) that
wasn't either one or two characters.  Pod::Man doesn't support *roff fonts
longer than two characters, although some *roff extensions do (the canonical
versions of B<nroff> and B<troff> don't either).

=item Invalid quote specification "%s"

(F) The quote specification given (the quotes option to the constructor) was
invalid.  A quote specification must be one, two, or four characters long.

=back

=head1 BUGS

Eight-bit input data isn't handled at all well at present.  The correct
approach would be to map EE<lt>E<gt> escapes to the appropriate UTF-8
characters and then do a translation pass on the output according to the
user-specified output character set.  Unfortunately, we can't send eight-bit
data directly to the output unless the user says this is okay, since some
vendor *roff implementations can't handle eight-bit data.  If the *roff
implementation can, however, that's far superior to the current hacked
characters that only work under troff.

There is currently no way to turn off the guesswork that tries to format
unmarked text appropriately, and sometimes it isn't wanted (particularly
when using POD to document something other than Perl).  Most of the work
towards fixing this has now been done, however, and all that's still needed
is a user interface.

The NAME section should be recognized specially and index entries emitted
for everything in that section.  This would have to be deferred until the
next section, since extraneous things in NAME tends to confuse various man
page processors.  Currently, no index entries are emitted for anything in
NAME.

Pod::Man doesn't handle font names longer than two characters.  Neither do
most B<troff> implementations, but GNU troff does as an extension.  It would
be nice to support as an option for those who want to use it.

The preamble added to each output file is rather verbose, and most of it
is only necessary in the presence of non-ASCII characters.  It would
ideally be nice if all of those definitions were only output if needed,
perhaps on the fly as the characters are used.

Pod::Man is excessively slow.

=head1 CAVEATS

The handling of hyphens and em dashes is somewhat fragile, and one may get
the wrong one under some circumstances.  This should only matter for
B<troff> output.

When and whether to use small caps is somewhat tricky, and Pod::Man doesn't
necessarily get it right.

Converting neutral double quotes to properly matched double quotes doesn't
work unless there are no formatting codes between the quote marks.  This
only matters for troff output.

=head1 AUTHOR

Russ Allbery <rra@stanford.edu>, based I<very> heavily on the original
B<pod2man> by Tom Christiansen <tchrist@mox.perl.com>.  The modifications to
work with Pod::Simple instead of Pod::Parser were originally contributed by
Sean Burke (but I've since hacked them beyond recognition and all bugs are
mine).

=head1 COPYRIGHT AND LICENSE

Copyright 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006, 2007
by Russ Allbery <rra@stanford.edu>.

This program is free software; you may redistribute it and/or modify it
under the same terms as Perl itself.

=head1 SEE ALSO

L<Pod::Simple>, L<perlpod(1)>, L<pod2man(1)>, L<nroff(1)>, L<troff(1)>,
L<man(1)>, L<man(7)>

Ossanna, Joseph F., and Brian W. Kernighan.  "Troff User's Manual,"
Computing Science Technical Report No. 54, AT&T Bell Laboratories.  This is
the best documentation of standard B<nroff> and B<troff>.  At the time of
this writing, it's available at
L<http://www.cs.bell-labs.com/cm/cs/cstr.html>.

The man page documenting the man macro set may be L<man(5)> instead of
L<man(7)> on your system.  Also, please see L<pod2man(1)> for extensive
documentation on writing manual pages if you've not done it before and
aren't familiar with the conventions.

The current version of this module is always available from its web site at
L<http://www.eyrie.org/~eagle/software/podlators/>.  It is also part of the
Perl core distribution as of 5.6.0.

=cut
