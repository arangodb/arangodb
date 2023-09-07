import { Alert, AlertIcon, Button, Stack, Text } from "@chakra-ui/react";
import React, { useEffect } from "react";
import { AddEdgeModal } from "./AddEdgeModal";
import { useGraph } from "../GraphContext";

export const AddEdgeHandler = () => {
  const { network, selectedAction, setSelectedAction } = useGraph();
  useEffect(() => {
    if (network) {
      if (
        selectedAction?.action === "add" &&
        selectedAction.entityType === "edge"
      ) {
        network.addEdgeMode();
      } else {
        network.disableEditMode();
      }
    }
    return () => {
      network?.disableEditMode();
    };
  }, [selectedAction, network]);

  return (
    <>
      <Alert status="info" position="absolute" top="10" width="auto">
        <AlertIcon />
        <Stack spacing={4} direction="row" align="center">
          <Text fontSize="md">
            &quot;Add edge mode&quot; is on: Click a node and drag the edge to
            the end node
          </Text>
          <Button
            onClick={() => setSelectedAction(undefined)}
            colorScheme="blue"
          >
            Turn off
          </Button>
        </Stack>
      </Alert>
      {selectedAction?.from && selectedAction.to && <AddEdgeModal />}
    </>
  );
};
