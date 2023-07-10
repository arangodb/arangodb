import { Box, Button, Progress } from "@chakra-ui/react";
import React, { useMemo, useRef } from "react";
import { QueryResultType } from "../QueryContextProvider";
import { convertToGraphData, useSetupQueryGraph } from "./useSetupQueryGraph";

export const QueryGraphView = ({
  queryResult,
  graphDataType
}: {
  queryResult: QueryResultType;
  graphDataType: "graphObject" | "edgeArray";
}) => {
  const graphData = useMemo(() => {
    return convertToGraphData({ graphDataType, data: queryResult.result });
  }, [queryResult.result, graphDataType]);
  const visJsRef = useRef<HTMLDivElement>(null);
  const { progressValue, network } = useSetupQueryGraph({
    visJsRef,
    graphData
  });
  return (
    <Box position="relative">
      {progressValue < 100 && (
        <Box
          marginX="10"
          marginY="10"
          position="absolute"
          top="50%"
          transform={"translateY(-50%)"}
          width="calc(100% - 80px)"
        >
          <Progress value={progressValue} colorScheme="green" />
        </Box>
      )}
      {progressValue === 100 && (
        <Button
          position="absolute"
          zIndex="1"
          right="12px"
          top="12px"
          size="xs"
          variant="ghost"
          onClick={() => {
            network?.fit();
          }}
        >
          Reset zoom
        </Button>
      )}
      <Box height="500px" width="full" ref={visJsRef} />
    </Box>
  );
};
