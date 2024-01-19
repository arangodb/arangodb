import { PortalProps } from "@chakra-ui/react";
import React, { MutableRefObject } from "react";
import { ContextMenu } from "../../../../components/contextMenu/ContextMenu";
import { CanvasRightClickMenu } from "./CanvasRightClickMenu";
import { EdgeRightClickMenu } from "./EdgeRightClickMenu";
import { useGraph } from "../GraphContext";
import { NodeRightClickMenu } from "./NodeRightClickMenu";

export const GraphRightClickMenu = ({
  portalProps
}: {
  portalProps?: Omit<PortalProps, "children">;
}) => {
  const { rightClickedEntity, setRightClickedEntity } = useGraph();
  const { x, y } = rightClickedEntity?.pointer.DOM || { x: 0, y: 0 };
  const position = { left: `${x}px`, top: `${y}px` };
  const renderMenu = (ref: MutableRefObject<HTMLDivElement>) => {
    if (rightClickedEntity && rightClickedEntity.type === "canvas") {
      return <CanvasRightClickMenu ref={ref} />;
    }
    if (rightClickedEntity && rightClickedEntity.type === "edge") {
      return <EdgeRightClickMenu ref={ref} />;
    }
    if (rightClickedEntity && rightClickedEntity.type === "node") {
      return <NodeRightClickMenu ref={ref} />;
    }
    return null;
  };
  return (
    <ContextMenu
      portalProps={portalProps}
      position={position}
      onClose={() => {
        setRightClickedEntity(undefined);
      }}
      isOpen={rightClickedEntity && rightClickedEntity.type ? true : false}
      renderMenu={renderMenu}
    />
  );
};
