import { Icon, IconButton, Tooltip } from "@chakra-ui/react";
import React from "react";
import { CloudDownload } from "styled-icons/material";
import { useGraph } from "../GraphContext";

export const LoadFullGraphButton = () => {
  const { setSelectedAction } = useGraph();
  return (
    <Tooltip
      hasArrow
      label={"Load full graph - use with caution"}
      placement="bottom"
    >
      <IconButton
        size="sm"
        onClick={() => {
          setSelectedAction({ action: "loadFullGraph" });
        }}
        icon={<Icon width="5" height="5" as={CloudDownload} />}
        aria-label={"Load full graph"} />
    </Tooltip>
  );
};
