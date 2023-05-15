import { MenuDivider, MenuItem, MenuList, MenuOptionGroup } from "@chakra-ui/react";
import React, { forwardRef, LegacyRef } from "react";
import { useGraph } from "../GraphContext";

export const EdgeRightClickMenu = forwardRef(
  (_props, ref: LegacyRef<HTMLDivElement>) => {
    const { setSelectedAction, rightClickedEntity } = useGraph();
    if (!rightClickedEntity) {
      return null;
    }
    return (
      <MenuList ref={ref}>
        <MenuOptionGroup title={`Edge: ${rightClickedEntity.edgeId}` || ""}>
          <MenuItem
            onClick={() =>
              setSelectedAction({
                action: "edit",
                entity: rightClickedEntity
              })
            }
          >
            Edit Edge
          </MenuItem>
          <MenuDivider />
          <MenuItem
            onClick={() =>
              setSelectedAction({
                action: "delete",
                entity: rightClickedEntity
              })
            }
          >
            Delete Edge
          </MenuItem>
        </MenuOptionGroup>
      </MenuList>
    );
  }
);
