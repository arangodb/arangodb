import { aql } from "arangojs";
import React, { useCallback } from "react";
import {
  getApiRouteForCurrentDB,
  getCurrentDB
} from "../../utils/arangoClient";
import { QueryExecutionOptions, QueryResultType } from "./QueryContextProvider";

export const useQueryExecutors = (
  setQueryResults: React.Dispatch<React.SetStateAction<QueryResultType[]>>
) => {
  const onRemoveResult = (index: number) => {
    setQueryResults(queryResults => {
      const newResults = [...queryResults];
      newResults.splice(index, 1);
      return newResults;
    });
  };
  const onExecute = useCallback(
    async ({ queryValue, queryBindParams }: QueryExecutionOptions) => {
      const route = getApiRouteForCurrentDB();
      try {
        const query = aql.literal(queryValue);
        // call the /cursor API, get async job id
        const cursorResponse = await route.post(
          "cursor",
          {
            query: query.toAQL(),
            bindVars: queryBindParams,
            options: {
              profile: true
            }
          },
          undefined,
          {
            "x-arango-async": "store"
          }
        );
        const asyncJobId = cursorResponse.headers[
          "x-arango-async-id"
        ] as string;
        setQueryResults(queryResults => [
          {
            type: "query",
            asyncJobId,
            status: "loading"
          },
          ...queryResults
        ]);
      } catch (e: any) {
        const message = e.message || e.response.body.errorMessage;
        window.arangoHelper.arangoError(
          `Could not execute query. Error - ${message}`
        );
      }
    },
    [setQueryResults]
  );

  const onProfile = async ({
    queryValue,
    queryBindParams
  }: QueryExecutionOptions) => {
    const currentDB = getCurrentDB();
    const literal = aql.literal(queryValue);
    const path = `/_admin/aardvark/query/profile`;
    const route = currentDB.route(path);
    const profile = await route.post({
      query: literal.toAQL(),
      bindVars: queryBindParams
    });
    setQueryResults(queryResults => [
      {
        type: "profile",
        result: profile.body.msg,
        status: "success"
      },
      ...queryResults
    ]);
  };
  const onExplain = useCallback(
    async ({ queryValue, queryBindParams }: QueryExecutionOptions) => {
      const currentDB = getCurrentDB();
      const literal = aql.literal(queryValue);
      const path = `/_admin/aardvark/query/explain`;
      const route = currentDB.route(path);
      try {
        const explainResult = await route.post({
          query: literal.toAQL(),
          bindVars: queryBindParams
        });
        setQueryResults(queryResults => [
          {
            type: "explain",
            result: explainResult.body.msg,
            status: "success"
          },
          ...queryResults
        ]);
      } catch (e: any) {
        const message = e.message || e.response.body.errorMessage;
        window.arangoHelper.arangoError(
          `Could not execute query. Error - ${message}`
        );
      }
    },
    [setQueryResults]
  );
  return { onExecute, onProfile, onExplain, onRemoveResult };
};
