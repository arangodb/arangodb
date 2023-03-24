import { Button, Checkbox, HStack } from "@chakra-ui/react";
import { pick } from "lodash";
import React, { useEffect, useState } from "react";
import {
  Modal,
  ModalBody,
  ModalFooter,
  ModalHeader
} from "../../../components/modal";
import {
  getApiRouteForCurrentDB,
  getCurrentDB
} from "../../../utils/arangoClient";
import { SelectedActionType, useGraph } from "../GraphContext";
import { AttributesInfo } from "./AttributesInfo";

const fetchNodeData = ({
  nodeId,
  setNodeData
}: {
  nodeId: string;
  setNodeData: (data: { [key: string]: string }) => void;
}) => {
  const slashPos = nodeId.indexOf("/");
  const nodeDataObject = {
    keys: [nodeId.substring(slashPos + 1)],
    collection: nodeId.substring(0, slashPos)
  };
  getApiRouteForCurrentDB()
    .put("/simple/lookup-by-keys", nodeDataObject)
    .then(data => {
      const basicData = pick(data.body.documents[0], ["_id", "_key", "_rev"]);
      setNodeData(basicData);
    })
    .catch(err => {
      window.window.arangoHelper.arangoError(
        "Graph",
        "Could not look up this node."
      );
      console.log(err);
    });
};

const useNodeData = ({ nodeId }: { nodeId?: string }) => {
  const [nodeData, setNodeData] = useState<{ [key: string]: string }>();
  useEffect(() => {
    nodeId && fetchNodeData({ nodeId, setNodeData });
  }, [nodeId]);
  return nodeData;
};

const useDeleteNodeAction = ({
  selectedAction,
  deleteEdges,
  graphName
}: {
  selectedAction?: SelectedActionType;
  deleteEdges: boolean;
  graphName: string;
}) => {
  const { nodeId } = selectedAction?.entity || {};
  const nodeData = useNodeData({ nodeId });
  const deleteNode = async (nodeId: string) => {
    const slashPos = nodeId.indexOf("/");
    const collection = nodeId.substring(0, slashPos);
    const vertex = nodeId.substring(slashPos + 1);
    const data = {
      keys: [vertex],
      collection: collection
    };
    const db = getCurrentDB();
    if (deleteEdges) {
      const graphCollection = db.graph(graphName).vertexCollection(collection);
      try {
        await graphCollection.remove(vertex);
        window.arangoHelper.arangoNotification(
          `The node ${nodeId} and connected edges were successfully deleted`
        );
      } catch (e) {
        console.log("Error: ", e);
        window.arangoHelper.arangoError("Graph", "Could not delete node.");
        console.log("ERROR: Could not delete edge");
      }
    } else {
      const keys = data.keys;
      const dbCollection = db.collection(collection);
      try {
        await dbCollection.removeByKeys(keys);
        window.arangoHelper.arangoNotification(
          `The node ${nodeId} was successfully deleted`
        );
      } catch (e) {
        console.log("Error: ", e);
        window.arangoHelper.arangoError("Graph", "Could not delete node.");
        console.log("ERROR: Could not delete edge");
      }
    }
  };
  return { nodeId, nodeData, deleteNode };
};

export const DeleteNodeModal = () => {
  const { graphName, selectedAction, onCancelAction } = useGraph();
  const [deleteEdges, setDeleteEdges] = useState(true);
  const { nodeId, nodeData, deleteNode } = useDeleteNodeAction({
    selectedAction,
    deleteEdges,
    graphName
  });

  if (!nodeId) {
    return null;
  }

  return (
    <Modal isOpen onClose={onCancelAction}>
      <ModalHeader>Delete Node: {nodeId}?</ModalHeader>
      <ModalBody>
        <AttributesInfo attributes={nodeData} />
        <Checkbox
          isChecked={deleteEdges}
          onChange={() => {
            setDeleteEdges(deleteEdges => !deleteEdges);
          }}
        />
      </ModalBody>
      <ModalFooter>
        <HStack>
          <Button onClick={onCancelAction}>Cancel</Button>
          <Button
            colorScheme="red"
            onClick={() => {
              deleteNode(nodeId);
            }}
          >
            Delete
          </Button>
        </HStack>
      </ModalFooter>
    </Modal>
  );
};
