//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_POLICY_HPP
#define XENIUM_POLICY_HPP

#include <cstdint>

namespace xenium { namespace policy {

/**
 * @brief Policy to configure the reclamation scheme to be used.
 *
 * This policy is used by the following data structures:
 *   * `michael_scott_queue`
 *   * `ramalhete_queue`
 *   * `harris_michael_list_based_set`
 *   * `harris_michael_hash_map`
 *
 * @tparam Reclaimer
 */
template <class Reclaimer>
struct reclaimer;

/**
 * @brief Policy to configure the backoff strategy.
 *
 * This policy is used by the following data structures:
 *   * `michael_scott_queue`
 *   * `ramalhete_queue`
 *   * `harris_michael_list_based_set`
 *   * `harris_michael_hash_map`
 *
 * @tparam Backoff
 */
template <class Backoff>
struct backoff;

/**
 * @brief Policy to configure the comparison function.
 *
 * This policy is used by the following data structures:
 *   * `harris_michael_list_based_set`
 *
 * @tparam Compare
 */
template <class Backoff>
struct compare;

/**
 * @brief Policy to configure the capacity of various containers.
 *
 * This policy is used by the following data structures:
 *   * `chase_work_stealing_deque`
 *
 * @tparam Value
 */
template <std::size_t Value>
struct capacity;


/**
 * @brief Policy to configure the internal container type of some
 *   data structures.
 *
 * This policy is used by the following data structures:
 *   * `chase_work_stealing_deque`
 *
 * @tparam Container
 */
template <class Container>
struct container;

/**
 * @brief Policy to configure the hash function.
 * 
 * This policy is used by the following data structures:
 *   * `harris_michael_hash_map`
 *   * `vyukov_hash_map`
 * 
 * @tparam T 
 */
template <class T>
struct hash;
}}
#endif
