#############################################################################
# Pod/Usage.pm -- print usage messages for the running script.
#
# Copyright (C) 1996-2000 by Bradford Appleton. All rights reserved.
# This file is part of "PodParser". PodParser is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

package Pod::Usage;

use vars qw($VERSION);
$VERSION = "1.35";  ## Current version of this package
require  5.005;    ## requires this Perl version or later

=head1 NAME

Pod::Usage, pod2usage() - print a usage message from embedded pod documentation

=head1 SYNOPSIS

  use Pod::Usage

  my $message_text  = "This text precedes the usage message.";
  my $exit_status   = 2;          ## The exit status to use
  my $verbose_level = 0;          ## The verbose level to use
  my $filehandle    = \*STDERR;   ## The filehandle to write to

  pod2usage($message_text);

  pod2usage($exit_status);

  pod2usage( { -message => $message_text ,
               -exitval => $exit_status  ,  
               -verbose => $verbose_level,  
               -output  => $filehandle } );

  pod2usage(   -msg     => $message_text ,
               -exitval => $exit_status  ,  
               -verbose => $verbose_level,  
               -output  => $filehandle   );

  pod2usage(   -verbose => 2,
               -noperldoc => 1  )

=head1 ARGUMENTS

B<pod2usage> should be given either a single argument, or a list of
arguments corresponding to an associative array (a "hash"). When a single
argument is given, it should correspond to exactly one of the following:

=over 4

=item *

A string containing the text of a message to print I<before> printing
the usage message

=item *

A numeric value corresponding to the desired exit status

=item *

A reference to a hash

=back

If more than one argument is given then the entire argument list is
assumed to be a hash.  If a hash is supplied (either as a reference or
as a list) it should contain one or more elements with the following
keys:

=over 4

=item C<-message>

=item C<-msg>

The text of a message to print immediately prior to printing the
program's usage message. 

=item C<-exitval>

The desired exit status to pass to the B<exit()> function.
This should be an integer, or else the string "NOEXIT" to
indicate that control should simply be returned without
terminating the invoking process.

=item C<-verbose>

The desired level of "verboseness" to use when printing the usage
message. If the corresponding value is 0, then only the "SYNOPSIS"
section of the pod documentation is printed. If the corresponding value
is 1, then the "SYNOPSIS" section, along with any section entitled
"OPTIONS", "ARGUMENTS", or "OPTIONS AND ARGUMENTS" is printed.  If the
corresponding value is 2 or more then the entire manpage is printed.

The special verbosity level 99 requires to also specify the -sections
parameter; then these sections are extracted (see L<Pod::Select>)
and printed.

=item C<-sections>

A string representing a selection list for sections to be printed
when -verbose is set to 99, e.g. C<"NAME|SYNOPSIS|DESCRIPTION|VERSION">.

=item C<-output>

A reference to a filehandle, or the pathname of a file to which the
usage message should be written. The default is C<\*STDERR> unless the
exit value is less than 2 (in which case the default is C<\*STDOUT>).

=item C<-input>

A reference to a filehandle, or the pathname of a file from which the
invoking script's pod documentation should be read.  It defaults to the
file indicated by C<$0> (C<$PROGRAM_NAME> for users of F<English.pm>).

=item C<-pathlist>

A list of directory paths. If the input file does not exist, then it
will be searched for in the given directory list (in the order the
directories appear in the list). It defaults to the list of directories
implied by C<$ENV{PATH}>. The list may be specified either by a reference
to an array, or by a string of directory paths which use the same path
separator as C<$ENV{PATH}> on your system (e.g., C<:> for Unix, C<;> for
MSWin32 and DOS).

=item C<-noperldoc>

By default, Pod::Usage will call L<perldoc> when -verbose >= 2 is
specified. This does not work well e.g. if the script was packed
with L<PAR>. The -noperldoc option suppresses the external call to
L<perldoc> and uses the simple text formatter (L<Pod::Text>) to 
output the POD.

=back

=head1 DESCRIPTION

B<pod2usage> will print a usage message for the invoking script (using
its embedded pod documentation) and then exit the script with the
desired exit status. The usage message printed may have any one of three
levels of "verboseness": If the verbose level is 0, then only a synopsis
is printed. If the verbose level is 1, then the synopsis is printed
along with a description (if present) of the command line options and
arguments. If the verbose level is 2, then the entire manual page is
printed.

Unless they are explicitly specified, the default values for the exit
status, verbose level, and output stream to use are determined as
follows:

=over 4

=item *

If neither the exit status nor the verbose level is specified, then the
default is to use an exit status of 2 with a verbose level of 0.

=item *

If an exit status I<is> specified but the verbose level is I<not>, then the
verbose level will default to 1 if the exit status is less than 2 and
will default to 0 otherwise.

=item *

If an exit status is I<not> specified but verbose level I<is> given, then
the exit status will default to 2 if the verbose level is 0 and will
default to 1 otherwise.

=item *

If the exit status used is less than 2, then output is printed on
C<STDOUT>.  Otherwise output is printed on C<STDERR>.

=back

Although the above may seem a bit confusing at first, it generally does
"the right thing" in most situations.  This determination of the default
values to use is based upon the following typical Unix conventions:

=over 4

=item *

An exit status of 0 implies "success". For example, B<diff(1)> exits
with a status of 0 if the two files have the same contents.

=item *

An exit status of 1 implies possibly abnormal, but non-defective, program
termination.  For example, B<grep(1)> exits with a status of 1 if
it did I<not> find a matching line for the given regular expression.

=item *

An exit status of 2 or more implies a fatal error. For example, B<ls(1)>
exits with a status of 2 if you specify an illegal (unknown) option on
the command line.

=item *

Usage messages issued as a result of bad command-line syntax should go
to C<STDERR>.  However, usage messages issued due to an explicit request
to print usage (like specifying B<-help> on the command line) should go
to C<STDOUT>, just in case the user wants to pipe the output to a pager
(such as B<more(1)>).

=item *

If program usage has been explicitly requested by the user, it is often
desirable to exit with a status of 1 (as opposed to 0) after issuing
the user-requested usage message.  It is also desirable to give a
more verbose description of program usage in this case.

=back

B<pod2usage> doesn't force the above conventions upon you, but it will
use them by default if you don't expressly tell it to do otherwise.  The
ability of B<pod2usage()> to accept a single number or a string makes it
convenient to use as an innocent looking error message handling function:

    use Pod::Usage;
    use Getopt::Long;

    ## Parse options
    GetOptions("help", "man", "flag1")  ||  pod2usage(2);
    pod2usage(1)  if ($opt_help);
    pod2usage(-verbose => 2)  if ($opt_man);

    ## Check for too many filenames
    pod2usage("$0: Too many files given.\n")  if (@ARGV > 1);

Some user's however may feel that the above "economy of expression" is
not particularly readable nor consistent and may instead choose to do
something more like the following:

    use Pod::Usage;
    use Getopt::Long;

    ## Parse options
    GetOptions("help", "man", "flag1")  ||  pod2usage(-verbose => 0);
    pod2usage(-verbose => 1)  if ($opt_help);
    pod2usage(-verbose => 2)  if ($opt_man);

    ## Check for too many filenames
    pod2usage(-verbose => 2, -message => "$0: Too many files given.\n")
        if (@ARGV > 1);

As with all things in Perl, I<there's more than one way to do it>, and
B<pod2usage()> adheres to this philosophy.  If you are interested in
seeing a number of different ways to invoke B<pod2usage> (although by no
means exhaustive), please refer to L<"EXAMPLES">.

=head1 EXAMPLES

Each of the following invocations of C<pod2usage()> will print just the
"SYNOPSIS" section to C<STDERR> and will exit with a status of 2:

    pod2usage();

    pod2usage(2);

    pod2usage(-verbose => 0);

    pod2usage(-exitval => 2);

    pod2usage({-exitval => 2, -output => \*STDERR});

    pod2usage({-verbose => 0, -output  => \*STDERR});

    pod2usage(-exitval => 2, -verbose => 0);

    pod2usage(-exitval => 2, -verbose => 0, -output => \*STDERR);

Each of the following invocations of C<pod2usage()> will print a message
of "Syntax error." (followed by a newline) to C<STDERR>, immediately
followed by just the "SYNOPSIS" section (also printed to C<STDERR>) and
will exit with a status of 2:

    pod2usage("Syntax error.");

    pod2usage(-message => "Syntax error.", -verbose => 0);

    pod2usage(-msg  => "Syntax error.", -exitval => 2);

    pod2usage({-msg => "Syntax error.", -exitval => 2, -output => \*STDERR});

    pod2usage({-msg => "Syntax error.", -verbose => 0, -output => \*STDERR});

    pod2usage(-msg  => "Syntax error.", -exitval => 2, -verbose => 0);

    pod2usage(-message => "Syntax error.",
              -exitval => 2,
              -verbose => 0,
              -output  => \*STDERR);

Each of the following invocations of C<pod2usage()> will print the
"SYNOPSIS" section and any "OPTIONS" and/or "ARGUMENTS" sections to
C<STDOUT> and will exit with a status of 1:

    pod2usage(1);

    pod2usage(-verbose => 1);

    pod2usage(-exitval => 1);

    pod2usage({-exitval => 1, -output => \*STDOUT});

    pod2usage({-verbose => 1, -output => \*STDOUT});

    pod2usage(-exitval => 1, -verbose => 1);

    pod2usage(-exitval => 1, -verbose => 1, -output => \*STDOUT});

Each of the following invocations of C<pod2usage()> will print the
entire manual page to C<STDOUT> and will exit with a status of 1:

    pod2usage(-verbose  => 2);

    pod2usage({-verbose => 2, -output => \*STDOUT});

    pod2usage(-exitval  => 1, -verbose => 2);

    pod2usage({-exitval => 1, -verbose => 2, -output => \*STDOUT});

=head2 Recommended Use

Most scripts should print some type of usage message to C<STDERR> when a
command line syntax error is detected. They should also provide an
option (usually C<-H> or C<-help>) to print a (possibly more verbose)
usage message to C<STDOUT>. Some scripts may even wish to go so far as to
provide a means of printing their complete documentation to C<STDOUT>
(perhaps by allowing a C<-man> option). The following complete example
uses B<Pod::Usage> in combination with B<Getopt::Long> to do all of these
things:

    use Getopt::Long;
    use Pod::Usage;

    my $man = 0;
    my $help = 0;
    ## Parse options and print usage if there is a syntax error,
    ## or if usage was explicitly requested.
    GetOptions('help|?' => \$help, man => \$man) or pod2usage(2);
    pod2usage(1) if $help;
    pod2usage(-verbose => 2) if $man;

    ## If no arguments were given, then allow STDIN to be used only
    ## if it's not connected to a terminal (otherwise print usage)
    pod2usage("$0: No files given.")  if ((@ARGV == 0) && (-t STDIN));
    __END__

    =head1 NAME

    sample - Using GetOpt::Long and Pod::Usage

    =head1 SYNOPSIS

    sample [options] [file ...]

     Options:
       -help            brief help message
       -man             full documentation

    =head1 OPTIONS

    =over 8

    =item B<-help>

    Print a brief help message and exits.

    =item B<-man>

    Prints the manual page and exits.

    =back

    =head1 DESCRIPTION

    B<This program> will read the given input file(s) and do something
    useful with the contents thereof.

    =cut

=head1 CAVEATS

By default, B<pod2usage()> will use C<$0> as the path to the pod input
file.  Unfortunately, not all systems on which Perl runs will set C<$0>
properly (although if C<$0> isn't found, B<pod2usage()> will search
C<$ENV{PATH}> or else the list specified by the C<-pathlist> option).
If this is the case for your system, you may need to explicitly specify
the path to the pod docs for the invoking script using something
similar to the following:

    pod2usage(-exitval => 2, -input => "/path/to/your/pod/docs");

In the pathological case that a script is called via a relative path
I<and> the script itself changes the current working directory
(see L<perlfunc/chdir>) I<before> calling pod2usage, Pod::Usage will
fail even on robust platforms. Don't do that.

=head1 AUTHOR

Please report bugs using L<http://rt.cpan.org>.

Brad Appleton E<lt>bradapp@enteract.comE<gt>

Based on code for B<Pod::Text::pod2text()> written by
Tom Christiansen E<lt>tchrist@mox.perl.comE<gt>

=head1 ACKNOWLEDGMENTS

Steven McDougall E<lt>swmcd@world.std.comE<gt> for his help and patience
with re-writing this manpage.

=cut

#############################################################################

use strict;
#use diagnostics;
use Carp;
use Config;
use Exporter;
use File::Spec;

use vars qw(@ISA @EXPORT);
@EXPORT = qw(&pod2usage);
BEGIN {
    if ( $] >= 5.005_58 ) {
       require Pod::Text;
       @ISA = qw( Pod::Text );
    }
    else {
       require Pod::PlainText;
       @ISA = qw( Pod::PlainText );
    }
}


##---------------------------------------------------------------------------

##---------------------------------
## Function definitions begin here
##---------------------------------

sub pod2usage {
    local($_) = shift;
    my %opts;
    ## Collect arguments
    if (@_ > 0) {
        ## Too many arguments - assume that this is a hash and
        ## the user forgot to pass a reference to it.
        %opts = ($_, @_);
    }
    elsif (!defined $_) {
      $_ = "";
    }
    elsif (ref $_) {
        ## User passed a ref to a hash
        %opts = %{$_}  if (ref($_) eq 'HASH');
    }
    elsif (/^[-+]?\d+$/) {
        ## User passed in the exit value to use
        $opts{"-exitval"} =  $_;
    }
    else {
        ## User passed in a message to print before issuing usage.
        $_  and  $opts{"-message"} = $_;
    }

    ## Need this for backward compatibility since we formerly used
    ## options that were all uppercase words rather than ones that
    ## looked like Unix command-line options.
    ## to be uppercase keywords)
    %opts = map {
        my $val = $opts{$_};
        s/^(?=\w)/-/;
        /^-msg/i   and  $_ = '-message';
        /^-exit/i  and  $_ = '-exitval';
        lc($_) => $val;    
    } (keys %opts);

    ## Now determine default -exitval and -verbose values to use
    if ((! defined $opts{"-exitval"}) && (! defined $opts{"-verbose"})) {
        $opts{"-exitval"} = 2;
        $opts{"-verbose"} = 0;
    }
    elsif (! defined $opts{"-exitval"}) {
        $opts{"-exitval"} = ($opts{"-verbose"} > 0) ? 1 : 2;
    }
    elsif (! defined $opts{"-verbose"}) {
        $opts{"-verbose"} = (lc($opts{"-exitval"}) eq "noexit" ||
                             $opts{"-exitval"} < 2);
    }

    ## Default the output file
    $opts{"-output"} = (lc($opts{"-exitval"}) eq "noexit" ||
                        $opts{"-exitval"} < 2) ? \*STDOUT : \*STDERR
            unless (defined $opts{"-output"});
    ## Default the input file
    $opts{"-input"} = $0  unless (defined $opts{"-input"});

    ## Look up input file in path if it doesnt exist.
    unless ((ref $opts{"-input"}) || (-e $opts{"-input"})) {
        my ($dirname, $basename) = ('', $opts{"-input"});
        my $pathsep = ($^O =~ /^(?:dos|os2|MSWin32)$/) ? ";"
                            : (($^O eq 'MacOS' || $^O eq 'VMS') ? ',' :  ":");
        my $pathspec = $opts{"-pathlist"} || $ENV{PATH} || $ENV{PERL5LIB};

        my @paths = (ref $pathspec) ? @$pathspec : split($pathsep, $pathspec);
        for $dirname (@paths) {
            $_ = File::Spec->catfile($dirname, $basename)  if length;
            last if (-e $_) && ($opts{"-input"} = $_);
        }
    }

    ## Now create a pod reader and constrain it to the desired sections.
    my $parser = new Pod::Usage(USAGE_OPTIONS => \%opts);
    if ($opts{"-verbose"} == 0) {
        $parser->select('SYNOPSIS\s*');
    }
    elsif ($opts{"-verbose"} == 1) {
        my $opt_re = '(?i)' .
                     '(?:OPTIONS|ARGUMENTS)' .
                     '(?:\s*(?:AND|\/)\s*(?:OPTIONS|ARGUMENTS))?';
        $parser->select( 'SYNOPSIS', $opt_re, "DESCRIPTION/$opt_re" );
    }
    elsif ($opts{"-verbose"} >= 2 && $opts{"-verbose"} != 99) {
        $parser->select('.*');
    }
    elsif ($opts{"-verbose"} == 99) {
        $parser->select( $opts{"-sections"} );
        $opts{"-verbose"} = 1;
    }

    ## Now translate the pod document and then exit with the desired status
    if ( !$opts{"-noperldoc"}
             and  $opts{"-verbose"} >= 2 
             and  !ref($opts{"-input"})
             and  $opts{"-output"} == \*STDOUT )
    {
       ## spit out the entire PODs. Might as well invoke perldoc
       my $progpath = File::Spec->catfile($Config{scriptdir}, "perldoc");
       system($progpath, $opts{"-input"});
       if($?) {
         # RT16091: fall back to more if perldoc failed
         system($ENV{PAGER} || 'more', $opts{"-input"});
       }
    }
    else {
       $parser->parse_from_file($opts{"-input"}, $opts{"-output"});
    }

    exit($opts{"-exitval"})  unless (lc($opts{"-exitval"}) eq 'noexit');
}

##---------------------------------------------------------------------------

##-------------------------------
## Method definitions begin here
##-------------------------------

sub new {
    my $this = shift;
    my $class = ref($this) || $this;
    my %params = @_;
    my $self = {%params};
    bless $self, $class;
    if ($self->can('initialize')) {
        $self->initialize();
    } else {
        $self = $self->SUPER::new();
        %$self = (%$self, %params);
    }
    return $self;
}

sub select {
    my ($self, @res) = @_;
    if ($ISA[0]->can('select')) {
        $self->SUPER::select(@_);
    } else {
        $self->{USAGE_SELECT} = \@res;
    }
}

# Override Pod::Text->seq_i to return just "arg", not "*arg*".
sub seq_i { return $_[1] }

# This overrides the Pod::Text method to do something very akin to what
# Pod::Select did as well as the work done below by preprocess_paragraph.
# Note that the below is very, very specific to Pod::Text.
sub _handle_element_end {
    my ($self, $element) = @_;
    if ($element eq 'head1') {
        $$self{USAGE_HEAD1} = $$self{PENDING}[-1][1];
        if ($self->{USAGE_OPTIONS}->{-verbose} < 2) {
            $$self{PENDING}[-1][1] =~ s/^\s*SYNOPSIS\s*$/USAGE/;
        }
    } elsif ($element eq 'head2') {
        $$self{USAGE_HEAD2} = $$self{PENDING}[-1][1];
    }
    if ($element eq 'head1' || $element eq 'head2') {
        $$self{USAGE_SKIPPING} = 1;
        my $heading = $$self{USAGE_HEAD1};
        $heading .= '/' . $$self{USAGE_HEAD2} if defined $$self{USAGE_HEAD2};
        if (!$$self{USAGE_SELECT} || !@{ $$self{USAGE_SELECT} }) {
           $$self{USAGE_SKIPPING} = 0;
        } else {
          for (@{ $$self{USAGE_SELECT} }) {
              if ($heading =~ /^$_\s*$/) {
                  $$self{USAGE_SKIPPING} = 0;
                  last;
              }
          }
        }

        # Try to do some lowercasing instead of all-caps in headings, and use
        # a colon to end all headings.
        if($self->{USAGE_OPTIONS}->{-verbose} < 2) {
            local $_ = $$self{PENDING}[-1][1];
            s{([A-Z])([A-Z]+)}{((length($2) > 2) ? $1 : lc($1)) . lc($2)}ge;
            s/\s*$/:/  unless (/:\s*$/);
            $_ .= "\n";
            $$self{PENDING}[-1][1] = $_;
        }
    }
    if ($$self{USAGE_SKIPPING}) {
        pop @{ $$self{PENDING} };
    } else {
        $self->SUPER::_handle_element_end($element);
    }
}

sub start_document {
    my $self = shift;
    $self->SUPER::start_document();
    my $msg = $self->{USAGE_OPTIONS}->{-message}  or  return 1;
    my $out_fh = $self->output_fh();
    print $out_fh "$msg\n";
}

sub begin_pod {
    my $self = shift;
    $self->SUPER::begin_pod();  ## Have to call superclass
    my $msg = $self->{USAGE_OPTIONS}->{-message}  or  return 1;
    my $out_fh = $self->output_handle();
    print $out_fh "$msg\n";
}

sub preprocess_paragraph {
    my $self = shift;
    local $_ = shift;
    my $line = shift;
    ## See if this is a heading and we arent printing the entire manpage.
    if (($self->{USAGE_OPTIONS}->{-verbose} < 2) && /^=head/) {
        ## Change the title of the SYNOPSIS section to USAGE
        s/^=head1\s+SYNOPSIS\s*$/=head1 USAGE/;
        ## Try to do some lowercasing instead of all-caps in headings
        s{([A-Z])([A-Z]+)}{((length($2) > 2) ? $1 : lc($1)) . lc($2)}ge;
        ## Use a colon to end all headings
        s/\s*$/:/  unless (/:\s*$/);
        $_ .= "\n";
    }
    return  $self->SUPER::preprocess_paragraph($_);
}

1; # keep require happy
