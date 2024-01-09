import { Box } from "@chakra-ui/react";
import React from "react";
import { QueryResultType } from "../ArangoQuery.types";

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
      backgroundColor="green.700"
      color="white"
      textTransform="capitalize"
    >
      {queryResult.type}
    </Box>
  );
};
