import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterEdgeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeColorByCollection, setEdgeColorByCollection] = useState(
    urlParameters.edgeColorByCollection
  );

  const handleChange = event => {
    setEdgeColorByCollection(event.target.checked);
    const newUrlParameters = {
      ...urlParameters,
      edgeColorByCollection: event.target.checked
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeColorByCollection">
        Color edges by collection
      </FormLabel>
      <Checkbox
        id="edgeColorByCollection"
        checked={edgeColorByCollection}
        onChange={handleChange}
      />
      <InfoTooltip
        label={
          "Should edges be colorized by their collection? If enabled, edge color and edge color attribute will be ignored."
        }
      />
    </>
  );
};

export default ParameterEdgeColorByCollection;
