#!/usr/bin/perl -w

# Copyright (c) 2003-2017 Christopher M. Kohlhoff (chris at kohlhoff dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

use strict;

# Determine version number
my $version = "X.Y.Z";
open(VERSION, "../../include/asio/version.hpp") or die("Can't open version.hpp");
while (my $line = <VERSION>)
{
  if ($line =~ /^#define ASIO_VERSION .* \/\/ (.*)$/)
  {
    $version = $1;
  }
}
close(VERSION);

# Generate PDF output
system("bjam asioref");
system("xsltproc --stringparam asio.version $version asioref.xsl asio.docbook > asioref.docbook");
system("dblatex -I overview -s asioref.sty -P table.in.float=0 -o asioref-$version.pdf asioref.docbook");
system("rm asioref.docbook");
