


# [Boost.Tokenizer](http://boost.org/libs/tokenizer)



Boost.Tokenizer is a part of [Boost C++ Libraries](http://github.com/boostorg).  The Boost.Tokenizer package provides a flexible and easy-to-use way to break a string or other character sequence into a series of tokens.

## License

Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).

## Properties

* C++03
* Header-Only

## Build Status

Branch          | Travis | Appveyor | Coverity Scan | codecov.io | Deps | Docs | Tests |
:-------------: | ------ | -------- | ------------- | ---------- | ---- | ---- | ----- |
[`master`](https://github.com/boostorg/tokenizer/tree/master) | [![Build Status](https://travis-ci.org/boostorg/tokenizer.svg?branch=master)](https://travis-ci.org/boostorg/tokenizer) | [![Build status](https://ci.appveyor.com/api/projects/status/FIXME-vc81nhd5i2f6hi8y/branch/master?svg=true)](https://ci.appveyor.com/project/jeking3/tokenizer-c6pnd/branch/master) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/15854/badge.svg)](https://scan.coverity.com/projects/boostorg-tokenizer) | [![codecov](https://codecov.io/gh/boostorg/tokenizer/branch/master/graph/badge.svg)](https://codecov.io/gh/boostorg/tokenizer/branch/master)| [![Deps](https://img.shields.io/badge/deps-master-brightgreen.svg)](https://pdimov.github.io/boostdep-report/master/tokenizer.html) | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](http://www.boost.org/doc/libs/master/doc/html/tokenizer.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-master-brightgreen.svg)](http://www.boost.org/development/tests/master/developer/tokenizer.html)
[`develop`](https://github.com/boostorg/tokenizer/tree/develop) | [![Build Status](https://travis-ci.org/boostorg/tokenizer.svg?branch=develop)](https://travis-ci.org/boostorg/tokenizer) | [![Build status](https://ci.appveyor.com/api/projects/status/FIXME-vc81nhd5i2f6hi8y/branch/develop?svg=true)](https://ci.appveyor.com/project/jeking3/tokenizer-c6pnd/branch/develop) | [![Coverity Scan Build Status](https://scan.coverity.com/projects/15854/badge.svg)](https://scan.coverity.com/projects/boostorg-tokenizer) | [![codecov](https://codecov.io/gh/boostorg/tokenizer/branch/develop/graph/badge.svg)](https://codecov.io/gh/boostorg/tokenizer/branch/develop) | [![Deps](https://img.shields.io/badge/deps-develop-brightgreen.svg)](https://pdimov.github.io/boostdep-report/develop/tokenizer.html) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](http://www.boost.org/doc/libs/develop/doc/html/tokenizer.html) | [![Enter the Matrix](https://img.shields.io/badge/matrix-develop-brightgreen.svg)](http://www.boost.org/development/tests/develop/developer/tokenizer.html)


## Overview


> break up a phrase into words.

 <a target="_blank" href="http://melpon.org/wandbox/permlink/kZeKmQAtqDlpn8if">![Try it online][badge.wandbox]</a>

```c++
#include <iostream>
#include <boost/tokenizer.hpp>
#include <string>

int main(){
    std::string s = "This is,  a test";
    typedef boost::tokenizer<> Tok;
    Tok tok(s);
    for (Tok::iterator beg = tok.begin(); beg != tok.end(); ++beg){
        std::cout << *beg << "\n";
    }
}

```

> Using Range-based for loop (>c++11)

 <a target="_blank" href="http://melpon.org/wandbox/permlink/z94YLs8PdYSh7rXz">![Try it online][badge.wandbox]</a>
```c++
#include <iostream>
#include <boost/tokenizer.hpp>
#include <string>

int main(){
    std::string s = "This is,  a test";
    boost::tokenizer<> tok(s);
    for (auto token: tok) {
        std::cout << token << "\n";
    }
}
```

## Documentation

Documentation can be found at [Boost.Tokenizer](http://boost.org/libs/tokenizer)

## Related Material
[Boost.Tokenizer](http://theboostcpplibraries.com/boost.tokenizer) Chapter 10 at theboostcpplibraries.com, contains several examples including **escaped_list_separator**.

##Contributing

>This library is being maintained as a part of the [Boost Library Official Maintainer Program](http://beta.boost.org/community/official_library_maintainer_program.html)


Open Issues on <a target="_blank" href="https://svn.boost.org/trac/boost/query?status=assigned&status=new&status=reopened&component=tokenizer&col=id&col=summary&col=status&col=owner&col=type&col=milestone&order=priority">![][badge.trac]</a>



##Acknowledgements
>From the author:
>
I wish to thank the members of the boost mailing list, whose comments, compliments, and criticisms during both the development and formal review helped make the Tokenizer library what it is. I especially wish to thank Aleksey Gurtovoy for the idea of using a pair of iterators to specify the input, instead of a string. I also wish to thank Jeremy Siek for his idea of providing a container interface for the token iterators and for simplifying the template parameters for the TokenizerFunctions. He and Daryle Walker also emphasized the need to separate interface and implementation. Gary Powell sparked the idea of using the isspace and ispunct as the defaults for char_delimiters_separator. Jeff Garland provided ideas on how to change to order of the template parameters in order to make tokenizer easier to declare. Thanks to Douglas Gregor who served as review manager and provided many insights both on the boost list and in e-mail on how to polish up the implementation and presentation of Tokenizer. Finally, thanks to Beman Dawes who integrated the final version into the boost distribution.

##License
Distributed under the Boost Software License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)


[badge.Wandbox]: https://img.shields.io/badge/try%20it-online-blue.svg
[badge.Trac]:https://svn.boost.org/htdocs/common/trac_logo_mini.png

