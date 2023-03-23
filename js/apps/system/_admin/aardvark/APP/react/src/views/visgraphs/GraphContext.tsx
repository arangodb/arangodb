import { useDisclosure } from "@chakra-ui/react";
import { ArangojsResponse } from "arangojs/lib/request.node";
import React, {
  createContext,
  ReactNode,
  useCallback,
  useContext,
  useState
} from "react";
import useSWR from "swr";
import { Network } from "vis-network";
import { getRouteForDB } from "../../utils/arangoClient";
import { UrlParametersContext } from "./url-parameters-context";
import URLPARAMETERS from "./UrlParameters";
import { VisGraphData } from "./VisGraphData.types";

type GraphContextType = {
  graphData: VisGraphData | undefined;
  graphName: string;
  network?: Network;
  isSettingsOpen?: boolean;
  setNetwork: (network: Network) => void;
  onOpenSettings: () => void;
  onCloseSettings: () => void;
};
const GraphContext = createContext<GraphContextType>({
  graphName: ""
} as GraphContextType);

export const GraphContextProvider = ({ children }: { children: ReactNode }) => {
  const currentUrl = window.location.href;
  const graphName = currentUrl.substring(currentUrl.lastIndexOf("/") + 1);
  let [network, setNetwork] = useState<Network>();
  const fetchVisData = useCallback(() => {
    return getRouteForDB(window.frontendConfig.db, "_admin").get(
      `/aardvark/visgraph/${graphName}`
    );
  }, [graphName]);

  const { data } = useSWR<ArangojsResponse>("visData", () => fetchVisData());
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
        graphData: data && (data.body as VisGraphData),
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
