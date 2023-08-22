import { CloseIcon } from "@chakra-ui/icons";
import { IconButton } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";

export const RemoveResultButton = ({ index }: { index: number }) => {
  const { onRemoveResult } = useQueryContext();
  return (
    <IconButton
      size="xs"
      variant="ghost"
      aria-label="Close"
      icon={<CloseIcon />}
      onClick={() => onRemoveResult(index)}
    />
  );
};
