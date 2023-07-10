import { Box } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { QueryExecuteResult } from "./QueryExecuteResult";
import { QueryProfileResult } from "./QueryProfileResult";
import { QueryResultError } from "./QueryResultError";
import { QueryResultLoading } from "./QueryResultLoading";

export const QueryResults = () => {
  const { queryResults } = useQueryContext();
  if (queryResults.length === 0) {
    return null;
  }
  return (
    <Box background="white" marginTop="4" paddingBottom="4">
      {queryResults.map((queryResult, index) => {
        if (queryResult.status === "loading") {
          return (
            <QueryResultLoading
              key={index}
              index={index}
              queryResult={queryResult}
            />
          );
        }
        if (queryResult.status === "error") {
          <QueryResultError
            key={index}
            index={index}
            queryResult={queryResult}
          />;
        }
        if (queryResult.type === "profile" || queryResult.type === "explain") {
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
              key={index}
              index={index}
              queryResult={queryResult}
            />
          );
        }
        return null;
      })}
    </Box>
  );
};
