#include <cstdint>

//using namespace arangodb::velocypack;

class Slice {};

namespace arangodb {
namespace consensus {

/**
 * @brief Log repilca
 */
class Log  {
  
public:
  typedef uint64_t index_t;

  /**
   * @brief Default constructor
   */
  Log ();

  /**
   * @brief Default Destructor
   */
  virtual ~Log();

  /**
   * @brief Log
   */
  
  Slice const& log (Slice const&);
  
private:
  
  index_t _commit_id;      /**< @brief: index of highest log entry known
                              to be committed (initialized to 0, increases monotonically) */
  index_t _last_applied;   /**< @brief: index of highest log entry applied to state machine  */
};
  
}}
