//////////////////////////////////////////////////////////////////////////////
//
// (C) Copyright Ion Gaztanaga 2004-2012. Distributed under the Boost
// Software License, Version 1.0. (See accompanying file
// LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/libs/interprocess for documentation.
//
//////////////////////////////////////////////////////////////////////////////

#include <boost/interprocess/detail/workaround.hpp>

#if defined(BOOST_INTERPROCESS_MAPPED_FILES)

#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <cstdio>
#include <string>
#include "get_process_id_name.hpp"

using namespace boost::interprocess;

template <class CharT>
struct filename_traits;

template <>
struct filename_traits<char>
{

   static const char* get()
   {  return filename.c_str();  }

   static std::string filename;
};

std::string filename_traits<char>::filename = get_filename();


#ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES

template <>
struct filename_traits<wchar_t>
{

   static const wchar_t* get()
   {  return filename.c_str();  }

   static std::wstring filename;
};

std::wstring filename_traits<wchar_t>::filename = get_wfilename();

#endif   //#ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES

template<class CharT>
int test_managed_mapped_file()
{
   const int FileSize          = 65536*10;
   const CharT *FileName = filename_traits<CharT>::get();

   //STL compatible allocator object for memory-mapped file
   typedef allocator<int, managed_mapped_file::segment_manager>
      allocator_int_t;
   //A vector that uses that allocator
   typedef boost::interprocess::vector<int, allocator_int_t> MyVect;

   {
      //Remove the file it is already created
      file_mapping::remove(FileName);

      const int max              = 100;
      void *array[std::size_t(max)];
      //Named allocate capable shared memory allocator
      managed_mapped_file mfile(create_only, FileName, FileSize);

      std::size_t i;
      //Let's allocate some memory
      for(i = 0; i < max; ++i){
         array[std::ptrdiff_t(i)] = mfile.allocate(i+1u);
      }

      //Deallocate allocated memory
      for(i = 0; i < max; ++i){
         mfile.deallocate(array[std::ptrdiff_t(i)]);
      }
   }

   {
      //Remove the file it is already created
      file_mapping::remove(FileName);
      //Named allocate capable memory mapped file managed memory class
      managed_mapped_file tmp(create_only, FileName, FileSize);
   }
   {
      //Remove the file it is already created
      file_mapping::remove(FileName);

      //Now re-create it with create or open
      managed_mapped_file mfile(open_or_create, FileName, FileSize);

      //Construct the STL-like allocator with the segment manager
      const allocator_int_t myallocator (mfile.get_segment_manager());

      //Construct vector
      MyVect *mfile_vect = mfile.construct<MyVect> ("MyVector") (myallocator);

      //Test that vector can be found via name
      if(mfile_vect != mfile.find<MyVect>("MyVector").first)
         return -1;

      //Destroy and check it is not present
      mfile.destroy<MyVect> ("MyVector");
      if(0 != mfile.find<MyVect>("MyVector").first)
         return -1;

      //Construct a vector in the memory-mapped file
      mfile_vect = mfile.construct<MyVect> ("MyVector") (myallocator);

      //Flush cached data from memory-mapped file to disk
      mfile.flush();
   }
   {
      //Map preexisting file again in memory
      managed_mapped_file mfile(open_only, FileName);

      //Check vector is still there
      MyVect *mfile_vect = mfile.find<MyVect>("MyVector").first;
      if(!mfile_vect)
         return -1;
   }
   {
      //Map preexisting file again in memory
      managed_mapped_file mfile(open_or_create, FileName, FileSize);

      //Check vector is still there
      MyVect *mfile_vect = mfile.find<MyVect>("MyVector").first;
      if(!mfile_vect)
         return -1;
   }
   {
      {
         //Map preexisting file again in copy-on-write
         managed_mapped_file mfile(open_copy_on_write, FileName);

         //Check vector is still there
         MyVect *mfile_vect = mfile.find<MyVect>("MyVector").first;
         if(!mfile_vect)
            return -1;

         //Erase vector
         mfile.destroy_ptr(mfile_vect);

         //Make sure vector is erased
         mfile_vect = mfile.find<MyVect>("MyVector").first;
         if(mfile_vect)
            return -1;
      }
      //Now check vector is still in the file
      {
         //Map preexisting file again in copy-on-write
         managed_mapped_file mfile(open_copy_on_write, FileName);

         //Check vector is still there
         MyVect *mfile_vect = mfile.find<MyVect>("MyVector").first;
         if(!mfile_vect)
            return -1;
      }
   }
   {
      //Map preexisting file again in read-only
      managed_mapped_file mfile(open_read_only, FileName);

      //Check vector is still there
      MyVect *mfile_vect = mfile.find<MyVect>("MyVector").first;
      if(!mfile_vect)
         return -1;
   }
   {
      managed_mapped_file::size_type old_free_memory;
      {
         //Map preexisting file again in memory
         managed_mapped_file mfile(open_only, FileName);
         old_free_memory = mfile.get_free_memory();
      }

      //Now grow the file
      managed_mapped_file::grow(FileName, FileSize);

      //Map preexisting file again in memory
      managed_mapped_file mfile(open_only, FileName);

      //Check vector is still there
      MyVect *mfile_vect = mfile.find<MyVect>("MyVector").first;
      if(!mfile_vect)
         return -1;

      if(mfile.get_size() != (FileSize*2))
         return -1;
      if(mfile.get_free_memory() <= old_free_memory)
         return -1;
   }
   {
      managed_mapped_file::size_type old_free_memory, next_free_memory,
                  old_file_size, next_file_size, final_file_size;
      {
         //Map preexisting file again in memory
         managed_mapped_file mfile(open_only, FileName);
         old_free_memory = mfile.get_free_memory();
         old_file_size   = mfile.get_size();
      }

      //Now shrink the file
      managed_mapped_file::shrink_to_fit(FileName);

      {
         //Map preexisting file again in memory
         managed_mapped_file mfile(open_only, FileName);
         next_file_size = mfile.get_size();

         //Check vector is still there
         MyVect *mfile_vect = mfile.find<MyVect>("MyVector").first;
         if(!mfile_vect)
            return -1;

         next_free_memory = mfile.get_free_memory();
         if(next_free_memory >= old_free_memory)
            return -1;
         if(old_file_size <= next_file_size)
            return -1;
      }

      //Now destroy the vector
      {
         //Map preexisting file again in memory
         managed_mapped_file mfile(open_only, FileName);

         //Destroy and check it is not present
         mfile.destroy<MyVect>("MyVector");
         if(0 != mfile.find<MyVect>("MyVector").first)
            return -1;
      }

      //Now shrink the file
      managed_mapped_file::shrink_to_fit(FileName);
      {
         //Map preexisting file again in memory
         managed_mapped_file mfile(open_only, FileName);
         final_file_size = mfile.get_size();
         if(next_file_size <= final_file_size)
            return -1;
      }
      {
         //Now test move semantics
         managed_mapped_file original(open_only, FileName);
         managed_mapped_file move_ctor(boost::move(original));
         managed_mapped_file move_assign;
         move_assign = boost::move(move_ctor);
         move_assign.swap(original);
      }
   }

   file_mapping::remove(FileName);
   return 0;
}

int main ()
{
   int r;
   r = test_managed_mapped_file<char>();
   if(r) return r;
   #ifdef BOOST_INTERPROCESS_WCHAR_NAMED_RESOURCES
   r = test_managed_mapped_file<wchar_t>();
   if(r) return r;
   #endif
   return 0;
}


#else //#if defined(BOOST_INTERPROCESS_MAPPED_FILES)

int main()
{
   return 0;
}

#endif//#if defined(BOOST_INTERPROCESS_MAPPED_FILES)
