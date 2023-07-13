import {
  CheckCircleIcon,
  ChevronDownIcon,
  TimeIcon,
  WarningIcon
} from "@chakra-ui/icons";
import {
  Box,
  Divider,
  Flex,
  Grid,
  GridItem,
  Icon,
  Popover,
  PopoverArrow,
  PopoverBody,
  PopoverCloseButton,
  PopoverContent,
  PopoverTrigger,
  Stack,
  Tag,
  Text
} from "@chakra-ui/react";
import { round } from "lodash";
import React from "react";
import { Table } from "styled-icons/boxicons-regular";
import { QueryResultType } from "../ArangoQuery.types";

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
export const TimingInfo = ({
  queryResult
}: {
  queryResult: QueryResultType;
}) => {
  const { profile, stats } = queryResult;
  const { executionTime, writesExecuted, writesIgnored } = stats || {};
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
    <>
      <Stack spacing="1" direction="row" alignItems="center">
        <Icon as={Table} width="20px" height="20px" />
        <Text>
          {queryResult.result.length}{" "}
          {queryResult.result.length === 1 ? "element" : "elements"}
        </Text>
      </Stack>
      <Popover flip placement="top-start">
        <PopoverTrigger>
          <Stack
            cursor="pointer"
            spacing="1"
            direction="row"
            alignItems="center"
          >
            <Icon as={TimeIcon} width="20px" height="20px" />
            <Text>{finalExecutionTime}</Text>
            <ChevronDownIcon />
          </Stack>
        </PopoverTrigger>
        <PopoverContent width="520px" padding="2">
          <PopoverArrow />
          <PopoverCloseButton />
          <PopoverBody>
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
                  time > 1
                    ? `${round(time, 3)} s`
                    : `${round(time * 1000, 3)} ms`;
                return (
                  <Grid
                    key={key}
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
                    key={info.key}
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
          </PopoverBody>
        </PopoverContent>
      </Popover>
      <WritesInfo
        writesExecuted={writesExecuted}
        writesIgnored={writesIgnored}
      />
    </>
  );
};

const WritesInfo = ({
  writesExecuted,
  writesIgnored
}: {
  writesExecuted: any;
  writesIgnored: any;
}) => {
  const hasWrites = writesExecuted > 0 || writesIgnored > 0;

  if (!hasWrites) {
    return null;
  }
  return (
    <>
      <Stack spacing="1" direction="row" alignItems="center">
        <CheckCircleIcon color="green.500" />
        <Text>
          {writesExecuted} {writesExecuted === 1 ? "write" : "writes"} executed
        </Text>
      </Stack>
      <Stack spacing="1" direction="row" alignItems="center">
        {writesIgnored === 0 ? (
          <CheckCircleIcon color="green.500" />
        ) : (
          <WarningIcon color="yellow.500" />
        )}
        <Text>
          {writesIgnored} {writesIgnored === 1 ? "write" : "writes"} ignored
        </Text>
      </Stack>
    </>
  );
};
