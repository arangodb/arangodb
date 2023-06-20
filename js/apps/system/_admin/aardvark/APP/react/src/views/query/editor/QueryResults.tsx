import { ChevronDownIcon, CloseIcon, TimeIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  ButtonGroup,
  Flex,
  Icon,
  IconButton,
  Popover,
  PopoverContent,
  PopoverTrigger,
  Stack,
  Text
} from "@chakra-ui/react";
import React from "react";
import { Table } from "styled-icons/boxicons-regular";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";

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
            <Box height="500px">
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
          return (
            <Box height="300px">
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
                  <RemoveResultButton
                    onRemoveResult={onRemoveResult}
                    index={index}
                  />
                </Stack>
              </Flex>
              <ControlledJSONEditor
                key={index}
                mode="code"
                value={result.result}
                theme="ace/theme/textmate"
                htmlElementProps={{
                  style: {
                    height: "100%"
                  }
                }}
                mainMenuBar={false}
              />
            </Box>
          );
        }
        return null;
      })}
    </Box>
  );
};
const TimingInfo = ({ result }: { result: QueryResultType }) => {
  console.log({ result });
  return (
    <Popover>
      <PopoverTrigger>
        <Stack spacing="1" direction="row" alignItems="center">
          <Icon as={TimeIcon} width="20px" height="20px" />
          <Text>x ms</Text>
          <ChevronDownIcon />
        </Stack>
      </PopoverTrigger>
      <PopoverContent>
        <Box padding="2">
          <Text>Profiling Information</Text>
        </Box>
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
