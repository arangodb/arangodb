// Copyright (C) Anders Kjersem. Licensed under the zlib/libpng license, see License.txt for details.

#pragma once
#include <TChar.h>


typedef TCHAR NSISCH;
#define NSISCALL      __stdcall

namespace NSIS {

__forceinline void* NSISCALL MemAlloc(SIZE_T cb) {return GlobalAlloc(LPTR,cb);}
__forceinline void NSISCALL MemFree(void* p) {if (p)GlobalFree(p);}

enum {
INST_0,         // $0
INST_1,         // $1
INST_2,         // $2
INST_3,         // $3
INST_4,         // $4
INST_5,         // $5
INST_6,         // $6
INST_7,         // $7
INST_8,         // $8
INST_9,         // $9
INST_R0,        // $R0
INST_R1,        // $R1
INST_R2,        // $R2
INST_R3,        // $R3
INST_R4,        // $R4
INST_R5,        // $R5
INST_R6,        // $R6
INST_R7,        // $R7
INST_R8,        // $R8
INST_R9,        // $R9
INST_CMDLINE,   // $CMDLINE
INST_INSTDIR,   // $INSTDIR
INST_OUTDIR,    // $OUTDIR
INST_EXEDIR,    // $EXEDIR
INST_LANG,      // $LANGUAGE
__INST_LAST,
VIDX_TEMP=(INST_LANG+1), //#define state_temp_dir            g_usrvars[25]
VIDX_PLUGINSDIR,//#  define state_plugins_dir       g_usrvars[26]
VIDX_EXEPATH,//#define state_exe_path            g_usrvars[27]
VIDX_EXEFILENAME,//#define state_exe_file            g_usrvars[28]
VIDX_STATECLICKNEXT,//#define state_click_next          g_usrvars[30]
__VIDX_UNDOCLAST
};


enum NSPIM
{
	NSPIM_UNLOAD,    // This is the last message a plugin gets, do final cleanup
	NSPIM_GUIUNLOAD, // Called after .onGUIEnd
};
typedef UINT_PTR (__cdecl*NSISPLUGINCALLBACK)(enum NSPIM);



typedef struct _stack_t {
  struct _stack_t *next;
  NSISCH text[ANYSIZE_ARRAY];
} stack_t;

typedef struct {
  int autoclose;
  int all_user_var;
  int exec_error;
  int abort;
  int exec_reboot;
  int reboot_called;
  int XXX_cur_insttype; // deprecated
  int plugin_api_version;
  int silent;
  int instdir_error;
  int rtl;
  int errlvl;
//NSIS v2.3x ?
  int alter_reg_view;
  int status_update;
} exec_flags_type;

typedef struct {
  exec_flags_type *exec_flags;
  int (NSISCALL *ExecuteCodeSegment)(int, HWND);
  void (NSISCALL *validate_filename)(char *);
  int (NSISCALL *RegisterPluginCallback)(HMODULE, NSISPLUGINCALLBACK); // returns 0 on success, 1 if already registered and < 0 on errors
} extra_parameters;

#define WM_NOTIFY_OUTER_NEXT (WM_USER+0x8) // sent to the outer window to tell it to go to the next inner window
#define WM_NOTIFY_CUSTOM_READY (WM_USER+0xd) // custom pages should send this message to let NSIS know they're ready
#define WM_NOTIFY_INSTPROC_DONE (WM_USER+0x4)// sent to the last child window to tell it that the install thread is done
#define WM_NOTIFY_START (WM_USER+0x5)// sent to every child window to tell it it can start executing NSIS code
#define WM_NOTIFY_INIGO_MONTOYA (WM_USER+0xb)// sent to every child window to tell it it is closing soon
#define WM_IN_UPDATEMSG (WM_USER+0xf)// update message used by DirProc and SelProc for space display
#define WM_TREEVIEW_KEYHACK (WM_USER+0x13)// simulates clicking on the tree
#define WM_NOTIFY_SELCHANGE (WM_USER+0x19)// notifies a component selection change (.onMouseOverSection)
#define WM_NOTIFY_INSTTYPE_CHANGED (WM_USER+0x20)// Notifies that the installation type has been changed by the user

#define NSIS_UNSAFE_GETVAR(VarId,pVars,StrSize) ((pVars)+((VarId)*(StrSize)))


}; /* namespace */
