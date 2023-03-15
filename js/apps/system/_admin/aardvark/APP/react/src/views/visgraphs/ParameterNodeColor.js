import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterNodeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColor, setNodeColor] = useState(urlParameters.nodeColor);

  const handleChange = (event) => {
    setNodeColor(event.target.value);
    const newUrlParameters = {...urlParameters, nodeColor: event.target.value.replace("#", "")};
    setUrlParameters(newUrlParameters);
  }

  let calculatedNodeColor = nodeColor;
  if (!nodeColor.startsWith('#')) {
    calculatedNodeColor = '#' + nodeColor;
  }

  return (
    <div style={{ 'marginTop': '12px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Textinput
        label={'Default node color'}
        type={'color'}
        value={calculatedNodeColor}
        template={'graphviewer'}
        width={'60px'}
        height={'30px'}
        onChange={handleChange}
      disabled={urlParameters.nodeColorByCollection}>
      </Textinput>
    </div>
  );
};

export default ParameterNodeColor;
