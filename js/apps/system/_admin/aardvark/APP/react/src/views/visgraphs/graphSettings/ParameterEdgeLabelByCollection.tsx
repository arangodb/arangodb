import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent, useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterEdgeLabelByCollection = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [edgeLabelByCollection, setEdgeLabelByCollection] = useState(
    urlParams.edgeLabelByCollection
  );

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    setEdgeLabelByCollection(event.target.checked);
    const newUrlParameters = {
      ...urlParams,
      edgeLabelByCollection: event.target.checked
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeLabelByCollection">
        Show collection name
      </FormLabel>
      <Checkbox
        id="edgeLabelByCollection"
        isChecked={edgeLabelByCollection}
        onChange={handleChange}
      />
      <InfoTooltip label={"Adds a collection name to the edge label."} />
    </>
  );
};

export default ParameterEdgeLabelByCollection;
