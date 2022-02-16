import React, { useContext } from "react";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterNodeStart = () => {
  const urlParameters = useContext(UrlParametersContext);
  
  return (
    <>
      <button
        onClick={() => {
          console.log("urlParameters (nodeStart): ", urlParameters);
          console.log("NodeStart: ", urlParameters.nodeStart);
          urlParameters.nodeStart = "germanCity/Berlin";
        }}
      >
        Change nodeStart to germanCity/Berlin
      </button>
      <br />
      <strong>nodeStart: {urlParameters.nodeStart}</strong>
      <hr />
    </>
  );
};

export default ParameterNodeStart;
