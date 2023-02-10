import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterDepth = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [depth, setDepth] = useState(urlParameters.depth);

  const newUrlParameters = { ...urlParameters };

  return (
    <>
      <div style={{ 'marginTop': '24px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
        <Textinput
            label={'Depth'}
            value={depth}
            style={'graphviewer'}
            width={'60px'}
            onChange={(e) => {
              setDepth(+e.target.value);
              newUrlParameters.depth = +e.target.value;
              setUrlParameters(newUrlParameters);
            }}>
        </Textinput>
        <ToolTip
          title={"Search depth, starting from your start node."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </div>
    </>
  );
};

export default ParameterDepth;
