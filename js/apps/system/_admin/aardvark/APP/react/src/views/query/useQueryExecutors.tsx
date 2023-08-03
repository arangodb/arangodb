import { literal } from "arangojs/aql";
import React, { useCallback } from "react";
import {
  getApiRouteForCurrentDB,
  getCurrentDB
} from "../../utils/arangoClient";
import { QueryResultType } from "./ArangoQuery.types";
import { QueryExecutionOptions } from "./QueryContextProvider";

export const useQueryExecutors = ({
  setQueryResults,
  setQueryResultById
}: {
  setQueryResults: React.Dispatch<React.SetStateAction<QueryResultType[]>>;
  setQueryResultById: (queryResult: QueryResultType) => void;
}) => {
  const onExecute = useCallback(
    async ({ queryValue, queryBindParams }: QueryExecutionOptions) => {
      const route = getApiRouteForCurrentDB();
      try {
        const literalValue = literal(queryValue);
        // call the /cursor API, get async job id
        const cursorResponse = await route.post(
          "cursor",
          {
            query: literalValue.toAQL(),
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
            queryValue,
            queryBindParams,
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
    const literalValue = literal(queryValue);
    const path = `/_admin/aardvark/query/profile`;
    const route = currentDB.route(path);
    setQueryResults(queryResults => [
      {
        queryValue,
        queryBindParams,
        type: "profile",
        status: "loading"
      },
      ...queryResults
    ]);
    try {
      const profile = await route.post({
        query: literalValue.toAQL(),
        bindVars: queryBindParams
      });
      setQueryResultById({
        queryValue,
        queryBindParams,
        type: "profile",
        result: profile.body.msg,
        status: "success"
      });
    } catch (e: any) {
      const message = e.message || e.response.body.errorMessage;
      setQueryResultById({
        queryValue,
        queryBindParams,
        type: "profile",
        status: "error",
        errorMessage: message
      });
      window.arangoHelper.arangoError(
        `Could not execute query. Error - ${message}`
      );
    }
  };
  const onExplain = useCallback(
    async ({ queryValue, queryBindParams }: QueryExecutionOptions) => {
      const currentDB = getCurrentDB();
      const literalValue = literal(queryValue);
      const path = `/_admin/aardvark/query/explain`;
      const route = currentDB.route(path);
      setQueryResults(queryResults => [
        {
          queryValue,
          queryBindParams,
          type: "explain",
          status: "loading"
        },
        ...queryResults
      ]);
      try {
        const explainResult = await route.post({
          query: literalValue.toAQL(),
          bindVars: queryBindParams
        });
        setQueryResultById({
          queryValue,
          queryBindParams,
          type: "explain",
          result: explainResult.body.msg,
          status: "success"
        });
      } catch (e: any) {
        const message = e.message || e.response.body.errorMessage;
        setQueryResultById({
          queryValue,
          queryBindParams,
          type: "profile",
          status: "error",
          errorMessage: message
        });
        window.arangoHelper.arangoError(
          `Could not execute query. Error - ${message}`
        );
      }
    },
    [setQueryResultById, setQueryResults]
  );
  return { onExecute, onProfile, onExplain };
};
