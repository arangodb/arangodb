import { MenuItem, MenuList } from "@chakra-ui/react";
import React, { forwardRef } from "react";
import { useGraph } from "../GraphContext";

export const CanvasRightClickMenu = forwardRef(
  (_props, ref: React.LegacyRef<HTMLDivElement>) => {
    const { setSelectedAction, rightClickedEntity } = useGraph();
    if (!rightClickedEntity) {
      return null;
    }
    return (
      <MenuList ref={ref}>
        <MenuItem
          onClick={() => {
            setSelectedAction({
              action: "add",
              entityType: "node",
              entity: rightClickedEntity
            });
          }}
        >
          Add node to database
        </MenuItem>
        <MenuItem
          onClick={() => {
            setSelectedAction({
              action: "add",
              entityType: "edge",
              entity: rightClickedEntity
            });
          }}
        >
          Add edge to database
        </MenuItem>
      </MenuList>
    );
  }
);
