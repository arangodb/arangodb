IO extensions
=============

Overview
--------

This extension to boost::gil provides an easy to use interface for reading and
writing various image formats. It also includes a framework for adding
new formats.

Please see section 3.3 for all supported image formats. A basic tutorial is
provided in section [link gil.io.tutorial Tutorial].
Also, this extension requires Boost version 1.42 and up.
Furthermore the GIL extension Toolbox is used.

For adding new image formats please refer to section
[link gil.io.using_io.extending_gil__io_with_new_formats Extending GIL::IO with new Formats].

Supported Platforms
-------------------

All platforms supported by Boost which have a decent C++ compiler.
Depending on the image format one or more of the following image
libraries might be needed:

* libtiff
* libjpeg
* libpng
* libraw
* zlib

The library is designed to support as many formats as required by the user.
For instance, if the user only needs bmp support none of the above mentioned
dependencies are required.

There are more details available in this documentation on the image format
dependencies. Please see section
[link gil.io.using_io.supported_image_formats Supported Image Formats].

Tutorial
--------

Thanks to modern C++ programming techniques the interface for this library
is rather small and easy to use. In this tutorial I'll give you a short
walk-around on how to use this boost::gil extension.
For more details please refer to section 3.

For each supported IO format a single top-level header file is provided.
For instance, include `boost/gil/extension/io/tiff.hpp` to be able
to read or write TIFF files.

Reading An Image
~~~~~~~~~~~~~~~~

Probably the most common case to read a tiff image can be done as follows::

    std::string filename( "image.tif" );
    rgb8_image_t img;
    read_image( filename, img, tiff_tag() );

The code would be same for all other image formats. The only thing that needs
to change is the tag type ( tiff_tag ) in the read_image call.
The read_image() expects the supplied image type to be compatible with the
image stored in the file. If the user doesn't know what format an image has she
can use read_and_convert_image().
Another important fact is that read_image() will allocate the appropriate
memory needed for the read operation. There are ``read_view`` or
``read_and_convert_view`` counterparts, if the memory is already allocated.

Sometimes the user only wants to read a sub-part of an image,
then the above call would look as follows::

    read_image( filename
              , img
              , image_read_settings< tiff_tag >( point_t( 0, 0 ), point_t( 50, 50 ) )
              );

The image_read_settings class will provide the user with image format
independent reading setting but can also serves as a pointer for format
dependent settings.
Please see the specific image format sections
[link gil.io.using_io.supported_image_formats Supported Image Formats]
for more details.

Writing An Image
~~~~~~~~~~~~~~~~

Besides reading the information also writing is the second part of this
Boost.GIL extension. Writing is a lot simpler than reading since an existing
image view contains all the information.

For instance writing an image can be done as follows::

    std::string filename( "image.tif" );
    rgb8_image_t img( 640, 480 );

    // write data into image

    write_view( filename
              , view( img )
              , tiff_tag()
              );


The interface is similar to reading an image. To add image format specific
parameter the user can use ``image_write_info`` class.
For instance, a user can specify the JPEG quality when writing like this::

    std::string filename( "image.jpg" );
    rgb8_image_t img( 640, 480 );

    // write data into image

    write_view( filename
              , view( img )
              , image_write_info< jpeg_tag >( 95 )
              );


The above example will write an image where the jpeg quality is
set to 95 percent.

Reading And Writing In-Memory Buffers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Reading and writing in-memory buffers are supported as well. See as follows::

    // 1. Read an image.
    ifstream in( "test.tif", ios::binary );

    rgb8_image_t img;
    read_image( in, img, tiff_tag() );

    // 2. Write image to in-memory buffer.
    stringstream out_buffer( ios_base::out | ios_base::binary );

    rgb8_image_t src;
    write_view( out_buffer, view( src ), tiff_tag() );

    // 3. Copy in-memory buffer to another.
    stringstream in_buffer( ios_base::in | ios_base::binary );
    in_buffer << out_buffer.rdbuf();

    // 4. Read in-memory buffer to gil image
    rgb8_image_t dst;
    read_image( in_buffer, dst, tag_t() );

    // 5. Write out image.
    string filename( "out.tif" );
    ofstream out( filename.c_str(), ios_base::binary );
    write_view( out, view( dst ), tiff_tag() );

In case the user is using his own stream classes he has to make sure it
has the common interface read, write, seek, close, etc. Interface.

Using IO
--------

General Overview
~~~~~~~~~~~~~~~~

The tutorial pointed out some use cases for reading and writing images in
various image formats. This section will provide a more thorough overview.

The next sections will introduce the Read and Write interface. But it might be
worth pointing out that by using some advanced metaprogramming techniques the
interface is rather small and hopefully easy to understand.

Besides the general interface the user also has the ability to interface
directly with the underlying image format. For that each reader or writer
provides access to the so-called backend.

For instance::

    typedef get_reader_backend< const std::string
                              , tag_t
                              >::type backend_t;

    backend_t backend = read_image_info( bmp_filename
                                       , tag_t()
                                       );

    BOOST_CHECK_EQUAL( backend._info._width , 127 );
    BOOST_CHECK_EQUAL( backend._info._height, 64 );

Of course, the typedef can be removed when using c++11's auto feature.

Read Interface
~~~~~~~~~~~~~~

As the Tutorial demonstrated there are a few ways to read images.
Here is an enumeration of all read functions with a short description:

* ``read_image`` - read into a gil image with no conversion.
  Memory is allocated.
* ``read_view`` - read into a gil view with no conversion.
* ``read_and_convert_image`` - read and convert into a gil image.
  Memory is allocated.
* ``read_and_convert_view`` - read and convert into a gil view.
* ``read_image_info`` - read the image header.

Conversion in this context is necessary if the source (file) has an
incompatible color space with the destination (gil image type).
If that's the case the user has to use the xxx_and_convert_xxx variants.

All functions take the filename or a device as the first parameter.
The filename can be anything from a C-string, ``std::string``,
``std::wstring`` and ``boost::filesystem`` path. When using the path
object the user needs to define the ADD_FS_PATH_SUPPORT compiler symbol to
include the boost::filesystem dependency.
Devices could be a ``FILE*``, ``std::ifstream``, and ``TIFF*`` for TIFF images.

The second parameter is either an image or view type depending on the
``read_xxx`` function.
The third and last parameter is either an instance of the
``image_read_settings<FormatTag>`` or just the ``FormatTag``.
The settings can be various depending on the format which is being read.
But the all share settings for reading a partial image area.
The first point describes the top left image coordinate whereas the second
are the dimensions in x and y directions.

Here an example of setting up partial read::

    read_image( filename
              , img
              , image_read_settings< tiff_tag >( point_t( 0, 0 ), point_t( 50, 50 ) )
              );

Each format supports reading just the header information,
using ``read_image_info``. Please refer to the format specific sections
under 3.3. A basic example follows::

    image_read_info< tiff_t > info = read_image_info( filename
                                                    , tiff_t()
                                                    );

GIL also comes with a dynamic image extension.
In the context of GIL.IO a user can define an ``any_image`` type based on
several image types. The IO extension would then pick the matching image type
to the current image file.
The following example shows this feature::

    typedef mpl::vector< gray8_image_t
                       , gray16_image_t
                       , rgb8_image_t
                       , rgba_image_t
                       > my_img_types;

    any_image< my_img_types > runtime_image;

    read_image( filename
              , runtime_image
              , tiff_tag()
              );

During the review it became clear that there is a need to read big images
scanline by scanline. To support such use case a ``scanline_reader`` is
implemented for all supported image formats.
The ``scanline_read_iterators`` will then allow to traverse through the image.
The following code sample shows the usage::

    typedef tiff_tag tag_t;

    typedef scanline_reader< typename get_read_device< const char*
                                                     , tag_t
                                                     >::type
                            , tag_t
                            > reader_t;

    reader_t reader = make_scanline_reader( "C:/boost/libs/gil/test/extension/io/images/tiff/test.tif", tag_t() );

    typedef rgba8_image_t image_t;

    image_t dst( reader._info._width, reader._info._height );
    fill_pixels( view(dst), image_t::value_type() );

    typedef reader_t::iterator_t iterator_t;

    iterator_t it  = reader.begin();
    iterator_t end = reader.end();

    for( int row = 0; it != end; ++it, ++row )
    {
        copy_pixels( interleaved_view( reader._info._width
                                        , 1
                                        , ( image_t::view_t::x_iterator ) *it
                                        , reader._scanline_length
                                        )
                    , subimage_view( view( dst )
                                    , 0
                                    , row
                                    , reader._info._width
                                    , 1
                                    )
                    );
    }

There are many ways to traverse an image but for as of now only by
scanline is supported.


Write Interface
~~~~~~~~~~~~~~~

There is only one function for writing out images, write_view.
Similar to reading the first parameter is either a filename or a device.
The filename can be anything from a C-string, ``std::string``,
``std::wstring``, and ``boost::filesystem`` path. When using the path object
the user needs to define the ``ADD_FS_PATH_SUPPORT`` compiler symbol to
include the ``boost::filesystem`` dependency.
Devices could be ``FILE*``, ``std::ifstream``, and ``TIFF*`` for TIFF images.

The second parameter is an view object to image being written.
The third and last parameter is either a tag or an
``image_write_info<FormatTag>`` object containing more settings.
One example for instance is the JPEG quality.
Refer to the format specific sections under 3.3. to have a list of all
the possible settings.

Writing an any_image<...> is supported. See the following example::

    typedef mpl::vector< gray8_image_t
                       , gray16_image_t
                       , rgb8_image_t
                       , rgba_image_t
                       > my_img_types;


    any_image< my_img_types > runtime_image;

    // fill any_image

    write_view( filename
              , view( runtime_image )
              , tiff_tag()
              );

Compiler Symbols
~~~~~~~~~~~~~~~~

The following table gives an overview of all supported compiler symbols
that can be set by the user:

.. comment [table Compiler Symbols

======================================================== ========================================================
   Symbol                                                   Description
======================================================== ========================================================
BOOST_GIL_IO_ENABLE_GRAY_ALPHA                           Enable the color space "gray_alpha".
BOOST_GIL_IO_ADD_FS_PATH_SUPPORT                         Enable boost::filesystem 3.0 library.
BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED                Use libpng in floating point mode. This symbol is incompatible with BOOST_GIL_IO_PNG_FIXED_POINT_SUPPORTED.
BOOST_GIL_IO_PNG_FIXED_POINT_SUPPORTED                   Use libpng in integer mode. This symbol is incompatible with BOOST_GIL_IO_PNG_FLOATING_POINT_SUPPORTED.
BOOST_GIL_IO_PNG_DITHERING_SUPPORTED                     Look up "dithering" in libpng manual for explanation.
BOOST_GIL_IO_PNG_1_4_OR_LOWER                            Allow compiling with libpng 1.4 or lower.
BOOST_GIL_EXTENSION_IO_JPEG_C_LIB_COMPILED_AS_CPLUSPLUS  libjpeg is compiled as c++ lib.
BOOST_GIL_EXTENSION_IO_PNG_C_LIB_COMPILED_AS_CPLUSPLUS   libpng is compiled as c++ lib.
BOOST_GIL_EXTENSION_IO_TIFF_C_LIB_COMPILED_AS_CPLUSPLUS  libtiff is compiled as c++ lib.
BOOST_GIL_EXTENSION_IO_ZLIB_C_LIB_COMPILED_AS_CPLUSPLUS  zlib is compiled as c++ lib.
BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES                   Allow basic test images to be read from local hard drive. The paths can be set in paths.hpp
BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES                   Allow images to be written to the local hard drive. The paths can be set in paths.hpp
BOOST_GIL_IO_USE_BMP_TEST_SUITE_IMAGES                   Run tests using the bmp test images suite. See _BMP_TEST_FILES
BOOST_GIL_IO_USE_PNG_TEST_SUITE_IMAGES                   Run tests using the png test images suite. See _PNG_TEST_FILES
BOOST_GIL_IO_USE_PNM_TEST_SUITE_IMAGES                   Run tests using the pnm test images suite. Send me an email for accessing the files.
BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES          Run tests using the targa file format test images suite. See _TIFF_LIB_TIFF_TEST_FILES
BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES   Run tests using the targa file format test images suite. See _TIFF_GRAPHICSMAGICK_TEST_FILES
======================================================== ========================================================

Supported Image Formats
~~~~~~~~~~~~~~~~~~~~~~~

BMP
+++

For a general overview of the BMP image file format go to the
following BMP_Wiki_.

Please note, the code has not been tested on X Windows System variations
of the BMP format which are usually referred to XBM and XPM formats.

Here, only the MS Windows and OS/2 format is relevant.

Currently the code is able to read and write the following image types:

:Read: ``gray1_image_t``, ``gray4_image_t``, ``gray8_image_t``, ``rgb8_image_t`` and, ``rgba8_image_t``
:Write: ``rgb8_image_t`` and, ``rgba8_image_t``

The lack of having an indexed image type in gil restricts the current
interface to only write out non-indexed images.
This is subject to change soon.

JPEG
++++

For a general overview of the JPEG image file format go to the
following JPEG_Wiki_.

This jpeg extension is based on the libjpeg library which can be
found here, JPEG_Lib_.

All versions starting from 8x are supported.

The user has to make sure this library is properly installed.
I strongly recommend the user to build the library yourself.
It could potentially save you a lot of trouble.

Currently the code is able to read and write the following image types:

:Read: ``gray8_image_t``, ``rgb8_image_t``, ``cmyk8_image_t``
:Write: ``gray8_image_t``, ``rgb8_image_t``, ``cmyk8_image_t``

Reading YCbCr or YCCK images is possible but might result in inaccuracies since
both color spaces aren't available yet for gil.
For now these color space are read as rgb images.
This is subject to change soon.

PNG
+++

For a general overview of the PNG image file format go to the
following PNG_Wiki_.

This png extension is based on the libpng, which can be found
here, PNG_Lib_.

All versions starting from 1.5.x are supported.

The user has to make sure this library is properly installed.
I strongly recommend the user to build the library yourself.
It could potentially save you a lot of trouble.

Currently the code is able to read and write the following image types:

:Read: gray1, gray2, gray4, gray8, gray16, gray_alpha_8, gray_alpha_16, rgb8, rgb16, rgba8, rgba16
:Write: gray1, gray2, gray4, gray8, gray16, gray_alpha_8, gray_alpha_16, rgb8, rgb16, rgba8, rgba16

For reading gray_alpha images the user has to compile application with ``BOOST_GIL_IO_ENABLE_GRAY_ALPHA``
macro  defined. This color space is defined in the toolbox by using ``gray_alpha.hpp``.

PNM
+++

For a general overview of the PNM image file format go to the
following PNM_Wiki_. No external library is needed for the pnm format.

The extension can read images in both flavours of the formats, ASCII and binary,
that is types from P1 through P6; can write only binary formats.

Currently the code is able to read and write the following image types:

:Read: gray1, gray8, rgb8
:Write: gray1, gray8, rgb8

When reading a mono text image the data is read as a gray8 image.

RAW
+++

For a general overview see RAW_Wiki_.

Currently the extension is only able to read rgb8 images.

TARGA
+++++

For a general overview of the BMP image file format go to the
following TARGA_Wiki_.

Currently the code is able to read and write the following image types:

:Read: rgb8_image_t and rgba8_image_t
:Write: rgb8_image_t and rgba8_image_t

The lack of having an indexed image type in gil restricts the current
interface to only write out non-indexed images.
This is subject to change soon.

TIFF
++++

For a general overview of the TIFF image file format go to the
following TIFF_Wiki_.

This tiff extension is based on the libtiff, which can be found, TIFF_Lib_.

All versions starting from 3.9.x are supported.

The user has to make sure this library is properly installed. I strongly
recommend the user to build the library yourself. It could potentially
save you a lot of trouble.

TIFF images can virtually encode all kinds of channel sizes representing
various color spaces. Even planar images are possible.
For instance, ``rbg323`` or ``gray7``. The channels also can have specific
formats, like integer values or floating point values.

For a complete set of options please consult the following websites:

* TIFF_Base_Tags_
* TIFF_Extension_Tags_

The author of this extension is not claiming all tiff formats are supported.
This extension is likely to be a moving target adding new features with each
new milestone. Here is an incomplete lists:

* Multi-page TIFF - read only
* Strip TIFF - read and write support
* Tiled TIFF - read and write support with user defined tiled sizes
* Bit images TIFF - fully supported, like ``gray1_image_t`` (minisblack)
* Planar TIFF - fully supported
* Floating-point TIFF - fully supported
* Palette TIFF - supported but no indexed image type is available as of now

This gil extension uses two different test image suites to test read and
write capabilities. See ``test_image`` folder.
It's advisable to use ImageMagick test viewer to display images.


Extending GIL::IO with new Formats
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Extending the gil::io with new formats is meant to be simple and
straightforward. Before adding I would recommend to have a look at existing
implementations and then trying to follow a couple of guidelines:

* Create the following files for your new xxx format
    * ``xxx_read.hpp`` - Only includes read code
    * ``xxx_write.hpp`` - Only includes write code
    * ``xxx_all.hpp`` - includes xxx_read.hpp and xxx_write.hpp
* Add the code to the ``boost::gil::detail`` namespace
* Create a tag type for the new format. Like this::

    struct xxx_tag : format_tag {};

* Create the image_read_info for the new format. It contains all the
  information that are necessary to read an image. It should be filled
  and returned by the ``get_info`` member of the reader class. See below::

    template<> struct image_read_info< xxx_tag > {};

* Create the image_write_info for the new format. It contains all the
  information that are necessary to write an image::

    template<> struct image_write_info< xxx_tag > {};

* Use the following reader skeleton as a start::

    template< typename Device
            , typename ConversionPolicy
            >
    class reader< Device
                , xxx_tag
                , ConversionPolicy
                >
                : public reader_base< xxx_tag
                                    , ConversionPolicy
                                    >
    {
    private:

        typedef typename ConversionPolicy::color_converter_type cc_t;

    public:

        reader( Device& device )
        : _io_dev( device )
        {}

        reader( Device&     device
              , const cc_t& cc
              )
        : _io_dev( device )
        , reader_base< xxx_tag
                     , ConversionPolicy
                     >( cc )
        {}

        image_read_info< xxx_tag > get_info()
        {
            // your implementation here
        }

        template< typename View >
        void apply( const View& dst_view )
        {
            // your implementation here
        }
    };

* The writer skeleton::

    template< typename Device >
    class writer< Device
                , xxx_tag
                >
    {
    public:

        writer( Device & file )
        : out(file)
        {}

        template<typename View>
        void apply( const View& view )
        {
            // your implementation here
        }

        template<typename View>
        void apply( const View&                        view
                  , const image_write_info< xxx_tag >& info )
        {
            // your implementation here
        }
    };

Running gil::io tests
---------------------

gil::io comes with a large suite of test cases which reads and writes various
file formats. It uses some test image suites which can be found online or
which can be demanded from me by sending me an email.

There are some test images created by me in the test folder.
To enable unit tests which make use of them set the following compiler options
``BOOST_GIL_IO_TEST_ALLOW_READING_IMAGES`` and
``BOOST_GIL_IO_TEST_ALLOW_WRITING_IMAGES``.

The following list provides all links to the image suites the compiler symbol
to enable the tests:

:BMP:   BMP_TEST_FILES_                 -- BOOST_GIL_IO_USE_BMP_TEST_SUITE_IMAGES
:PNG:   PNG_TEST_FILES_                 -- BOOST_GIL_IO_USE_PNG_TEST_SUITE_IMAGES
:PNM:   request files from me           -- BOOST_GIL_IO_USE_PNM_TEST_SUITE_IMAGES
:TIFF:  TIFF_LIB_TIFF_TEST_FILES_       -- BOOST_GIL_IO_USE_TIFF_LIBTIFF_TEST_SUITE_IMAGES
:TIFF:  TIFF_GRAPHICSMAGICK_TEST_FILES_ -- BOOST_GIL_IO_USE_TIFF_GRAPHICSMAGICK_TEST_SUITE_IMAGES


.. _BMP_Wiki: http://en.wikipedia.org/wiki/BMP_file_format
.. _JPEG_Wiki: http://en.wikipedia.org/wiki/JPEG
.. _JPEG_lib: http://www.ijg.org/
.. _PNG_Wiki: http://en.wikipedia.org/wiki/Portable_Network_Graphics
.. _PNG_Lib: http://libpng.org/pub/png/libpng.html
.. _PNM_Wiki: http://en.wikipedia.org/wiki/Portable_anymap
.. _RAW_Wiki: http://en.wikipedia.org/wiki/Raw_image_format
.. _TARGA_Wiki: http://en.wikipedia.org/wiki/Truevision_TGA
.. _RAW_lib: http://www.libraw.org/
.. _RAW_Wiki: http://en.wikipedia.org/wiki/Raw_image_format
.. _TIFF_Wiki: http://en.wikipedia.org/wiki/Tagged_Image_File_Format
.. _TIFF_Lib: http://www.remotesensing.org/libtiff/
.. _TIFF_Base_Tags: http://www.awaresystems.be/imaging/tiff/tifftags/baseline.html
.. _TIFF_Extension_Tags: http://www.awaresystems.be/imaging/tiff/tifftags/extension.html
.. _BMP_TEST_FILES: http://entropymine.com/jason/bmpsuite/
.. _PNG_TEST_FILES: http://www.schaik.com/pngsuite/pngsuite.html
.. _TARGA_TEST_FILES: http://www.fileformat.info/format/tga/sample/index.htm
.. _TIFF_LIB_TIFF_TEST_FILES: http://www.remotesensing.org/libtiff/images.html
.. _TIFF_GRAPHICSMAGICK_TEST_FILES: ftp://ftp.graphicsmagick.org/pub/tiff-samples/tiff-sample-images-be.tar.gz
