import { Box } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { QueryExecuteResult } from "./QueryExecuteResult";
import { QueryProfileResult } from "./QueryProfileResult";

export const QueryResults = () => {
  const { queryResults } = useQueryContext();
  if (queryResults.length === 0) {
    return null;
  }
  return (
    <Box background="white" marginTop="4">
      {queryResults.map((queryResult, index) => {
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
