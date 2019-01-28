Long: data-urlencode
Arg: <data>
Help: HTTP POST data url encoded
Protocols: HTTP
See-also: data data-raw
Added: 7.18.0
---
This posts data, similar to the other --data options with the exception
that this performs URL-encoding.

To be CGI-compliant, the <data> part should begin with a \fIname\fP followed
by a separator and a content specification. The <data> part can be passed to
curl using one of the following syntaxes:
.RS
.IP "content"
This will make curl URL-encode the content and pass that on. Just be careful
so that the content doesn't contain any = or @ symbols, as that will then make
the syntax match one of the other cases below!
.IP "=content"
This will make curl URL-encode the content and pass that on. The preceding =
symbol is not included in the data.
.IP "name=content"
This will make curl URL-encode the content part and pass that on. Note that
the name part is expected to be URL-encoded already.
.IP "@filename"
This will make curl load data from the given file (including any newlines),
URL-encode that data and pass it on in the POST.
.IP "name@filename"
This will make curl load data from the given file (including any newlines),
URL-encode that data and pass it on in the POST. The name part gets an equal
sign appended, resulting in \fIname=urlencoded-file-content\fP. Note that the
name is expected to be URL-encoded already.
.RE
