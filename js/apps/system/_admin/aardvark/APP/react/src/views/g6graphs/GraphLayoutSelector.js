import React, { useContext, useState } from 'react';
import ToolTip from '../../components/arango/tootip';
import { UrlParametersContext } from "./url-parameters-context";

const GraphLayoutSelector = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [layout, setLayout] = useState(urlParameters.layout);

  const newUrlParameters = { ...urlParameters };

  const layouts = [
    {
      layout: 'barnesHut'
    },
    {
      layout: 'forceAtlas2'
    },
    {
      layout: 'hierarchical'
    }
  ];

  const handleChange = (event) => {
    const layout = event.target.value;
    setLayout(layout);
    newUrlParameters.layout = layout;
    setUrlParameters(newUrlParameters);
  };

  return (
    <div style={{ 'marginBottom': '20px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <label for="graphlayout" style={{ 'color': '#ffffff', 'width': '150px' }}>Layout</label>
      <select
        name="graphlayout"
        value={layout}
        onChange={handleChange}
        style={{ width: "200px", 'marginRight': '8px' }}
      >
        {
          layouts.map(style => {
            const { layout } = style;
            return (
              <option key={layout} value={layout}>{layout}</option>
            );
          })
        }
      </select>
      <ToolTip
        title={"Graph layouts are the algorithms arranging the node positions."}
        setArrow={true}
      >
        <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
      </ToolTip>
    </div>
  );
}

export default GraphLayoutSelector;
