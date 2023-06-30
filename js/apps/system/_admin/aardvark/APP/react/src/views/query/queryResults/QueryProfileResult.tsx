import { Box, Flex } from "@chakra-ui/react";
import React from "react";
import { QueryResultType } from "../QueryContextProvider";
import { ProfileResultDisplayJSON } from "./ProfileResultDisplayJSON";
import { RemoveResultButton } from "./RemoveResultButton";
import { ResultTypeBox } from "./ResultTypeBox";

export const QueryProfileResult = ({
  index,
  queryResult
}: {
  index: number;
  queryResult: QueryResultType;
}) => {
  return (
    <Box height="500px" key={index}>
      <Flex padding="2" alignItems="center">
        <ResultTypeBox queryResult={queryResult} />
        <Box marginLeft="auto">
          <RemoveResultButton index={index} />
        </Box>
      </Flex>
      <ProfileResultDisplayJSON defaultValue={queryResult.result} />
    </Box>
  );
};
