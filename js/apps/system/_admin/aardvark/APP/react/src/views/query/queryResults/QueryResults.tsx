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
      {queryResults.map((result, index) => {
        if (result.type === "profile" || result.type === "explain") {
          return (
            <QueryProfileResult key={index} index={index} result={result} />
          );
        }
        if (result.type === "query") {
          return (
            <QueryExecuteResult key={index} index={index} result={result} />
          );
        }
        return null;
      })}
    </Box>
  );
};
