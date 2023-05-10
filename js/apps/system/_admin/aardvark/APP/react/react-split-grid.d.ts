
declare module "react-split-grid" {
	import React from "react"

	export interface TrackSize {
		[track: number]: number
	}

	export interface GridStyles {
		gridTemplateColumns?: string
		gridTemplateRows?: string
	}

	export interface RenderProps {
		getGridProps: () => {
			styles: GridStyles
		}
		getGutterProps: (
			direction: "row" | "column",
			track: number,
		) => {
			onMouseDown: (e: unknown) => void
			onTouchStart: (e: unknown) => void
		}
	}

	export type Render = (props: RenderProps) => React.Element | null

	export interface SplitGridProps extends GridStyles {
		minSize?: number
		columnMinSize?: number
		rowMinSize?: number
		columnMinSizes?: TrackSize
		rowMinSizes?: TrackSize
		snapOffset?: number
		columnSnapOffset?: number
		rowSnapOffset?: number
		dragInterval?: number
		columnDragInterval?: number
		rowDragInterval?: number
		cursor?: string
		columnCursor?: string
		rowCursor?: string
		onDrag?: (
			direction: "row" | "column",
			track: number,
			gridTemplateStyle: string,
		) => void
		onDragStart?: (direction: "row" | "column", track: number) => void
		onDragEnd?: (direction: "row" | "column", track: number) => void
		gridTemplateColumns?: string
		gridTemplateRows?: string
	}

	export interface ChildrenPattern {
		children: Render
	}

	export interface RenderPattern {
		render: Render
	}

	export interface ComponentPattern {
		component: React.Node
	}

	declare const Split: React.FunctionComponent<SplitGridProps &
		(ChildrenPattern | RenderPattern | ComponentPattern)>

	export default Split
}
