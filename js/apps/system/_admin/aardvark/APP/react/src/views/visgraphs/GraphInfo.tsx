import { HStack, Tag } from "@chakra-ui/react";
import React from "react";
import { useGraph } from "./GraphContext";

export const GraphInfo = () => {
  const { datasets, fetchDuration, selectedEntity } = useGraph();
  if (!datasets) {
    return null;
  }
  return (
    <HStack paddingX="8" spacing={4}>
      <Tag size={"md"} background="gray.800" color="white">
        {`${datasets.nodes.length} nodes`}
      </Tag>
      <Tag size={"md"} background="gray.800" color="white">
        {`${datasets.edges.length} edges`}
      </Tag>
      <Tag size={"md"} background="gray.800" color="white">
        {`Response time: ${fetchDuration} ms`}
      </Tag>
      {selectedEntity && (
        <Tag size={"md"} background="gray.800" color="white">
          {`_id: ${selectedEntity}`}
        </Tag>
      )}
    </HStack>
  );
};
