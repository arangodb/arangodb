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
#include "error.hpp"

NS_ROOT

// ----------------------------------------------------------------------------
//                                                                   error_base
// ----------------------------------------------------------------------------

ErrorCode error_base::code() const NOEXCEPT {
  return ErrorCode::undefined_error;
}

const char* error_base::what() const NOEXCEPT {
  return "An unspecified error has occured.";
}

// ----------------------------------------------------------------------------
//                                                                not_supported
// ----------------------------------------------------------------------------

ErrorCode not_supported::code() const NOEXCEPT {
  return CODE;
}

const char* not_supported::what() const NOEXCEPT { 
  return "Operation not supported."; 
}

// ----------------------------------------------------------------------------
//                                                                     io_error
// ----------------------------------------------------------------------------

ErrorCode io_error::code() const NOEXCEPT {
  return CODE;
}

const char* io_error::what() const NOEXCEPT { 
  return "An unspecified I/O error has occured."; 
}

// ----------------------------------------------------------------------------
//                                                                    eof_error
// ----------------------------------------------------------------------------

ErrorCode eof_error::code() const NOEXCEPT {
  return CODE;
}

const char* eof_error::what() const NOEXCEPT { 
  return "Read past EOF."; 
}

// ----------------------------------------------------------------------------
//                                                            detailed_io_error
// ----------------------------------------------------------------------------

detailed_io_error::detailed_io_error(
  const irs::string_ref& error /*= irs::string_ref::nil*/
) {
  if (!error.empty()) {
    error_.append(error.c_str(), error.size());
  }
}

detailed_io_error::detailed_io_error(std::string&& error)
  : error_(std::move(error)) {
}

detailed_io_error::detailed_io_error(const char* error)
  : detailed_io_error(irs::string_ref(error)) {
}

detailed_io_error& detailed_io_error::operator<<(const irs::string_ref& error) {
  if (!error.empty()) {
    error_.append(error.c_str(), error.size());
  }

  return *this;
}

detailed_io_error& detailed_io_error::operator<<(std::string&& error) {
  error.append(std::move(error));

  return *this;
}

detailed_io_error& detailed_io_error::operator<<(const char* error) {
  return (*this) << irs::string_ref(error);
}

ErrorCode detailed_io_error::code() const NOEXCEPT {
  return CODE;
}

const char* detailed_io_error::what() const NOEXCEPT {
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                           lock_obtain_failed
// ----------------------------------------------------------------------------

lock_obtain_failed::lock_obtain_failed(
    const irs::string_ref& filename /*= irs::string_ref::nil*/
) : error_("Lock obtain timed out") {
  if (filename.null()) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_ + filename.c_str();
  }
}

ErrorCode lock_obtain_failed::code() const NOEXCEPT {
  return CODE;
}

const char* lock_obtain_failed::what() const NOEXCEPT {
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                               file_not_found
// ----------------------------------------------------------------------------

file_not_found::file_not_found(
    const irs::string_ref& filename /*= irs::string_ref::nil*/
): error_("File not found") {
  if (filename.null()) {
    error_ += ".";
  } else {
    error_ += ": ";
    error_ + filename.c_str();
  }
}

ErrorCode file_not_found::code() const NOEXCEPT{
  return CODE;
}

const char* file_not_found::what() const NOEXCEPT { 
  return error_.c_str();
}

// ----------------------------------------------------------------------------
//                                                              index_not_found
// ----------------------------------------------------------------------------

ErrorCode index_not_found::code() const NOEXCEPT{
  return CODE;
}

const char* index_not_found::what() const NOEXCEPT {
  return "No segments* file found.";
}

// ----------------------------------------------------------------------------
//                                                                  index_error
// ----------------------------------------------------------------------------

ErrorCode index_error::code() const NOEXCEPT{
  return CODE;
}

const char* index_error::what() const NOEXCEPT { 
  return "Index error."; 
}

// ----------------------------------------------------------------------------
//                                                               not_impl_error
// ----------------------------------------------------------------------------

 ErrorCode not_impl_error::code() const NOEXCEPT{
  return CODE;
}

const char* not_impl_error::what() const NOEXCEPT { 
  return "Not implemented."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

ErrorCode illegal_argument::code() const NOEXCEPT{
  return CODE;
}

const char* illegal_argument::what() const NOEXCEPT{ 
  return "Invalid argument."; 
}

// ----------------------------------------------------------------------------
//                                                             illegal_argument
// ----------------------------------------------------------------------------

ErrorCode illegal_state::code() const NOEXCEPT{
  return CODE;
}

const char* illegal_state::what() const NOEXCEPT{
  return "Illegal state."; 
}

NS_END // NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------