import { Button, Flex, HStack, Spinner } from "@chakra-ui/react";
import { JsonEditor, ValidationError } from "jsoneditor-react";
import { omit, pick } from "lodash";
import React, { useState } from "react";
import { JSONErrors } from "../../../../components/jsonEditor/JSONErrors";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../../components/modal";
import { getCurrentDB } from "../../../../utils/arangoClient";
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
  const { nodeId, nodeData, immutableIds, isLoading, updateNode } =
    useUpdateNodeAction({
      selectedAction,
      graphName,
      onSuccess: onClearAction,
      onFailure: onClearAction
    });
  const mutableNodeData = omit(nodeData, immutableIds);
  const immutableNodeData = pick(nodeData, immutableIds);
  const [json, setJson] = useState(mutableNodeData);
  const [errors, setErrors] = useState<ValidationError[]>();

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
        <AttributesInfo attributes={immutableNodeData} />
          <JsonEditor
            value={mutableNodeData}
            onChange={value => {
              setJson(value);
            }}
            allowedModes={['tree', 'code']}
            mode={"code"}
            history={true}
            onValidationError={errors => {
              setErrors(errors);
            }}
          />
          <JSONErrors errors={errors} />
      </ModalBody>
      <ModalFooter>
        <HStack>
          <Button onClick={onClearAction}>Cancel</Button>
          <Button
            isDisabled={!!(errors?.length && errors.length > 0)}
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
