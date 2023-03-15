import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterEdgeColor = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColor, setEdgeColor] = useState(urlParameters.edgeColor);

  const handleChange = (event) => {
    setEdgeColor(event.target.value);
    const newUrlParameters = {...urlParameters, edgeColor: event.target.value.replace("#", "")};
    setUrlParameters(newUrlParameters);
  }
  let calculatedEdgeColor = edgeColor;
  if (!edgeColor.startsWith('#')) {
    calculatedEdgeColor = '#' + edgeColor;
  }

  return (
    <div style={{ 'marginTop': '12px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <Textinput
        label={'Default edge color'}
        type={'color'}
        value={calculatedEdgeColor}
        template={'graphviewer'}
        width={'60px'}
        height={'30px'}
        onChange={handleChange}
      disabled={urlParameters.edgeColorByCollection}>
      </Textinput>
    </div>
  );
};

export default ParameterEdgeColor;
