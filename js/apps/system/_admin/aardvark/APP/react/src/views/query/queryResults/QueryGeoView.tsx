import { Box } from "@chakra-ui/react";
import type { GeoJSON as GeoJSONUnionType } from "geojson";
import L, { Layer } from "leaflet";
import GestureHandling from "leaflet-gesture-handling";
import "leaflet-gesture-handling/dist/leaflet-gesture-handling.css";
import { GeodesicLine } from "leaflet.geodesic";
import React from "react";
import { GeoJSON, MapContainer, TileLayer, useMap } from "react-leaflet";
import { QueryResultType } from "../QueryContextProvider";

type NestedGeometryType = {
  geometry: GeoJSONUnionType;
};
type GeometryResultType = GeoJSONUnionType[] | NestedGeometryType[];
const geojsonMarkerOptions = {
  radius: 8,
  fillColor: "#2ecc71",
  color: "white",
  weight: 1,
  opacity: 1,
  fillOpacity: 0.64
};
export const QueryGeoView = ({
  queryResult
}: {
  queryResult: QueryResultType<GeometryResultType>;
}) => {
  // const geometry = queryResult.result?.hasOwnProperty("geometry").map
  //   ? (queryResult.result as NestedGeometryType)?.geometry
  //   : (queryResult.result as GeoJSONType);
  const newResult = queryResult.result?.map(item => {
    if (item.hasOwnProperty("geometry")) {
      return (item as NestedGeometryType).geometry;
    }
    return item as GeoJSONUnionType;
  });
  if (!newResult) {
    return null;
  }
  L.Map.addInitHook("addHandler", "gestureHandling", GestureHandling);
  return (
    <Box height="500px">
      <MapContainer
        style={{
          height: "500px"
        }}
        scrollWheelZoom={false}
        // @ts-ignore
        gestureHandling={true}
      >
        <MapInner geometryResult={newResult} />
      </MapContainer>
    </Box>
  );
};

const MapInner = ({
  geometryResult
}: {
  geometryResult: GeoJSONUnionType[];
}) => {
  const areAllPoints = geometryResult.every(
    geometry => geometry.type === "Point" || geometry.type === "MultiPoint"
  );
  const map = useMap();
  React.useEffect(() => {
    if (areAllPoints) {
      const markers = geometryResult
        .map(geometry => {
          if (geometry.type === "Point" || geometry.type === "MultiPoint") {
            return new L.CircleMarker(
              L.latLng(
                (geometry.coordinates as any)[1],
                (geometry.coordinates as any)[0]
              )
            );
          }
          return null;
        })
        .filter(Boolean);
      const featureGroup = new L.FeatureGroup(markers as any);
      const bounds = featureGroup.getBounds();
      map.fitBounds(bounds);
    }
  }, [areAllPoints, geometryResult, map]);

  return (
    <>
      <TileLayer
        attribution={
          '&copy; <a href="https://www.openstreetmap.org/copyright">OpenStreetMap</a> contributors'
        }
        url="https://tile.openstreetmap.org/{z}/{x}/{y}.png"
      />
      {geometryResult.map((geometry, index) => {
        return <SingleGeometry key={index} geometry={geometry} />;
      })}
    </>
  );
};

const SingleGeometry = ({ geometry }: { geometry: GeoJSONUnionType }) => {
  const map = useMap();

  const [markers, setMarkers] = React.useState<any[]>([]);
  const isPointGeometry =
    geometry.type === "Point" || geometry.type === "MultiPoint";
  const isPolygonGeometry =
    geometry.type === "Polygon" || geometry.type === "MultiPolygon";
  const isLineStringGeometry =
    geometry.type === "MultiLineString" || geometry.type === "LineString";
  React.useEffect(() => {
    if (markers.length > 0 && !isPointGeometry) {
      const featureGroup = new L.FeatureGroup(markers);
      const bounds = featureGroup.getBounds();
      map.fitBounds(bounds);
    }
  }, [markers, map, isPointGeometry]);
  React.useEffect(() => {
    if (isPolygonGeometry || isLineStringGeometry) {
      try {
        const geojson = (
          new GeodesicLine().fromGeoJson(geometry) as any as Layer
        ).addTo(map);
        setMarkers([geojson]);
      } catch (ignore) {
        // ignore error as the tab will not be displayed after first render
      }
    }
  }, [geometry, isPolygonGeometry, isLineStringGeometry, map]);
  if (isPointGeometry) {
    return (
      <GeoJSON
        onEachFeature={(feature, layer) => {
          // create a HTML element for the popup
          const element = document.createElement("pre");
          element.appendChild(
            document.createTextNode(JSON.stringify(feature, null, 2))
          );
          layer.bindPopup(element, {
            maxWidth: 300,
            minWidth: 300,
            maxHeight: 250
          });
        }}
        data={geometry}
        pointToLayer={(_feature, latlng) => {
          const marker = L.circleMarker(latlng, geojsonMarkerOptions);
          setMarkers(markers => [...markers, marker]);
          return marker;
        }}
      />
    );
  }
  return null;
};
