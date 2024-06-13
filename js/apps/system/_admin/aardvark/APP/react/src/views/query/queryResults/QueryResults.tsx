import { Box, Button, Flex } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { QueryExecuteResult } from "./QueryExecuteResult";
import { QueryProfileResult } from "./QueryProfileResult";
import { QueryResultError } from "./QueryResultError";
import { QueryResultLoading } from "./QueryResultLoading";

export const QueryResults = () => {
  const { queryResults, onRemoveAllResults } = useQueryContext();
  if (queryResults.length === 0) {
    return null;
  }
  return (
    <Box paddingX="3" paddingTop="5">
      <Flex width={"100%"} alignItems={"flex-end"}>
        <Button
          ml={"auto"}
          size="sm"
          colorScheme="gray"
          onClick={onRemoveAllResults}
        >
          Remove all results
        </Button>
      </Flex>
      <Box background="white" marginTop="4" paddingBottom="4">
        {queryResults.map((queryResult, index) => {
          if (
            queryResult.status === "loading" &&
            queryResult.type !== "query"
          ) {
            return (
              <QueryResultLoading
                key={index}
                index={index}
                queryResult={queryResult}
              />
            );
          }
          if (queryResult.status === "error") {
            return (
              <QueryResultError
                key={index}
                index={index}
                queryResult={queryResult}
              />
            );
          }
          if (
            queryResult.type === "profile" ||
            queryResult.type === "explain"
          ) {
            return (
              <QueryProfileResult
                key={index}
                index={index}
                queryResult={queryResult}
              />
            );
          }
          if (queryResult.type === "query") {
            return (
              <QueryExecuteResult
                key={queryResult.asyncJobId}
                index={index}
                queryResult={queryResult}
              />
            );
          }
          return null;
        })}
      </Box>
    </Box>
  );
};
