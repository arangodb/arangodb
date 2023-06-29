import { ChevronDownIcon, CloseIcon, TimeIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  ButtonGroup,
  Divider,
  Flex,
  Grid,
  GridItem,
  Icon,
  IconButton,
  Popover,
  PopoverArrow,
  PopoverCloseButton,
  PopoverContent,
  PopoverTrigger,
  Spinner,
  Stack,
  Table,
  Tag,
  Text
} from "@chakra-ui/react";
import { round } from "lodash";
import React, { useEffect } from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { QueryResultType, useQueryContext } from "../QueryContextProvider";
import { ProfileResultDisplay } from "./ProfileResultDisplay";

export const QueryResults = () => {
  const { queryResults, onRemoveResult } = useQueryContext();
  if (queryResults.length === 0) {
    return null;
  }
  return (
    <Box background="white" marginTop="4">
      {queryResults.map((result, index) => {
        if (result.type === "profile" || result.type === "explain") {
          return (
            <Box height="500px" key={index}>
              <Flex padding="2" alignItems="center">
                <ResultTypeBox result={result} />
                <Box marginLeft="auto">
                  <RemoveResultButton
                    onRemoveResult={onRemoveResult}
                    index={index}
                  />
                </Box>
              </Flex>
              <ProfileResultDisplay defaultValue={result.result} />
            </Box>
          );
        }
        if (result.type === "query") {
          return <QueryExecuteResult index={index} result={result} />;
        }
        return null;
      })}
    </Box>
  );
};

type ProfileKey =
  | "initializing"
  | "parsing"
  | "optimizing ast"
  | "loading collections"
  | "instantiating plan"
  | "optimizing plan"
  | "instantiating executors"
  | "executing"
  | "finalizing";

const PROFILE_ORDER = [
  "initializing",
  "parsing",
  "optimizing ast",
  "loading collections",
  "instantiating plan",
  "optimizing plan",
  "instantiating executors",
  "executing",
  "finalizing"
];
const profileInfoMap = {
  initializing: {
    color: "rgb(48, 125, 153)",
    description: "startup time for query engine",
    label: "A"
  },
  parsing: {
    color: "rgb(241, 124, 176)",
    description: "query parsing",
    label: "B"
  },
  "optimizing ast": {
    color: "rgb(137, 110, 37)",
    description: "abstract syntax tree optimizations",
    label: "C"
  },
  "loading collections": {
    color: "rgb(93, 165, 218)",
    description: "loading collections",
    label: "D"
  },
  "instantiating plan": {
    color: "rgb(250, 164, 58)",
    description: "instantiation of initial execution plan",
    label: "E"
  },
  "optimizing plan": {
    color: "rgb(64, 74, 83)",
    description: "execution plan optimization and permutation",
    label: "F"
  },
  "instantiating executors": {
    color: "rgb(110, 112, 57)",
    description: "instantiation of query executors (incl. distribution)",
    label: "G"
  },
  executing: {
    color: "rgb(96, 189, 104)",
    description: "query execution",
    label: "H"
  },
  finalizing: {
    color: "rgb(221, 224, 114)",
    description: "query finalization",
    label: "I"
  }
};

const TimingInfo = ({ result }: { result: QueryResultType }) => {
  const { profile, stats } = result;
  const { executionTime } = stats || {};
  const finalExecutionTime = executionTime
    ? executionTime > 1
      ? `${round(executionTime, 3)} s`
      : `${round(executionTime * 1000, 3)} ms`
    : "";
  if (!finalExecutionTime || !executionTime) {
    return null;
  }
  // timing chart to show times as a chart, with total width fixed to 504px
  const timingChartInfo = PROFILE_ORDER.map(key => {
    const { label, color } = profileInfoMap[key as ProfileKey];
    const time = profile?.[key];
    if (!time) {
      return null;
    }
    const width = (time / executionTime) * 504;
    const widthPercentage = (width / 504) * 100;
    return {
      width,
      key,
      label,
      color,
      // minimum percentage is 1%
      widthPercentage: widthPercentage < 1 ? 1 : widthPercentage
    };
  });
  return (
    <Popover flip placement="top-start">
      <PopoverTrigger>
        <Stack spacing="1" direction="row" alignItems="center">
          <Icon as={TimeIcon} width="20px" height="20px" />
          <Text>{finalExecutionTime}</Text>
          <ChevronDownIcon />
        </Stack>
      </PopoverTrigger>
      <PopoverContent width="520px" padding="2">
        <PopoverArrow />
        <PopoverCloseButton />

        <Stack>
          <Text fontSize="lg" fontWeight="medium">
            Profiling Information
          </Text>
          {PROFILE_ORDER.map(key => {
            const profileInfo = profileInfoMap[key as ProfileKey];
            const time = profile?.[key];
            if (!time) {
              return null;
            }
            const finalTime =
              time > 1 ? `${round(time, 3)} s` : `${round(time * 1000, 3)} ms`;
            return (
              <Grid
                gridTemplateColumns="32px 100px 100px 1fr"
                alignItems="start"
              >
                <Tag
                  width="20px"
                  height="20px"
                  borderRadius="4"
                  backgroundColor={profileInfo.color}
                >
                  <Text color="white" fontWeight="bold">
                    {profileInfo.label}
                  </Text>
                </Tag>
                <Text>{finalTime}</Text>
                <Text>{key}</Text>
                <Text>{profileInfo.description}</Text>
                <GridItem marginTop="1" as={Divider} colSpan={4} />
              </Grid>
            );
          })}
        </Stack>
        <Flex>
          {timingChartInfo.map(info => {
            if (!info) {
              return null;
            }
            return (
              <Flex
                width={`${info.widthPercentage}%`}
                direction="column"
                alignItems="center"
              >
                <Box
                  width="100%"
                  display="inline-block"
                  height="20px"
                  backgroundColor={info.color}
                  marginRight="1"
                />
                <Text fontSize="xx-small" display="inline-block">
                  {info.label}
                </Text>
              </Flex>
            );
          })}
        </Flex>
      </PopoverContent>
    </Popover>
  );
};
const ResultTypeBox = ({ result }: { result: QueryResultType }) => {
  return (
    <Box
      display="inline-block"
      padding="1"
      borderRadius="4"
      backgroundColor="blue.400"
      color="white"
      textTransform="capitalize"
    >
      {result.type}
    </Box>
  );
};
const RemoveResultButton = ({
  onRemoveResult,
  index
}: {
  onRemoveResult: (index: number) => void;
  index: number;
}) => {
  return (
    <IconButton
      size="xs"
      variant="ghost"
      aria-label="Close"
      icon={<CloseIcon />}
      onClick={() => onRemoveResult(index)}
    />
  );
};

const useSyncJob = ({
  result,
  asyncJobId,
  index
}: {
  result: QueryResultType;
  asyncJobId?: string;
  index: number;
}) => {
  const { setQueryResultById, appendQueryResultById, aqlJsonEditorRef } =
    useQueryContext();
  useEffect(() => {
    const route = getApiRouteForCurrentDB();
    const checkCursor = async ({
      cursorId,
      asyncJobId
    }: {
      cursorId: string;
      asyncJobId?: string;
    }) => {
      const cursorResponse = await route.post(`/cursor/${cursorId}`);
      if (cursorResponse.statusCode === 200) {
        const { hasMore, result } = cursorResponse.body;
        if (hasMore) {
          appendQueryResultById({
            asyncJobId,
            result,
            status: "loading"
          });
          await checkCursor({ cursorId, asyncJobId });
        } else {
          appendQueryResultById({
            asyncJobId,
            result,
            status: "success"
          });
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
        if (jobResponse.statusCode === 204) {
          // job is still running
          checkJob();
        } else if (jobResponse.statusCode === 201) {
          // job is created
          const { hasMore, result, id: cursorId, extra } = jobResponse.body;
          const { stats, profile } = extra || {};
          setQueryResultById({
            queryResult: {
              type: "query",
              status: hasMore ? "loading" : "success",
              result,
              stats,
              profile,
              asyncJobId
            }
          });
          if (hasMore) {
            await checkCursor({ cursorId, asyncJobId: asyncJobId });
          }
        }
      } catch (e: any) {
        const message = e.response?.body?.errorMessage || e.message;
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
          queryResult: {
            type: "query",
            status: "error",
            asyncJobId,
            errorMessage: message
          }
        });
      }
    };
    if (!asyncJobId || result.status !== "loading") {
      return;
    }
    checkJob();
    // disabled because these functions don't need to be in deps
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [asyncJobId, index]);
  // .then(data => readJob(data, job.id))
  // .catch(error => onJobError(error, job.id));
};
const QueryExecuteResult = ({
  index,
  result
}: {
  index: number;
  result: QueryResultType;
}) => {
  const { onRemoveResult } = useQueryContext();
  useSyncJob({
    result,
    asyncJobId: result.asyncJobId,
    index
  });
  if (result.status === "error") {
    return <Box>{result.errorMessage}</Box>;
  }
  if (result.status === "loading") {
    return <Spinner key={result.asyncJobId} />;
  }
  return (
    <Box height="300px" key={index}>
      <Flex padding="2" alignItems="center">
        <Stack direction="row">
          <ResultTypeBox result={result} />
          <Stack spacing="1" direction="row" alignItems="center">
            <Icon as={Table} width="20px" height="20px" />
            <Text>
              {result.result.length}{" "}
              {result.result.length === 1 ? "element" : "elements"}
            </Text>
          </Stack>
          <TimingInfo result={result} />
        </Stack>
        <Stack direction="row" alignItems="center" marginLeft="auto">
          <ButtonGroup isAttached size="xs">
            <Button colorScheme="blue">JSON</Button>
            <Button>Graph</Button>
          </ButtonGroup>
          <RemoveResultButton onRemoveResult={onRemoveResult} index={index} />
        </Stack>
      </Flex>
      <ControlledJSONEditor
        isReadOnly
        key={index}
        mode="code"
        value={result.result}
        theme="ace/theme/textmate"
        htmlElementProps={{
          style: {
            height: "calc(100% - 48px)"
          }
        }}
        mainMenuBar={false}
      />
    </Box>
  );
};
