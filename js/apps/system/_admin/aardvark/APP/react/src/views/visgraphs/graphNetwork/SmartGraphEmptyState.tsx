import { Box, Button, Center, Text, Stack } from "@chakra-ui/react";
import React from "react";
import { useGraph } from "../GraphContext";

export const SmartGraphEmptyState = () => {
  const { graphName } = useGraph();

  return (
    <Center h="100%">
      <Box width="auto">
        <Stack spacing={4} align="center">
          <Text fontSize="md">
            SmartGraphs are not supported in the new graph viewer yet.
          </Text>
          <Button
            size="sm"
            colorScheme="gray"
            onClick={() => {
              window.App.navigate(`#graph/${graphName}`, { trigger: true });
            }}
          >
            Switch to the old graph viewer
          </Button>
        </Stack>
      </Box>
    </Center>
  );
};
