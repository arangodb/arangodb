import { Icon, IconButton, Tooltip } from "@chakra-ui/react";
import React from "react";
import { Fullscreen } from "styled-icons/material";

const enterFullscreen = (element: any) => {
  if (element.requestFullscreen) {
    element.requestFullscreen();
  } else if (element.webkitRequestFullscreen) {
    /* Safari */
    element.webkitRequestFullscreen();
  } else if (element.msRequestFullscreen) {
    /* IE11 */
    element.msRequestFullscreen();
  }
};
export const FullscreenGraphButton = () => {
  return (
    <Tooltip hasArrow label={"Enter fullscreen"} placement="bottom">
      <IconButton
        size="sm"
        onClick={() => {
          const elem = document.documentElement;
          enterFullscreen(elem);
        }}
        icon={<Icon width="5" height="5" as={Fullscreen} />}
        aria-label={"Enter fullscreen"} />
    </Tooltip>
  );
};
