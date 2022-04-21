import React from 'react';
import G6 from '@antv/g6';
import ReactDOM from 'react-dom';
import { Card } from 'antd';
import { data2 } from './data2';
import NodeStyleSelector from './NodeStyleSelector.js';
import EdgeStyleSelector from './EdgeStyleSelector.js';
import AddCollectionNameToNodesSelector from './AddCollectionNameToNodesSelector';
import AddCollectionNameToEdgesSelector from './AddCollectionNameToEdgesSelector'
import { Headerinfo } from './Headerinfo';
import LayoutSelector from './LayoutSelector.js';
import styles from './graphview.module.css';
import menustyles from './graphview.menu.css';
import {
  TagFilled,
  DeleteFilled,
  ExpandAltOutlined,
  TrademarkCircleFilled,
  ChromeFilled,
  BranchesOutlined,
  ApartmentOutlined,
  AppstoreFilled,
  CopyrightCircleFilled,
  CustomerServiceFilled,
  ShareAltOutlined,
  EditOutlined,
  QuestionCircleOutlined
} from '@ant-design/icons';

export class GraphView extends React.Component {

  constructor(props) {
    super(props)
    this.ref = React.createRef();
  }

  componentDidMount() {
    const contextMenu = new G6.Menu({
      getContent(evt) {
        let header = '';
        let menu = '';
        if (evt.target && evt.target.isCanvas && evt.target.isCanvas()) {
          header = 'Canvas ContextMenu';
          menu = `<ul id='graphViewerMenu'>
            <li title='addNodeToDb'>Add node</li>
          </ul>`;
        } else if (evt.item) {
          const itemType = evt.item.getType();
          console.log("itemType: ", itemType);
          if(itemType === 'node') {
            header = `${itemType.toUpperCase()} ContextMenu`;
            menu = `<ul id='graphViewerMenu'>
              <li title='deleteNode'>Delete node</li>
              <li title='editNode'>Edit node</li>
              <li title='expandNode'>Expand node</li>
              <li title='setAsStartnode'>Set as startnode</li>
            </ul>`;
          }
          if(itemType === 'edge') {
            header = `${itemType.toUpperCase()} ContextMenu`;
            menu = `<ul id='graphViewerMenu'>
              <li title='deleteEdge'>Delete edge</li>
              <li title='editEdge'>Edit edge</li>
            </ul>`;
          }
        }
        return `${menu}`;
      },
      handleMenuClick: (target, item) => {
        console.log(target, item);
        let chosenAction = target.getAttribute('title');
        console.log("chosenAction: ", chosenAction);
        if(chosenAction === 'deleteNode') {
          console.log("Trigger deleteNode() with node: ", item._cfg);
          console.log("Trigger deleteNode() with nodeId: ", item._cfg.id);
          this.removeNode(item._cfg.id);
        } else if(chosenAction === 'editNode') {
          console.log("Trigger editNode() with node: ", item._cfg);
          console.log("Trigger editNode() with nodeId: ", item._cfg.id);
          this.props.onEditNode(item._cfg.id);
        } else if(chosenAction === 'expandNode') {
          console.log("Trigger expandNode() with node: ", item._cfg);
          console.log("Trigger expandNode() with nodeId: ", item._cfg.id);
          this.props.onExpandNode(item._cfg.id);
        } else if(chosenAction === 'setAsStartnode') {
          console.log("Trigger setAsStartnode() with node: ", item._cfg);
          console.log("Trigger setAsStartnode() with nodeId: ", item._cfg.id);
          this.props.onSetStartnode(item._cfg.id);
        } else if(chosenAction === 'addNode') {
          console.log("Trigger addNode()");
          this.addNode();
        }  else if(chosenAction === 'addNodeToDb') {
          console.log("Trigger addNodeToDb()");
          this.props.onAddNodeToDb();
        } else if(chosenAction === 'deleteEdge') {
          console.log("Trigger deleteEdge() with edge: ", item._cfg);
          console.log("Trigger deleteEdge() with edgeId: ", item._cfg.id);
          this.removeEdge(item._cfg.id);
        } else if(chosenAction === 'editEdge') {
          console.log("Trigger editEdge() with edge: ", item._cfg);
          console.log("Trigger editEdge() with edgeId: ", item._cfg.id);
          this.props.onEditEdge(item._cfg.id);
        } 
      },
      offsetX: 16 + 10,
      offsetY: 0,
      itemTypes: ['node', 'edge', 'canvas'],
    });
    
    const toolbar = new G6.ToolBar({
      position: { x: 10, y: 10 },
    });

    /*
    const tc = document.createElement('div');
    tc.id = 'toolbarContainer';
    document.body.appendChild(tc);
    
    const toolbar = new G6.ToolBar({
      //container: tc,
      position: { x: 10, y: 10 },
      getContent: () => {
        return `
          <ul id="toolbar-design">
            <li code='redo'>Redo</li>
            <li code='undo'>Undo</li>
            <li code='zoomin'>Zoom In</li>
            <li code='zoomout'>Zoom Out</li>
            <li code='dontknow'>Don't know</li>
            <li code='fit'>Fit</li>
            <li code='add'>Add</li>
          </ul>
        `
      },
      handleClick: (code, graph) => {
        if (code === 'add') {
          graph.addItem('node', {
            id: 'node2',
            label: 'node2',
            x: 300,
            y: 150
          })
        } else if (code === 'redo') {
          toolbar.redo();
        } else if (code === 'undo') {
          toolbar.undo();
        } else if (code === 'zoomin') {
          console.log("zoom in");
          toolbar.zoomIn();
        } else if (code === 'zoomout') {
          console.log("zoom out");
          toolbar.zoomOut();
        } else if (code === 'dontknow') {
          console.log("Do not know");
        } else if (code === 'fit') {
          console.log("fit");
        }
      }
    });
    */

    const container = ReactDOM.findDOMNode(this.ref.current);
    console.log(`Size: ${container.offsetWidth} x ${container.offsetHeight}`);
    this.graph = new G6.Graph({
      container: this.ref.current,
      //width: container.offsetWidth,
      width: 1200,
      //height: container.offsetHeight,
      height: 400,
      plugins: [toolbar, contextMenu],
      enabledStack: true,
      layout: {
        type: 'gForce',
        //minMovement: 0.01,
        //maxIteration: 100,
        preventOverlap: true,
        //damping: 0.99,
        fitView: true,
        linkDistance: 100
      },
      modes: {
        default: [
          'drag-canvas',
          'zoom-canvas',
          'drag-node',
          'activate-relations',
          {
            type: 'tooltip', // Tooltip
            formatText(model) {
              console.log("model: ", model);
              // The content of tooltip
              //const text = 'Label: ' + model.label + ((model.population !== undefined) ? ('<br />population: ' + model.population) : ('<br/> population: No information '));
              const text = '<strong>Label:</strong> ' + model.label;
              return text;
            },
          },
          {
            type: 'edge-tooltip', // Edge tooltip
            formatText(model) {
              // The content of the edge tooltip
              const text =
                '<strong>id:</strong> ' +
                model.id +
                '<br /><strong>source:</strong> ' +
                model.source +
                '<br/><strong>target:</strong> ' +
                model.target;
              return text;
            },
          },
          {
            type: 'create-edge',
            key: 'shift', // undefined by default, options: 'shift', 'control', 'ctrl', 'meta', 'alt'
          },
        ],
      },
      defaultNode: {
        type: 'circle', // 'bubble'
        size: 40,
        style: {
          fill: '#dee072', // The filling color of nodes
          stroke: '#576e3e', // The stroke color of nodes
          lineWidth: 1, // The line width of the stroke of nodes
          cursor: 'pointer',
        },
        labelCfg: {
          position: 'center',
          style: {
            fill: '#576e3e',
            fontStyle: 'regular',
            //fontFamily: 'sans-serif',
            fontFamily: 'Roboto',
            fontSize: 12
          },
        },
      },
      nodeStateStyles: {
        hover: {
          fill: '#edf042',
          //fillOpacity: 0.1,
          lineWidth: 1,
          shadowColor: '#576e3e',
          shadowBlur: 10,
          cursor: 'pointer',
        },
      },
      defaultEdge: {
        type: 'quadratic', // assign the edges to be quadratic bezier curves
        labelCfg: {
          autoRotate: true,
        },
        style: {
          stroke: '#e2e2e2',
          cursor: 'pointer',
        },
      },
      edgeStateStyles: {
        hover: {
          fillOpacity: 0.1,
          lineWidth: 2,
          cursor: 'pointer',
        },
      },
    });

    console.log("Render this data: ", this.props.data);
    console.log("Render this data.nodes: ", this.props.data.nodes);
    this.graph.data(this.props.data);
    this.graph.render();

    this.graph.on('node:click', (e) => {
      this.props.onClickDocument(e);
    });

    this.graph.on('edge:click', (e) => {
      this.props.onClickDocument(e);
    });

    this.graph.on('node:mouseenter', (evt) => {
      const node = evt.item;
      this.graph.setItemState(node, 'hover', true);
    });

    this.graph.on('node:mouseleave', (evt) => {
      const node = evt.item;
      this.graph.setItemState(node, 'hover', false);
    });

    this.graph.on('edge:mouseenter', (evt) => {
      const edge = evt.item;
      this.graph.setItemState(edge, 'hover', true);
    });

    this.graph.on('edge:mouseleave', (evt) => {
      const edge = evt.item;
      this.graph.setItemState(edge, 'hover', false);
    });

    this.graph.on('aftercreateedge', (e) => {
      console.log("Newly added edge (e): ", e);
      console.log("Newly added edge (e.edge._cfg.model.source): ", e.edge._cfg.model.source);
      console.log("Newly added edge (e.edge._cfg.model.target): ", e.edge._cfg.model.target);
      const id = (Math.random() + 1).toString(36).substring(7);
      const edgeModel = {
        id: id,
        source: e.edge._cfg.model.source,
        target: e.edge._cfg.model.target
      };
      
      this.graph.addItem('edge', edgeModel);
      this.props.onAddSingleEdge(edgeModel);

      const edges = this.graph.save().edges;
      /*
      G6.Util.processParallelEdges(edges);
      this.graph.getEdges().forEach((edge, i) => {
        this.graph.updateItem(edge, {
          curveOffset: edges[i].curveOffset,
          curvePosition: edges[i].curvePosition,
        });
      });
      */
    });
  }

  componentDidUpdate() {
    const container = ReactDOM.findDOMNode(this.ref.current);
    this.graph.changeSize(container.offsetWidth, container.offsetHeight);
    this.graph.data(this.props.data);
    this.graph.render();
  }

  getNodes = () => {
    const nodes = this.graph.getNodes();
    console.log("getNodes(): ", nodes);
  }

  getEdges = () => {
    const edges = this.graph.getEdges();
    console.log("getEdges(): ", edges);
  }

  addNode = () => {
    const min = 0;
    const maxX = 1000;
    const maxY = 400;
    const id = (Math.random() + 1).toString(36).substring(7);
    const address = (Math.random() + 1).toString(8).substring(7);
    //const fillColor = `#${Math.floor(Math.random()*16777215).toString(16)}`;
    const fillColor = `#FFFFFF`;
    const nodeModel = {
      id: id,
      label: id,
      address: address,
      x: Math.floor(Math.random() * (Math.floor(maxX) - Math.ceil(min))) + Math.ceil(min),
      y: Math.floor(Math.random() * (Math.floor(maxY) - Math.ceil(min))) + Math.ceil(min),
      style: {
        fill: fillColor,
      },
    };
    
    this.graph.addItem('node', nodeModel);
    this.props.onAddSingleNode(nodeModel);
  }

  removeNode = (nodeId = '') => {
    const item = this.graph.findById(nodeId);
    this.props.onRemoveSingleNode(nodeId);
    this.graph.removeItem(item);
  }

  removeEdge = (edgeId = '') => {
    const item = this.graph.findById(edgeId);
    this.props.onRemoveSingleEdge(edgeId);
    this.graph.removeItem(item);
  }

  addEdge = () => {
    /*
    const node1 = null;
    const node2 = null;
    const min = 0;
    const max = this.graph.getNodes().length - 1;
    console.log('max: ', max);
    const randomNode1 = Math.floor(Math.random() * (max - min) + min);
    console.log('randomNode1: ', randomNode1);
    const randomNode2 = Math.floor(Math.random() * (max - min) + min);
    console.log('randomNode2: ', randomNode2);
    this.graph.findAll("node", (node) => {
      console.log('node.get("model").id: ', node.get("model").id);
      //tempNodes.push(node.get("model"));
      return node.get("model");
    });
    */
    const edgeModel = {
      id: 'alicetodiana',
      source: 'female/alice',
      target: 'female/diana',
      data: {
        type: 'name1',
        amount: '1000,00 元',
        date: '2022-01-13'
      }
    };
    
    this.graph.addItem('edge', edgeModel);
    this.props.onAddSingleEdge(edgeModel);
  }

  changeNodeStyle = (typeModel) => {
    this.graph.node((node) => {
      return {
        id: node.id,
        ...typeModel
      };
    });
    
    this.graph.data(this.props.data);
    this.graph.render();
  }

  changeEdgeStyle = (typeModel) => {
    this.graph.edge((edge) => {
      return {
        id: edge.id,
        ...typeModel
      };
    });
    
    this.graph.data(this.props.data);
    this.graph.render();
  }

  updateNodeGraphData = () => {
    const tempNodes = [];
    this.graph.findAll("node", (node) => {
      console.log('node.get("model"): ', node.get("model"));
      tempNodes.push(node.get("model"));
      return node.get("model");
    });
    console.log("updateNodeGraphData (tempNodes in child): ", tempNodes);
    this.props.onUpdateNodeGraphData(tempNodes);
  }

  updateEdgeGraphData = () => {
    const tempEdges = [];
    this.graph.findAll("edge", (edge) => {
      tempEdges.push(edge.get("model"));
      return edge.get("model");
    });
    this.props.onUpdateEdgeGraphData(tempEdges);
  }

  addCollectionNameToNodes = () => {
    console.log("addCollectionNameToNodes triggered");
    this.graph.node((node) => {
      console.log("NODE: ", node);
      return {
        id: node.id,
      };
    });
    
    /*
    this.graph.data(this.props.data);
    this.graph.render();
    */
  }

  /*
  updateNodeModel = () => {
    // Find the item instance by id
    const item = this.graph.findById('worldVertices/continent-south-america');
    console.log("item to update: ", item);
    console.log("item._cfg.id: ", item._cfg.id);
    const model = {
      id: '2',
      label: 'node2',
      population: '2,950,000',
      type: 'diamond',
      style: {
        fill: 'red',
      },
    };

    model = {
      label: item._cfg.id
    };

    this.graph.updateItem(item, model);
  }
  */

  updateNodeModel = () => {
    const nodes = this.graph.getNodes();
    nodes.forEach((node) => {
      console.log("node: ", node);
      const idSplit = node._cfg.id.split('/');

      const model = {
        label: `${idSplit[1]} (${idSplit[0]})`
      };
      this.graph.updateItem(node, model);

      /*
      if (!node.style) {
        node.style = {};
      }
      switch (
        node.class // Configure the graphics type of nodes according to their class
      ) {
        case 'c0': {
          node.type = 'circle'; // The graphics type is circle when class = 'c0'
          break;
        }
        case 'c1': {
          node.type = 'rect'; // The graphics type is rect when class = 'c1'
          node.size = [35, 20]; // The node size when class = 'c1'
          break;
        }
        case 'c2': {
          node.type = 'ellipse'; // The graphics type is ellipse when class = 'c2'
          node.size = [35, 20]; // The node size when class = 'c2'
          break;
        }
      }
      */
    });
  }

  updateEdgeModel = () => {
    const edges = this.graph.getEdges();
    edges.forEach((edge) => {
      const idSplit = edge._cfg.id.split('/');

      const model = {
        label: `${idSplit[1]} (${idSplit[0]})`
      };
      this.graph.updateItem(edge, model);
    });
  }

  addCollectionNameToNodes = (value) => {
    this.graph.node((node) => {
      const slashPos = node.id.indexOf("/");
      node.label = node.id.substring(slashPos + 1) + " - " + node.id.substring(0, slashPos);
      return {
        node
      };
    });
    
    this.graph.data(this.props.data);
    this.graph.render();
  }

  addCollectionNameToEdges = (value) => {
    this.graph.edge((edge) => {
      console.log("edge:" , edge);
      const slashPos = edge.id.indexOf("/");
      edge.label = edge.id.substring(slashPos + 1) + " - " + edge.id.substring(0, slashPos);
      return {
        edge
      };
    });
    
    this.graph.data(this.props.data);
    this.graph.render();
  }

  changeLayout = (value) => {
    this.graph.updateLayout({
      type: value,
    });
  }

  downloadScreenshot = () => {
    this.graph.downloadImage('graph', 'image/png', '#fff');
  }

  downloadFullScreenshot = () => {
    this.graph.downloadFullImage(
      'fullGraph',
      'image/png',
      {
        backgroundColor: '#fff',
        padding: [30, 15, 15, 15],
      }
    );
  }

  highlightDocument = (document) => {
    if(document) {
      this.graph.focusItem(document, true);
    }
  }

  /*
  <button onClick={this.changeLayout}>Change layout</button>
  <button onClick={this.addCollectionNameToNodes}>Add collection name (nodes)</button>
  <button onClick={this.getNodes}>Get nodes (new)</button>
  <button onClick={this.getEdges}>Get edges (new)</button>
  <button onClick={this.addNode}>Add node (new)</button>
  <button onClick={this.addEdge}>Add edge (new)</button>
  <button onClick={this.updateNodeGraphData}>Update node graph data (new)</button>
  <button onClick={this.updateEdgeGraphData}>Update edge graph data (new)</button>
  <button onClick={this.changeLayout}>Change layout</button>

  <LayoutSelector value={this.type} onChange={this.changeLayout} />
  <AddCollectionNameToNodesSelector onAddCollectionNameToNodesChange={(value) => this.addCollectionNameToNodes(value)} />
  <AddCollectionNameToEdgesSelector onAddCollectionNameToEdgesChange={(value) => this.addCollectionNameToEdges(value)} />
  
  <NodeStyleSelector onNodeStyleChange={(typeModel) => this.changeNodeStyle(typeModel)} />
  <EdgeStyleSelector onEdgeStyleChange={(typeModel) => this.changeEdgeStyle(typeModel)} />
  */

  colorNodesByCollection = (graphData) => {
    console.log("Color these nodes: ", graphData.nodes);
    /*
    data.nodes.forEach(node => {
      if (model.id === '0') {
        node.style = {
          fill: '#e18826',
          // ... other styles
        }
      } else {
        node.style = {
          fill: '#002a67',
          // ... other styles
        }
      }
    });
    */
  }

  printVertexCollections = () => {
    console.log("Print vertex collections in GraphView: ", this.props.vertexCollections);
  }

  render() {
    return <>
      <button onClick={this.updateNodeModel}>Show collection to nodes</button>
      <button onClick={this.updateEdgeModel}>Show collection to edges</button>
      <button onClick={() => this.printVertexCollections()}>Print vertex collections</button>
      <button onClick={() => {
        this.props.data.nodes.forEach(node => {
          if (node.id.startsWith("frenchCity")) {
            console.log("Color node: ", node);
            node.style = {
              fill: '#f00',
              // ... other styles
            }
          } else {
            node.style = {
              fill: '#002a67',
              // ... other styles
            }
          }
        });
      }}>
        Color nodes by collection
      </button>
      <button onClick={() => {
        const node = this.graph.findById('frenchCity/Paris');
        console.log("Found node: ", node);

        this.graph.moveTo(node.get('model').x, node.get('model').y, true, {
          duration: 100,
        });
      }}>Find node Paris</button>
      <Headerinfo
        graphName={this.props.graphName}
        graphData={this.props.data}
        responseDuration={this.props.responseDuration}
        onDownloadScreenshot={() => this.downloadScreenshot()}
        onDownloadFullScreenshot={() => this.downloadFullScreenshot()}
        onChangeLayout={(layout) => {
          console.log("Change layout to: ", layout);
          this.changeLayout(layout);
        }}
        onChangeGraphData={(newGraphData) => {
          console.log("newGraphData in GraphView: ", newGraphData);
          this.props.onChangeGraphData(newGraphData);
        }}
        onLoadFullGraph={() => this.props.onLoadFullGraph}
        onDocumentSelect={(document) => this.highlightDocument(document)}
        onGraphDataLoaded={(newGraphData) => this.props.onGraphDataLoaded(newGraphData)}
      />
      <Card
          title={this.props.graphName}
          id="graph-card"
        >
          <div ref={this.ref} className={styles.graphContainer}> </div>
      </Card>
    </>;
  }
}
