// `arc` is ESM-only and declares its types through the "exports" field, which
// this project's moduleResolution ("node") does not read. Only the surface the
// geo map uses is declared here.
declare module "arc" {
  import type { Feature } from "geojson";

  export class GreatCircle {
    constructor(
      start: { x: number; y: number },
      end: { x: number; y: number },
      properties?: Record<string, unknown>
    );
    interpolate(fraction: number): [number, number];
    Arc(points?: number): { json(): Feature };
  }
}
