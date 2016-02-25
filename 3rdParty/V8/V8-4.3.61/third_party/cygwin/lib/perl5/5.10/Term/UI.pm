package Term::UI;

use Carp;
use Params::Check qw[check allow];
use Term::ReadLine;
use Locale::Maketext::Simple Style => 'gettext';
use Term::UI::History;

use strict;

BEGIN {
    use vars        qw[$VERSION $AUTOREPLY $VERBOSE $INVALID];
    $VERBOSE    =   1;
    $VERSION    =   '0.18';
    $INVALID    =   loc('Invalid selection, please try again: ');
}

push @Term::ReadLine::Stub::ISA, __PACKAGE__
        unless grep { $_ eq __PACKAGE__ } @Term::ReadLine::Stub::ISA;


=pod

=head1 NAME

Term::UI - Term::ReadLine UI made easy

=head1 SYNOPSIS

    use Term::UI;
    use Term::ReadLine;

    my $term = Term::ReadLine->new('brand');

    my $reply = $term->get_reply(
                    prompt => 'What is your favourite colour?',
                    choices => [qw|blue red green|],
                    default => blue,
    );

    my $bool = $term->ask_yn(
                        prompt => 'Do you like cookies?',
                        default => 'y',
                );


    my $string = q[some_command -option --no-foo --quux='this thing'];

    my ($options,$munged_input) = $term->parse_options($string);


    ### don't have Term::UI issue warnings -- default is '1'
    $Term::UI::VERBOSE = 0;

    ### always pick the default (good for non-interactive terms)
    ### -- default is '0'
    $Term::UI::AUTOREPLY = 1;
    
    ### Retrieve the entire session as a printable string:
    $hist = Term::UI::History->history_as_string;
    $hist = $term->history_as_string;

=head1 DESCRIPTION

C<Term::UI> is a transparent way of eliminating the overhead of having
to format a question and then validate the reply, informing the user
if the answer was not proper and re-issuing the question.

Simply give it the question you want to ask, optionally with choices
the user can pick from and a default and C<Term::UI> will DWYM.

For asking a yes or no question, there's even a shortcut.

=head1 HOW IT WORKS

C<Term::UI> places itself at the back of the C<Term::ReadLine> 
C<@ISA> array, so you can call its functions through your term object.

C<Term::UI> uses C<Term::UI::History> to record all interactions
with the commandline. You can retrieve this history, or alter
the filehandle the interaction is printed to. See the 
C<Term::UI::History> manpage or the C<SYNOPSIS> for details.

=head1 METHODS

=head2 $reply = $term->get_reply( prompt => 'question?', [choices => \@list, default => $list[0], multi => BOOL, print_me => "extra text to print & record", allow => $ref] );

C<get_reply> asks a user a question, and then returns the reply to the
caller. If the answer is invalid (more on that below), the question will
be reposed, until a satisfactory answer has been entered.

You have the option of providing a list of choices the user can pick from
using the C<choices> argument. If the answer is not in the list of choices
presented, the question will be reposed.

If you provide a C<default>  answer, this will be returned when either
C<$AUTOREPLY> is set to true, (see the C<GLOBAL VARIABLES> section further
below), or when the user just hits C<enter>.

You can indicate that the user is allowed to enter multiple answers by
toggling the C<multi> flag. Note that a list of answers will then be
returned to you, rather than a simple string.

By specifying an C<allow> hander, you can yourself validate the answer
a user gives. This can be any of the types that the Params::Check C<allow> 
function allows, so please refer to that manpage for details. 

Finally, you have the option of adding a C<print_me> argument, which is
simply printed before the prompt. It's printed to the same file handle
as the rest of the questions, so you can use this to keep track of a
full session of Q&A with the user, and retrieve it later using the
C<< Term::UI->history_as_string >> function.

See the C<EXAMPLES> section for samples of how to use this function.

=cut

sub get_reply {
    my $term = shift;
    my %hash = @_;

    my $tmpl = {
        default     => { default => undef,  strict_type => 1 },
        prompt      => { default => '',     strict_type => 1, required => 1 },
        choices     => { default => [],     strict_type => 1 },
        multi       => { default => 0,      allow => [0, 1] },
        allow       => { default => qr/.*/ },
        print_me    => { default => '',     strict_type => 1 },
    };

    my $args = check( $tmpl, \%hash, $VERBOSE )
                or ( carp( loc(q[Could not parse arguments]) ), return );


    ### add this to the prompt to indicate the default
    ### answer to the question if there is one.
    my $prompt_add;
    
    ### if you supplied several choices to pick from,
    ### we'll print them seperately before the prompt
    if( @{$args->{choices}} ) {
        my $i;

        for my $choice ( @{$args->{choices}} ) {
            $i++;   # the answer counter -- but humans start counting
                    # at 1 :D
            
            ### so this choice is the default? add it to 'prompt_add'
            ### so we can construct a "foo? [DIGIT]" type prompt
            $prompt_add = $i if $choice eq $args->{default};

            ### create a "DIGIT> choice" type line
            $args->{print_me} .= sprintf "\n%3s> %-s", $i, $choice;
        }

        ### we listed some choices -- add another newline for 
        ### pretty printing
        $args->{print_me} .= "\n" if $i;

        ### allowable answers are now equal to the choices listed
        $args->{allow} = $args->{choices};

    ### no choices, but a default? set 'prompt_add' to the default
    ### to construct a 'foo? [DEFAULT]' type prompt
    } elsif ( defined $args->{default} ) {
        $prompt_add = $args->{default};
    }

    ### we set up the defaults, prompts etc, dispatch to the readline call
    return $term->_tt_readline( %$args, prompt_add => $prompt_add );

} 

=head2 $bool = $term->ask_yn( prompt => "your question", [default => (y|1,n|0), print_me => "extra text to print & record"] )

Asks a simple C<yes> or C<no> question to the user, returning a boolean
indicating C<true> or C<false> to the caller.

The C<default> answer will automatically returned, if the user hits 
C<enter> or if C<$AUTOREPLY> is set to true. See the C<GLOBAL VARIABLES>
section further below.

Also, you have the option of adding a C<print_me> argument, which is
simply printed before the prompt. It's printed to the same file handle
as the rest of the questions, so you can use this to keep track of a
full session of Q&A with the user, and retrieve it later using the
C<< Term::UI->history_as_string >> function.


See the C<EXAMPLES> section for samples of how to use this function.

=cut

sub ask_yn {
    my $term = shift;
    my %hash = @_;

    my $tmpl = {
        default     => { default => undef, allow => [qw|0 1 y n|],
                                                            strict_type => 1 },
        prompt      => { default => '', required => 1,      strict_type => 1 },
        print_me    => { default => '',                     strict_type => 1 },        
        multi       => { default => 0,                      no_override => 1 },
        choices     => { default => [qw|y n|],              no_override => 1 },
        allow       => { default => [qr/^y(?:es)?$/i, qr/^n(?:o)?$/i],
                         no_override => 1
                       },
    };

    my $args = check( $tmpl, \%hash, $VERBOSE ) or return undef;
    
    ### uppercase the default choice, if there is one, to be added
    ### to the prompt in a 'foo? [Y/n]' type style.
    my $prompt_add;
    {   my @list = @{$args->{choices}};
        if( defined $args->{default} ) {

            ### if you supplied the default as a boolean, rather than y/n
            ### transform it to a y/n now
            $args->{default} = $args->{default} =~ /\d/ 
                                ? { 0 => 'n', 1 => 'y' }->{ $args->{default} }
                                : $args->{default};
        
            @list = map { lc $args->{default} eq lc $_
                                ? uc $args->{default}
                                : $_
                    } @list;
        }

        $prompt_add .= join("/", @list);
    }

    my $rv = $term->_tt_readline( %$args, prompt_add => $prompt_add );
    
    return $rv =~ /^y/i ? 1 : 0;
}



sub _tt_readline {
    my $term = shift;
    my %hash = @_;

    local $Params::Check::VERBOSE = 0;  # why is this?
    local $| = 1;                       # print ASAP


    my ($default, $prompt, $choices, $multi, $allow, $prompt_add, $print_me);
    my $tmpl = {
        default     => { default => undef,  strict_type => 1, 
                            store => \$default },
        prompt      => { default => '',     strict_type => 1, required => 1,
                            store => \$prompt },
        choices     => { default => [],     strict_type => 1, 
                            store => \$choices },
        multi       => { default => 0,      allow => [0, 1], store => \$multi },
        allow       => { default => qr/.*/, store => \$allow, },
        prompt_add  => { default => '',     store => \$prompt_add },
        print_me    => { default => '',     store => \$print_me },
    };

    check( $tmpl, \%hash, $VERBOSE ) or return;

    ### prompts for Term::ReadLine can't be longer than one line, or
    ### it can display wonky on some terminals.
    history( $print_me ) if $print_me;

    
    ### we might have to add a default value to the prompt, to
    ### show the user what will be picked by default:
    $prompt .= " [$prompt_add]: " if $prompt_add;


    ### are we in autoreply mode?
    if ($AUTOREPLY) {
        
        ### you used autoreply, but didnt provide a default!
        carp loc(   
            q[You have '%1' set to true, but did not provide a default!],
            '$AUTOREPLY' 
        ) if( !defined $default && $VERBOSE);

        ### print it out for visual feedback
        history( join ' ', grep { defined } $prompt, $default );
        
        ### and return the default
        return $default;
    }


    ### so, no AUTOREPLY, let's see what the user will answer
    LOOP: {
        
        ### annoying bug in T::R::Perl that mucks up lines with a \n
        ### in them; So split by \n, save the last line as the prompt
        ### and just print the rest
        {   my @lines   = split "\n", $prompt;
            $prompt     = pop @lines;
            
            history( "$_\n" ) for @lines;
        }
        
        ### pose the question
        my $answer  = $term->readline($prompt);
        $answer     = $default unless length $answer;

        $term->addhistory( $answer ) if length $answer;

        ### add both prompt and answer to the history
        history( "$prompt $answer", 0 );

        ### if we're allowed to give multiple answers, split
        ### the answer on whitespace
        my @answers = $multi ? split(/\s+/, $answer) : $answer;

        ### the return value list
        my @rv;
        
        if( @$choices ) {
            
            for my $answer (@answers) {
                
                ### a digit implies a multiple choice question, 
                ### a non-digit is an open answer
                if( $answer =~ /\D/ ) {
                    push @rv, $answer if allow( $answer, $allow );
                } else {

                    ### remember, the answer digits are +1 compared to
                    ### the choices, because humans want to start counting
                    ### at 1, not at 0 
                    push @rv, $choices->[ $answer - 1 ] 
                        if $answer > 0 && defined $choices->[ $answer - 1];
                }    
            }
     
        ### no fixed list of choices.. just check if the answers
        ### (or otherwise the default!) pass the allow handler
        } else {       
            push @rv, grep { allow( $_, $allow ) }
                        scalar @answers ? @answers : ($default);  
        }

        ### if not all the answers made it to the return value list,
        ### at least one of them was an invalid answer -- make the 
        ### user do it again
        if( (@rv != @answers) or 
            (scalar(@$choices) and not scalar(@answers)) 
        ) {
            $prompt = $INVALID;
            $prompt .= "[$prompt_add] " if $prompt_add;
            redo LOOP;

        ### otherwise just return the answer, or answers, depending
        ### on the multi setting
        } else {
            return $multi ? @rv : $rv[0];
        }
    }
}

=head2 ($opts, $munged) = $term->parse_options( STRING );

C<parse_options> will convert all options given from an input string
to a hash reference. If called in list context it will also return
the part of the input string that it found no options in.

Consider this example:

    my $str =   q[command --no-foo --baz --bar=0 --quux=bleh ] .
                q[--option="some'thing" -one-dash -single=blah' arg];

    my ($options,$munged) =  $term->parse_options($str);

    ### $options would contain: ###
    $options = {
                'foo'       => 0,
                'bar'       => 0,
                'one-dash'  => 1,
                'baz'       => 1,
                'quux'      => 'bleh',
                'single'    => 'blah\'',
                'option'    => 'some\'thing'
    };

    ### and this is the munged version of the input string,
    ### ie what's left of the input minus the options
    $munged = 'command arg';

As you can see, you can either use a single or a double C<-> to
indicate an option.
If you prefix an option with C<no-> and do not give it a value, it
will be set to 0.
If it has no prefix and no value, it will be set to 1.
Otherwise, it will be set to its value. Note also that it can deal
fine with single/double quoting issues.

=cut

sub parse_options {
    my $term    = shift;
    my $input   = shift;

    my $return = {};

    ### there's probably a more elegant way to do this... ###
    while ( $input =~ s/(?:^|\s+)--?([-\w]+=("|').+?\2)(?=\Z|\s+)//  or
            $input =~ s/(?:^|\s+)--?([-\w]+=\S+)(?=\Z|\s+)//         or
            $input =~ s/(?:^|\s+)--?([-\w]+)(?=\Z|\s+)//
    ) {
        my $match = $1;

        if( $match =~ /^([-\w]+)=("|')(.+?)\2$/ ) {
            $return->{$1} = $3;

        } elsif( $match =~ /^([-\w]+)=(\S+)$/ ) {
            $return->{$1} = $2;

        } elsif( $match =~ /^no-?([-\w]+)$/i ) {
            $return->{$1} = 0;

        } elsif ( $match =~ /^([-\w]+)$/ ) {
            $return->{$1} = 1;

        } else {
            carp(loc(q[I do not understand option "%1"\n], $match)) if $VERBOSE;
        }
    }

    return wantarray ? ($return,$input) : $return;
}

=head2 $str = $term->history_as_string

Convenience wrapper around C<< Term::UI::History->history_as_string >>.

Consult the C<Term::UI::History> man page for details.

=cut

sub history_as_string { return Term::UI::History->history_as_string };

1;

=head1 GLOBAL VARIABLES

The behaviour of Term::UI can be altered by changing the following
global variables:

=head2 $Term::UI::VERBOSE

This controls whether Term::UI will issue warnings and explanations
as to why certain things may have failed. If you set it to 0,
Term::UI will not output any warnings.
The default is 1;

=head2 $Term::UI::AUTOREPLY

This will make every question be answered by the default, and warn if
there was no default provided. This is particularly useful if your
program is run in non-interactive mode.
The default is 0;

=head2 $Term::UI::INVALID

This holds the string that will be printed when the user makes an
invalid choice.
You can override this string from your program if you, for example,
wish to do localization.
The default is C<Invalid selection, please try again: >

=head2 $Term::UI::History::HISTORY_FH

This is the filehandle all the print statements from this module
are being sent to. Please consult the C<Term::UI::History> manpage
for details.

This defaults to C<*STDOUT>.

=head1 EXAMPLES

=head2 Basic get_reply sample

    ### ask a user (with an open question) for their favourite colour
    $reply = $term->get_reply( prompt => 'Your favourite colour? );
    
which would look like:

    Your favourite colour? 

and C<$reply> would hold the text the user typed.

=head2 get_reply with choices

    ### now provide a list of choices, so the user has to pick one
    $reply = $term->get_reply(
                prompt  => 'Your favourite colour?',
                choices => [qw|red green blue|] );
                
which would look like:

      1> red
      2> green
      3> blue
    
    Your favourite colour? 
                
C<$reply> will hold one of the choices presented. C<Term::UI> will repose
the question if the user attempts to enter an answer that's not in the
list of choices. The string presented is held in the C<$Term::UI::INVALID>
variable (see the C<GLOBAL VARIABLES> section for details.

=head2 get_reply with choices and default

    ### provide a sensible default option -- everyone loves blue!
    $reply = $term->get_reply(
                prompt  => 'Your favourite colour?',
                choices => [qw|red green blue|],
                default => 'blue' );

which would look like:

      1> red
      2> green
      3> blue
    
    Your favourite colour? [3]:  

Note the default answer after the prompt. A user can now just hit C<enter>
(or set C<$Term::UI::AUTOREPLY> -- see the C<GLOBAL VARIABLES> section) and
the sensible answer 'blue' will be returned.

=head2 get_reply using print_me & multi

    ### allow the user to pick more than one colour and add an 
    ### introduction text
    @reply = $term->get_reply(
                print_me    => 'Tell us what colours you like', 
                prompt      => 'Your favourite colours?',
                choices     => [qw|red green blue|],
                multi       => 1 );

which would look like:

    Tell us what colours you like
      1> red
      2> green
      3> blue
    
    Your favourite colours?

An answer of C<3 2 1> would fill C<@reply> with C<blue green red>

=head2 get_reply & allow

    ### pose an open question, but do a custom verification on 
    ### the answer, which will only exit the question loop, if 
    ### the answer matches the allow handler.
    $reply = $term->get_reply(
                prompt  => "What is the magic number?",
                allow   => 42 );
                
Unless the user now enters C<42>, the question will be reposed over
and over again. You can use more sophisticated C<allow> handlers (even
subroutines can be used). The C<allow> handler is implemented using
C<Params::Check>'s C<allow> function. Check its manpage for details.

=head2 an elaborate ask_yn sample

    ### ask a user if he likes cookies. Default to a sensible 'yes'
    ### and inform him first what cookies are.
    $bool = $term->ask_yn( prompt   => 'Do you like cookies?',
                           default  => 'y',
                           print_me => 'Cookies are LOVELY!!!' ); 

would print:                           

    Cookies are LOVELY!!!
    Do you like cookies? [Y/n]: 

If a user then simply hits C<enter>, agreeing with the default, 
C<$bool> would be set to C<true>. (Simply hitting 'y' would also 
return C<true>. Hitting 'n' would return C<false>)

We could later retrieve this interaction by printing out the Q&A 
history as follows:

    print $term->history_as_string;

which would then print:

    Cookies are LOVELY!!!
    Do you like cookies? [Y/n]:  y

There's a chance we're doing this non-interactively, because a console
is missing, the user indicated he just wanted the defaults, etc.

In this case, simply setting C<$Term::UI::AUTOREPLY> to true, will
return from every question with the default answer set for the question.
Do note that if C<AUTOREPLY> is true, and no default is set, C<Term::UI>
will warn about this and return C<undef>.

=head1 See Also

C<Params::Check>, C<Term::ReadLine>, C<Term::UI::History>

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-term-ui@rt.cpan.org<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.

=cut
