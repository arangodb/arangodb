import { useEffect } from "react";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { QueryResultType } from "../ArangoQuery.types";
import { useQueryContext } from "../QueryContextProvider";

export const useSyncQueryExecuteJob = ({
  queryResult,
  asyncJobId
}: {
  queryResult: QueryResultType;
  asyncJobId: string;
}) => {
  const {
    setQueryResultById,
    appendQueryResultById,
    aqlJsonEditorRef,
    queryResults,
    queryLimit
  } = useQueryContext();
  useEffect(() => {
    let timer = 0;
    const route = getApiRouteForCurrentDB();

    const deleteCursor = async ({
      cursorId
    }: {
      cursorId: any;
      asyncJobId: string;
    }) => {
      if (!cursorId) {
        return Promise.resolve();
      }
      const cursorResponse = await route.delete(`/cursor/${cursorId}`);
      return cursorResponse;
    };
    const checkCursor = async ({
      cursorId,
      asyncJobId
    }: {
      cursorId: string;
      asyncJobId?: string;
    }) => {
      const cursorResponse = await route.post(`/cursor/${cursorId}`);
      if (cursorResponse.status === 200) {
        const { hasMore, result } = cursorResponse.parsedBody;
        const shouldFetchMore =
          hasMore && (queryLimit === "all" || queryResults.length < queryLimit);
        appendQueryResultById({
          asyncJobId,
          result,
          status: shouldFetchMore ? "loading" : "success"
        });
        if (shouldFetchMore) {
          await checkCursor({ cursorId, asyncJobId });
        }
      }
    };
    const detectPositionError = (message: string) => {
      if (message) {
        if (message.match(/\d+:\d+/g) !== null) {
          const text = message.match(/'.*'/gs)?.[0];
          const position = message.match(/\d+:\d+/g)?.[0];
          return {
            text,
            position
          };
        } else {
          const text = message.match(/\(\w+\)/g)?.[0];
          return {
            text
          };
        }
      }
    };
    const checkJob = async () => {
      try {
        const jobResponse = await route.put(`/job/${asyncJobId}`);
        if (jobResponse.status === 204) {
          // job is still running
          timer = window.setTimeout(checkJob, 2000);
        } else if (jobResponse.status === 201) {
          // job is created
          const {
            hasMore,
            result,
            id: cursorId,
            extra
          } = jobResponse.parsedBody;
          const { stats, profile, warnings } = extra || {};
          const shouldFetchMore =
            hasMore && (queryLimit === "all" || result.length < queryLimit);
          setQueryResultById({
            queryValue: queryResult.queryValue,
            queryBindParams: queryResult.queryBindParams,
            type: "query",
            status: shouldFetchMore ? "loading" : "success",
            result,
            stats,
            warnings,
            profile,
            asyncJobId,
            queryLimit
          });
          if (shouldFetchMore) {
            await checkCursor({ cursorId, asyncJobId: asyncJobId });
          } else {
            try {
              await deleteCursor({
                cursorId,
                asyncJobId: asyncJobId
              });
            } catch (e) {
              // ignore
            }
          }
        }
      } catch (e: any) {
        const message = e.response?.parsedBody?.errorMessage || e.message;
        const positionError = detectPositionError(message);
        if (positionError) {
          const { text, position } = positionError;
          if (position && text) {
            const row = parseInt(position.split(":")[0]);
            const column = parseInt(position.split(":")[1]);
            const editor = (aqlJsonEditorRef.current as any)?.jsonEditor;

            const searchText = text.substring(1, text.length - 1);
            // this highlights the text
            const found = editor.aceEditor.find(searchText);

            if (!found) {
              editor.aceEditor.selection.moveCursorToPosition({
                row: row,
                column
              });
              editor.aceEditor.selection.selectLine();
            }
          }
        }
        setQueryResultById({
          queryValue: queryResult.queryValue,
          queryBindParams: queryResult.queryBindParams,
          type: "query",
          status: "error",
          asyncJobId,
          errorMessage: message
        });
      }
    };
    if (!asyncJobId || queryResult.status !== "loading") {
      return;
    }
    checkJob();
    return () => {
      clearTimeout(timer);
    };
    // disabled because these functions don't need to be in deps
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [asyncJobId]);
};
