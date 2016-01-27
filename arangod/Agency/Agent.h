#ifndef __ARANGODB_CONSENSUS_AGENT__
#define __ARANGODB_CONSENSUS_AGENT__

#include "Constituent.h"
#include "Log.h"

namespace arangodb {
namespace consensus {
    
  class Agent {
    
  public:
    /**
     * @brief Host descriptor
     */
    
    template<class T> struct host_t {
      std::string host;
      std::string port;
      T vote;
    };
    
    /**
     * @brief Agent configuration
     */
    template<class T> struct AgentConfig {
      T min_ping;
      T max_ping;
      std::vector<host_t<T> > start_hosts;
    }; 
    
    /**
     * @brief Default ctor
     */
    Agent ();
    
    /**
     * @brief Construct with program options \n
     *        One process starts with options, the \n remaining start with list of peers and gossip.
     */
    Agent (AgentConfig<double> const&);
    
    /**
     * @brief Copy ctor
     */
    Agent (Agent const&);
    
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
    Slice const& log (Slice const&);
    Slice const& redirect (Slice const&);
      
  private:
      Constituent _constituent; /**< @brief Leader election delegate */
    Log         _log;         /**< @brief Log replica              */
    
  };
  
}}

#endif
