//
// Copyright 2013 Christian Henning
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
#define BOOST_GIL_IO_ADD_FS_PATH_SUPPORT
#define BOOST_FILESYSTEM_VERSION 3
#include <boost/gil.hpp>
#include <boost/gil/extension/io/bmp.hpp>

#include <boost/gil.hpp>
#include <boost/gil/detail/mp11.hpp>

#include <boost/core/ignore_unused.hpp>
#include <boost/core/lightweight_test.hpp>

#include <fstream>
#include <string>
#include <type_traits>

#include "paths.hpp"

namespace fs = boost::filesystem;
namespace gil = boost::gil;

void test_make_reader_backend()
{
    {
        static_assert(
            std::is_same<gil::detail::is_supported_path_spec<char*>::type, std::true_type>::value,
            "");

        gil::get_reader_backend<const char*, gil::bmp_tag>::type backend_char =
            gil::make_reader_backend(bmp_filename.c_str(), gil::bmp_tag());
        gil::get_reader_backend<std::string, gil::bmp_tag>::type backend_string =
            gil::make_reader_backend(bmp_filename, gil::bmp_tag());

        FILE* file = fopen(bmp_filename.c_str(), "rb");
        gil::get_reader_backend<FILE*, gil::bmp_tag>::type backend_file =
            gil::make_reader_backend(file, gil::bmp_tag());

        std::ifstream in(bmp_filename.c_str(), std::ios::binary);
        gil::get_reader_backend<std::ifstream, gil::bmp_tag>::type backend_ifstream =
            gil::make_reader_backend(in, gil::bmp_tag());

        fs::path my_path(bmp_filename);
        gil::get_reader_backend<std::wstring, gil::bmp_tag>::type backend_wstring =
            gil::make_reader_backend(my_path.wstring(), gil::bmp_tag());
        gil::get_reader_backend<fs::path, gil::bmp_tag>::type backend_path =
            gil::make_reader_backend(my_path, gil::bmp_tag());
    }
    {
        gil::get_reader_backend<const char*, gil::bmp_tag>::type backend_char =
            gil::make_reader_backend(bmp_filename.c_str(), gil::image_read_settings<gil::bmp_tag>());
        gil::get_reader_backend<std::string, gil::bmp_tag>::type backend_string =
            gil::make_reader_backend(bmp_filename, gil::image_read_settings<gil::bmp_tag>());

        FILE* file = fopen(bmp_filename.c_str(), "rb");
        gil::get_reader_backend<FILE*, gil::bmp_tag>::type backend_file =
            gil::make_reader_backend(file, gil::image_read_settings<gil::bmp_tag>());

        std::ifstream in(bmp_filename.c_str(), std::ios::binary);
        gil::get_reader_backend<std::ifstream, gil::bmp_tag>::type backend_ifstream =
            gil::make_reader_backend(in, gil::image_read_settings<gil::bmp_tag>());

        fs::path my_path(bmp_filename);
        gil::get_reader_backend<std::wstring, gil::bmp_tag>::type backend_wstring =
            gil::make_reader_backend(my_path.wstring(), gil::image_read_settings<gil::bmp_tag>());
        gil::get_reader_backend<fs::path, gil::bmp_tag>::type backend_path =
            gil::make_reader_backend(my_path, gil::image_read_settings<gil::bmp_tag>());
    }
}

void test_make_reader()
{
    {
        gil::get_reader_backend<const char*, gil::bmp_tag>::type reader_char = gil::make_reader(
            bmp_filename.c_str(), gil::bmp_tag(), gil::detail::read_and_no_convert());
        gil::get_reader_backend<std::string, gil::bmp_tag>::type reader_string =
            gil::make_reader(bmp_filename, gil::bmp_tag(), gil::detail::read_and_no_convert());

        FILE* file = fopen(bmp_filename.c_str(), "rb");
        gil::get_reader_backend<FILE*, gil::bmp_tag>::type reader_file =
            gil::make_reader(file, gil::bmp_tag(), gil::detail::read_and_no_convert());

        std::ifstream in(bmp_filename.c_str(), std::ios::binary);
        gil::get_reader_backend<std::ifstream, gil::bmp_tag>::type reader_ifstream =
            gil::make_reader(in, gil::bmp_tag(), gil::detail::read_and_no_convert());

        fs::path my_path(bmp_filename);
        gil::get_reader_backend<std::wstring, gil::bmp_tag>::type reader_wstring =
            gil::make_reader(my_path.wstring(), gil::bmp_tag(), gil::detail::read_and_no_convert());
        gil::get_reader_backend<fs::path, gil::bmp_tag>::type reader_path =
            gil::make_reader(my_path, gil::bmp_tag(), gil::detail::read_and_no_convert());
    }
    {
        gil::get_reader_backend<const char*, gil::bmp_tag>::type reader_char = gil::make_reader(
            bmp_filename.c_str(), gil::image_read_settings<gil::bmp_tag>(),
            gil::detail::read_and_no_convert());
        gil::get_reader_backend<std::string, gil::bmp_tag>::type reader_string = gil::make_reader(
            bmp_filename, gil::image_read_settings<gil::bmp_tag>(),
            gil::detail::read_and_no_convert());

        FILE* file = fopen(bmp_filename.c_str(), "rb");
        gil::get_reader_backend<FILE*, gil::bmp_tag>::type reader_file = gil::make_reader(
            file, gil::image_read_settings<gil::bmp_tag>(), gil::detail::read_and_no_convert());

        std::ifstream in(bmp_filename.c_str(), std::ios::binary);
        gil::get_reader_backend<std::ifstream, gil::bmp_tag>::type reader_ifstream =
            gil::make_reader(
                in, gil::image_read_settings<gil::bmp_tag>(), gil::detail::read_and_no_convert());

        fs::path my_path(bmp_filename);
        gil::get_reader_backend<std::wstring, gil::bmp_tag>::type reader_wstring = gil::make_reader(
            my_path.wstring(), gil::image_read_settings<gil::bmp_tag>(),
            gil::detail::read_and_no_convert());
        gil::get_reader_backend<fs::path, gil::bmp_tag>::type reader_path = gil::make_reader(
            my_path, gil::image_read_settings<gil::bmp_tag>(), gil::detail::read_and_no_convert());
    }
}

void test_make_dynamic_image_reader()
{
    {
        gil::get_dynamic_image_reader<const char*, gil::bmp_tag>::type reader_char =
            gil::make_dynamic_image_reader(bmp_filename.c_str(), gil::bmp_tag());
        gil::get_dynamic_image_reader<std::string, gil::bmp_tag>::type reader_string =
            gil::make_dynamic_image_reader(bmp_filename, gil::bmp_tag());

        FILE* file = fopen(bmp_filename.c_str(), "rb");
        gil::get_dynamic_image_reader<FILE*, gil::bmp_tag>::type reader_file =
            gil::make_dynamic_image_reader(file, gil::bmp_tag());

        std::ifstream in(bmp_filename.c_str(), std::ios::binary);
        gil::get_dynamic_image_reader<std::ifstream, gil::bmp_tag>::type reader_ifstream =
            gil::make_dynamic_image_reader(in, gil::bmp_tag());

        fs::path my_path(bmp_filename);
        gil::get_dynamic_image_reader<std::wstring, gil::bmp_tag>::type reader_wstring =
            gil::make_dynamic_image_reader(my_path.wstring(), gil::bmp_tag());
        gil::get_dynamic_image_reader<fs::path, gil::bmp_tag>::type reader_path =
            gil::make_dynamic_image_reader(my_path, gil::bmp_tag());
    }
    {
        gil::get_dynamic_image_reader<const char*, gil::bmp_tag>::type reader_char =
            gil::make_dynamic_image_reader(
                bmp_filename.c_str(), gil::image_read_settings<gil::bmp_tag>());
        gil::get_dynamic_image_reader<std::string, gil::bmp_tag>::type reader_string =
            gil::make_dynamic_image_reader(bmp_filename, gil::image_read_settings<gil::bmp_tag>());

        FILE* file = fopen(bmp_filename.c_str(), "rb");
        gil::get_dynamic_image_reader<FILE*, gil::bmp_tag>::type reader_file =
            gil::make_dynamic_image_reader(file, gil::image_read_settings<gil::bmp_tag>());

        std::ifstream in(bmp_filename.c_str(), std::ios::binary);
        gil::get_dynamic_image_reader<std::ifstream, gil::bmp_tag>::type reader_ifstream =
            gil::make_dynamic_image_reader(in, gil::image_read_settings<gil::bmp_tag>());

        fs::path my_path(bmp_filename);
        gil::get_dynamic_image_reader<std::wstring, gil::bmp_tag>::type reader_wstring =
            gil::make_dynamic_image_reader(
                my_path.wstring(), gil::image_read_settings<gil::bmp_tag>());
        gil::get_dynamic_image_reader<fs::path, gil::bmp_tag>::type reader_path =
            gil::make_dynamic_image_reader(my_path, gil::image_read_settings<gil::bmp_tag>());
    }
}

void test_make_writer()
{
    // Empty files may be created, but noo image data is written.
    {
        using writer_t = gil::get_writer<char const*, gil::bmp_tag>::type;

        static_assert(
            std::is_same<gil::detail::is_writer<writer_t>::type, std::true_type>::value, "");
    }
    {
        gil::get_writer<const char*, gil::bmp_tag>::type writer_char =
            gil::make_writer((bmp_out + "make_test.bmp").c_str(), gil::bmp_tag());
        gil::get_writer<std::string, gil::bmp_tag>::type writer_string =
            gil::make_writer((bmp_out + "make_test.bmp"), gil::bmp_tag());

        FILE* file = fopen((bmp_out + "make_test.bmp").c_str(), "wb");
        gil::get_writer<FILE*, gil::bmp_tag>::type writer_file =
            gil::make_writer(file, gil::bmp_tag());

        std::ofstream out((bmp_out + "make_test.bmp").c_str(), std::ios::binary);
        gil::get_writer<std::ofstream, gil::bmp_tag>::type writer_ofstream =
            gil::make_writer(out, gil::image_write_info<gil::bmp_tag>());
        boost::ignore_unused(writer_ofstream);

        fs::path my_path((bmp_out + "make_test.bmp").c_str());
        gil::get_writer<std::wstring, gil::bmp_tag>::type writer_wstring =
            gil::make_writer(my_path.wstring(), gil::bmp_tag());
        gil::get_writer<fs::path, gil::bmp_tag>::type writer_path =
            gil::make_writer(my_path, gil::bmp_tag());
    }
    {
        gil::get_writer<const char*, gil::bmp_tag>::type writer_char =
            gil::make_writer((bmp_out + "make_test.bmp").c_str(), gil::image_write_info<gil::bmp_tag>());
        gil::get_writer<std::string, gil::bmp_tag>::type writer_string =
            gil::make_writer((bmp_out + "make_test.bmp"), gil::image_write_info<gil::bmp_tag>());

        FILE* file = fopen((bmp_out + std::string("make_test.bmp")).c_str(), "wb");
        gil::get_writer<FILE*, gil::bmp_tag>::type writer_file =
            gil::make_writer(file, gil::image_write_info<gil::bmp_tag>());

        std::ofstream out((bmp_out + "make_test.bmp").c_str(), std::ios::binary);
        gil::get_writer<std::ofstream, gil::bmp_tag>::type writer_ofstream =
            gil::make_writer(out, gil::image_write_info<gil::bmp_tag>());
        boost::ignore_unused(writer_ofstream);

        fs::path my_path(bmp_out + "make_test.bmp");
        gil::get_writer<std::wstring, gil::bmp_tag>::type writer_wstring =
            gil::make_writer(my_path.wstring(), gil::image_write_info<gil::bmp_tag>());
        gil::get_writer<fs::path, gil::bmp_tag>::type writer_path =
            gil::make_writer(my_path, gil::image_write_info<gil::bmp_tag>());
    }
}

void test_make_dynamic_image_writer()
{
    // Empty files may be created, but noo image data is written.
    {
        gil::get_dynamic_image_writer<const char*, gil::bmp_tag>::type writer_char =
            gil::make_dynamic_image_writer(
                (bmp_out + std::string("make_test.bmp")).c_str(), gil::bmp_tag());
        gil::get_dynamic_image_writer<std::string, gil::bmp_tag>::type writer_string =
            gil::make_dynamic_image_writer(bmp_out + "make_test.bmp", gil::bmp_tag());

        FILE* file = fopen((bmp_out + std::string("make_test.bmp")).c_str(), "wb");
        gil::get_dynamic_image_writer<FILE*, gil::bmp_tag>::type writer_file =
            gil::make_dynamic_image_writer(file, gil::bmp_tag());

        std::ofstream out((bmp_out + "make_test.bmp").c_str(), std::ios::binary);
        gil::get_dynamic_image_writer<std::ofstream, gil::bmp_tag>::type writer_ofstream =
            gil::make_dynamic_image_writer(out, gil::bmp_tag());
        boost::ignore_unused(writer_ofstream);

        fs::path my_path(bmp_out + "make_test.bmp");
        gil::get_dynamic_image_writer<std::wstring, gil::bmp_tag>::type writer_wstring =
            gil::make_dynamic_image_writer(my_path.wstring(), gil::bmp_tag());
        gil::get_dynamic_image_writer<fs::path, gil::bmp_tag>::type writer_path =
            gil::make_dynamic_image_writer(my_path, gil::bmp_tag());
    }
    {
        gil::get_dynamic_image_writer<const char*, gil::bmp_tag>::type writer_char =
            gil::make_dynamic_image_writer(
                (bmp_out + std::string("make_test.bmp")).c_str(), gil::image_write_info<gil::bmp_tag>());

        gil::get_dynamic_image_writer<std::string, gil::bmp_tag>::type writer_string =
            gil::make_dynamic_image_writer(
                bmp_out + "make_test.bmp", gil::image_write_info<gil::bmp_tag>());

        FILE* file = fopen((bmp_out + std::string("make_test.bmp")).c_str(), "wb");
        gil::get_dynamic_image_writer<FILE*, gil::bmp_tag>::type writer_file =
            gil::make_dynamic_image_writer(file, gil::image_write_info<gil::bmp_tag>());

        std::ofstream out((bmp_out + "make_test.bmp").c_str(), std::ios::binary);
        gil::get_dynamic_image_writer<std::ofstream, gil::bmp_tag>::type writer_ofstream =
            gil::make_dynamic_image_writer(out, gil::image_write_info<gil::bmp_tag>());
        boost::ignore_unused(writer_ofstream);

        fs::path my_path(bmp_out + "make_test.bmp");
        gil::get_dynamic_image_writer<std::wstring, gil::bmp_tag>::type writer_wstring =
            gil::make_dynamic_image_writer(my_path.wstring(), gil::image_write_info<gil::bmp_tag>());
        gil::get_dynamic_image_writer<fs::path, gil::bmp_tag>::type writer_path =
            gil::make_dynamic_image_writer(my_path, gil::image_write_info<gil::bmp_tag>());
    }
}

int main(int argc, char *argv[])
{
    try
    {
        test_make_reader_backend();
        test_make_reader();
        test_make_dynamic_image_reader();
        test_make_writer();
        test_make_dynamic_image_writer();
    }
    catch (std::exception const& e)
    {
        BOOST_ERROR(e.what());
    }
    return boost::report_errors();
}
