# !!Notice!!
    This is a preliminary release for internal team review.
    The URLs and addresses described below are not available yet.
    The official release will be announced later.
    Any suggestion for modification is welcome.
    Delays in replies are to be expected. Sorry in advance.

## What's mruby

mruby is the lightweight implementation of the Ruby language complying to
the [ISO standard](http://www.iso.org/iso/iso_catalogue/catalogue_tc/catalogue_detail.htm?csnumber=59579). 
mruby can run Ruby code in 'interpreter mode' or 'compile and execute it on a virtual machine' depending on the developer's preference.

This achievement was sponsored by the Regional Innovation Creation R&D Programs of
the Ministry of Economy, Trade and Industry of Japan.


## Features of mruby

    | Compatibility with MRI(Matz Ruby Implementation) version... 
    |
    |FIXME:
    |  + Simple Syntax
    |  + *Normal* Object-Oriented features(ex. class, method calls)
    |  + *Advanced* Object-Oriented features(ex. Mixin, Singleton-method)
    |  + Operator Overloading
    |  + Exception Handling
    |  + Iterators and Closures
    |  + Garbage Collection
    |  + Dynamic Loading of Object files(on some architecture)
    |  + Highly Portable (works on many Unix-like/POSIX compatible platforms
    |    as well as Windows, Mac OS X, BeOS etc.)
    |    cf. http://redmine.ruby-lang.org/wiki/ruby-19/SupportedPlatforms


## How to get mruby

The mruby distribution files can be found in the following site:

  https://github.com/mruby/mruby/zipball/master

The trunk of the mruby source tree can be checked out with the
following command:

    $ git clone https://github.com/mruby/mruby.git

There are some other branches under development. Try the following
command and see the list of branches:

    $ git branch -r


## mruby home-page

mruby's website is not launched yet but we are actively working on it.

The URL of the mruby home-page will be:

  http://www.mruby.org/


## Mailing list

To subscribe to the mruby mailing list....[T.B.D.]


## How to compile and install

See the INSTALL file.


## License

Copyright (c) 2012 mruby developers

Permission is hereby granted, free of charge, to any person obtaining a 
copy of this software and associated documentation files (the "Software"), 
to deal in the Software without restriction, including without limitation 
the rights to use, copy, modify, merge, publish, distribute, sublicense, 
and/or sell copies of the Software, and to permit persons to whom the 
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in 
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
DEALINGS IN THE SOFTWARE.

## Note for License

mruby has chosen a MIT License due to its permissive license allowing
developers to target various environments such as embedded systems.
However, the license requires the display of the copyright notice and license
information in manuals for instance. Doing so for big projects can be 
complicated or troublesome.
This is why mruby has decided to display "mruby developers" as the copyright name
to make it simple conventionally.
In the future, mruby might ask you to distribute your new code
(that you will commit,) under the MIT License as a member of
"mruby developers" but contributors will keep their copyright.
(We did not intend for contributors to transfer or waive their copyrights,
 Actual copyright holder name (contributors) will be listed in the AUTHORS file.)

Please ask us if you want to distribute your code under another license.

## How to Contribute

Send pull request to <http://github.com/mruby/mruby>.   We consider you have granted
non-exclusive right to your contributed code under MIT license.  If you want to be named
as one of mruby developers, include update to the AUTHORS file in your pull request.
