.. -*-rst-*-

.. Copyright David Abrahams 2009. Distributed under the Boost Software
.. License, Version 1.0. (See accompanying file LICENSE_1_0.txt or
.. copy at http://www.boost.org/LICENSE_1_0.txt)

To build the html::

  bjam html

To test the code in this documentation:

.. parsed-literal::

  python ../../../tools/litre/tool.py `pwd`/index.rst --dump_dir=../test/literate
  cd ../test/literate
  bjam
