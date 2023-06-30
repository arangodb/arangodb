import { Box, Flex } from "@chakra-ui/react";
import React from "react";
import { QueryResultType } from "../QueryContextProvider";
import { ProfileResultDisplayJSON } from "./ProfileResultDisplay";
import { RemoveResultButton } from "./RemoveResultButton";
import { ResultTypeBox } from "./ResultTypeBox";

export const QueryProfileResult = ({
  index,
  result
}: {
  index: number;
  result: QueryResultType;
}) => {
  return (
    <Box height="500px" key={index}>
      <Flex padding="2" alignItems="center">
        <ResultTypeBox result={result} />
        <Box marginLeft="auto">
          <RemoveResultButton index={index} />
        </Box>
      </Flex>
      <ProfileResultDisplayJSON defaultValue={result.result} />
    </Box>
  );
};
