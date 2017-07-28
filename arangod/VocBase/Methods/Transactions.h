#ifndef ARANGOD_VOCBASE_METHODS_TRANSACTIONS_HANDLER_H
#define ARANGOD_VOCBASW_METHODS_TRANSACTIONS_HANDLER_H 1

#include "Basics/Result.h"

#include <v8.h>
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

Result executeTransaction(TRI_vocbase_t*, VPackSlice, VPackBuilder&);

Result executeTransactionJS(v8::Isolate*, v8::Handle<v8::Value> const& arg,
    v8::Handle<v8::Value>& result, v8::TryCatch&, bool& failAtParsing);

}
#endif
