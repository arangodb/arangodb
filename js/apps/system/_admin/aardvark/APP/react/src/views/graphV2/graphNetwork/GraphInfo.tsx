import { Box, Flex, HStack, Tag, Text, Tooltip } from "@chakra-ui/react";
import { omit } from "lodash";
import React from "react";
import { SelectedEntityType } from "../GraphAction.types";
import { useGraph } from "../GraphContext";
import { useEdgeData } from "../graphModals/useEdgeData";
import { useNodeData } from "../graphModals/useNodeData";

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
  const attributes = Object.keys(displayData);
  return (
    <HStack paddingX="3" background="white">
      <HStack>
        <Box>ID</Box>
        <Tag size={"md"} background="gray.800" color="white">
          {entityData?._id}
        </Tag>
      </HStack>
      <Attributes attributes={attributes} displayData={displayData} />
    </HStack>
  );
};
const Attributes = ({
  attributes,
  displayData
}: {
  attributes: string[];
  displayData: { [key: string]: string };
}) => {
  if (attributes.length === 0) {
    return null;
  }
  return (
    <Flex direction="row" flexWrap="wrap" alignItems="center">
      <Text>Attributes</Text>
      {attributes.map(attribute => {
        const keyData = displayData[attribute];
        let tooltip = `${keyData}`;
        if (typeof keyData === "object") {
          tooltip = JSON.stringify(keyData);
        }
        return (
          <Tooltip key={attribute} hasArrow label={tooltip}>
            <Tag
              marginY="2"
              marginLeft="2"
              size={"md"}
              background="gray.800"
              color="white"
            >
              {attribute}
            </Tag>
          </Tooltip>
        );
      })}
    </Flex>
  );
};
