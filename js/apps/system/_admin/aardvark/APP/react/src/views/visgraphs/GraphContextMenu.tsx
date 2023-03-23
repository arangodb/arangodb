import { MenuItem, MenuList, MenuOptionGroup } from "@chakra-ui/react";
import React, { MutableRefObject, useEffect, useState } from "react";
import { IdType } from "vis-network";
import { ContextMenu } from "../../components/contextMenu/ContextMenu";
import { useGraph } from "./GraphContext";

export const GraphContextMenu = ({
  visJsRef
}: {
  visJsRef: MutableRefObject<HTMLElement | null>;
}) => {
  const [position, setPosition] = useState({ left: "0", top: "0" });
  const [menuData, setMenuData] = useState<
    | {
        type: string;
        nodeId?: IdType;
        edgeId?: IdType;
      }
    | undefined
  >();
  const { network } = useGraph();
  useEffect(() => {
    if (!network) {
      return;
    }
    network.on("oncontext", args => {
      args.event.preventDefault();
      const rect =
        visJsRef &&
        visJsRef.current &&
        visJsRef.current.getBoundingClientRect();
      const { left, top } = rect || {};
      const { x, y } = args.pointer.DOM;

      const finalX = x + left;
      const finalY = y + top;
      setPosition({
        left: `${finalX}px`,
        top: `${finalY}px`
      });
      const nodeId = network.getNodeAt(args.pointer.DOM);
      const edgeId = network.getEdgeAt(args.pointer.DOM);
      if (nodeId) {
        setMenuData({ type: "node", nodeId });
        network.selectNodes([nodeId]);
      } else if (edgeId) {
        setMenuData({ type: "edge", edgeId });
        network.selectEdges([edgeId]);
      } else {
        setMenuData({ type: "canvas" });
      }
    });
  }, [network, visJsRef]);
  const renderMenu = (ref: MutableRefObject<HTMLDivElement>) => {
    if (menuData && menuData.type === "canvas") {
      return (
        <MenuList ref={ref}>
          <MenuItem>Add node to database</MenuItem>
          <MenuItem>Add edge to database</MenuItem>
        </MenuList>
      );
    }
    if (menuData && menuData.type === "edge") {
      return (
        <MenuOptionGroup title={(menuData && `Edge: ${menuData.edgeId}`) || ""}>
          <MenuList ref={ref}>
            <MenuItem>Delete Edge</MenuItem>
            <MenuItem>Edit Edge</MenuItem>
          </MenuList>
        </MenuOptionGroup>
      );
    }
    if (!menuData) {
      return null;
    }
    return (
      <MenuList ref={ref}>
        <MenuOptionGroup title={(menuData && `Node: ${menuData.nodeId}`) || ""}>
          <MenuItem>Delete Node</MenuItem>
          <MenuItem>Edit Node</MenuItem>
          <MenuItem>Expand Node</MenuItem>
          <MenuItem>Set as Start Node</MenuItem>
        </MenuOptionGroup>
      </MenuList>
    );
  };
  return (
    <ContextMenu
      position={position}
      onClose={() => {
        setMenuData(undefined);
      }}
      isOpen={menuData && menuData.type ? true : false}
      renderMenu={renderMenu}
    />
  );
};
