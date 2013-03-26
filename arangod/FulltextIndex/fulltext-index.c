////////////////////////////////////////////////////////////////////////////////
/// @brief full text search
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2013 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "fulltext-index.h"

#include "BasicsC/common.h"
#include "BasicsC/locks.h"
#include "BasicsC/logging.h"

#include "fulltext-handles.h"
#include "fulltext-list.h"
#include "fulltext-query.h"
#include "fulltext-result.h"
#include "fulltext-wordlist.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief use padding for pointers in binary data
////////////////////////////////////////////////////////////////////////////////

#undef FULLTEXT_PADDING

////////////////////////////////////////////////////////////////////////////////
/// @brief maximum length of an indexed word in bytes
/// a UTF-8 character can contain up to 4 bytes
////////////////////////////////////////////////////////////////////////////////

#define MAX_WORD_BYTES ((TRI_FULLTEXT_MAX_WORD_LENGTH) * 4)

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                     private types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief the type of characters indexed. should be one byte long
////////////////////////////////////////////////////////////////////////////////

typedef uint8_t node_char_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef for follower nodes. this is just void because for the
/// compiler it is a sequence of binary data with no internal structure
////////////////////////////////////////////////////////////////////////////////

typedef void followers_t;

////////////////////////////////////////////////////////////////////////////////
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
////////////////////////////////////////////////////////////////////////////////

typedef struct node_s {
  followers_t*            _followers;
  TRI_fulltext_list_t*    _handles;
}
node_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief the actual fulltext index
////////////////////////////////////////////////////////////////////////////////

typedef struct {
  node_t*                 _root;                // root node of the index

  TRI_fulltext_handles_t* _handles;             // handles management instance

  TRI_read_write_lock_t   _lock;

  size_t                  _memoryAllocated;     // total memory used by index
#if TRI_FULLTEXT_DEBUG
  size_t                  _memoryBase;          // base memory
  size_t                  _memoryNodes;         // total memory used by nodes (node_t only)
  size_t                  _memoryFollowers;     // total memory used for followers (no documents)
  uint32_t                _nodesAllocated;      // number of nodes currently in use
#endif

  uint32_t                _nodeChunkSize;       // how many sub-nodes to allocate per chunk
  uint32_t                _initialNodeHandles;  // how many handles to allocate per node
}
index_t;

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                          forwards
// -----------------------------------------------------------------------------

static uint32_t NodeNumFollowers (const node_t* const);

static uint32_t NodeNumAllocated (const node_t* const);

static node_char_t* NodeFollowersKeys (const node_t* const);

static node_t** NodeFollowersNodes (const node_t* const);

static void FreeFollowers (index_t* const, node_t*);

static void FreeNode (index_t* const, node_t*);

static size_t MemorySubNodeList (const uint32_t);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief print some indentation
////////////////////////////////////////////////////////////////////////////////

#if TRI_FULLTEXT_DEBUG
void Indent (const uint32_t level) {
  uint32_t i;

  for (i = 0; i < level; ++i) {
    printf("  ");
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief dump the contents of a node
////////////////////////////////////////////////////////////////////////////////

#if TRI_FULLTEXT_DEBUG
void DumpNode (const node_t* const node, uint32_t level) {
  uint32_t numFollowers;
  uint32_t numHandles;
  uint32_t i;

  numFollowers = NodeNumFollowers(node);
  if (node->_handles != NULL) {
    numHandles = TRI_NumEntriesListFulltextIndex(node->_handles);
  }
  else {
    numHandles = 0;
  }

  if (numFollowers == 0) {
    printf(" (x) ");
  }
  else {
    printf("     ");
  }

  if (level < 20) {
    Indent(20 - level);
  }
  printf("node %p (%lu followers, %lu handles)\n", node, (unsigned long) numFollowers, (unsigned long) numHandles);

  if (numFollowers > 0) {
    node_char_t* followerKeys = NodeFollowersKeys(node);
    node_t** followerNodes    = NodeFollowersNodes(node);

    for (i = 0; i < numFollowers; ++i) {
      node_char_t followerKey  = followerKeys[i];
      node_t* followerNode = followerNodes[i];

      Indent(level);
      printf("%c", (char) followerKey);
      DumpNode(followerNode, level + 1);
    }
  }

  if (numHandles > 0) {
    Indent(level);
    if (level < 20) {
      Indent(20 - level);
    }
    printf("(");
    TRI_DumpListFulltextIndex(node->_handles);

    printf(")\n");
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return the padding to be applied when allocating memory for the
/// sub-node list. the padding is done between the (uint8_t) keys and the
/// (node_t*) pointers. padding can be used to align the pointers to some
/// "good" boundary
////////////////////////////////////////////////////////////////////////////////

static inline size_t Padding (const uint32_t numEntries) {
#ifdef FULLTEXT_PADDING
  size_t offset = sizeof(uint8_t) +  // numAllocated
                  sizeof(uint8_t) +  // numUsed
                  (sizeof(node_char_t) * numEntries); // followerKeys

  if (offset % PAD == 0) {
    // already aligned
    return 0;
  }
  else {
    // not aligned, apply padding
    return PAD - (offset % PAD);
  }
#else
  return 0;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief re-allocate memory for the index and update memory usage statistics
////////////////////////////////////////////////////////////////////////////////

static inline void* ReallocateMemory (index_t* const idx, 
                                      void* old, 
                                      const size_t newSize, 
                                      const size_t oldSize) {
  void* data;

#if TRI_FULLTEXT_DEBUG
  assert(old != NULL);
  assert(newSize > 0);
  assert(oldSize > 0);
#endif

  data = TRI_Reallocate(TRI_UNKNOWN_MEM_ZONE, old, newSize);
  if (data != NULL) {
    idx->_memoryAllocated += newSize;
    idx->_memoryAllocated -= oldSize;
  }
  return data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief allocate memory for the index and update memory usage statistics
////////////////////////////////////////////////////////////////////////////////

static inline void* AllocateMemory (index_t* const idx, const size_t size) {
  void* data;

#if TRI_FULLTEXT_DEBUG
  assert(size > 0);
#endif

  data = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, size, false);
  if (data != NULL) {
    idx->_memoryAllocated += size;
  }
  return data;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free memory and update memory usage statistics
////////////////////////////////////////////////////////////////////////////////

static inline void FreeMemory (index_t* const idx,
                               void* data,
                               const size_t size) {
#if TRI_FULLTEXT_DEBUG
  assert(size > 0);
  assert(idx->_memoryAllocated >= size);
#endif

  idx->_memoryAllocated -= size;
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, data);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adjust the number of followers for a node
/// note: if the value is set to 0, this might free the sub-nodes list
////////////////////////////////////////////////////////////////////////////////

static inline void SetNodeNumFollowers (index_t* const idx,
                                        node_t* const node,
                                        uint32_t value) {
#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
  assert(node->_followers != NULL);
  assert(value <= 255);
#endif

  // note: value must be <= current number of followers
  if (value == 0) {
    // new value is 0, now free old sub-nodes list (if any)
    uint32_t numAllocated = NodeNumAllocated(node);

#if TRI_FULLTEXT_DEBUG
    idx->_memoryFollowers -= MemorySubNodeList(numAllocated);
#endif
    FreeMemory(idx, node->_followers, MemorySubNodeList(numAllocated));
    node->_followers = NULL;
  }
  else {
    // value is not 0, now write the new value
    uint8_t* head = (uint8_t*) node->_followers;
    *(++head) = (uint8_t) value;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of used sub-nodes in a sub node list
////////////////////////////////////////////////////////////////////////////////

static inline uint32_t NodeNumFollowers (const node_t* const node) {
  uint8_t* head;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  if (node->_followers == NULL) {
    return 0;
  }

  head = (uint8_t*) node->_followers;
  return (uint32_t) *(++head);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of allocated (not necessarily used) sub-nodes in a
/// sub node list
////////////////////////////////////////////////////////////////////////////////

static uint32_t NodeNumAllocated (const node_t* const node) {
  uint8_t* head;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  if (node->_followers == NULL) {
    return 0;
  }

  head = (uint8_t*) node->_followers;
  return (uint32_t) *head;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief initialise a sub-node list with length information
////////////////////////////////////////////////////////////////////////////////

static void InitialiseSubNodeList (void* data,
                                   const uint32_t numAllocated,
                                   const uint32_t numFollowers) {
  uint8_t* head = (uint8_t*) data;

  *(head++) = (uint8_t) numAllocated;
  *head = (uint8_t) numFollowers;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a pointer to the start of the keys in a sub-node list
/// the caller must make sure the node actually has sub-nodes
////////////////////////////////////////////////////////////////////////////////

static inline node_char_t* FollowersKeys (void* data) {
  uint8_t* head = (uint8_t*) data;

#if TRI_FULLTEXT_DEBUG
  assert(data != NULL);
#endif

  return (node_char_t*) (head + 2); // numAllocated + numEntries
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a pointer to the start of the keys in a sub-node list
/// the caller must make sure the node actually has sub-nodes
////////////////////////////////////////////////////////////////////////////////

static inline node_char_t* NodeFollowersKeys (const node_t* const node) {
#if TRI_FULLTEXT_DEBUG
  assert(node->_followers != NULL);
#endif

  return FollowersKeys(node->_followers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a pointer to the start of the node-list in a sub-node list
/// the caller must make sure the node actually has sub-nodes
////////////////////////////////////////////////////////////////////////////////

static inline node_t** FollowersNodes (void* data) {
  uint8_t* head = (uint8_t*) data;
  uint8_t numAllocated = *head;
  uint8_t* keys = (uint8_t*) (head + 2); // numAllocated + numEntries

  return (node_t**) (uint8_t*) ((keys + numAllocated) + Padding(numAllocated));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a pointer to the start of the node-list in a sub-node list
/// the caller must make sure the node actually has sub-nodes
////////////////////////////////////////////////////////////////////////////////

static inline node_t** NodeFollowersNodes (const node_t* const node) {
#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
  assert(node->_followers != NULL);
#endif

  return FollowersNodes(node->_followers);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get a pointer to the start of the node-list in a sub-node list
/// the caller must make sure the node actually has sub-nodes
////////////////////////////////////////////////////////////////////////////////

static inline node_t** FollowersNodesPos (void* data, const uint32_t numAllocated) {
  uint8_t* head = (uint8_t*) data;
  uint8_t* keys = (uint8_t*) (head + 2); // numAllocated + numEntries

  return (node_t**) (uint8_t*) ((keys + numAllocated) + Padding(numAllocated));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the memory required to store a sub-node list of the
/// specific length
////////////////////////////////////////////////////////////////////////////////

static size_t MemorySubNodeList (const uint32_t numEntries) {
  return sizeof(uint8_t) +  // numAllocated
         sizeof(uint8_t) +  // numEntries
         ((sizeof(node_char_t) + sizeof(node_t*)) * numEntries) +  // follower keys & nodes
         Padding(numEntries);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check the current list of sub-nodes for a node and increase its
/// size if it is too small to hold another node
////////////////////////////////////////////////////////////////////////////////

static bool ExtendSubNodeList (index_t* const idx,
                               node_t* const node,
                               const uint32_t numFollowers,
                               const uint32_t numAllocated) {
  size_t nextSize;
  uint32_t nextAllocated;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  // current list has reached its limit, we must increase it

  nextAllocated = numAllocated + idx->_nodeChunkSize;
  nextSize = MemorySubNodeList(nextAllocated);

  if (node->_followers == NULL) {
    // allocate a new list
    node->_followers = AllocateMemory(idx, nextSize);
    if (node->_followers == NULL) {
      // out of memory
      return false;
    }
  
    // initialise the chunk of memory we just got
    InitialiseSubNodeList(node->_followers, nextAllocated, numFollowers);
#if TRI_FULLTEXT_DEBUG
    idx->_memoryFollowers += nextSize;
#endif
    return true;
  }
  else {
    // re-allocate an existing list
    followers_t* followers;
    size_t oldSize;

    oldSize = MemorySubNodeList(numAllocated);

    followers = ReallocateMemory(idx, node->_followers, nextSize, oldSize); 
    if (followers == NULL) {
      // out of memory
      return false;
    }

    // initialise the chunk of memory we just got
    InitialiseSubNodeList(followers, nextAllocated, numFollowers);
#if TRI_FULLTEXT_DEBUG
    idx->_memoryFollowers += nextSize;
    idx->_memoryFollowers -= oldSize;
#endif

    // note the new pointer
    node->_followers = followers;

    if (numFollowers > 0) {
      // copy existing sub-nodes into the new sub-nodes list
      memmove(FollowersNodes(followers), FollowersNodesPos(followers, numAllocated), sizeof(node_t*) * numFollowers);
    }
  
    return true;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a new, empty node
////////////////////////////////////////////////////////////////////////////////

static node_t* CreateNode (index_t* const idx) {
  node_t* node;

  node = AllocateMemory(idx, sizeof(node_t));
  if (node == NULL) {
    return NULL;
  }

  node->_followers = NULL;
  node->_handles   = NULL;

#if TRI_FULLTEXT_DEBUG
  idx->_nodesAllocated++;
  idx->_memoryNodes += sizeof(node_t);
#endif

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a node's follower nodes
////////////////////////////////////////////////////////////////////////////////

static void FreeFollowers (index_t* const idx, node_t* node) {
  uint32_t numFollowers;
  uint32_t numAllocated;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  if (node->_followers == NULL) {
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

  node->_followers = NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free a node in the index
////////////////////////////////////////////////////////////////////////////////

static void FreeNode (index_t* const idx, node_t* node) {
  if (node == NULL) {
    return;
  }

  if (node->_handles != NULL) {
    // free handles
    idx->_memoryAllocated -= TRI_MemoryListFulltextIndex(node->_handles);
    TRI_FreeListFulltextIndex(node->_handles);
  }

  // free followers
  if (node->_followers != NULL) {
    FreeFollowers(idx, node);
  }

  // free node itself
  FreeMemory(idx, node, sizeof(node_t));
#if TRI_FULLTEXT_DEBUG
  idx->_memoryNodes -= sizeof(node_t);
  idx->_nodesAllocated--;
#endif
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively cleanup nodes (used during compaction)
/// the map contains a rewrite-map of document handles
////////////////////////////////////////////////////////////////////////////////

static bool CleanupNodes (index_t* idx,
                          node_t* node,
                          void* map) {
  bool isActive;

  // assume we can delete the node we are processing
  // we may set this flag to true further down below if we find the node is
  // still useful
  isActive = false;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  if (node->_followers != NULL) {
    // recurse into sub-nodes
    node_char_t* followerKeys = NodeFollowersKeys(node);
    node_t** followerNodes    = NodeFollowersNodes(node);
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
      assert(follower != NULL);
#endif

      // recursively clean up sub-nodes
      if (! CleanupNodes(idx, follower, map)) {
        // the sub-node is empty, kill it!
        FreeNode(idx, follower);
        // and go to next follower
        continue;
      }

      // sub-node is still relevant
      isActive = true;

      if (i != j) {
#if TRI_FULLTEXT_DEBUG
        assert(i > j);
#endif

        // move nodes
        followerKeys[j]  = followerKeys[i];
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
  if (node->_handles != NULL) {
    uint32_t remain;

    remain = TRI_RewriteListFulltextIndex(node->_handles, map);
    if (remain > 0) {
      // there are still handles left in the rewritten handles list
      // we must keep this node
      isActive = true;
    }
    else {
      // no handles left, we can delete the node's handle list
      idx->_memoryAllocated -= TRI_MemoryListFulltextIndex(node->_handles);
      TRI_FreeListFulltextIndex(node->_handles);
      node->_handles = NULL;

    }
  }

  return isActive;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a sub-node of a node with only one sub-node
/// the caller must make sure the node actually has exactly one sub-node!
////////////////////////////////////////////////////////////////////////////////

static inline node_t* FindDirectSubNodeSingle (const node_t* const node,
                                               const node_char_t c) {
  node_char_t* followerKeys;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
  assert(NodeNumFollowers(node) == 1);
#endif

  followerKeys = NodeFollowersKeys(node);

  if (followerKeys[0] == c) {
    node_t** followerNodes = NodeFollowersNodes(node);

    return followerNodes[0];
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a sub-node of a node using a linear search
/// this will compare the node's follower characters with the character passed
/// followers are sorted so it will stop at the first character that is higher
/// than the character passed
/// the caller must make sure the node actually has sub-nodes!
////////////////////////////////////////////////////////////////////////////////

static inline node_t* FindDirectSubNodeLinear (const node_t* const node,
                                               const node_char_t c) {
  node_char_t* followerKeys;
  uint32_t numFollowers;
  uint32_t i;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  numFollowers = NodeNumFollowers(node);
#if TRI_FULLTEXT_DEBUG
  assert(numFollowers >= 1);
#endif

  followerKeys = NodeFollowersKeys(node);

  for (i = 0; i < numFollowers; ++i) {
    node_char_t followerKey = followerKeys[i];

    if (followerKey > c) {
      // we're are already beyond of what we look for
      break; // trampoline to return NULL
    }
    else if (followerKey == c) {
      node_t** followerNodes = NodeFollowersNodes(node);

      return followerNodes[i];
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a sub-node of a node using a binary search
/// the caller must ensure the node actually has sub-nodes!
////////////////////////////////////////////////////////////////////////////////

static node_t* FindDirectSubNodeBinary (const node_t* const node,
                                        const node_char_t c) {
  node_char_t* followerKeys;
  node_t** followerNodes;
  uint32_t numFollowers;
  uint32_t l, r;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  numFollowers = NodeNumFollowers(node);
#if TRI_FULLTEXT_DEBUG
  assert(numFollowers >= 1);
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
        return NULL;
      }
      // this is safe
      r = m - 1;
    }
    else {
      l = m + 1;
    }

    if (r < l) {
      return NULL;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a node's sub-node, identified by its start character
////////////////////////////////////////////////////////////////////////////////

static inline node_t* FindDirectSubNode (const node_t* const node,
                                         const node_char_t c) {
  uint32_t numFollowers;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  numFollowers = NodeNumFollowers(node);

  if (numFollowers >= 8) {
    return FindDirectSubNodeBinary(node, c);
  }
  else if (numFollowers > 1) {
    return FindDirectSubNodeLinear(node, c);
  }
  else if (numFollowers == 1) {
    return FindDirectSubNodeSingle(node, c);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find a node by its key, starting from the index root
////////////////////////////////////////////////////////////////////////////////

static node_t* FindNode (const index_t* idx,
                         const char* const key,
                         const size_t keyLength) {
  node_t* node;
  node_char_t* p;
  size_t i;

  node = (node_t*) idx->_root;
#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif
  p = (node_char_t*) key;

  for (i = 0; i < keyLength; ++i) {
    node = FindDirectSubNode(node, *(p++));
    if (node == NULL) {
      return NULL;
    }
  }

  return node;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create a list with the handles of a node
////////////////////////////////////////////////////////////////////////////////

static TRI_fulltext_list_t* GetDirectNodeHandles (const node_t* const node) {
  return TRI_CloneListFulltextIndex(node->_handles);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively merge node and sub-node handles into the result list
////////////////////////////////////////////////////////////////////////////////

static TRI_fulltext_list_t* MergeSubNodeHandles (const node_t* const node,
                                                 TRI_fulltext_list_t* list) {
  node_t** followerNodes;
  uint32_t numFollowers;
  uint32_t i;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
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
  assert(follower != NULL);
#endif
    if (follower->_handles != NULL) {
      // OR-merge the follower node's documents with what we already have found
      list = TRI_UnioniseListFulltextIndex(list, GetDirectNodeHandles(follower));
      if (list == NULL) {
        return NULL;
      }
    }

    // recurse into sub-nodes
    list = MergeSubNodeHandles(follower, list);
    if (list == NULL) {
      return NULL;
    }
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief recursively create a result list with the handles of a node and
/// all of its sub-nodes
////////////////////////////////////////////////////////////////////////////////

static inline TRI_fulltext_list_t* GetSubNodeHandles (const node_t* const node) {
  TRI_fulltext_list_t* list;

  list = GetDirectNodeHandles(node);
  return MergeSubNodeHandles(node, list);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a new sub-node underneath an existing node
/// the caller must make sure that the node already has memory allocated for
/// the _followers property
////////////////////////////////////////////////////////////////////////////////

static node_t* InsertSubNode (index_t* const idx,
                              node_t* const node,
                              const uint32_t position,
                              const node_char_t key) {
  node_t** followerNodes;
  node_char_t* followerKeys;
  node_t* subNode;
  uint32_t numFollowers;
  uint32_t moveCount;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  // create the sub-node
  subNode = CreateNode(idx);
  if (subNode == NULL) {
    // out of memory
    return NULL;
  }

  numFollowers  = NodeNumFollowers(node);
  followerKeys  = NodeFollowersKeys(node);
  followerNodes = NodeFollowersNodes(node);

#if TRI_FULLTEXT_DEBUG
  assert(numFollowers >= position);
#endif

  // we have to move this many elements
  moveCount     = numFollowers - position;

  if (moveCount > 0) {
    // make room for the new node
    memmove(followerKeys  + position + 1, followerKeys  + position, moveCount * sizeof(node_char_t));
    memmove(followerNodes + position + 1, followerNodes + position, moveCount * sizeof(node_t*));
  }

  // register the new sub node
  followerNodes[position] = subNode;
  followerKeys[position]  = key;
  SetNodeNumFollowers(idx, node, numFollowers + 1);

  return subNode;
}

////////////////////////////////////////////////////////////////////////////////
/// ensure that a specific sub-node (with a specific key) is there
/// if it is not there, it will be created by this function
////////////////////////////////////////////////////////////////////////////////

static node_t* EnsureSubNode (index_t* const idx,
                              node_t* node,
                              const node_char_t c) {
  uint32_t numFollowers;
  uint32_t numAllocated;
  uint32_t start;
  uint32_t i;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  // search the node and find the correct insert position if it does not exist
  numFollowers = NodeNumFollowers(node);

  if (numFollowers > 0) {
    // linear search
    // TODO: optimise this search
    node_char_t* followerKeys;

    followerKeys = NodeFollowersKeys(node);
    // divide the search space in 2 halves
    if (numFollowers >= 8 && followerKeys[numFollowers / 2] < c) {
      start = numFollowers / 2;
    }
    else {
      start = 0;
    }

    for (i = start; i < numFollowers; ++i) {
      node_char_t followerKey;

      followerKey = followerKeys[i];

      if (followerKey > c) {
        // we have found a key beyond what we're looking for. abort the search
        // i now contains the correct insert position
        break;
      }
      else if (followerKey == c) {
        // found the node, return it
        node_t** followerNodes = NodeFollowersNodes(node);

        return followerNodes[i];
      }
    }
  }
  else {
    // no followers yet. insert position is 0
    i = 0;
  }

  // must insert a new node

  numAllocated = NodeNumAllocated(node);

  // we'll be doing an insert. make sure the node has enough space for containing
  // a list with one element more
  if (numFollowers >= numAllocated) {
    if (! ExtendSubNodeList(idx, node, numFollowers, numAllocated)) {
      // out of memory
      return NULL;
    }
  }

#if TRI_FULLTEXT_DEBUG
  assert(node->_followers != NULL);
#endif

  return InsertSubNode(idx, node, i, c);
}

////////////////////////////////////////////////////////////////////////////////
/// insert a handle for a node
////////////////////////////////////////////////////////////////////////////////

static bool InsertHandle (index_t* const idx,
                          node_t* const node,
                          const TRI_fulltext_handle_t handle) {
  TRI_fulltext_list_t* list;
  TRI_fulltext_list_t* oldList;
  size_t oldAlloc;

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  if (node->_handles == NULL) {
    // node does not yet have any handles. now allocate a new chunk of handles
    node->_handles = TRI_CreateListFulltextIndex(idx->_initialNodeHandles);
    idx->_memoryAllocated += TRI_MemoryListFulltextIndex(node->_handles);
  }

  if (node->_handles == NULL) {
    // out of memory
    return false;
  }

  oldList  = node->_handles;
  oldAlloc = TRI_MemoryListFulltextIndex(oldList);

  // adding to the list might change the list pointer!
  list = TRI_InsertListFulltextIndex(node->_handles, handle);
  if (list == NULL) {
    // out of memory
    return false;
  }

  if (list != oldList) {
    // the insert might have changed the pointer
    node->_handles = list;
    idx->_memoryAllocated += TRI_MemoryListFulltextIndex(list);
    idx->_memoryAllocated -= oldAlloc;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// turn a handle list into a proper document list result
/// this will also exclude all deleted documents
////////////////////////////////////////////////////////////////////////////////

static TRI_fulltext_result_t* MakeListResult (index_t* const idx,
                                              TRI_fulltext_list_t* list) {
  TRI_fulltext_result_t* result;
  TRI_fulltext_list_entry_t* listEntries;
  uint32_t numResults;
  uint32_t i, pos;

  if (list == NULL) {
    return NULL;
  }

  // we have a list of handles
  // now turn the handles into documents and exclude deleted ones on the fly
  numResults = TRI_NumEntriesListFulltextIndex(list);
  result = TRI_CreateResultFulltextIndex(numResults);
  if (result == NULL) {
    TRI_FreeListFulltextIndex(list);
    return NULL;
  }

  pos = 0;
  listEntries = TRI_StartListFulltextIndex(list);

  for (i = 0; i < numResults; ++i) {
    TRI_fulltext_handle_t handle;
    TRI_fulltext_doc_t doc;

    handle = listEntries[i];
    doc = TRI_GetDocumentFulltextIndex(idx->_handles, handle);

    if (doc == 0) {
      // deleted document
      continue;
    }

    result->_documents[pos++] = doc;
  }

  result->_numDocuments = pos;

  // don't need the list anymore
  TRI_FreeListFulltextIndex(list);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find all documents from the index that match the key
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_result_t* FindDocuments (index_t* const idx,
                                      const char* const key,
                                      const size_t keyLength,
                                      const bool recursive) {
  node_t* node;
  TRI_fulltext_list_t* list;

  node = FindNode(idx, key, keyLength);
  if (node == NULL) {
    // not found, create empty result
    return TRI_CreateResultFulltextIndex(0);
  }

  if (recursive) {
    // prefix matching
    list = GetSubNodeHandles(node);
  }
  else {
    // complete match
    list = GetDirectNodeHandles(node);
  }

  return MakeListResult(idx, list);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  string functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief determine the common prefix length of two words
////////////////////////////////////////////////////////////////////////////////

static inline size_t CommonPrefixLength (const char* const lhs,
                                         const char* const rhs) {
  char* p1;
  char* p2;
  size_t length = 0;

  for (p1 = (char*) lhs, p2 = (char*) rhs; *p1 && *p2 && *p1 == *p2; ++p1, ++p2, ++length);

  return length;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief create the fulltext index
////////////////////////////////////////////////////////////////////////////////

TRI_fts_index_t* TRI_CreateFtsIndex (uint32_t handleChunkSize,
                                     uint32_t nodeChunkSize,
                                     uint32_t initialNodeHandles) {
  index_t* idx;

  idx = TRI_Allocate(TRI_UNKNOWN_MEM_ZONE, sizeof(index_t), false);
  if (idx == NULL) {
    return NULL;
  }

  idx->_memoryAllocated    = sizeof(index_t);
#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase         = sizeof(index_t);
  idx->_memoryNodes        = 0;
  idx->_memoryFollowers    = 0;
  idx->_nodesAllocated     = 0;
#endif
  // how many followers to allocate at once
  idx->_nodeChunkSize      = nodeChunkSize;
  // how many handles to create per node by default
  idx->_initialNodeHandles = initialNodeHandles;

  // create the root node
  idx->_root               = CreateNode(idx);
  if (idx->_root == NULL) {
    // out of memory
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
    return NULL;
  }

  // create an instance for managing document handles
  idx->_handles = TRI_CreateHandlesFulltextIndex(handleChunkSize);
  if (idx->_handles == NULL) {
    // out of memory
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx->_root);
    TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
    return NULL;
  }

  idx->_memoryAllocated += sizeof(TRI_fulltext_handles_t);
#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase += sizeof(TRI_fulltext_handles_t);
#endif

  TRI_InitReadWriteLock(&idx->_lock);

  return (TRI_fts_index_t*) idx;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief free the fulltext index
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeFtsIndex (TRI_fts_index_t* ftx) {
  index_t* idx = (index_t*) ftx;

  // free root node (this will recursively free all other nodes)
  FreeNode(idx, idx->_root);

  // free handles
  TRI_FreeHandlesFulltextIndex(idx->_handles);
  idx->_handles = NULL;
  idx->_memoryAllocated -= sizeof(TRI_fulltext_handles_t);

#if TRI_FULLTEXT_DEBUG
  idx->_memoryBase -= sizeof(TRI_fulltext_handles_t);
  assert(idx->_memoryBase == sizeof(index_t));
  assert(idx->_memoryFollowers == 0);
  assert(idx->_memoryNodes == 0);
  assert(idx->_memoryAllocated == sizeof(index_t));
#endif

  TRI_DestroyReadWriteLock(&idx->_lock);

  // free index itself
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, idx);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                             document addition / removal functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief delete a document from the index
////////////////////////////////////////////////////////////////////////////////

void TRI_DeleteDocumentFulltextIndex (TRI_fts_index_t* const ftx,
                                      const TRI_fulltext_doc_t document) {
  index_t* idx = (index_t*) ftx;

  TRI_WriteLockReadWriteLock(&idx->_lock);
  TRI_DeleteDocumentHandleFulltextIndex(idx->_handles, document);
  TRI_WriteUnlockReadWriteLock(&idx->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief add a document word/pair to the index (single word)
/// if multiple words are to be added, the wordlist batch function should be
/// used for better performance
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertWordFulltextIndex (TRI_fts_index_t* const ftx,
                                  const TRI_fulltext_doc_t document,
                                  const char* const key,
                                  const size_t keyLength) {
  index_t* idx;
  TRI_fulltext_handle_t handle;
  node_t* node;
  char* p;
  size_t i;
  size_t end;
  bool result;

  idx = (index_t*) ftx;

  TRI_WriteLockReadWriteLock(&idx->_lock);
  // get a new handle for the document
  handle = TRI_InsertHandleFulltextIndex(idx->_handles, document);
  if (handle == 0) {
    TRI_WriteUnlockReadWriteLock(&idx->_lock);
    return false;
  }

  node = idx->_root;
#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  p = (char*) key;

  end = keyLength;
  if (keyLength > MAX_WORD_BYTES) {
    end = MAX_WORD_BYTES;
  }

  for (i = 0; i < end; ++i) {
    node_char_t c = (node_char_t) *(p++);

    node = EnsureSubNode(idx, node, c);
    if (node == NULL) {
      TRI_WriteUnlockReadWriteLock(&idx->_lock);
      return false;
    }
  }

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

  result = InsertHandle(idx, node, handle);
  TRI_WriteUnlockReadWriteLock(&idx->_lock);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief insert a list of words into the index
/// calling this function requires a wordlist that has word with the correct
/// lengths. especially, words in the wordlist must not be longer than
/// MAX_WORD_BYTES. the caller must check this before calling this function
///
/// The function will sort the wordlist in place to
/// - filter out duplicates on insertion
/// - save redundant lookups of prefix nodes for adjacent words with shared
///   prefixes
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertWordsFulltextIndex (TRI_fts_index_t* const ftx,
                                   const TRI_fulltext_doc_t document,
                                   TRI_fulltext_wordlist_t* wordlist) {
  index_t* idx;
  TRI_fulltext_handle_t handle;
  node_t* paths[MAX_WORD_BYTES + 4];
  size_t lastLength;
  size_t w;

  if (wordlist->_numWords == 0) {
    return true;
  }

  // the words must be sorted so we can avoid duplicate words and use an optimisation
  // for words with common prefixes (which will be adjacent in the sorted list of words)
  TRI_SortWordlistFulltextIndex(wordlist);

  idx = (index_t*) ftx;

  TRI_WriteLockReadWriteLock(&idx->_lock);

  // get a new handle for the document
  handle = TRI_InsertHandleFulltextIndex(idx->_handles, document);
  if (handle == 0) {
    TRI_WriteUnlockReadWriteLock(&idx->_lock);
    return false;
  }

  // if words are all different, we must start from the root node. the root node is also the
  // start for the 1st word inserted
  paths[0] = idx->_root;
  lastLength = 0;

  w = 0;
  while (w < wordlist->_numWords) {
    node_t* node;
    char* p;
    size_t start;
    size_t i;

    // LOG_DEBUG("checking word %s", wordlist->_words[w]);

    if (w > 0) {
      // check if current word has a shared/common prefix with the previous word inserted
      // in case this is true, we can use an optimisation and do not need to traverse the
      // tree from the root again. instead, we just start at the node at the end of the
      // shared/common prefix. this will save us a lot of tree lookups
      start = CommonPrefixLength(wordlist->_words[w - 1], wordlist->_words[w]);
      if (start > MAX_WORD_BYTES) {
        start = MAX_WORD_BYTES;
      }

      // check if current word is the same as the last word. we do not want to insert the
      // same word multiple times for the same document
      if (start > 0 && start == lastLength && start == strlen(wordlist->_words[w])) {
        // duplicate word, skip it and continue with next word
        w++;
        continue;
      }
    }
    else {
      start = 0;
    }

    // for words with common prefixes, use the most appropriate start node we
    // do not need to traverse the tree from the root again
    node = paths[start];
#if TRI_FULLTEXT_DEBUG
    assert(node != NULL);
#endif

    // now insert into the tree, starting at the next character after the common prefix
    p = wordlist->_words[w++] + start;

    for (i = start; *p && i <= MAX_WORD_BYTES; ++i) {
      node_char_t c = (node_char_t) *(p++);

#if TRI_FULLTEXT_DEBUG
      assert(node != NULL);
#endif

      node = EnsureSubNode(idx, node, c);
      if (node == NULL) {
        TRI_WriteUnlockReadWriteLock(&idx->_lock);
        return false;
      }

#if TRI_FULLTEXT_DEBUG
  assert(node != NULL);
#endif

      paths[i + 1] = node;
    }

    if (! InsertHandle(idx, node, handle)) {
      // document was added at least once, mark it as deleted
      TRI_DeleteDocumentHandleFulltextIndex(idx->_handles, document);
      TRI_WriteUnlockReadWriteLock(&idx->_lock);
      return false;
    }

    // store length of word just inserted
    // we'll use that to compare with the next word for duplicate removal
    lastLength = i;
  }

  TRI_WriteUnlockReadWriteLock(&idx->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                   query functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief find all documents that contain a word (exact match)
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_result_t* TRI_FindExactFulltextIndex (TRI_fts_index_t* const ftx,
                                                   const char* const key,
                                                   const size_t keyLength) {
  return FindDocuments((index_t*) ftx, key, keyLength, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief find all documents that contain a word (exact match)
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_result_t* TRI_FindPrefixFulltextIndex (TRI_fts_index_t* const ftx,
                                                    const char* key,
                                                    const size_t keyLength) {
  return FindDocuments((index_t*) ftx, key, keyLength, true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief execute a query on the fulltext index
/// note: this will free the query
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_result_t* TRI_QueryFulltextIndex (TRI_fts_index_t* const ftx,
                                               TRI_fulltext_query_t* query) {
  index_t* idx;
  TRI_fulltext_list_t* result;
  size_t i;

  if (query == NULL) {
    return NULL;
  }

  if (query->_numWords == 0) {
    // query is empty
    TRI_FreeQueryFulltextIndex(query);
    return TRI_CreateResultFulltextIndex(0);
  }

  idx = (index_t*) ftx;

  TRI_ReadLockReadWriteLock(&idx->_lock);

  // initial result is empty
  result = NULL;

  // iterate over all words in query
  for (i = 0; i < query->_numWords; ++i) {
    char* word;
    TRI_fulltext_query_match_e match;
    TRI_fulltext_query_operation_e operation;
    TRI_fulltext_list_t* list;
    node_t* node;

    word      = query->_words[i];
    if (word == NULL) {
      break;
    }

    match     = query->_matches[i];
    operation = query->_operations[i];

    LOG_DEBUG("searching for word: '%s'", word);

    if ((operation == TRI_FULLTEXT_AND || operation == TRI_FULLTEXT_EXCLUDE) &&
      i > 0 && TRI_NumEntriesListFulltextIndex(result) == 0) {
      // current result set is empty so logical AND or EXCLUDE will not have any result either
      continue;
    }

    list = NULL;
    node = FindNode(idx, word, strlen(word));
    if (node != NULL) {
      if (match == TRI_FULLTEXT_COMPLETE) {
        // complete matching
        list = GetDirectNodeHandles(node);
      }
      else if (match == TRI_FULLTEXT_PREFIX) {
        // prefix matching
        list = GetSubNodeHandles(node);
      }
      else {
        LOG_WARNING("invalid matching option for fulltext index query");
        list = TRI_CreateListFulltextIndex(0);
      }
    }
    else {
      list = TRI_CreateListFulltextIndex(0);
    }

    if (operation == TRI_FULLTEXT_AND) {
      // perform a logical AND of current and previous result (if any)
      result = TRI_IntersectListFulltextIndex(result, list);
    }
    else if (operation == TRI_FULLTEXT_OR) {
      // perform a logical OR of current and previous result (if any)
      result = TRI_UnioniseListFulltextIndex(result, list);
    }
    else if (operation == TRI_FULLTEXT_EXCLUDE) {
      // perform a logical exclusion of current from previous result (if any)
      result = TRI_ExcludeListFulltextIndex(result, list);
    }

    if (result == NULL) {
      // out of memory
      break;
    }
  }

  TRI_ReadUnlockReadWriteLock(&idx->_lock);

  TRI_FreeQueryFulltextIndex(query);

  if (result == NULL) {
    // if we haven't found anything...
    return TRI_CreateResultFulltextIndex(0);
  }

  // now convert the handle list into a result (this will also filter out
  // deleted documents)
  return MakeListResult(idx, result);
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @addtogroup Fulltext
/// @{
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
/// @brief dump index tree
////////////////////////////////////////////////////////////////////////////////

#if TRI_FULLTEXT_DEBUG
void TRI_DumpTreeFtsIndex (const TRI_fts_index_t* const ftx) {
  index_t* idx = (index_t*) ftx;

  TRI_DumpHandleFulltextIndex(idx->_handles);
  DumpNode(idx->_root, 0);
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief dump index statistics
////////////////////////////////////////////////////////////////////////////////

#if TRI_FULLTEXT_DEBUG
void TRI_DumpStatsFtsIndex (const TRI_fts_index_t* const ftx) {
  index_t* idx = (index_t*) ftx;
  TRI_fulltext_stats_t stats;

  stats = TRI_StatsFulltextIndex(idx);
  printf("memoryTotal     %llu\n", (unsigned long long) stats._memoryTotal);
#if TRI_FULLTEXT_DEBUG
  printf("memoryOwn       %llu\n", (unsigned long long) stats._memoryOwn);
  printf("memoryBase      %llu\n", (unsigned long long) stats._memoryBase);
  printf("memoryNodes     %llu\n", (unsigned long long) stats._memoryNodes);
  printf("memoryFollowers %llu\n", (unsigned long long) stats._memoryFollowers);
  printf("memoryDocuments %llu\n", (unsigned long long) stats._memoryDocuments);
  printf("numNodes        %llu\n", (unsigned long long) stats._numNodes);
#endif

  if (idx->_handles != NULL) {
    printf("memoryHandles   %llu\n", (unsigned long long) stats._memoryHandles);
    printf("numDocuments    %llu\n", (unsigned long long) stats._numDocuments);
    printf("numDeleted      %llu\n", (unsigned long long) stats._numDeleted);
    printf("deletionGrade   %f\n",   stats._handleDeletionGrade);
    printf("should compact  %d\n", (int)  stats._shouldCompact);
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////
/// @brief return stats about the index
////////////////////////////////////////////////////////////////////////////////

TRI_fulltext_stats_t TRI_StatsFulltextIndex (const TRI_fts_index_t* const ftx) {
  index_t* idx;
  TRI_fulltext_stats_t stats;

  idx = (index_t*) ftx;

  TRI_ReadLockReadWriteLock(&idx->_lock);

  stats._memoryTotal         = TRI_MemoryFulltextIndex(idx);
#if TRI_FULLTEXT_DEBUG
  stats._memoryOwn           = idx->_memoryAllocated;
  stats._memoryBase          = idx->_memoryBase;
  stats._memoryNodes         = idx->_memoryNodes;
  stats._memoryFollowers     = idx->_memoryFollowers;
  stats._memoryDocuments     = idx->_memoryAllocated - idx->_memoryNodes - idx->_memoryBase;
  stats._numNodes            = idx->_nodesAllocated;
#endif

  if (idx->_handles != NULL) {
    stats._memoryHandles       = TRI_MemoryHandleFulltextIndex(idx->_handles);
    stats._numDocuments        = TRI_NumHandlesHandleFulltextIndex(idx->_handles);
    stats._numDeleted          = TRI_NumDeletedHandleFulltextIndex(idx->_handles);
    stats._handleDeletionGrade = TRI_DeletionGradeHandleFulltextIndex(idx->_handles);
    stats._shouldCompact       = TRI_ShouldCompactHandleFulltextIndex(idx->_handles);
  }
  else {
    stats._memoryHandles       = 0;
    stats._numNodes            = 0;
    stats._numDocuments        = 0;
    stats._numDeleted          = 0;
    stats._handleDeletionGrade = 0.0;
    stats._shouldCompact       = false;
  }

  TRI_ReadUnlockReadWriteLock(&idx->_lock);

  return stats;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the total memory used by the index
////////////////////////////////////////////////////////////////////////////////

size_t TRI_MemoryFulltextIndex (const TRI_fts_index_t* const ftx) {
  index_t* idx = (index_t*) ftx;

  if (idx->_handles != NULL) {
    return idx->_memoryAllocated + TRI_MemoryHandleFulltextIndex(idx->_handles);
  }

  return idx->_memoryAllocated;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compact the fulltext index
/// note: the caller must hold a lock on the index before called this
////////////////////////////////////////////////////////////////////////////////

bool TRI_CompactFulltextIndex (TRI_fts_index_t* const ftx) {
  index_t* idx;
  TRI_fulltext_handles_t* clone;

  idx = (index_t*) ftx;

  // but don't block if the index is busy
  // try to acquire the write lock to clean up
  if (! TRI_TryWriteLockReadWriteLock(&idx->_lock)) {
    return true;
  }

  if (! TRI_ShouldCompactHandleFulltextIndex(idx->_handles)) {
    // not enough cleanup work to do
    TRI_WriteUnlockReadWriteLock(&idx->_lock);
    return true;
  }

  // this will create a copy of the handles from the existing index, but will
  // re-align the handle numbers consecutively, starting at 1.
  // this will also populate the _map property, which can be used to clean up
  // handles of existing nodes
  clone = TRI_CompactHandleFulltextIndex(idx->_handles);
  if (clone == NULL) {
    TRI_WriteUnlockReadWriteLock(&idx->_lock);
    return false;
  }

  CleanupNodes(idx, idx->_root, clone->_map);

  // delete the original handle list
  TRI_FreeHandlesFulltextIndex(idx->_handles);

  // free the rewrite map
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, clone->_map);
  clone->_map = NULL;

  // cleanup finished, now switch over
  idx->_handles = clone;
  TRI_WriteUnlockReadWriteLock(&idx->_lock);

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @}
////////////////////////////////////////////////////////////////////////////////

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @addtogroup\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
