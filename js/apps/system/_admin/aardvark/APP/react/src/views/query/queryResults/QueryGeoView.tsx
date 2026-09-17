import { Box } from "@chakra-ui/react";
import type { Geometry } from "geojson";
import L from "leaflet";
import GestureHandling from "leaflet-gesture-handling";
import "leaflet-gesture-handling/dist/leaflet-gesture-handling.css";
import React from "react";
import { QueryResultType } from "../ArangoQuery.types";
import { toGreatCircle } from "./great-circle";

type NestedGeometryType = {
  geometry: Geometry;
};
type GeometryResultType = Geometry[] | NestedGeometryType[];
const geojsonMarkerOptions = {
  radius: 8,
  fillColor: "#2ecc71",
  color: "white",
  weight: 1,
  opacity: 1,
  fillOpacity: 0.64
};

L.Map.addInitHook("addHandler", "gestureHandling", GestureHandling);

export const QueryGeoView = ({
  queryResult
}: {
  queryResult: QueryResultType<GeometryResultType>;
}) => {
  const geometries = React.useMemo(
    () =>
      queryResult.result?.map(item =>
        Object.prototype.hasOwnProperty.call(item, "geometry")
          ? (item as NestedGeometryType).geometry
          : (item as Geometry)
      ) ?? [],
    [queryResult.result]
  );
  const [map, setMap] = React.useState<L.Map | null>(null);
  const mapRef = React.useRef<L.Map | null>(null);

  // ref callback so the map is created once the container mounts and torn
  // down when it unmounts.
  const containerRef = React.useCallback((node: HTMLDivElement | null) => {
    if (node && !mapRef.current) {
      const instance = L.map(node, {
        center: [0, 0],
        zoom: 2,
        scrollWheelZoom: false,
        gestureHandling: true
      });
      L.tileLayer("https://tile.openstreetmap.org/{z}/{x}/{y}.png", {
        attribution:
          '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
      }).addTo(instance);
      mapRef.current = instance;
      setMap(instance);
    } else if (!node && mapRef.current) {
      mapRef.current.remove();
      mapRef.current = null;
      setMap(null);
    }
  }, []);

  React.useEffect(() => {
    if (!map) {
      return;
    }
    const layers: L.Layer[] = [];
    const bounds = L.latLngBounds([]);
    geometries.forEach(geometry => {
      try {
        const layer = L.geoJSON(toGreatCircle(geometry), {
          pointToLayer: (_feature, latlng) =>
            L.circleMarker(latlng, geojsonMarkerOptions),
          onEachFeature: (feature, layerInstance) => {
            // create a HTML element for the popup, showing the geometry as the
            // document stores it rather than the interpolated arc
            const element = document.createElement("pre");
            element.appendChild(
              document.createTextNode(
                JSON.stringify({ ...feature, geometry }, null, 2)
              )
            );
            layerInstance.bindPopup(element, {
              maxWidth: 300,
              minWidth: 300,
              maxHeight: 250
            });
          }
        });
        layer.addTo(map);
        layers.push(layer);
        bounds.extend(layer.getBounds());
      } catch {
        // ignore error as the tab will not be displayed after first render
      }
    });
    if (bounds.isValid()) {
      map.fitBounds(bounds);
    }
    return () => {
      layers.forEach(layer => layer.remove());
    };
  }, [map, geometries]);

  if (!queryResult.result) {
    return null;
  }

  return (
    <Box height="500px">
      <div ref={containerRef} style={{ height: "500px" }} />
    </Box>
  );
};
