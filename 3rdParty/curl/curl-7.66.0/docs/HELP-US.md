# How to get started helping out in the curl project

We are always in need of more help. If you are new to the project and are
looking for ways to contribute and help out, this document aims to give a few
good starting points.

A good idea is to start by subscribing to the [curl-library mailing
list](https://cool.haxx.se/mailman/listinfo/curl-library) to keep track of the
current discussion topics.

## Scratch your own itch

One of the best ways is to start working on any problems or issues you have
found yourself or perhaps got annoyed at in the past. It can be a spelling
error in an error text or a weirdly phrased section in a man page. Hunt it
down and report the bug. Or make your first pull request with a fix for that.

## Help wanted

In the issue tracker we occasionally mark bugs with [help
wanted](https://github.com/curl/curl/labels/help%20wanted), as a sign that the
bug is acknowledged to exist and that there's nobody known to work on this
issue for the moment. Those are bugs that are fine to "grab" and provide a
pull request for. The complexity level of these will of course vary, so pick
one that piques your interest.

## Work on known bugs

Some bugs are known and haven't yet received attention and work enough to get
fixed. We collect such known existing flaws in the
[KNOWN_BUGS](https://curl.haxx.se/docs/knownbugs.html) page. Many of them link
to the original bug report with some additional details, but some may also
have aged a bit and may require some verification that the bug still exists in
the same way and that what was said about it in the past is still valid.

## Fix autobuild problems

On the [autobuilds page](https://curl.haxx.se/dev/builds.html) we show a
collection of test results from the automatic curl build and tests that are
performed by volunteers. Fixing compiler warnings and errors shown there is
something we value greatly. Also, if you own or run systems or architectures
that aren't already tested in the autobuilds, we also appreciate more
volunteers running builds automatically to help us keep curl portable.

## TODO items

Ideas for features and functions that we have considered worthwhile to
implement and provide are kept in the
[TODO](https://curl.haxx.se/docs/todo.html) file. Some of the ideas are
rough. Some are well thought out. Some probably aren't really suitable
anymore.

Before you invest a lot of time on a TODO item, do bring it up for discussion
on the mailing list. For discussion on applicability but also for ideas and
brainstorming on specific ways to do the implementation etc.

## You decide

You can also come up with a completely new thing you think we should do. Or
not do. Or fix. Or add to the project. You then either bring it to the mailing
list first to see if people will shoot down the idea at once, or you bring a
first draft of the idea as a pull request and take the discussion there around
the specific implementation. Either way is fine.

## CONTRIBUTE

We offer [guidelines](https://curl.haxx.se/dev/contribute.html) that are
suitable to be familiar with before you decide to contribute to curl. If
you're used to open source development, you'll probably not find many
surprises in there.
