import { ExternalLinkIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Flex,
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
import { notifyError, notifySuccess } from "../../../utils/notifications";

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
  const [isAnyLoading, setIsAnyLoading] = useState(false);
  const [loadingState, setLoadingState] = useState<{ [key: string]: boolean }>(
    {}
  );

  const createExampleGraph = async ({ graphName }: { graphName: string }) => {
    try {
      setIsAnyLoading(true);
      setLoadingState(prev => {
        return {
          ...prev,
          [graphName]: true
        };
      });
      await getRouteForDB(window.frontendConfig.db, "_admin").post(
        `/aardvark/graph-examples/create/${graphName}`
      );
      notifySuccess(`Successfully created the graph: ${graphName}`);
      mutate("/graphs");
      onClose();
    } catch (e: any) {
      const errorMessage =
        e?.response?.parsedBody?.errorMessage || "Unknown error";
      notifyError(`Could not create graph: ${errorMessage}`);
    }
    setIsAnyLoading(false);
    setLoadingState(prev => {
      return {
        ...prev,
        [graphName]: false
      };
    });
  };
  const version = window.versionHelper.getDocuVersion();
  return (
    <VStack spacing={4} align="stretch">
      {exampleGraphsMap.map(exampleGraphField => {
        return (
          <HStack key={exampleGraphField.name}>
            <Box w="210px">
              <Text>{exampleGraphField.label}</Text>
            </Box>
            <Button
              colorScheme="green"
              size="xs"
              type="submit"
              isDisabled={isAnyLoading}
              isLoading={loadingState[exampleGraphField.name]}
              onClick={async () => {
                await createExampleGraph({ graphName: exampleGraphField.name });
              }}
            >
              Create
            </Button>
          </HStack>
        );
      })}
      <Flex gap="1" justifyContent="center">
        Need help? Visit our
        <Link
          target="_blank"
          textDecoration="underline"
          color="blue.600"
          _hover={{
            color: "blue.800"
          }}
          href={`https://docs.arangodb.com/${version}/graphs/example-graphs/`}
        >
          <Flex gap="1" alignItems="center">
            <Text>Graph Documentation</Text> <ExternalLinkIcon />
          </Flex>
        </Link>
      </Flex>
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
