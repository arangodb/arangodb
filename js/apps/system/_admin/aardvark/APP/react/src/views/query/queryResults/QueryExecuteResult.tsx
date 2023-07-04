import { WarningIcon } from "@chakra-ui/icons";
import {
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
import { QueryResultType } from "../QueryContextProvider";
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
      <Stack direction="row" alignItems="center" padding="2">
        <ResultTypeBox queryResult={queryResult} />
        <WarningIcon color="red.500" />
        <Text>Query error: {queryResult.errorMessage}</Text>
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
      <Flex padding="2" alignItems="center">
        <Stack direction="row">
          <ResultTypeBox queryResult={queryResult} />
          <TimingInfo queryResult={queryResult} />
        </Stack>
        <Stack direction="row" alignItems="center" marginLeft="auto">
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
          <RemoveResultButton index={index} />
        </Stack>
      </Flex>
      <QueryExecuteResultDisplay
        currentView={currentView}
        queryResult={queryResult}
        graphDataType={graphDataType}
      />
    </Box>
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
  if(currentView === "table") {
    return <QueryTableView queryResult={queryResult} />
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
