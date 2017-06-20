//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "shared.hpp"
#include "format_utils.hpp"

#include "index/index_meta.hpp"

#include "formats/formats.hpp"

NS_ROOT

void validate_footer(iresearch::index_input& in) {
  const int64_t remain = in.length() - in.file_pointer();
  if (remain != format_utils::FOOTER_LEN) {
    // invalid position
    throw iresearch::index_error();
  }

  const int32_t magic = in.read_int();
  if (magic != format_utils::FOOTER_MAGIC) {
    // invalid magic number 
    throw iresearch::index_error();
  }

  const int32_t alg_id = in.read_int();
  if (alg_id != 0) {
    // invalid algorithm
    throw iresearch::index_error();
  }
}

NS_BEGIN(format_utils)

void write_header(index_output& out, const string_ref& format, int32_t ver) {
  out.write_int(FORMAT_MAGIC);
  write_string(out, format);
  out.write_int(ver);
}

void write_footer(index_output& out) {
  out.write_int(FOOTER_MAGIC);
  out.write_int(0);
  out.write_long(out.checksum());
}

int32_t check_header(
    index_input& in, 
    const string_ref& req_format,
    int32_t min_ver, int32_t max_ver) {
  const int32_t magic = in.read_int();
  if (FORMAT_MAGIC != magic) {
    // index format
    throw index_error();
  }

  const auto format = read_string<std::string>(in);
  if (compare(req_format, format) != 0) {
    // invalid format
    throw index_error();
  }

  const int32_t ver = in.read_int();
  if (ver < min_ver || ver > max_ver) {
    // invalid version
    throw index_error();
  }

  return ver;
}

int64_t read_checksum( index_input& in ) {
  in.seek( in.length() - FOOTER_LEN );
  validate_footer( in );
  return in.read_long();
}

NS_END
NS_END
