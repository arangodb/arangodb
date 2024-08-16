import { WarningIcon } from "@chakra-ui/icons";
import {
  Alert,
  Box,
  Button,
  ButtonGroup,
  Flex,
  Stack,
  Text
} from "@chakra-ui/react";
import React, { useMemo } from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { downloadPost } from "../../../utils/downloadHelper";
import { QueryResultType } from "../ArangoQuery.types";
import { useQueryContext } from "../QueryContextProvider";
import { CSVDownloadButton } from "./CSVDownloadButton";
import { QueryGeoView } from "./QueryGeoView";
import { QueryGraphView } from "./QueryGraphView";
import { QueryResultError } from "./QueryResultError";
import { QueryResultLoading } from "./QueryResultLoading";

import { QueryTableView } from "./QueryTableView";
import { RemoveResultButton } from "./RemoveResultButton";
import { ResultTypeBox } from "./ResultTypeBox";
import { TimingInfo } from "./TimingInfo";
import { DisplayType, useDisplayTypes } from "./useDisplayTypes";
import { useSyncQueryExecuteJob } from "./useSyncQueryExecuteJob";

export const QueryExecuteResult = ({
  index,
  queryResult
}: {
  index: number;
  queryResult: QueryResultType;
}) => {
  useSyncQueryExecuteJob({
    queryResult,
    asyncJobId: queryResult.asyncJobId
  });
  const {
    displayTypes,
    graphDataType,
    currentDisplayType,
    setCurrentDisplayType
  } = useDisplayTypes({
    queryResult
  });
  const limitedQueryResult = useMemo(() => {
    const limitedResult =
      queryResult.queryLimit !== "all"
        ? queryResult.result?.slice(0, queryResult.queryLimit)
        : queryResult.result;
    return {
      ...queryResult,
      result: limitedResult
    };
  }, [queryResult]);
  if (queryResult.status === "loading") {
    return <QueryResultLoading index={index} queryResult={queryResult} />;
  }
  if (queryResult.status === "error") {
    return <QueryResultError index={index} queryResult={queryResult} />;
  }
  return (
    <Box
      boxShadow="0 0 15px 0 rgba(0,0,0,0.2)"
      borderRadius="md"
      marginBottom="4"
    >
      <QueryExecuteResultHeader
        queryResult={limitedQueryResult}
        setCurrentDisplayType={setCurrentDisplayType}
        currentDisplayType={currentDisplayType}
        displayTypes={displayTypes}
        index={index}
      />
      <WarningPane queryResult={limitedQueryResult} />
      <QueryExecuteResultDisplay
        currentDisplayType={currentDisplayType}
        queryResult={limitedQueryResult}
        graphDataType={graphDataType}
      />
      <QueryExecuteResultFooter
        currentDisplayType={currentDisplayType}
        queryResult={limitedQueryResult}
      />
    </Box>
  );
};

const WarningPane = ({ queryResult }: { queryResult: QueryResultType }) => {
  const { warnings } = queryResult;
  if (!warnings || !warnings.length) {
    return null;
  }
  return (
    <Alert status="warning" borderRadius="0">
      <Stack direction="row" alignItems="center">
        <WarningIcon color="yellow.500" />
        <Text>Warnings:</Text>
        {warnings.map((warning, index) => {
          return (
            <Text key={index}>
              [{warning.code}], {warning.message}
            </Text>
          );
        })}
      </Stack>
    </Alert>
  );
};
const QueryExecuteResultDisplay = ({
  queryResult,
  currentDisplayType,
  graphDataType
}: {
  queryResult: QueryResultType;
  currentDisplayType: DisplayType;
  graphDataType: "graphObject" | "edgeArray";
}) => {
  if (currentDisplayType === "table") {
    return <QueryTableView queryResult={queryResult} />;
  }
  if (currentDisplayType === "geo") {
    return <QueryGeoView queryResult={queryResult} />;
  }
  if (currentDisplayType === "graph") {
    return (
      <QueryGraphView graphDataType={graphDataType} queryResult={queryResult} />
    );
  }
  const { result } = queryResult;
  return (
    <ControlledJSONEditor
      isReadOnly
      mode="code"
      value={result}
      theme="ace/theme/textmate"
      htmlElementProps={{
        style: {
          height: "400px"
        }
      }}
      mainMenuBar={false}
    />
  );
};
const QueryExecuteResultHeader = ({
  queryResult,
  setCurrentDisplayType,
  currentDisplayType,
  displayTypes,
  index
}: {
  queryResult: QueryResultType<any>;
  setCurrentDisplayType: React.Dispatch<React.SetStateAction<DisplayType>>;
  currentDisplayType: string;
  displayTypes: DisplayType[];
  index: number;
}) => {
  return (
    <Flex padding="2" alignItems="center">
      <Stack direction="row">
        <ResultTypeBox queryResult={queryResult} />
        <TimingInfo queryResult={queryResult} />
      </Stack>
      <Stack direction="row" alignItems="center" marginLeft="auto">
        <ViewSwitch
          setCurrentDisplayType={setCurrentDisplayType}
          currentDisplayType={currentDisplayType}
          displayTypes={displayTypes}
        />
        <RemoveResultButton index={index} />
      </Stack>
    </Flex>
  );
};
const ViewSwitch = ({
  setCurrentDisplayType,
  currentDisplayType,
  displayTypes
}: {
  setCurrentDisplayType: React.Dispatch<React.SetStateAction<DisplayType>>;
  currentDisplayType: string;
  displayTypes: DisplayType[];
}) => {
  return (
    <ButtonGroup isAttached size="xs">
      <Button
        onClick={() => {
          setCurrentDisplayType("json");
        }}
        colorScheme={currentDisplayType === "json" ? "green" : "gray"}
      >
        JSON
      </Button>
      {displayTypes.map(type => {
        return (
          <Button
            key={type}
            onClick={() => {
              setCurrentDisplayType(type);
            }}
            colorScheme={currentDisplayType === type ? "green" : "gray"}
            textTransform="capitalize"
          >
            {type}
          </Button>
        );
      })}
    </ButtonGroup>
  );
};

export const getAllowCSVDownload = (queryResult: QueryResultType) => {
  // if nested array, don't allow CSV download
  if (!queryResult.result || !queryResult.result.length) {
    return false;
  }
  let allowCSVDownload = true;
  queryResult.result.forEach((row: any) => {
    if (!row) {
      allowCSVDownload = false;
      return;
    }
    if (typeof row === "object") {
      Object.keys(row).forEach(key => {
        if (typeof row[key] === "object") {
          allowCSVDownload = false;
        }
      });
    }
    if (typeof row === "number" || typeof row === "string") {
      allowCSVDownload = false;
    }
  });
  return allowCSVDownload;
};

const QueryExecuteResultFooter = ({
  queryResult,
  currentDisplayType
}: {
  queryResult: QueryResultType;
  currentDisplayType: DisplayType;
}) => {
  const {
    onQueryChange,
    setResetEditor,
    resetEditor,
    aqlJsonEditorRef,
    setCurrentView
  } = useQueryContext();
  const { queryValue, queryBindParams } = queryResult;
  const onDownload = async () => {
    const path = `query/result/download`;
    await downloadPost({
      url: path,
      body: {
        query: queryValue,
        bindVars: queryBindParams || {}
      }
    });
  };
  const onCopyToQueryEditor = () => {
    onQueryChange({
      value: queryValue,
      parameter: queryBindParams || {}
    });
    setCurrentView("editor");
    setResetEditor(!resetEditor);
    aqlJsonEditorRef.current?.jsonEditor?.focus();
    const containerDiv = document.querySelector("#content-react > div");
    if (containerDiv) {
      containerDiv.scrollTop = 0;
    }
  };
  return (
    <Flex padding="2" alignItems="center" justifyContent="end">
      <Stack direction="row">
        {currentDisplayType === "graph" && (
          <OpenInGraphButton queryResult={queryResult} />
        )}
        <CSVDownloadButton queryResult={queryResult} />
        <Button size="sm" onClick={onDownload}>
          Download JSON
        </Button>
        <Button size="sm" onClick={onCopyToQueryEditor}>
          Copy query to editor
        </Button>
      </Stack>
    </Flex>
  );
};

const OpenInGraphButton = ({
  queryResult
}: {
  queryResult: QueryResultType;
}) => {
  const { setQueryGraphResult } = useQueryContext();
  return (
    <Button
      size="sm"
      onClick={() => {
        setQueryGraphResult(queryResult);
      }}
    >
      Open in Graph Viewer
    </Button>
  );
};
