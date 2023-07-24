import { FormLabel, Select } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const STYLES_OPTIONS = [
  {
    type: "solid"
  },
  {
    type: "dashed"
  },
  {
    type: "dotted"
  }
];

const EdgeStyleSelector = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLSelectElement>) => {
    const newUrlParameters = { ...urlParams, edgeType: event.target.value };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeType">Type</FormLabel>
      <Select
        size="sm"
        id="edgeType"
        value={urlParams.edgeType}
        onChange={handleChange}
      >
        {STYLES_OPTIONS.map(style => {
          const { type } = style;
          return (
            <option key={type} value={type}>
              {type}
            </option>
          );
        })}
      </Select>
      <InfoTooltip label={"The type of the edge."} />
    </>
  );
};

export default EdgeStyleSelector;
