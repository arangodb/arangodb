import React from "react";
import { QueryResultType } from "./ArangoQuery.types";

export const useQueryResultHandlers = () => {
  const [queryResults, setQueryResults] = React.useState<QueryResultType[]>([]);
  const setQueryResultById = (queryResult: QueryResultType) => {
    setQueryResults(prev => {
      const newResults = prev.map(prevQueryResult => {
        if (prevQueryResult.asyncJobId === queryResult.asyncJobId) {
          return queryResult;
        }
        return prevQueryResult;
      });
      return newResults;
    });
  };
  const appendQueryResultById = ({
    asyncJobId,
    result,
    status
  }: {
    asyncJobId?: string;
    result: any;
    status: "success" | "error" | "loading";
  }) => {
    setQueryResults(prev => {
      const newResults = prev.map(prevQueryResult => {
        if (asyncJobId === prevQueryResult.asyncJobId) {
          return {
            ...prevQueryResult,
            result: [...prevQueryResult.result, ...result],
            status
          };
        }
        return prevQueryResult;
      });
      return newResults;
    });
  };
  return {
    queryResults,
    setQueryResults,
    setQueryResultById,
    appendQueryResultById
  };
};
