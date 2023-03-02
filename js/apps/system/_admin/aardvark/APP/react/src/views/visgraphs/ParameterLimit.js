import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import ToolTip from '../../components/arango/tootip';
import Textinput from "./components/pure-css/form/Textinput.tsx";

const ParameterLimit = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [limit, setLimit] = useState(urlParameters.limit);

  const newUrlParameters = { ...urlParameters };

  return (
    <>
      <div style={{ 'marginTop': '24px', 'marginBottom': '24px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
        <Textinput
          label={'Limit'}
          value={limit}
          template={'graphviewer'}
          width={'60px'}
          onChange={(e) => {
            setLimit(+e.target.value);
            newUrlParameters.limit = +e.target.value;
            setUrlParameters(newUrlParameters);
          }}>
        </Textinput>
        <ToolTip
          title={"Limit nodes count. If empty or zero, no limit is set."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </div>
    </>
  );
};

export default ParameterLimit;
