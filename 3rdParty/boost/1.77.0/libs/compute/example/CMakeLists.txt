# ---------------------------------------------------------------------------
#  Copyright (c) 2013-2014 Kyle Lutz <kyle.r.lutz@gmail.com>
#
#  Distributed under the Boost Software License, Version 1.0
#  See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
# ---------------------------------------------------------------------------

include_directories(../include)

set(EXAMPLES
  amd_cpp_kernel
  black_scholes
  copy_data
  fizz_buzz
  hello_world
  host_sort
  inline_ptx
  longest_vector
  list_devices
  mapped_view
  memory_limits
  monte_carlo
  point_centroid
  price_cross
  print_vector
  sort_vector
  simple_kernel
  time_copy
  transform_sqrt
  vector_addition
  simple_moving_average
  matrix_transpose
)

# boost library link dependencies
set(EXAMPLE_BOOST_COMPONENTS program_options)

if (${BOOST_COMPUTE_USE_OFFLINE_CACHE})
  set(EXAMPLE_BOOST_COMPONENTS ${EXAMPLE_BOOST_COMPONENTS} system filesystem)
endif()

if(${BOOST_COMPUTE_THREAD_SAFE} AND NOT ${BOOST_COMPUTE_USE_CPP11})
  set(EXAMPLE_BOOST_COMPONENTS ${EXAMPLE_BOOST_COMPONENTS} system thread)
endif()

if(MSVC AND EXAMPLE_BOOST_COMPONENTS)
  set(EXAMPLE_BOOST_COMPONENTS ${EXAMPLE_BOOST_COMPONENTS} chrono)
endif()

if(EXAMPLE_BOOST_COMPONENTS)
  list(REMOVE_DUPLICATES EXAMPLE_BOOST_COMPONENTS)
endif()
find_package(Boost 1.54 REQUIRED COMPONENTS ${EXAMPLE_BOOST_COMPONENTS})
include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

foreach(EXAMPLE ${EXAMPLES})
  add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
  target_link_libraries(${EXAMPLE} ${OpenCL_LIBRARIES} ${Boost_LIBRARIES})

  # add example program to list of tests (if testing is enabled)
  if(${BOOST_COMPUTE_BUILD_TESTS})
    add_test("example.${EXAMPLE}" ${EXAMPLE})
  endif()
endforeach()

# opencl test example
add_executable(opencl_test opencl_test.cpp)
target_link_libraries(opencl_test ${OpenCL_LIBRARIES})

# eigen examples
if(${BOOST_COMPUTE_HAVE_EIGEN})
  find_package(Eigen REQUIRED)
  include_directories(SYSTEM ${EIGEN_INCLUDE_DIRS})
  add_executable(batched_determinant batched_determinant.cpp)
  target_link_libraries(batched_determinant ${OpenCL_LIBRARIES} ${Boost_LIBRARIES})
endif()

# opencv examples
if(${BOOST_COMPUTE_HAVE_OPENCV})
  find_package(OpenCV REQUIRED)
  include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})

  set(OPENCV_EXAMPLES
    k_means
    opencv_flip
    random_walk
    opencv_optical_flow
    opencv_convolution
    opencv_sobel_filter
    opencv_histogram
  )

  foreach(EXAMPLE ${OPENCV_EXAMPLES})
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    target_link_libraries(${EXAMPLE} ${OpenCL_LIBRARIES} ${Boost_LIBRARIES} ${OpenCV_LIBS})
  endforeach()
endif()

# opengl/vtk examples
if(${BOOST_COMPUTE_HAVE_VTK})
  find_package(VTK REQUIRED)
  include(${VTK_USE_FILE})
  add_executable(opengl_sphere opengl_sphere.cpp)
  target_link_libraries(opengl_sphere ${OpenCL_LIBRARIES} ${Boost_LIBRARIES} ${VTK_LIBRARIES})
  if(APPLE)
    target_link_libraries(opengl_sphere "-framework OpenGL")
  elseif(UNIX)
    target_link_libraries(opengl_sphere GL)
  endif()
endif()

# qt examples
if(${BOOST_COMPUTE_HAVE_QT})

  # look for Qt4 in the first place
  find_package(Qt4 QUIET)

  if(${QT4_FOUND})
    # build with Qt4
    find_package(Qt4 REQUIRED COMPONENTS QtCore QtGui QtOpenGL)
    set(QT_USE_QTOPENGL TRUE)
    include(${QT_USE_FILE})
  else()

    # look for Qt5
    find_package(Qt5Widgets QUIET)

    if(${Qt5Widgets_FOUND})
      # build with Qt5
      find_package(Qt5Core REQUIRED)
      find_package(Qt5Widgets REQUIRED)
      find_package(Qt5OpenGL REQUIRED)
      include_directories(${Qt5OpenGL_INCLUDE_DIRS})
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5OpenGL_EXECUTABLE_COMPILE_FLAGS}")
      set(QT_LIBRARIES ${Qt5OpenGL_LIBRARIES})
    else()
      # no valid Qt framework found
      message(FATAL_ERROR "Error: Did not find Qt4 or Qt5")
    endif()
  endif()

  # required by both versions
  set(CMAKE_AUTOMOC TRUE)
  include_directories(${CMAKE_CURRENT_BINARY_DIR})

  # add examples
  add_executable(qimage_blur qimage_blur.cpp)
  target_link_libraries(qimage_blur ${OpenCL_LIBRARIES} ${Boost_LIBRARIES} ${QT_LIBRARIES})

  set(QT_OPENGL_EXAMPLES
    mandelbrot
    nbody
    resize_image
  )
  foreach(EXAMPLE ${QT_OPENGL_EXAMPLES})
    add_executable(${EXAMPLE} ${EXAMPLE}.cpp)
    target_link_libraries(${EXAMPLE} ${OpenCL_LIBRARIES} ${Boost_LIBRARIES} ${QT_LIBRARIES})
    if(APPLE)
      target_link_libraries(${EXAMPLE} "-framework OpenGL")
    elseif(UNIX)
      target_link_libraries(${EXAMPLE} GL)
    endif()
  endforeach()
endif()
