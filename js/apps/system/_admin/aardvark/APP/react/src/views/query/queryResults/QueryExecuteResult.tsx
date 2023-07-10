import { WarningIcon } from "@chakra-ui/icons";
import {
  Alert,
  Box,
  Button,
  ButtonGroup,
  Flex,
  Spinner,
  Stack,
  Text
} from "@chakra-ui/react";
import React from "react";
import { useHistory } from "react-router";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { downloadPost } from "../../../utils/downloadHelper";
import { QueryResultType, useQueryContext } from "../QueryContextProvider";
import { CancelQueryButton } from "./CancelQueryButton";
import { CSVDownloadButton } from "./CSVDownloadButton";
import { QueryGeoView } from "./QueryGeoView";
import { QueryGraphView } from "./QueryGraphView";
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
    asyncJobId: queryResult.asyncJobId,
    index
  });
  const {
    displayTypes,
    graphDataType,
    currentDisplayType,
    setCurrentDisplayType
  } = useDisplayTypes({
    queryResult
  });

  if (queryResult.status === "error") {
    return (
      <Stack
        boxShadow="0 0 15px 0 rgba(0,0,0,0.2)"
        borderRadius="md"
        marginBottom="4"
        direction="row"
        alignItems="center"
        padding="2"
      >
        <ResultTypeBox queryResult={queryResult} />
        <Alert status="error" borderRadius="0">
          <Stack direction="row" alignItems="center">
            <WarningIcon color="red.500" />
            <Text>Query error: {queryResult.errorMessage}</Text>
          </Stack>
        </Alert>
        <RemoveResultButton index={index} />
      </Stack>
    );
  }
  if (queryResult.status === "loading") {
    return (
      <Flex
        boxShadow="0 0 15px 0 rgba(0,0,0,0.2)"
        borderRadius="md"
        marginBottom="4"
        direction="row"
        alignItems="center"
        padding="2"
        gap="2"
      >
        <ResultTypeBox queryResult={queryResult} />
        <Spinner size="sm" />
        <Text>Query in progress</Text>
        <CancelQueryButton index={index} asyncJobId={queryResult.asyncJobId} />
      </Flex>
    );
  }
  return (
    <Box
      boxShadow="0 0 15px 0 rgba(0,0,0,0.2)"
      borderRadius="md"
      marginBottom="4"
    >
      <QueryExecuteResultHeader
        queryResult={queryResult}
        setCurrentDisplayType={setCurrentDisplayType}
        currentDisplayType={currentDisplayType}
        displayTypes={displayTypes}
        index={index}
      />
      <WarningPane queryResult={queryResult} />
      <QueryExecuteResultDisplay
        currentDisplayType={currentDisplayType}
        queryResult={queryResult}
        graphDataType={graphDataType}
      />
      <QueryExecuteResultFooter
        currentDisplayType={currentDisplayType}
        queryResult={queryResult}
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
        colorScheme={currentDisplayType === "json" ? "blue" : "gray"}
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
            colorScheme={currentDisplayType === type ? "blue" : "gray"}
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
    if (typeof row === "object") {
      Object.keys(row).forEach(key => {
        if (typeof row[key] === "object") {
          allowCSVDownload = false;
        }
      });
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
  const { onQueryChange, setResetEditor, resetEditor, aqlJsonEditorRef } =
    useQueryContext();
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
        <Button
          size="sm"
          onClick={() => {
            onQueryChange({
              value: queryValue,
              parameter: queryBindParams || {}
            });
            setResetEditor(!resetEditor);
            aqlJsonEditorRef.current?.jsonEditor?.container?.scrollIntoView();
            aqlJsonEditorRef.current?.jsonEditor?.focus();
          }}
        >
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
  const history = useHistory();
  return (
    <Button
      size="sm"
      onClick={() => {
        setQueryGraphResult(queryResult);
        history.push(`/queries/graph`);
      }}
    >
      Open in Graph Viewer
    </Button>
  );
};
