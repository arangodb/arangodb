
/* ELF Relocations */

#define ELF_RELOC(name, value) name = value,

/** i386 relocations. */
enum  RELOC_i386 {

#ifndef ELF_RELOC
#error "ELF_RELOC must be defined"
#endif

/* TODO: this is just a subset */
ELF_RELOC(R_386_NONE,           0)
ELF_RELOC(R_386_32,             1)
ELF_RELOC(R_386_PC32,           2)
ELF_RELOC(R_386_GOT32,          3)
ELF_RELOC(R_386_PLT32,          4)
ELF_RELOC(R_386_COPY,           5)
ELF_RELOC(R_386_GLOB_DAT,       6)
ELF_RELOC(R_386_JUMP_SLOT,      7)
ELF_RELOC(R_386_RELATIVE,       8)
ELF_RELOC(R_386_GOTOFF,         9)
ELF_RELOC(R_386_GOTPC,          10)
ELF_RELOC(R_386_32PLT,          11)
ELF_RELOC(R_386_TLS_TPOFF,      14)
ELF_RELOC(R_386_TLS_IE,         15)
ELF_RELOC(R_386_TLS_GOTIE,      16)
ELF_RELOC(R_386_TLS_LE,         17)
ELF_RELOC(R_386_TLS_GD,         18)
ELF_RELOC(R_386_TLS_LDM,        19)
ELF_RELOC(R_386_16,             20)
ELF_RELOC(R_386_PC16,           21)
ELF_RELOC(R_386_8,              22)
ELF_RELOC(R_386_PC8,            23)
ELF_RELOC(R_386_TLS_GD_32,      24)
ELF_RELOC(R_386_TLS_GD_PUSH,    25)
ELF_RELOC(R_386_TLS_GD_CALL,    26)
ELF_RELOC(R_386_TLS_GD_POP,     27)
ELF_RELOC(R_386_TLS_LDM_32,     28)
ELF_RELOC(R_386_TLS_LDM_PUSH,   29)
ELF_RELOC(R_386_TLS_LDM_CALL,   30)
ELF_RELOC(R_386_TLS_LDM_POP,    31)
ELF_RELOC(R_386_TLS_LDO_32,     32)
ELF_RELOC(R_386_TLS_IE_32,      33)
ELF_RELOC(R_386_TLS_LE_32,      34)
ELF_RELOC(R_386_TLS_DTPMOD32,   35)
ELF_RELOC(R_386_TLS_DTPOFF32,   36)
ELF_RELOC(R_386_TLS_TPOFF32,    37)
ELF_RELOC(R_386_TLS_GOTDESC,    39)
ELF_RELOC(R_386_TLS_DESC_CALL,  40)
ELF_RELOC(R_386_TLS_DESC,       41)
ELF_RELOC(R_386_IRELATIVE,      42)
ELF_RELOC(R_386_NUM,            43)
};

#ifndef SWITCH_VERSION

#ifdef FROZEN_VERSION
#include "frozen/map.h"
#else
#include <map>
#endif

#ifdef FROZEN_VERSION
constexpr
frozen::map<RELOC_i386, const char*, 41>
#else
const
std::map<RELOC_i386, const char*>
#endif
e2s = {
    { RELOC_i386::R_386_NONE,          "NONE"},
    { RELOC_i386::R_386_32,            "R32"},
    { RELOC_i386::R_386_PC32,          "PC32"},
    { RELOC_i386::R_386_GOT32,         "GOT32"},
    { RELOC_i386::R_386_PLT32,         "PLT32"},
    { RELOC_i386::R_386_COPY,          "COPY"},
    { RELOC_i386::R_386_GLOB_DAT,      "GLOB_DAT"},
    { RELOC_i386::R_386_JUMP_SLOT,     "JUMP_SLOT"},
    { RELOC_i386::R_386_RELATIVE,      "RELATIVE"},
    { RELOC_i386::R_386_GOTOFF,        "GOTOFF"},
    { RELOC_i386::R_386_GOTPC,         "GOTPC"},
    { RELOC_i386::R_386_32PLT,         "R32PLT"},
    { RELOC_i386::R_386_TLS_TPOFF,     "TLS_TPOFF"},
    { RELOC_i386::R_386_TLS_IE,        "TLS_IE"},
    { RELOC_i386::R_386_TLS_GOTIE,     "TLS_GOTIE"},
    { RELOC_i386::R_386_TLS_LE,        "TLS_LE"},
    { RELOC_i386::R_386_TLS_GD,        "TLS_GD"},
    { RELOC_i386::R_386_TLS_LDM,       "TLS_LDM"},
    { RELOC_i386::R_386_16,            "R16"},
    { RELOC_i386::R_386_PC16,          "PC16"},
    { RELOC_i386::R_386_8,             "R8"},
    { RELOC_i386::R_386_PC8,           "PC8"},
    { RELOC_i386::R_386_TLS_GD_32,     "TLS_GD_32"},
    { RELOC_i386::R_386_TLS_GD_PUSH,   "TLS_GD_PUSH"},
    { RELOC_i386::R_386_TLS_GD_CALL,   "TLS_GD_CALL"},
    { RELOC_i386::R_386_TLS_GD_POP,    "TLS_GD_POP"},
    { RELOC_i386::R_386_TLS_LDM_32,    "TLS_LDM_32"},
    { RELOC_i386::R_386_TLS_LDM_PUSH,  "TLS_LDM_PUSH"},
    { RELOC_i386::R_386_TLS_LDM_CALL,  "TLS_LDM_CALL"},
    { RELOC_i386::R_386_TLS_LDM_POP,   "TLS_LDM_POP"},
    { RELOC_i386::R_386_TLS_LDO_32,    "TLS_LDO_32"},
    { RELOC_i386::R_386_TLS_IE_32,     "TLS_IE_32"},
    { RELOC_i386::R_386_TLS_LE_32,     "TLS_LE_32"},
    { RELOC_i386::R_386_TLS_DTPMOD32,  "TLS_DTPMOD32"},
    { RELOC_i386::R_386_TLS_DTPOFF32,  "TLS_DTPOFF32"},
    { RELOC_i386::R_386_TLS_TPOFF32,   "TLS_TPOFF32"},
    { RELOC_i386::R_386_TLS_GOTDESC,   "TLS_GOTDESC"},
    { RELOC_i386::R_386_TLS_DESC_CALL, "TLS_DESC_CALL"},
    { RELOC_i386::R_386_TLS_DESC,      "TLS_DESC"},
    { RELOC_i386::R_386_IRELATIVE,     "IRELATIVE"},
    { RELOC_i386::R_386_NUM,           "NUM"},
};

char const * enum_to_string(RELOC_i386 e) {
  return e2s.at(e);
}
#else
char const * enum_to_string(RELOC_i386 e) {
  switch(e) {
    case RELOC_i386::R_386_NONE: return "NONE";
    case RELOC_i386::R_386_32: return "R32";
    case RELOC_i386::R_386_PC32: return "PC32";
    case RELOC_i386::R_386_GOT32: return "GOT32";
    case RELOC_i386::R_386_PLT32: return "PLT32";
    case RELOC_i386::R_386_COPY: return "COPY";
    case RELOC_i386::R_386_GLOB_DAT: return "GLOB_DAT";
    case RELOC_i386::R_386_JUMP_SLOT: return "JUMP_SLOT";
    case RELOC_i386::R_386_RELATIVE: return "RELATIVE";
    case RELOC_i386::R_386_GOTOFF: return "GOTOFF";
    case RELOC_i386::R_386_GOTPC: return "GOTPC";
    case RELOC_i386::R_386_32PLT: return "R32PLT";
    case RELOC_i386::R_386_TLS_TPOFF: return "TLS_TPOFF";
    case RELOC_i386::R_386_TLS_IE: return "TLS_IE";
    case RELOC_i386::R_386_TLS_GOTIE: return "TLS_GOTIE";
    case RELOC_i386::R_386_TLS_LE: return "TLS_LE";
    case RELOC_i386::R_386_TLS_GD: return "TLS_GD";
    case RELOC_i386::R_386_TLS_LDM: return "TLS_LDM";
    case RELOC_i386::R_386_16: return "R16";
    case RELOC_i386::R_386_PC16: return "PC16";
    case RELOC_i386::R_386_8: return "R8";
    case RELOC_i386::R_386_PC8: return "PC8";
    case RELOC_i386::R_386_TLS_GD_32: return "TLS_GD_32";
    case RELOC_i386::R_386_TLS_GD_PUSH: return "TLS_GD_PUSH";
    case RELOC_i386::R_386_TLS_GD_CALL: return "TLS_GD_CALL";
    case RELOC_i386::R_386_TLS_GD_POP: return "TLS_GD_POP";
    case RELOC_i386::R_386_TLS_LDM_32: return "TLS_LDM_32";
    case RELOC_i386::R_386_TLS_LDM_PUSH: return "TLS_LDM_PUSH";
    case RELOC_i386::R_386_TLS_LDM_CALL: return "TLS_LDM_CALL";
    case RELOC_i386::R_386_TLS_LDM_POP: return "TLS_LDM_POP";
    case RELOC_i386::R_386_TLS_LDO_32: return "TLS_LDO_32";
    case RELOC_i386::R_386_TLS_IE_32: return "TLS_IE_32";
    case RELOC_i386::R_386_TLS_LE_32: return "TLS_LE_32";
    case RELOC_i386::R_386_TLS_DTPMOD32: return "TLS_DTPMOD32";
    case RELOC_i386::R_386_TLS_DTPOFF32: return "TLS_DTPOFF32";
    case RELOC_i386::R_386_TLS_TPOFF32: return "TLS_TPOFF32";
    case RELOC_i386::R_386_TLS_GOTDESC: return "TLS_GOTDESC";
    case RELOC_i386::R_386_TLS_DESC_CALL: return "TLS_DESC_CALL";
    case RELOC_i386::R_386_TLS_DESC: return "TLS_DESC";
    case RELOC_i386::R_386_IRELATIVE: return "IRELATIVE";
    case RELOC_i386::R_386_NUM: return "NUM";
  }
}

#endif

int main() {
  puts(enum_to_string(RELOC_i386::R_386_8));
  return 0;
}
