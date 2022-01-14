This folder contains the documentation for the Boost.Test library.
Any contribution or submission to the library should be accompanied by the corresponding documentation.

The format of the documentation uses [Quickbook](http://www.boost.org/tools/quickbook/index.html).

How to build the documentation
==============================

In order to generate the documentation, the following is needed:

* Docbook
* Doxygen
* xsltproc

Doxygen
-------
Part of the documentation needs [Doxygen](http://www.doxygen.org). `doxygen` should be accessible from the ``PATH``.

Docbook
-------
Quickbook needs Docbook (XSL and XML) to be installed. Download and untar the docbook archives:

* Docbook XSL that can be found here: http://sourceforge.net/projects/docbook/files/docbook-xsl/
* Docbook DTD that can be found here: http://www.docbook.org/schemas/

The directories `$docbook_xsl_directory` and `$docbook_dtd_directory`, respectively, will refer to the location
of the deflated archive.

Download xsltproc
-----------------
This program is needed by Docbook, in order to be able to transform XMLs into HTMLs.
`xsltproc` should be accessible from the ``PATH``.

Construct b2
------------

Simply by typing in a console at the root of the Boost repository:

```
> ./bootstrap.[sh|bat]
```

Build the documentation
-----------------------

Running the following commands will construct the documentation with `b2` and
all the needed dependencies:

````
> cd $boost_root/libs/test/doc
> ../../../b2 -sDOCBOOK_XSL_DIR=$docbook_xsl_directory -sDOCBOOK_DTD_DIR=$docbook_dtd_directory
```

It is possible to run directly
```
> ../../../b2
```

but this results in a download from the Internet of the Docbook XLS and DTD, which is much slower.

Recommendations
===============

- Documentation is part of the "definition of done". A feature does not exist until it is implemented, tested, documented and reviewed.
- It is highly recommended that each of your pull request comes with an updated documentation. Not doing so put this work on the shoulders
  of the maintainers and as a result, it would be likely that the pull request is not addressed in a timely manner.
- Please also update the changelog for referencing your contribution
- Every file should come with a copyright notice on the very beginning
