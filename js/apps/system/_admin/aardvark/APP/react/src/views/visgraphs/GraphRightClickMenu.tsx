import { PortalProps } from "@chakra-ui/react";
import React, { MutableRefObject, useEffect, useState } from "react";
import { ContextMenu } from "../../components/contextMenu/ContextMenu";
import { CanvasRightClickMenu } from "./CanvasRightClickMenu";
import { EdgeRightClickMenu } from "./EdgeRightClickMenu";
import { useGraph } from "./GraphContext";
import { NodeRightClickMenu } from "./NodeRightClickMenu";

export const GraphRightClickMenu = ({
  portalProps
}: {
  portalProps?: Omit<PortalProps, "children">;
}) => {
  const [position, setPosition] = useState({ left: "0", top: "0" });
  const { network, rightClickedEntity, setRightClickedEntity } = useGraph();
  useEffect(() => {
    if (!network) {
      return;
    }
    network.on("oncontext", ({ event, pointer }) => {
      event.preventDefault();
      const { x, y } = pointer.DOM;
      setPosition({
        left: `${x}px`,
        top: `${y}px`
      });
      const nodeId = network.getNodeAt(pointer.DOM) as string;
      const edgeId = network.getEdgeAt(pointer.DOM) as string;
      if (nodeId) {
        setRightClickedEntity({ type: "node", nodeId, pointer });
        network.selectNodes([nodeId]);
      } else if (edgeId) {
        setRightClickedEntity({ type: "edge", edgeId, pointer });
        network.selectEdges([edgeId]);
      } else {
        setRightClickedEntity({ type: "canvas", pointer });
      }
    });
  }, [network, setRightClickedEntity]);
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
