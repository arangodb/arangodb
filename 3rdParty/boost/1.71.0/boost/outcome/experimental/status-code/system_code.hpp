/* Proposed SG14 status_code
(C) 2018-2019 Niall Douglas <http://www.nedproductions.biz/> (5 commits)
File Created: Feb 2018


Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.
*/

#ifndef BOOST_OUTCOME_SYSTEM_ERROR2_SYSTEM_CODE_HPP
#define BOOST_OUTCOME_SYSTEM_ERROR2_SYSTEM_CODE_HPP

#include "posix_code.hpp"

#if defined(_WIN32) || defined(BOOST_OUTCOME_STANDARDESE_IS_IN_THE_HOUSE)
#include "nt_code.hpp"
#include "win32_code.hpp"
// NOT "com_code.hpp"
#endif

BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE_BEGIN
/*! An erased-mutable status code suitably large for all the system codes
which can be returned on this system.

For Windows, these might be:

    - `com_code` (`HRESULT`)  [you need to include "com_code.hpp" explicitly for this]
    - `nt_code` (`LONG`)
    - `win32_code` (`DWORD`)

For POSIX, `posix_code` is possible.

You are guaranteed that `system_code` can be transported by the compiler
in exactly two CPU registers.
*/
using system_code = status_code<erased<intptr_t>>;

#ifndef NDEBUG
static_assert(sizeof(system_code) == 2 * sizeof(void *), "system_code is not exactly two pointers in size!");
static_assert(traits::is_move_relocating<system_code>::value, "system_code is not move relocating!");
#endif

BOOST_OUTCOME_SYSTEM_ERROR2_NAMESPACE_END

#endif
