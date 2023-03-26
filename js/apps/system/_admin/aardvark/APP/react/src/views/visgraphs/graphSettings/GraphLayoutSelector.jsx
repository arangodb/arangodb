import { FormLabel, Select } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const GraphLayoutSelector = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [layout, setLayout] = useState(urlParams.layout);

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
    const newUrlParameters = { ...urlParams, layout: event.target.value };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="layout">Layout</FormLabel>
      <Select size="sm" id="layout" value={layout} onChange={handleChange}>
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
