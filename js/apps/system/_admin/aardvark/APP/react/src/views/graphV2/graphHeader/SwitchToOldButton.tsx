import { RepeatClockIcon } from "@chakra-ui/icons";
import { IconButton, Tooltip } from "@chakra-ui/react";
import React from "react";
import { useGraph } from "../GraphContext";

export const SwitchToOldButton = () => {
  const { graphName } = useGraph();
  return (
    <Tooltip
      hasArrow
      label={"Switch to the old graph viewer"}
      placement="bottom"
    >
      <IconButton
        size="sm"
        onClick={() => {
          window.App.navigate(`#graph/${graphName}`, { trigger: true });
        }}
        icon={<RepeatClockIcon />}
        aria-label={"Switch to the old graph viewer"} />
    </Tooltip>
  );
};
