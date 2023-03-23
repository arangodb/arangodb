import { FormLabel, Select } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "./url-parameters-context";

const GraphLayoutSelector = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [layout, setLayout] = useState(urlParameters.layout);

  const layouts = [
    {
      layout: "forceAtlas2"
    },
    {
      layout: "hierarchical"
    }
  ];

  const handleChange = event => {
    setLayout(event.target.value);
    const newUrlParameters = { ...urlParameters, layout: event.target.value };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="layout">Layout</FormLabel>
      <Select id="layout" value={layout} onChange={handleChange}>
        {layouts.map(style => {
          const { layout } = style;
          return (
            <option key={layout} value={layout}>
              {layout}
            </option>
          );
        })}
      </Select>
      <InfoTooltip />
    </>
  );
};

export default GraphLayoutSelector;
