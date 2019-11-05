$! File: build_gnv_curl.com
$!
$! $Id$
$!
$! All in one build procedure
$!
$! Copyright 2009, John Malmberg
$!
$! Permission to use, copy, modify, and/or distribute this software for any
$! purpose with or without fee is hereby granted, provided that the above
$! copyright notice and this permission notice appear in all copies.
$!
$! THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
$! WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
$! MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
$! ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
$! WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
$! ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
$! OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
$!
$!
$! 11-Jun-2009	J. Malmberg
$!-----------------------------------------------------------------------
$!
$ @setup_gnv_curl_build.com
$!
$ bash gnv_curl_configure.sh
$!
$ @clean_gnv_curl.com
$!
$ bash make_gnv_curl_install.sh
$!
$ @gnv_link_curl.com
$!
$ purge new_gnu:[*...]/log
$!
$!
$exit
