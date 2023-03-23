import React, { useContext } from "react";

const UrlParametersContext = React.createContext(null);

const useUrlParameterContext = () => useContext(UrlParametersContext);

export { UrlParametersContext, useUrlParameterContext };
