Dynamic images and image views
==============================

The GIL extension called ``dynamic_image`` allows for images, image views
or any GIL constructs to have their parameters defined at run time.

The color space, channel depth, channel ordering, and interleaved/planar
structure of an image are defined by the type of its template argument, which
makes them compile-time bound. Often some of these parameters are available
only at run time. Consider, for example, writing a module that opens the image
at a given file path, rotates it and saves it back in its original color space
and channel depth. How can we possibly write this using our generic image?
What type is the image loading code supposed to return?

Here is an example:

.. code-block:: cpp

  #include <boost/gil/extension/dynamic_image/dynamic_image_all.hpp>
  using namespace boost;

  #define ASSERT_SAME(A,B) static_assert(is_same< A,B >::value, "")

  // Create any_image class (or any_image_view) class with a set of allowed images
  typedef any_image<rgb8_image_t, cmyk16_planar_image_t> my_any_image_t;

  // Associated view types are available (equivalent to the ones in image_t)
  typedef any_image_view<rgb8_view_t, cmyk16_planar_view_t> AV;
  ASSERT_SAME(my_any_image_t::view_t, AV);

  typedef any_image_view<rgb8c_view_t, cmyk16c_planar_view_t>> CAV;
  ASSERT_SAME(my_any_image_t::const_view_t, CAV);
  ASSERT_SAME(my_any_image_t::const_view_t, my_any_image_t::view_t::const_t);

  typedef any_image_view<rgb8_step_view_t, cmyk16_planar_step_view_t> SAV;
  ASSERT_SAME(typename dynamic_x_step_type<my_any_image_t::view_t>::type, SAV);

  // Assign it a concrete image at run time:
  my_any_image_t myImg = my_any_image_t(rgb8_image_t(100,100));

  // Change it to another at run time. The previous image gets destroyed
  myImg = cmyk16_planar_image_t(200,100);

  // Assigning to an image not in the allowed set throws an exception
  myImg = gray8_image_t();        // will throw std::bad_cast

The ``any_image`` and ``any_image_view`` subclass from Boost.Variant2 ``variant`` class,
a never valueless variant type, compatible with ``std::variant`` in C++17.

GIL ``any_image_view`` and ``any_image`` are subclasses of ``variant``:

.. code-block:: cpp

  template <typename ...ImageViewTypes>
  class any_image_view : public variant<ImageViewTypes...>
  {
  public:
    typedef ... const_t; // immutable equivalent of this
    typedef std::ptrdiff_t x_coord_t;
    typedef std::ptrdiff_t y_coord_t;
    typedef point<std::ptrdiff_t> point_t;
    using size_type = std::size_t;

    any_image_view();
    template <typename T> explicit any_image_view(const T& obj);
    any_image_view(const any_image_view& v);

    template <typename T> any_image_view& operator=(const T& obj);
    any_image_view&                       operator=(const any_image_view& v);

    // parameters of the currently instantiated view
    std::size_t num_channels()  const;
    point_t     dimensions()    const;
    size_type   size()          const;
    x_coord_t   width()         const;
    y_coord_t   height()        const;
  };

  template <typename ...ImageTypes>
  class any_image : public variant<ImageTypes...>
  {
  public:
    typedef ... const_view_t;
    typedef ... view_t;
    typedef std::ptrdiff_t x_coord_t;
    typedef std::ptrdiff_t y_coord_t;
    typedef point<std::ptrdiff_t> point_t;

    any_image();
    template <typename T> explicit any_image(const T& obj);
    template <typename T> explicit any_image(T& obj, bool do_swap);
    any_image(const any_image& v);

    template <typename T> any_image& operator=(const T& obj);
    any_image&                       operator=(const any_image& v);

    void recreate(const point_t& dims, unsigned alignment=1);
    void recreate(x_coord_t width, y_coord_t height, unsigned alignment=1);

    std::size_t num_channels()  const;
    point_t     dimensions()    const;
    x_coord_t   width()         const;
    y_coord_t   height()        const;
  };

Operations are invoked on variants via ``apply_operation`` passing a
function object to perform the operation. The code for every allowed
type in the variant is instantiated and the appropriate instantiation
is selected via a switch statement. Since image view algorithms
typically have time complexity at least linear on the number of
pixels, the single switch statement of image view variant adds
practically no measurable performance overhead compared to templated
image views.

Variants behave like the underlying type. Their copy constructor will
invoke the copy constructor of the underlying instance. Equality
operator will check if the two instances are of the same type and then
invoke their ``operator==``, etc. The default constructor of a variant
will default-construct the first type. That means that
``any_image_view`` has shallow default-constructor, copy-constructor,
assignment and equality comparison, whereas ``any_image`` has deep
ones.

It is important to note that even though ``any_image_view`` and
``any_image`` resemble the static ``image_view`` and ``image``, they
do not model the full requirements of ``ImageViewConcept`` and
``ImageConcept``. In particular they don't provide access to the
pixels. There is no "any_pixel" or "any_pixel_iterator" in GIL. Such
constructs could be provided via the ``variant`` mechanism, but doing
so would result in inefficient algorithms, since the type resolution
would have to be performed per pixel. Image-level algorithms should be
implemented via ``apply_operation``. That said, many common operations
are shared between the static and dynamic types. In addition, all of
the image view transformations and many STL-like image view algorithms
have overloads operating on ``any_image_view``, as illustrated with
``copy_pixels``:

.. code-block:: cpp

  rgb8_view_t v1(...);  // concrete image view
  bgr8_view_t v2(...);  // concrete image view compatible with v1 and of the same size
  any_image_view<Types>  av(...);  // run-time specified image view

  // Copies the pixels from v1 into v2.
  // If the pixels are incompatible triggers compile error
  copy_pixels(v1,v2);

  // The source or destination (or both) may be run-time instantiated.
  // If they happen to be incompatible, throws std::bad_cast
  copy_pixels(v1, av);
  copy_pixels(av, v2);
  copy_pixels(av, av);

By having algorithm overloads supporting dynamic constructs, we create
a base upon which it is possible to write algorithms that can work
with either compile-time or runtime images or views. The following
code, for example, uses the GIL I/O extension to turn an image on disk
upside down:

.. code-block:: cpp

  #include <boost\gil\extension\io\jpeg_dynamic_io.hpp>

  template <typename Image>    // Could be rgb8_image_t or any_image<...>
  void save_180rot(const std::string& file_name)
  {
    Image img;
    jpeg_read_image(file_name, img);
    jpeg_write_view(file_name, rotated180_view(view(img)));
  }

It can be instantiated with either a compile-time or a runtime image
because all functions it uses have overloads taking runtime
constructs. For example, here is how ``rotated180_view`` is
implemented:

.. code-block:: cpp

  // implementation using templated view
  template <typename View>
  typename dynamic_xy_step_type<View>::type rotated180_view(const View& src) { ... }

  namespace detail
  {
    // the function, wrapped inside a function object
    template <typename Result> struct rotated180_view_fn
    {
        typedef Result result_type;
        template <typename View> result_type operator()(const View& src) const
  {
            return result_type(rotated180_view(src));
        }
    };
  }

  // overloading of the function using variant. Takes and returns run-time bound view.
  // The returned view has a dynamic step
  template <typename ViewTypes> inline // Models MPL Random Access Container of models of ImageViewConcept
  typename dynamic_xy_step_type<any_image_view<ViewTypes> >::type rotated180_view(const any_image_view<ViewTypes>& src)
  {
    return apply_operation(src,detail::rotated180_view_fn<typename dynamic_xy_step_type<any_image_view<ViewTypes> >::type>());
  }

Variants should be used with caution (especially algorithms that take
more than one variant) because they instantiate the algorithm for
every possible model that the variant can take. This can take a toll
on compile time and executable size. Despite these limitations,
``variant`` is a powerful technique that allows us to combine the
speed of compile-time resolution with the flexibility of run-time
resolution. It allows us to treat images of different parameters
uniformly as a collection and store them in the same container.
