import { Alert, Box, Button, Progress, Stack, Text } from "@chakra-ui/react";
import React, { useRef } from "react";
import { useElementPosition } from "../../../../components/hooks/useElementPosition";
import { useGraph } from "../GraphContext";
import { GraphInfo } from "./GraphInfo";
import { GraphRightClickMenu } from "./GraphRightClickMenu";
import { useSetupGraphNetwork } from "./useSetupGraphNetwork";

export const GraphNetwork = () => {
  const visJsRef = useRef<HTMLDivElement>(null);
  const { progressValue } = useSetupGraphNetwork({ visJsRef });
  const containerRef = useRef<HTMLDivElement>(null);
  const containerPosition = useElementPosition(containerRef);
  const height = containerPosition
    ? `calc(100vh - ${containerPosition.top}px)`
    : "80vh";
  return (
    <Box
      ref={containerRef}
      height={height}
      background="white"
      position="relative"
    >
      <GraphNetworkInner
        containerRef={containerRef}
        progressValue={progressValue}
        visJsRef={visJsRef}
      />
    </Box>
  );
};

const GraphNetworkInner = ({
  containerRef,
  progressValue,
  visJsRef
}: {
  containerRef: React.RefObject<HTMLDivElement>;
  progressValue: number;
  visJsRef: React.RefObject<HTMLDivElement>;
}) => {
  const { graphError, datasets } = useGraph();
  return (
    <>
      <GraphRightClickMenu
        portalProps={{
          containerRef
        }}
      />
      <Box id="graphNetworkWrap" height="full" backgroundColor="white">
        {datasets?.nodes.length !== 0 && progressValue < 100 && !graphError ? (
          <Box
            width="full"
            position="absolute"
            paddingX="10"
            top="50%"
            translateY="-100%"
            backgroundColor="white"
          >
            <Progress value={progressValue} colorScheme="green" />
          </Box>
        ) : null}
        {datasets?.nodes.length === 0 && (
          <Alert status="info" position="absolute">
            Right-click anywhere to add a new node to the graph
          </Alert>
        )}
        <GraphError />
        {!graphError && (
          <Box ref={visJsRef} height="calc(100% - 40px)" width="full" />
        )}
        <GraphInfo />
      </Box>
    </>
  );
};

const GraphError = () => {
  const { graphError, onRestoreDefaults } = useGraph();

  if (!graphError) {
    return null;
  }
  const errorMessage = graphError.response?.parsedBody.errorMessage;
  return (
    <Box
      width="full"
      position="absolute"
      paddingX="10"
      top="30%"
      translateY="-100%"
      display="flex"
      alignItems="center"
      justifyContent="center"
    >
      <Alert
        status="error"
        position="absolute"
        top="10"
        width="auto"
        borderRadius="sm"
      >
        <Stack>
          <Text>Something went wrong while loading the graph</Text>
          {errorMessage && <Text>Error: {errorMessage}</Text>}
          <Text>Code: {graphError.code}</Text>
          <Button
            onClick={onRestoreDefaults}
            colorScheme="red"
            variant="ghost"
            size="sm"
          >
            Restore defaults
          </Button>
        </Stack>
      </Alert>
    </Box>
  );
};
