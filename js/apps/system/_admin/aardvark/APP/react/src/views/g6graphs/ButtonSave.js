/* global arangoHelper, $ */

import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Button, Tooltip } from 'antd';
import { SaveOutlined } from '@ant-design/icons';

const ButtonSave = ({ graphName, onGraphDataLoaded, onIsLoadingData }) => {
  const urlParameters = useContext(UrlParametersContext);
  const [isLoadingData, setIsLoadingData] = useState(false)
  let responseTimesObject = {
    fetchStarted: null,
    fetchFinished: null,
    fetchDuration: null
  }
  const [responseTimes, setResponseTimes] = useState(responseTimesObject);

  const callApi = () => {
    localStorage.setItem(`${graphName}-gv-urlparameters`, JSON.stringify(urlParameters[0]));
    setIsLoadingData(true);
    onIsLoadingData(true);
    responseTimesObject.fetchStarted = new Date();

    $.ajax({
      type: 'GET',
      url: arangoHelper.databaseUrl(`/_admin/aardvark/visgraph/${graphName}`),
      contentType: 'application/json',
      data: urlParameters[0],
      success: function (data) {
        responseTimesObject.fetchFinished = new Date();
        responseTimesObject.fetchDuration = Math.abs(responseTimesObject.fetchFinished.getTime() - responseTimesObject.fetchStarted.getTime());
        setResponseTimes(responseTimesObject);
        setIsLoadingData(false);
        onIsLoadingData(false);
        onGraphDataLoaded(data, responseTimesObject);
      },
      error: function (e) {
        console.log(e);
        arangoHelper.arangoError('Graph', e.responseJSON.errorMessage);
        setIsLoadingData(false);
        onIsLoadingData(false);
      }
    });
  };
  
  return (
    <>
      <Tooltip placement="left" title={"Use current settings to receive data"} overlayClassName="graphReactViewContainer">    
        <Button
          type="primary"
          style={{ background: "#2ecc71", borderColor: "#2ecc71", marginLeft: 'auto' }}
          className="graphReactViewContainer"
          overlayClassName="graphReactViewContainer"
          icon={<SaveOutlined />}
          onClick={() => {
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
