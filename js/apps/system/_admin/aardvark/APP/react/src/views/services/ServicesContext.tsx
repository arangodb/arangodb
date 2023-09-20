import React, { createContext, ReactNode, useContext } from "react";
import { ServiceDescription } from "./Service.types";
import { useFetchServices } from "./useFetchServices";

type ServicesContextType = {
  services: ServiceDescription[] | undefined;
};
const ServicesContext = createContext<ServicesContextType>(
  {} as ServicesContextType
);

export const ServicesProvider = ({ children }: { children: ReactNode }) => {
  const { services } = useFetchServices();
  return (
    <ServicesContext.Provider
      value={{
        services
      }}
    >
      {children}
    </ServicesContext.Provider>
  );
};

export const useServicesContext = () => {
  return useContext(ServicesContext);
};
