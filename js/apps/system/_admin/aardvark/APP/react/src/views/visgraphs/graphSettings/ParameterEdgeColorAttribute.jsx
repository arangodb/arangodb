import { FormLabel, Input } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "../url-parameters-context";

const ParameterEdgeColorAttribute = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorAttribute, setEdgeColorAttribute] = useState(
    urlParameters.edgeColorAttribute
  );

  const handleChange = event => {
    setEdgeColorAttribute(event.target.value);
    const newUrlParameters = {
      ...urlParameters,
      edgeColorAttribute: event.target.value
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeColorAttribute"> Edge color attribute</FormLabel>

      <Input
        id="edgeColorAttribute"
        width="200px"
        min={1}
        value={edgeColorAttribute}
        background="#ffffff"
        size="sm"
        onChange={handleChange}
        disabled={urlParameters.edgeColorByCollection}
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
