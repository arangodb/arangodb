#############################################################################
# Pod/Parser.pm -- package which defines a base class for parsing POD docs.
#
# Copyright (C) 1996-2000 by Bradford Appleton. All rights reserved.
# This file is part of "PodParser". PodParser is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

package Pod::Parser;

use vars qw($VERSION);
$VERSION = 1.35;  ## Current version of this package
require  5.005;    ## requires this Perl version or later

#############################################################################

=head1 NAME

Pod::Parser - base class for creating POD filters and translators

=head1 SYNOPSIS

    use Pod::Parser;

    package MyParser;
    @ISA = qw(Pod::Parser);

    sub command { 
        my ($parser, $command, $paragraph, $line_num) = @_;
        ## Interpret the command and its text; sample actions might be:
        if ($command eq 'head1') { ... }
        elsif ($command eq 'head2') { ... }
        ## ... other commands and their actions
        my $out_fh = $parser->output_handle();
        my $expansion = $parser->interpolate($paragraph, $line_num);
        print $out_fh $expansion;
    }

    sub verbatim { 
        my ($parser, $paragraph, $line_num) = @_;
        ## Format verbatim paragraph; sample actions might be:
        my $out_fh = $parser->output_handle();
        print $out_fh $paragraph;
    }

    sub textblock { 
        my ($parser, $paragraph, $line_num) = @_;
        ## Translate/Format this block of text; sample actions might be:
        my $out_fh = $parser->output_handle();
        my $expansion = $parser->interpolate($paragraph, $line_num);
        print $out_fh $expansion;
    }

    sub interior_sequence { 
        my ($parser, $seq_command, $seq_argument) = @_;
        ## Expand an interior sequence; sample actions might be:
        return "*$seq_argument*"     if ($seq_command eq 'B');
        return "`$seq_argument'"     if ($seq_command eq 'C');
        return "_${seq_argument}_'"  if ($seq_command eq 'I');
        ## ... other sequence commands and their resulting text
    }

    package main;

    ## Create a parser object and have it parse file whose name was
    ## given on the command-line (use STDIN if no files were given).
    $parser = new MyParser();
    $parser->parse_from_filehandle(\*STDIN)  if (@ARGV == 0);
    for (@ARGV) { $parser->parse_from_file($_); }

=head1 REQUIRES

perl5.005, Pod::InputObjects, Exporter, Symbol, Carp

=head1 EXPORTS

Nothing.

=head1 DESCRIPTION

B<Pod::Parser> is a base class for creating POD filters and translators.
It handles most of the effort involved with parsing the POD sections
from an input stream, leaving subclasses free to be concerned only with
performing the actual translation of text.

B<Pod::Parser> parses PODs, and makes method calls to handle the various
components of the POD. Subclasses of B<Pod::Parser> override these methods
to translate the POD into whatever output format they desire.

=head1 QUICK OVERVIEW

To create a POD filter for translating POD documentation into some other
format, you create a subclass of B<Pod::Parser> which typically overrides
just the base class implementation for the following methods:

=over 2

=item *

B<command()>

=item *

B<verbatim()>

=item *

B<textblock()>

=item *

B<interior_sequence()>

=back

You may also want to override the B<begin_input()> and B<end_input()>
methods for your subclass (to perform any needed per-file and/or
per-document initialization or cleanup).

If you need to perform any preprocesssing of input before it is parsed
you may want to override one or more of B<preprocess_line()> and/or
B<preprocess_paragraph()>.

Sometimes it may be necessary to make more than one pass over the input
files. If this is the case you have several options. You can make the
first pass using B<Pod::Parser> and override your methods to store the
intermediate results in memory somewhere for the B<end_pod()> method to
process. You could use B<Pod::Parser> for several passes with an
appropriate state variable to control the operation for each pass. If
your input source can't be reset to start at the beginning, you can
store it in some other structure as a string or an array and have that
structure implement a B<getline()> method (which is all that
B<parse_from_filehandle()> uses to read input).

Feel free to add any member data fields you need to keep track of things
like current font, indentation, horizontal or vertical position, or
whatever else you like. Be sure to read L<"PRIVATE METHODS AND DATA">
to avoid name collisions.

For the most part, the B<Pod::Parser> base class should be able to
do most of the input parsing for you and leave you free to worry about
how to interpret the commands and translate the result.

Note that all we have described here in this quick overview is the
simplest most straightforward use of B<Pod::Parser> to do stream-based
parsing. It is also possible to use the B<Pod::Parser::parse_text> function
to do more sophisticated tree-based parsing. See L<"TREE-BASED PARSING">.

=head1 PARSING OPTIONS

A I<parse-option> is simply a named option of B<Pod::Parser> with a
value that corresponds to a certain specified behavior. These various
behaviors of B<Pod::Parser> may be enabled/disabled by setting
or unsetting one or more I<parse-options> using the B<parseopts()> method.
The set of currently accepted parse-options is as follows:

=over 3

=item B<-want_nonPODs> (default: unset)

Normally (by default) B<Pod::Parser> will only provide access to
the POD sections of the input. Input paragraphs that are not part
of the POD-format documentation are not made available to the caller
(not even using B<preprocess_paragraph()>). Setting this option to a
non-empty, non-zero value will allow B<preprocess_paragraph()> to see
non-POD sections of the input as well as POD sections. The B<cutting()>
method can be used to determine if the corresponding paragraph is a POD
paragraph, or some other input paragraph.

=item B<-process_cut_cmd> (default: unset)

Normally (by default) B<Pod::Parser> handles the C<=cut> POD directive
by itself and does not pass it on to the caller for processing. Setting
this option to a non-empty, non-zero value will cause B<Pod::Parser> to
pass the C<=cut> directive to the caller just like any other POD command
(and hence it may be processed by the B<command()> method).

B<Pod::Parser> will still interpret the C<=cut> directive to mean that
"cutting mode" has been (re)entered, but the caller will get a chance
to capture the actual C<=cut> paragraph itself for whatever purpose
it desires.

=item B<-warnings> (default: unset)

Normally (by default) B<Pod::Parser> recognizes a bare minimum of
pod syntax errors and warnings and issues diagnostic messages
for errors, but not for warnings. (Use B<Pod::Checker> to do more
thorough checking of POD syntax.) Setting this option to a non-empty,
non-zero value will cause B<Pod::Parser> to issue diagnostics for
the few warnings it recognizes as well as the errors.

=back

Please see L<"parseopts()"> for a complete description of the interface
for the setting and unsetting of parse-options.

=cut

#############################################################################

use vars qw(@ISA);
use strict;
#use diagnostics;
use Pod::InputObjects;
use Carp;
use Exporter;
BEGIN {
   if ($] < 5.6) {
      require Symbol;
      import Symbol;
   }
}
@ISA = qw(Exporter);

## These "variables" are used as local "glob aliases" for performance
use vars qw(%myData %myOpts @input_stack);

#############################################################################

=head1 RECOMMENDED SUBROUTINE/METHOD OVERRIDES

B<Pod::Parser> provides several methods which most subclasses will probably
want to override. These methods are as follows:

=cut

##---------------------------------------------------------------------------

=head1 B<command()>

            $parser->command($cmd,$text,$line_num,$pod_para);

This method should be overridden by subclasses to take the appropriate
action when a POD command paragraph (denoted by a line beginning with
"=") is encountered. When such a POD directive is seen in the input,
this method is called and is passed:

=over 3

=item C<$cmd>

the name of the command for this POD paragraph

=item C<$text>

the paragraph text for the given POD paragraph command.

=item C<$line_num>

the line-number of the beginning of the paragraph

=item C<$pod_para>

a reference to a C<Pod::Paragraph> object which contains further
information about the paragraph command (see L<Pod::InputObjects>
for details).

=back

B<Note> that this method I<is> called for C<=pod> paragraphs.

The base class implementation of this method simply treats the raw POD
command as normal block of paragraph text (invoking the B<textblock()>
method with the command paragraph).

=cut

sub command {
    my ($self, $cmd, $text, $line_num, $pod_para)  = @_;
    ## Just treat this like a textblock
    $self->textblock($pod_para->raw_text(), $line_num, $pod_para);
}

##---------------------------------------------------------------------------

=head1 B<verbatim()>

            $parser->verbatim($text,$line_num,$pod_para);

This method may be overridden by subclasses to take the appropriate
action when a block of verbatim text is encountered. It is passed the
following parameters:

=over 3

=item C<$text>

the block of text for the verbatim paragraph

=item C<$line_num>

the line-number of the beginning of the paragraph

=item C<$pod_para>

a reference to a C<Pod::Paragraph> object which contains further
information about the paragraph (see L<Pod::InputObjects>
for details).

=back

The base class implementation of this method simply prints the textblock
(unmodified) to the output filehandle.

=cut

sub verbatim {
    my ($self, $text, $line_num, $pod_para) = @_;
    my $out_fh = $self->{_OUTPUT};
    print $out_fh $text;
}

##---------------------------------------------------------------------------

=head1 B<textblock()>

            $parser->textblock($text,$line_num,$pod_para);

This method may be overridden by subclasses to take the appropriate
action when a normal block of POD text is encountered (although the base
class method will usually do what you want). It is passed the following
parameters:

=over 3

=item C<$text>

the block of text for the a POD paragraph

=item C<$line_num>

the line-number of the beginning of the paragraph

=item C<$pod_para>

a reference to a C<Pod::Paragraph> object which contains further
information about the paragraph (see L<Pod::InputObjects>
for details).

=back

In order to process interior sequences, subclasses implementations of
this method will probably want to invoke either B<interpolate()> or
B<parse_text()>, passing it the text block C<$text>, and the corresponding
line number in C<$line_num>, and then perform any desired processing upon
the returned result.

The base class implementation of this method simply prints the text block
as it occurred in the input stream).

=cut

sub textblock {
    my ($self, $text, $line_num, $pod_para) = @_;
    my $out_fh = $self->{_OUTPUT};
    print $out_fh $self->interpolate($text, $line_num);
}

##---------------------------------------------------------------------------

=head1 B<interior_sequence()>

            $parser->interior_sequence($seq_cmd,$seq_arg,$pod_seq);

This method should be overridden by subclasses to take the appropriate
action when an interior sequence is encountered. An interior sequence is
an embedded command within a block of text which appears as a command
name (usually a single uppercase character) followed immediately by a
string of text which is enclosed in angle brackets. This method is
passed the sequence command C<$seq_cmd> and the corresponding text
C<$seq_arg>. It is invoked by the B<interpolate()> method for each interior
sequence that occurs in the string that it is passed. It should return
the desired text string to be used in place of the interior sequence.
The C<$pod_seq> argument is a reference to a C<Pod::InteriorSequence>
object which contains further information about the interior sequence.
Please see L<Pod::InputObjects> for details if you need to access this
additional information.

Subclass implementations of this method may wish to invoke the 
B<nested()> method of C<$pod_seq> to see if it is nested inside
some other interior-sequence (and if so, which kind).

The base class implementation of the B<interior_sequence()> method
simply returns the raw text of the interior sequence (as it occurred
in the input) to the caller.

=cut

sub interior_sequence {
    my ($self, $seq_cmd, $seq_arg, $pod_seq) = @_;
    ## Just return the raw text of the interior sequence
    return  $pod_seq->raw_text();
}

#############################################################################

=head1 OPTIONAL SUBROUTINE/METHOD OVERRIDES

B<Pod::Parser> provides several methods which subclasses may want to override
to perform any special pre/post-processing. These methods do I<not> have to
be overridden, but it may be useful for subclasses to take advantage of them.

=cut

##---------------------------------------------------------------------------

=head1 B<new()>

            my $parser = Pod::Parser->new();

This is the constructor for B<Pod::Parser> and its subclasses. You
I<do not> need to override this method! It is capable of constructing
subclass objects as well as base class objects, provided you use
any of the following constructor invocation styles:

    my $parser1 = MyParser->new();
    my $parser2 = new MyParser();
    my $parser3 = $parser2->new();

where C<MyParser> is some subclass of B<Pod::Parser>.

Using the syntax C<MyParser::new()> to invoke the constructor is I<not>
recommended, but if you insist on being able to do this, then the
subclass I<will> need to override the B<new()> constructor method. If
you do override the constructor, you I<must> be sure to invoke the
B<initialize()> method of the newly blessed object.

Using any of the above invocations, the first argument to the
constructor is always the corresponding package name (or object
reference). No other arguments are required, but if desired, an
associative array (or hash-table) my be passed to the B<new()>
constructor, as in:

    my $parser1 = MyParser->new( MYDATA => $value1, MOREDATA => $value2 );
    my $parser2 = new MyParser( -myflag => 1 );

All arguments passed to the B<new()> constructor will be treated as
key/value pairs in a hash-table. The newly constructed object will be
initialized by copying the contents of the given hash-table (which may
have been empty). The B<new()> constructor for this class and all of its
subclasses returns a blessed reference to the initialized object (hash-table).

=cut

sub new {
    ## Determine if we were called via an object-ref or a classname
    my $this = shift;
    my $class = ref($this) || $this;
    ## Any remaining arguments are treated as initial values for the
    ## hash that is used to represent this object.
    my %params = @_;
    my $self = { %params };
    ## Bless ourselves into the desired class and perform any initialization
    bless $self, $class;
    $self->initialize();
    return $self;
}

##---------------------------------------------------------------------------

=head1 B<initialize()>

            $parser->initialize();

This method performs any necessary object initialization. It takes no
arguments (other than the object instance of course, which is typically
copied to a local variable named C<$self>). If subclasses override this
method then they I<must> be sure to invoke C<$self-E<gt>SUPER::initialize()>.

=cut

sub initialize {
    #my $self = shift;
    #return;
}

##---------------------------------------------------------------------------

=head1 B<begin_pod()>

            $parser->begin_pod();

This method is invoked at the beginning of processing for each POD
document that is encountered in the input. Subclasses should override
this method to perform any per-document initialization.

=cut

sub begin_pod {
    #my $self = shift;
    #return;
}

##---------------------------------------------------------------------------

=head1 B<begin_input()>

            $parser->begin_input();

This method is invoked by B<parse_from_filehandle()> immediately I<before>
processing input from a filehandle. The base class implementation does
nothing, however, subclasses may override it to perform any per-file
initializations.

Note that if multiple files are parsed for a single POD document
(perhaps the result of some future C<=include> directive) this method
is invoked for every file that is parsed. If you wish to perform certain
initializations once per document, then you should use B<begin_pod()>.

=cut

sub begin_input {
    #my $self = shift;
    #return;
}

##---------------------------------------------------------------------------

=head1 B<end_input()>

            $parser->end_input();

This method is invoked by B<parse_from_filehandle()> immediately I<after>
processing input from a filehandle. The base class implementation does
nothing, however, subclasses may override it to perform any per-file
cleanup actions.

Please note that if multiple files are parsed for a single POD document
(perhaps the result of some kind of C<=include> directive) this method
is invoked for every file that is parsed. If you wish to perform certain
cleanup actions once per document, then you should use B<end_pod()>.

=cut

sub end_input {
    #my $self = shift;
    #return;
}

##---------------------------------------------------------------------------

=head1 B<end_pod()>

            $parser->end_pod();

This method is invoked at the end of processing for each POD document
that is encountered in the input. Subclasses should override this method
to perform any per-document finalization.

=cut

sub end_pod {
    #my $self = shift;
    #return;
}

##---------------------------------------------------------------------------

=head1 B<preprocess_line()>

          $textline = $parser->preprocess_line($text, $line_num);

This method should be overridden by subclasses that wish to perform
any kind of preprocessing for each I<line> of input (I<before> it has
been determined whether or not it is part of a POD paragraph). The
parameter C<$text> is the input line; and the parameter C<$line_num> is
the line number of the corresponding text line.

The value returned should correspond to the new text to use in its
place.  If the empty string or an undefined value is returned then no
further processing will be performed for this line.

Please note that the B<preprocess_line()> method is invoked I<before>
the B<preprocess_paragraph()> method. After all (possibly preprocessed)
lines in a paragraph have been assembled together and it has been
determined that the paragraph is part of the POD documentation from one
of the selected sections, then B<preprocess_paragraph()> is invoked.

The base class implementation of this method returns the given text.

=cut

sub preprocess_line {
    my ($self, $text, $line_num) = @_;
    return  $text;
}

##---------------------------------------------------------------------------

=head1 B<preprocess_paragraph()>

            $textblock = $parser->preprocess_paragraph($text, $line_num);

This method should be overridden by subclasses that wish to perform any
kind of preprocessing for each block (paragraph) of POD documentation
that appears in the input stream. The parameter C<$text> is the POD
paragraph from the input file; and the parameter C<$line_num> is the
line number for the beginning of the corresponding paragraph.

The value returned should correspond to the new text to use in its
place If the empty string is returned or an undefined value is
returned, then the given C<$text> is ignored (not processed).

This method is invoked after gathering up all the lines in a paragraph
and after determining the cutting state of the paragraph,
but before trying to further parse or interpret them. After
B<preprocess_paragraph()> returns, the current cutting state (which
is returned by C<$self-E<gt>cutting()>) is examined. If it evaluates
to true then input text (including the given C<$text>) is cut (not
processed) until the next POD directive is encountered.

Please note that the B<preprocess_line()> method is invoked I<before>
the B<preprocess_paragraph()> method. After all (possibly preprocessed)
lines in a paragraph have been assembled together and either it has been
determined that the paragraph is part of the POD documentation from one
of the selected sections or the C<-want_nonPODs> option is true,
then B<preprocess_paragraph()> is invoked.

The base class implementation of this method returns the given text.

=cut

sub preprocess_paragraph {
    my ($self, $text, $line_num) = @_;
    return  $text;
}

#############################################################################

=head1 METHODS FOR PARSING AND PROCESSING

B<Pod::Parser> provides several methods to process input text. These
methods typically won't need to be overridden (and in some cases they
can't be overridden), but subclasses may want to invoke them to exploit
their functionality.

=cut

##---------------------------------------------------------------------------

=head1 B<parse_text()>

            $ptree1 = $parser->parse_text($text, $line_num);
            $ptree2 = $parser->parse_text({%opts}, $text, $line_num);
            $ptree3 = $parser->parse_text(\%opts, $text, $line_num);

This method is useful if you need to perform your own interpolation 
of interior sequences and can't rely upon B<interpolate> to expand
them in simple bottom-up order.

The parameter C<$text> is a string or block of text to be parsed
for interior sequences; and the parameter C<$line_num> is the
line number corresponding to the beginning of C<$text>.

B<parse_text()> will parse the given text into a parse-tree of "nodes."
and interior-sequences.  Each "node" in the parse tree is either a
text-string, or a B<Pod::InteriorSequence>.  The result returned is a
parse-tree of type B<Pod::ParseTree>. Please see L<Pod::InputObjects>
for more information about B<Pod::InteriorSequence> and B<Pod::ParseTree>.

If desired, an optional hash-ref may be specified as the first argument
to customize certain aspects of the parse-tree that is created and
returned. The set of recognized option keywords are:

=over 3

=item B<-expand_seq> =E<gt> I<code-ref>|I<method-name>

Normally, the parse-tree returned by B<parse_text()> will contain an
unexpanded C<Pod::InteriorSequence> object for each interior-sequence
encountered. Specifying B<-expand_seq> tells B<parse_text()> to "expand"
every interior-sequence it sees by invoking the referenced function
(or named method of the parser object) and using the return value as the
expanded result.

If a subroutine reference was given, it is invoked as:

  &$code_ref( $parser, $sequence )

and if a method-name was given, it is invoked as:

  $parser->method_name( $sequence )

where C<$parser> is a reference to the parser object, and C<$sequence>
is a reference to the interior-sequence object.
[I<NOTE>: If the B<interior_sequence()> method is specified, then it is
invoked according to the interface specified in L<"interior_sequence()">].

=item B<-expand_text> =E<gt> I<code-ref>|I<method-name>

Normally, the parse-tree returned by B<parse_text()> will contain a
text-string for each contiguous sequence of characters outside of an
interior-sequence. Specifying B<-expand_text> tells B<parse_text()> to
"preprocess" every such text-string it sees by invoking the referenced
function (or named method of the parser object) and using the return value
as the preprocessed (or "expanded") result. [Note that if the result is
an interior-sequence, then it will I<not> be expanded as specified by the
B<-expand_seq> option; Any such recursive expansion needs to be handled by
the specified callback routine.]

If a subroutine reference was given, it is invoked as:

  &$code_ref( $parser, $text, $ptree_node )

and if a method-name was given, it is invoked as:

  $parser->method_name( $text, $ptree_node )

where C<$parser> is a reference to the parser object, C<$text> is the
text-string encountered, and C<$ptree_node> is a reference to the current
node in the parse-tree (usually an interior-sequence object or else the
top-level node of the parse-tree).

=item B<-expand_ptree> =E<gt> I<code-ref>|I<method-name>

Rather than returning a C<Pod::ParseTree>, pass the parse-tree as an
argument to the referenced subroutine (or named method of the parser
object) and return the result instead of the parse-tree object.

If a subroutine reference was given, it is invoked as:

  &$code_ref( $parser, $ptree )

and if a method-name was given, it is invoked as:

  $parser->method_name( $ptree )

where C<$parser> is a reference to the parser object, and C<$ptree>
is a reference to the parse-tree object.

=back

=cut

sub parse_text {
    my $self = shift;
    local $_ = '';

    ## Get options and set any defaults
    my %opts = (ref $_[0]) ? %{ shift() } : ();
    my $expand_seq   = $opts{'-expand_seq'}   || undef;
    my $expand_text  = $opts{'-expand_text'}  || undef;
    my $expand_ptree = $opts{'-expand_ptree'} || undef;

    my $text = shift;
    my $line = shift;
    my $file = $self->input_file();
    my $cmd  = "";

    ## Convert method calls into closures, for our convenience
    my $xseq_sub   = $expand_seq;
    my $xtext_sub  = $expand_text;
    my $xptree_sub = $expand_ptree;
    if (defined $expand_seq  and  $expand_seq eq 'interior_sequence') {
        ## If 'interior_sequence' is the method to use, we have to pass
        ## more than just the sequence object, we also need to pass the
        ## sequence name and text.
        $xseq_sub = sub {
            my ($self, $iseq) = @_;
            my $args = join("", $iseq->parse_tree->children);
            return  $self->interior_sequence($iseq->name, $args, $iseq);
        };
    }
    ref $xseq_sub    or  $xseq_sub   = sub { shift()->$expand_seq(@_) };
    ref $xtext_sub   or  $xtext_sub  = sub { shift()->$expand_text(@_) };
    ref $xptree_sub  or  $xptree_sub = sub { shift()->$expand_ptree(@_) };

    ## Keep track of the "current" interior sequence, and maintain a stack
    ## of "in progress" sequences.
    ##
    ## NOTE that we push our own "accumulator" at the very beginning of the
    ## stack. It's really a parse-tree, not a sequence; but it implements
    ## the methods we need so we can use it to gather-up all the sequences
    ## and strings we parse. Thus, by the end of our parsing, it should be
    ## the only thing left on our stack and all we have to do is return it!
    ##
    my $seq       = Pod::ParseTree->new();
    my @seq_stack = ($seq);
    my ($ldelim, $rdelim) = ('', '');

    ## Iterate over all sequence starts text (NOTE: split with
    ## capturing parens keeps the delimiters)
    $_ = $text;
    my @tokens = split /([A-Z]<(?:<+\s)?)/;
    while ( @tokens ) {
        $_ = shift @tokens;
        ## Look for the beginning of a sequence
        if ( /^([A-Z])(<(?:<+\s)?)$/ ) {
            ## Push a new sequence onto the stack of those "in-progress"
            my $ldelim_orig;
            ($cmd, $ldelim_orig) = ($1, $2);
            ($ldelim = $ldelim_orig) =~ s/\s+$//;
            ($rdelim = $ldelim) =~ tr/</>/;
            $seq = Pod::InteriorSequence->new(
                       -name   => $cmd,
                       -ldelim => $ldelim_orig,  -rdelim => $rdelim,
                       -file   => $file,    -line   => $line
                   );
            (@seq_stack > 1)  and  $seq->nested($seq_stack[-1]);
            push @seq_stack, $seq;
        }
        ## Look for sequence ending
        elsif ( @seq_stack > 1 ) {
            ## Make sure we match the right kind of closing delimiter
            my ($seq_end, $post_seq) = ("", "");
            if ( ($ldelim eq '<'   and  /\A(.*?)(>)/s)
                 or  /\A(.*?)(\s+$rdelim)/s )
            {
                ## Found end-of-sequence, capture the interior and the
                ## closing the delimiter, and put the rest back on the
                ## token-list
                $post_seq = substr($_, length($1) + length($2));
                ($_, $seq_end) = ($1, $2);
                (length $post_seq)  and  unshift @tokens, $post_seq;
            }
            if (length) {
                ## In the middle of a sequence, append this text to it, and
                ## dont forget to "expand" it if that's what the caller wanted
                $seq->append($expand_text ? &$xtext_sub($self,$_,$seq) : $_);
                $_ .= $seq_end;
            }
            if (length $seq_end) {
                ## End of current sequence, record terminating delimiter
                $seq->rdelim($seq_end);
                ## Pop it off the stack of "in progress" sequences
                pop @seq_stack;
                ## Append result to its parent in current parse tree
                $seq_stack[-1]->append($expand_seq ? &$xseq_sub($self,$seq)
                                                   : $seq);
                ## Remember the current cmd-name and left-delimiter
                if(@seq_stack > 1) {
                    $cmd = $seq_stack[-1]->name;
                    $ldelim = $seq_stack[-1]->ldelim;
                    $rdelim = $seq_stack[-1]->rdelim;
                } else {
                    $cmd = $ldelim = $rdelim = '';
                }
            }
        }
        elsif (length) {
            ## In the middle of a sequence, append this text to it, and
            ## dont forget to "expand" it if that's what the caller wanted
            $seq->append($expand_text ? &$xtext_sub($self,$_,$seq) : $_);
        }
        ## Keep track of line count
        $line += s/\r*\n//;
        ## Remember the "current" sequence
        $seq = $seq_stack[-1];
    }

    ## Handle unterminated sequences
    my $errorsub = (@seq_stack > 1) ? $self->errorsub() : undef;
    while (@seq_stack > 1) {
       ($cmd, $file, $line) = ($seq->name, $seq->file_line);
       $ldelim  = $seq->ldelim;
       ($rdelim = $ldelim) =~ tr/</>/;
       $rdelim  =~ s/^(\S+)(\s*)$/$2$1/;
       pop @seq_stack;
       my $errmsg = "*** ERROR: unterminated ${cmd}${ldelim}...${rdelim}".
                    " at line $line in file $file\n";
       (ref $errorsub) and &{$errorsub}($errmsg)
           or (defined $errorsub) and $self->$errorsub($errmsg)
               or  warn($errmsg);
       $seq_stack[-1]->append($expand_seq ? &$xseq_sub($self,$seq) : $seq);
       $seq = $seq_stack[-1];
    }

    ## Return the resulting parse-tree
    my $ptree = (pop @seq_stack)->parse_tree;
    return  $expand_ptree ? &$xptree_sub($self, $ptree) : $ptree;
}

##---------------------------------------------------------------------------

=head1 B<interpolate()>

            $textblock = $parser->interpolate($text, $line_num);

This method translates all text (including any embedded interior sequences)
in the given text string C<$text> and returns the interpolated result. The
parameter C<$line_num> is the line number corresponding to the beginning
of C<$text>.

B<interpolate()> merely invokes a private method to recursively expand
nested interior sequences in bottom-up order (innermost sequences are
expanded first). If there is a need to expand nested sequences in
some alternate order, use B<parse_text> instead.

=cut

sub interpolate {
    my($self, $text, $line_num) = @_;
    my %parse_opts = ( -expand_seq => 'interior_sequence' );
    my $ptree = $self->parse_text( \%parse_opts, $text, $line_num );
    return  join "", $ptree->children();
}

##---------------------------------------------------------------------------

=begin __PRIVATE__

=head1 B<parse_paragraph()>

            $parser->parse_paragraph($text, $line_num);

This method takes the text of a POD paragraph to be processed, along
with its corresponding line number, and invokes the appropriate method
(one of B<command()>, B<verbatim()>, or B<textblock()>).

For performance reasons, this method is invoked directly without any
dynamic lookup; Hence subclasses may I<not> override it!

=end __PRIVATE__

=cut

sub parse_paragraph {
    my ($self, $text, $line_num) = @_;
    local *myData = $self;  ## alias to avoid deref-ing overhead
    local *myOpts = ($myData{_PARSEOPTS} ||= {});  ## get parse-options
    local $_;

    ## See if we want to preprocess nonPOD paragraphs as well as POD ones.
    my $wantNonPods = $myOpts{'-want_nonPODs'};

    ## Update cutting status
    $myData{_CUTTING} = 0 if $text =~ /^={1,2}\S/;

    ## Perform any desired preprocessing if we wanted it this early
    $wantNonPods  and  $text = $self->preprocess_paragraph($text, $line_num);

    ## Ignore up until next POD directive if we are cutting
    return if $myData{_CUTTING};

    ## Now we know this is block of text in a POD section!

    ##-----------------------------------------------------------------
    ## This is a hook (hack ;-) for Pod::Select to do its thing without
    ## having to override methods, but also without Pod::Parser assuming
    ## $self is an instance of Pod::Select (if the _SELECTED_SECTIONS
    ## field exists then we assume there is an is_selected() method for
    ## us to invoke (calling $self->can('is_selected') could verify this
    ## but that is more overhead than I want to incur)
    ##-----------------------------------------------------------------

    ## Ignore this block if it isnt in one of the selected sections
    if (exists $myData{_SELECTED_SECTIONS}) {
        $self->is_selected($text)  or  return ($myData{_CUTTING} = 1);
    }

    ## If we havent already, perform any desired preprocessing and
    ## then re-check the "cutting" state
    unless ($wantNonPods) {
       $text = $self->preprocess_paragraph($text, $line_num);
       return 1  unless ((defined $text) and (length $text));
       return 1  if ($myData{_CUTTING});
    }

    ## Look for one of the three types of paragraphs
    my ($pfx, $cmd, $arg, $sep) = ('', '', '', '');
    my $pod_para = undef;
    if ($text =~ /^(={1,2})(?=\S)/) {
        ## Looks like a command paragraph. Capture the command prefix used
        ## ("=" or "=="), as well as the command-name, its paragraph text,
        ## and whatever sequence of characters was used to separate them
        $pfx = $1;
        $_ = substr($text, length $pfx);
        ($cmd, $sep, $text) = split /(\s+)/, $_, 2; 
        ## If this is a "cut" directive then we dont need to do anything
        ## except return to "cutting" mode.
        if ($cmd eq 'cut') {
           $myData{_CUTTING} = 1;
           return  unless $myOpts{'-process_cut_cmd'};
        }
    }
    ## Save the attributes indicating how the command was specified.
    $pod_para = new Pod::Paragraph(
          -name      => $cmd,
          -text      => $text,
          -prefix    => $pfx,
          -separator => $sep,
          -file      => $myData{_INFILE},
          -line      => $line_num
    );
    # ## Invoke appropriate callbacks
    # if (exists $myData{_CALLBACKS}) {
    #    ## Look through the callback list, invoke callbacks,
    #    ## then see if we need to do the default actions
    #    ## (invoke_callbacks will return true if we do).
    #    return  1  unless $self->invoke_callbacks($cmd, $text, $line_num, $pod_para);
    # }
    if (length $cmd) {
        ## A command paragraph
        $self->command($cmd, $text, $line_num, $pod_para);
    }
    elsif ($text =~ /^\s+/) {
        ## Indented text - must be a verbatim paragraph
        $self->verbatim($text, $line_num, $pod_para);
    }
    else {
        ## Looks like an ordinary block of text
        $self->textblock($text, $line_num, $pod_para);
    }
    return  1;
}

##---------------------------------------------------------------------------

=head1 B<parse_from_filehandle()>

            $parser->parse_from_filehandle($in_fh,$out_fh);

This method takes an input filehandle (which is assumed to already be
opened for reading) and reads the entire input stream looking for blocks
(paragraphs) of POD documentation to be processed. If no first argument
is given the default input filehandle C<STDIN> is used.

The C<$in_fh> parameter may be any object that provides a B<getline()>
method to retrieve a single line of input text (hence, an appropriate
wrapper object could be used to parse PODs from a single string or an
array of strings).

Using C<$in_fh-E<gt>getline()>, input is read line-by-line and assembled
into paragraphs or "blocks" (which are separated by lines containing
nothing but whitespace). For each block of POD documentation
encountered it will invoke a method to parse the given paragraph.

If a second argument is given then it should correspond to a filehandle where
output should be sent (otherwise the default output filehandle is
C<STDOUT> if no output filehandle is currently in use).

B<NOTE:> For performance reasons, this method caches the input stream at
the top of the stack in a local variable. Any attempts by clients to
change the stack contents during processing when in the midst executing
of this method I<will not affect> the input stream used by the current
invocation of this method.

This method does I<not> usually need to be overridden by subclasses.

=cut

sub parse_from_filehandle {
    my $self = shift;
    my %opts = (ref $_[0] eq 'HASH') ? %{ shift() } : ();
    my ($in_fh, $out_fh) = @_;
    $in_fh = \*STDIN  unless ($in_fh);
    local *myData = $self;  ## alias to avoid deref-ing overhead
    local *myOpts = ($myData{_PARSEOPTS} ||= {});  ## get parse-options
    local $_;

    ## Put this stream at the top of the stack and do beginning-of-input
    ## processing. NOTE that $in_fh might be reset during this process.
    my $topstream = $self->_push_input_stream($in_fh, $out_fh);
    (exists $opts{-cutting})  and  $self->cutting( $opts{-cutting} );

    ## Initialize line/paragraph
    my ($textline, $paragraph) = ('', '');
    my ($nlines, $plines) = (0, 0);

    ## Use <$fh> instead of $fh->getline where possible (for speed)
    $_ = ref $in_fh;
    my $tied_fh = (/^(?:GLOB|FileHandle|IO::\w+)$/  or  tied $in_fh);

    ## Read paragraphs line-by-line
    while (defined ($textline = $tied_fh ? <$in_fh> : $in_fh->getline)) {
        $textline = $self->preprocess_line($textline, ++$nlines);
        next  unless ((defined $textline)  &&  (length $textline));

        if ((! length $paragraph) && ($textline =~ /^==/)) {
            ## '==' denotes a one-line command paragraph
            $paragraph = $textline;
            $plines    = 1;
            $textline  = '';
        } else {
            ## Append this line to the current paragraph
            $paragraph .= $textline;
            ++$plines;
        }

        ## See if this line is blank and ends the current paragraph.
        ## If it isnt, then keep iterating until it is.
        next unless (($textline =~ /^([^\S\r\n]*)[\r\n]*$/)
                                     && (length $paragraph));

        ## Issue a warning about any non-empty blank lines
        if (length($1) > 0 and $myOpts{'-warnings'} and ! $myData{_CUTTING}) {
            my $errorsub = $self->errorsub();
            my $file = $self->input_file();
            my $errmsg = "*** WARNING: line containing nothing but whitespace".
                         " in paragraph at line $nlines in file $file\n";
            (ref $errorsub) and &{$errorsub}($errmsg)
                or (defined $errorsub) and $self->$errorsub($errmsg)
                    or  warn($errmsg);
        }

        ## Now process the paragraph
        parse_paragraph($self, $paragraph, ($nlines - $plines) + 1);
        $paragraph = '';
        $plines = 0;
    }
    ## Dont forget about the last paragraph in the file
    if (length $paragraph) {
       parse_paragraph($self, $paragraph, ($nlines - $plines) + 1)
    }

    ## Now pop the input stream off the top of the input stack.
    $self->_pop_input_stream();
}

##---------------------------------------------------------------------------

=head1 B<parse_from_file()>

            $parser->parse_from_file($filename,$outfile);

This method takes a filename and does the following:

=over 2

=item *

opens the input and output files for reading
(creating the appropriate filehandles)

=item *

invokes the B<parse_from_filehandle()> method passing it the
corresponding input and output filehandles.

=item *

closes the input and output files.

=back

If the special input filename "-" or "<&STDIN" is given then the STDIN
filehandle is used for input (and no open or close is performed). If no
input filename is specified then "-" is implied.

If a second argument is given then it should be the name of the desired
output file. If the special output filename "-" or ">&STDOUT" is given
then the STDOUT filehandle is used for output (and no open or close is
performed). If the special output filename ">&STDERR" is given then the
STDERR filehandle is used for output (and no open or close is
performed). If no output filehandle is currently in use and no output
filename is specified, then "-" is implied.
Alternatively, an L<IO::String> object is also accepted as an output
file handle.

This method does I<not> usually need to be overridden by subclasses.

=cut

sub parse_from_file {
    my $self = shift;
    my %opts = (ref $_[0] eq 'HASH') ? %{ shift() } : ();
    my ($infile, $outfile) = @_;
    my ($in_fh,  $out_fh);
    if ($] < 5.006) {
      ($in_fh,  $out_fh) = (gensym(), gensym());
    }
    my ($close_input, $close_output) = (0, 0);
    local *myData = $self;
    local *_;

    ## Is $infile a filename or a (possibly implied) filehandle
    if (defined $infile && ref $infile) {
        if (ref($infile) =~ /^(SCALAR|ARRAY|HASH|CODE|REF)$/) {
            croak "Input from $1 reference not supported!\n";
        }
        ## Must be a filehandle-ref (or else assume its a ref to an object
        ## that supports the common IO read operations).
        $myData{_INFILE} = ${$infile};
        $in_fh = $infile;
    }
    elsif (!defined($infile) || !length($infile) || ($infile eq '-')
        || ($infile =~ /^<&(?:STDIN|0)$/i))
    {
        ## Not a filename, just a string implying STDIN
        $infile ||= '-';
        $myData{_INFILE} = "<standard input>";
        $in_fh = \*STDIN;
    }
    else {
        ## We have a filename, open it for reading
        $myData{_INFILE} = $infile;
        open($in_fh, "< $infile")  or
             croak "Can't open $infile for reading: $!\n";
        $close_input = 1;
    }

    ## NOTE: we need to be *very* careful when "defaulting" the output
    ## file. We only want to use a default if this is the beginning of
    ## the entire document (but *not* if this is an included file). We
    ## determine this by seeing if the input stream stack has been set-up
    ## already

    ## Is $outfile a filename, a (possibly implied) filehandle, maybe a ref?
    if (ref $outfile) {
        ## we need to check for ref() first, as other checks involve reading
        if (ref($outfile) =~ /^(ARRAY|HASH|CODE)$/) {
            croak "Output to $1 reference not supported!\n";
        }
        elsif (ref($outfile) eq 'SCALAR') {
#           # NOTE: IO::String isn't a part of the perl distribution,
#           #       so probably we shouldn't support this case...
#           require IO::String;
#           $myData{_OUTFILE} = "$outfile";
#           $out_fh = IO::String->new($outfile);
            croak "Output to SCALAR reference not supported!\n";
        }
        else {
            ## Must be a filehandle-ref (or else assume its a ref to an
            ## object that supports the common IO write operations).
            $myData{_OUTFILE} = ${$outfile};
            $out_fh = $outfile;
        }
    }
    elsif (!defined($outfile) || !length($outfile) || ($outfile eq '-')
        || ($outfile =~ /^>&?(?:STDOUT|1)$/i))
    {
        if (defined $myData{_TOP_STREAM}) {
            $out_fh = $myData{_OUTPUT};
        }
        else {
            ## Not a filename, just a string implying STDOUT
            $outfile ||= '-';
            $myData{_OUTFILE} = "<standard output>";
            $out_fh  = \*STDOUT;
        }
    }
    elsif ($outfile =~ /^>&(STDERR|2)$/i) {
        ## Not a filename, just a string implying STDERR
        $myData{_OUTFILE} = "<standard error>";
        $out_fh  = \*STDERR;
    }
    else {
        ## We have a filename, open it for writing
        $myData{_OUTFILE} = $outfile;
        (-d $outfile) and croak "$outfile is a directory, not POD input!\n";
        open($out_fh, "> $outfile")  or
             croak "Can't open $outfile for writing: $!\n";
        $close_output = 1;
    }

    ## Whew! That was a lot of work to set up reasonably/robust behavior
    ## in the case of a non-filename for reading and writing. Now we just
    ## have to parse the input and close the handles when we're finished.
    $self->parse_from_filehandle(\%opts, $in_fh, $out_fh);

    $close_input  and 
        close($in_fh) || croak "Can't close $infile after reading: $!\n";
    $close_output  and
        close($out_fh) || croak "Can't close $outfile after writing: $!\n";
}

#############################################################################

=head1 ACCESSOR METHODS

Clients of B<Pod::Parser> should use the following methods to access
instance data fields:

=cut

##---------------------------------------------------------------------------

=head1 B<errorsub()>

            $parser->errorsub("method_name");
            $parser->errorsub(\&warn_user);
            $parser->errorsub(sub { print STDERR, @_ });

Specifies the method or subroutine to use when printing error messages
about POD syntax. The supplied method/subroutine I<must> return TRUE upon
successful printing of the message. If C<undef> is given, then the B<warn>
builtin is used to issue error messages (this is the default behavior).

            my $errorsub = $parser->errorsub()
            my $errmsg = "This is an error message!\n"
            (ref $errorsub) and &{$errorsub}($errmsg)
                or (defined $errorsub) and $parser->$errorsub($errmsg)
                    or  warn($errmsg);

Returns a method name, or else a reference to the user-supplied subroutine
used to print error messages. Returns C<undef> if the B<warn> builtin
is used to issue error messages (this is the default behavior).

=cut

sub errorsub {
   return (@_ > 1) ? ($_[0]->{_ERRORSUB} = $_[1]) : $_[0]->{_ERRORSUB};
}

##---------------------------------------------------------------------------

=head1 B<cutting()>

            $boolean = $parser->cutting();

Returns the current C<cutting> state: a boolean-valued scalar which
evaluates to true if text from the input file is currently being "cut"
(meaning it is I<not> considered part of the POD document).

            $parser->cutting($boolean);

Sets the current C<cutting> state to the given value and returns the
result.

=cut

sub cutting {
   return (@_ > 1) ? ($_[0]->{_CUTTING} = $_[1]) : $_[0]->{_CUTTING};
}

##---------------------------------------------------------------------------

##---------------------------------------------------------------------------

=head1 B<parseopts()>

When invoked with no additional arguments, B<parseopts> returns a hashtable
of all the current parsing options.

            ## See if we are parsing non-POD sections as well as POD ones
            my %opts = $parser->parseopts();
            $opts{'-want_nonPODs}' and print "-want_nonPODs\n";

When invoked using a single string, B<parseopts> treats the string as the
name of a parse-option and returns its corresponding value if it exists
(returns C<undef> if it doesn't).

            ## Did we ask to see '=cut' paragraphs?
            my $want_cut = $parser->parseopts('-process_cut_cmd');
            $want_cut and print "-process_cut_cmd\n";

When invoked with multiple arguments, B<parseopts> treats them as
key/value pairs and the specified parse-option names are set to the
given values. Any unspecified parse-options are unaffected.

            ## Set them back to the default
            $parser->parseopts(-warnings => 0);

When passed a single hash-ref, B<parseopts> uses that hash to completely
reset the existing parse-options, all previous parse-option values
are lost.

            ## Reset all options to default 
            $parser->parseopts( { } );

See L<"PARSING OPTIONS"> for more information on the name and meaning of each
parse-option currently recognized.

=cut

sub parseopts {
   local *myData = shift;
   local *myOpts = ($myData{_PARSEOPTS} ||= {});
   return %myOpts  if (@_ == 0);
   if (@_ == 1) {
      local $_ = shift;
      return  ref($_)  ?  $myData{_PARSEOPTS} = $_  :  $myOpts{$_};
   }
   my @newOpts = (%myOpts, @_);
   $myData{_PARSEOPTS} = { @newOpts };
}

##---------------------------------------------------------------------------

=head1 B<output_file()>

            $fname = $parser->output_file();

Returns the name of the output file being written.

=cut

sub output_file {
   return $_[0]->{_OUTFILE};
}

##---------------------------------------------------------------------------

=head1 B<output_handle()>

            $fhandle = $parser->output_handle();

Returns the output filehandle object.

=cut

sub output_handle {
   return $_[0]->{_OUTPUT};
}

##---------------------------------------------------------------------------

=head1 B<input_file()>

            $fname = $parser->input_file();

Returns the name of the input file being read.

=cut

sub input_file {
   return $_[0]->{_INFILE};
}

##---------------------------------------------------------------------------

=head1 B<input_handle()>

            $fhandle = $parser->input_handle();

Returns the current input filehandle object.

=cut

sub input_handle {
   return $_[0]->{_INPUT};
}

##---------------------------------------------------------------------------

=begin __PRIVATE__

=head1 B<input_streams()>

            $listref = $parser->input_streams();

Returns a reference to an array which corresponds to the stack of all
the input streams that are currently in the middle of being parsed.

While parsing an input stream, it is possible to invoke
B<parse_from_file()> or B<parse_from_filehandle()> to parse a new input
stream and then return to parsing the previous input stream. Each input
stream to be parsed is pushed onto the end of this input stack
before any of its input is read. The input stream that is currently
being parsed is always at the end (or top) of the input stack. When an
input stream has been exhausted, it is popped off the end of the
input stack.

Each element on this input stack is a reference to C<Pod::InputSource>
object. Please see L<Pod::InputObjects> for more details.

This method might be invoked when printing diagnostic messages, for example,
to obtain the name and line number of the all input files that are currently
being processed.

=end __PRIVATE__

=cut

sub input_streams {
   return $_[0]->{_INPUT_STREAMS};
}

##---------------------------------------------------------------------------

=begin __PRIVATE__

=head1 B<top_stream()>

            $hashref = $parser->top_stream();

Returns a reference to the hash-table that represents the element
that is currently at the top (end) of the input stream stack
(see L<"input_streams()">). The return value will be the C<undef>
if the input stack is empty.

This method might be used when printing diagnostic messages, for example,
to obtain the name and line number of the current input file.

=end __PRIVATE__

=cut

sub top_stream {
   return $_[0]->{_TOP_STREAM} || undef;
}

#############################################################################

=head1 PRIVATE METHODS AND DATA

B<Pod::Parser> makes use of several internal methods and data fields
which clients should not need to see or use. For the sake of avoiding
name collisions for client data and methods, these methods and fields
are briefly discussed here. Determined hackers may obtain further
information about them by reading the B<Pod::Parser> source code.

Private data fields are stored in the hash-object whose reference is
returned by the B<new()> constructor for this class. The names of all
private methods and data-fields used by B<Pod::Parser> begin with a
prefix of "_" and match the regular expression C</^_\w+$/>.

=cut

##---------------------------------------------------------------------------

=begin _PRIVATE_

=head1 B<_push_input_stream()>

            $hashref = $parser->_push_input_stream($in_fh,$out_fh);

This method will push the given input stream on the input stack and
perform any necessary beginning-of-document or beginning-of-file
processing. The argument C<$in_fh> is the input stream filehandle to
push, and C<$out_fh> is the corresponding output filehandle to use (if
it is not given or is undefined, then the current output stream is used,
which defaults to standard output if it doesnt exist yet).

The value returned will be reference to the hash-table that represents
the new top of the input stream stack. I<Please Note> that it is
possible for this method to use default values for the input and output
file handles. If this happens, you will need to look at the C<INPUT>
and C<OUTPUT> instance data members to determine their new values.

=end _PRIVATE_

=cut

sub _push_input_stream {
    my ($self, $in_fh, $out_fh) = @_;
    local *myData = $self;

    ## Initialize stuff for the entire document if this is *not*
    ## an included file.
    ##
    ## NOTE: we need to be *very* careful when "defaulting" the output
    ## filehandle. We only want to use a default value if this is the
    ## beginning of the entire document (but *not* if this is an included
    ## file).
    unless (defined  $myData{_TOP_STREAM}) {
        $out_fh  = \*STDOUT  unless (defined $out_fh);
        $myData{_CUTTING}       = 1;   ## current "cutting" state
        $myData{_INPUT_STREAMS} = [];  ## stack of all input streams
    }

    ## Initialize input indicators
    $myData{_OUTFILE} = '(unknown)'  unless (defined  $myData{_OUTFILE});
    $myData{_OUTPUT}  = $out_fh      if (defined  $out_fh);
    $in_fh            = \*STDIN      unless (defined  $in_fh);
    $myData{_INFILE}  = '(unknown)'  unless (defined  $myData{_INFILE});
    $myData{_INPUT}   = $in_fh;
    my $input_top     = $myData{_TOP_STREAM}
                      = new Pod::InputSource(
                            -name        => $myData{_INFILE},
                            -handle      => $in_fh,
                            -was_cutting => $myData{_CUTTING}
                        );
    local *input_stack = $myData{_INPUT_STREAMS};
    push(@input_stack, $input_top);

    ## Perform beginning-of-document and/or beginning-of-input processing
    $self->begin_pod()  if (@input_stack == 1);
    $self->begin_input();

    return  $input_top;
}

##---------------------------------------------------------------------------

=begin _PRIVATE_

=head1 B<_pop_input_stream()>

            $hashref = $parser->_pop_input_stream();

This takes no arguments. It will perform any necessary end-of-file or
end-of-document processing and then pop the current input stream from
the top of the input stack.

The value returned will be reference to the hash-table that represents
the new top of the input stream stack.

=end _PRIVATE_

=cut

sub _pop_input_stream {
    my ($self) = @_;
    local *myData = $self;
    local *input_stack = $myData{_INPUT_STREAMS};

    ## Perform end-of-input and/or end-of-document processing
    $self->end_input()  if (@input_stack > 0);
    $self->end_pod()    if (@input_stack == 1);

    ## Restore cutting state to whatever it was before we started
    ## parsing this file.
    my $old_top = pop(@input_stack);
    $myData{_CUTTING} = $old_top->was_cutting();

    ## Dont forget to reset the input indicators
    my $input_top = undef;
    if (@input_stack > 0) {
       $input_top = $myData{_TOP_STREAM} = $input_stack[-1];
       $myData{_INFILE}  = $input_top->name();
       $myData{_INPUT}   = $input_top->handle();
    } else {
       delete $myData{_TOP_STREAM};
       delete $myData{_INPUT_STREAMS};
    }

    return  $input_top;
}

#############################################################################

=head1 TREE-BASED PARSING

If straightforward stream-based parsing wont meet your needs (as is
likely the case for tasks such as translating PODs into structured
markup languages like HTML and XML) then you may need to take the
tree-based approach. Rather than doing everything in one pass and
calling the B<interpolate()> method to expand sequences into text, it
may be desirable to instead create a parse-tree using the B<parse_text()>
method to return a tree-like structure which may contain an ordered
list of children (each of which may be a text-string, or a similar
tree-like structure).

Pay special attention to L<"METHODS FOR PARSING AND PROCESSING"> and
to the objects described in L<Pod::InputObjects>. The former describes
the gory details and parameters for how to customize and extend the
parsing behavior of B<Pod::Parser>. B<Pod::InputObjects> provides
several objects that may all be used interchangeably as parse-trees. The
most obvious one is the B<Pod::ParseTree> object. It defines the basic
interface and functionality that all things trying to be a POD parse-tree
should do. A B<Pod::ParseTree> is defined such that each "node" may be a
text-string, or a reference to another parse-tree.  Each B<Pod::Paragraph>
object and each B<Pod::InteriorSequence> object also supports the basic
parse-tree interface.

The B<parse_text()> method takes a given paragraph of text, and
returns a parse-tree that contains one or more children, each of which
may be a text-string, or an InteriorSequence object. There are also
callback-options that may be passed to B<parse_text()> to customize
the way it expands or transforms interior-sequences, as well as the
returned result. These callbacks can be used to create a parse-tree
with custom-made objects (which may or may not support the parse-tree
interface, depending on how you choose to do it).

If you wish to turn an entire POD document into a parse-tree, that process
is fairly straightforward. The B<parse_text()> method is the key to doing
this successfully. Every paragraph-callback (i.e. the polymorphic methods
for B<command()>, B<verbatim()>, and B<textblock()> paragraphs) takes
a B<Pod::Paragraph> object as an argument. Each paragraph object has a
B<parse_tree()> method that can be used to get or set a corresponding
parse-tree. So for each of those paragraph-callback methods, simply call
B<parse_text()> with the options you desire, and then use the returned
parse-tree to assign to the given paragraph object.

That gives you a parse-tree for each paragraph - so now all you need is
an ordered list of paragraphs. You can maintain that yourself as a data
element in the object/hash. The most straightforward way would be simply
to use an array-ref, with the desired set of custom "options" for each
invocation of B<parse_text>. Let's assume the desired option-set is
given by the hash C<%options>. Then we might do something like the
following:

    package MyPodParserTree;

    @ISA = qw( Pod::Parser );

    ...

    sub begin_pod {
        my $self = shift;
        $self->{'-paragraphs'} = [];  ## initialize paragraph list
    }

    sub command { 
        my ($parser, $command, $paragraph, $line_num, $pod_para) = @_;
        my $ptree = $parser->parse_text({%options}, $paragraph, ...);
        $pod_para->parse_tree( $ptree );
        push @{ $self->{'-paragraphs'} }, $pod_para;
    }

    sub verbatim { 
        my ($parser, $paragraph, $line_num, $pod_para) = @_;
        push @{ $self->{'-paragraphs'} }, $pod_para;
    }

    sub textblock { 
        my ($parser, $paragraph, $line_num, $pod_para) = @_;
        my $ptree = $parser->parse_text({%options}, $paragraph, ...);
        $pod_para->parse_tree( $ptree );
        push @{ $self->{'-paragraphs'} }, $pod_para;
    }

    ...

    package main;
    ...
    my $parser = new MyPodParserTree(...);
    $parser->parse_from_file(...);
    my $paragraphs_ref = $parser->{'-paragraphs'};

Of course, in this module-author's humble opinion, I'd be more inclined to
use the existing B<Pod::ParseTree> object than a simple array. That way
everything in it, paragraphs and sequences, all respond to the same core
interface for all parse-tree nodes. The result would look something like:

    package MyPodParserTree2;

    ...

    sub begin_pod {
        my $self = shift;
        $self->{'-ptree'} = new Pod::ParseTree;  ## initialize parse-tree
    }

    sub parse_tree {
        ## convenience method to get/set the parse-tree for the entire POD
        (@_ > 1)  and  $_[0]->{'-ptree'} = $_[1];
        return $_[0]->{'-ptree'};
    }

    sub command { 
        my ($parser, $command, $paragraph, $line_num, $pod_para) = @_;
        my $ptree = $parser->parse_text({<<options>>}, $paragraph, ...);
        $pod_para->parse_tree( $ptree );
        $parser->parse_tree()->append( $pod_para );
    }

    sub verbatim { 
        my ($parser, $paragraph, $line_num, $pod_para) = @_;
        $parser->parse_tree()->append( $pod_para );
    }

    sub textblock { 
        my ($parser, $paragraph, $line_num, $pod_para) = @_;
        my $ptree = $parser->parse_text({<<options>>}, $paragraph, ...);
        $pod_para->parse_tree( $ptree );
        $parser->parse_tree()->append( $pod_para );
    }

    ...

    package main;
    ...
    my $parser = new MyPodParserTree2(...);
    $parser->parse_from_file(...);
    my $ptree = $parser->parse_tree;
    ...

Now you have the entire POD document as one great big parse-tree. You
can even use the B<-expand_seq> option to B<parse_text> to insert
whole different kinds of objects. Just don't expect B<Pod::Parser>
to know what to do with them after that. That will need to be in your
code. Or, alternatively, you can insert any object you like so long as
it conforms to the B<Pod::ParseTree> interface.

One could use this to create subclasses of B<Pod::Paragraphs> and
B<Pod::InteriorSequences> for specific commands (or to create your own
custom node-types in the parse-tree) and add some kind of B<emit()>
method to each custom node/subclass object in the tree. Then all you'd
need to do is recursively walk the tree in the desired order, processing
the children (most likely from left to right) by formatting them if
they are text-strings, or by calling their B<emit()> method if they
are objects/references.

=head1 CAVEATS

Please note that POD has the notion of "paragraphs": this is something
starting I<after> a blank (read: empty) line, with the single exception
of the file start, which is also starting a paragraph. That means that
especially a command (e.g. C<=head1>) I<must> be preceded with a blank
line; C<__END__> is I<not> a blank line.

=head1 SEE ALSO

L<Pod::InputObjects>, L<Pod::Select>

B<Pod::InputObjects> defines POD input objects corresponding to
command paragraphs, parse-trees, and interior-sequences.

B<Pod::Select> is a subclass of B<Pod::Parser> which provides the ability
to selectively include and/or exclude sections of a POD document from being
translated based upon the current heading, subheading, subsubheading, etc.

=for __PRIVATE__
B<Pod::Callbacks> is a subclass of B<Pod::Parser> which gives its users
the ability the employ I<callback functions> instead of, or in addition
to, overriding methods of the base class.

=for __PRIVATE__
B<Pod::Select> and B<Pod::Callbacks> do not override any
methods nor do they define any new methods with the same name. Because
of this, they may I<both> be used (in combination) as a base class of
the same subclass in order to combine their functionality without
causing any namespace clashes due to multiple inheritance.

=head1 AUTHOR

Please report bugs using L<http://rt.cpan.org>.

Brad Appleton E<lt>bradapp@enteract.comE<gt>

Based on code for B<Pod::Text> written by
Tom Christiansen E<lt>tchrist@mox.perl.comE<gt>

=cut

1;
# vim: ts=4 sw=4 et
