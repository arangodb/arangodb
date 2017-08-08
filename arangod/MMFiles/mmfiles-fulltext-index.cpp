////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "mmfiles-fulltext-index.h"

#include "Basics/locks.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "MMFiles/mmfiles-fulltext-handles.h"
#include "MMFiles/mmfiles-fulltext-list.h"
#include "MMFiles/mmfiles-fulltext-query.h"
#include "MMFiles/mmfiles-fulltext-result.h"
#include "StorageEngine/DocumentIdentifierToken.h"

/// @brief use padding for pointers in binary data
#ifndef TRI_UNALIGNED_ACCESS
// must properly align memory on some architectures to prevent
// unaligned memory accesses
#define FULLTEXT_PADDING 1
#else
#undef FULLTEXT_PADDING
#endif

/// @brief maximum length of an indexed word in bytes
/// a UTF-8 character can contain up to 4 bytes
#define MAX_WORD_BYTES ((TRI_FULLTEXT_MAX_WORD_LENGTH)*4)

/// @brief the type of characters indexed. should be one byte long
typedef uint8_t node_char_t;

/// @brief typedef for follower nodes. this is just void because for the
/// compiler it is a sequence of binary data with no internal structure
typedef void followers_t;

/// @brief a node in the fulltext index
///
/// the _followers property is a pointer to dynamic memory. If it is NULL, then
/// the node does not have any followers/sub-nodes. if the _followers property
/// is non-NULL, it contains a byte stream consisting of the following values:
/// - uint8_t numAllocated: number of sub-nodes we have allocated memory for
/// - uint8_t numFollowers: the actual number of sub-nodes for the node
/// - node_char_t* keys: keys of sub-nodes, sorted binary
/// - node_t** sub-nodes: pointers to sub-nodes, in the same order as keys
/// this structure is fiddly, but saves a lot of memory and malloc calls when
/// compared to a "proper" structure.
/// As the "properties" inside _followers are just binary data for the compiler,
/// it is not wise to access them directly, but use the access functions this
/// file provides. There is no need to calculate the offsets of the different
/// sub-properties directly, as this is all done by special functions which
/// provide the offsets at relatively low costs.
///
/// The _handles property is a pointer to dynamic memory, too. If it is NULL,
/// then the node does not have any handles attached. If it is non-NULL, it
/// contains a byte stream consisting of the following values:
/// - uint32_t numAllocated: number of handles allocated for the node
/// - unit32_t numEntries: number of handles currently in use
/// - TRI_fulltext_handle_t* handles: all the handle values subsequently
/// Note that the highest bit of the numAllocated value contains a flag whether
/// the handles list is sorted or not. It is therefore not safe to access the
/// properties directly, but instead always the special functions provided in
/// fulltext-list.c must be used. These provide access to the individual values
/// at relatively low cost
typedef struct node_s {
  followers_t* _followers;
  TRI_fulltext_list_t* _handles;
} node_t;

/// @brief the actual fulltext index
struct index__t {
  node_t* _root;  // root node of the index

  TRI_fulltext_handles_t* _handles;  // handles management instance

  arangodb::basics::ReadWriteLock _lock;

  size_t _memoryAllocated;  // total memory used by index
#if TRI_FULLTEXT_DEBUG
  size_t _memoryBase;        // base memory
  size_t _memoryNodes;       // total memory used by nodes (node_t only)
  size_t _memoryFollowers;   // total memory used for followers (no documents)
  uint32_t _nodesAllocated;  // number of nodes currently in use
#endif

  uint32_t _nodeChunkSize;       // how many sub-nodes to allocate per chunk
  uint32_t _initialNodeHandles;  // how many handles to allocate per node
};

static uint32_t NodeNumFollowers(const node_t* const);

static uint32_t NodeNumAllocated(const node_t* const);

static node_char_t* NodeFollowersKeys(const node_t* const);

static node_t** NodeFollowersNodes(const node_t* const);

static void FreeFollowers(index__t* const, node_t*);

static void FreeNode(index__t* const, node_t*);

static size_t MemorySubNodeList(uint32_t);

/// @brief print some indentation
#if TRI_FULLTEXT_DEBUG
void Indent(uint32_t level) {
  uint32_t i;

  for (i = 0; i < level; ++i) {
    printf("  ");
  }
}
#endif

/// @brief dump the contents of a node
#if TRI_FULLTEXT_DEBUG
void DumpNode(const node_t* const node, uint32_t level) {
  uint32_t numFollowers;
  uint32_t numHandles;
  uint32_t i;

  numFollowers = NodeNumFollowers(node);
  if (node->_handles != nullptr) {
    numHandles = TRI_NumEntriesListMMFilesFulltextIndex(node->_handles);
  } else {
    numHandles = 0;
  }

  if (numFollowers == 0) {
    printf(" (x) ");
  } else {
    printf("     ");
  }

  if (level < 20) {
    Indent(20 - level);
  }
  printf("node %p (%lu followers, %lu handles)\n", node,
         (unsigned long)numFollowers, (unsigned long)numHandles);

  if (numFollowers > 0) {
    node_char_t* followerKeys = NodeFollowersKeys(node);
    node_t** followerNodes = NodeFollowersNodes(node);

    for (i = 0; i < numFollowers; ++i) {
      node_char_t followerKey = followerKeys[i];
      node_t* followerNode = followerNodes[i];

      Indent(level);
      printf("%c", (char)followerKey);
      DumpNode(followerNode, level + 1);
    }
  }

  if (numHandles > 0) {
    Indent(level);
    if (level < 20) {
      Indent(20 - level);
    }
    printf("(");
    TRI_DumpListMMFilesFulltextIndex(node->_handles);

    printf(")\n");
  }
}
#endif

/// @brief return the padding to be applied when allocating memory for the
/// sub-node list. the padding is done between the (uint8_t) keys and the
/// (node_t*) pointers. padding can be used to align the pointers to some
/// "good" boundary
static inline size_t Padding(uint32_t numEntries) {
#ifdef FULLTEXT_PADDING
  size_t const PAD = 8;
  size_t offset = sizeof(uint8_t) +                    // numAllocated
                  sizeof(uint8_t) +                    // numUsed
                  (sizeof(node_char_t) * numEntries);  // followerKeys

  if (offset % PAD == 0) {
    // already aligned
    return 0;
  } else {
    // not aligned, apply padding
    return PAD - (offset % PAD);
  }
#else
  return 0;
#endif
}

/// @brief re-allocate memory for the index and update memory usage statistics
static inline void* ReallocateMemory(index__t* const idx, void* old,
                                     size_t const newSize,
                                     size_t const oldSize) {
  void* data;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(old != nullptr);
  TRI_ASSERT(newSize > 0);
  TRI_ASSERT(oldSize > 0);
#endif

  data = TRI_Reallocate(TRI_UNKNOWN_MEM_ZONE, old, newSize);
  if (data != nullptr) {
    idx->_memoryAllocated += newSize;
    idx->_memoryAllocated -= oldSize;
  }
  return data;
}

/// @brief allocate memory for the index and update memory usage statistics
static inline void* AllocateMemory(index__t* const idx, size_t const size) {
  void* data;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(size > 0);
#endif

  data = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, size);
  if (data != nullptr) {
    idx->_memoryAllocated += size;
  }
  return data;
}

/// @brief free memory and update memory usage statistics
static inline void FreeMemory(index__t* const idx, void* data,
                              size_t const size) {
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(size > 0);
  TRI_ASSERT(idx->_memoryAllocated >= size);
#endif

  idx->_memoryAllocated -= size;
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);
}

/// @brief adjust the number of followers for a node
/// note: if the value is set to 0, this might free the sub-nodes list
static inline void SetNodeNumFollowers(index__t* const idx, node_t* const node,
                                       uint32_t value) {
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->_followers != nullptr);
  TRI_ASSERT(value <= 255);
#endif

  // note: value must be <= current number of followers
  if (value == 0) {
    // new value is 0, now free old sub-nodes list (if any)
    uint32_t numAllocated = NodeNumAllocated(node);

#if TRI_FULLTEXT_DEBUG
    idx->_memoryFollowers -= MemorySubNodeList(numAllocated);
#endif
    FreeMemory(idx, node->_followers, MemorySubNodeList(numAllocated));
    node->_followers = nullptr;
  } else {
    // value is not 0, now write the new value
    uint8_t* head = (uint8_t*)node->_followers;
    *(++head) = (uint8_t)value;
  }
}

/// @brief get the number of used sub-nodes in a sub node list
static inline uint32_t NodeNumFollowers(node_t const* node) {
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  if (node == nullptr || node->_followers == nullptr) {
    return 0;
  }

  uint8_t* head = (uint8_t*)node->_followers;
  return (uint32_t) * (++head);
}

/// @brief get the number of allocated (not necessarily used) sub-nodes in a
/// sub node list
static uint32_t NodeNumAllocated(const node_t* const node) {
  uint8_t* head;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  if (node->_followers == nullptr) {
    return 0;
  }

  head = (uint8_t*)node->_followers;
  return (uint32_t)*head;
}

/// @brief initialize a sub-node list with length information
static void InitializeSubNodeList(void* data, uint32_t numAllocated,
                                  uint32_t numFollowers) {
  uint8_t* head = (uint8_t*)data;

  *(head++) = (uint8_t)numAllocated;
  *head = (uint8_t)numFollowers;
}

/// @brief get a pointer to the start of the keys in a sub-node list
/// the caller must make sure the node actually has sub-nodes
static inline node_char_t* FollowersKeys(void* data) {
  uint8_t* head = (uint8_t*)data;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(data != nullptr);
#endif

  return (node_char_t*)(head + 2);  // numAllocated + numEntries
}

/// @brief get a pointer to the start of the keys in a sub-node list
/// the caller must make sure the node actually has sub-nodes
static inline node_char_t* NodeFollowersKeys(const node_t* const node) {
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node->_followers != nullptr);
#endif

  return FollowersKeys(node->_followers);
}

/// @brief get a pointer to the start of the node-list in a sub-node list
/// the caller must make sure the node actually has sub-nodes
static inline node_t** FollowersNodes(void* data) {
  uint8_t* head = (uint8_t*)data;
  uint8_t numAllocated = *head;
  uint8_t* keys = (uint8_t*)(head + 2);  // numAllocated + numEntries

  return reinterpret_cast<node_t**>(keys + numAllocated + Padding(numAllocated));
}

/// @brief get a pointer to the start of the node-list in a sub-node list
/// the caller must make sure the node actually has sub-nodes
static inline node_t** NodeFollowersNodes(const node_t* const node) {
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(node->_followers != nullptr);
#endif

  return FollowersNodes(node->_followers);
}

/// @brief get a pointer to the start of the node-list in a sub-node list
/// the caller must make sure the node actually has sub-nodes
static inline node_t** FollowersNodesPos(void* data, uint32_t numAllocated) {
  uint8_t* head = (uint8_t*)data;
  uint8_t* keys = (uint8_t*)(head + 2);  // numAllocated + numEntries

  return reinterpret_cast<node_t**>(keys + numAllocated + Padding(numAllocated));
}

/// @brief return the memory required to store a sub-node list of the
/// specific length
static size_t MemorySubNodeList(uint32_t numEntries) {
  return sizeof(uint8_t) +  // numAllocated
         sizeof(uint8_t) +  // numEntries
         ((sizeof(node_char_t) + sizeof(node_t*)) *
          numEntries) +  // follower keys & nodes
         Padding(numEntries);
}

/// @brief check the current list of sub-nodes for a node and increase its
/// size if it is too small to hold another node
static bool ExtendSubNodeList(index__t* const idx, node_t* const node,
                              uint32_t numFollowers, uint32_t numAllocated) {
  size_t nextSize;
  uint32_t nextAllocated;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  // current list has reached its limit, we must increase it

  nextAllocated = numAllocated + idx->_nodeChunkSize;
  nextSize = MemorySubNodeList(nextAllocated);

  if (node->_followers == nullptr) {
    // allocate a new list
    node->_followers = AllocateMemory(idx, nextSize);
    if (node->_followers == nullptr) {
      // out of memory
      return false;
    }

    // initialize the chunk of memory we just got
    InitializeSubNodeList(node->_followers, nextAllocated, numFollowers);
#if TRI_FULLTEXT_DEBUG
    idx->_memoryFollowers += nextSize;
#endif
    return true;
  } else {
    // re-allocate an existing list
    followers_t* followers;
    size_t oldSize;

    oldSize = MemorySubNodeList(numAllocated);

    followers = ReallocateMemory(idx, node->_followers, nextSize, oldSize);
    if (followers == nullptr) {
      // out of memory
      return false;
    }

    // initialize the chunk of memory we just got
    InitializeSubNodeList(followers, nextAllocated, numFollowers);
#if TRI_FULLTEXT_DEBUG
    idx->_memoryFollowers += nextSize;
    idx->_memoryFollowers -= oldSize;
#endif

    // note the new pointer
    node->_followers = followers;

    if (numFollowers > 0) {
      // copy existing sub-nodes into the new sub-nodes list
      memmove(FollowersNodes(followers),
              FollowersNodesPos(followers, numAllocated),
              sizeof(node_t*) * numFollowers);
    }

    return true;
  }
}

/// @brief create a new, empty node
static node_t* CreateNode(index__t* const idx) {
  node_t* node = static_cast<node_t*>(AllocateMemory(idx, sizeof(node_t)));

  if (node == nullptr) {
    return nullptr;
  }

  node->_followers = nullptr;
  node->_handles = nullptr;

#if TRI_FULLTEXT_DEBUG
  idx->_nodesAllocated++;
  idx->_memoryNodes += sizeof(node_t);
#endif

  return node;
}

/// @brief free a node's follower nodes
static void FreeFollowers(index__t* const idx, node_t* node) {
  uint32_t numFollowers;
  uint32_t numAllocated;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  if (node->_followers == nullptr) {
    return;
  }

  numFollowers = NodeNumFollowers(node);

  if (numFollowers > 0) {
    node_t** followerNodes;
    uint32_t i;

    followerNodes = NodeFollowersNodes(node);
    for (i = 0; i < numFollowers; ++i) {
      FreeNode(idx, followerNodes[i]);
    }
  }

  numAllocated = NodeNumAllocated(node);
#if TRI_FULLTEXT_DEBUG
  idx->_memoryFollowers -= MemorySubNodeList(numAllocated);
#endif
  FreeMemory(idx, node->_followers, MemorySubNodeList(numAllocated));

  node->_followers = nullptr;
}

/// @brief free a node in the index
static void FreeNode(index__t* const idx, node_t* node) {
  if (node == nullptr) {
    return;
  }

  if (node->_handles != nullptr) {
    // free handles
    idx->_memoryAllocated -= TRI_MemoryListMMFilesFulltextIndex(node->_handles);
    TRI_FreeListMMFilesFulltextIndex(node->_handles);
  }

  // free followers
  if (node->_followers != nullptr) {
    FreeFollowers(idx, node);
  }

  // free node itself
  FreeMemory(idx, node, sizeof(node_t));
#if TRI_FULLTEXT_DEBUG
  idx->_memoryNodes -= sizeof(node_t);
  idx->_nodesAllocated--;
#endif
}

/// @brief recursively cleanup nodes (used during compaction)
/// the map contains a rewrite-map of document handles
static bool CleanupNodes(index__t* idx, node_t* node, void* map) {
  bool isActive;

  // assume we can delete the node we are processing
  // we may set this flag to true further down below if we find the node is
  // still useful
  isActive = false;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  if (node->_followers != nullptr) {
    // recurse into sub-nodes
    node_char_t* followerKeys = NodeFollowersKeys(node);
    node_t** followerNodes = NodeFollowersNodes(node);
    uint32_t numFollowers;
    uint32_t i, j;

    numFollowers = NodeNumFollowers(node);

    j = 0;
    // traverse over all follower nodes and during that rewrite the
    // node's follower list with only the followers that are still in
    // use. this will delete all unused subnodes from the node's follower
    // list, leaving the node's follower list potentially empty
    for (i = 0; i < numFollowers; ++i) {
      node_t* follower;

      follower = followerNodes[i];
#if TRI_FULLTEXT_DEBUG
      TRI_ASSERT(follower != nullptr);
#endif

      // recursively clean up sub-nodes
      if (!CleanupNodes(idx, follower, map)) {
        // the sub-node is empty, kill it!
        FreeNode(idx, follower);
        // and go to next follower
        continue;
      }

      // sub-node is still relevant
      isActive = true;

      if (i != j) {
#if TRI_FULLTEXT_DEBUG
        TRI_ASSERT(i > j);
#endif

        // move nodes
        followerKeys[j] = followerKeys[i];
        followerNodes[j] = followerNodes[i];
      }

      ++j;
    }

    if (i != j) {
      // number of followers has changed
      // this might delete the memory for the followers!
      SetNodeNumFollowers(idx, node, j);
    }
  }

  // rewrite the node's handle list if present
  if (node->_handles != nullptr) {
    uint32_t remain;

    remain = TRI_RewriteListMMFilesFulltextIndex(node->_handles, map);
    if (remain > 0) {
      // there are still handles left in the rewritten handles list
      // we must keep this node
      isActive = true;
    } else {
      // no handles left, we can delete the node's handle list
      idx->_memoryAllocated -= TRI_MemoryListMMFilesFulltextIndex(node->_handles);
      TRI_FreeListMMFilesFulltextIndex(node->_handles);
      node->_handles = nullptr;
    }
  }

  return isActive;
}

/// @brief find a sub-node of a node with only one sub-node
/// the caller must make sure the node actually has exactly one sub-node!
static inline node_t* FindDirectSubNodeSingle(const node_t* const node,
                                              const node_char_t c) {
  node_char_t* followerKeys;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
  TRI_ASSERT(NodeNumFollowers(node) == 1);
#endif

  followerKeys = NodeFollowersKeys(node);

  if (followerKeys[0] == c) {
    node_t** followerNodes = NodeFollowersNodes(node);

    return followerNodes[0];
  }

  return nullptr;
}

/// @brief find a sub-node of a node using a linear search
/// this will compare the node's follower characters with the character passed
/// followers are sorted so it will stop at the first character that is higher
/// than the character passed
/// the caller must make sure the node actually has sub-nodes!
static inline node_t* FindDirectSubNodeLinear(const node_t* const node,
                                              const node_char_t c) {
  node_char_t* followerKeys;
  uint32_t numFollowers;
  uint32_t i;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  numFollowers = NodeNumFollowers(node);
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(numFollowers >= 1);
#endif

  followerKeys = NodeFollowersKeys(node);

  for (i = 0; i < numFollowers; ++i) {
    node_char_t followerKey = followerKeys[i];

    if (followerKey > c) {
      // we're are already beyond of what we look for
      break;  // trampoline to return NULL
    } else if (followerKey == c) {
      node_t** followerNodes = NodeFollowersNodes(node);

      return followerNodes[i];
    }
  }

  return nullptr;
}

/// @brief find a sub-node of a node using a binary search
/// the caller must ensure the node actually has sub-nodes!
static node_t* FindDirectSubNodeBinary(const node_t* const node,
                                       const node_char_t c) {
  node_char_t* followerKeys;
  node_t** followerNodes;
  uint32_t numFollowers;
  uint32_t l, r;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  numFollowers = NodeNumFollowers(node);
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(numFollowers >= 1);
#endif

  followerKeys = NodeFollowersKeys(node);
  followerNodes = NodeFollowersNodes(node);

  l = 0;
  // this is safe (look at the function documentation)
  r = numFollowers - 1;

  while (true) {
    node_char_t followerKey;
    uint32_t m;

    // determine midpoint
    m = l + ((r - l) / 2);
    followerKey = followerKeys[m];
    if (followerKey == c) {
      return followerNodes[m];
    }

    if (followerKey > c) {
      if (m == 0) {
        // we must abort because the following subtraction would
        // make the uin32_t underflow to UINT32_MAX!
        return nullptr;
      }
      // this is safe
      r = m - 1;
    } else {
      l = m + 1;
    }

    if (r < l) {
      return nullptr;
    }
  }
}

/// @brief find a node's sub-node, identified by its start character
static inline node_t* FindDirectSubNode(const node_t* const node,
                                        const node_char_t c) {
  uint32_t numFollowers;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  numFollowers = NodeNumFollowers(node);

  if (numFollowers >= 8) {
    return FindDirectSubNodeBinary(node, c);
  } else if (numFollowers > 1) {
    return FindDirectSubNodeLinear(node, c);
  } else if (numFollowers == 1) {
    return FindDirectSubNodeSingle(node, c);
  }

  return nullptr;
}

/// @brief find a node by its key, starting from the index root
static node_t* FindNode(const index__t* idx, char const* const key,
                        size_t const keyLength) {
  node_t* node;
  node_char_t* p;
  size_t i;

  node = (node_t*)idx->_root;
#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif
  p = (node_char_t*)key;

  for (i = 0; i < keyLength; ++i) {
    node = FindDirectSubNode(node, *(p++));
    if (node == nullptr) {
      return nullptr;
    }
  }

  return node;
}

/// @brief create a list with the handles of a node
static TRI_fulltext_list_t* GetDirectNodeHandles(const node_t* const node) {
  return TRI_CloneListMMFilesFulltextIndex(node->_handles);
}

/// @brief recursively merge node and sub-node handles into the result list
static TRI_fulltext_list_t* MergeSubNodeHandles(const node_t* const node,
                                                TRI_fulltext_list_t* list) {
  node_t** followerNodes;
  uint32_t numFollowers;
  uint32_t i;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  numFollowers = NodeNumFollowers(node);
  if (numFollowers == 0) {
    return list;
  }

  followerNodes = NodeFollowersNodes(node);

  for (i = 0; i < numFollowers; ++i) {
    node_t* follower;

    follower = followerNodes[i];
#if TRI_FULLTEXT_DEBUG
    TRI_ASSERT(follower != nullptr);
#endif
    if (follower->_handles != nullptr) {
      // OR-merge the follower node's documents with what we already have found
      list =
          TRI_UnioniseListMMFilesFulltextIndex(list, GetDirectNodeHandles(follower));
      if (list == nullptr) {
        return nullptr;
      }
    }

    // recurse into sub-nodes
    list = MergeSubNodeHandles(follower, list);
    if (list == nullptr) {
      return nullptr;
    }
  }

  return list;
}

/// @brief recursively create a result list with the handles of a node and
/// all of its sub-nodes
static inline TRI_fulltext_list_t* GetSubNodeHandles(const node_t* const node) {
  TRI_fulltext_list_t* list;

  list = GetDirectNodeHandles(node);
  return MergeSubNodeHandles(node, list);
}

/// @brief insert a new sub-node underneath an existing node
/// the caller must make sure that the node already has memory allocated for
/// the _followers property
static node_t* InsertSubNode(index__t* const idx, node_t* const node,
                             uint32_t position, const node_char_t key) {
  node_t** followerNodes;
  node_char_t* followerKeys;
  node_t* subNode;
  uint32_t numFollowers;
  uint32_t moveCount;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  // create the sub-node
  subNode = CreateNode(idx);
  if (subNode == nullptr) {
    // out of memory
    return nullptr;
  }

  numFollowers = NodeNumFollowers(node);
  followerKeys = NodeFollowersKeys(node);
  followerNodes = NodeFollowersNodes(node);

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(numFollowers >= position);
#endif

  // we have to move this many elements
  moveCount = numFollowers - position;

  if (moveCount > 0) {
    // make room for the new node
    memmove(followerKeys + position + 1, followerKeys + position,
            moveCount * sizeof(node_char_t));
    memmove(followerNodes + position + 1, followerNodes + position,
            moveCount * sizeof(node_t*));
  }

  // register the new sub node
  followerNodes[position] = subNode;
  followerKeys[position] = key;
  SetNodeNumFollowers(idx, node, numFollowers + 1);

  return subNode;
}

/// ensure that a specific sub-node (with a specific key) is there
/// if it is not there, it will be created by this function
static node_t* EnsureSubNode(index__t* const idx, node_t* node,
                             const node_char_t c) {
  uint32_t numFollowers;
  uint32_t numAllocated;
  uint32_t i;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  // search the node and find the correct insert position if it does not exist
  numFollowers = NodeNumFollowers(node);

  if (numFollowers > 0) {
    // linear search
    node_char_t* followerKeys;

    followerKeys = NodeFollowersKeys(node);
    // divide the search space in 2 halves
    uint32_t start;
    if (numFollowers >= 8 && followerKeys[numFollowers / 2] < c) {
      start = numFollowers / 2;
    } else {
      start = 0;
    }

    for (i = start; i < numFollowers; ++i) {
      node_char_t followerKey;

      followerKey = followerKeys[i];

      if (followerKey > c) {
        // we have found a key beyond what we're looking for. abort the search
        // i now contains the correct insert position
        break;
      } else if (followerKey == c) {
        // found the node, return it
        node_t** followerNodes = NodeFollowersNodes(node);

        return followerNodes[i];
      }
    }
  } else {
    // no followers yet. insert position is 0
    i = 0;
  }

  // must insert a new node

  numAllocated = NodeNumAllocated(node);

  // we'll be doing an insert. make sure the node has enough space for
  // containing
  // a list with one element more
  if (numFollowers >= numAllocated) {
    if (!ExtendSubNodeList(idx, node, numFollowers, numAllocated)) {
      // out of memory
      return nullptr;
    }
  }

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node->_followers != nullptr);
#endif

  return InsertSubNode(idx, node, i, c);
}

/// insert a handle for a node
static bool InsertHandle(index__t* const idx, node_t* const node,
                         const TRI_fulltext_handle_t handle) {
  TRI_fulltext_list_t* list;
  TRI_fulltext_list_t* oldList;
  size_t oldAlloc;

#if TRI_FULLTEXT_DEBUG
  TRI_ASSERT(node != nullptr);
#endif

  if (node == nullptr) {
    return false;
  }

  if (node->_handles == nullptr) {
    // node does not yet have any handles. now allocate a new chunk of handles
    node->_handles = TRI_CreateListMMFilesFulltextIndex(idx->_initialNodeHandles);

    if (node->_handles != nullptr) {
      idx->_memoryAllocated += TRI_MemoryListMMFilesFulltextIndex(node->_handles);
    }
  }

  if (node->_handles == nullptr) {
    // out of memory
    return false;
  }

  oldList = node->_handles;
  oldAlloc = TRI_MemoryListMMFilesFulltextIndex(oldList);

  // adding to the list might change the list pointer!
  list = TRI_InsertListMMFilesFulltextIndex(node->_handles, handle);
  if (list == nullptr) {
    // out of memory
    return false;
  }

  if (list != oldList) {
    // the insert might have changed the pointer
    node->_handles = list;
    idx->_memoryAllocated += TRI_MemoryListMMFilesFulltextIndex(list);
    idx->_memoryAllocated -= oldAlloc;
  }

  return true;
}

/// turn a handle list into a proper document list result
/// this will also exclude all deleted documents
static TRI_fulltext_result_t* MakeListResult(index__t* const idx,
                                             TRI_fulltext_list_t* list,
                                             size_t maxResults) {
  TRI_fulltext_result_t* result;
  TRI_fulltext_list_entry_t* listEntries;
  uint32_t numResults;
  uint32_t originalNumResults;
  uint32_t i, pos;

  if (list == nullptr) {
    return nullptr;
  }

  // we have a list of handles
  // now turn the handles into documents and exclude deleted ones on the fly
  numResults = TRI_NumEntriesListMMFilesFulltextIndex(list);
  originalNumResults = numResults;
  if (static_cast<size_t>(numResults) > maxResults && maxResults > 0) {
    // cap the number of results
    numResults = static_cast<uint32_t>(maxResults);
  }
  result = TRI_CreateResultMMFilesFulltextIndex(numResults);
  if (result == nullptr) {
    TRI_FreeListMMFilesFulltextIndex(list);
    return nullptr;
  }

  pos = 0;
  listEntries = TRI_StartListMMFilesFulltextIndex(list);

  for (i = 0; i < originalNumResults; ++i) {
    TRI_fulltext_handle_t handle;

    handle = listEntries[i];
    arangodb::DocumentIdentifierToken doc =
        TRI_GetDocumentMMFilesFulltextIndex(idx->_handles, handle);

    if (doc == 0) {
      // deleted document
      continue;
    }

    result->_documents[pos++] = doc;
    if (pos >= numResults) {
      break;
    }
  }

  result->_numDocuments = pos;

  // don't need the list anymore
  TRI_FreeListMMFilesFulltextIndex(list);

  return result;
}

/// @brief find all documents from the index that match the key
#if 0
TRI_fulltext_result_t* FindDocuments (index__t* const idx,
                                      char const* const key,
                                      size_t const keyLength,
                                      bool const recursive) {
  node_t* node;
  TRI_fulltext_list_t* list;

  node = FindNode(idx, key, keyLength);
  if (node == nullptr) {
    // not found, create empty result
    return TRI_CreateResultMMFilesFulltextIndex(0);
  }

  if (recursive) {
    // prefix matching
    list = GetSubNodeHandles(node);
  }
  else {
    // complete match
    list = GetDirectNodeHandles(node);
  }

  return MakeListResult(idx, list, 0);
}
#endif

/// @brief determine the common prefix length of two words
static inline size_t CommonPrefixLength(std::string const& left,
                                        std::string const& right) {
  char const* lhs = left.c_str();
  char const* rhs = right.c_str();
  size_t length = 0;

  for (; *lhs && *rhs && *lhs == *rhs; ++lhs, ++rhs, ++length)
    ;

  return length;
}

/// @brief create the fulltext index
TRI_fts_index_t* TRI_CreateFtsIndex(uint32_t handleChunkSize,
                                    uint32_t nodeChunkSize,
                                    uint32_t initialNodeHandles) {
  auto idx = std::make_unique<index__t>();

  idx->_memoryAllocated = sizeof(index__t);
#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase = sizeof(index__t);
  idx->_memoryNodes = 0;
  idx->_memoryFollowers = 0;
  idx->_nodesAllocated = 0;
#endif
  // how many followers to allocate at once
  idx->_nodeChunkSize = nodeChunkSize;
  // how many handles to create per node by default
  idx->_initialNodeHandles = initialNodeHandles;

  // create the root node
  idx->_root = CreateNode(idx.get());
  if (idx->_root == nullptr) {
    // out of memory
    return nullptr;
  }

  // create an instance for managing document handles
  idx->_handles = TRI_CreateHandlesMMFilesFulltextIndex(handleChunkSize);
  if (idx->_handles == nullptr) {
    // out of memory
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx->_root);
    return nullptr;
  }

  idx->_memoryAllocated += sizeof(TRI_fulltext_handles_t);
#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase += sizeof(TRI_fulltext_handles_t);
#endif

  return (TRI_fts_index_t*)idx.release();
}

/// @brief free the fulltext index
void TRI_FreeFtsIndex(TRI_fts_index_t* ftx) {
  index__t* idx = static_cast<index__t*>(ftx);

  // free root node (this will recursively free all other nodes)
  FreeNode(idx, idx->_root);

  // free handles
  TRI_FreeHandlesMMFilesFulltextIndex(idx->_handles);
  idx->_handles = nullptr;
  idx->_memoryAllocated -= sizeof(TRI_fulltext_handles_t);

#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase -= sizeof(TRI_fulltext_handles_t);
  TRI_ASSERT(idx->_memoryBase == sizeof(index__t));
  TRI_ASSERT(idx->_memoryFollowers == 0);
  TRI_ASSERT(idx->_memoryNodes == 0);
  TRI_ASSERT(idx->_memoryAllocated == sizeof(index__t));
#endif

  // free index itself
  delete idx;
}

void TRI_TruncateMMFilesFulltextIndex(TRI_fts_index_t* ftx) {
  index__t* idx = static_cast<index__t*>(ftx);

  // free root node (this will recursively free all other nodes)
  FreeNode(idx, idx->_root);
  
  // free handles
  TRI_FreeHandlesMMFilesFulltextIndex(idx->_handles);
  idx->_handles = nullptr;
  
  idx->_memoryAllocated = sizeof(index__t);
#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase = sizeof(index__t);
  idx->_memoryNodes = 0;
  idx->_memoryFollowers = 0;
  idx->_nodesAllocated = 0;
#endif
  
  // create the root node
  idx->_root = CreateNode(idx);
  if (idx->_root == nullptr) {
    // out of memory
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  // create an instance for managing document handles
  idx->_handles = TRI_CreateHandlesMMFilesFulltextIndex(2048);
  if (idx->_handles == nullptr) {
    // out of memory
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx->_root);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  idx->_memoryAllocated += sizeof(TRI_fulltext_handles_t);
#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase += sizeof(TRI_fulltext_handles_t);
#endif
}

/// @brief delete a document from the index
void TRI_DeleteDocumentMMFilesFulltextIndex(TRI_fts_index_t* const ftx,
                                            const TRI_voc_rid_t document) {
  index__t* idx = static_cast<index__t*>(ftx);

  WRITE_LOCKER(guard, idx->_lock);
  TRI_DeleteDocumentHandleMMFilesFulltextIndex(idx->_handles, document);
}

/// @brief insert a list of words into the index
/// calling this function requires a wordlist that has word with the correct
/// lengths. especially, words in the wordlist must not be longer than
/// MAX_WORD_BYTES. the caller must check this before calling this function
///
/// The function will sort the wordlist in place to
/// - filter out duplicates on insertion
/// - save redundant lookups of prefix nodes for adjacent words with shared
///   prefixes
bool TRI_InsertWordsMMFilesFulltextIndex(TRI_fts_index_t* const ftx,
                                         TRI_voc_rid_t document,
                                         std::set<std::string> const& wordlist) {
  index__t* idx;
  TRI_fulltext_handle_t handle;
  node_t* paths[MAX_WORD_BYTES + 4];
  size_t lastLength;

  if (wordlist.empty()) {
    return true;
  }

  // initialize to satisfy scan-build
  paths[0] = nullptr;
  paths[MAX_WORD_BYTES] = nullptr;

  // the words must be sorted so we can avoid duplicate words and use an
  // optimization
  // for words with common prefixes (which will be adjacent in the sorted list
  // of words)
  // The default comparator (<) is exactly what we need here

  idx = static_cast<index__t*>(ftx);

  WRITE_LOCKER(guard, idx->_lock);

  // get a new handle for the document
  handle = TRI_InsertHandleMMFilesFulltextIndex(idx->_handles, document);
  if (handle == 0) {
    return false;
  }

  // if words are all different, we must start from the root node. the root node
  // is also the
  // start for the 1st word inserted
  paths[0] = idx->_root;
  lastLength = 0;
  
  std::string const* prev = nullptr;
  for (std::string const& tmp : wordlist) {
    node_t* node;
    char const* p;
    size_t start;
    size_t i;
    
    if (prev != nullptr) {
      // check if current word has a shared/common prefix with the previous word
      // inserted
      // in case this is true, we can use an optimisation and do not need to
      // traverse the
      // tree from the root again. instead, we just start at the node at the end
      // of the
      // shared/common prefix. this will save us a lot of tree lookups
      start = CommonPrefixLength(*prev, tmp);
      if (start > MAX_WORD_BYTES) {
        start = MAX_WORD_BYTES;
      }
      
      // check if current word is the same as the last word. we do not want to
      // insert the
      // same word multiple times for the same document
      if (start > 0 && start == lastLength &&
          start == tmp.size()) {
        // duplicate word, skip it and continue with next word
        continue;
      }
    } else {
      start = 0;
    }
    prev = &tmp;
    
    // for words with common prefixes, use the most appropriate start node we
    // do not need to traverse the tree from the root again
    node = paths[start];
#if TRI_FULLTEXT_DEBUG
    TRI_ASSERT(node != nullptr);
#endif
    
    // now insert into the tree, starting at the next character after the common
    // prefix
    //std::string suffix = tmp.substr(start);
    p = tmp.c_str() + start;
    
    for (i = start; *p && i <= MAX_WORD_BYTES; ++i) {
      node_char_t c = (node_char_t) * (p++);
      
#if TRI_FULLTEXT_DEBUG
      TRI_ASSERT(node != nullptr);
#endif
      
      node = EnsureSubNode(idx, node, c);
      if (node == nullptr) {
        return false;
      }
      
#if TRI_FULLTEXT_DEBUG
      TRI_ASSERT(node != nullptr);
#endif
      
      paths[i + 1] = node;
    }
    
    if (!InsertHandle(idx, node, handle)) {
      // document was added at least once, mark it as deleted
      TRI_DeleteDocumentHandleMMFilesFulltextIndex(idx->_handles, document);
      return false;
    }
    
    // store length of word just inserted
    // we'll use that to compare with the next word for duplicate removal
    lastLength = i;
  }

  return true;
}

/// @brief find all documents that contain a word (exact match)
#if 0
TRI_fulltext_result_t* TRI_FindExactMMFilesFulltextIndex (TRI_fts_index_t* const ftx,
                                                   char const* const key,
                                                   size_t const keyLength) {
  return FindDocuments((index__t*) ftx, key, keyLength, false);
}
#endif

/// @brief find all documents that contain a word (exact match)
#if 0
TRI_fulltext_result_t* TRI_FindPrefixMMFilesFulltextIndex (TRI_fts_index_t* const ftx,
                                                    char const* key,
                                                    size_t const keyLength) {
  return FindDocuments((index__t*) ftx, key, keyLength, true);
}
#endif

/// @brief execute a query on the fulltext index
/// note: this will free the query
TRI_fulltext_result_t* TRI_QueryMMFilesFulltextIndex(TRI_fts_index_t* const ftx,
                                              TRI_fulltext_query_t* query) {
  index__t* idx;
  TRI_fulltext_list_t* result;
  size_t i;

  if (query == nullptr) {
    return nullptr;
  }

  if (query->_numWords == 0) {
    // query is empty
    TRI_FreeQueryMMFilesFulltextIndex(query);
    return TRI_CreateResultMMFilesFulltextIndex(0);
  }

  auto maxResults = query->_maxResults;

  idx = static_cast<index__t*>(ftx);

  READ_LOCKER(guard, idx->_lock);

  // initial result is empty
  result = nullptr;

  // iterate over all words in query
  for (i = 0; i < query->_numWords; ++i) {
    char* word;
    TRI_fulltext_query_match_e match;
    TRI_fulltext_query_operation_e operation;
    TRI_fulltext_list_t* list;
    node_t* node;

    word = query->_words[i];
    if (word == nullptr) {
      break;
    }

    match = query->_matches[i];
    operation = query->_operations[i];

    LOG_TOPIC(DEBUG, arangodb::Logger::FIXME) << "searching for word: '" << word << "'";

    if ((operation == TRI_FULLTEXT_AND || operation == TRI_FULLTEXT_EXCLUDE) &&
        i > 0 && TRI_NumEntriesListMMFilesFulltextIndex(result) == 0) {
      // current result set is empty so logical AND or EXCLUDE will not have any
      // result either
      continue;
    }

    list = nullptr;
    node = FindNode(idx, word, strlen(word));
    if (node != nullptr) {
      if (match == TRI_FULLTEXT_COMPLETE) {
        // complete matching
        list = GetDirectNodeHandles(node);
      } else if (match == TRI_FULLTEXT_PREFIX) {
        // prefix matching
        list = GetSubNodeHandles(node);
      } else {
        LOG_TOPIC(WARN, arangodb::Logger::FIXME) << "invalid matching option for fulltext index query";
        list = TRI_CreateListMMFilesFulltextIndex(0);
      }
    } else {
      list = TRI_CreateListMMFilesFulltextIndex(0);
    }

    if (operation == TRI_FULLTEXT_AND) {
      // perform a logical AND of current and previous result (if any)
      result = TRI_IntersectListMMFilesFulltextIndex(result, list);
    } else if (operation == TRI_FULLTEXT_OR) {
      // perform a logical OR of current and previous result (if any)
      result = TRI_UnioniseListMMFilesFulltextIndex(result, list);
    } else if (operation == TRI_FULLTEXT_EXCLUDE) {
      // perform a logical exclusion of current from previous result (if any)
      result = TRI_ExcludeListMMFilesFulltextIndex(result, list);
    }

    if (result == nullptr) {
      // out of memory
      break;
    }
  }

  TRI_FreeQueryMMFilesFulltextIndex(query);

  if (result == nullptr) {
    // if we haven't found anything...
    return TRI_CreateResultMMFilesFulltextIndex(0);
  }

  // now convert the handle list into a result (this will also filter out
  // deleted documents)
  TRI_fulltext_result_t* r = MakeListResult(idx, result, maxResults);

  return r;
}

/// @brief dump index tree
#if TRI_FULLTEXT_DEBUG
void TRI_DumpTreeFtsIndex(TRI_fts_index_t* ftx) {
  index__t* idx = static_cast<index__t*>(ftx);

  TRI_DumpHandleMMFilesFulltextIndex(idx->_handles);
  DumpNode(idx->_root, 0);
}
#endif

/// @brief dump index statistics
#if TRI_FULLTEXT_DEBUG
void TRI_DumpStatsFtsIndex(TRI_fts_index_t* ftx) {
  index__t* idx = static_cast<index__t*>(ftx);
  TRI_fulltext_stats_t stats;

  stats = TRI_StatsMMFilesFulltextIndex(idx);
  printf("memoryTotal     %llu\n", (unsigned long long)stats._memoryTotal);
#if TRI_FULLTEXT_DEBUG
  printf("memoryOwn       %llu\n", (unsigned long long)stats._memoryOwn);
  printf("memoryBase      %llu\n", (unsigned long long)stats._memoryBase);
  printf("memoryNodes     %llu\n", (unsigned long long)stats._memoryNodes);
  printf("memoryFollowers %llu\n", (unsigned long long)stats._memoryFollowers);
  printf("memoryDocuments %llu\n", (unsigned long long)stats._memoryDocuments);
  printf("numNodes        %llu\n", (unsigned long long)stats._numNodes);
#endif

  if (idx->_handles != nullptr) {
    printf("memoryHandles   %llu\n", (unsigned long long)stats._memoryHandles);
    printf("numDocuments    %llu\n", (unsigned long long)stats._numDocuments);
    printf("numDeleted      %llu\n", (unsigned long long)stats._numDeleted);
    printf("deletionGrade   %f\n", stats._handleDeletionGrade);
    printf("should compact  %d\n", (int)stats._shouldCompact);
  }
}
#endif

/// @brief return stats about the index
TRI_fulltext_stats_t TRI_StatsMMFilesFulltextIndex(TRI_fts_index_t* ftx) {
  index__t* idx = static_cast<index__t*>(ftx);

  READ_LOCKER(guard, idx->_lock);

  TRI_fulltext_stats_t stats;
  stats._memoryTotal = TRI_MemoryMMFilesFulltextIndex(idx);
#if TRI_FULLTEXT_DEBUG
  stats._memoryOwn = idx->_memoryAllocated;
  stats._memoryBase = idx->_memoryBase;
  stats._memoryNodes = idx->_memoryNodes;
  stats._memoryFollowers = idx->_memoryFollowers;
  stats._memoryDocuments =
      idx->_memoryAllocated - idx->_memoryNodes - idx->_memoryBase;
  stats._numNodes = idx->_nodesAllocated;
#endif

  if (idx->_handles != nullptr) {
    stats._memoryHandles = TRI_MemoryHandleMMFilesFulltextIndex(idx->_handles);
    stats._numDocuments = TRI_NumHandlesHandleMMFilesFulltextIndex(idx->_handles);
    stats._numDeleted = TRI_NumDeletedHandleMMFilesFulltextIndex(idx->_handles);
    stats._handleDeletionGrade =
        TRI_DeletionGradeHandleMMFilesFulltextIndex(idx->_handles);
    stats._shouldCompact = TRI_ShouldCompactHandleMMFilesFulltextIndex(idx->_handles);
  } else {
    stats._memoryHandles = 0;
    stats._numDocuments = 0;
    stats._numDeleted = 0;
    stats._handleDeletionGrade = 0.0;
    stats._shouldCompact = false;
  }


  return stats;
}

/// @brief return the total memory used by the index
size_t TRI_MemoryMMFilesFulltextIndex(TRI_fts_index_t* ftx) {
  // no need to lock here, as we are called from under a lock already
  index__t* idx = static_cast<index__t*>(ftx);

  if (idx->_handles != nullptr) {
    return idx->_memoryAllocated + TRI_MemoryHandleMMFilesFulltextIndex(idx->_handles);
  }

  return idx->_memoryAllocated;
}

/// @brief compact the fulltext index
/// note: the caller must hold a lock on the index before called this
bool TRI_CompactMMFilesFulltextIndex(TRI_fts_index_t* ftx) {
  index__t* idx = static_cast<index__t*>(ftx);

  // but don't block if the index is busy
  // try to acquire the write lock to clean up
  TRY_WRITE_LOCKER(guard, idx->_lock);
  if (!guard.isLocked()) {
    return true;
  }

  if (!TRI_ShouldCompactHandleMMFilesFulltextIndex(idx->_handles)) {
    // not enough cleanup work to do
    return true;
  }

  // this will create a copy of the handles from the existing index, but will
  // re-align the handle numbers consecutively, starting at 1.
  // this will also populate the _map property, which can be used to clean up
  // handles of existing nodes
  TRI_fulltext_handles_t* clone = TRI_CompactHandleMMFilesFulltextIndex(idx->_handles);
  if (clone == nullptr) {
    return false;
  }

  CleanupNodes(idx, idx->_root, clone->_map);

  // delete the original handle list
  TRI_FreeHandlesMMFilesFulltextIndex(idx->_handles);

  // free the rewrite map
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, clone->_map);
  clone->_map = nullptr;

  // cleanup finished, now switch over
  idx->_handles = clone;

  return true;
}
