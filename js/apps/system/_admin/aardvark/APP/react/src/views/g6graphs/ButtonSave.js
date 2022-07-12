/* global arangoHelper, $ */

import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Button, Tooltip } from 'antd';
import { SaveOutlined } from '@ant-design/icons';

const ButtonSave = ({ graphName, onGraphDataLoaded, onIsLoadingData }) => {
  const urlParameters = useContext(UrlParametersContext);
  const [isLoadingData, setIsLoadingData] = useState(false)

  const callApi = () => {
    localStorage.setItem(`${graphName}-gv-urlparameters`, JSON.stringify(urlParameters[0]));
    setIsLoadingData(true);
    onIsLoadingData(true);

    $.ajax({
      type: 'GET',
      url: arangoHelper.databaseUrl(`/_admin/aardvark/g6graph/${graphName}`),
      contentType: 'application/json',
      data: urlParameters[0],
      success: function (data) {
        const element = document.getElementById("graph-card");
        console.log("element: ", element);
        element.scrollIntoView({ behavior: "smooth" });
        setIsLoadingData(false);
        onIsLoadingData(false);
        onGraphDataLoaded(data);
      },
      error: function (e) {
        arangoHelper.arangoError('Graph', 'Could not load graph.');
      }
    });
  };
  
  return (
    <>
      <Tooltip placement="left" title={"Use current settings to receive data"} overlayClassName="graphReactViewContainer">    
        <Button
          type="primary"
          style={{ background: "#2ecc71", borderColor: "#2ecc71" }}
          className="graphReactViewContainer"
          overlayClassName="graphReactViewContainer"
          icon={<SaveOutlined />}
          onClick={() => {
            console.log("urlParameters (to make API call): ", urlParameters);
            callApi();
          }}
          disabled={isLoadingData}
        >
          Run
        </Button>
      </Tooltip>
    </>
  );
};

export default ButtonSave;
