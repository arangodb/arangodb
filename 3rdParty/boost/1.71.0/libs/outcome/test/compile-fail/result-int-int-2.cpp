/* clang-format off
(use of deleted function|call to deleted constructor|attempting to reference a deleted function)
clang-format on


Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License in the accompanying file
Licence.txt or at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


Distributed under the Boost Software License, Version 1.0.
(See accompanying file Licence.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
*/

#include "../../include/boost/outcome/result.hpp"

int main()
{
  using namespace BOOST_OUTCOME_V2_NAMESPACE;
  // Must not be possible to initialise a result with same R and S types
  result<int, int> m(in_place_type<int>);
  return 0;
}
