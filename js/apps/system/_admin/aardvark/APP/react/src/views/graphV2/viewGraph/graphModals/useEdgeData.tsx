import { useEffect, useState } from "react";
import { getCurrentDB } from "../../../../utils/arangoClient";

const immutableIds = ["_id", "_key", "_rev", "_from", "_to"];

export const useEdgeData = ({
  edgeId,
  disabled
}: {
  edgeId?: string;
  disabled?: boolean;
}) => {
  const [edgeData, setEdgeData] = useState<{ [key: string]: string }>();
  const [isLoading, setIsLoading] = useState(false);
  const fetchEdgeData = async (edgeId: string) => {
    const slashPos = edgeId.indexOf("/");
    const collectionName = edgeId.substring(0, slashPos);
    const vertex = edgeId.substring(slashPos + 1);
    const collection = getCurrentDB().collection(collectionName);
    try {
      setIsLoading(true);
      const firstDoc = await collection.document(vertex);
      setEdgeData(firstDoc);
      setIsLoading(false);
    } catch (err) {
      setIsLoading(false);
      window.arangoHelper.arangoError("Graph", "Could not look up this edge.");
      console.log(err);
    }
  };
  useEffect(() => {
    edgeId && !disabled && fetchEdgeData(edgeId);
  }, [edgeId, disabled]);

  return { edgeData, immutableIds, isLoading };
};
