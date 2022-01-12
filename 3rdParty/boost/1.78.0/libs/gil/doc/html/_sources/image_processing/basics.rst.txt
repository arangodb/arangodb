Basics
------

Here are basic concepts that might help to understand documentation
written in this folder:

Convolution
~~~~~~~~~~~

The simplest way to look at this is "tweaking the input so that it would
look like the shape provided". What exact tweaking is applied depends on
the kernel.

--------------

Filters, kernels, weights
~~~~~~~~~~~~~~~~~~~~~~~~~

Those three words usually mean the same thing, unless context is clear
about a different usage. Simply put, they are matrices, that are used to
achieve certain effects on the image. Lets consider a simple one, 3 by 3
Scharr filter

``ScharrX = [1,0,-1][1,0,-1][1,0,-1]``

The filter above, when convolved with a single channel image
(intensity/luminance strength), will produce a gradient in X
(horizontal) direction. There is filtering that cannot be done with a
kernel though, and one good example is median filter (mean is the
arithmetic mean, whereas median will be the center element of a sorted
array).

--------------

Derivatives
~~~~~~~~~~~

A derivative of an image is a gradient in one of two directions: x
(horizontal) and y (vertical). To compute a derivative, one can use
Scharr, Sobel and other gradient filters.

--------------

Curvature
~~~~~~~~~

The word, when used alone, will mean the curvature that would be
generated if values of an image would be plotted in 3D graph. X and Z
axises (which form horizontal plane) will correspond to X and Y indices
of an image, and Y axis will correspond to value at that pixel. By
little stretch of an imagination, filters (another names are kernels,
weights) could be considered an image (or any 2D matrix). A mean filter
would draw a flat plane, whereas Gaussian filter would draw a hill that
gets sharper depending on it's sigma value.
