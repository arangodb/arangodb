//
// Copyright (c) 2018 Manuel Pöter.
// Licensed under the MIT License. See LICENSE file in the project root for full license information.
//

#ifndef XENIUM_HASH_HPP
#define XENIUM_HASH_HPP

#include <xenium/utils.hpp>

#include <cstdint>
#include <functional>

namespace xenium {

  using hash_t = std::size_t;

  /**
   * @brief Slim wrapper around std::hash with specialization for pointer types.
   * 
   * The default instantiation simply forwards to std::hash. This functor mainly
   * exists to allow the specialization for pointer types (see xenium::hash< Key * >).
   */
  template <class Key>
  struct hash {
    hash_t operator()(const Key& key) const noexcept { return _hash(key); }
  private:
    std::hash<Key> _hash;
  };

  /**
   * @brief Specialized hash functor for pointer types to get rid of the alignment.
   * 
   * Pointers are usually aligned, with the alignment depending on the type they
   * point to. This means that the lower bits of the pointer are zero.
   * Many hash maps calculate the bucket index using a modulo by a power of two,
   * because this is simply a logical `and` operation. However, by using this
   * approach with aligned pointers the lower buckets never get populated. To
   * avoid this problem and achieve a better distribution in such hash maps, this
   * specialization shifts the `size_t` representation of the pointer to the right
   * by \a X bits, where \a X is calculated based on the alignment of `Key`.
   */
  template <class Key>
  struct hash<Key*> {
    hash_t operator()(const Key* key) const noexcept {
      constexpr auto alignment = std::alignment_of<Key>();
      constexpr auto shift = utils::find_last_bit_set(alignment) - 1;
      auto hash = reinterpret_cast<hash_t>(key);
      assert((hash >> shift) << shift == hash); // we must not loose a bit!
      return hash >> shift;
    }
  };
}

#endif
