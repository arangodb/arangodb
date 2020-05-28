// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include <boost/dll/detail/elf_info.hpp>
#include <boost/dll/detail/pe_info.hpp>
#include <boost/dll/detail/macho_info.hpp>
#include <boost/static_assert.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/predef/os.h>

#if BOOST_OS_WINDOWS
#   include <windows.h>
#elif BOOST_OS_MACOS || BOOST_OS_IOS
#   include <mach-o/loader.h>
#   include <mach-o/nlist.h>
#elif BOOST_OS_QNX
// QNX's copy of <elf.h> and <link.h> reside in sys folder
#   include <sys/elf.h>
#else
    #include <elf.h>
#endif

namespace dd = boost::dll::detail;

template <class T1, class T2>
inline std::size_t get_offset(const T1& v1, const T2& v2) {
    const unsigned char* p1 = reinterpret_cast<const unsigned char*>(&v1);
    const unsigned char* p2 = reinterpret_cast<const unsigned char*>(&v2);

    if (p1 < p2) {
        return static_cast<std::size_t>(p2 - p1);
    }

    return static_cast<std::size_t>(p1 - p2);
}

#define CHECK_FIELD(Field)                                              \
    BOOST_STATIC_ASSERT(sizeof(v1.Field) == sizeof(v2.Field));          \
    BOOST_TEST(get_offset(v1, v1.Field) == get_offset(v2, v2.Field))   \
    /**/


// ELF structures
template <class T1, class T2>
void elf_header_checks(const T1& v1, const T2& v2) {
    BOOST_STATIC_ASSERT(sizeof(T1) == sizeof(T2));

    CHECK_FIELD(e_ident);
    CHECK_FIELD(e_type);
    CHECK_FIELD(e_machine);
    CHECK_FIELD(e_version);
    CHECK_FIELD(e_entry);
    CHECK_FIELD(e_phoff);
    CHECK_FIELD(e_shoff);
    CHECK_FIELD(e_flags);
    CHECK_FIELD(e_ehsize);
    CHECK_FIELD(e_phentsize);
    CHECK_FIELD(e_phnum);
    CHECK_FIELD(e_shentsize);
    CHECK_FIELD(e_shnum);
    CHECK_FIELD(e_shstrndx);
}

template <class T1, class T2>
void elf_sheader_checks(const T1& v1, const T2& v2) {
    BOOST_STATIC_ASSERT(sizeof(T1) == sizeof(T2));

    CHECK_FIELD(sh_name);
    CHECK_FIELD(sh_type);
    CHECK_FIELD(sh_flags);
    CHECK_FIELD(sh_addr);
    CHECK_FIELD(sh_offset);
    CHECK_FIELD(sh_size);
    CHECK_FIELD(sh_link);
    CHECK_FIELD(sh_info);
    CHECK_FIELD(sh_addralign);
    CHECK_FIELD(sh_entsize);
}

template <class T1, class T2>
void elf_sym_header_checks(const T1& v1, const T2& v2) {
    BOOST_STATIC_ASSERT(sizeof(T1) == sizeof(T2));

    CHECK_FIELD(st_name);
    CHECK_FIELD(st_value);
    CHECK_FIELD(st_size);
    CHECK_FIELD(st_info);
    CHECK_FIELD(st_other);
    CHECK_FIELD(st_shndx);
}


// PE structures
template <class T>
void generic_header_check(const T& v1, const dd::IMAGE_DOS_HEADER_& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));

    CHECK_FIELD(e_magic);
    CHECK_FIELD(e_cblp);
    CHECK_FIELD(e_cp);
    CHECK_FIELD(e_crlc);
    CHECK_FIELD(e_cparhdr);
    CHECK_FIELD(e_minalloc);
    CHECK_FIELD(e_maxalloc);
    CHECK_FIELD(e_ss);
    CHECK_FIELD(e_sp);
    CHECK_FIELD(e_csum);
    CHECK_FIELD(e_ip);
    CHECK_FIELD(e_cs);
    CHECK_FIELD(e_lfarlc);
    CHECK_FIELD(e_ovno);
    CHECK_FIELD(e_res);
    CHECK_FIELD(e_oemid);
    CHECK_FIELD(e_oeminfo);
    CHECK_FIELD(e_res2);
    CHECK_FIELD(e_lfanew);
}

template <class T>
void generic_header_check(const T& v1, const dd::IMAGE_FILE_HEADER_& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));

    CHECK_FIELD(Machine);
    CHECK_FIELD(NumberOfSections);
    CHECK_FIELD(TimeDateStamp);
    CHECK_FIELD(PointerToSymbolTable);
    CHECK_FIELD(NumberOfSymbols);
    CHECK_FIELD(SizeOfOptionalHeader);
    CHECK_FIELD(Characteristics);
}

template <class T>
void generic_header_check(const T& v1, const dd::IMAGE_DATA_DIRECTORY_& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));

    CHECK_FIELD(VirtualAddress);
    CHECK_FIELD(Size);
}

template <class T>
void generic_header_check(const T& v1, const dd::IMAGE_EXPORT_DIRECTORY_& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));
    
    CHECK_FIELD(Characteristics);
    CHECK_FIELD(TimeDateStamp);
    CHECK_FIELD(MajorVersion);
    CHECK_FIELD(MinorVersion);
    CHECK_FIELD(Name);
    CHECK_FIELD(Base);
    CHECK_FIELD(NumberOfFunctions);
    CHECK_FIELD(NumberOfNames);
    CHECK_FIELD(AddressOfFunctions);
    CHECK_FIELD(AddressOfNames);
    CHECK_FIELD(AddressOfNameOrdinals);
}


template <class T>
void generic_header_check(const T& v1, const dd::IMAGE_SECTION_HEADER_& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));
    
    CHECK_FIELD(Name);
    CHECK_FIELD(VirtualAddress);
    CHECK_FIELD(SizeOfRawData);
    CHECK_FIELD(PointerToRawData);
    CHECK_FIELD(PointerToRelocations);
    CHECK_FIELD(PointerToLinenumbers);
    CHECK_FIELD(NumberOfRelocations);
    CHECK_FIELD(NumberOfLinenumbers);
    CHECK_FIELD(Characteristics);
}

template <class T, class AddrT>
void generic_header_check(const T& v1, const dd::IMAGE_OPTIONAL_HEADER_template<AddrT>& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));
    
    CHECK_FIELD(Magic);
    CHECK_FIELD(MajorLinkerVersion);
    CHECK_FIELD(MinorLinkerVersion);
    CHECK_FIELD(SizeOfCode);
    CHECK_FIELD(SizeOfInitializedData);
    CHECK_FIELD(SizeOfUninitializedData);
    CHECK_FIELD(AddressOfEntryPoint);
    CHECK_FIELD(ImageBase);
    CHECK_FIELD(SectionAlignment);
    CHECK_FIELD(FileAlignment);
    CHECK_FIELD(MajorOperatingSystemVersion);
    CHECK_FIELD(MinorOperatingSystemVersion);
    CHECK_FIELD(MajorImageVersion);
    CHECK_FIELD(MinorImageVersion);
    CHECK_FIELD(MajorSubsystemVersion);
    CHECK_FIELD(MinorSubsystemVersion);
    CHECK_FIELD(Win32VersionValue);
    CHECK_FIELD(SizeOfImage);
    CHECK_FIELD(SizeOfHeaders);
    CHECK_FIELD(CheckSum);
    CHECK_FIELD(Subsystem);
    CHECK_FIELD(DllCharacteristics);
    CHECK_FIELD(SizeOfStackReserve);
    CHECK_FIELD(SizeOfStackCommit);
    CHECK_FIELD(SizeOfHeapReserve);
    CHECK_FIELD(SizeOfHeapCommit);
    CHECK_FIELD(LoaderFlags);
    CHECK_FIELD(NumberOfRvaAndSizes);
    CHECK_FIELD(DataDirectory);
}

template <class T, class AddrT>
void generic_header_check(const T& v1, const dd::IMAGE_NT_HEADERS_template<AddrT>& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));

    CHECK_FIELD(Signature);
    CHECK_FIELD(FileHeader);
    CHECK_FIELD(OptionalHeader);
}
template <class T, class AddrT>
void generic_header_check(const T& v1, const dd::mach_header_template<AddrT>& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));

    CHECK_FIELD(magic);
    CHECK_FIELD(cputype);
    CHECK_FIELD(cpusubtype);
    CHECK_FIELD(filetype);
    CHECK_FIELD(ncmds);
    CHECK_FIELD(sizeofcmds);
    //CHECK_FIELD(flags);
}

template <class T, class AddrT>
void generic_header_check(const T& v1, const dd::segment_command_template<AddrT>& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));

    CHECK_FIELD(cmd);          
    CHECK_FIELD(cmdsize);      
    CHECK_FIELD(segname);  
    CHECK_FIELD(vmaddr);       
    CHECK_FIELD(vmsize);       
    CHECK_FIELD(fileoff);
    CHECK_FIELD(filesize);     
    CHECK_FIELD(maxprot);      
    CHECK_FIELD(initprot);     
    CHECK_FIELD(nsects);       
    CHECK_FIELD(flags);        
}

template <class T, class AddrT>
void generic_header_check(const T& v1, const dd::section_template<AddrT>& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));

    CHECK_FIELD(sectname);
    CHECK_FIELD(segname); 
    CHECK_FIELD(addr);    
    CHECK_FIELD(size);    
    CHECK_FIELD(offset);  
    CHECK_FIELD(align);   
    CHECK_FIELD(reloff);  
    CHECK_FIELD(nreloc);  
    CHECK_FIELD(flags);   
    //CHECK_FIELD(reserved vs reserveed1&reserved2);
}

template <class T>
void generic_header_check(const T& v1, const dd::symtab_command_& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));
    
    CHECK_FIELD(cmd);    
    CHECK_FIELD(cmdsize);
    CHECK_FIELD(symoff); 
    CHECK_FIELD(nsyms);  
    CHECK_FIELD(stroff); 
    CHECK_FIELD(strsize);
}

template <class T, class AddrT>
void generic_header_check(const T& v1, const dd::nlist_template<AddrT>& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));
    
    //CHECK_FIELD(n_strx);
    CHECK_FIELD(n_type);
    CHECK_FIELD(n_sect);
    CHECK_FIELD(n_desc);
    CHECK_FIELD(n_value);
}

template <class T>
void generic_header_check(const T& v1, const dd::load_command_& v2) {
    BOOST_STATIC_ASSERT(sizeof(v1) == sizeof(v2));
    
    CHECK_FIELD(cmd);    
    CHECK_FIELD(cmdsize);
}



// Unit Tests
int main(int /*argc*/, char* /*argv*/[]) {

#if BOOST_OS_WINDOWS
    generic_header_check(::IMAGE_DOS_HEADER(), dd::IMAGE_DOS_HEADER_());
    generic_header_check(::IMAGE_FILE_HEADER(), dd::IMAGE_FILE_HEADER_());
    generic_header_check(::IMAGE_DATA_DIRECTORY(), dd::IMAGE_DATA_DIRECTORY_());
    generic_header_check(::IMAGE_EXPORT_DIRECTORY(), dd::IMAGE_EXPORT_DIRECTORY_());
    generic_header_check(::IMAGE_SECTION_HEADER(), dd::IMAGE_SECTION_HEADER_());
    generic_header_check(::IMAGE_OPTIONAL_HEADER32(), dd::IMAGE_OPTIONAL_HEADER32_());
    generic_header_check(::IMAGE_OPTIONAL_HEADER64(), dd::IMAGE_OPTIONAL_HEADER64_());
    generic_header_check(::IMAGE_NT_HEADERS32(), dd::IMAGE_NT_HEADERS32_());
    generic_header_check(::IMAGE_NT_HEADERS64(), dd::IMAGE_NT_HEADERS64_());
#elif BOOST_OS_MACOS || BOOST_OS_IOS
    generic_header_check(::mach_header(), dd::mach_header_32_());
    generic_header_check(::mach_header_64(), dd::mach_header_64_());

    generic_header_check(::segment_command(), dd::segment_command_32_());
    generic_header_check(::segment_command_64(), dd::segment_command_64_());

    generic_header_check(::section(), dd::section_32_());
    generic_header_check(::section_64(), dd::section_64_());

    generic_header_check(::load_command(), dd::load_command_());
    generic_header_check(::symtab_command(), dd::symtab_command_());

    struct ::nlist nl32_var;
    generic_header_check(nl32_var, dd::nlist_32_());
    struct ::nlist_64 nl64_var;
    generic_header_check(nl64_var, dd::nlist_64_());

#else
    elf_header_checks(::Elf32_Ehdr(), dd::Elf32_Ehdr_());
    elf_header_checks(::Elf64_Ehdr(), dd::Elf64_Ehdr_());

    elf_sheader_checks(::Elf32_Shdr(), dd::Elf32_Shdr_());
    elf_sheader_checks(::Elf64_Shdr(), dd::Elf64_Shdr_());

    elf_sym_header_checks(::Elf32_Sym(), dd::Elf32_Sym_());
    elf_sym_header_checks(::Elf64_Sym(), dd::Elf64_Sym_());
#endif 

    return boost::report_errors();
}

