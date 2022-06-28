import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColor, setNodeColor] = useState(urlParameters.nodeColor);

  const newUrlParameters = { ...urlParameters };
  let calculatedNodeColor = nodeColor;
  if (!nodeColor.startsWith('#')) {
    calculatedNodeColor = '#' + nodeColor;
  }

  return (
    <div>
      <Textinput
        label={'Default node color'}
        type={'color'}
        value={calculatedNodeColor}
        width={'60px'}
        height={'30px'}
        onChange={(e) => {
          setNodeColor(e.target.value);
          newUrlParameters.nodeColor = e.target.value.replace("#", "");
          setUrlParameters(newUrlParameters);
      }}
      disabled={urlParameters.nodeColorByCollection}>
      </Textinput>
    </div>
  );
};

export default ParameterNodeColor;
