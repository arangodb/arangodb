import { Spacer } from '@chakra-ui/react';
import React, { useContext, useState } from 'react';
import ToolTip from '../../components/arango/tootip';
import { UrlParametersContext } from "./url-parameters-context";

const GraphLayoutSelector = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [layout, setLayout] = useState(urlParameters.layout);

  const layouts = [
    {
      layout: 'forceAtlas2'
    },
    {
      layout: 'hierarchical'
    }
  ];

  const handleChange = (event) => {
    setLayout(event.target.value);
    const newUrlParameters = {...urlParameters, layout: event.target.value};
    setUrlParameters(newUrlParameters);
  }

  return (
    <div style={{ 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <label for="graphlayout" style={{ 'color': '#ffffff', 'width': '150px' }}>Layout</label>
      <select
        name="graphlayout"
        value={layout}
        onChange={handleChange}
        style={{ width: "200px" }}
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
      <Spacer />
      <ToolTip
        title={"Graph layouts are the algorithms arranging the node positions."}
        setArrow={true}
      >
        <div className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></div>
      </ToolTip>
    </div>
  );
}

export default GraphLayoutSelector;
