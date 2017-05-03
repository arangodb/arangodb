


# [Boost.Tokenizer](http://boost.org/libs/tokenizer)



Boost tokenizer is a part of [Boost C++ Libraries](http://github.com/boostorg).  The Boost.Tokenizer package provides a flexible and easy-to-use way to break a string or other character sequence into a series of tokens.


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

