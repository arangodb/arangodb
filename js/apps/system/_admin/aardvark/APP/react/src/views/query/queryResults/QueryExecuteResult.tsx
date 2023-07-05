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
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { downloadPost } from "../../../utils/downloadHelper";
import { QueryResultType, useQueryContext } from "../QueryContextProvider";
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
  const { displayTypes, graphDataType, currentView, setCurrentView } =
    useDisplayTypes({
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
      <Stack direction="row" alignItems="center" padding="2">
        <ResultTypeBox queryResult={queryResult} />
        <Spinner size="sm" />
        <Text>Query in progress</Text>
      </Stack>
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
        setCurrentView={setCurrentView}
        currentView={currentView}
        displayTypes={displayTypes}
        index={index}
      />
      <WarningPane queryResult={queryResult} />
      <QueryExecuteResultDisplay
        currentView={currentView}
        queryResult={queryResult}
        graphDataType={graphDataType}
      />
      <QueryExecuteResultFooter queryResult={queryResult} />
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
  currentView,
  graphDataType
}: {
  queryResult: QueryResultType;
  currentView: DisplayType;
  graphDataType: "graphObject" | "edgeArray";
}) => {
  if (currentView === "table") {
    return <QueryTableView queryResult={queryResult} />;
  }
  if (currentView === "geo") {
    return <QueryGeoView queryResult={queryResult} />;
  }
  if (currentView === "graph") {
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
          height: "calc(100% - 48px)"
        }
      }}
      mainMenuBar={false}
    />
  );
};
const QueryExecuteResultHeader = ({
  queryResult,
  setCurrentView,
  currentView,
  displayTypes,
  index
}: {
  queryResult: QueryResultType<any>;
  setCurrentView: React.Dispatch<React.SetStateAction<DisplayType>>;
  currentView: string;
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
          setCurrentView={setCurrentView}
          currentView={currentView}
          displayTypes={displayTypes}
        />
        <RemoveResultButton index={index} />
      </Stack>
    </Flex>
  );
};
const ViewSwitch = ({
  setCurrentView,
  currentView,
  displayTypes
}: {
  setCurrentView: React.Dispatch<React.SetStateAction<DisplayType>>;
  currentView: string;
  displayTypes: DisplayType[];
}) => {
  return (
    <ButtonGroup isAttached size="xs">
      <Button
        onClick={() => {
          setCurrentView("json");
        }}
        colorScheme={currentView === "json" ? "blue" : "gray"}
      >
        JSON
      </Button>
      {displayTypes.map(type => {
        return (
          <Button
            key={type}
            onClick={() => {
              setCurrentView(type);
            }}
            colorScheme={currentView === type ? "blue" : "gray"}
            textTransform="capitalize"
          >
            {type}
          </Button>
        );
      })}
    </ButtonGroup>
  );
};

const QueryExecuteResultFooter = ({
  queryResult
}: {
  queryResult: QueryResultType;
}) => {
  const { onQueryChange, setResetEditor, resetEditor } = useQueryContext();
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
        <Button
          size="sm"
          onClick={onDownload}
        >
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
          }}
        >
          Copy to editor
        </Button>
      </Stack>
    </Flex>
  );
};
