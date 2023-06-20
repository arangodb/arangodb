#pragma once

#include "IResearch/IResearchDataStore.h"
#include "IResearchDataStore.h"

namespace arangodb::iresearch {

struct IResearchDataStoreHotbackupHelper : public IResearchDataStore {
  IResearchDataStoreHotbackupHelper(std::string path)
      : IResearchDataStore(IResearchDataStore::DefaultConstructKey{}) {
    _dataStore._path = path;
  }

  [[nodiscard]] Index& index() noexcept final { TRI_ASSERT(false); }
  [[nodiscard]] Index const& index() const noexcept final { TRI_ASSERT(false); }
  [[nodiscard]] AnalyzerPool::ptr findAnalyzer(
      AnalyzerPool const& analyzer) const final {
    TRI_ASSERT(false);
    return {};
  }
  void invalidateQueryCache(TRI_vocbase_t*) final { TRI_ASSERT(false); }
  irs::Comparer const* getComparator() const noexcept final {
    TRI_ASSERT(false);
    return {};
  }
};

}  // namespace arangodb::iresearch
