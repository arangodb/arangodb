////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Wilfried Goesgens
////////////////////////////////////////////////////////////////////////////////

#ifndef _WIN32
#include <gssapi/gssapi.h>
#include <gssapi/gssapi_krb5.h>
#include <gssapi/gssapi_spnego.h>
#include <string>

#include "Utilities/GssApiClient.h"

#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StringUtils.h"
#include "Basics/system-functions.h"
#include "Basics/StringUtils.h"

using namespace arangodb;
using namespace arangodb::basics;

std::string arangodb::gssApiError(uint32_t maj, uint32_t min) {
  std::string errorString;
  auto getErrorString = [&errorString](int type, uint32_t err) {
                          uint32_t maj_ret, min_ret;
                          gss_buffer_desc text;
                          uint32_t msg_ctx;

                          msg_ctx = 0;
                          do {
                            auto scguard = scopeGuard([&text, &min_ret] {
                                                        gss_release_buffer(&min_ret, &text);
                                                      });

                            maj_ret = gss_display_status(&min_ret, err, type,
                                                         GSS_C_NO_OID, &msg_ctx, &text);
                            if (maj_ret != GSS_S_COMPLETE) {
                              return;
                            }
                            errorString += std::string(static_cast<char*>(text.value), text.length);
                          } while (msg_ctx != 0);
                        };
  getErrorString(GSS_C_GSS_CODE, maj);
  errorString += " (";
  getErrorString(GSS_C_MECH_CODE, min);
  errorString += ")";
  return errorString;
}

std::string arangodb::getKerberosBase64Token(std::string const& service, std::string& error, sockaddr_in* source, sockaddr_in* dest) {
  
  uint32_t maj, min;
  OM_uint32 req_flags = GSS_C_MUTUAL_FLAG | GSS_C_SEQUENCE_FLAG; // GSS_C_REPLAY_FLAG;
  gss_OID mech_type = GSS_C_NO_OID;
  
  // OM_uint32 ret_flags;
  gss_buffer_desc input_token = GSS_C_EMPTY_BUFFER;
  gss_buffer_desc output_token = GSS_C_EMPTY_BUFFER;
  gss_buffer_desc service_buffer = TO_GSS_BUFFER(service);
  gss_name_t       server_name = GSS_C_NO_NAME;
  auto scguard = scopeGuard([&] {
                              gss_release_name(&min, &server_name);
                              gss_release_buffer(&min, &output_token);
                            });
  maj = gss_import_name(&min,
                        &service_buffer,
                        GSS_C_NT_HOSTBASED_SERVICE,// */ gss_krb5_nt_service_name,
                        &server_name);
  if(GSS_ERROR(maj)) {
    error = gssApiError(maj, min);
    return std::string();
  }

  gss_ctx_id_t init_ctx = GSS_C_NO_CONTEXT;
  /* TODO: do we need this?
  struct gss_channel_bindings_struct input_chan_bindings;
  input_chan_bindings.initiator_addrtype = GSS_C_AF_INET;
  input_chan_bindings.initiator_address.length = 4; // ipv6?
  input_chan_bindings.initiator_address.value = (void*)&source->sin_addr.s_addr;
  input_chan_bindings.acceptor_addrtype = GSS_C_AF_INET;
  input_chan_bindings.acceptor_address.length = 4;
  input_chan_bindings.acceptor_address.value = (void*)&dest->sin_addr.s_addr;
  input_chan_bindings.application_data.length = 0;
  input_chan_bindings.application_data.value = NULL;
  */
  maj = gss_init_sec_context(&min,
                             GSS_C_NO_CREDENTIAL, /* cred_handle */
                             &init_ctx,
                             server_name,
                             mech_type,
                             req_flags,
                             0, /* time_req */
                             GSS_C_NO_CHANNEL_BINDINGS, //*/ &input_chan_bindings,
                             &input_token,
                             NULL, /* actual_mech_type */
                             &output_token,
                             NULL, //&ret_flags,
                             NULL /* time_rec */
                             );



  if (GSS_ERROR(maj)) {
    error = gssApiError(maj, min);
    return std::string();
  }

  std::string outputBinary((const char *)output_token.value, output_token.length);
  return StringUtils::encodeBase64(outputBinary);
}
#endif
