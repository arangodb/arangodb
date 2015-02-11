package URI::URL;

require URI::WithBase;
@ISA=qw(URI::WithBase);

use strict;
use vars qw(@EXPORT $VERSION);

$VERSION = "5.03";

# Provide as much as possible of the old URI::URL interface for backwards
# compatibility...

require Exporter;
*import = \&Exporter::import;
@EXPORT = qw(url);

# Easy to use constructor
sub url ($;$) { URI::URL->new(@_); }

use URI::Escape qw(uri_unescape);

sub new
{
    my $class = shift;
    my $self = $class->SUPER::new(@_);
    $self->[0] = $self->[0]->canonical;
    $self;
}

sub newlocal
{
    my $class = shift;
    require URI::file;
    bless [URI::file->new_abs(shift)], $class;
}

{package URI::_foreign;
    sub _init  # hope it is not defined
    {
	my $class = shift;
	die "Unknown URI::URL scheme $_[1]:" if $URI::URL::STRICT;
	$class->SUPER::_init(@_);
    }
}

sub strict
{
    my $old = $URI::URL::STRICT;
    $URI::URL::STRICT = shift if @_;
    $old;
}

sub print_on
{
    my $self = shift;
    require Data::Dumper;
    print STDERR Data::Dumper::Dumper($self);
}

sub _try
{
    my $self = shift;
    my $method = shift;
    scalar(eval { $self->$method(@_) });
}

sub crack
{
    # should be overridden by subclasses
    my $self = shift;
    (scalar($self->scheme),
     $self->_try("user"),
     $self->_try("password"),
     $self->_try("host"),
     $self->_try("port"),
     $self->_try("path"),
     $self->_try("params"),
     $self->_try("query"),
     scalar($self->fragment),
    )
}

sub full_path
{
    my $self = shift;
    my $path = $self->path_query;
    $path = "/" unless length $path;
    $path;
}

sub netloc
{
    shift->authority(@_);
}

sub epath
{
    my $path = shift->SUPER::path(@_);
    $path =~ s/;.*//;
    $path;
}

sub eparams
{
    my $self = shift;
    my @p = $self->path_segments;
    return unless ref($p[-1]);
    @p = @{$p[-1]};
    shift @p;
    join(";", @p);
}

sub params { shift->eparams(@_); }

sub path {
    my $self = shift;
    my $old = $self->epath(@_);
    return unless defined wantarray;
    return '/' if !defined($old) || !length($old);
    Carp::croak("Path components contain '/' (you must call epath)")
	if $old =~ /%2[fF]/ and !@_;
    $old = "/$old" if $old !~ m|^/| && defined $self->netloc;
    return uri_unescape($old);
}

sub path_components {
    shift->path_segments(@_);
}

sub query {
    my $self = shift;
    my $old = $self->equery(@_);
    if (defined(wantarray) && defined($old)) {
	if ($old =~ /%(?:26|2[bB]|3[dD])/) {  # contains escaped '=' '&' or '+'
	    my $mess;
	    for ($old) {
		$mess = "Query contains both '+' and '%2B'"
		  if /\+/ && /%2[bB]/;
		$mess = "Form query contains escaped '=' or '&'"
		  if /=/  && /%(?:3[dD]|26)/;
	    }
	    if ($mess) {
		Carp::croak("$mess (you must call equery)");
	    }
	}
	# Now it should be safe to unescape the string without loosing
	# information
	return uri_unescape($old);
    }
    undef;

}

sub abs
{
    my $self = shift;
    my $base = shift;
    my $allow_scheme = shift;
    $allow_scheme = $URI::URL::ABS_ALLOW_RELATIVE_SCHEME
	unless defined $allow_scheme;
    local $URI::ABS_ALLOW_RELATIVE_SCHEME = $allow_scheme;
    local $URI::ABS_REMOTE_LEADING_DOTS = $URI::URL::ABS_REMOTE_LEADING_DOTS;
    $self->SUPER::abs($base);
}

sub frag { shift->fragment(@_); }
sub keywords { shift->query_keywords(@_); }

# file:
sub local_path { shift->file; }
sub unix_path  { shift->file("unix"); }
sub dos_path   { shift->file("dos");  }
sub mac_path   { shift->file("mac");  }
sub vms_path   { shift->file("vms");  }

# mailto:
sub address { shift->to(@_); }
sub encoded822addr { shift->to(@_); }
sub URI::mailto::authority { shift->to(@_); }  # make 'netloc' method work

# news:
sub groupart { shift->_group(@_); }
sub article  { shift->message(@_); }

1;

__END__

=head1 NAME

URI::URL - Uniform Resource Locators

=head1 SYNOPSIS

 $u1 = URI::URL->new($str, $base);
 $u2 = $u1->abs;

=head1 DESCRIPTION

This module is provided for backwards compatibility with modules that
depend on the interface provided by the C<URI::URL> class that used to
be distributed with the libwww-perl library.

The following differences exist compared to the C<URI> class interface:

=over 3

=item *

The URI::URL module exports the url() function as an alternate
constructor interface.

=item *

The constructor takes an optional $base argument.  The C<URI::URL>
class is a subclass of C<URI::WithBase>.

=item *

The URI::URL->newlocal class method is the same as URI::file->new_abs.

=item *

URI::URL::strict(1)

=item *

$url->print_on method

=item *

$url->crack method

=item *

$url->full_path: same as ($uri->abs_path || "/")

=item *

$url->netloc: same as $uri->authority

=item *

$url->epath, $url->equery: same as $uri->path, $uri->query

=item *

$url->path and $url->query pass unescaped strings.

=item *

$url->path_components: same as $uri->path_segments (if you don't
consider path segment parameters)

=item *

$url->params and $url->eparams methods

=item *

$url->base method.  See L<URI::WithBase>.

=item *

$url->abs and $url->rel have an optional $base argument.  See
L<URI::WithBase>.

=item *

$url->frag: same as $uri->fragment

=item *

$url->keywords: same as $uri->query_keywords

=item *

$url->localpath and friends map to $uri->file.

=item *

$url->address and $url->encoded822addr: same as $uri->to for mailto URI

=item *

$url->groupart method for news URI

=item *

$url->article: same as $uri->message

=back



=head1 SEE ALSO

L<URI>, L<URI::WithBase>

=head1 COPYRIGHT

Copyright 1998-2000 Gisle Aas.

=cut
