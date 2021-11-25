Tutorial: Histogram
===================

.. contents::
   :local:
   :depth: 1

This is a short tutorial presenting an example of a very simple sample of code
from an existing code base that calculates histogram of an image.
Next, the program is rewritten using GIL featres.

Original implementation
-----------------------

Actual code from a commercial software product that computes the luminosity
histogram (variable names have been changed and unrelated parts removed):

.. code-block:: cpp

  void luminosity_hist(
      std::uint8_t const* r, std::uint8_t const* g, std::uint8_t const* b,
      int rows, int cols, int sRowBytes, Histogram* hist)
  {
      for (int r = 0; r < rows; r++)
      {
          for (int c = 0; c < cols; c++)
          {
              int v = RGBToGray(r[c], g[c], b[c]); // call internal function or macro
              (*hist)[v]++;
          }
          r += sRowBytes;
          g += sRowBytes;
          b += sRowBytes;
      }
  }

Let's consider the following issues of the implementation above:

- Works only for RGB (duplicate versions exist for other color spaces)
- Works only for 8-bit images (duplicate versions exist)
- Works only for planar images

GIL implementation
------------------

.. code-block:: cpp

  template <typename GrayView, typename R>
  void grayimage_histogram(GrayView& img, R& hist)
  {
      for (typename GrayView::iterator it=img.begin(); it!=img.end(); ++it)
          ++hist[*it];
  }

  template <typename View, typename R>
  void luminosity8bit_hist(View& img, R& hist)
  {
      grayimage_histogram(color_converted_view<gray8_pixel_t>(img),hist);
  }

Using the Boost.Lambda library (or C++11 lambda) features it can written
even simpler:

.. code-block:: cpp

  using boost::lambda;

  template <typename GrayView, typename R>
  void grayimage_histogram(GrayView& img, R& hist)
  {
      for_each_pixel(img, ++var(hist)[_1]);
  }

Let's consider the following advantages of the GIL version:

- Works with any supported channel depth, color space, channel ordering
  (RGB vs BGR), and row alignment policy.
- Works for both planar and interleaved images.
- Works with new color spaces, channel depths and image types that can be
  provided in future extensions of GIL
- The second version is as efficient as the hand-coded version

Shortly, it is also very flexible.

For example, to compute the histogram of the second channel of the top left
quadrant of the image, taking every other row and column, call:

.. code-block:: cpp

  grayimage_histogram(
      nth_channel_view(
          subsampled_view(
              subimage_view(img,
                  0,0, img.width() / 2, img.height() / 2), // upper left quadrant
                  2, 2                                     // skip every other row and column
              ),
          1   // index of the second channel (for example, green for RGB)
      ),
      hist
  );

Since GIL operates on the source pixels of ``img`` object directly, no extra
memory is allocated and no images are copied.
