import React, { useContext, useState } from 'react';
import ToolTip from '../../components/arango/tootip';
import { UrlParametersContext } from "./url-parameters-context";

const EdgeStyleSelector = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [type, setType] = useState(urlParameters.edgeType);

  const styles = [
    {
      type: 'solid'
    },
    {
      type: 'dashed'
    },
    {
      type: 'dotted'
    }
  ];

  const handleChange = (event) => {
    setType(event.target.value);
    const newUrlParameters = {...urlParameters, edgeType: event.target.value};
    setUrlParameters(newUrlParameters);
  }

  return (
    <div style={{ 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <label for="edgetype" style={{ 'color': '#ffffff', 'width': '150px' }}>Type</label>
      <select
        name="edgetype"
        value={type}
        onChange={handleChange}
        style={{ width: "200px", 'marginRight': '8px' }}
      >
        {
          styles.map(style => {
          const { type } = style;
          return (
            <option key={type} value={type}>{type}</option>
          );
        })}
      </select>
      <ToolTip
        title={"The type of the edge."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
}

export default EdgeStyleSelector;
