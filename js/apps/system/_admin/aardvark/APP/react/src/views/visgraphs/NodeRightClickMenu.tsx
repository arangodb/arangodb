import { MenuItem, MenuList, MenuOptionGroup } from "@chakra-ui/react";
import React, { forwardRef } from "react";
import { fetchVisData, useGraph } from "./GraphContext";
import { useUrlParameterContext } from "./UrlParametersContext";

export const NodeRightClickMenu = forwardRef(
  (_props, ref: React.LegacyRef<HTMLDivElement>) => {
    const {
      rightClickedEntity,
      setSelectedAction,
      onApplySettings,
      graphName,
      datasets
    } = useGraph();
    const { urlParams, setUrlParams } = useUrlParameterContext();
    const foundNode =
      rightClickedEntity?.nodeId &&
      datasets?.nodes.get(rightClickedEntity.nodeId);
    if (!rightClickedEntity || !rightClickedEntity.nodeId || !foundNode) {
      return null;
    }
    return (
      <MenuList ref={ref}>
        <MenuOptionGroup title={`Node: ${rightClickedEntity.nodeId}`}>
          <MenuItem
            onClick={() => {
              setSelectedAction({
                action: "delete",
                entity: rightClickedEntity
              });
            }}
          >
            Delete Node
          </MenuItem>
          <MenuItem
            onClick={() =>
              setSelectedAction({
                action: "edit",
                entity: rightClickedEntity
              })
            }
          >
            Edit Node
          </MenuItem>
          <MenuItem
            onClick={async () => {
              if (!rightClickedEntity.nodeId) {
                return;
              }
              const params = {
                ...urlParams,
                query:
                  "FOR v, e, p IN 1..1 ANY '" +
                  rightClickedEntity.nodeId +
                  "' GRAPH '" +
                  graphName +
                  "' RETURN p"
              };
              const newData = await fetchVisData({
                graphName,
                params
              });
              const newLabel = foundNode?.label
                ? `${foundNode.label} (expanded)`
                : `(expanded)`;
              datasets?.nodes.updateOnly({
                id: rightClickedEntity.nodeId,
                label: newLabel
              });
              const newNodes = newData.nodes.filter((node: any) => {
                return !datasets?.nodes.get(node.id);
              });
              const newEdges = newData.edges.filter((edge: any) => {
                return !datasets?.edges.get(edge.id);
              });
              datasets?.nodes.add(newNodes);
              datasets?.edges.add(newEdges);
            }}
          >
            Expand Node
          </MenuItem>
          <MenuItem
            onClick={() => {
              if (!rightClickedEntity.nodeId) {
                return;
              }
              setUrlParams({
                ...urlParams,
                nodeStart: rightClickedEntity.nodeId
              });
              onApplySettings({ nodeStart: rightClickedEntity.nodeId });
            }}
          >
            Set as Start Node
          </MenuItem>
          <MenuItem
            onClick={() => {
              if (!rightClickedEntity.nodeId) {
                return;
              }
              if (foundNode.fixed) {
                const newLabel =
                  foundNode.label && foundNode.label.includes(" (fixed)")
                    ? foundNode.label.replace(" (fixed)", "")
                    : foundNode.label;
                datasets?.nodes.updateOnly({
                  id: rightClickedEntity.nodeId,
                  label: newLabel,
                  fixed: false
                });
                return;
              }
              const newLabel = foundNode?.label
                ? `${foundNode.label} (fixed)`
                : `(fixed)`;
              datasets?.nodes.updateOnly({
                id: rightClickedEntity.nodeId,
                label: newLabel,
                fixed: true
              });
            }}
          >
            {foundNode?.fixed ? "Unpin node" : "Pin node"}
          </MenuItem>
        </MenuOptionGroup>
      </MenuList>
    );
  }
);
