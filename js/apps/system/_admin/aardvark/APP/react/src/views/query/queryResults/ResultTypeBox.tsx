import { Box } from "@chakra-ui/react";
import React from "react";
import { QueryResultType } from "../QueryContextProvider";

export const ResultTypeBox = ({
  queryResult
}: {
  queryResult: QueryResultType;
}) => {
  return (
    <Box
      display="inline-block"
      padding="1"
      borderRadius="4"
      backgroundColor="blue.400"
      color="white"
      textTransform="capitalize"
    >
      {queryResult.type}
    </Box>
  );
};
