/* global arangoHelper, arangoFetch, frontendConfig, $ */

import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Button, Tooltip } from 'antd';
import { SaveOutlined } from '@ant-design/icons';

const ParameterStartnode = ({ onGraphDataLoaded }) => {
  const urlParameters = useContext(UrlParametersContext);

  const callApi = () => {
    console.log("urlParameters to use for API call: ", urlParameters);
    /*
    fetch(`https://pokeapi.co/api/v2/pokemon/${urlParameters.pokemon}`)
      .then((res) => res.json())
      .then((data) => console.log(data))
      .catch((err) => console.error(err));
    */
      $.ajax({
        type: 'GET',
        //url: arangoHelper.databaseUrl(`/_admin/aardvark/graph/${graphName}`),
        url: arangoHelper.databaseUrl(`/_admin/aardvark/graph/routeplanner`),
        contentType: 'application/json',
        data: urlParameters,
        success: function (data) {
          console.log("Graph based on urlParameter context loaded: ", data);
          console.log("data.nodes: ", data.nodes);
          console.log("data.nodes[0]: ", data.nodes[0]);
          console.log("data.nodes[0].color: ", data.nodes[0].color);
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
          type="primary"
          icon={<SaveOutlined />}
          onClick={() => {
            //onChangeGraphData(data2)
            console.log("urlParameters (to make API call): ", urlParameters);
            callApi();
          }}>
          Save
        </Button>
      </Tooltip>
    </>
  );
};

export default ParameterStartnode;
