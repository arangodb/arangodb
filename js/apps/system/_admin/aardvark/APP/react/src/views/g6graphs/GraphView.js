import React from 'react';
import G6 from '@antv/g6';
import ReactDOM from 'react-dom';
import { Headerinfo } from './Headerinfo';
import styles from './graphview.module.css';
import './graphview.menu.css';
import './visgraphstyles.css'
import VisNetwork from './VisNetwork';
import Tag from "../../components/pure-css/form/Tag";
import GraphInfo from './components/pure-css/form/GraphInfo';
export class GraphView extends React.Component {

  shouldComponentUpdate(nextProps, nextState) {
    if (this.props.visGraphData === nextProps.visGraphData) {
      return false;
    } else {
      return true;
    }
  }

  constructor(props) {
    super(props)
    this.ref = React.createRef(); 
    this.handleResize = () => {
      this.graph.changeSize(this.ref.current.clientWidth, this.ref.current.clientHeight);
      this.graph.fitCenter();
    }
  }
    
  componentDidMount() {
    const contextMenu = new G6.Menu({
      getContent(evt) {
        let menu = '';
        if (evt.target && evt.target.isCanvas && evt.target.isCanvas()) {
          menu = `<ul id='graphViewerMenu'>
            <li title='addNodeToDb'>Add node</li>
          </ul>`;
        } else if (evt.item) {
          const itemType = evt.item.getType();
          if(itemType === 'node') {
            menu = `<ul id='graphViewerMenu'>
              <li title='deleteNode'>Delete node</li>
              <li title='editNode'>Edit node</li>
              <li title='expandNode'>Expand node</li>
              <li title='setAsStartnode'>Set as startnode</li>
            </ul>`;
          }
          if(itemType === 'edge') {
            menu = `<ul id='graphViewerMenu'>
              <li title='deleteEdge'>Delete edge</li>
              <li title='editEdge'>Edit edge</li>
            </ul>`;
          }
        }
        return `${menu}`;
      },
      handleMenuClick: (target, item) => {
        let chosenAction = target.getAttribute('title');
        if(chosenAction === 'deleteNode') {
          this.removeNode(item._cfg.id);
        } else if(chosenAction === 'editNode') {
          this.props.onEditNode(item._cfg.id);
        } else if(chosenAction === 'expandNode') {
          this.props.onExpandNode(item._cfg.id);
        } else if(chosenAction === 'setAsStartnode') {
          this.props.onSetStartnode(item._cfg.id);
        } else if(chosenAction === 'addNode') {
          this.addNode();
        }  else if(chosenAction === 'addNodeToDb') {
          this.props.onAddNodeToDb();
        } else if(chosenAction === 'deleteEdge') {
          this.props.onDeleteEdge(item._cfg.id);
        } else if(chosenAction === 'editEdge') {
          this.props.onEditEdge(item._cfg.id);
        } 
      },
      offsetX: 16,
      offsetY: -90,
      itemTypes: ['node', 'edge', 'canvas'],
    });

    const container = ReactDOM.findDOMNode(this.ref.current);
    this.graph = new G6.Graph({
      container: this.ref.current,
      width: 1200,
      height: 800,
      plugins: [contextMenu],
      enabledStack: true,
      layout: {
        type: 'gForce',
        preventOverlap: true,
        linkDistance: 150,
        maxIteration: 0,
        center: [Math.floor(container.clientWidth/2), 400],
        //center: [400, 400],
      },
      modes: {
        default: [
          'brush-select',
          'drag-canvas',
          'drag-node',
          'zoom-canvas',
          {
            type: 'tooltip',
            formatText(model) {
              const text = '<strong>Label:</strong> ' + model.label;
              return text;
            },
          },
          {
            type: 'edge-tooltip',
            formatText(model) {
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
            key: 'shift',
          },
        ],
      },
      defaultNode: {
        type: 'circle',
        size: 40,
        style: {
          fill: '#' + this.props.nodeColor,
          stroke: '#' + this.props.nodeColor,
          lineWidth: 8,
          cursor: 'pointer',
        },
        labelCfg: {
          position: 'bottom',
          offset: 6,
          style: {
            fill: '#555555',
            fontStyle: 'regular',
            fontFamily: 'Roboto',
            fontSize: 12,
          },
        },
      },
      nodeStateStyles: {
        hover: {
          stroke: '#ffffff',
          lineWidth: 4,
          shadowColor: '#cccccc',
          shadowBlur: 20,
          cursor: 'pointer',
          style: {
            stroke: '#ffffff',
            lineWidth: 2,
            shadowColor: '#cccccc',
            shadowBlur: 20,
            cursor: 'pointer'
          },
        },
        selected: {
          stroke: '#555555',
          lineWidth: 4
        },
        searchedNode: {
          fill: '#' + this.props.nodeColor,
          stroke: '#1D2A12',
          lineWidth: 4,
          shadowColor: '#' + this.props.nodeColor,
          shadowBlur: 10,
          cursor: 'pointer'
        }
      },
      defaultEdge: {
        type: this.props.edgeType,
        labelCfg: {
          autoRotate: true,
          refY: 10,
          style: {
            fill: '#1D2A12',
            fontStyle: 'regular',
            fontFamily: 'Roboto',
            fontSize: 12
          },
        },
        style: {
          stroke: '#' + this.props.edgeColor,
          cursor: 'pointer',
        },
      },
      edgeStateStyles: {
        hover: {
          fill: '#9F7000',
          shadowColor: '#848484',
          shadowBlur: 10,
          lineWidth: 4,
          cursor: 'pointer'
        },
        selected: {
          stroke: '#f00',
          shadowColor: '#848484',
          shadowBlur: 10,
          lineWidth: 1
        },
        line: {
          type: 'polyline',
          startArrow: false,
          endArrow: {
            path: G6.Arrow.triangle(10, 20, 25),
            fill: '#f00',
            stroke: '#0f0',
            opacity: 0,
            lineWidth: 3,
          },
          lineWidth: 1,
          cursor: 'pointer'
        },
        arrow: {
          endArrow: true,
          lineWidth: 1,
          cursor: 'pointer'
        },
        curve: {
          type: 'arc',
          startArrow: false,
          endArrow: {
            path: G6.Arrow.triangle(10, 20, 25),
            fill: '#f00',
            stroke: '#0f0',
            opacity: 0,
            lineWidth: 3,
          },
          lineWidth: 1,
          cursor: 'pointer'
        },
        dotted: {
          startArrow: false,
          endArrow: {
            path: G6.Arrow.triangle(10, 20, 25),
            fill: '#f00',
            stroke: '#0f0',
            opacity: 0,
            lineWidth: 3,
          },
          lineWidth: 1,
          lineDash: [2, 2, 2, 2, 2, 2],
          cursor: 'pointer'
        },
        dashed: {
          startArrow: false,
          endArrow: {
            path: G6.Arrow.triangle(10, 20, 25),
            fill: '#f00',
            stroke: '#0f0',
            opacity: 0,
            lineWidth: 3,
          },
          lineWidth: 1,
          lineDash: [7, 7, 7, 7, 7, 7],
          cursor: 'pointer'
        },
        tapered: {
          startArrow: false,
          endArrow: {
            path: G6.Arrow.triangle(10, 20, 25),
            fill: '#f00',
            stroke: '#0f0',
            opacity: 0,
            lineWidth: 3,
          },
          lineWidth: 1,
          stroke: `l(0) 0:#ffffff 0.5:#9fb53a 1:#82962d`,
          cursor: 'pointer'
        },
        searchedEdge: {
          stroke: '#555555',
          lineWidth: 2,
          shadowColor: '#555555',
          shadowBlur: 5,
          cursor: 'pointer',
        }
      },
    });

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
      const id = (Math.random() + 1).toString(36).substring(7);
      const edgeModel = {
        id: id,
        source: e.edge._cfg.model.source,
        target: e.edge._cfg.model.target
      };
      
      this.graph.addItem('edge', edgeModel);
      this.props.onAddSingleEdge(edgeModel);
    });

    window.addEventListener("resize", this.handleResize);
  }

  componentDidUpdate() {
    const container = ReactDOM.findDOMNode(this.ref.current);
    this.graph.changeSize(container.offsetWidth, container.offsetHeight);
    this.graph.data(this.props.data);
    this.graph.render();
    if(this.props.nodeColorAttribute) {
      if(!this.props.nodeColorByCollection) {
        this.colorNodesByAttribute();
      }
    }
    if(this.props.nodesSizeMinMax) {
      if(this.props.nodesSizeMinMax[0] !== null) {
        this.resizeNodesByAttribute();
      }
    }
    if(this.props.connectionsMinMax) {
      if(this.props.connectionsMinMax[0] !== null) {
        this.resizeNodesByConnections();
      }
    }
    if(this.props.edgeColorAttribute) {
      if(!this.props.edgeColorByCollection) {
        this.colorEdgesByAttribute();
      }
    }
  }

  componentWillUnmount() {
    window.removeEventListener("resize", this.handleResize);
  }

  getNodes = () => {
    const nodes = this.graph.getNodes();
  }

  getEdges = () => {
    const edges = this.graph.getEdges();
  }

  addNode = () => {
    const min = 0;
    const maxX = 1000;
    const maxY = 400;
    const id = (Math.random() + 1).toString(36).substring(7);
    const address = (Math.random() + 1).toString(8).substring(7);
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
      tempNodes.push(node.get("model"));
      return node.get("model");
    });
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
    this.graph.node((node) => {
      return {
        id: node.id,
      };
    });
  }

  updateNodeModel = () => {
    const nodes = this.graph.getNodes();
    nodes.forEach((node) => {
      const idSplit = node._cfg.id.split('/');

      const model = {
        label: `${idSplit[1]} (${idSplit[0]})`
      };
      this.graph.updateItem(node, model);
    });
  }

  colorNodesByAttribute = () => {
    const nodes = this.graph.getNodes();
    nodes.forEach((node) => {
      const tempNodeColor = Math.floor(Math.random()*16777215).toString(16).substring(1, 3) + Math.floor(Math.random()*16777215).toString(16).substring(1, 3) + Math.floor(Math.random()*16777215).toString(16).substring(1, 3);
      const value2 = {
        'name': node._cfg.model.colorCategory || '',
        'color': tempNodeColor
      };
      const nodesColorAttributeIndex = this.props.nodesColorAttributes.findIndex(object => object.name === value2.name);
      if (nodesColorAttributeIndex === -1) {
        this.props.nodesColorAttributes.push(value2);
      }
      const categoryColor = this.props.nodesColorAttributes.find(object => object.name === node._cfg.model.colorCategory) ? '#' + this.props.nodesColorAttributes.find(object => object.name === node._cfg.model.colorCategory).color : '#f00';
      const model = {
        color: categoryColor,
        style: {
          fill: categoryColor,
          stroke: categoryColor
        },
      };
      this.graph.updateItem(node, model);
    });
    if(this.props.edgeColorAttribute) {
      this.colorEdgesByAttribute();
    }
    this.graph.render();
  }

  resizeNodesByAttribute = () => {
    const nodes = this.graph.getNodes();

    nodes.forEach((node) => {
      const minNodeSize = 20;
      const maxNodeSize = 200;

      const maxDiff = this.props.nodesSizeMinMax[1] - this.props.nodesSizeMinMax[0];
      const nodeSizeDiff = maxNodeSize - minNodeSize;
      const d = node._cfg.model.sizeCategory - this.props.nodesSizeMinMax[0];
      const tempNodeSize = Math.floor(minNodeSize + nodeSizeDiff * (d / maxDiff));
      const model = {
        size: tempNodeSize
      };
      this.graph.updateItem(node, model);
    });
    this.graph.render();
  }

  resizeNodesByConnections = () => {
    const nodes = this.graph.getNodes();

    nodes.forEach((node) => {
      const minNodeSize = 20;
      const maxNodeSize = 200;
      const maxDiff = this.props.connectionsMinMax[1] - this.props.connectionsMinMax[0];
      const nodeSizeDiff = maxNodeSize - minNodeSize;
      const d = node._cfg.edges.length - this.props.connectionsMinMax[0];
      const tempNodeSize = Math.floor(minNodeSize + nodeSizeDiff * (d / maxDiff));
      const model = {
        size: tempNodeSize
      };
      this.graph.updateItem(node, model);
    });
    this.graph.render();
  }

  colorEdgesByAttribute = () => {
    const edges = this.graph.getEdges();

    edges.forEach((edge) => {
      const tempEdgeColor = Math.floor(Math.random()*16777215).toString(16).substring(1, 3) + Math.floor(Math.random()*16777215).toString(16).substring(1, 3) + Math.floor(Math.random()*16777215).toString(16).substring(1, 3);
      const value2 = {
        'name': edge._cfg.model.colorCategory || '',
        'color': tempEdgeColor
      };
      const edgesColorAttributeIndex = this.props.edgesColorAttributes.findIndex(object => object.name === value2.name);
      if (edgesColorAttributeIndex === -1) {
        this.props.edgesColorAttributes.push(value2);
      }
      const categoryColor = this.props.edgesColorAttributes.find(object => object.name === edge._cfg.model.colorCategory) ? '#' + this.props.edgesColorAttributes.find(object => object.name === edge._cfg.model.colorCategory).color : '#f00';
    
      if (!edge.style) {
        edge.style = {};
      }
      const edgeModel = {
        style: {
          lineWidth: 4,
          opacity: 0.6,
          stroke: categoryColor
        }
      };
      this.graph.updateItem(edge, edgeModel);
    });
    this.graph.render();
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
      this.graph.setItemState(document, 'searched', true);
      this.graph.focusItem(document, true);
    }
  }

  highlightNode = (previousSearchedNode, node) => {
    if(node !== undefined) {
      if(previousSearchedNode) {
        this.graph.clearItemStates(previousSearchedNode);
      }
      this.graph.setItemState(node, 'searchedNode', true);
      this.graph.focusItem(node, true);
    }
  }

  highlightEdge = (previousSearchedEdge, edge) => {
    if(edge) {
      if(previousSearchedEdge) {
        this.graph.clearItemStates(previousSearchedEdge);
      }
      this.graph.setItemState(edge, 'searchedEdge', true);
      this.graph.focusItem(edge, true);
    }
  }

  changeEdgeStyleFromUi = (edgeStyle) => {
    if(edgeStyle) {
      const edges = this.graph.getEdges();
      
      edges.forEach((edge) => {
        this.graph.clearItemStates(edge);
        this.graph.setItemState(edge, edgeStyle.type, true);
      });
    }
  }

  changeGraphLayoutFromUi = (layout) => {
    this.graph.updateLayout({
      type: layout,
      preventOverlap: true,
    });
  }

  changeVisGraphLayoutFromUi = (layout) => {
    /*
    this.graph.updateLayout({
      type: layout,
      preventOverlap: true,
    });
    */
   console.log("WUHUUU Changed VisGraphLayout in GraphView: ", layout);
  }

  render() {
    return <>
      <Headerinfo
        graphName={this.props.graphName}
        graphData={this.props.data}
        responseDuration={this.props.responseDuration}
        nodesColorAttributes={this.props.nodesColorAttributes}
        onDownloadScreenshot={() => this.downloadScreenshot()}
        onDownloadFullScreenshot={() => this.downloadFullScreenshot()}
        onChangeLayout={(layout) => {
          this.changeLayout(layout);
        }}
        onChangeGraphData={(newGraphData) => {
          this.props.onChangeGraphData(newGraphData);
        }}
        onLoadFullGraph={() => this.props.onLoadFullGraph}
        onDocumentSelect={(document) => this.highlightDocument(document)}
        onNodeSearched={(previousSearchedNode, node) => this.highlightNode(previousSearchedNode, node)}
        onEdgeSearched={(previousSearchedEdge, edge) => this.highlightEdge(previousSearchedEdge, edge)}
        onEdgeStyleChanged={(edgeStyle) => this.changeEdgeStyleFromUi(edgeStyle)}
        onGraphLayoutChange={(layout) => this.changeGraphLayoutFromUi(layout)}
        onVisGraphLayoutChange={(layout) => this.changeVisGraphLayoutFromUi(layout)}
        onGraphDataLoaded={(newGraphData, responseTimesObject) => {
          this.props.onGraphDataLoaded({newGraphData, responseTimesObject})}}
      />

      <VisNetwork
        graphData={this.props.visGraphData}
        graphName={this.props.graphName}
        options={{}}
      />

      <GraphInfo>
        <Tag label={`${this.props.visGraphData.nodes.length} nodes`}/><Tag label={`${this.props.visGraphData.edges.length} edges`}/><Tag style='transparent' label={`Response time: ${this.props.responseDuration}ms`}/>
      </GraphInfo>

      <div ref={this.ref} className={styles.graphContainer} style={{ display: 'none' }}> </div>
    </>;
  }
}
