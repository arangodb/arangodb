import { ViewDescription } from "./View.types";
import React, { createContext, ReactNode, useContext } from "react";
import { useFetchViews } from "./useFetchViews";

type ViewsContextType = {
  views: ViewDescription[] | undefined;
  isFormDisabled: boolean;
};
const ViewsContext = createContext<ViewsContextType>({} as ViewsContextType);

export const ViewsProvider = ({
  children,
  isFormDisabled
}: {
  children: ReactNode;
  isFormDisabled?: boolean;
}) => {
  const { views } = useFetchViews();
  return (
    <ViewsContext.Provider
      value={{
        views,
        isFormDisabled: !!isFormDisabled
      }}
    >
      {children}
    </ViewsContext.Provider>
  );
};

export const useViewsContext = () => {
  return useContext(ViewsContext);
};
