import { InfoIcon } from "@chakra-ui/icons";
import { Box, Tooltip } from "@chakra-ui/react";
import React from "react";

export const InfoTooltip = ({ label }: { label: string }) => {
  return (
    <Tooltip hasArrow label={label} placement="top">
      <Box alignSelf="start">
        <Box
          position="relative"
          top="1"
          padding="2"
          display="flex"
          justifyContent="center"
        >
          <InfoIcon />
        </Box>
      </Box>
    </Tooltip>
  );
};
