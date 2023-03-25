import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "../url-parameters-context";

const ParameterNodeLabelByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeLabelByCollection, setNodeLabelByCollection] = useState(
    urlParameters.nodeLabelByCollection
  );

  const handleChange = event => {
    setNodeLabelByCollection(event.target.checked);
    const newUrlParameters = {
      ...urlParameters,
      nodeLabelByCollection: event.target.checked
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeLabelByCollection">
        Show collection name
      </FormLabel>
      <Checkbox
        id="nodeLabelByCollection"
        checked={nodeLabelByCollection}
        onChange={handleChange}
      />
      <InfoTooltip label={"Adds a collection name to the node label."} />
    </>
  );
};

export default ParameterNodeLabelByCollection;
