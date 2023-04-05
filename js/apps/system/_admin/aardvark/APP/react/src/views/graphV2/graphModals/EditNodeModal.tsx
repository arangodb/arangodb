import { Button, Flex, HStack, Spinner, Stack } from "@chakra-ui/react";
import { JsonEditor } from "jsoneditor-react";
import { omit } from "lodash";
import React, { useState } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import { getCurrentDB } from "../../../utils/arangoClient";
import { SelectedActionType } from "../GraphAction.types";
import { useGraph } from "../GraphContext";
import { AttributesInfo } from "./AttributesInfo";
import { useNodeData } from "./useNodeData";

const useUpdateNodeAction = ({
  selectedAction,
  graphName,
  onSuccess,
  onFailure
}: {
  selectedAction?: SelectedActionType;
  graphName: string;
  onSuccess: () => void;
  onFailure: () => void;
}) => {
  const { nodeId } = selectedAction?.entity || {};
  const { nodeData, isLoading, immutableIds } = useNodeData({ nodeId });
  const updateNode = async ({
    nodeId,
    updatedData
  }: {
    nodeId: string;
    updatedData: { [key: string]: string };
  }) => {
    const slashPos = nodeId.indexOf("/");
    const collection = nodeId.substring(0, slashPos);
    const db = getCurrentDB();
    const graphCollection = db.graph(graphName).vertexCollection(collection);
    try {
      await graphCollection.update(nodeId, updatedData);
      window.arangoHelper.arangoNotification(
        `The node ${nodeId} was successfully updated`
      );
      onSuccess();
    } catch (error) {
      console.log("Error saving document: ", error);
      window.arangoHelper.arangoError("Graph", "Could not update this node.");
      onFailure();
    }
  };
  return { nodeId, nodeData, immutableIds, isLoading, updateNode };
};

export const EditNodeModal = () => {
  const { graphName, selectedAction, onClearAction } = useGraph();
  const {
    nodeId,
    nodeData,
    immutableIds,
    isLoading,
    updateNode
  } = useUpdateNodeAction({
    selectedAction,
    graphName,
    onSuccess: onClearAction,
    onFailure: onClearAction
  });
  const mutableNodeData = omit(nodeData, immutableIds);
  const [json, setJson] = useState(mutableNodeData);

  if (!nodeId) {
    return null;
  }
  if (isLoading) {
    return (
      <Flex width="full" height="full" align="center">
        <Spinner />
      </Flex>
    );
  }

  return (
    <Modal isOpen onClose={onClearAction}>
      <ModalHeader>Edit Node: {nodeId}</ModalHeader>
      <ModalBody>
        <Stack spacing="4">
          <AttributesInfo attributes={nodeData} />
          <JsonEditor
            value={mutableNodeData}
            onChange={value => {
              setJson(value);
            }}
            mode={"code"}
            history={true}
          />
        </Stack>
      </ModalBody>
      <ModalFooter>
        <HStack>
          <Button onClick={onClearAction}>Cancel</Button>
          <Button
            colorScheme="green"
            onClick={() => {
              updateNode({ nodeId, updatedData: json });
            }}
          >
            Save
          </Button>
        </HStack>
      </ModalFooter>
    </Modal>
  );
};
