import { literal } from "arangojs/aql";
import { uniqueId } from "lodash";
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
    async ({
      queryValue,
      queryBindParams,
      queryOptions,
      disabledRules
    }: QueryExecutionOptions) => {
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
              profile: true,
              ...queryOptions,
              optimizer: {
                rules: disabledRules
              }
            }
          },
          undefined,
          {
            "x-arango-async": "store"
          }
        );
        const asyncJobId = cursorResponse.headers.get(
          "x-arango-async-id"
        ) as string;
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
        const message = e.message || e.response.parsedBody.errorMessage;
        window.arangoHelper.arangoError(
          `Could not execute query. Error - ${message}`
        );
      }
    },
    [setQueryResults]
  );

  const onProfile = async ({
    queryValue,
    queryBindParams,
    queryOptions,
    disabledRules
  }: QueryExecutionOptions) => {
    const currentDB = getCurrentDB();
    const literalValue = literal(queryValue);
    const path = `/_admin/aardvark/query/profile`;
    const route = currentDB.route(path);
    const id = uniqueId();
    setQueryResults(queryResults => [
      {
        queryValue,
        queryBindParams,
        type: "profile",
        status: "loading",
        asyncJobId: id
      },
      ...queryResults
    ]);
    try {
      const profile = await route.post({
        query: literalValue.toAQL(),
        bindVars: queryBindParams,
        options: {
          ...queryOptions,
          optimizer: {
            rules: disabledRules
          }
        }
      });
      setQueryResultById({
        asyncJobId: id,
        queryValue,
        queryBindParams,
        type: "profile",
        result: profile.parsedBody.msg,
        status: "success"
      });
    } catch (e: any) {
      const message = e.message || e.response.parsedBody.errorMessage;
      setQueryResultById({
        asyncJobId: id,
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
    async ({
      queryValue,
      queryBindParams,
      queryOptions,
      disabledRules
    }: QueryExecutionOptions) => {
      const currentDB = getCurrentDB();
      const literalValue = literal(queryValue);
      const path = `/_admin/aardvark/query/explain`;
      const route = currentDB.route(path);
      const id = uniqueId();
      setQueryResults(queryResults => [
        {
          queryValue,
          queryBindParams,
          type: "explain",
          status: "loading",
          asyncJobId: id
        },
        ...queryResults
      ]);
      try {
        const explainResult = await route.post({
          query: literalValue.toAQL(),
          bindVars: queryBindParams,
          options: {
            ...queryOptions,
            optimizer: {
              rules: disabledRules
            }
          }
        });
        setQueryResultById({
          asyncJobId: id,
          queryValue,
          queryBindParams,
          type: "explain",
          result: explainResult.parsedBody.msg,
          status: "success"
        });
      } catch (e: any) {
        const message = e.message || e.response.parsedBody.errorMessage;
        setQueryResultById({
          asyncJobId: id,
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
