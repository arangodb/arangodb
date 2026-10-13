import "leaflet";

declare module "leaflet" {
  interface MapOptions {
    gestureHandling?: boolean;
  }
}

declare module "leaflet-gesture-handling" {
  const GestureHandling: unknown;
  export default GestureHandling;
}
