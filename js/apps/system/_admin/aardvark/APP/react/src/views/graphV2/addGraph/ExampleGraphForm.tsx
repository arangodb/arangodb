import {
  Box,
  Button,
  HStack,
  Link,
  Stack,
  Text,
  VStack
} from "@chakra-ui/react";
import React, { useState } from "react";
import { mutate } from "swr";
import { ModalFooter } from "../../../components/modal";
import { getRouteForDB } from "../../../utils/arangoClient";

const exampleGraphsMap = [
  {
    label: "Knows Graph",
    name: "knows_graph"
  },
  {
    label: "Traversal Graph",
    name: "traversalGraph"
  },
  {
    label: "k Shortest Paths Graph",
    name: "kShortestPathsGraph"
  },
  {
    label: "Mps Graph",
    name: "mps_graph"
  },
  {
    label: "World Graph",
    name: "worldCountry"
  },
  {
    label: "Social Graph",
    name: "social"
  },
  {
    label: "City Graph",
    name: "routeplanner"
  },
  {
    label: "Connected Components Graph",
    name: "connectedComponentsGraph"
  }
];

export const ExampleGraphForm = ({ onClose }: { onClose: () => void }) => {
  const createExampleGraph = async ({ graphName }: { graphName: string }) => {
    const data = await getRouteForDB(window.frontendConfig.db, "_admin").post(
      `/aardvark/graph-examples/create/${graphName}`
    );
    return data.body;
  };
  const [isLoading, setIsLoading] = useState(false);

  return (
    <VStack spacing={4} align="stretch">
      {exampleGraphsMap.map(exampleGraphField => {
        return (
          <HStack key={exampleGraphField.name}>
            <Box w="210px">
              <Text>{exampleGraphField.label}</Text>
            </Box>
            <Button
              colorScheme="blue"
              size="xs"
              type="submit"
              isLoading={isLoading}
              onClick={async () => {
                setIsLoading(true);
                try {
                  await createExampleGraph({
                    graphName: exampleGraphField.name
                  });
                  window.arangoHelper.arangoNotification(
                    "Graph",
                    `Successfully created the graph: ${exampleGraphField.name}`
                  );
                  mutate("/graphs");
                  onClose();
                } catch (e: any) {
                  const errorMessage = e.response.body.errorMessage;
                  window.arangoHelper.arangoError(
                    "Could not add graph",
                    errorMessage
                  );
                }
                setIsLoading(false);
              }}
            >
              Create
            </Button>
          </HStack>
        );
      })}
        <Text textAlign="center">
          Need help? Visit our{" "}
          <Link href="https://chakra-ui.com" isExternal textDecoration="underline">
            Graph Documentation
          </Link>
        </Text>
      <ModalFooter>
        <VStack>
          <Stack direction="row" spacing={4} align="center">
            <Button onClick={onClose} colorScheme="gray">
              Cancel
            </Button>
          </Stack>
        </VStack>
      </ModalFooter>
    </VStack>
  );
};
