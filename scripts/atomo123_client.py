#!/usr/bin/env python3
"""
atomo123_client.py -- reusable Python client for scripting Atomo123
(the native Haiku spreadsheet app) through Pippo's local MCP server.

Pippo must already be running on this machine (it listens on
127.0.0.1:2607). Atomo123 itself does not need to be running ahead of
time: open_file() launches it automatically, same as double-clicking
a file in Tracker.

Quick start:

    from atomo123_client import Atomo123

    app = Atomo123()
    app.open_file("/boot/home/Desktop/report.xlsx")
    app.set_cell("A1", "Hello")
    app.set_cell("B1", "=A1")
    print(app.get_cell("B1"))
    app.save_file("/boot/home/Desktop/report.xlsx")

All methods raise Atomo123Error on failure (bad address, Atomo123 not
running, Pippo unreachable, etc.) -- there are no silent None returns
to check for.
"""
from __future__ import annotations

import base64
import json
import time
import urllib.error
import urllib.request
from typing import Any, Optional


class Atomo123Error(RuntimeError):
    """Raised when an MCP tool call fails or Atomo123 itself reports an error."""


class Atomo123:
    def __init__(self, mcp_url: str = "http://127.0.0.1:2607/mcp",
                 timeout: float = 15.0, retries: int = 3):
        self.mcp_url = mcp_url
        self.timeout = timeout
        self.retries = retries
        self._request_id = 0

    # ---- low-level MCP plumbing -------------------------------------

    def _call(self, tool: str, **args: Any) -> Any:
        """Call a Pippo MCP tool and return its decoded result payload."""
        self._request_id += 1
        payload = {
            "jsonrpc": "2.0",
            "id": self._request_id,
            "method": "tools/call",
            "params": {"name": tool, "arguments": args},
        }
        data = json.dumps(payload).encode("utf-8")

        last_err: Optional[Exception] = None
        for _ in range(self.retries):
            try:
                req = urllib.request.Request(
                    self.mcp_url, data=data,
                    headers={"Content-Type": "application/json"},
                    method="POST",
                )
                with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                    envelope = json.loads(resp.read().decode("utf-8"))
                return self._unwrap(tool, envelope)
            except (urllib.error.URLError, json.JSONDecodeError, KeyError) as e:
                last_err = e
                time.sleep(0.3)
        raise Atomo123Error(
            f"{tool}: MCP call failed after {self.retries} attempts: {last_err}")

    @staticmethod
    def _unwrap(tool: str, envelope: dict) -> Any:
        if "error" in envelope:
            raise Atomo123Error(f"{tool}: {envelope['error']}")

        result = envelope["result"]
        content = result["content"][0]
        text = content["text"]
        try:
            inner = json.loads(text)
        except json.JSONDecodeError:
            inner = text  # plain string payload, not JSON-wrapped

        if result.get("isError"):
            raise Atomo123Error(f"{tool}: {inner}")
        if isinstance(inner, dict) and inner.get("status") == "error":
            raise Atomo123Error(f"{tool}: {inner.get('error', inner)}")

        # Every Atomo123-suite tool wraps its real payload one level
        # deeper: {"status": "ok", "result": {...}}. Plain tools (e.g.
        # list_windows) don't, so only unwrap when that shape is there.
        if isinstance(inner, dict) and "status" in inner and "result" in inner:
            return inner["result"]
        return inner

    @staticmethod
    def _as_list(value: Any) -> list:
        """BMessage->JSON serialization collapses a single repeated field
        to a bare value instead of a one-element array (see get_range)."""
        return value if isinstance(value, list) else [value]

    # ---- spreadsheet cell/range access -------------------------------

    def get_cell(self, address: str) -> str:
        """Read the computed value of one cell, e.g. "A1"."""
        return self._call("get_cell", address=address)["value"]

    def set_cell(self, address: str, value: Any) -> None:
        """Write a value/formula into a cell. A formula must start with '='."""
        self._call("set_cell", address=address, value=str(value))

    def get_selection(self) -> str:
        """Return the active cell/range address, e.g. "A1" or "A1:B2"."""
        return self._call("get_selection")["selection"]

    def set_selection(self, address: str) -> None:
        """Move/extend the selection to a cell or range -- same as clicking it."""
        self._call("set_selection", address=address)

    def get_range(self, range_: str) -> list[list[str]]:
        """Read a rectangular range (e.g. "A1:C10") as rows of computed
        values. Capped at 2000 cells by Pippo itself."""
        payload = self._call("get_range", range=range_)
        rows = self._as_list(payload.get("rows", {}).get("row", []))
        return [self._as_list(row.get("cell", [])) for row in rows]

    # ---- file I/O -----------------------------------------------------

    def open_file(self, path: str) -> None:
        """Open a spreadsheet file (CSV/XLS/XLSX/ODS/native ASCD),
        launching Atomo123 first if it isn't already running."""
        self._call("open_file", path=path)

    def save_file(self, path: str) -> None:
        """Save the current document to path (format chosen from the
        extension: .csv/.xlsx/.ods, anything else stays native .ascd)."""
        self._call("save_file", path=path)

    # ---- formatting -----------------------------------------------------

    def set_cell_color(self, range_: str, color: str) -> None:
        """Background color: "R,G,B" (0-255 each) or a 6-digit hex string."""
        self._call("set_cell_color", range=range_, color=color)

    def set_column_width(self, range_: str, width: int) -> None:
        """Width in pixels, applied to every column spanned by range_."""
        self._call("set_column_width", range=range_, width=width)

    def set_cell_bold(self, range_: str, bold: bool = True) -> None:
        self._call("set_cell_bold", range=range_, bold=bold)

    def set_cell_border(self, range_: str, thickness: int) -> None:
        """Grid/box border on every cell in range_. thickness=0 removes it."""
        self._call("set_cell_border", range=range_, thickness=thickness)

    def set_number_format(self, range_: str, format_: str) -> None:
        """format_: one of general/fixed/currency/percent/scientific."""
        self._call("set_number_format", range=range_, format=format_)

    def set_wrap_text(self, range_: str, wrap: bool = True) -> None:
        self._call("set_wrap_text", range=range_, wrap=wrap)

    # ---- charts -----------------------------------------------------------

    def insert_chart(self, data_range: str, dest_cell: str, type_: str,
                      title: Optional[str] = None) -> None:
        """Insert a live-updating embedded chart.
        type_: bar/line/pie/area/scatter/combo/hbar.
        data_range: category column + one or more value columns, e.g. "A1:B5".
        dest_cell: top-left anchor cell for the chart, e.g. "D1"."""
        kwargs = {"dataRange": data_range, "destCell": dest_cell, "type": type_}
        if title is not None:
            kwargs["title"] = title
        self._call("insert_chart", **kwargs)

    # ---- goal seek ----------------------------------------------------------

    def goal_seek(self, set_cell: str, to_value: float, changing_cell: str) -> dict:
        """Vary changing_cell (secant-method search) until set_cell's
        formula reaches to_value -- same as Data > Goal Seek.
        set_cell must already contain a real formula.

        Returns {"converged": bool, "achieved": float}. A non-convergent
        search is NOT an error -- check result["converged"]; changing_cell
        is left unchanged when it's False."""
        return self._call("goal_seek", setCell=set_cell, toValue=to_value,
                           changingCell=changing_cell)

    # ---- window / app management --------------------------------------------

    def list_windows(self) -> list:
        return self._call("list_windows")

    def main_window(self) -> Optional[dict]:
        """The first Atomo123 document window found, or None if not running."""
        for w in self.list_windows():
            if w.get("app") == "application/x-vnd.Atomo-Atomo123":
                return w
        return None

    def quit(self) -> None:
        self._call("quit_app", app="application/x-vnd.Atomo-Atomo123")

    # ---- screenshots ----------------------------------------------------------

    def screenshot_region(self, left: int, top: int, right: int, bottom: int,
                           save_to: Optional[str] = None) -> bytes:
        """Capture a screen region. Returns PNG bytes; writes to save_to if given."""
        result = self._call("screenshot_region", left=left, top=top,
                             right=right, bottom=bottom)
        data = base64.b64decode(result["data"])
        if save_to:
            with open(save_to, "wb") as f:
                f.write(data)
        return data

    def screenshot_window(self, save_to: Optional[str] = None) -> bytes:
        """Capture the main Atomo123 window's current on-screen frame."""
        win = self.main_window()
        if win is None:
            raise Atomo123Error("No Atomo123 window found -- is it running?")
        frame = win["frame"]
        return self.screenshot_region(frame["left"], frame["top"],
                                       frame["right"], frame["bottom"],
                                       save_to=save_to)


if __name__ == "__main__":
    # Minimal smoke test: list windows, and if Atomo123 is running,
    # read back its active selection.
    app = Atomo123()
    print("Open windows:")
    for w in app.list_windows():
        print(f"  [{w['index']}] {w['app']}: {w['title']!r}")

    win = app.main_window()
    if win is not None:
        print("\nAtomo123 is running.")
        print("Active selection:", app.get_selection())
    else:
        print("\nAtomo123 is not running -- call app.open_file(...) to start it.")
