Boost Graph Library
===================

A generic interface for traversing graphs, using C++ templates.

The full documentation is available on [boost.org](http://www.boost.org/doc/libs/release/libs/graph/doc/index.html).

## Support, bugs and feature requests ##

Bugs and feature requests can be reported through the [Trac issue tracker](https://svn.boost.org/trac/boost/query?component=graph&desc=1&order=id)
(see [open issues](https://svn.boost.org/trac/boost/query?status=!closed&component=graph&desc=1&order=id) and
[closed issues](https://svn.boost.org/trac/boost/query?status=closed&component=graph&col=id&col=summary&col=status&col=owner&col=type&col=milestone&col=version&desc=1&order=id)). [Here](http://lists.boost.org/Archives/boost/2015/04/221780.php) is why Trac is still in use.

You can submit your changes through a [pull request](https://github.com/boostorg/graph/pulls). One of the maintainers will take a look (remember that it can take some time).

There is no mailing-list specific to Boost Graph, although you can use the general-purpose Boost [mailing-list](https://groups.google.com/forum/#!forum/boost-devel-archive) using the tag [graph].


## Development ##

Clone the whole boost project, which includes the individual Boost projects as submodules ([see boost+git doc](https://svn.boost.org/trac/boost/wiki/TryModBoost#InstallingModularBoost)): 

    git clone https://github.com/boostorg/boost
    cd boost
    git submodule update --init

The Boost Graph Library is located in `libs/graph/`. 

Boost Graph Library is mostly made of headers but also contains some compiled components. Here are the build commands:
    
    ./bootstrap.sh            <- compile b2 
    ./b2 headers              <- just installs headers
    ./b2                      <- build compiled components

**Note:** The Boost Graph Library cannot currently be built outside of Boost itself.

### Running tests ###
First, make sure you are in `libs/graph/test`. 
You can either run all the 300+ tests listed in `Jamfile.v2` or run a single test:

    ../../../b2                        <- run all tests
    ../../../b2 cycle_canceling_test   <- single test

You can also check the [regression tests reports](http://beta.boost.org/development/tests/develop/developer/graph.html).
