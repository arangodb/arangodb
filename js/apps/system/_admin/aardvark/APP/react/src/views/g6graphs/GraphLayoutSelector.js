import React, { useContext, useState } from 'react';
import { Select } from 'antd';
import ToolTip from '../../components/arango/tootip';
import { UrlParametersContext } from "./url-parameters-context";

const GraphLayoutSelector = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [layout, setLayout] = useState(urlParameters.layout);

  const newUrlParameters = { ...urlParameters };

  const SelectOption = Select.Option;

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

  const handleChange = layout => {
    setLayout(layout);
    newUrlParameters.layout = layout;
    setUrlParameters(newUrlParameters);
  };

  return <>
    <form>
    <div style={{ 'marginBottom': '20px', 'display': 'flex', 'alignItems': 'center', 'justifyContent': 'flexStart' }}>
      <label for="edgetype" style={{ 'color': '#ffffff', 'width': '150px' }}>Layout</label>
        <Select
          name="graphlayout"
          value={layout}
          className="graphReactViewContainer"
          dropdownClassName="graphReactViewContainer"
          onChange={handleChange}
          style={{
            'width': '200px',
            'height': 'auto',
            'margin-right': '8px',
            'color': '#555555',
            'border': '2px solid rgba(140, 138, 137, 0.25)',
            'border-radius': '4px',
            'background-color': '#fff !important',
            'box-shadow': 'none',
            'outline': 'none',
            'outline-color': 'transparent',
            'outline-style': 'none'
          }}
        >
          {layouts.map(style => {
            const { layout } = style;
            return (
              <SelectOption 
                key={layout} 
                value={layout} 
                className='graphReactViewContainer' 
                dropdownClassName="graphReactViewContainer"
              >
                {layout}
              </SelectOption>
            );
          })}
        </Select>
        <ToolTip
          title={"Graph layouts are the algorithms arranging the node positions."}
          setArrow={true}
        >
          <span className="arangoicon icon_arangodb_info" style={{ fontSize: '16px', color: '#989CA1' }}></span>
        </ToolTip>
      </div>
    </form>
  </>;
}

export default GraphLayoutSelector;
