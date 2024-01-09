import { WarningIcon } from "@chakra-ui/icons";
import {
  Alert, Stack,
  Text
} from "@chakra-ui/react";
import React from "react";
import { QueryResultType } from "../ArangoQuery.types";
import { RemoveResultButton } from "./RemoveResultButton";
import { ResultTypeBox } from "./ResultTypeBox";

export const QueryResultError = ({
  queryResult, index
}: {
  queryResult: QueryResultType<any>;
  index: number;
}) => {
  return (
    <Stack
      boxShadow="0 0 15px 0 rgba(0,0,0,0.2)"
      borderRadius="md"
      marginBottom="4"
      direction="row"
      alignItems="center"
      padding="2"
    >
      <ResultTypeBox queryResult={queryResult} />
      <Alert status="error" borderRadius="0">
        <Stack direction="row" alignItems="center">
          <WarningIcon color="red.500" />
          <Text>Query error: {queryResult.errorMessage}</Text>
        </Stack>
      </Alert>
      <RemoveResultButton index={index} />
    </Stack>
  );
};
