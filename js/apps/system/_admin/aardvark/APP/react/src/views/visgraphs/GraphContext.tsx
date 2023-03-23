import { useDisclosure } from "@chakra-ui/react";
import React, {
  createContext,
  ReactNode,
  useCallback,
  useContext,
  useEffect,
  useState
} from "react";
import { Network } from "vis-network";
import { getRouteForDB } from "../../utils/arangoClient";
import { loadingNode } from "./data";
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";
import { VisDataResponse, VisGraphData } from "./VisGraphData.types";

type GraphContextType = {
  graphData: VisGraphData;
  graphName: string;
  network?: Network;
  isSettingsOpen?: boolean;
  setNetwork: (network: Network) => void;
  onOpenSettings: () => void;
  onCloseSettings: () => void;
};
const GraphContext = createContext<GraphContextType>({
  graphData: {} as VisGraphData,
  graphName: ""
} as GraphContextType);

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);

  let [visGraphData, setVisGraphData] = useState(loadingNode as VisGraphData);
  let [network, setNetwork] = useState<Network>();

  const fetchVisData = useCallback(() => {
    getRouteForDB(window.frontendConfig.db, "_admin")
      .get(`/aardvark/visgraph/${graphName}`)
      .then((data: VisDataResponse) => {
        if (data.body) {
          setVisGraphData(data.body);
        }
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
  const {
    onOpen: onOpenSettings,
    onClose: onCloseSettings,
    isOpen: isSettingsOpen
  } = useDisclosure();
  const [urlParameters, setUrlParameters] = useState(URLPARAMETERS);
  return (
    <GraphContext.Provider
      value={{
        network,
        setNetwork,
        graphData: visGraphData,
        graphName,
        onOpenSettings,
        onCloseSettings,
        isSettingsOpen
      }}
    >
      <UrlParametersContext.Provider
        value={[urlParameters, setUrlParameters] as any}
      >
        {children}
      </UrlParametersContext.Provider>
    </GraphContext.Provider>
  );
};

export const useGraph = () => {
  return useContext(GraphContext);
};
