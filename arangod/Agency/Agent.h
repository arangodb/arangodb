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
     * @brief Construct with program options
     */
    Agent (config_t const&);

    /**
     * @brief Clean up
     */
    virtual ~Agent();
    
    /**
     * @brief Get current term
     */
    term_t term() const;
    
    /**
     * @brief 
     */
    template<typename T> Log::ret_t log (T const&);

    /**
     * @brief Vote request
     */
    bool vote(id_t, term_t);

    /**
     * @brief Provide configuration
     */
    Config<double> const& config () const;
    
    /**
     * @brief Start thread
     */
    void start ();

    /**
     * @brief Verbose print of myself
     */ 
    void print (arangodb::LoggerStream&) const;

    /**
     * @brief Are we fit to run?
     */
    bool fitness () const;
    
    /**
     * @brief 
     */
    void report (status_t);

    /**
     * @brief Leader ID
     */
    id_t leaderID () const;

    /**
     * @brief Attempt write
     */
    query_ret_t write (agency_io_t) const;

    /**
     * @brief Read from agency
     */
    query_ret_t read (std::shared_ptr<agency_io_t>) const;

  private:
    Constituent _constituent; /**< @brief Leader election delegate */
    Log         _log;         /**< @brief Log replica              */
    config_t    _config;
    status_t    _status;
    
  };
  
}

  LoggerStream& operator<< (LoggerStream&, arangodb::consensus::Agent const&);
  
}



#endif
