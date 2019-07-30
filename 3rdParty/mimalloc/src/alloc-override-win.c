/* ----------------------------------------------------------------------------
Copyright (c) 2018, Microsoft Research, Daan Leijen
This is free software; you can redistribute it and/or modify it under the
terms of the MIT license. A copy of the license can be found in the file
"LICENSE" at the root of this distribution.
-----------------------------------------------------------------------------*/

#include "mimalloc.h"
#include "mimalloc-internal.h"

#if !defined(_WIN32)
#error "this file should only be included on Windows"
#endif

#include <windows.h>
#include <psapi.h>


/*
To override the C runtime `malloc` on Windows we need to patch the allocation
functions at runtime initialization. Unfortunately we can never patch before the
runtime initializes itself, because as soon as we call `GetProcAddress` on the
runtime module (a DLL or EXE in Windows speak), it will first load and initialize
(by the OS calling `DllMain` on it).

This means that some things might be already allocated by the C runtime itself
(and possibly other DLL's) before we get to resolve runtime adresses. This is
no problem if everyone unwinds in order: when we unload, we unpatch and restore
the original crt `free` routines and crt malloc'd memory is freed correctly.

But things go wrong if such early CRT alloc'd memory is freed or re-allocated
_after_ we patch, but _before_ we unload (and unpatch), or if any memory allocated
by us is freed after we unpatched.

There are two tricky situations to deal with:

1. The Thread Local Storage (TLS): when the main thread stops it will call registered
   callbacks on TLS entries (allocated by `FlsAlloc`). This is done by the OS
   before any DLL's are unloaded. Unfortunately, the C runtime registers such
   TLS entries with CRT allocated memory which is freed in the callback.

2. Inside the CRT:
   a. Some variables might get initialized by patched allocated
      blocks but freed during CRT unloading after we unpatched
      (like temporary file buffers).
   b. Some blocks are allocated at CRT and freed by the CRT (like the
      environment storage).
   c. And some blocks are allocated by the CRT and then reallocated
      while patched, and finally freed after unpatching! This
      happens with the `atexit` functions for example to grow the array
      of registered functions.

In principle situation 2 is hopeless: since we cannot patch before CRT initialization,
we can never be sure how to free or reallocate a pointer during CRT unloading.
However, in practice there is a good solution: when terminating, we just patch
the reallocation and free routines to no-ops -- we are winding down anyway! This leaves
just the reallocation problm of CRT alloc'd memory once we are patched. Here, a study of the
CRT reveals that there seem to be just three such situations:

1. When registering `atexit` routines (to grow the exit function table),
2. When calling `_setmaxstdio` (to grow the file handle table),
3. and `_popen`/`_wpopen` (to grow handle pairs). These turn out not to be
   a problem as these are NULL initialized.

We fix these by providing wrappers:

1. We first register a _global_ `atexit` routine ourselves (`mi_patches_at_exit`) before patching,
   and then patch the `_crt_atexit` function to implement our own global exit list (and the
   same for `_crt_at_quick_exit`). All module local lists are no problem since they are always fully
   (un)patched from initialization to end. We can register in the global list by dynamically
   getting the global `_crt_atexit` entry from `ucrtbase.dll`.

2. The `_setmaxstdio`  is _detoured_: we patch it by a stub that unpatches first,
   calls the original routine and repatches again.

That leaves us to reliably shutdown and enter "termination mode":

1. Using our trick to get the global exit list entry point, we register an exit function `mi_patches_atexit`
   that first executes all our home brew list of exit functions, and then enters a _termination_
   phase that patches realloc/free variants with no-ops. Patching later again with special no-ops for
   `free` also improves efficiency during the program run since no flags need to be checked.

2. That is not quite good enough yet since after executing exit routines after us on the
   global exit list (registered by the CRT),
   the OS starts to unwind the TLS callbacks and we would like to run callbacks registered after loading
   our DLL to be done in patched mode. So, we also allocate a TLS entry when our DLL is loaded and when its
   callback is called, we re-enable the original patches again. Since TLS is destroyed in FIFO order
   this runs any callbacks in later DLL's in patched mode.

3. Finally the DLL's get unloaded by the OS in order (still patched) until our DLL gets unloaded
   and then we start a termination phase again, and patch realloc/free with no-ops for good this time.

*/

static int __cdecl mi_setmaxstdio(int newmax);

// ------------------------------------------------------
// Microsoft allocation extensions
// ------------------------------------------------------

static void* mi__expand(void* p, size_t newsize) {
  void* res = mi_expand(p, newsize);
  if (res == NULL) errno = ENOMEM;
  return res;
}

typedef size_t mi_nothrow_t;

static void mi_free_nothrow(void* p, mi_nothrow_t tag) {
  UNUSED(tag);
  mi_free(p);
}

// Versions of `free`, `realloc`, `recalloc`, `expand` and `msize`
// that are used during termination and are no-ops.
static void mi_free_term(void* p) {
  UNUSED(p);
}

static void mi_free_size_term(void* p, size_t size) {
  UNUSED(size);
  UNUSED(p);
}

static void mi_free_nothrow_term(void* p, mi_nothrow_t tag) {
  UNUSED(tag);
  UNUSED(p);
}

static void* mi_realloc_term(void* p, size_t newsize) {
  UNUSED(p); UNUSED(newsize);
  return NULL;
}

static void* mi__recalloc_term(void* p, size_t newcount, size_t newsize) {
  UNUSED(p); UNUSED(newcount); UNUSED(newsize);
  return NULL;
}

static void* mi__expand_term(void* p, size_t newsize) {
  UNUSED(p); UNUSED(newsize);
  return NULL;
}

static size_t mi__msize_term(void* p) {
  UNUSED(p);
  return 0;
}


// Debug versions, forward to base versions (that get patched)

static void* mi__malloc_dbg(size_t size, int block_type, const char* fname, int line) {
  UNUSED(block_type); UNUSED(fname); UNUSED(line);
  return _malloc_base(size);
}

static void* mi__calloc_dbg(size_t count, size_t size, int block_type, const char* fname, int line) {
  UNUSED(block_type); UNUSED(fname); UNUSED(line);
  return _calloc_base(count, size);
}

static void* mi__realloc_dbg(void* p, size_t size, int block_type, const char* fname, int line) {
  UNUSED(block_type); UNUSED(fname); UNUSED(line);
  return _realloc_base(p, size);
}

static void mi__free_dbg(void* p, int block_type) {
  UNUSED(block_type);
  _free_base(p);
}


// the `recalloc`,`expand`, and `msize` don't have base versions and thus need a separate term version

static void* mi__recalloc_dbg(void* p, size_t count, size_t size, int block_type, const char* fname, int line) {
  UNUSED(block_type); UNUSED(fname); UNUSED(line);
  return mi_recalloc(p, count, size);
}

static void* mi__expand_dbg(void* p, size_t size, int block_type, const char* fname, int line) {
  UNUSED(block_type); UNUSED(fname); UNUSED(line);
  return mi__expand(p, size);
}

static size_t mi__msize_dbg(void* p, int block_type) {
  UNUSED(block_type);
  return mi_usable_size(p);
}

static void* mi__recalloc_dbg_term(void* p, size_t count, size_t size, int block_type, const char* fname, int line) {
  UNUSED(block_type); UNUSED(fname); UNUSED(line);
  return mi__recalloc_term(p, count, size);
}

static void* mi__expand_dbg_term(void* p, size_t size, int block_type, const char* fname, int line) {
  UNUSED(block_type); UNUSED(fname); UNUSED(line);
  return mi__expand_term(p, size);
}

static size_t mi__msize_dbg_term(void* p, int block_type) {
  UNUSED(block_type);
  return mi__msize_term(p);
}


// ------------------------------------------------------
// implement our own global atexit handler
// ------------------------------------------------------
typedef void (cbfun_t)(void);
typedef int (atexit_fun_t)(cbfun_t* fn);
typedef uintptr_t encoded_t;

typedef struct exit_list_s {
  encoded_t  functions;  // encoded pointer to array of encoded function pointers
  size_t     count;
  size_t     capacity;
} exit_list_t;

#define MI_EXIT_INC (64)

static exit_list_t atexit_list = { 0, 0, 0 };
static exit_list_t at_quick_exit_list = { 0, 0, 0 };
static CRITICAL_SECTION atexit_lock;

// encode/decode function pointers with a random canary for security
static encoded_t canary;

static inline void *decode(encoded_t x) {
  return (void*)(x^canary);
}

static inline encoded_t encode(void* p) {
  return ((uintptr_t)p ^ canary);
}


static void init_canary()
{
  canary = _mi_random_init(0);
  atexit_list.functions = at_quick_exit_list.functions = encode(NULL);
}


// initialize the list
static void mi_initialize_atexit(void) {
  InitializeCriticalSection(&atexit_lock);
  init_canary();
}

// register an exit function
static int mi_register_atexit(exit_list_t* list, cbfun_t* fn) {
  if (fn == NULL) return EINVAL;
  EnterCriticalSection(&atexit_lock);
  encoded_t* functions = (encoded_t*)decode(list->functions);
  if (list->count >= list->capacity) {   // at first `functions == decode(0) == NULL`
    encoded_t* newf = (encoded_t*)mi_recalloc(functions, list->capacity + MI_EXIT_INC, sizeof(cbfun_t*));
    if (newf != NULL) {
      list->capacity += MI_EXIT_INC;
      list->functions = encode(newf);
      functions = newf;
    }
  }
  int result;
  if (list->count < list->capacity && functions != NULL) {
    functions[list->count] = encode(fn);
    list->count++;
    result = 0; // success
  }
  else {
    result = ENOMEM;
  }
  LeaveCriticalSection(&atexit_lock);
  return result;
}

// Register a global `atexit` function
static int mi_atexit(cbfun_t* fn) {
  return mi_register_atexit(&atexit_list,fn);
}

static int mi_at_quick_exit(cbfun_t* fn) {
  return mi_register_atexit(&at_quick_exit_list,fn);
}

static int mi_register_onexit(void* table, cbfun_t* fn) {
  // TODO: how can we distinguish a quick_exit from atexit?
  return mi_atexit(fn);
}

// Execute exit functions in a list
static void mi_execute_exit_list(exit_list_t* list) {
  // copy and zero the list structure
  EnterCriticalSection(&atexit_lock);
  exit_list_t clist = *list;
  memset(list,0,sizeof(*list));
  LeaveCriticalSection(&atexit_lock);

  // now execute the functions outside of the lock
  encoded_t* functions = (encoded_t*)decode(clist.functions);
  if (functions != NULL) {
    for (size_t i = clist.count; i > 0; i--) {  // careful with unsigned count down..
      cbfun_t* fn = (cbfun_t*)decode(functions[i-1]);
      if (fn==NULL) break; // corrupted!
      fn();
    }
    mi_free(functions);
  }
}



// ------------------------------------------------------
// Jump assembly instructions for patches
// ------------------------------------------------------

#if defined(_M_IX86) || defined(_M_X64)

#define MI_JUMP_SIZE  14   // at most 2+4+8 for a long jump or 1+5 for a short one

typedef struct mi_jump_s {
  uint8_t opcodes[MI_JUMP_SIZE];
} mi_jump_t;

void mi_jump_restore(void* current, const mi_jump_t* saved) {
  memcpy(current, &saved->opcodes, MI_JUMP_SIZE);
}

void mi_jump_write(void* current, void* target, mi_jump_t* save) {
  if (save != NULL) {
    memcpy(&save->opcodes, current, MI_JUMP_SIZE);
  }
  uint8_t*   opcodes = ((mi_jump_t*)current)->opcodes;
  ptrdiff_t  diff    = (uint8_t*)target - (uint8_t*)current;
  uint32_t   ofs32   = (uint32_t)diff;
  #ifdef _M_X64
  uint64_t   ofs64   = (uint64_t)diff;
  if (ofs64 != (uint64_t)ofs32) {
    // use long jump
    opcodes[0] = 0xFF;
    opcodes[1] = 0x25;
    *((uint32_t*)&opcodes[2]) = 0;
    *((uint64_t*)&opcodes[6]) = (uint64_t)target;
  }
  else
  #endif
  {
    // use short jump
    opcodes[0] = 0xE9;
    *((uint32_t*)&opcodes[1]) = ofs32 - 5 /* size of the short jump instruction */;
  }
}

#elif defined(_M_ARM64)

#define MI_JUMP_SIZE  16

typedef struct mi_jump_s {
  uint8_t opcodes[MI_JUMP_SIZE];
} mi_jump_t;

void mi_jump_restore(void* current, const mi_jump_t* saved) {
  memcpy(current, &saved->opcodes, MI_JUMP_SIZE);
}

void mi_jump_write(void* current, void* target, mi_jump_t* save) {
  if (save != NULL) {
    memcpy(&save->opcodes, current, MI_JUMP_SIZE);
  }
  uint8_t*  opcodes = ((mi_jump_t*)current)->opcodes;
  uint64_t  diff = (uint8_t*)target - (uint8_t*)current;

  // 0x50 0x00 0x00 0x58   ldr x16, .+8   # load PC relative +8
  // 0x00 0x02 0x3F 0xD6   blr x16        # and jump
  //                       <address>
  //                       <address>
  static const uint8_t jump_opcodes[8] = { 0x50, 0x00, 0x00, 0x58, 0x00, 0x02, 0x3F, 0xD6 };
  memcpy(&opcodes[0], jump_opcodes, sizeof(jump_opcodes));
  *((uint64_t*)&opcodes[8]) = diff;
}

#else
#error "define jump instructions for this platform"
#endif


// ------------------------------------------------------
// Patches
// ------------------------------------------------------
typedef enum patch_apply_e {
  PATCH_NONE,
  PATCH_TARGET,
  PATCH_TARGET_TERM
} patch_apply_t;

#define MAX_ENTRIES  4      // maximum number of patched entry points (like `malloc` in ucrtbase and msvcrt)

typedef struct mi_patch_s {
  const char*   name;                   // name of the function to patch
  void*         target;                 // the address of the new target (never NULL)
  void*         target_term;            // the address of the target during termination (or NULL)
  patch_apply_t applied;                // what target has been applied?
  void*         originals[MAX_ENTRIES]; // the resolved addresses of the function (or NULLs)
  mi_jump_t     saves[MAX_ENTRIES];     // the saved instructions in case it was applied
} mi_patch_t;

#define MI_PATCH_NAME3(name,target,term)  { name, &target, &term, PATCH_NONE, {NULL,NULL,NULL,NULL} }
#define MI_PATCH_NAME2(name,target)       { name, &target, NULL, PATCH_NONE, {NULL,NULL,NULL,NULL} }
#define MI_PATCH3(name,target,term)       MI_PATCH_NAME3(#name, target, term)
#define MI_PATCH2(name,target)            MI_PATCH_NAME2(#name, target)
#define MI_PATCH1(name)                   MI_PATCH2(name,mi_##name)

static mi_patch_t patches[] = {
  // we implement our own global exit handler (as the CRT versions do a realloc internally)
  //MI_PATCH2(_crt_atexit, mi_atexit),
  //MI_PATCH2(_crt_at_quick_exit, mi_at_quick_exit),
  MI_PATCH2(_setmaxstdio, mi_setmaxstdio),
  MI_PATCH2(_register_onexit_function, mi_register_onexit),

  // override higher level atexit functions so we can implement at_quick_exit correcty
  MI_PATCH2(atexit, mi_atexit),
  MI_PATCH2(at_quick_exit, mi_at_quick_exit),

  // regular entries
  MI_PATCH2(malloc, mi_malloc),
  MI_PATCH2(calloc, mi_calloc),
  MI_PATCH3(realloc, mi_realloc,mi_realloc_term),
  MI_PATCH3(free, mi_free,mi_free_term),
  
  // extended api
  MI_PATCH2(_strdup, mi_strdup),
  MI_PATCH2(_strndup, mi_strndup),
  MI_PATCH3(_expand, mi__expand,mi__expand_term),
  MI_PATCH3(_recalloc, mi_recalloc,mi__recalloc_term),
  MI_PATCH3(_msize, mi_usable_size,mi__msize_term),

  // base versions 
  MI_PATCH2(_malloc_base, mi_malloc),
  MI_PATCH2(_calloc_base, mi_calloc),
  MI_PATCH3(_realloc_base, mi_realloc,mi_realloc_term),
  MI_PATCH3(_free_base, mi_free,mi_free_term),

  // these base versions are in the crt but without import records
  MI_PATCH_NAME3("_recalloc_base", mi_recalloc,mi__recalloc_term),
  MI_PATCH_NAME3("_msize_base", mi_usable_size,mi__msize_term),

  // debug
  MI_PATCH2(_malloc_dbg, mi__malloc_dbg),
  MI_PATCH2(_realloc_dbg, mi__realloc_dbg),
  MI_PATCH2(_calloc_dbg, mi__calloc_dbg),
  MI_PATCH2(_free_dbg, mi__free_dbg),

  MI_PATCH3(_expand_dbg, mi__expand_dbg, mi__expand_dbg_term),
  MI_PATCH3(_recalloc_dbg, mi__recalloc_dbg, mi__recalloc_dbg_term),
  MI_PATCH3(_msize_dbg, mi__msize_dbg, mi__msize_dbg_term),

#if 0
  // override new/delete variants for efficiency (?)
#ifdef _WIN64
  // 64 bit new/delete
  MI_PATCH_NAME2("??2@YAPEAX_K@Z", mi_new),
  MI_PATCH_NAME2("??_U@YAPEAX_K@Z", mi_new),
  MI_PATCH_NAME3("??3@YAXPEAX@Z", mi_free, mi_free_term),  
  MI_PATCH_NAME3("??_V@YAXPEAX@Z", mi_free, mi_free_term), 
  MI_PATCH_NAME3("??3@YAXPEAX_K@Z", mi_free_size, mi_free_size_term), // delete sized
  MI_PATCH_NAME3("??_V@YAXPEAX_K@Z", mi_free_size, mi_free_size_term), // delete sized
  MI_PATCH_NAME2("??2@YAPEAX_KAEBUnothrow_t@std@@@Z", mi_new),
  MI_PATCH_NAME2("??_U@YAPEAX_KAEBUnothrow_t@std@@@Z", mi_new),
  MI_PATCH_NAME3("??3@YAXPEAXAEBUnothrow_t@std@@@Z", mi_free_nothrow, mi_free_nothrow_term),
  MI_PATCH_NAME3("??_V@YAXPEAXAEBUnothrow_t@std@@@Z", mi_free_nothrow, mi_free_nothrow_term),
  
  
#else
  // 32 bit new/delete
  MI_PATCH_NAME2("??2@YAPAXI@Z", mi_new),
  MI_PATCH_NAME2("??_U@YAPAXI@Z", mi_new),
  MI_PATCH_NAME3("??3@YAXPAX@Z", mi_free, mi_free_term),
  MI_PATCH_NAME3("??_V@YAXPAX@Z", mi_free, mi_free_term),
  MI_PATCH_NAME3("??3@YAXPAXI@Z", mi_free_size, mi_free_size_term), // delete sized
  MI_PATCH_NAME3("??_V@YAXPAXI@Z", mi_free_size, mi_free_size_term), // delete sized

  MI_PATCH_NAME2("??2@YAPAXIABUnothrow_t@std@@@Z", mi_new),
  MI_PATCH_NAME2("??_U@YAPAXIABUnothrow_t@std@@@Z", mi_new),
  MI_PATCH_NAME3("??3@YAXPAXABUnothrow_t@std@@@Z", mi_free_nothrow, mi_free_nothrow_term),
  MI_PATCH_NAME3("??_V@YAXPAXABUnothrow_t@std@@@Z", mi_free_nothrow, mi_free_nothrow_term),

#endif
#endif
  { NULL, NULL, NULL, PATCH_NONE, {NULL,NULL,NULL,NULL} }
};


// Apply a patch
static bool mi_patch_apply(mi_patch_t* patch, patch_apply_t apply)
{
  if (patch->originals[0] == NULL) return true;  // unresolved
  if (apply == PATCH_TARGET_TERM && patch->target_term == NULL) apply = PATCH_TARGET;  // avoid re-applying non-term variants
  if (patch->applied == apply) return false;

  for (int i = 0; i < MAX_ENTRIES; i++) {
    void* original = patch->originals[i];
    if (original == NULL) break; // no more

    DWORD protect = PAGE_READWRITE;
    if (!VirtualProtect(original, MI_JUMP_SIZE, PAGE_EXECUTE_READWRITE, &protect)) return false;
    if (apply == PATCH_NONE) {
      mi_jump_restore(original, &patch->saves[i]);
    }
    else {
      void* target = (apply == PATCH_TARGET ? patch->target : patch->target_term);
      mi_assert_internal(target != NULL);
      if (target != NULL) mi_jump_write(original, target, &patch->saves[i]);
    }
    VirtualProtect(original, MI_JUMP_SIZE, protect, &protect);
  }
  patch->applied = apply;
  return true;
}

// Apply all patches
static bool _mi_patches_apply(patch_apply_t apply, patch_apply_t* previous) {
  static patch_apply_t current = PATCH_NONE;
  if (previous != NULL) *previous = current;
  if (current == apply) return true;
  current = apply;
  bool ok = true;
  for (size_t i = 0; patches[i].name != NULL; i++) {
    if (!mi_patch_apply(&patches[i], apply)) ok = false;
  }
  return ok;
}

// Export the following three functions just in case
// a user needs that level of control.

// Disable all patches
mi_decl_export void mi_patches_disable(void) {
  _mi_patches_apply(PATCH_NONE, NULL);
}

// Enable all patches normally
mi_decl_export bool mi_patches_enable(void) {
  return _mi_patches_apply( PATCH_TARGET, NULL );
}

// Enable all patches in termination phase where free is a no-op
mi_decl_export bool mi_patches_enable_term(void) {
  return _mi_patches_apply(PATCH_TARGET_TERM, NULL);
}

// ------------------------------------------------------
// Stub for _setmaxstdio
// ------------------------------------------------------

static int __cdecl mi_setmaxstdio(int newmax) {
  patch_apply_t previous;
  _mi_patches_apply(PATCH_NONE, &previous); // disable patches
  int result = _setmaxstdio(newmax);       // call original function (that calls original CRT recalloc)
  _mi_patches_apply(previous,NULL);         // and re-enable patches
  return result;
}


// ------------------------------------------------------
// Resolve addresses dynamically
// ------------------------------------------------------

// Try to resolve patches for a given module (DLL)
static void mi_module_resolve(const char* fname, HMODULE mod, int priority) {
  // see if any patches apply
  for (size_t i = 0; patches[i].name != NULL; i++) {
    mi_patch_t* patch = &patches[i];
    if (patch->applied == PATCH_NONE) {
      // find an available entry
      int i = 0;
      while (i < MAX_ENTRIES && patch->originals[i] != NULL) i++;
      if (i < MAX_ENTRIES) {
        void* addr = GetProcAddress(mod, patch->name);
        if (addr != NULL) {
          // found it! set the address
          patch->originals[i] = addr;          
          _mi_trace_message("  override %s at %s!%p (entry %i)\n", patch->name, fname, addr, i);
        }
      }
    }
  }
}

#define MIMALLOC_NAME "mimalloc-override.dll"
#define UCRTBASE_NAME "ucrtbase.dll"
#define UCRTBASED_NAME "ucrtbased.dll"

// Resolve addresses of all patches by inspecting the loaded modules
static atexit_fun_t* crt_atexit = NULL;
static atexit_fun_t* crt_at_quick_exit = NULL;


static bool mi_patches_resolve(void) {
  // get all loaded modules
  HANDLE process = GetCurrentProcess(); // always -1, no need to release
  DWORD needed = 0;
  HMODULE modules[400];  // try to stay under 4k to not trigger the guard page
  EnumProcessModules(process, modules, sizeof(modules), &needed);
  if (needed == 0) return false;
  int count = needed / sizeof(HMODULE);
  int ucrtbase_index = 0;
  int mimalloc_index = 0;
  // iterate through the loaded modules
  _mi_trace_message("overriding malloc dynamically...\n");
  for (int i = 0; i < count;  i++) {
    HMODULE mod = modules[i];
    char filename[MAX_PATH] = { 0 };
    DWORD slen = GetModuleFileName(mod, filename, MAX_PATH);
    if (slen > 0 && slen < MAX_PATH) {
      // filter out potential crt modules only
      filename[slen] = 0;
      const char* lastsep = strrchr(filename, '\\');
      const char* basename = (lastsep==NULL ? filename : lastsep+1);
      _mi_trace_message("  %i: dynamic module %s\n", i, filename);

      // remember indices so we can check load order (in debug mode)
      if (_stricmp(basename, MIMALLOC_NAME) == 0) mimalloc_index = i;
      if (_stricmp(basename, UCRTBASE_NAME) == 0) ucrtbase_index = i;
      if (_stricmp(basename, UCRTBASED_NAME) == 0) ucrtbase_index = i;

      // see if we potentially patch in this module
      int priority = 0; 
      if (i == 0) priority = 2; // main module to allow static crt linking
      else if (_strnicmp(basename, "ucrt", 4) == 0) priority = 3;   // new ucrtbase.dll in windows 10
      // NOTE: don't override msvcr -- leads to crashes in setlocale (needs more testing)
      // else if (_strnicmp(basename, "msvcr", 5) == 0) priority = 1;  // older runtimes      
      
      if (priority > 0) {
        // probably found a crt module, try to patch it
        mi_module_resolve(basename,mod,priority);

        // try to find the atexit functions for the main process (in `ucrtbase.dll`)
        if (crt_atexit==NULL) crt_atexit = (atexit_fun_t*)GetProcAddress(mod, "_crt_atexit");
        if (crt_at_quick_exit == NULL) crt_at_quick_exit = (atexit_fun_t*)GetProcAddress(mod, "_crt_at_quick_exit");
      }
    }
  }
  int diff = mimalloc_index - ucrtbase_index;
  if (diff > 1) {
    _mi_warning_message("warning: the \"mimalloc-override\" DLL seems not to load before or right after the C runtime (\"ucrtbase\").\n"
                        "  Try to fix this by changing the linking order.\n");
  }
  return true;
}


// ------------------------------------------------------
// Dll Entry
// ------------------------------------------------------

extern BOOL WINAPI _DllMainCRTStartup(HINSTANCE inst, DWORD reason, LPVOID reserved);

static DWORD mi_fls_unwind_entry;
static void NTAPI mi_fls_unwind(PVOID value) {
  if (value != NULL) mi_patches_enable();   // and re-enable normal patches again for DLL's loaded after us
  return;
}

static void mi_patches_atexit(void) {
  mi_execute_exit_list(&atexit_list);
  mi_patches_enable_term();             // enter termination phase and patch realloc/free with a no-op
}

static void mi_patches_at_quick_exit(void) {
  mi_execute_exit_list(&at_quick_exit_list);
  mi_patches_enable_term();             // enter termination phase and patch realloc/free with a no-op
}

__declspec(dllexport) BOOL WINAPI DllEntry(HINSTANCE inst, DWORD reason, LPVOID reserved) {
  if (reason == DLL_PROCESS_ATTACH) {
    __security_init_cookie();
  }
  else if (reason == DLL_PROCESS_DETACH) {
    // enter termination phase for good now
    mi_patches_enable_term();
  }
  // C runtime main
  BOOL ok = _DllMainCRTStartup(inst, reason, reserved);
  if (reason == DLL_PROCESS_ATTACH && ok) {
    // Now resolve patches
    ok = mi_patches_resolve();
    if (ok) {
      // and register our unwind entry (this must be after resolving due to possible delayed DLL initialization from GetProcAddress)
      mi_fls_unwind_entry = FlsAlloc(&mi_fls_unwind);
      if (mi_fls_unwind_entry != FLS_OUT_OF_INDEXES) {
        FlsSetValue(mi_fls_unwind_entry, (void*)1);
      }

      // register our patch disabler in the global exit list
      mi_initialize_atexit();
      if (crt_atexit != NULL)        (*crt_atexit)(&mi_patches_atexit);
      if (crt_at_quick_exit != NULL) (*crt_at_quick_exit)(&mi_patches_at_quick_exit);

      // and patch !  this also redirects the `atexit` handling for the global exit list
      mi_patches_enable();

      // hide internal allocation
      mi_stats_reset();
    }
  }
  return ok;
}
