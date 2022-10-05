#include "Basics/voc-errors.h"
#include "gtest/gtest.h"

#include "Basics/ResultT.h"
#include "Pregel/Messaging/Message.h"

using namespace arangodb;
using namespace arangodb::pregel;

TEST(PregelWorkerConductorMessages, converts_variant_into_specific_type) {
  auto variant = MessagePayload{ResultT<Stored>{}};
  ASSERT_EQ(getResultTMessage<Stored>(variant), ResultT<Stored>{});
}

TEST(PregelWorkerConductorMessages,
     converts_variant_error_into_specific_type_error) {
  auto variant =
      MessagePayload{ResultT<Stored>::error(TRI_ERROR_FAILED, "Some error")};
  ASSERT_EQ(getResultTMessage<Stored>(variant).errorNumber(), TRI_ERROR_FAILED);
  ASSERT_NE(
      getResultTMessage<Stored>(variant).errorMessage().find("Some error"),
      std::string::npos);
}

TEST(PregelWorkerConductorMessages,
     fails_conversion_when_variant_does_not_hold_requested_type) {
  auto variant = MessagePayload{ResultT<GraphLoaded>{}};
  ASSERT_TRUE(getResultTMessage<Stored>(variant).fail());
}

TEST(PregelWorkerConductorMessages,
     fails_conversion_when_variant_does_not_include_a_resultT) {
  auto variant = MessagePayload{LoadGraph{}};
  ASSERT_TRUE(getResultTMessage<GraphLoaded>(variant).fail());
}
