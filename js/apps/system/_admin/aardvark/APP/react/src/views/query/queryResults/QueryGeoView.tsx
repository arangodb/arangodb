import { Box } from "@chakra-ui/react";
import type { GeoJSON as GeoJSONUnionType } from "geojson";
import L, { Layer } from "leaflet";
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

  return (
    <Box height="500px">
      <MapContainer
        style={{
          height: "500px"
        }}
        scrollWheelZoom={false}
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
    if (markers.length > 0) {
      const featureGroup = new L.FeatureGroup(markers);
      const bounds = featureGroup.getBounds();
      map.fitBounds(bounds);
    }
  }, [markers, map]);
  React.useEffect(() => {
    if (isPolygonGeometry || isLineStringGeometry) {
      const geojson = (
        new GeodesicLine().fromGeoJson(geometry) as any as Layer
      ).addTo(map);
      setMarkers([geojson]);
    }
  }, [geometry, isPolygonGeometry, isLineStringGeometry, map]);

  if (isPointGeometry) {
    return (
      <GeoJSON
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
