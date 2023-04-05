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
import { useEdgeData } from "./useEdgeData";

const useUpdateEdgeAction = ({
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
  const { edgeId } = selectedAction?.entity || {};
  const { edgeData, isLoading, immutableIds } = useEdgeData({ edgeId });
  const udpateEdge = async ({
    edgeId,
    updatedData
  }: {
    edgeId: string;
    updatedData: { [key: string]: string };
  }) => {
    const slashPos = edgeId.indexOf("/");
    const collection = edgeId.substring(0, slashPos);
    const db = getCurrentDB();
    const graphCollection = db.graph(graphName).edgeCollection(collection);
    try {
      await graphCollection.update(edgeId, updatedData);
      window.arangoHelper.arangoNotification(
        `The edge ${edgeId} was successfully updated`
      );
      onSuccess();
    } catch (error) {
      window.arangoHelper.arangoError("Graph", "Could not update this edge.");
      onFailure();
    }
  };
  return { edgeId, edgeData, immutableIds, isLoading, udpateEdge };
};

export const EditEdgeModal = () => {
  const { graphName, selectedAction, onClearAction } = useGraph();
  const {
    edgeId,
    edgeData,
    immutableIds,
    isLoading,
    udpateEdge
  } = useUpdateEdgeAction({
    selectedAction,
    graphName,
    onSuccess: onClearAction,
    onFailure: onClearAction
  });
  const mutableEdgeData = omit(edgeData, immutableIds);
  const [json, setJson] = useState(mutableEdgeData);

  if (!edgeId) {
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
      <ModalHeader>Edit Edge: {edgeId}</ModalHeader>
      <ModalBody>
        <Stack spacing="4">
          <AttributesInfo attributes={edgeData} />
          <JsonEditor
            value={mutableEdgeData}
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
              udpateEdge({ edgeId, updatedData: json });
            }}
          >
            Save
          </Button>
        </HStack>
      </ModalFooter>
    </Modal>
  );
};
