import { aql } from "arangojs";
import React from "react";
import { getCurrentDB } from "../../utils/arangoClient";
import { QueryResultType, QueryExecutionOptions } from "./QueryContextProvider";

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
  const onExecute = async ({
    queryValue,
    queryBindParams
  }: QueryExecutionOptions) => {
    const currentDB = getCurrentDB();
    try {
      const cursor = await currentDB.query(
        // An AQL literal created from a normal multi-line string
        aql.literal(queryValue),
        queryBindParams
      );
      const result = await cursor.all();
      const extra = cursor.extra;
      setQueryResults(queryResults => [
        {
          type: "query",
          result,
          extra
        },
        ...queryResults
      ]);
    } catch (e: any) {
      const message = e.message || e.response.body.errorMessage;
      window.arangoHelper.arangoError(
        `Could not execute query. Error - ${message}`
      );
    }
  };

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
        result: profile.body.msg
      },
      ...queryResults
    ]);
  };
  const onExplain = async ({
    queryValue,
    queryBindParams
  }: QueryExecutionOptions) => {
    const currentDB = getCurrentDB();
    const literal = aql.literal(queryValue);
    const path = `/_admin/aardvark/query/explain`;
    const route = currentDB.route(path);
    const explainResult = await route.post({
      query: literal.toAQL(),
      bindVars: queryBindParams
    });
    setQueryResults(queryResults => [
      {
        type: "explain",
        result: explainResult.body.msg
      },
      ...queryResults
    ]);
  };
  return { onExecute, onProfile, onExplain, onRemoveResult };
};
