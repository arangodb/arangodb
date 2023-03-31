import {
  MenuItem,
  MenuList,
  MenuOptionGroup,
  PortalProps
} from "@chakra-ui/react";
import React, {
  forwardRef,
  LegacyRef,
  MutableRefObject,
  useEffect,
  useState
} from "react";
import { ContextMenu } from "../../components/contextMenu/ContextMenu";
import { fetchVisData, useGraph } from "./GraphContext";
import { useUrlParameterContext } from "./UrlParametersContext";

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
      return <CanvasContextMenu ref={ref} />;
    }
    if (rightClickedEntity && rightClickedEntity.type === "edge") {
      return <EdgeContextMenu ref={ref} />;
    }
    if (rightClickedEntity && rightClickedEntity.type === "node") {
      return <NodeContextMenu ref={ref} />;
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

const EdgeContextMenu = forwardRef((_props, ref: LegacyRef<HTMLDivElement>) => {
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
              action: "delete",
              entity: rightClickedEntity
            })
          }
        >
          Delete Edge
        </MenuItem>
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
      </MenuOptionGroup>
    </MenuList>
  );
});

const NodeContextMenu = forwardRef(
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

const CanvasContextMenu = forwardRef(
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
