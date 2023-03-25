import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "./url-parameters-context";

const ParameterEdgeLabelByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabelByCollection, setEdgeLabelByCollection] = useState(
    urlParameters.edgeLabelByCollection
  );

  const handleChange = event => {
    setEdgeLabelByCollection(event.target.checked);
    const newUrlParameters = {
      ...urlParameters,
      edgeLabelByCollection: event.target.checked
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeLabelByCollection">
        Show collection name
      </FormLabel>
      <Checkbox
        id="edgeLabelByCollection"
        checked={edgeLabelByCollection}
        onChange={handleChange}
      />
      <InfoTooltip label={"Adds a collection name to the edge label."} />
    </>
  );
};

export default ParameterEdgeLabelByCollection;
