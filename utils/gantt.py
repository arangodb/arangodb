#!/usr/bin/python3
from pathlib import Path
import sys
from dash import Dash, html, dash_table, dcc, callback, Output, Input
import plotly.express as px
import pandas as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots

#print(dir(px))
#print(dir(dcc))
from tools.process_examine import (
    LOADS,
    MEMORY,
    TREE_TEXTS,
    PARSED_LINES,
    load_file,
    load_testing_js_mappings,
    build_process_tree,
    convert_pids_to_gantt,
    get_load,
    append_dict_to_df,
)

filter_str = None
if len(sys.argv) > 2:
    filter_str = sys.argv[2]
if len(sys.argv) > 3:
    start_pid = int(sys.argv[3])
main_log_file = Path(sys.argv[1])
PID_TO_FN = load_testing_js_mappings(main_log_file)
load_file(main_log_file, PID_TO_FN, filter_str)
jobs = convert_pids_to_gantt(PID_TO_FN)

df_load = get_load()
df_jobs = pd.DataFrame(jobs)

time_range = [
    df_load["Date"][0],
    df_load["Date"][len(df_load.index)-1]
]
print(time_range[1] - time_range[0])


app = Dash()
app.layout = [
    html.Div(children='Load to CI jobs'),
    html.Hr(),
    html.Div([
        html.H1(children='Testsystem Analyzer'),

        html.Div(children='''
            Testsuites and their run times
        '''),

        dcc.Graph(id='gantt-chart'),
    ]),
    html.Div([
        dcc.Graph(id='load-graph'),
        dcc.Checklist(
            id="loadgraph-checklist",
            options=["load", "netio"],
            value=[ "load", "netio"],
            inline=True
        )
    ]),
    html.Div([
        dcc.RangeSlider(0, 20, 1, value=[5, 15], id='date-range-slider'),
        html.Div(id='output-container-range-slider')
    ])
]

@callback(
    Output('output-container-range-slider', 'children'),
    Input('date-range-slider', 'value'))
def update_output(value):
    return 'You have selected "{}"'.format(value)

@app.callback(
    Output("load-graph", "figure"),
    Input('date-range-slider', 'value'),
    Input("loadgraph-checklist", "value"))
def update_line_chart(time_range, gauge_select):
    print(gauge_select)
    # Create figure with secondary y-axis
    fig = make_subplots(specs=[[{"secondary_y": True}]])

    # Add traces
    fig.add_trace(
        go.Scatter(x=df_load['Date'], y=df_load['load'], # replace with your own data source
        name="Load"), secondary_y=False,
    )

    if 'netio' in gauge_select:
        fig.add_trace(
            go.Scatter(x=df_load['Date'], y=df_load['netio'], # replace with your own data source
                       name="NetIO"), secondary_y=True
        )

    # Add figure title
    fig.update_layout(title_text="Load graph")

    # Set x-axis title
    fig.update_xaxes(title_text="Date")

    # Set y-axes titles
    fig.update_yaxes(
        title_text="<b>Load</b>",
        secondary_y=False)
    fig.update_yaxes(
        title_text="<b>Net IO LO</b>",
        secondary_y=True)
    return fig

@app.callback(
    Output("gantt-chart", "figure"),
    Input('date-range-slider', 'value'))
def update_line_chart(time_range):
    fig_gantt = px.timeline(df_jobs, x_start="Start", x_end="Finish", y="Task")
    fig_gantt.update_yaxes(autorange="reversed") # otherwise tasks are listed from the bottom
    return fig_gantt



if __name__ == '__main__':
    app.run(debug=True)
