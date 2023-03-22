/* global arangoHelper, $ */

import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { IconButton } from "../../components/arango/buttons";

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
    let newResponseTimesObject = {...responseTimesObject};
    newResponseTimesObject = {...responseTimesObject, fetchStarted: new Date()};

    $.ajax({
      type: 'GET',
      url: arangoHelper.databaseUrl(`/_admin/aardvark/visgraph/${graphName}`),
      contentType: 'application/json',
      data: urlParameters[0],
      success: function (data) {
        newResponseTimesObject = {...newResponseTimesObject, fetchFinished: new Date()};

        const fetchDuration = Math.abs(newResponseTimesObject.fetchFinished.getTime() - newResponseTimesObject.fetchStarted.getTime());
        newResponseTimesObject = {...newResponseTimesObject, fetchDuration: fetchDuration};
        
        setResponseTimes(newResponseTimesObject);
        setIsLoadingData(false);
        onIsLoadingData(false);
        onGraphDataLoaded(data, newResponseTimesObject);
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
    <IconButton icon={'floppy-o'} onClick={() => {
      callApi();
    }}
      style={{
      background: '#2ECC71',
      color: 'white',
      paddingLeft: '14px',
      marginLeft: 'auto'
    }}>
      Apply
    </IconButton>
  );
};

export default ButtonSave;
