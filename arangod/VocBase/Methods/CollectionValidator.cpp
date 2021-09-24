#include "CollectionValidator.h"
#include <Basics/StaticStrings.h>
#include <Basics/VelocyPackHelper.h>
#include <Utilities/NameValidator.h>
#include <Utils/Events.h>
#include <VocBase/vocbase.h>

using namespace arangodb;

Result CollectionValidator::verifyCollectionName(const arangodb::velocypack::Slice parameters, arangodb::CreateDatabaseInfo const& info) {
  std::string pars = arangodb::basics::VelocyPackHelper::getStringValue(parameters,
                                                                                    StaticStrings::DataSourceName, "");
  if (!CollectionNameValidator::isAllowedName(false, false, pars)) { // todo check the second false
    std::string name;
    std::string const& dbName = info.getName();
    if (parameters.isObject()) {
      name = arangodb::basics::VelocyPackHelper::getStringValue(parameters,
                                              StaticStrings::DataSourceName, "");
    }

    // print error before returning it
    events::CreateCollection(dbName, name, TRI_ERROR_ARANGO_ILLEGAL_NAME);
    return Result(TRI_ERROR_ARANGO_ILLEGAL_NAME);

  }
  return Result(TRI_ERROR_NO_ERROR);
}