////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_MESSAGES_H
#define ARANGODB_BASICS_MESSAGES_H 1

////////////////////////////////////////////////////////////////////////////////
/// @brief bye bye message
///
/// #include <iostream>
/// #include <iomanip>
/// int main(int argc, char* argv[]) {
///   std::string line;
///   std::getline(std::cin, line);
///   std::cout << std::hex << "\"";
///   for (auto c : line) {
///     std::cout << "\\x" << (unsigned int)((unsigned char)c);
///   }
///   std::cout << "\"\n" << std::endl;
/// }
////////////////////////////////////////////////////////////////////////////////

#define TRI_BYE_MESSAGE_CH "Uf wiederluege!"
#define TRI_BYE_MESSAGE_CN "\xE5\x86\x8D\xE8\xA7\x81\xEF\xBC\x81"
#define TRI_BYE_MESSAGE_CZ "Na shledanou!"
#define TRI_BYE_MESSAGE_DE "Auf Wiedersehen!"
#define TRI_BYE_MESSAGE_EN "Bye Bye!"
#define TRI_BYE_MESSAGE_EO "Adiau!"
#define TRI_BYE_MESSAGE_ES \
  "\xc2\xa1\x48\x61\x73\x74\x61\x20\x6c\x75\x65\x67\x6f\x21"
#define TRI_BYE_MESSAGE_FR "Au revoir!"
#define TRI_BYE_MESSAGE_GR                                                   \
  "\xce\x95\xce\xb9\xcf\x82\x20\xcf\x84\xce\xbf\x20\xce\xb5\xcf\x80\xce\xb1" \
  "\xce\xbd\xce\xb9\xce\xb4\xce\xb5\xce\xaf\xce\xbd\x21"
#define TRI_BYE_MESSAGE_ID "Sampai jumpa!"
#define TRI_BYE_MESSAGE_IL \
  "\xd7\x9c\xd7\x94\xd7\xaa\xd7\xa8\xd7\x90\xd7\x95\xd7\xaa\x21"
// Should really be the following, but most terminals do not write right
// to left, so we put it here backwards.
//#define TRI_BYE_MESSAGE_IL
//"\xd7\xaa\xd7\x95\xd7\x90\xd7\xa8\xd7\xaa\xd7\x94\xd7\x9c\x21"
#define TRI_BYE_MESSAGE_IT "Arrivederci!"
#define TRI_BYE_MESSAGE_JP \
  "\xe3\x81\x95\xe3\x82\x88\xe3\x81\x86\xe3\x81\xaa\xe3\x82\x89"
#define TRI_BYE_MESSAGE_KR                                                   \
  "\xec\x95\x88\xeb\x85\x95\xed\x9e\x88\x20\xea\xb0\x80\xec\x84\xb8\xec\x9a" \
  "\x94\x21"
#define TRI_BYE_MESSAGE_LV \
  "\x55\x7a\x20\x72\x65\x64\x7a\xC4\x93\xC5\xA1\x61\x6e\x6f\x73\x21"
#define TRI_BYE_MESSAGE_NL "Tot ziens!"
#define TRI_BYE_MESSAGE_RU                                                   \
  "\xd0\x94\xd0\xbe\x20\xd1\x81\xd0\xb2\xd0\xb8\xd0\xb4\xd0\xb0\xd0\xbd\xd0" \
  "\xb8\xd1\x8f\x21"
#define TRI_BYE_MESSAGE_SV "\x41\x64\x6a\xc3\xb6\x21"
#define TRI_BYE_MESSAGE_SY \
  "\xd8\xa1\xd8\xa7\xd9\x82\xd9\x84\xd9\x84\xd8\xa7\x20\xd9\x89\xd9\x84\xd8\xa5"
#define TRI_BYE_MESSAGE_PT "\x41\x74\xc3\xa9\x20\x42\x72\x65\x76\x65\x21"
#define TRI_BYE_MESSAGE_FA \
  "\xd8\xae\xd8\xaf\xd8\xa7\xd8\xad\xd8\xa7\xd9\x81\xd8\xb8\x21"
#define TRI_BYE_MESSAGE_GE                                                   \
  "\xe1\x83\xa8\xe1\x83\x94\xe1\x83\xae\xe1\x83\x95\xe1\x83\x94\xe1\x83\x93" \
  "\xe1\x83\xa0\xe1\x83\x90\xe1\x83\x9b\xe1\x83\x93\xe1\x83\x94"

#define TRI_BYE_MESSAGE                                                 \
  TRI_BYE_MESSAGE_CH                                                    \
  " " TRI_BYE_MESSAGE_CN " " TRI_BYE_MESSAGE_CZ " " TRI_BYE_MESSAGE_DE  \
  " " TRI_BYE_MESSAGE_EN " " TRI_BYE_MESSAGE_EO " " TRI_BYE_MESSAGE_ES  \
  " " TRI_BYE_MESSAGE_GR " " TRI_BYE_MESSAGE_SY "\n" TRI_BYE_MESSAGE_IL \
  " " TRI_BYE_MESSAGE_IT " " TRI_BYE_MESSAGE_NL " " TRI_BYE_MESSAGE_SV  \
  " " TRI_BYE_MESSAGE_FR " " TRI_BYE_MESSAGE_JP " " TRI_BYE_MESSAGE_RU  \
  " " TRI_BYE_MESSAGE_PT " " TRI_BYE_MESSAGE_FA " " TRI_BYE_MESSAGE_LV  \
  " " TRI_BYE_MESSAGE_GE " " TRI_BYE_MESSAGE_KR " " TRI_BYE_MESSAGE_ID
#endif
