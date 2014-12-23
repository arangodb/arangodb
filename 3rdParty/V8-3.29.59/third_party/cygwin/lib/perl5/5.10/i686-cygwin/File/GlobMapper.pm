package File::GlobMapper;

use strict;
use warnings;
use Carp;

our ($CSH_GLOB);

BEGIN
{
    if ($] < 5.006)
    { 
        require File::BSDGlob; import File::BSDGlob qw(:glob) ;
        $CSH_GLOB = File::BSDGlob::GLOB_CSH() ;
        *globber = \&File::BSDGlob::csh_glob;
    }  
    else
    { 
        require File::Glob; import File::Glob qw(:glob) ;
        $CSH_GLOB = File::Glob::GLOB_CSH() ;
        #*globber = \&File::Glob::bsd_glob;
        *globber = \&File::Glob::csh_glob;
    }  
}

our ($Error);

our ($VERSION, @EXPORT_OK);
$VERSION = '1.000';
@EXPORT_OK = qw( globmap );


our ($noPreBS, $metachars, $matchMetaRE, %mapping, %wildCount);
$noPreBS = '(?<!\\\)' ; # no preceeding backslash
$metachars = '.*?[](){}';
$matchMetaRE = '[' . quotemeta($metachars) . ']';

%mapping = (
                '*' => '([^/]*)',
                '?' => '([^/])',
                '.' => '\.',
                '[' => '([',
                '(' => '(',
                ')' => ')',
           );

%wildCount = map { $_ => 1 } qw/ * ? . { ( [ /;           

sub globmap ($$;)
{
    my $inputGlob = shift ;
    my $outputGlob = shift ;

    my $obj = new File::GlobMapper($inputGlob, $outputGlob, @_)
        or croak "globmap: $Error" ;
    return $obj->getFileMap();
}

sub new
{
    my $class = shift ;
    my $inputGlob = shift ;
    my $outputGlob = shift ;
    # TODO -- flags needs to default to whatever File::Glob does
    my $flags = shift || $CSH_GLOB ;
    #my $flags = shift ;

    $inputGlob =~ s/^\s*\<\s*//;
    $inputGlob =~ s/\s*\>\s*$//;

    $outputGlob =~ s/^\s*\<\s*//;
    $outputGlob =~ s/\s*\>\s*$//;

    my %object =
            (   InputGlob   => $inputGlob,
                OutputGlob  => $outputGlob,
                GlobFlags   => $flags,
                Braces      => 0,
                WildCount   => 0,
                Pairs       => [],
                Sigil       => '#',
            );

    my $self = bless \%object, ref($class) || $class ;

    $self->_parseInputGlob()
        or return undef ;

    $self->_parseOutputGlob()
        or return undef ;
    
    my @inputFiles = globber($self->{InputGlob}, $flags) ;

    if (GLOB_ERROR)
    {
        $Error = $!;
        return undef ;
    }

    #if (whatever)
    {
        my $missing = grep { ! -e $_ } @inputFiles ;

        if ($missing)
        {
            $Error = "$missing input files do not exist";
            return undef ;
        }
    }

    $self->{InputFiles} = \@inputFiles ;

    $self->_getFiles()
        or return undef ;

    return $self;
}

sub _retError
{
    my $string = shift ;
    $Error = "$string in input fileglob" ;
    return undef ;
}

sub _unmatched
{
    my $delimeter = shift ;

    _retError("Unmatched $delimeter");
    return undef ;
}

sub _parseBit
{
    my $self = shift ;

    my $string = shift ;

    my $out = '';
    my $depth = 0 ;

    while ($string =~ s/(.*?)$noPreBS(,|$matchMetaRE)//)
    {
        $out .= quotemeta($1) ;
        $out .= $mapping{$2} if defined $mapping{$2};

        ++ $self->{WildCount} if $wildCount{$2} ;

        if ($2 eq ',')
        { 
            return _unmatched "("
                if $depth ;
            
            $out .= '|';
        }
        elsif ($2 eq '(')
        { 
            ++ $depth ;
        }
        elsif ($2 eq ')')
        { 
            return _unmatched ")"
                if ! $depth ;

            -- $depth ;
        }
        elsif ($2 eq '[')
        {
            # TODO -- quotemeta & check no '/'
            # TODO -- check for \]  & other \ within the []
            $string =~ s#(.*?\])##
                or return _unmatched "[" ;
            $out .= "$1)" ;
        }
        elsif ($2 eq ']')
        {
            return _unmatched "]" ;
        }
        elsif ($2 eq '{' || $2 eq '}')
        {
            return _retError "Nested {} not allowed" ;
        }
    }

    $out .= quotemeta $string;

    return _unmatched "("
        if $depth ;

    return $out ;
}

sub _parseInputGlob
{
    my $self = shift ;

    my $string = $self->{InputGlob} ;
    my $inGlob = '';

    # Multiple concatenated *'s don't make sense
    #$string =~ s#\*\*+#*# ;

    # TODO -- Allow space to delimit patterns?
    #my @strings = split /\s+/, $string ;
    #for my $str (@strings)
    my $out = '';
    my $depth = 0 ;

    while ($string =~ s/(.*?)$noPreBS($matchMetaRE)//)
    {
        $out .= quotemeta($1) ;
        $out .= $mapping{$2} if defined $mapping{$2};
        ++ $self->{WildCount} if $wildCount{$2} ;

        if ($2 eq '(')
        { 
            ++ $depth ;
        }
        elsif ($2 eq ')')
        { 
            return _unmatched ")"
                if ! $depth ;

            -- $depth ;
        }
        elsif ($2 eq '[')
        {
            # TODO -- quotemeta & check no '/' or '(' or ')'
            # TODO -- check for \]  & other \ within the []
            $string =~ s#(.*?\])##
                or return _unmatched "[";
            $out .= "$1)" ;
        }
        elsif ($2 eq ']')
        {
            return _unmatched "]" ;
        }
        elsif ($2 eq '}')
        {
            return _unmatched "}" ;
        }
        elsif ($2 eq '{')
        {
            # TODO -- check no '/' within the {}
            # TODO -- check for \}  & other \ within the {}

            my $tmp ;
            unless ( $string =~ s/(.*?)$noPreBS\}//)
            {
                return _unmatched "{";
            }
            #$string =~ s#(.*?)\}##;

            #my $alt = join '|', 
            #          map { quotemeta $_ } 
            #          split "$noPreBS,", $1 ;
            my $alt = $self->_parseBit($1);
            defined $alt or return 0 ;
            $out .= "($alt)" ;

            ++ $self->{Braces} ;
        }
    }

    return _unmatched "("
        if $depth ;

    $out .= quotemeta $string ;


    $self->{InputGlob} =~ s/$noPreBS[\(\)]//g;
    $self->{InputPattern} = $out ;

    #print "# INPUT '$self->{InputGlob}' => '$out'\n";

    return 1 ;

}

sub _parseOutputGlob
{
    my $self = shift ;

    my $string = $self->{OutputGlob} ;
    my $maxwild = $self->{WildCount};

    if ($self->{GlobFlags} & GLOB_TILDE)
    #if (1)
    {
        $string =~ s{
              ^ ~             # find a leading tilde
              (               # save this in $1
                  [^/]        # a non-slash character
                        *     # repeated 0 or more times (0 means me)
              )
            }{
              $1
                  ? (getpwnam($1))[7]
                  : ( $ENV{HOME} || $ENV{LOGDIR} )
            }ex;

    }

    # max #1 must be == to max no of '*' in input
    while ( $string =~ m/#(\d)/g )
    {
        croak "Max wild is #$maxwild, you tried #$1"
            if $1 > $maxwild ;
    }

    my $noPreBS = '(?<!\\\)' ; # no preceeding backslash
    #warn "noPreBS = '$noPreBS'\n";

    #$string =~ s/${noPreBS}\$(\d)/\${$1}/g;
    $string =~ s/${noPreBS}#(\d)/\${$1}/g;
    $string =~ s#${noPreBS}\*#\${inFile}#g;
    $string = '"' . $string . '"';

    #print "OUTPUT '$self->{OutputGlob}' => '$string'\n";
    $self->{OutputPattern} = $string ;

    return 1 ;
}

sub _getFiles
{
    my $self = shift ;

    my %outInMapping = ();
    my %inFiles = () ;

    foreach my $inFile (@{ $self->{InputFiles} })
    {
        next if $inFiles{$inFile} ++ ;

        my $outFile = $inFile ;

        if ( $inFile =~ m/$self->{InputPattern}/ )
        {
            no warnings 'uninitialized';
            eval "\$outFile = $self->{OutputPattern};" ;

            if (defined $outInMapping{$outFile})
            {
                $Error =  "multiple input files map to one output file";
                return undef ;
            }
            $outInMapping{$outFile} = $inFile;
            push @{ $self->{Pairs} }, [$inFile, $outFile];
        }
    }

    return 1 ;
}

sub getFileMap
{
    my $self = shift ;

    return $self->{Pairs} ;
}

sub getHash
{
    my $self = shift ;

    return { map { $_->[0] => $_->[1] } @{ $self->{Pairs} } } ;
}

1;

__END__

=head1 NAME

File::GlobMapper - Extend File Glob to Allow Input and Output Files

=head1 SYNOPSIS

    use File::GlobMapper qw( globmap );

    my $aref = globmap $input => $output
        or die $File::GlobMapper::Error ;

    my $gm = new File::GlobMapper $input => $output
        or die $File::GlobMapper::Error ;


=head1 DESCRIPTION

This module needs Perl5.005 or better.

This module takes the existing C<File::Glob> module as a starting point and
extends it to allow new filenames to be derived from the files matched by
C<File::Glob>.

This can be useful when carrying out batch operations on multiple files that
have both an input filename and output filename and the output file can be
derived from the input filename. Examples of operations where this can be
useful include, file renaming, file copying and file compression.


=head2 Behind The Scenes

To help explain what C<File::GlobMapper> does, consider what code you
would write if you wanted to rename all files in the current directory
that ended in C<.tar.gz> to C<.tgz>. So say these files are in the
current directory

    alpha.tar.gz
    beta.tar.gz
    gamma.tar.gz

and they need renamed to this

    alpha.tgz
    beta.tgz
    gamma.tgz

Below is a possible implementation of a script to carry out the rename
(error cases have been omitted)

    foreach my $old ( glob "*.tar.gz" )
    {
        my $new = $old;
        $new =~ s#(.*)\.tar\.gz$#$1.tgz# ;

        rename $old => $new 
            or die "Cannot rename '$old' to '$new': $!\n;
    }

Notice that a file glob pattern C<*.tar.gz> was used to match the
C<.tar.gz> files, then a fairly similar regular expression was used in
the substitute to allow the new filename to be created.

Given that the file glob is just a cut-down regular expression and that it
has already done a lot of the hard work in pattern matching the filenames,
wouldn't it be handy to be able to use the patterns in the fileglob to
drive the new filename?

Well, that's I<exactly> what C<File::GlobMapper> does. 

Here is same snippet of code rewritten using C<globmap>

    for my $pair (globmap '<*.tar.gz>' => '<#1.tgz>' )
    {
        my ($from, $to) = @$pair;
        rename $from => $to 
            or die "Cannot rename '$old' to '$new': $!\n;
    }

So how does it work?

Behind the scenes the C<globmap> function does a combination of a
file glob to match existing filenames followed by a substitute
to create the new filenames. 

Notice how both parameters to C<globmap> are strings that are delimited by <>.
This is done to make them look more like file globs - it is just syntactic
sugar, but it can be handy when you want the strings to be visually
distinctive. The enclosing <> are optional, so you don't have to use them - in
fact the first thing globmap will do is remove these delimiters if they are
present.

The first parameter to C<globmap>, C<*.tar.gz>, is an I<Input File Glob>. 
Once the enclosing "< ... >" is removed, this is passed (more or
less) unchanged to C<File::Glob> to carry out a file match.

Next the fileglob C<*.tar.gz> is transformed behind the scenes into a
full Perl regular expression, with the additional step of wrapping each
transformed wildcard metacharacter sequence in parenthesis.

In this case the input fileglob C<*.tar.gz> will be transformed into
this Perl regular expression 

    ([^/]*)\.tar\.gz

Wrapping with parenthesis allows the wildcard parts of the Input File
Glob to be referenced by the second parameter to C<globmap>, C<#1.tgz>,
the I<Output File Glob>. This parameter operates just like the replacement
part of a substitute command. The difference is that the C<#1> syntax
is used to reference sub-patterns matched in the input fileglob, rather
than the C<$1> syntax that is used with perl regular expressions. In
this case C<#1> is used to refer to the text matched by the C<*> in the
Input File Glob. This makes it easier to use this module where the
parameters to C<globmap> are typed at the command line.

The final step involves passing each filename matched by the C<*.tar.gz>
file glob through the derived Perl regular expression in turn and
expanding the output fileglob using it.

The end result of all this is a list of pairs of filenames. By default
that is what is returned by C<globmap>. In this example the data structure
returned will look like this

     ( ['alpha.tar.gz' => 'alpha.tgz'],
       ['beta.tar.gz'  => 'beta.tgz' ],
       ['gamma.tar.gz' => 'gamma.tgz']
     )


Each pair is an array reference with two elements - namely the I<from>
filename, that C<File::Glob> has matched, and a I<to> filename that is
derived from the I<from> filename.



=head2 Limitations

C<File::GlobMapper> has been kept simple deliberately, so it isn't intended to
solve all filename mapping operations. Under the hood C<File::Glob> (or for
older versions of Perl, C<File::BSDGlob>) is used to match the files, so you
will never have the flexibility of full Perl regular expression.

=head2 Input File Glob

The syntax for an Input FileGlob is identical to C<File::Glob>, except
for the following

=over 5

=item 1.

No nested {}

=item 2.

Whitespace does not delimit fileglobs.

=item 3.

The use of parenthesis can be used to capture parts of the input filename.

=item 4.

If an Input glob matches the same file more than once, only the first
will be used.

=back

The syntax

=over 5

=item B<~>

=item B<~user>


=item B<.>

Matches a literal '.'.
Equivalent to the Perl regular expression

    \.

=item B<*>

Matches zero or more characters, except '/'. Equivalent to the Perl
regular expression

    [^/]*

=item B<?>

Matches zero or one character, except '/'. Equivalent to the Perl
regular expression

    [^/]?

=item B<\>

Backslash is used, as usual, to escape the next character.

=item  B<[]>

Character class.

=item  B<{,}>

Alternation

=item  B<()>

Capturing parenthesis that work just like perl

=back

Any other character it taken literally.

=head2 Output File Glob

The Output File Glob is a normal string, with 2 glob-like features.

The first is the '*' metacharacter. This will be replaced by the complete
filename matched by the input file glob. So

    *.c *.Z

The second is     

Output FileGlobs take the 

=over 5

=item "*"

The "*" character will be replaced with the complete input filename.

=item #1

Patterns of the form /#\d/ will be replaced with the 

=back

=head2 Returned Data


=head1 EXAMPLES

=head2 A Rename script

Below is a simple "rename" script that uses C<globmap> to determine the
source and destination filenames.

    use File::GlobMapper qw(globmap) ;
    use File::Copy;

    die "rename: Usage rename 'from' 'to'\n"
        unless @ARGV == 2 ;

    my $fromGlob = shift @ARGV;
    my $toGlob   = shift @ARGV;

    my $pairs = globmap($fromGlob, $toGlob)
        or die $File::GlobMapper::Error;

    for my $pair (@$pairs)
    {
        my ($from, $to) = @$pair;
        move $from => $to ;
    }



Here is an example that renames all c files to cpp.
    
    $ rename '*.c' '#1.cpp'

=head2 A few example globmaps

Below are a few examples of globmaps

To copy all your .c file to a backup directory

    '</my/home/*.c>'    '</my/backup/#1.c>'

If you want to compress all    

    '</my/home/*.[ch]>'    '<*.gz>'

To uncompress

    '</my/home/*.[ch].gz>'    '</my/home/#1.#2>'

=head1 SEE ALSO

L<File::Glob|File::Glob>

=head1 AUTHOR

The I<File::GlobMapper> module was written by Paul Marquess, F<pmqs@cpan.org>.

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2005 Paul Marquess. All rights reserved.
This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.
