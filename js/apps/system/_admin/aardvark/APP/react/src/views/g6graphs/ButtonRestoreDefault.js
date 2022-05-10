/* global arangoHelper, arangoFetch, frontendConfig, $ */

import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Button, Tooltip } from 'antd';
import { SaveOutlined } from '@ant-design/icons';

const ButtonRestoreDefault = ({ graphName, onGraphDataLoaded }) => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);

  const DEFAULTURLPARAMETERS = {
    depth: 2,
    limit: 250,
    fruchtermann: "fruchtermann",
    nodeColorByCollection: false,
    edgeColorByCollection: false,
    nodeColor: "fbe08e",
    nodeColorAttribute: "",
    edgeColor: "848484",
    edgeColorAttribute: "",
    nodeLabel: "",
    edgeLabel: "",
    edgeType: "line",
    nodeSize: "",
    nodeSizeByEdges: false,
    edgeEditable: true,
    nodeLabelByCollection: false,
    edgeLabelByCollection: false,
    nodeStart: "",
    barnesHutOptimize: false,
    query: "",
  };

  const callApi = () => {
      $.ajax({
        type: 'GET',
        url: arangoHelper.databaseUrl(`/_admin/aardvark/g6graph/${graphName}`),
        contentType: 'application/json',
        data: urlParameters[0],
        success: function (data) {
          onGraphDataLoaded(data);
        },
        error: function (e) {
          arangoHelper.arangoError('Graph', 'Could not load graph.');
        }
      });
  };
  
  return (
    <>
      <Tooltip placement="bottom" title={"Use current settings to receive data"}>    
        <Button
          icon={<SaveOutlined />}
          onClick={() => {
            setUrlParameters(DEFAULTURLPARAMETERS);
            callApi();
          }}>
          Restore default values
        </Button>
      </Tooltip>
    </>
  );
};

export default ButtonRestoreDefault;
