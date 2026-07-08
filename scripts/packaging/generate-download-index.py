#!/usr/bin/env python3
"""Generate per-directory index.html files for the download site.

Replacement for the file-browser tooling oskar used over the NAS staging
tree (arangodb-helper/Static-HTML-file-browser-for-Dropbox): since packages
are now published straight to GCS, the directory tree is reconstructed from
a `gsutil ls -l -r <root>` listing instead of a local filesystem walk, and
one index.html is emitted for every directory level.

The emitted HTML replicates that tool's templates/template.html rendering
(with MONOSPACED_FONTS/SHOW_SERVER_INFO on and icons commented out, as
configured there), so the nightly pages keep the exact look of the rest of
download.arangodb.com. Differences to the original: directory sizes show
"-" instead of the filesystem block size, directory dates are the newest
date of any file below them, and dates stay UTC (the original added one
hour to fake local time).

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

# Matches the original tool's config.py (icons are commented out in the
# rendered pages, but the markup keeps the <!--...--> cells).
LINK_TO_ICONS = "https://dl.dropboxusercontent.com/u/31525733/icons"
SERVER_INFO = "https://download.arangodb.com at Port 443 (Official ArangoDB Repository)"
ICON_BY_EXTENSION = {
    ".txt": "text.gif",
    ".png": "image2.gif",
    ".jpg": "image2.gif",
    ".jpeg": "image2.gif",
    ".bmp": "image2.gif",
    ".gif": "image2.gif",
    ".doc": "doc.gif",
    ".htm": "link.gif",
    ".html": "link.gif",
    ".py": "python.gif",
    ".tex": "tex.gif",
    ".tar": "tar.gif",
}


def sizeof_fmt(filesize_in_bytes: int) -> str:
    """Byte-for-byte port of the original tool's sizeof_fmt."""
    num = float(filesize_in_bytes)
    for x in ["bytes", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB"]:
        if num < 1024.0 and x == "bytes":
            return str(int(num))
        elif num < 1024.0:
            return "{0:.2f}&nbsp;{1}".format(num, x)
        num /= 1024.0
    return str(filesize_in_bytes)


def icon_for(name: str) -> str:
    for extension, icon in ICON_BY_EXTENSION.items():
        if name.endswith(extension):
            return icon
    return "unknown.gif"


def format_date(iso_date: str) -> str:
    # 2026-07-02T01:23:45Z -> 2026-07-02&nbsp;01:23 (template date format)
    match = re.match(r"(\d{4}-\d{2}-\d{2})T(\d{2}:\d{2})", iso_date)
    if not match:
        return ""
    return f"{match.group(1)}&nbsp;{match.group(2)}"


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
            tree.setdefault(dir_key, {"dirs": set(), "files": {}})["files"][name] = (int(size), date)
    return tree


def latest_dates(tree):
    """Newest file date below each directory (the GCS substitute for the
    directory mtime the original tool showed)."""
    latest = {}
    for rel, entry in tree.items():
        for _, (_, date) in entry["files"].items():
            ancestors = [""]
            if rel:
                parts = rel.split("/")
                ancestors += ["/".join(parts[:i + 1]) for i in range(len(parts))]
            for ancestor in ancestors:
                if date > latest.get(ancestor, ""):
                    latest[ancestor] = date
    return latest


# Rendered markup of templates/template.html from
# arangodb-helper/Static-HTML-file-browser-for-Dropbox (icons commented
# out, monospaced fonts, server info shown). Plain token replacement
# instead of str.format because the GA snippet contains JS braces.
PAGE = """<!DOCTYPE html>
<html>
    <head>
	<!-- Global site tag (gtag.js) - Google Analytics -->
	<script async src="https://www.googletagmanager.com/gtag/js?id=UA-81053435-3"></script>
	<script>
	  window.dataLayer = window.dataLayer || [];
	  function gtag(){dataLayer.push(arguments);}
	  gtag('js', new Date());

	  gtag('config', 'UA-81053435-3');
	</script>
	<!-- END Global site tag (gtag.js) - Google Analytics -->
        <meta charset="utf-8"/>
        <meta name="robots" content="noindex" />
        <title>
            Index of /@INDEX_OF@
        </title>
        <style>
            .icon {
                width: 23px;
            }
            .name {
                width: 500px;
            }
            .date {
                width: 170px;
            }
            .size {
                width: 100px;
            }
            .open {
                width: 170px;
            }
            .description {
                width: 170px;
            }
            #top_hr {
                margin-top: 0px;
            }
            h1 {
                font-family: 'Times New Roman';
                font-weight: bold;
            }
            table {
                font-family : monospace;
                border-collapse: collapse;
            }
            td {
                padding-top: 0px;
                padding-bottom: 0px;
            }
            h1 {
                display: block;
                font-size: 32px;
                font-weight: bold;
            }
            body{
                color : black;
                background-color : #eaf1fd;
            }
        </style>
    </head>
    <body>
        <h1>
            Index of /@INDEX_OF@
        </h1>
        <table>
                <tr>
                  <!--<td class="icon"><img src="@ICONS@/blank.gif" alt="blank.gif"></td>-->
                    <td class="name">Name</td>
                    <td class="date">Last modified</td>
                    <td class="size">Size</td>
                    <td class="open">Open</td>
                    <td class="description">Description</td>
                </tr>
        </table>
        <hr id="top_hr">
        <table>
            <tbody>
@ROWS@
            </tbody>
        </table>
        <hr id="bottom_hr">
        <address>@SERVER_INFO@</address>
    </body>
</html>
"""

PARENT_ROW = """                <tr>
                  <!--<td class="icon"><img src="@ICONS@/back.gif" alt="back.gif"></td>-->
                    <td class="name"><a href="../index.html">Parent Directory</a></td>
                    <td class="date" />
                    <td class="size">-</td>
                    <td class="open" />
                    <td class="description" />
                </tr>"""

ENTRY_ROW = """                <tr>
                  <!--<td class="icon"><img src="@ICONS@/@ICON@" alt="@ICON@"></td>-->
                    <td class="name"><a href="@URL@">@NAME@</a></td>
                    <td class="date">@DATE@</td>
                    <td class="size">@SIZE@</td>
                    <td class="open">&nbsp;</td>
                    <td class="description"></td>
                </tr>"""


def entry_row(name: str, url: str, date: str, size: str, icon: str) -> str:
    return (
        ENTRY_ROW.replace("@ICONS@", LINK_TO_ICONS)
        .replace("@ICON@", icon)
        .replace("@URL@", html.escape(url, quote=True))
        .replace("@NAME@", html.escape(name))
        .replace("@DATE@", date)
        .replace("@SIZE@", size)
    )


def render_dir(index_of: str, entry, dir_dates, rel: str, is_root: bool) -> str:
    rows = []
    if not is_root:
        rows.append(PARENT_ROW.replace("@ICONS@", LINK_TO_ICONS))
    # like the original: directories first, then files, each sorted by name;
    # entries named "enterprise" and hidden entries are not listed
    for d in sorted(entry["dirs"]):
        if d == "enterprise" or d.startswith("."):
            continue
        child = f"{rel}/{d}".lstrip("/")
        rows.append(entry_row(
            name=d,
            url=f"{d}/index.html",
            date=format_date(dir_dates.get(child, "")),
            size="-",
            icon="folder.gif",
        ))
    for name, (size, date) in sorted(entry["files"].items()):
        if name == "enterprise" or name.startswith("."):
            continue
        rows.append(entry_row(
            name=name,
            url=name,
            date=format_date(date),
            size=sizeof_fmt(size),
            icon=icon_for(name),
        ))
    return (
        PAGE.replace("@INDEX_OF@", html.escape(index_of))
        .replace("@ICONS@", LINK_TO_ICONS)
        .replace("@SERVER_INFO@", SERVER_INFO)
        .replace("@ROWS@", "\n".join(rows))
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, help="gs:// URL the listing was taken from")
    parser.add_argument("--prefix", default="", help="display prefix for page titles, e.g. /nightly")
    parser.add_argument("--listing", required=True, help="output of: gsutil ls -l -r <root>")
    parser.add_argument("--out", required=True, help="directory to write index.html files into")
    args = parser.parse_args()

    tree = parse_listing(args.listing, args.root)
    dir_dates = latest_dates(tree)
    out_root = Path(args.out)
    count = 0
    for rel, entry in tree.items():
        out_dir = out_root / rel if rel else out_root
        out_dir.mkdir(parents=True, exist_ok=True)
        index_of = f"{args.prefix}/{rel}".strip("/")
        (out_dir / "index.html").write_text(
            render_dir(index_of, entry, dir_dates, rel, is_root=(rel == "")),
            encoding="utf-8",
        )
        count += 1
    print(f"generated {count} index.html files under {out_root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
