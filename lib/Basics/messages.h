////////////////////////////////////////////////////////////////////////////////
/// @brief common messages shared by all binaries
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_BASICS_C_MESSAGES_H
#define ARANGODB_BASICS_C_MESSAGES_H 1

// -----------------------------------------------------------------------------
// --SECTION--                                                    public defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief bye bye message
////////////////////////////////////////////////////////////////////////////////

#define TRI_BYE_MESSAGE_CH "Uf wiederluege!"
#define TRI_BYE_MESSAGE_CZ "Na shledanou!"
#define TRI_BYE_MESSAGE_DE "Auf Wiedersehen!"
#define TRI_BYE_MESSAGE_EN "Bye Bye!"
#define TRI_BYE_MESSAGE_EO "Adiau!"
#define TRI_BYE_MESSAGE_ES "¡Hasta luego!"
#define TRI_BYE_MESSAGE_FR "Au revoir!"
#define TRI_BYE_MESSAGE_GR "Εις το επανιδείν!"
#define TRI_BYE_MESSAGE_IT "Arrivederci!"
#define TRI_BYE_MESSAGE_IL "להתראות"
#define TRI_BYE_MESSAGE_JP "さようなら"
#define TRI_BYE_MESSAGE_NL "Tot ziens!"
#define TRI_BYE_MESSAGE_RU "До свидания!"
#define TRI_BYE_MESSAGE_SV "Adjö!"

#define TRI_BYE_MESSAGE \
  TRI_BYE_MESSAGE_CH " " \
  TRI_BYE_MESSAGE_CZ " " \
  TRI_BYE_MESSAGE_DE " " \
  TRI_BYE_MESSAGE_EN " " \
  TRI_BYE_MESSAGE_EO " " \
  TRI_BYE_MESSAGE_ES " " \
  TRI_BYE_MESSAGE_FR "\n" \
  TRI_BYE_MESSAGE_IT " " \
  TRI_BYE_MESSAGE_IL " " \
  TRI_BYE_MESSAGE_NL " " \
  TRI_BYE_MESSAGE_SV " " \
  TRI_BYE_MESSAGE_GR " " \
  TRI_BYE_MESSAGE_JP " " \
  TRI_BYE_MESSAGE_RU

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
