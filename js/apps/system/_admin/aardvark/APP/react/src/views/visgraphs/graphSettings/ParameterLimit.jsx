import { FormLabel, Input } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "../url-parameters-context";

const ParameterLimit = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [limit, setLimit] = useState(urlParameters.limit);

  const handleChange = event => {
    setLimit(event.target.value);
    const newUrlParameters = { ...urlParameters, limit: event.target.value };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="limit">Limit</FormLabel>
      <Input
        id="limit"
        width="60px"
        min={1}
        required={true}
        value={limit}
        background="#ffffff"
        size="sm"
        onChange={handleChange}
      />
      <InfoTooltip
        label={"Limit nodes count. If empty or zero, no limit is set."}
      />
    </>
  );
};

export default ParameterLimit;
