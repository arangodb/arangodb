import { useEffect, useState } from "react";
import { getCurrentDB } from "../../../../utils/arangoClient";

const immutableIds = ["_id", "_key", "_rev"];

export const useNodeData = ({
  nodeId,
  disabled
}: {
  nodeId?: string;
  disabled?: boolean;
}) => {
  const [nodeData, setNodeData] = useState<{ [key: string]: string }>();
  const [isLoading, setIsLoading] = useState(false);

  const fetchNodeData = async (nodeId: string) => {
    const slashPos = nodeId.indexOf("/");
    const collection = nodeId.substring(0, slashPos);
    const vertex = nodeId.substring(slashPos + 1);
    const db = getCurrentDB();
    const dbCollection = db.collection(collection);
    try {
      setIsLoading(true);
      const firstDoc = await dbCollection.document(vertex);
      setNodeData(firstDoc);
      setIsLoading(false);
    } catch (error) {
      setIsLoading(false);
      window.window.arangoHelper.arangoError(
        "Graph",
        "Could not look up this node."
      );
      console.log(error);
    }
  };
  useEffect(() => {
    nodeId && !disabled && fetchNodeData(nodeId);
  }, [nodeId, disabled]);
  return { nodeData, immutableIds, isLoading };
};
