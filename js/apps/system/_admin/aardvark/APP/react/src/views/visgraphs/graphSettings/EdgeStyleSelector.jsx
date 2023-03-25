import { FormLabel, Select } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "../url-parameters-context";

const EdgeStyleSelector = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [type, setType] = useState(urlParameters.edgeType);

  const styles = [
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

  const handleChange = event => {
    setType(event.target.value);
    const newUrlParameters = { ...urlParameters, edgeType: event.target.value };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="edgeType">Type</FormLabel>
      <Select size="sm" id="edgeType" value={type} onChange={handleChange}>
        {styles.map(style => {
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
