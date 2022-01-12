ToolBox extension
=================

.. _Reference: reference.html

Overview
--------

ToolBox provides collection of Boost.GIL extensions which are too
small to be maintained as standalone extensions.

Content:

* Color converters: Gray to  RGBA

* Color spaces: CMYKA, Gray with Alpha, HSL, HSV, Lab, XYZ

* Metafunctions:

  * ``channel_type``
  * ``channel_type_to_index``
  * ``get_num_bits``
  * ``get_pixel_type``
  * ``is_bit_aligned``
  * ``is_homogeneous``
  * ``is_similar``
  * ``pixel_bit_size``

* Image types:

  * ``indexed_image``

This extension will hopefully be added on by the community.

Since the extension is header-only, user just needs to include
its main header ``#include <boost/gil/extension/toolbox.hpp>``.

All definitions of the toolbox belong to the ``boost::gil`` namespace.

Folder Structure
----------------

The toolbox structured in the following sub-directories:

* color_converters
* color_spaces
* metafunctions
* image_types

Acknowledgements
----------------

Thanks to all the people who have reviewed this library and
made suggestions for improvements.

Reference
---------

The Reference_ section.
