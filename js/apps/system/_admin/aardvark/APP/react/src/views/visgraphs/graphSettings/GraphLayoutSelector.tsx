import { FormLabel, Select } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { LayoutType, useUrlParameterContext } from "../UrlParametersContext";

const GraphLayoutSelector = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const layouts: Array<{ layout: LayoutType }> = [
    {
      layout: "forceAtlas2"
    },
    {
      layout: "hierarchical"
    }
  ];

  const handleChange = (event: ChangeEvent<HTMLSelectElement>) => {
    const newUrlParameters = {
      ...urlParams,
      layout: event.target.value as LayoutType
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="layout">Layout</FormLabel>
      <Select
        size="sm"
        id="layout"
        value={urlParams.layout}
        onChange={handleChange}
      >
        {layouts.map(style => {
          const { layout } = style;
          return (
            <option key={layout} value={layout}>
              {layout}
            </option>
          );
        })}
      </Select>
      <InfoTooltip
        label={"Graph layouts are the algorithms arranging the node positions."}
      />
    </>
  );
};

export default GraphLayoutSelector;
