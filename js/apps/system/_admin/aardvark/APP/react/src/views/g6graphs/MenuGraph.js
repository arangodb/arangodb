/* global arangoHelper, arangoFetch, frontendConfig */

import React, { useEffect, useState, useCallback } from 'react';
import Graphin, { Utils, GraphinContext } from '@antv/graphin';
import { message, Card } from 'antd';
import { ContextMenu, FishEye } from '@antv/graphin-components';
import { useFetch } from './useFetch';
import AqlEditor from './AqlEditor';
import {
  TagFilled,
  DeleteFilled,
  ExpandAltOutlined
} from '@ant-design/icons';

const MenuGraph = () => {
  let [queryString, setQueryString] = useState("/_admin/aardvark/g6graph/routeplanner");
  let [graphData, setGraphData] = useState(null);
  let [queryMethod, setQueryMethod] = useState("GET");

  // fetchData component - start
  const fetchData = useCallback(() => {

    arangoFetch(arangoHelper.databaseUrl(queryString), {
      method: queryMethod,
    })
    .then(response => response.json())
    .then(data => {
      console.log("NEW DATA");
      console.log(data);
      
      setGraphData(data);
    })
    .catch((err) => {
      console.log(err);
    });
  }, [queryString]);
  // fetchData component - end

  useEffect(() => {
    fetchData();
  }, [fetchData]);

  // Menu Component
  const { Menu } = ContextMenu;
  const options = [
        {
            key: 'tag',
            icon: <TagFilled />,
            name: 'Tag (social)',
        },
        {
            key: 'delete',
            icon: <DeleteFilled />,
            name: 'Delete',
        },
        {
            key: 'expand',
            icon: <ExpandAltOutlined />,
            name: 'Expand (Routeplanner)',
        },
  ];
  
  const HandleChange = (menuItem, menuData) => {
      console.log(menuItem, menuData);
      message.info(`Element：${menuData.id}，Action：${menuItem.name}, Key：${menuItem.key}`);
      let query;
      switch(menuItem.key) {
        case "tag":
            query = '/_admin/aardvark/g6graph/social';
            queryMethod = "GET";
            break;
        case "delete":
            query = `/_api/gharial/routeplanner/vertex/${menuData.id}`;
            queryMethod = "DELETE";
            break;
        case "expand":
            query = `/_admin/aardvark/graph/routeplanner?depth=2&limit=250&nodeColor=#2ecc71&nodeColorAttribute=&nodeColorByCollection=true&edgeColor=#cccccc&edgeColorAttribute=&edgeColorByCollection=false&nodeLabel=_key&edgeLabel=&nodeSize=&nodeSizeByEdges=true&edgeEditable=true&nodeLabelByCollection=false&edgeLabelByCollection=false&nodeStart=&barnesHutOptimize=true&query=FOR v, e, p IN 1..1 ANY "${menuData.id}" GRAPH "routeplanner" RETURN p`;
            queryMethod = "GET";
            break;
        default:
            break;
      }
      message.info(`query: ${query}; queryMethod: ${queryMethod}`);
      setQueryMethod(queryMethod);
      setQueryString(query);
      /*
      const {loading, data, error} = useFetch(arangoHelper.databaseUrl(query));
      if(loading) {
        message.info(`Deleting node`);
      }
      if(error) {
        message.info(`Error deleting node: ${JSON.stringify(error, null, 2)}`);
      }
      if(data) {
        console.log("DATA after deleting node:");
        console.log(data);
      }
      */
      message.info("AFTER deleting");
  };

  // /_admin/aardvark/g6graph/social
  // /_admin/aardvark/g6graph/routeplanner
  if(graphData) {
    return (
      <div>
        <AqlEditor
          queryString={queryString}
          onNewSearch={(myString) => {setQueryString(myString)}}
          onQueryChange={(myString) => {setQueryString(myString)}} />
        <Card title="Routeplanner">
          <Graphin data={graphData}>
            <ContextMenu style={{ width: '80px' }}>
              <Menu options={options} onChange={HandleChange} bindType="node" />
            </ContextMenu>
          </Graphin>
        </Card>
      </div>
    );
  } else {
    return null;
  }

}
/*
const MenuGraph = () => {
  
  const { Menu } = ContextMenu;
    const options = [
        {
            key: 'tag',
            icon: <TagFilled />,
            name: 'Tag',
        },
        {
            key: 'delete',
            icon: <DeleteFilled />,
            name: 'Delete',
        },
        {
            key: 'expand',
            icon: <ExpandAltOutlined />,
            name: 'Expand',
        },
    ];
    const CanvasMenu = props => {
        const { graph, contextmenu } = React.useContext(GraphinContext);
        const context = contextmenu.canvas;
        const handleDownload = () => {
            graph.downloadFullImage('canvas-contextmenu');
            context.handleClose();
        };
        const handleClear = () => {
            message.info(`Canvas successfully deleted`);
            context.handleClose();
        };
        const handleStopLayout = () => {
            message.info(`Layout successfully stopped`);
            context.handleClose();
        };
        const handleOpenFishEye = () => {
            props.handleOpenFishEye();
        };
        return (
            <Menu bindType="canvas">
                <Menu.Item onClick={handleOpenFishEye}>Open Fisheye</Menu.Item>
                <Menu.Item onClick={handleClear}>Clear Canvas</Menu.Item>
                <Menu.Item onClick={handleStopLayout}>Stop layout</Menu.Item>
                <Menu.Item onClick={handleDownload}>Download Screenshot</Menu.Item>
            </Menu>
        );
    };

    const MenuGraphContainer = () => {
        const [visible, setVisible] = React.useState(false);
        const handleOpenFishEye = () => {
            setVisible(true);
        };
        const handleClose = () => {
            setVisible(false);
        };

        //const data = Utils.mock(5)
            //.circle()
            //.graphin();
        const [graphData, setGraphData] = useState(null);
        //const [query, setQuery] = useState("");
        //const [info, setInfo] = useState("");
        //setQuery('/_admin/aardvark/g6graph/routeplanner');
        //const {loading, data, error} = useFetch(arangoHelper.databaseUrl(query));
        const {loading, data, error} = useFetch(arangoHelper.databaseUrl('/_admin/aardvark/g6graph/routeplanner'));
        if(loading) {
          message.info(`Data is loading`);
          //return <h1>Loading...</h1>;
        }
        if(error) {
          message.info(`Error loading graph data: ${JSON.stringify(error, null, 2)}`);
          //return <pre>{JSON.stringify(error, null, 2)}</pre>
        }
        if(data) {
          //setGraphData(data);
          console.log("DATA:");
          console.log(data);
          
          //return (
            //<div>
              //<pre>{JSON.stringify(data, null, 2)}</pre>
            //</div>
          //)
        }

        //useEffect(() => {
          //arangoFetch(arangoHelper.databaseUrl('/_admin/aardvark/g6graph/routeplanner'))
          //.then(res => res.json())
          //.then(setData)
          //.catch(console.error)
          //.catch(console.error);
        //}, []);
        
        const HandleChange = (menuItem, menuData) => {
            console.log(menuItem, menuData);
            message.info(`Element：${menuData.id}，Action：${menuItem.name}, Key：${menuItem.key}`);
            let query;
            switch(menuItem.key) {
              case "tag":
                  query = `TAG node with ID ${menuData.id}`;
                  break;
              case "delete":
                  //query = `DELETE node with ID ${menuData.id}`;
                  query = `/_api/gharial/routeplanner/vertex/${menuData.id}`;
                  break;
              case "expand":
                  query = `EXPAND node with ID ${menuData.id}`;
                  break;
              default:
                  break;
            }
            message.info(query);
            const {loading, data, error} = useFetch(arangoHelper.databaseUrl(query));
            if(loading) {
              message.info(`Deleting node`);
            }
            if(error) {
              message.info(`Error deleting node: ${JSON.stringify(error, null, 2)}`);
            }
            if(data) {
              console.log("DATA after deleting node:");
              console.log(data);
            }
            //useEffect(() => {
              //arangoFetch(arangoHelper.databaseUrl(query))
              //.then(res => res.json())
              //.then(setData)
              //.catch(console.error)
              //.catch(console.error);
            //}, []);
            message.info("AFTER deleting");
        };

        if(data) {
          return (
            <div>
                          <Card title="Routeplanner">
                              <Graphin data={data}>
                                  <ContextMenu style={{ width: '80px' }}>
                                      <Menu options={options} onChange={HandleChange} bindType="node" />
                                  </ContextMenu>
                                  <ContextMenu style={{ width: '80px' }} bindType="canvas">
                                      <CanvasMenu handleOpenFishEye={handleOpenFishEye} />
                                  </ContextMenu>
                                  <ContextMenu style={{ width: '120px' }} bindType="edge">
                                      <Menu
                                          options={options.map(item => {
                                              return { ...item, name: `${item.name}-EDGE` };
                                          })}
                                          onChange={HandleChange}
                                          bindType="edge"
                                      />
                                  </ContextMenu>
                                  <FishEye options={{}} visible={visible} handleEscListener={handleClose} />
                              </Graphin>
                          </Card>
              </div>
          );
        } else {
          return null;
        }
    }

    return <div><MenuGraphContainer /></div>;
}
*/

export default MenuGraph;
