import { Button } from "@chakra-ui/react";
import React from "react";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { useQueryContext } from "../QueryContextProvider";

export const CancelQueryButton = ({
  index,
  asyncJobId
}: {
  index: number;
  asyncJobId?: string;
}) => {
  const { onRemoveResult } = useQueryContext();
  const onCancel = async () => {
    const route = getApiRouteForCurrentDB();
    try {
      await route.request({
        method: "PUT",
        path: `/job/${asyncJobId}/cancel`
      });
      onRemoveResult(index);
      window.arangoHelper.arangoNotification("Query cancelled");
    } catch (e) {}
  };
  return (
    <Button
      marginLeft="auto"
      colorScheme="red"
      size="sm"
      aria-label="Close"
      onClick={onCancel}
    >
      Cancel
    </Button>
  );
};
