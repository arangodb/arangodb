/*
 * Frozen
 * Copyright 2016 QuarksLab
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef FROZEN_LETITGO_BITS_TYPE_TRAITS_H
#define FROZEN_LETITGO_BITS_TYPE_TRAITS_H

#include <type_traits>

namespace frozen {

namespace bits {

template <class...>
using void_t = void;

template <class, class, class = void_t<>>
struct has_is_transparent
{};
template <class T, class U>
struct has_is_transparent<T, U, void_t<typename T::is_transparent>>
{
  typedef void type;
};

template <class T, class U>
using has_is_transparent_t = typename has_is_transparent<T, U>::type;

} // namespace bits
} // namespace frozen

#endif
