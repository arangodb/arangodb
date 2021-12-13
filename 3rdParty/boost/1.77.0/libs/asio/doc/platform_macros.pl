#!/usr/bin/perl -w

# Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

use strict;

open(my $fh, "<../include/boost/asio/detail/config.hpp") or die("can't open config.hpp");

my $current_comment = "";
my %has_macros = ();
my %disable_macros = ();

while (my $line = <$fh>)
{
  chomp($line);
  if ($line =~ /^$/)
  {
    $current_comment = "";
  }
  elsif ($line =~ /^\/\/ (.*)$/)
  {
    $current_comment = $current_comment . $1 . "\n";
  }
  elsif ($line =~ /^# *define *BOOST_ASIO_HAS_([A-Z-0-9_]*) 1/)
  {
    if (not defined($has_macros{$1}))
    {
      $has_macros{$1} = $current_comment;
    }
  }
  elsif ($line =~ /BOOST_ASIO_DISABLE_([A-Z-0-9_]*)/)
  {
    $disable_macros{$1} = 1;
  }
}

my $intro = <<EOF;
[/
 / Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
 /
 / Distributed under the Boost Software License, Version 1.0. (See accompanying
 / file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 /]

[heading Compiler/platform feature detection macros]

Asio automatically defines preprocessor macros corresponding to the detected
available features on a particular compiler and target platform. These macros
are named with the prefix `BOOST_ASIO_HAS_`, and are listed in the table below.

Many of these macros also have a corresponding `BOOST_ASIO_DISABLE_` macro that
may be used to explicitly disable the feature.

In general, `BOOST_ASIO_HAS_` macros should not be explicitly defined by the
user, except when absolutely required as a workaround for the latest version of
a compiler or platform. For older compiler/platform combinations where a
specific `BOOST_ASIO_HAS_` macro is not automatically defined, testing may have
shown that a claimed feature isn't sufficiently conformant to be compatible
with Boost.Asio's needs.
EOF

print("$intro\n");
print("[table\n");
print("  [[Macro][Description][Macro to disable feature]]\n");
for my $macro (sort keys %has_macros)
{
  print("  [\n");
  print("    [`BOOST_ASIO_HAS_$macro`]\n");
  print("    [\n");
  my $description = $has_macros{$macro};
  chomp($description);
  $description =~ s/\n/\n      /g;
  print("      $description\n");
  print("    ]\n");
  if (defined $disable_macros{$macro})
  {
    print("    [`BOOST_ASIO_DISABLE_$macro`]\n");
  }
  else
  {
    print("    []\n");
  }
  print("  ]\n");
}
print("]\n");
