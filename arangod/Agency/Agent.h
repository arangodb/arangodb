////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#ifndef __ARANGODB_CONSENSUS_AGENT__
#define __ARANGODB_CONSENSUS_AGENT__

#include "AgencyCommon.h"
#include "Constituent.h"
#include "Log.h"

namespace arangodb {
namespace consensus {
    
  class Agent {
    
  public:

    /**
     * @brief Default ctor
     */
    Agent ();
    
    /**
     * @brief Construct with program options \n
     *        One process starts with options, the \n remaining start with list of peers and gossip.
     */
    Agent (Config<double> const&);

    /**
     * @brief Clean up
     */
    virtual ~Agent();
    
    /**
     * @brief Get current term
     */
    Constituent::term_t term() const;
    
    /**
     * @brief 
     */
    Log::ret_t
      log (std::shared_ptr<arangodb::velocypack::Builder> const);

    Slice const& redirect (Slice const&);

    bool vote(Constituent::id_t, Constituent::term_t);

    Config<double> const& config () const;

    void start ();
      
  private:
    Constituent _constituent; /**< @brief Leader election delegate */
    Log         _log;         /**< @brief Log replica              */
    Config<double> _config;
    
  };
  
}}

#endif
