"""ComfyUI Cloud client for the PS3 Monopoly Party asset generator.

Mirrors the flow used by story-teller-visualizer's cloudUtils.ts:
  1. POST /api/prompt                      -> { prompt_id }
  2. WebSocket wss://cloud.comfy.org/ws    -> collect `executed` outputs, done on
     `execution_success` (the reliable path — see below)
  3. GET  /api/view?...                    -> 302 to a signed URL -> download bytes

  Fallback if the ws lib is missing or a frame is missed: GET /api/job/{id}/status
  until terminal, then GET /api/history_v2/{id} for the outputs.

IMPORTANT: for comfy.org API-node jobs (Grok / OpenAI GPT Image) the
/api/history_v2 `outputs` come back EMPTY even after the job reports success, so a
status-poll + history fetch fails ("no image output ... []"). story-teller works
around this with the WebSocket, which carries the output events directly — that's
why we listen on the ws first.

Auth is a single X-API-Key header (COMFY_CLOUD_API_KEY); the ws passes it as
`?token=`. Workflows that contain comfy.org API nodes also need the key echoed in
the top-level `extra_data.api_key_comfy_org` field.
"""

from __future__ import annotations

import json
import os
import time
import uuid
from dataclasses import dataclass
from typing import Any, Optional

import requests

try:
    import websocket  # websocket-client
except ImportError:  # optional; generate() falls back to history polling
    websocket = None

CLOUD_BASE_URL = "https://cloud.comfy.org"

_TERMINAL_OK = {"success", "completed"}
_TERMINAL_FAIL = {"failed", "cancelled", "error"}


class ComfyCloudError(RuntimeError):
    """Raised for any non-recoverable Cloud API failure."""


@dataclass
class CloudFile:
    filename: str
    subfolder: str
    type: str


class ComfyCloudClient:
    def __init__(self, api_key: Optional[str] = None, base_url: str = CLOUD_BASE_URL,
                 timeout: int = 60):
        key = api_key or os.environ.get("COMFY_CLOUD_API_KEY")
        if not key:
            raise ComfyCloudError(
                "COMFY_CLOUD_API_KEY is not set. Export it or pass --api-key.\n"
                "Get one at https://platform.comfy.org/login"
            )
        self.api_key = key
        self.base_url = base_url.rstrip("/")
        self.timeout = timeout

    # ── low-level helpers ────────────────────────────────────────────────
    def _headers(self, json: bool = True) -> dict[str, str]:
        h = {"X-API-Key": self.api_key}
        if json:
            h["Content-Type"] = "application/json"
        return h

    # ── public API ───────────────────────────────────────────────────────
    def submit(self, workflow: dict[str, Any], *, comfy_org_api_node: bool = True) -> str:
        """Queue a workflow; returns the prompt_id."""
        body: dict[str, Any] = {"prompt": workflow}
        if comfy_org_api_node:
            body["extra_data"] = {"api_key_comfy_org": self.api_key}

        resp = requests.post(
            f"{self.base_url}/api/prompt",
            headers=self._headers(),
            json=body,
            timeout=self.timeout,
        )
        if not resp.ok:
            raise ComfyCloudError(
                f"submit failed ({resp.status_code}): {resp.text[:500]}"
            )
        data = resp.json()
        prompt_id = data.get("prompt_id")
        if not prompt_id:
            raise ComfyCloudError(f"submit returned no prompt_id: {data}")
        return prompt_id

    def status(self, prompt_id: str) -> str:
        resp = requests.get(
            f"{self.base_url}/api/job/{prompt_id}/status",
            headers=self._headers(json=False),
            timeout=self.timeout,
        )
        if not resp.ok:
            raise ComfyCloudError(
                f"status check failed ({resp.status_code}): {resp.text[:200]}"
            )
        return (resp.json().get("status") or "").lower()

    def wait(self, prompt_id: str, *, poll_interval: float = 3.0,
             max_wait: float = 600.0, on_tick=None) -> None:
        """Block until the job reaches a terminal state or times out."""
        start = time.time()
        while True:
            elapsed = time.time() - start
            if elapsed > max_wait:
                raise ComfyCloudError(
                    f"job {prompt_id} did not finish within {max_wait:.0f}s"
                )
            st = self.status(prompt_id)
            if on_tick:
                on_tick(int(elapsed), st)
            if st in _TERMINAL_OK:
                return
            if st in _TERMINAL_FAIL:
                raise ComfyCloudError(f"job {prompt_id} ended with status '{st}'")
            time.sleep(poll_interval)

    def listen_ws(self, prompt_id: str, *, timeout: float = 600.0,
                  on_tick=None) -> Optional[dict[str, Any]]:
        """Collect a job's outputs over the cloud WebSocket (the reliable path —
        history_v2 lags/empties for API-node jobs, which is why story-teller moved
        to WS). Returns outputs keyed by node id, or None if the ws lib is missing.
        Filters by prompt_id since the socket streams events for all jobs."""
        if websocket is None:
            return None
        client_id = str(uuid.uuid4())
        url = f"wss://cloud.comfy.org/ws?clientId={client_id}&token={self.api_key}"
        outputs: dict[str, Any] = {}
        ws = websocket.create_connection(url, timeout=30)
        ws.settimeout(10)
        start = time.time()
        try:
            while time.time() - start < timeout:
                try:
                    raw = ws.recv()
                except websocket.WebSocketTimeoutException:
                    continue  # keep waiting until the overall timeout
                if isinstance(raw, (bytes, bytearray)):
                    # binary/gzip frames (progress previews) — skip, not JSON
                    if not raw or raw[0] == 0x1f or raw[0] < 0x20:
                        continue
                    raw = raw.decode("utf-8", "ignore")
                try:
                    msg = json.loads(raw)
                except (ValueError, TypeError):
                    continue
                data = msg.get("data") or {}
                if data.get("prompt_id") != prompt_id:
                    continue
                mtype = msg.get("type")
                if on_tick:
                    on_tick(int(time.time() - start), mtype or "")
                if mtype == "executed" and data.get("output"):
                    outputs[data.get("node")] = data["output"]
                elif mtype == "execution_success":
                    return outputs
                elif mtype == "execution_error":
                    raise ComfyCloudError(
                        f"job {prompt_id} execution error: "
                        f"{data.get('exception_message', 'unknown')}"
                    )
            return outputs  # timed out; caller falls back to history
        finally:
            try:
                ws.close()
            except Exception:
                pass

    def history_image(self, prompt_id: str, *, retries: int = 10,
                      retry_delay: float = 3.0) -> CloudFile:
        """Return the first image output; history may lag behind status."""
        last_outputs: dict[str, Any] = {}
        for attempt in range(1, retries + 1):
            resp = requests.get(
                f"{self.base_url}/api/history_v2/{prompt_id}",
                headers=self._headers(json=False),
                timeout=self.timeout,
            )
            if not resp.ok:
                raise ComfyCloudError(
                    f"history fetch failed ({resp.status_code}): {resp.text[:200]}"
                )
            last_outputs = resp.json().get("outputs", {}) or {}
            found = _extract_image(last_outputs)
            if found:
                return found
            if attempt < retries:
                time.sleep(retry_delay)
        raise ComfyCloudError(
            f"no image output for {prompt_id} after {retries} attempts: "
            f"{list(last_outputs.keys())}"
        )

    def download(self, f: CloudFile) -> bytes:
        params = {
            "filename": f.filename,
            "subfolder": f.subfolder or "",
            "type": f.type or "output",
        }
        resp = requests.get(
            f"{self.base_url}/api/view",
            headers=self._headers(json=False),
            params=params,
            allow_redirects=False,
            timeout=120,
        )
        if resp.status_code == 302:
            signed = resp.headers.get("location")
            if not signed:
                raise ComfyCloudError("download: 302 without location header")
            file_resp = requests.get(signed, timeout=120)
            if not file_resp.ok:
                raise ComfyCloudError(
                    f"signed download failed ({file_resp.status_code})"
                )
            return file_resp.content
        if resp.ok:
            return resp.content
        raise ComfyCloudError(f"download failed ({resp.status_code})")

    def generate(self, workflow: dict[str, Any], *, comfy_org_api_node: bool = True,
                 poll_interval: float = 3.0, max_wait: float = 600.0,
                 on_tick=None) -> bytes:
        """End-to-end: submit -> collect outputs (WebSocket, history fallback) ->
        download bytes.

        The WebSocket carries the `executed` outputs directly and is reliable; the
        old status-poll + history_v2 path returns empty `outputs` for comfy.org
        API-node jobs (Grok / OpenAI GPT Image) even after the job reports success,
        which is the failure you hit. We use WS first and only fall back to a
        (longer) history poll if the ws lib is unavailable or a frame was missed."""
        prompt_id = self.submit(workflow, comfy_org_api_node=comfy_org_api_node)

        cloud_file: Optional[CloudFile] = None
        outputs = self.listen_ws(prompt_id, timeout=max_wait, on_tick=on_tick)
        if outputs is not None:
            cloud_file = _extract_image(outputs)

        if cloud_file is None:
            # WS missing/missed the frame: make sure the job is terminal, then poll
            # history_v2 for a good while (it lags well past the "success" status).
            try:
                self.wait(prompt_id, poll_interval=poll_interval, max_wait=max_wait,
                          on_tick=on_tick)
            except ComfyCloudError:
                pass
            cloud_file = self.history_image(prompt_id, retries=15, retry_delay=4.0)

        return self.download(cloud_file)


def _extract_image(outputs: dict[str, Any]) -> Optional[CloudFile]:
    for node_output in outputs.values():
        images = (node_output or {}).get("images")
        if images:
            img = images[0]
            return CloudFile(
                filename=img.get("filename", ""),
                subfolder=img.get("subfolder", ""),
                type=img.get("type", "output"),
            )
    return None
