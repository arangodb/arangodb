from argparse import ArgumentParser


def arguments():
    parser = ArgumentParser()
    parser.add_argument('--mode', type=str, choices=['cluster', 'single'], help="Run in cluster mode or single mode")
    parser.add_argument('--init', type=str, required=False, choices=['true', 'false'],
                        help="Initialize the database or not")
    parser.add_argument('--startupParameters', type=str, required=False,
                        help="Additional startup parameters for ArangoDB, format: JSON object string")
    return parser.parse_args()
