#include "trashHelper.h"

Result verifyCollectionName(arangodb::velocypack::Slice const parameters) {
  if (!IsAllowedName(parameters)) {
    std::string name;
    std::string const& dbName = _info.getName();
    if (parameters.isObject()) {
      name = VelocyPackHelper::getStringValue(parameters,
                                              StaticStrings::DataSourceName, "");
    }

    events::CreateCollection(dbName, name, TRI_ERROR_ARANGO_ILLEGAL_NAME);
    return Result(TRI_ERROR_ARANGO_ILLEGAL_NAME);

  }
  return Result(TRI_ERROR_NO_ERROR);
}