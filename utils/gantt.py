#!/usr/bin/python3
from pathlib import Path
import sys
from dash import Dash, html, dash_table, dcc, callback, Output, Input
import plotly.express as px
#import pandas as pd
import polars as pd
import plotly.graph_objects as go
from plotly.subplots import make_subplots
from datetime import datetime

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

DATE_FORMAT = '%Y-%m-%d %H:%M:%S.%f'

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
external_stylesheets = ['https://codepen.io/chriddyp/pen/bWLwgP.css']

time_range = [
    df_load["Date"][0],
    df_load["Date"][len(df_load)-1]
]
styles = {
    'pre': {
        'border': 'thin lightgrey solid',
        'overflowX': 'scroll'
    }
}

RESOLUTION=60
app = Dash(__name__, external_stylesheets=external_stylesheets)

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
    html.Div(className='row', children=[
        html.Div([
            html.Pre(id='relayout-data', style=styles['pre']),
        ], className='three columns')
    ])
]

@app.callback(
    Output("load-graph", "figure"),
    Input("loadgraph-checklist", "value"))
def update_line_chart(gauge_select):
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

    fig.update_layout(
        xaxis=dict(
            rangeselector=dict(
                buttons=list([
                    dict(count=1,
                         label="1m",
                         step="month",
                         stepmode="backward"),
                    dict(count=6,
                         label="6m",
                         step="month",
                         stepmode="backward"),
                    dict(count=1,
                         label="YTD",
                         step="year",
                         stepmode="todate"),
                    dict(count=1,
                         label="1y",
                         step="year",
                         stepmode="backward"),
                    dict(step="all")
                ])
            ),
            rangeslider=dict(
                visible=True
            ),
            type="date"
        )
    )

    return fig

@app.callback(
    Output("gantt-chart", "figure"),
    Input('load-graph', 'relayoutData'))
def update_line_chart(relayoutData):
    date_range = []
    if (relayoutData and
        not 'autosize' in relayoutData and
        not 'xaxis.autorange' in relayoutData):
        if "xaxis.range" in relayoutData:
            date_range = [
                datetime.strptime(relayoutData['xaxis.range'][0], DATE_FORMAT),
                datetime.strptime(relayoutData['xaxis.range'][1], DATE_FORMAT),
            ]
        if "xaxis.range[0]" in relayoutData:
            date_range = [
                datetime.strptime(relayoutData['xaxis.range[0]'], DATE_FORMAT),
                datetime.strptime(relayoutData['xaxis.range[1]'], DATE_FORMAT),
            ]
        fig_gantt = px.timeline(df_jobs.filter(
            (df_jobs["Start"] <= date_range[0]) & (df_jobs["Finish"] >= date_range[1]) |
            (df_jobs["Start"] >= date_range[1]) & (df_jobs["Finish"] <  date_range[1]) |
            (df_jobs["Start"] >  date_range[0]) & (df_jobs["Finish"] <= date_range[0]) |
            (df_jobs["Start"] >  date_range[0]) & (df_jobs["Finish"] <  date_range[1]))
        , x_start="Start", x_end="Finish", y="Task")
    else:
        fig_gantt = px.timeline(df_jobs, x_start="Start", x_end="Finish", y="Task")
    fig_gantt.update_yaxes(autorange="reversed") # otherwise tasks are listed from the bottom
    return fig_gantt


if __name__ == '__main__':
    app.run(debug=True)
