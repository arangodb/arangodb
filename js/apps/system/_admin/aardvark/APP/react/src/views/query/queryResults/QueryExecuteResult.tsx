import {
  Box,
  Button,
  ButtonGroup,
  Flex,
  Icon,
  Spinner,
  Stack,
  Table,
  Text
} from "@chakra-ui/react";
import React from "react";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { QueryResultType } from "../QueryContextProvider";
import { RemoveResultButton } from "./RemoveResultButton";
import { ResultTypeBox } from "./ResultTypeBox";
import { TimingInfo } from "./TimingInfo";
import { useSyncQueryExecuteJob } from "./useSyncQueryExecuteJob";

export const QueryExecuteResult = ({
  index,
  result
}: {
  index: number;
  result: QueryResultType;
}) => {
  useSyncQueryExecuteJob({
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
          <RemoveResultButton index={index} />
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
