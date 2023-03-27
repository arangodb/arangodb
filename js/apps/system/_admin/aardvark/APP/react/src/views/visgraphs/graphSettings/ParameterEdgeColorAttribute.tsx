import { FormLabel, Input } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeColorAttribute = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      edgeColorAttribute: event.target.value
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeColorAttribute"> Edge color attribute</FormLabel>

      <Input
        id="edgeColorAttribute"
        width="200px"
        value={urlParams.edgeColorAttribute}
        size="sm"
        onChange={handleChange}
        disabled={urlParams.edgeColorByCollection}
      />
      <InfoTooltip
        label={
          "If an attribute is given, edges will be colorized by the attribute. This setting ignores default edge color if set."
        }
      />
    </>
  );
};

export default ParameterEdgeColorAttribute;
