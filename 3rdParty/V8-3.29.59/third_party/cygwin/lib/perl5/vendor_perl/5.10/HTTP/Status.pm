package HTTP::Status;

use strict;
require 5.002;   # because we use prototypes

use vars qw(@ISA @EXPORT @EXPORT_OK $VERSION);

require Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(is_info is_success is_redirect is_error status_message);
@EXPORT_OK = qw(is_client_error is_server_error);
$VERSION = "5.811";

# Note also addition of mnemonics to @EXPORT below

# Unmarked codes are from RFC 2616
# See also: http://en.wikipedia.org/wiki/List_of_HTTP_status_codes

my %StatusCode = (
    100 => 'Continue',
    101 => 'Switching Protocols',
    102 => 'Processing',                      # RFC 2518 (WebDAV)
    200 => 'OK',
    201 => 'Created',
    202 => 'Accepted',
    203 => 'Non-Authoritative Information',
    204 => 'No Content',
    205 => 'Reset Content',
    206 => 'Partial Content',
    207 => 'Multi-Status',                    # RFC 2518 (WebDAV)
    300 => 'Multiple Choices',
    301 => 'Moved Permanently',
    302 => 'Found',
    303 => 'See Other',
    304 => 'Not Modified',
    305 => 'Use Proxy',
    307 => 'Temporary Redirect',
    400 => 'Bad Request',
    401 => 'Unauthorized',
    402 => 'Payment Required',
    403 => 'Forbidden',
    404 => 'Not Found',
    405 => 'Method Not Allowed',
    406 => 'Not Acceptable',
    407 => 'Proxy Authentication Required',
    408 => 'Request Timeout',
    409 => 'Conflict',
    410 => 'Gone',
    411 => 'Length Required',
    412 => 'Precondition Failed',
    413 => 'Request Entity Too Large',
    414 => 'Request-URI Too Large',
    415 => 'Unsupported Media Type',
    416 => 'Request Range Not Satisfiable',
    417 => 'Expectation Failed',
    422 => 'Unprocessable Entity',            # RFC 2518 (WebDAV)
    423 => 'Locked',                          # RFC 2518 (WebDAV)
    424 => 'Failed Dependency',               # RFC 2518 (WebDAV)
    425 => 'No code',                         # WebDAV Advanced Collections
    426 => 'Upgrade Required',                # RFC 2817
    449 => 'Retry with',                      # unofficial Microsoft
    500 => 'Internal Server Error',
    501 => 'Not Implemented',
    502 => 'Bad Gateway',
    503 => 'Service Unavailable',
    504 => 'Gateway Timeout',
    505 => 'HTTP Version Not Supported',
    506 => 'Variant Also Negotiates',         # RFC 2295
    507 => 'Insufficient Storage',            # RFC 2518 (WebDAV)
    509 => 'Bandwidth Limit Exceeded',        # unofficial
    510 => 'Not Extended',                    # RFC 2774
);

my $mnemonicCode = '';
my ($code, $message);
while (($code, $message) = each %StatusCode) {
    # create mnemonic subroutines
    $message =~ tr/a-z \-/A-Z__/;
    $mnemonicCode .= "sub RC_$message () { $code }\t";
    # make them exportable
    $mnemonicCode .= "push(\@EXPORT, 'RC_$message');\n";
}
# warn $mnemonicCode; # for development
eval $mnemonicCode; # only one eval for speed
die if $@;

# backwards compatibility
*RC_MOVED_TEMPORARILY = \&RC_FOUND;  # 302 was renamed in the standard
push(@EXPORT, "RC_MOVED_TEMPORARILY");


sub status_message  ($) { $StatusCode{$_[0]}; }

sub is_info         ($) { $_[0] >= 100 && $_[0] < 200; }
sub is_success      ($) { $_[0] >= 200 && $_[0] < 300; }
sub is_redirect     ($) { $_[0] >= 300 && $_[0] < 400; }
sub is_error        ($) { $_[0] >= 400 && $_[0] < 600; }
sub is_client_error ($) { $_[0] >= 400 && $_[0] < 500; }
sub is_server_error ($) { $_[0] >= 500 && $_[0] < 600; }

1;


__END__

=head1 NAME

HTTP::Status - HTTP Status code processing

=head1 SYNOPSIS

 use HTTP::Status;

 if ($rc != RC_OK) {
     print status_message($rc), "\n";
 }

 if (is_success($rc)) { ... }
 if (is_error($rc)) { ... }
 if (is_redirect($rc)) { ... }

=head1 DESCRIPTION

I<HTTP::Status> is a library of routines for defining and
classifying HTTP status codes for libwww-perl.  Status codes are
used to encode the overall outcome of a HTTP response message.  Codes
correspond to those defined in RFC 2616 and RFC 2518.

=head1 CONSTANTS

The following constant functions can be used as mnemonic status code
names:

   RC_CONTINUE				(100)
   RC_SWITCHING_PROTOCOLS		(101)
   RC_PROCESSING                        (102)

   RC_OK				(200)
   RC_CREATED				(201)
   RC_ACCEPTED				(202)
   RC_NON_AUTHORITATIVE_INFORMATION	(203)
   RC_NO_CONTENT			(204)
   RC_RESET_CONTENT			(205)
   RC_PARTIAL_CONTENT			(206)
   RC_MULTI_STATUS                      (207)

   RC_MULTIPLE_CHOICES			(300)
   RC_MOVED_PERMANENTLY			(301)
   RC_FOUND				(302)
   RC_SEE_OTHER				(303)
   RC_NOT_MODIFIED			(304)
   RC_USE_PROXY				(305)
   RC_TEMPORARY_REDIRECT		(307)

   RC_BAD_REQUEST			(400)
   RC_UNAUTHORIZED			(401)
   RC_PAYMENT_REQUIRED			(402)
   RC_FORBIDDEN				(403)
   RC_NOT_FOUND				(404)
   RC_METHOD_NOT_ALLOWED		(405)
   RC_NOT_ACCEPTABLE			(406)
   RC_PROXY_AUTHENTICATION_REQUIRED	(407)
   RC_REQUEST_TIMEOUT			(408)
   RC_CONFLICT				(409)
   RC_GONE				(410)
   RC_LENGTH_REQUIRED			(411)
   RC_PRECONDITION_FAILED		(412)
   RC_REQUEST_ENTITY_TOO_LARGE		(413)
   RC_REQUEST_URI_TOO_LARGE		(414)
   RC_UNSUPPORTED_MEDIA_TYPE		(415)
   RC_REQUEST_RANGE_NOT_SATISFIABLE     (416)
   RC_EXPECTATION_FAILED		(417)
   RC_UNPROCESSABLE_ENTITY              (422)
   RC_LOCKED                            (423)
   RC_FAILED_DEPENDENCY                 (424)
   RC_NO_CODE                           (425)
   RC_UPGRADE_REQUIRED                  (426)
   RC_RETRY_WITH                        (449)

   RC_INTERNAL_SERVER_ERROR		(500)
   RC_NOT_IMPLEMENTED			(501)
   RC_BAD_GATEWAY			(502)
   RC_SERVICE_UNAVAILABLE		(503)
   RC_GATEWAY_TIMEOUT			(504)
   RC_HTTP_VERSION_NOT_SUPPORTED	(505)
   RC_VARIANT_ALSO_NEGOTIATES           (506)
   RC_INSUFFICIENT_STORAGE              (507)
   RC_BANDWIDTH_LIMIT_EXCEEDED          (509)
   RC_NOT_EXTENDED                      (510)

=head1 FUNCTIONS

The following additional functions are provided.  Most of them are
exported by default.

=over 4

=item status_message( $code )

The status_message() function will translate status codes to human
readable strings. The string is the same as found in the constant
names above.  If the $code is unknown, then C<undef> is returned.

=item is_info( $code )

Return TRUE if C<$code> is an I<Informational> status code (1xx).  This
class of status code indicates a provisional response which can't have
any content.

=item is_success( $code )

Return TRUE if C<$code> is a I<Successful> status code (2xx).

=item is_redirect( $code )

Return TRUE if C<$code> is a I<Redirection> status code (3xx). This class of
status code indicates that further action needs to be taken by the
user agent in order to fulfill the request.

=item is_error( $code )

Return TRUE if C<$code> is an I<Error> status code (4xx or 5xx).  The function
return TRUE for both client error or a server error status codes.

=item is_client_error( $code )

Return TRUE if C<$code> is an I<Client Error> status code (4xx). This class
of status code is intended for cases in which the client seems to have
erred.

This function is B<not> exported by default.

=item is_server_error( $code )

Return TRUE if C<$code> is an I<Server Error> status code (5xx). This class
of status codes is intended for cases in which the server is aware
that it has erred or is incapable of performing the request.

This function is B<not> exported by default.

=back

=head1 BUGS

Wished @EXPORT_OK had been used instead of @EXPORT in the beginning.
Now too much is exported by default.

