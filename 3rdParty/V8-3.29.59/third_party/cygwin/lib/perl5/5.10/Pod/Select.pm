#############################################################################
# Pod/Select.pm -- function to select portions of POD docs
#
# Copyright (C) 1996-2000 by Bradford Appleton. All rights reserved.
# This file is part of "PodParser". PodParser is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

package Pod::Select;

use vars qw($VERSION);
$VERSION = 1.35;  ## Current version of this package
require  5.005;    ## requires this Perl version or later

#############################################################################

=head1 NAME

Pod::Select, podselect() - extract selected sections of POD from input

=head1 SYNOPSIS

    use Pod::Select;

    ## Select all the POD sections for each file in @filelist
    ## and print the result on standard output.
    podselect(@filelist);

    ## Same as above, but write to tmp.out
    podselect({-output => "tmp.out"}, @filelist):

    ## Select from the given filelist, only those POD sections that are
    ## within a 1st level section named any of: NAME, SYNOPSIS, OPTIONS.
    podselect({-sections => ["NAME|SYNOPSIS", "OPTIONS"]}, @filelist):

    ## Select the "DESCRIPTION" section of the PODs from STDIN and write
    ## the result to STDERR.
    podselect({-output => ">&STDERR", -sections => ["DESCRIPTION"]}, \*STDIN);

or

    use Pod::Select;

    ## Create a parser object for selecting POD sections from the input
    $parser = new Pod::Select();

    ## Select all the POD sections for each file in @filelist
    ## and print the result to tmp.out.
    $parser->parse_from_file("<&STDIN", "tmp.out");

    ## Select from the given filelist, only those POD sections that are
    ## within a 1st level section named any of: NAME, SYNOPSIS, OPTIONS.
    $parser->select("NAME|SYNOPSIS", "OPTIONS");
    for (@filelist) { $parser->parse_from_file($_); }

    ## Select the "DESCRIPTION" and "SEE ALSO" sections of the PODs from
    ## STDIN and write the result to STDERR.
    $parser->select("DESCRIPTION");
    $parser->add_selection("SEE ALSO");
    $parser->parse_from_filehandle(\*STDIN, \*STDERR);

=head1 REQUIRES

perl5.005, Pod::Parser, Exporter, Carp

=head1 EXPORTS

podselect()

=head1 DESCRIPTION

B<podselect()> is a function which will extract specified sections of
pod documentation from an input stream. This ability is provided by the
B<Pod::Select> module which is a subclass of B<Pod::Parser>.
B<Pod::Select> provides a method named B<select()> to specify the set of
POD sections to select for processing/printing. B<podselect()> merely
creates a B<Pod::Select> object and then invokes the B<podselect()>
followed by B<parse_from_file()>.

=head1 SECTION SPECIFICATIONS

B<podselect()> and B<Pod::Select::select()> may be given one or more
"section specifications" to restrict the text processed to only the
desired set of sections and their corresponding subsections.  A section
specification is a string containing one or more Perl-style regular
expressions separated by forward slashes ("/").  If you need to use a
forward slash literally within a section title you can escape it with a
backslash ("\/").

The formal syntax of a section specification is:

=over 4

=item *

I<head1-title-regex>/I<head2-title-regex>/...

=back

Any omitted or empty regular expressions will default to ".*".
Please note that each regular expression given is implicitly
anchored by adding "^" and "$" to the beginning and end.  Also, if a
given regular expression starts with a "!" character, then the
expression is I<negated> (so C<!foo> would match anything I<except>
C<foo>).

Some example section specifications follow.

=over 4

=item *

Match the C<NAME> and C<SYNOPSIS> sections and all of their subsections:

C<NAME|SYNOPSIS>

=item *

Match only the C<Question> and C<Answer> subsections of the C<DESCRIPTION>
section:

C<DESCRIPTION/Question|Answer>

=item *

Match the C<Comments> subsection of I<all> sections:

C</Comments>

=item *

Match all subsections of C<DESCRIPTION> I<except> for C<Comments>:

C<DESCRIPTION/!Comments>

=item *

Match the C<DESCRIPTION> section but do I<not> match any of its subsections:

C<DESCRIPTION/!.+>

=item *

Match all top level sections but none of their subsections:

C</!.+>

=back 

=begin _NOT_IMPLEMENTED_

=head1 RANGE SPECIFICATIONS

B<podselect()> and B<Pod::Select::select()> may be given one or more
"range specifications" to restrict the text processed to only the
desired ranges of paragraphs in the desired set of sections. A range
specification is a string containing a single Perl-style regular
expression (a regex), or else two Perl-style regular expressions
(regexs) separated by a ".." (Perl's "range" operator is "..").
The regexs in a range specification are delimited by forward slashes
("/").  If you need to use a forward slash literally within a regex you
can escape it with a backslash ("\/").

The formal syntax of a range specification is:

=over 4

=item *

/I<start-range-regex>/[../I<end-range-regex>/]

=back

Where each the item inside square brackets (the ".." followed by the
end-range-regex) is optional. Each "range-regex" is of the form:

    =cmd-expr text-expr

Where I<cmd-expr> is intended to match the name of one or more POD
commands, and I<text-expr> is intended to match the paragraph text for
the command. If a range-regex is supposed to match a POD command, then
the first character of the regex (the one after the initial '/')
absolutely I<must> be a single '=' character; it may not be anything
else (not even a regex meta-character) if it is supposed to match
against the name of a POD command.

If no I<=cmd-expr> is given then the text-expr will be matched against
plain textblocks unless it is preceded by a space, in which case it is
matched against verbatim text-blocks. If no I<text-expr> is given then
only the command-portion of the paragraph is matched against.

Note that these two expressions are each implicitly anchored. This
means that when matching against the command-name, there will be an
implicit '^' and '$' around the given I<=cmd-expr>; and when matching
against the paragraph text there will be an implicit '\A' and '\Z'
around the given I<text-expr>.

Unlike with section-specs, the '!' character does I<not> have any special
meaning (negation or otherwise) at the beginning of a range-spec!

Some example range specifications follow.

=over 4

=item
Match all C<=for html> paragraphs:

C</=for html/>

=item
Match all paragraphs between C<=begin html> and C<=end html>
(note that this will I<not> work correctly if such sections
are nested):

C</=begin html/../=end html/>

=item
Match all paragraphs between the given C<=item> name until the end of the
current section:

C</=item mine/../=head\d/>

=item
Match all paragraphs between the given C<=item> until the next item, or
until the end of the itemized list (note that this will I<not> work as
desired if the item contains an itemized list nested within it):

C</=item mine/../=(item|back)/>

=back 

=end _NOT_IMPLEMENTED_

=cut

#############################################################################

use strict;
#use diagnostics;
use Carp;
use Pod::Parser 1.04;
use vars qw(@ISA @EXPORT $MAX_HEADING_LEVEL);

@ISA = qw(Pod::Parser);
@EXPORT = qw(&podselect);

## Maximum number of heading levels supported for '=headN' directives
*MAX_HEADING_LEVEL = \3;

#############################################################################

=head1 OBJECT METHODS

The following methods are provided in this module. Each one takes a
reference to the object itself as an implicit first parameter.

=cut

##---------------------------------------------------------------------------

## =begin _PRIVATE_
## 
## =head1 B<_init_headings()>
## 
## Initialize the current set of active section headings.
## 
## =cut
## 
## =end _PRIVATE_

use vars qw(%myData @section_headings);

sub _init_headings {
    my $self = shift;
    local *myData = $self;

    ## Initialize current section heading titles if necessary
    unless (defined $myData{_SECTION_HEADINGS}) {
        local *section_headings = $myData{_SECTION_HEADINGS} = [];
        for (my $i = 0; $i < $MAX_HEADING_LEVEL; ++$i) {
            $section_headings[$i] = '';
        }
    }
}

##---------------------------------------------------------------------------

=head1 B<curr_headings()>

            ($head1, $head2, $head3, ...) = $parser->curr_headings();
            $head1 = $parser->curr_headings(1);

This method returns a list of the currently active section headings and
subheadings in the document being parsed. The list of headings returned
corresponds to the most recently parsed paragraph of the input.

If an argument is given, it must correspond to the desired section
heading number, in which case only the specified section heading is
returned. If there is no current section heading at the specified
level, then C<undef> is returned.

=cut

sub curr_headings {
    my $self = shift;
    $self->_init_headings()  unless (defined $self->{_SECTION_HEADINGS});
    my @headings = @{ $self->{_SECTION_HEADINGS} };
    return (@_ > 0  and  $_[0] =~ /^\d+$/) ? $headings[$_[0] - 1] : @headings;
}

##---------------------------------------------------------------------------

=head1 B<select()>

            $parser->select($section_spec1,$section_spec2,...);

This method is used to select the particular sections and subsections of
POD documentation that are to be printed and/or processed. The existing
set of selected sections is I<replaced> with the given set of sections.
See B<add_selection()> for adding to the current set of selected
sections.

Each of the C<$section_spec> arguments should be a section specification
as described in L<"SECTION SPECIFICATIONS">.  The section specifications
are parsed by this method and the resulting regular expressions are
stored in the invoking object.

If no C<$section_spec> arguments are given, then the existing set of
selected sections is cleared out (which means C<all> sections will be
processed).

This method should I<not> normally be overridden by subclasses.

=cut

use vars qw(@selected_sections);

sub select {
    my $self = shift;
    my @sections = @_;
    local *myData = $self;
    local $_;

### NEED TO DISCERN A SECTION-SPEC FROM A RANGE-SPEC (look for m{^/.+/$}?)

    ##---------------------------------------------------------------------
    ## The following is a blatant hack for backward compatibility, and for
    ## implementing add_selection(). If the *first* *argument* is the
    ## string "+", then the remaining section specifications are *added*
    ## to the current set of selections; otherwise the given section
    ## specifications will *replace* the current set of selections.
    ##
    ## This should probably be fixed someday, but for the present time,
    ## it seems incredibly unlikely that "+" would ever correspond to
    ## a legitimate section heading
    ##---------------------------------------------------------------------
    my $add = ($sections[0] eq "+") ? shift(@sections) : "";

    ## Reset the set of sections to use
    unless (@sections > 0) {
        delete $myData{_SELECTED_SECTIONS}  unless ($add);
        return;
    }
    $myData{_SELECTED_SECTIONS} = []
        unless ($add  &&  exists $myData{_SELECTED_SECTIONS});
    local *selected_sections = $myData{_SELECTED_SECTIONS};

    ## Compile each spec
    my $spec;
    for $spec (@sections) {
        if ( defined($_ = &_compile_section_spec($spec)) ) {
            ## Store them in our sections array
            push(@selected_sections, $_);
        }
        else {
            carp "Ignoring section spec \"$spec\"!\n";
        }
    }
}

##---------------------------------------------------------------------------

=head1 B<add_selection()>

            $parser->add_selection($section_spec1,$section_spec2,...);

This method is used to add to the currently selected sections and
subsections of POD documentation that are to be printed and/or
processed. See <select()> for replacing the currently selected sections.

Each of the C<$section_spec> arguments should be a section specification
as described in L<"SECTION SPECIFICATIONS">. The section specifications
are parsed by this method and the resulting regular expressions are
stored in the invoking object.

This method should I<not> normally be overridden by subclasses.

=cut

sub add_selection {
    my $self = shift;
    $self->select("+", @_);
}

##---------------------------------------------------------------------------

=head1 B<clear_selections()>

            $parser->clear_selections();

This method takes no arguments, it has the exact same effect as invoking
<select()> with no arguments.

=cut

sub clear_selections {
    my $self = shift;
    $self->select();
}

##---------------------------------------------------------------------------

=head1 B<match_section()>

            $boolean = $parser->match_section($heading1,$heading2,...);

Returns a value of true if the given section and subsection heading
titles match any of the currently selected section specifications in
effect from prior calls to B<select()> and B<add_selection()> (or if
there are no explictly selected/deselected sections).

The arguments C<$heading1>, C<$heading2>, etc. are the heading titles of
the corresponding sections, subsections, etc. to try and match.  If
C<$headingN> is omitted then it defaults to the current corresponding
section heading title in the input.

This method should I<not> normally be overridden by subclasses.

=cut

sub match_section {
    my $self = shift;
    my (@headings) = @_;
    local *myData = $self;

    ## Return true if no restrictions were explicitly specified
    my $selections = (exists $myData{_SELECTED_SECTIONS})
                       ?  $myData{_SELECTED_SECTIONS}  :  undef;
    return  1  unless ((defined $selections) && (@{$selections} > 0));

    ## Default any unspecified sections to the current one
    my @current_headings = $self->curr_headings();
    for (my $i = 0; $i < $MAX_HEADING_LEVEL; ++$i) {
        (defined $headings[$i])  or  $headings[$i] = $current_headings[$i];
    }

    ## Look for a match against the specified section expressions
    my ($section_spec, $regex, $negated, $match);
    for $section_spec ( @{$selections} ) {
        ##------------------------------------------------------
        ## Each portion of this spec must match in order for
        ## the spec to be matched. So we will start with a 
        ## match-value of 'true' and logically 'and' it with
        ## the results of matching a given element of the spec.
        ##------------------------------------------------------
        $match = 1;
        for (my $i = 0; $i < $MAX_HEADING_LEVEL; ++$i) {
            $regex   = $section_spec->[$i];
            $negated = ($regex =~ s/^\!//);
            $match  &= ($negated ? ($headings[$i] !~ /${regex}/)
                                 : ($headings[$i] =~ /${regex}/));
            last unless ($match);
        }
        return  1  if ($match);
    }
    return  0;  ## no match
}

##---------------------------------------------------------------------------

=head1 B<is_selected()>

            $boolean = $parser->is_selected($paragraph);

This method is used to determine if the block of text given in
C<$paragraph> falls within the currently selected set of POD sections
and subsections to be printed or processed. This method is also
responsible for keeping track of the current input section and
subsections. It is assumed that C<$paragraph> is the most recently read
(but not yet processed) input paragraph.

The value returned will be true if the C<$paragraph> and the rest of the
text in the same section as C<$paragraph> should be selected (included)
for processing; otherwise a false value is returned.

=cut

sub is_selected {
    my ($self, $paragraph) = @_;
    local $_;
    local *myData = $self;

    $self->_init_headings()  unless (defined $myData{_SECTION_HEADINGS});

    ## Keep track of current sections levels and headings
    $_ = $paragraph;
    if (/^=((?:sub)*)(?:head(?:ing)?|sec(?:tion)?)(\d*)\s+(.*?)\s*$/)
    {
        ## This is a section heading command
        my ($level, $heading) = ($2, $3);
        $level = 1 + (length($1) / 3)  if ((! length $level) || (length $1));
        ## Reset the current section heading at this level
        $myData{_SECTION_HEADINGS}->[$level - 1] = $heading;
        ## Reset subsection headings of this one to empty
        for (my $i = $level; $i < $MAX_HEADING_LEVEL; ++$i) {
            $myData{_SECTION_HEADINGS}->[$i] = '';
        }
    }

    return  $self->match_section();
}

#############################################################################

=head1 EXPORTED FUNCTIONS

The following functions are exported by this module. Please note that
these are functions (not methods) and therefore C<do not> take an
implicit first argument.

=cut

##---------------------------------------------------------------------------

=head1 B<podselect()>

            podselect(\%options,@filelist);

B<podselect> will print the raw (untranslated) POD paragraphs of all
POD sections in the given input files specified by C<@filelist>
according to the given options.

If any argument to B<podselect> is a reference to a hash
(associative array) then the values with the following keys are
processed as follows:

=over 4

=item B<-output>

A string corresponding to the desired output file (or ">&STDOUT"
or ">&STDERR"). The default is to use standard output.

=item B<-sections>

A reference to an array of sections specifications (as described in
L<"SECTION SPECIFICATIONS">) which indicate the desired set of POD
sections and subsections to be selected from input. If no section
specifications are given, then all sections of the PODs are used.

=begin _NOT_IMPLEMENTED_

=item B<-ranges>

A reference to an array of range specifications (as described in
L<"RANGE SPECIFICATIONS">) which indicate the desired range of POD
paragraphs to be selected from the desired input sections. If no range
specifications are given, then all paragraphs of the desired sections
are used.

=end _NOT_IMPLEMENTED_

=back

All other arguments should correspond to the names of input files
containing POD sections. A file name of "-" or "<&STDIN" will
be interpreted to mean standard input (which is the default if no
filenames are given).

=cut 

sub podselect {
    my(@argv) = @_;
    my %defaults = ();
    my $pod_parser = new Pod::Select(%defaults);
    my $num_inputs = 0;
    my $output = ">&STDOUT";
    my %opts;
    local $_;
    for (@argv) {
        if (ref($_)) {
        next unless (ref($_) eq 'HASH');
            %opts = (%defaults, %{$_});

            ##-------------------------------------------------------------
            ## Need this for backward compatibility since we formerly used
            ## options that were all uppercase words rather than ones that
            ## looked like Unix command-line options.
            ## to be uppercase keywords)
            ##-------------------------------------------------------------
            %opts = map {
                my ($key, $val) = (lc $_, $opts{$_});
                $key =~ s/^(?=\w)/-/;
                $key =~ /^-se[cl]/  and  $key  = '-sections';
                #! $key eq '-range'    and  $key .= 's';
                ($key => $val);    
            } (keys %opts);

            ## Process the options
            (exists $opts{'-output'})  and  $output = $opts{'-output'};

            ## Select the desired sections
            $pod_parser->select(@{ $opts{'-sections'} })
                if ( (defined $opts{'-sections'})
                     && ((ref $opts{'-sections'}) eq 'ARRAY') );

            #! ## Select the desired paragraph ranges
            #! $pod_parser->select(@{ $opts{'-ranges'} })
            #!     if ( (defined $opts{'-ranges'})
            #!          && ((ref $opts{'-ranges'}) eq 'ARRAY') );
        }
        else {
            $pod_parser->parse_from_file($_, $output);
            ++$num_inputs;
        }
    }
    $pod_parser->parse_from_file("-")  unless ($num_inputs > 0);
}

#############################################################################

=head1 PRIVATE METHODS AND DATA

B<Pod::Select> makes uses a number of internal methods and data fields
which clients should not need to see or use. For the sake of avoiding
name collisions with client data and methods, these methods and fields
are briefly discussed here. Determined hackers may obtain further
information about them by reading the B<Pod::Select> source code.

Private data fields are stored in the hash-object whose reference is
returned by the B<new()> constructor for this class. The names of all
private methods and data-fields used by B<Pod::Select> begin with a
prefix of "_" and match the regular expression C</^_\w+$/>.

=cut

##---------------------------------------------------------------------------

=begin _PRIVATE_

=head1 B<_compile_section_spec()>

            $listref = $parser->_compile_section_spec($section_spec);

This function (note it is a function and I<not> a method) takes a
section specification (as described in L<"SECTION SPECIFICATIONS">)
given in C<$section_sepc>, and compiles it into a list of regular
expressions. If C<$section_spec> has no syntax errors, then a reference
to the list (array) of corresponding regular expressions is returned;
otherwise C<undef> is returned and an error message is printed (using
B<carp>) for each invalid regex.

=end _PRIVATE_

=cut

sub _compile_section_spec {
    my ($section_spec) = @_;
    my (@regexs, $negated);

    ## Compile the spec into a list of regexs
    local $_ = $section_spec;
    s|\\\\|\001|g;  ## handle escaped backward slashes
    s|\\/|\002|g;   ## handle escaped forward slashes

    ## Parse the regexs for the heading titles
    @regexs = split('/', $_, $MAX_HEADING_LEVEL);

    ## Set default regex for ommitted levels
    for (my $i = 0; $i < $MAX_HEADING_LEVEL; ++$i) {
        $regexs[$i]  = '.*'  unless ((defined $regexs[$i])
                                     && (length $regexs[$i]));
    }
    ## Modify the regexs as needed and validate their syntax
    my $bad_regexs = 0;
    for (@regexs) {
        $_ .= '.+'  if ($_ eq '!');
        s|\001|\\\\|g;       ## restore escaped backward slashes
        s|\002|\\/|g;        ## restore escaped forward slashes
        $negated = s/^\!//;  ## check for negation
        eval "/$_/";         ## check regex syntax
        if ($@) {
            ++$bad_regexs;
            carp "Bad regular expression /$_/ in \"$section_spec\": $@\n";
        }
        else {
            ## Add the forward and rear anchors (and put the negator back)
            $_ = '^' . $_  unless (/^\^/);
            $_ = $_ . '$'  unless (/\$$/);
            $_ = '!' . $_  if ($negated);
        }
    }
    return  (! $bad_regexs) ? [ @regexs ] : undef;
}

##---------------------------------------------------------------------------

=begin _PRIVATE_

=head2 $self->{_SECTION_HEADINGS}

A reference to an array of the current section heading titles for each
heading level (note that the first heading level title is at index 0).

=end _PRIVATE_

=cut

##---------------------------------------------------------------------------

=begin _PRIVATE_

=head2 $self->{_SELECTED_SECTIONS}

A reference to an array of references to arrays. Each subarray is a list
of anchored regular expressions (preceded by a "!" if the expression is to
be negated). The index of the expression in the subarray should correspond
to the index of the heading title in C<$self-E<gt>{_SECTION_HEADINGS}>
that it is to be matched against.

=end _PRIVATE_

=cut

#############################################################################

=head1 SEE ALSO

L<Pod::Parser>

=head1 AUTHOR

Please report bugs using L<http://rt.cpan.org>.

Brad Appleton E<lt>bradapp@enteract.comE<gt>

Based on code for B<pod2text> written by
Tom Christiansen E<lt>tchrist@mox.perl.comE<gt>

=cut

1;
# vim: ts=4 sw=4 et
