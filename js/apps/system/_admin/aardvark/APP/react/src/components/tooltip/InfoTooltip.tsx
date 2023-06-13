import { InfoIcon } from "@chakra-ui/icons";
import { Box, BoxProps, Tooltip } from "@chakra-ui/react";
import React from "react";

export const InfoTooltip = ({
  label,
  boxProps
}: {
  label: string;
  boxProps?: BoxProps;
}) => {
  return (
    <Tooltip hasArrow label={label} placement="top">
      <Box
        position="relative"
        padding="2"
        display="flex"
        justifyContent="center"
        {...boxProps}
      >
        <InfoIcon />
      </Box>
    </Tooltip>
  );
};
