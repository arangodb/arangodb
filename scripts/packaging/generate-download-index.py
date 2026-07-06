#!/usr/bin/env python3
"""Generate per-directory index.html files for the download site.

Replacement for the file-browser tooling oskar used over the NAS staging
tree: since packages are now published straight to GCS, the directory tree
is reconstructed from a `gsutil ls -l -r <root>` listing instead of a local
filesystem walk, and one index.html is emitted for every directory level.

Usage:
  gsutil ls -l -r gs://download.arangodb.com/nightly > listing.txt
  generate-download-index.py --root gs://download.arangodb.com/nightly \
      --prefix /nightly --listing listing.txt --out ./index-out
  # then upload every generated index.html to the same location in GCS

The listing lines that matter look like:
     123456  2026-07-02T01:23:45Z  gs://bucket/nightly/3.12/Linux/x86_64/file
Anything else (directory headers, TOTAL line) is ignored.
"""

import argparse
import html
import re
import sys
from pathlib import Path

LISTING_RE = re.compile(r"^\s*(\d+)\s+(\S+)\s+(gs://\S+)$")


def human_size(size: int) -> str:
    value = float(size)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if value < 1024 or unit == "TiB":
            if unit == "B":
                return f"{int(value)} {unit}"
            return f"{value:.1f} {unit}"
        value /= 1024
    return f"{int(size)} B"


def parse_listing(listing_path: str, root: str):
    """Return {relative_dir: {"dirs": set(), "files": {name: (size, date)}}}."""
    tree = {"": {"dirs": set(), "files": {}}}
    root = root.rstrip("/")
    with open(listing_path, encoding="utf-8") as fh:
        for line in fh:
            match = LISTING_RE.match(line.rstrip("\n"))
            if not match:
                continue
            size, date, url = match.groups()
            if not url.startswith(root + "/"):
                continue
            rel = url[len(root) + 1:]
            if not rel or rel.endswith("/"):
                continue
            parts = rel.split("/")
            name = parts[-1]
            if name == "index.html":
                continue
            # register all parent directories
            for i in range(len(parts) - 1):
                parent = "/".join(parts[:i])
                child = parts[i]
                tree.setdefault(parent, {"dirs": set(), "files": {}})["dirs"].add(child)
                tree.setdefault("/".join(parts[:i + 1]), {"dirs": set(), "files": {}})
            dir_key = "/".join(parts[:-1])
            date = date.replace("T", " ").replace("Z", "")
            tree.setdefault(dir_key, {"dirs": set(), "files": {}})["files"][name] = (int(size), date)
    return tree


PAGE = """<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Index of {title}</title>
<style>
body {{ font-family: monospace; margin: 2em; }}
table {{ border-collapse: collapse; }}
th, td {{ text-align: left; padding: 0.15em 1.5em 0.15em 0; }}
th {{ border-bottom: 1px solid #aaa; }}
td.size {{ text-align: right; }}
</style>
</head>
<body>
<h1>Index of {title}</h1>
<table>
<tr><th>Name</th><th>Last modified</th><th class="size">Size</th></tr>
{rows}
</table>
</body>
</html>
"""


def render_dir(title: str, entry, is_root: bool) -> str:
    rows = []
    if not is_root:
        rows.append('<tr><td><a href="../index.html">../</a></td><td></td><td class="size">-</td></tr>')
    for d in sorted(entry["dirs"]):
        d_esc = html.escape(d, quote=True)
        rows.append(f'<tr><td><a href="{d_esc}/index.html">{html.escape(d)}/</a></td><td></td><td class="size">-</td></tr>')
    for name, (size, date) in sorted(entry["files"].items()):
        n_esc = html.escape(name, quote=True)
        rows.append(
            f'<tr><td><a href="{n_esc}">{html.escape(name)}</a></td>'
            f'<td>{html.escape(date)}</td><td class="size">{human_size(size)}</td></tr>'
        )
    return PAGE.format(title=html.escape(title), rows="\n".join(rows))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, help="gs:// URL the listing was taken from")
    parser.add_argument("--prefix", default="", help="display prefix for page titles, e.g. /nightly")
    parser.add_argument("--listing", required=True, help="output of: gsutil ls -l -r <root>")
    parser.add_argument("--out", required=True, help="directory to write index.html files into")
    args = parser.parse_args()

    tree = parse_listing(args.listing, args.root)
    out_root = Path(args.out)
    count = 0
    for rel, entry in tree.items():
        out_dir = out_root / rel if rel else out_root
        out_dir.mkdir(parents=True, exist_ok=True)
        title = f"{args.prefix}/{rel}".rstrip("/") or "/"
        (out_dir / "index.html").write_text(render_dir(title, entry, is_root=(rel == "")), encoding="utf-8")
        count += 1
    print(f"generated {count} index.html files under {out_root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
