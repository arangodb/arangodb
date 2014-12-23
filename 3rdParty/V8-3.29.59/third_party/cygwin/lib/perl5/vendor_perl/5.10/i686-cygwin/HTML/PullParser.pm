package HTML::PullParser;

# $Id: PullParser.pm,v 2.9 2006/04/26 08:00:28 gisle Exp $

require HTML::Parser;
@ISA=qw(HTML::Parser);
$VERSION = sprintf("%d.%02d", q$Revision: 2.9 $ =~ /(\d+)\.(\d+)/);

use strict;
use Carp ();

sub new
{
    my($class, %cnf) = @_;

    # Construct argspecs for the various events
    my %argspec;
    for (qw(start end text declaration comment process default)) {
	my $tmp = delete $cnf{$_};
	next unless defined $tmp;
	$argspec{$_} = $tmp;
    }
    Carp::croak("Info not collected for any events")
	  unless %argspec;

    my $file = delete $cnf{file};
    my $doc  = delete $cnf{doc};
    Carp::croak("Can't parse from both 'doc' and 'file' at the same time")
	  if defined($file) && defined($doc);
    Carp::croak("No 'doc' or 'file' given to parse from")
	  unless defined($file) || defined($doc);

    # Create object
    $cnf{api_version} = 3;
    my $self = $class->SUPER::new(%cnf);

    my $accum = $self->{pullparser_accum} = [];
    while (my($event, $argspec) = each %argspec) {
	$self->SUPER::handler($event => $accum, $argspec);
    }

    if (defined $doc) {
	$self->{pullparser_str_ref} = ref($doc) ? $doc : \$doc;
	$self->{pullparser_str_pos} = 0;
    }
    else {
	if (!ref($file) && ref(\$file) ne "GLOB") {
	    require IO::File;
	    $file = IO::File->new($file, "r") || return;
	}

	$self->{pullparser_file} = $file;
    }
    $self;
}


sub handler
{
    Carp::croak("Can't set handlers for HTML::PullParser");
}


sub get_token
{
    my $self = shift;
    while (!@{$self->{pullparser_accum}} && !$self->{pullparser_eof}) {
	if (my $f = $self->{pullparser_file}) {
	    # must try to parse more from the file
	    my $buf;
	    if (read($f, $buf, 512)) {
		$self->parse($buf);
	    } else {
		$self->eof;
		$self->{pullparser_eof}++;
		delete $self->{pullparser_file};
	    }
	}
	elsif (my $sref = $self->{pullparser_str_ref}) {
	    # must try to parse more from the scalar
	    my $pos = $self->{pullparser_str_pos};
	    my $chunk = substr($$sref, $pos, 512);
	    $self->parse($chunk);
	    $pos += length($chunk);
	    if ($pos < length($$sref)) {
		$self->{pullparser_str_pos} = $pos;
	    }
	    else {
		$self->eof;
		$self->{pullparser_eof}++;
		delete $self->{pullparser_str_ref};
		delete $self->{pullparser_str_pos};
	    }
	}
	else {
	    die;
	}
    }
    shift @{$self->{pullparser_accum}};
}


sub unget_token
{
    my $self = shift;
    unshift @{$self->{pullparser_accum}}, @_;
    $self;
}

1;


__END__

=head1 NAME

HTML::PullParser - Alternative HTML::Parser interface

=head1 SYNOPSIS

 use HTML::PullParser;

 $p = HTML::PullParser->new(file => "index.html",
                            start => 'event, tagname, @attr',
                            end   => 'event, tagname',
                            ignore_elements => [qw(script style)],
                           ) || die "Can't open: $!";
 while (my $token = $p->get_token) {
     #...do something with $token
 }

=head1 DESCRIPTION

The HTML::PullParser is an alternative interface to the HTML::Parser class.
It basically turns the HTML::Parser inside out.  You associate a file
(or any IO::Handle object or string) with the parser at construction time and
then repeatedly call $parser->get_token to obtain the tags and text
found in the parsed document.

The following methods are provided:

=over 4

=item $p = HTML::PullParser->new( file => $file, %options )

=item $p = HTML::PullParser->new( doc => \$doc, %options )

A C<HTML::PullParser> can be made to parse from either a file or a
literal document based on whether the C<file> or C<doc> option is
passed to the parser's constructor.

The C<file> passed in can either be a file name or a file handle
object.  If a file name is passed, and it can't be opened for reading,
then the constructor will return an undefined value and $!  will tell
you why it failed.  Otherwise the argument is taken to be some object
that the C<HTML::PullParser> can read() from when it needs more data.
The stream will be read() until EOF, but not closed.

A C<doc> can be passed plain or as a reference
to a scalar.  If a reference is passed then the value of this scalar
should not be changed before all tokens have been extracted.

Next the information to be returned for the different token types must
be set up.  This is done by simply associating an argspec (as defined
in L<HTML::Parser>) with the events you have an interest in.  For
instance, if you want C<start> tokens to be reported as the string
C<'S'> followed by the tagname and the attributes you might pass an
C<start>-option like this:

   $p = HTML::PullParser->new(
          doc   => $document_to_parse,
          start => '"S", tagname, @attr',
          end   => '"E", tagname',
        );

At last other C<HTML::Parser> options, like C<ignore_tags>, and
C<unbroken_text>, can be passed in.  Note that you should not use the
I<event>_h options to set up parser handlers.  That would confuse the
inner logic of C<HTML::PullParser>.

=item $token = $p->get_token

This method will return the next I<token> found in the HTML document,
or C<undef> at the end of the document.  The token is returned as an
array reference.  The content of this array match the argspec set up
during C<HTML::PullParser> construction.

=item $p->unget_token( @tokens )

If you find out you have read too many tokens you can push them back,
so that they are returned again the next time $p->get_token is called.

=back

=head1 EXAMPLES

The 'eg/hform' script shows how we might parse the form section of
HTML::Documents using HTML::PullParser.

=head1 SEE ALSO

L<HTML::Parser>, L<HTML::TokeParser>

=head1 COPYRIGHT

Copyright 1998-2001 Gisle Aas.

This library is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
