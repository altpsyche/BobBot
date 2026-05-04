"""read_overflow — fetch slices of an envelope spilled to disk.

When a tool's serialized envelope exceeds its inline byte budget, the full
envelope is written to `<BOB_PROJECT_ROOT>/Saved/BobBot/output_overflow/<id>.json`
and the inline reply carries `spill_path` + `meta.truncated=true`. The agent
fetches the full payload via this tool, which paginates by byte offset so a
single fetch never exceeds its own envelope budget.

Stop condition: when `data.eof = true`, you've read the whole file.
"""

import json
import os

from _registry import bob_tool
from _common import envelope


def register(mcp, send_fn):

    @mcp.tool()
    @bob_tool(category="Meta", output_kind="large", default_timeout=10)
    def read_overflow(spill_path: str, offset: int = 0, max_bytes: int = 8000):
        """Read a slice of a spilled envelope file.

        Args:
            spill_path: Absolute path returned by an earlier tool's
                        envelope.spill_path field.
            offset:     Byte offset into the spilled JSON file. Default 0.
            max_bytes:  Maximum bytes to return in this call. Default 8000.

        Returns envelope with data = {chunk, eof, size, offset, next_offset}.
        """
        if not spill_path:
            return envelope(summary="ERROR: spill_path required", data=None, ok=False, error="missing_arg")
        if not os.path.isfile(spill_path):
            return envelope(summary=f"ERROR: file not found: {spill_path}", data=None, ok=False, error="not_found")

        try:
            size = os.path.getsize(spill_path)
        except OSError as e:
            return envelope(summary=f"ERROR: stat failed: {e}", data=None, ok=False, error=str(e))

        if offset < 0:
            offset = 0
        if offset >= size:
            return envelope(
                summary=f"At EOF (offset={offset}, size={size})",
                data={"chunk": "", "eof": True, "size": size, "offset": offset, "next_offset": size},
            )
        max_bytes = max(1, min(int(max_bytes), 32 * 1024))

        try:
            with open(spill_path, "rb") as f:
                f.seek(offset)
                raw = f.read(max_bytes)
        except OSError as e:
            return envelope(summary=f"ERROR: read failed: {e}", data=None, ok=False, error=str(e))

        next_offset = offset + len(raw)
        eof = next_offset >= size
        try:
            chunk = raw.decode("utf-8")
        except UnicodeDecodeError:
            chunk = raw.decode("utf-8", errors="replace")
        summary = f"slice {offset}..{next_offset} of {size} bytes ({'EOF' if eof else 'more available'})"
        return envelope(
            summary=summary,
            data={
                "chunk": chunk,
                "eof": eof,
                "size": size,
                "offset": offset,
                "next_offset": next_offset,
            },
        )
