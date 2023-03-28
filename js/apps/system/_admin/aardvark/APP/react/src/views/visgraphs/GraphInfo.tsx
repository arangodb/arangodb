import { Box, HStack, Tag, Text, Tooltip } from "@chakra-ui/react";
import { omit } from "lodash";
import React from "react";
import { SelectedEntityType, useGraph } from "./GraphContext";
import { useEdgeData } from "./graphModals/useEdgeData";
import { useNodeData } from "./graphModals/useNodeData";

export const GraphInfo = () => {
  const { datasets, fetchDuration } = useGraph();
  if (!datasets) {
    return null;
  }
  return (
    <>
      <HStack
        position="absolute"
        bottom="40px"
        left="12px"
        spacing={4}
        paddingY="2"
      >
        <Tag size={"md"} background="gray.800" color="white">
          {`${datasets.nodes.length} nodes`}
        </Tag>
        <Tag size={"md"} background="gray.800" color="white">
          {`${datasets.edges.length} edges`}
        </Tag>
        <Tag size={"md"} background="gray.800" color="white">
          {`Response time: ${fetchDuration} ms`}
        </Tag>
      </HStack>
      <SelectedEntityInfo />
    </>
  );
};
const useEntityData = (selectedEntity?: SelectedEntityType) => {
  const { entityId, type } = selectedEntity || {};
  const isNode = type === "node";
  const isEdge = type === "edge";
  const { nodeData, immutableIds: nodeImmutableIds } = useNodeData({
    nodeId: entityId,
    disabled: isEdge
  });
  const { edgeData, immutableIds: edgeImmutableIds } = useEdgeData({
    edgeId: entityId,
    disabled: isNode
  });
  return isNode
    ? { entityData: nodeData, immutableIds: nodeImmutableIds }
    : { entityData: edgeData, immutableIds: edgeImmutableIds };
};
const SelectedEntityInfo = () => {
  const { selectedEntity } = useGraph();
  const { entityData, immutableIds } = useEntityData(selectedEntity);
  if (!selectedEntity) {
    return null;
  }
  const displayData = omit(entityData, immutableIds) as {
    [key: string]: string;
  };

  return (
    <HStack paddingX="3" paddingY="2" background="white">
      <HStack>
        <Box>ID</Box>
        <Tag size={"md"} background="gray.800" color="white">
          {entityData?._id}
        </Tag>
      </HStack>
      <HStack>
        <Text>Attributes</Text>
        {Object.keys(displayData).map(key => {
          return (
            <Tooltip key={key} hasArrow label={displayData[key]}>
              <Tag size={"md"} background="gray.800" color="white">
                {key}
              </Tag>
            </Tooltip>
          );
        })}
      </HStack>
    </HStack>
  );
};
