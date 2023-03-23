import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import Checkbox from "./components/pure-css/form/Checkbox.tsx";
import ToolTip from "../../components/arango/tootip";
import { Box, Flex, Center, FormLabel } from "@chakra-ui/react";
import { InfoTooltip } from "../../components/tooltip/InfoTooltip";

const ParameterNodeLabelByCollection = ({ onAddCollectionNameChange }) => {
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
        inline
        checked={nodeLabelByCollection}
        onChange={handleChange}
        template={"graphviewer"}
      />
      <InfoTooltip label={"Adds a collection name to the node label."} />
    </>
  );
};

export default ParameterNodeLabelByCollection;
