import {
  Flex,
  Spinner, Text
} from "@chakra-ui/react";
import React from "react";
import { QueryResultType } from "../ArangoQuery.types";
import { CancelQueryButton } from "./CancelQueryButton";
import { ResultTypeBox } from "./ResultTypeBox";

export const QueryResultLoading = ({
  queryResult, index
}: {
  queryResult: QueryResultType;
  index: number;
}) => {
  return (
    <Flex
      boxShadow="0 0 15px 0 rgba(0,0,0,0.2)"
      borderRadius="md"
      marginBottom="4"
      direction="row"
      alignItems="center"
      padding="2"
      gap="2"
    >
      <ResultTypeBox queryResult={queryResult} />
      <Spinner size="sm" />
      <Text>Query in progress</Text>
      <CancelQueryButton index={index} asyncJobId={queryResult.asyncJobId} />
    </Flex>
  );
};
