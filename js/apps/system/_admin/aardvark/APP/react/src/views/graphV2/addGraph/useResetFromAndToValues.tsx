import { usePrevious } from "@chakra-ui/react";
import { useFormikContext } from "formik";
import { useEffect, useMemo, useState } from "react";
import { useFetchGraphs } from "../useFetchGraphs";
import { GeneralGraphCreateValues } from "./CreateGraph.types";
import _ from "lodash";

const useCollectionToFromAndToMap = () => {
  const { graphs } = useFetchGraphs();
  const collectionsToFromAndToMap = useMemo(() => {
    let collectionsToFromAndToMap = {} as {
      [key: string]: {
        from: string[];
        to: string[];
      };
    };
    graphs?.forEach(graph => {
      graph.edgeDefinitions.forEach(edgeDefinition => {
        collectionsToFromAndToMap[edgeDefinition.collection] = {
          from: edgeDefinition.from,
          to: edgeDefinition.to
        };
      });
    });

    return collectionsToFromAndToMap;
  }, [graphs]);
  return collectionsToFromAndToMap;
};
/**
 * if any collection is selected by the user, this resets the from/to if required
 */
export const useResetFromAndToValues = () => {
  const { values, setValues } = useFormikContext<GeneralGraphCreateValues>();
  const [isFromAndToDisabled, setIsFromAndToDisabled] = useState<{
    [key: number]: boolean;
  }>({});
  const edgeDefinitionsMap = useCollectionToFromAndToMap();
  const prevEdgeDefinitions = usePrevious(values.edgeDefinitions) as any as
    | typeof values.edgeDefinitions
    | undefined;
  const isAnyCollectionUpdated = _.isEqual(
    values.edgeDefinitions,
    prevEdgeDefinitions
  );
  useEffect(() => {
    const newEdgeDefinitions = values.edgeDefinitions.map(
      (edgeDefinition, index) => {
        const foundDefinition =
          edgeDefinitionsMap[edgeDefinition.collection as string];
        if (foundDefinition) {
          const { from, to } = foundDefinition;
          setIsFromAndToDisabled(currentIsFromAndToDisabled => {
            return {
              ...currentIsFromAndToDisabled,
              [index]: true
            };
          });
          return {
            ...edgeDefinition,
            from,
            to
          };
        }
        setIsFromAndToDisabled(currentIsFromAndToDisabled => {
          return {
            ...currentIsFromAndToDisabled,
            [index]: false
          };
        });
        return edgeDefinition;
      }
    );
    setValues({
      ...values,
      edgeDefinitions: newEdgeDefinitions
    });
    // disabled sicne we are only interested
    // in running this if a collection changes
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [edgeDefinitionsMap, isAnyCollectionUpdated]);
  return { isFromAndToDisabled };
};
