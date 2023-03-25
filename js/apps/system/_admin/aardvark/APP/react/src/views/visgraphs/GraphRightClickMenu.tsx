import { MenuItem, MenuList, MenuOptionGroup } from "@chakra-ui/react";
import React, {
  forwardRef,
  LegacyRef,
  MutableRefObject,
  useContext,
  useEffect,
  useState
} from "react";
import { ContextMenu } from "../../components/contextMenu/ContextMenu";
import { fetchVisData, useGraph } from "./GraphContext";
import { UrlParametersContext } from "./url-parameters-context";

export const GraphRightClickMenu = ({
  visJsRef
}: {
  visJsRef: MutableRefObject<HTMLElement | null>;
}) => {
  const [position, setPosition] = useState({ left: "0", top: "0" });
  const { network, selectedEntity, setSelectedEntity } = useGraph();
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
      const nodeId = network.getNodeAt(args.pointer.DOM) as string;
      const edgeId = network.getEdgeAt(args.pointer.DOM) as string;
      if (nodeId) {
        setSelectedEntity({ type: "node", nodeId });
        network.selectNodes([nodeId]);
      } else if (edgeId) {
        setSelectedEntity({ type: "edge", edgeId });
        network.selectEdges([edgeId]);
      } else {
        setSelectedEntity({ type: "canvas" });
      }
    });
  }, [network, visJsRef, setSelectedEntity]);
  const renderMenu = (ref: MutableRefObject<HTMLDivElement>) => {
    if (selectedEntity && selectedEntity.type === "canvas") {
      return <CanvasContextMenu ref={ref} />;
    }
    if (selectedEntity && selectedEntity.type === "edge") {
      return <EdgeContextMenu ref={ref} />;
    }
    if (selectedEntity && selectedEntity.type === "node") {
      return <NodeContextMenu ref={ref} />;
    }
    return null;
  };
  return (
    <ContextMenu
      position={position}
      onClose={() => {
        setSelectedEntity(undefined);
      }}
      isOpen={selectedEntity && selectedEntity.type ? true : false}
      renderMenu={renderMenu}
    />
  );
};

const EdgeContextMenu = forwardRef((_props, ref: LegacyRef<HTMLDivElement>) => {
  const { setSelectedAction, selectedEntity } = useGraph();
  if (!selectedEntity) {
    return null;
  }
  return (
    <MenuList ref={ref}>
      <MenuOptionGroup title={`Edge: ${selectedEntity.edgeId}` || ""}>
        <MenuItem
          onClick={() =>
            setSelectedAction({
              action: "delete",
              entity: selectedEntity
            })
          }
        >
          Delete Edge
        </MenuItem>
        <MenuItem
          onClick={() =>
            setSelectedAction({
              action: "edit",
              entity: selectedEntity
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
      selectedEntity,
      setSelectedAction,
      onApplySettings,
      graphName,
      datasets
    } = useGraph();
    const [urlParameters] = useContext(UrlParametersContext) || [];
    if (!selectedEntity || !selectedEntity.nodeId) {
      return null;
    }
    return (
      <MenuList ref={ref}>
        <MenuOptionGroup title={`Node: ${selectedEntity.nodeId}`}>
          <MenuItem
            onClick={() => {
              setSelectedAction({
                action: "delete",
                entity: selectedEntity
              });
            }}
          >
            Delete Node
          </MenuItem>
          <MenuItem
            onClick={() =>
              setSelectedAction({
                action: "edit",
                entity: selectedEntity
              })
            }
          >
            Edit Node
          </MenuItem>
          <MenuItem
            onClick={async () => {
              if (!selectedEntity.nodeId) {
                return;
              }
              const params = {
                ...(urlParameters as any),
                query:
                  "FOR v, e, p IN 1..1 ANY '" +
                  selectedEntity.nodeId +
                  "' GRAPH '" +
                  graphName +
                  "' RETURN p"
              };
              const newData = await fetchVisData({
                graphName,
                params
              });
              const foundNode = datasets?.nodes.get(selectedEntity.nodeId);
              const newLabel = foundNode?.label
                ? `${foundNode.label} (expanded)`
                : `(expanded)`;
              datasets?.nodes.updateOnly({
                id: selectedEntity.nodeId,
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
              if (!selectedEntity.nodeId) {
                return;
              }
              onApplySettings({ startNode: selectedEntity.nodeId });
            }}
          >
            Set as Start Node
          </MenuItem>
        </MenuOptionGroup>
      </MenuList>
    );
  }
);

const CanvasContextMenu = forwardRef(
  (_props, ref: React.LegacyRef<HTMLDivElement>) => {
    const { setSelectedAction, selectedEntity } = useGraph();
    if (!selectedEntity) {
      return null;
    }
    return (
      <MenuList ref={ref}>
        <MenuItem
          onClick={() => {
            setSelectedAction({
              action: "add",
              entityType: "node",
              entity: selectedEntity
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
              entity: selectedEntity
            });
          }}
        >
          Add edge to database
        </MenuItem>
      </MenuList>
    );
  }
);
