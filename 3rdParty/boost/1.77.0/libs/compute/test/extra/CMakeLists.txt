# ---------------------------------------------------------------------------
#  Copyright (c) 2015 Kyle Lutz <kyle.r.lutz@gmail.com>
#
#  Distributed under the Boost Software License, Version 1.0
#  See accompanying file LICENSE_1_0.txt or copy at
#  http://www.boost.org/LICENSE_1_0.txt
#
# ---------------------------------------------------------------------------

# include local test headers
include_directories(..)

# Check for linkage problems
add_executable(test_multiple_objects
    test_multiple_objects1.cpp
    test_multiple_objects2.cpp
    )
target_link_libraries(test_multiple_objects
    ${OPENCL_LIBRARIES}
    ${Boost_LIBRARIES}
    )
# link with coverage library
if(${BOOST_COMPUTE_ENABLE_COVERAGE} AND ${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  target_link_libraries(test_multiple_objects -fprofile-arcs -ftest-coverage)
endif()
add_test("misc.multiple_objects" test_multiple_objects)

# eigen interop tests
if(${BOOST_COMPUTE_HAVE_EIGEN})
  find_package(Eigen REQUIRED)
  include_directories(SYSTEM ${EIGEN_INCLUDE_DIRS})
  add_compute_test("interop.eigen" test_interop_eigen.cpp)
endif()

# opencv interop tests
if(${BOOST_COMPUTE_HAVE_OPENCV})
  find_package(OpenCV REQUIRED)
  include_directories(SYSTEM ${OpenCV_INCLUDE_DIRS})
  add_compute_test("interop.opencv" test_interop_opencv.cpp)
  target_link_libraries(test_interop_opencv ${OpenCV_LIBS})
endif()

# qt interop tests
if(${BOOST_COMPUTE_HAVE_QT})
  # look for Qt4 in the first place
  find_package(Qt4 QUIET)

  if(${QT4_FOUND})
    find_package(Qt4 REQUIRED COMPONENTS QtCore QtGui QtOpenGL)
    include(${QT_USE_FILE})
  else()
    find_package(Qt5Widgets QUIET)

    # look for Qt5
    if(${Qt5Widgets_FOUND})
      find_package(Qt5Core REQUIRED)
      find_package(Qt5Widgets REQUIRED)
      find_package(Qt5OpenGL REQUIRED)
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${Qt5OpenGL_EXECUTABLE_COMPILE_FLAGS}")
      set(QT_LIBRARIES ${Qt5OpenGL_LIBRARIES})
    else()
      # no valid Qt framework found
      message(FATAL_ERROR "Error: Did not find Qt4 or Qt5")
    endif()
  endif()

  add_compute_test("interop.qt" test_interop_qt.cpp)
  target_link_libraries(test_interop_qt ${QT_LIBRARIES})

  # the opengl interop test depends on qt to create the opengl context
  add_compute_test("interop.opengl" test_interop_opengl.cpp)
  target_link_libraries(test_interop_opengl ${QT_LIBRARIES})
endif()

# vtk interop tests
if(${BOOST_COMPUTE_HAVE_VTK})
  find_package(VTK REQUIRED)
  include(${VTK_USE_FILE})
  add_compute_test("interop.vtk" test_interop_vtk.cpp)
  target_link_libraries(test_interop_vtk ${VTK_LIBRARIES})
endif()
