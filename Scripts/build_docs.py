"""Doc autogen for BobBot.

Reads the runtime tool manifest (`<ProjectRoot>/Saved/BobBot/.tool_manifest.json`,
written by the bridge at startup) and rewrites the autogen blocks inside:

  - Plugins/BobBot/CLAUDE.md
  - Plugins/BobBot/README.md
  - Plugins/BobBot/Docs/ToolReference.md

Each file has a single `<!-- AUTOGEN:TOOLS START --> ... <!-- AUTOGEN:TOOLS END -->`
marker pair. This script replaces the content between the markers; everything
outside is preserved.

Usage:
  python Scripts/build_docs.py            # rewrite docs in-place
  python Scripts/build_docs.py --check    # exit non-zero if the regenerated
                                          # content would differ (CI gate)

The script never invents content. If the manifest is missing or empty, it
writes a single line "No manifest found — restart the BobBot bridge." inside
each marker block so the failure is visible.
"""

import argparse
import json
import os
import re
import sys

PLUGIN_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROJECT_ROOT = os.environ.get("BOB_PROJECT_ROOT", "") or os.path.dirname(os.path.dirname(PLUGIN_ROOT))
MANIFEST_PATH = os.path.join(PROJECT_ROOT, "Saved", "BobBot", ".tool_manifest.json")

CLAUDE_MD = os.path.join(PLUGIN_ROOT, "CLAUDE.md")
README_MD = os.path.join(PLUGIN_ROOT, "README.md")
TOOLREF_MD = os.path.join(PLUGIN_ROOT, "Docs", "ToolReference.md")

START = "<!-- AUTOGEN:TOOLS START -->"
END = "<!-- AUTOGEN:TOOLS END -->"


def load_manifest():
    if not os.path.isfile(MANIFEST_PATH):
        return None
    try:
        with open(MANIFEST_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, ValueError):
        return None
    if not isinstance(data, list):
        return None
    return data


def group_by_category(tools):
    cats = {}
    for t in tools:
        cats.setdefault(t.get("category", "Uncategorized"), []).append(t)
    for v in cats.values():
        v.sort(key=lambda t: t["name"])
    return dict(sorted(cats.items()))


def render_claude_md_block(grouped, total):
    lines = [
        f"## Tools ({total})",
        "",
        "> **Live catalog:** call `list_tools` (optionally `list_tools(category=\"...\")`) for the runtime-authoritative list. The grouped lists below are auto-generated from the same registry — edit `bob_tool(...)` decorators in `Scripts/tools/*.py` to change them. Hand edits inside the AUTOGEN markers will be overwritten.",
        "",
    ]
    for cat, tools in grouped.items():
        bullet = []
        for t in tools:
            params = ", ".join(t.get("params", []))
            entry = f"`{t['name']}`" + (f" ({params})" if params else "")
            bullet.append(entry)
        lines.append(f"**{cat}:** {', '.join(bullet)}")
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def render_readme_md_block(grouped, total):
    cats = list(grouped.keys())
    return (
        f"BobBot has {total} MCP tools across {len(cats)} categories: "
        f"{', '.join(cats)}.\n"
    )


def _flatten_first_sentence(doc, max_chars=240):
    """Collapse a multi-line docstring into a single self-contained sentence.

    Joins lines, normalizes whitespace, and returns text up to (and including)
    the first sentence-ending period. Falls back to the first ~max_chars when
    no period is present so table cells stay on one line and never end on a
    dangling comma.
    """
    if not doc:
        return ""
    flat = " ".join(doc.split())
    if not flat:
        return ""
    # Find first ". " that ends a sentence (not "e.g.", "i.e.", or decimals).
    end = -1
    i = 0
    while i < len(flat):
        i = flat.find(". ", i)
        if i < 0:
            break
        # Skip "e.g.", "i.e."
        prev2 = flat[max(0, i - 3):i + 1].lower()
        if prev2.endswith("e.g.") or prev2.endswith("i.e."):
            i += 2
            continue
        # Skip decimals like "1.5".
        if i > 0 and flat[i - 1].isdigit() and i + 2 < len(flat) and flat[i + 2].isdigit():
            i += 2
            continue
        end = i + 1  # include the period
        break
    if end > 0:
        return flat[:end]
    if flat.endswith(".") and len(flat) <= max_chars:
        return flat
    if len(flat) <= max_chars:
        return flat
    return flat[:max_chars].rstrip(", ") + "…"


def render_toolref_md_block(grouped, total):
    lines = [
        f"BobBot has {total} MCP tools organized by category. Auto-generated from the tool registry — edit `bob_tool(...)` decorators in `Scripts/tools/*.py` to change.",
        "",
        "> **Live catalog:** call `list_tools` for the runtime-authoritative list.",
        "",
    ]
    for cat, tools in grouped.items():
        lines.append(f"## {cat} ({len(tools)})")
        lines.append("")
        lines.append("| Tool | Parameters | Output | Timeout | What it does |")
        lines.append("|------|-----------|--------|---------|-------------|")
        for t in tools:
            params = ", ".join(t.get("params", [])) or "—"
            doc_text = _flatten_first_sentence(t.get("doc") or "").replace("|", "\\|")
            lines.append(
                "| `{name}` | `{params}` | {kind} | {timeout}s | {doc} |".format(
                    name=t["name"],
                    params=params,
                    kind=t.get("output_kind", "small"),
                    timeout=t.get("default_timeout", 60),
                    doc=doc_text,
                )
            )
        lines.append("")
    return "\n".join(lines).rstrip() + "\n"


def replace_block(text, new_inner):
    """Replace content between AUTOGEN markers. Insert markers + content if absent."""
    pattern = re.compile(re.escape(START) + r"\s*\n.*?\n\s*" + re.escape(END), re.DOTALL)
    block = f"{START}\n\n{new_inner}\n{END}"
    if pattern.search(text):
        return pattern.sub(block, text)
    return text.rstrip() + "\n\n" + block + "\n"


def update_file(path, render_inner):
    if not os.path.isfile(path):
        print(f"  SKIP missing {path}", file=sys.stderr)
        return None
    with open(path, "r", encoding="utf-8") as f:
        original = f.read()
    new_inner = render_inner()
    updated = replace_block(original, new_inner)
    return original, updated


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--check", action="store_true", help="exit non-zero if regenerated content would differ")
    args = parser.parse_args()

    tools = load_manifest()
    if tools is None:
        warning = "No manifest found at {}.\nRestart the BobBot bridge to regenerate.\n".format(MANIFEST_PATH)
        renderers = {
            CLAUDE_MD: lambda: warning,
            README_MD: lambda: warning,
            TOOLREF_MD: lambda: warning,
        }
    else:
        grouped = group_by_category(tools)
        total = len(tools)
        renderers = {
            CLAUDE_MD: lambda: render_claude_md_block(grouped, total),
            README_MD: lambda: render_readme_md_block(grouped, total),
            TOOLREF_MD: lambda: render_toolref_md_block(grouped, total),
        }

    drift = False
    for path, render in renderers.items():
        result = update_file(path, render)
        if result is None:
            continue
        original, updated = result
        if original == updated:
            print(f"  OK  {path}")
            continue
        if args.check:
            drift = True
            print(f"  DIFF {path}", file=sys.stderr)
            continue
        with open(path, "w", encoding="utf-8") as f:
            f.write(updated)
        print(f"  WRITE {path}")

    if args.check and drift:
        print("\nDoc drift detected. Run `python Scripts/build_docs.py` and commit the result.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
