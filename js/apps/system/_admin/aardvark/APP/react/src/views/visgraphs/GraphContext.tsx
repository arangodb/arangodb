import React, {
  createContext,
  ReactNode,
  useCallback,
  useContext,
  useEffect,
  useState
} from "react";
import { getRouteForDB } from "../../utils/arangoClient";
import { loadingNode } from "./data";

const GraphContext = createContext<{ graphData: any; graphName: string }>({
  graphData: null,
  graphName: ""
});

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);

  let [visGraphData, setVisGraphData] = useState(loadingNode);

  const fetchVisData = useCallback(() => {
    getRouteForDB(window.frontendConfig.db, "_admin")
      .get(`/aardvark/visgraph/${graphName}`)
      .then(data => {
        setVisGraphData(data.body);
      })
      .catch(error => {
        window.arangoHelper.arangoError(
          "Graph",
          error.responseJSON.errorMessage
        );
        console.log(error);
      });
  }, [graphName]);

  useEffect(() => {
    fetchVisData();
  }, [fetchVisData]);

  return (
    <GraphContext.Provider value={{ graphData: visGraphData, graphName }}>
      {children}
    </GraphContext.Provider>
  );
};

export const useGraph = () => {
  return useContext(GraphContext);
};
