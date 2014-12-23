package HTTP::Headers::ETag;

use strict;
use vars qw($VERSION);
$VERSION = "5.810";

require HTTP::Date;

require HTTP::Headers;
package HTTP::Headers;

sub _etags
{
    my $self = shift;
    my $header = shift;
    my @old = _split_etag_list($self->_header($header));
    if (@_) {
	$self->_header($header => join(", ", _split_etag_list(@_)));
    }
    wantarray ? @old : join(", ", @old);
}

sub etag          { shift->_etags("ETag", @_); }
sub if_match      { shift->_etags("If-Match", @_); }
sub if_none_match { shift->_etags("If-None-Match", @_); }

sub if_range {
    # Either a date or an entity-tag
    my $self = shift;
    my @old = $self->_header("If-Range");
    if (@_) {
	my $new = shift;
	if (!defined $new) {
	    $self->remove_header("If-Range");
	}
	elsif ($new =~ /^\d+$/) {
	    $self->_date_header("If-Range", $new);
	}
	else {
	    $self->_etags("If-Range", $new);
	}
    }
    return unless defined(wantarray);
    for (@old) {
	my $t = HTTP::Date::str2time($_);
	$_ = $t if $t;
    }
    wantarray ? @old : join(", ", @old);
}


# Split a list of entity tag values.  The return value is a list
# consisting of one element per entity tag.  Suitable for parsing
# headers like C<If-Match>, C<If-None-Match>.  You might even want to
# use it on C<ETag> and C<If-Range> entity tag values, because it will
# normalize them to the common form.
#
#  entity-tag	  = [ weak ] opaque-tag
#  weak		  = "W/"
#  opaque-tag	  = quoted-string


sub _split_etag_list
{
    my(@val) = @_;
    my @res;
    for (@val) {
        while (length) {
            my $weak = "";
	    $weak = "W/" if s,^\s*[wW]/,,;
            my $etag = "";
	    if (s/^\s*(\"[^\"\\]*(?:\\.[^\"\\]*)*\")//) {
		push(@res, "$weak$1");
            }
            elsif (s/^\s*,//) {
                push(@res, qq(W/"")) if $weak;
            }
            elsif (s/^\s*([^,\s]+)//) {
                $etag = $1;
		$etag =~ s/([\"\\])/\\$1/g;
	        push(@res, qq($weak"$etag"));
            }
            elsif (s/^\s+// || !length) {
                push(@res, qq(W/"")) if $weak;
            }
            else {
	 	die "This should not happen: '$_'";
            }
        }
   }
   @res;
}

1;
