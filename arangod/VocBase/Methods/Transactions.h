#ifndef ARANGOD_VOCBASE_METHODS_TRANSACTIONS_HANDLER_H
#define ARANGOD_VOCBASE_METHODS_TRANSACTIONS_HANDLER_H 1

#include <v8.h>
#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "VocBase/vocbase.h"

namespace arangodb {

Result executeTransaction(v8::Isolate*, basics::ReadWriteLock& cancelLock,
                          std::atomic<bool>& canceled, VPackSlice transaction,
                          std::string requestPortType, VPackBuilder& result);

Result executeTransactionJS(v8::Isolate*, v8::Handle<v8::Value> const& arg,
                            v8::Handle<v8::Value>& result, v8::TryCatch&);

}  // namespace arangodb
#endif
