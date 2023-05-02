# MIT License
# 
# Copyright (c) 2018 vector-of-bool
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

set(_prev "${YARN_EXECUTABLE}")
find_program(YARN_EXECUTABLE yarn DOC "Path to Yarn, the better NPM")

if(YARN_EXECUTABLE)
    if(NOT _prev)
        message(STATUS "Found Yarn executable: ${YARN_EXECUTABLE}")
    endif()
    set(Yarn_FOUND TRUE CACHE INTERNAL "")
else()
    set(Yarn_FOUND FALSE CACHE INTERNAL "")
    if(Yarn_FIND_REQUIRED)
        message(FATAL_ERROR "Failed to find a Yarn executable")
    endif()
endif()