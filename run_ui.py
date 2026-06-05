#!/usr/bin/env python3
"""Local presentation server for the order book UI.

The UI uses this server as a small market-data proxy. It tries Yahoo Finance's
chart endpoint first, then falls back to deterministic synthetic ticks so the
demo always works without API keys.
"""

from __future__ import annotations

import argparse
import json
import math
import random
import ssl
import time
import urllib.parse
import urllib.request
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path


ROOT = Path(__file__).resolve().parent
UI_DIR = ROOT / "ui"
SYMBOL_DEFAULTS = {
    "AAPL": 311.23,
    "TSLA": 347.80,
    "NVDA": 142.50,
}
CACHE: dict[str, tuple[float, bytes]] = {}
CACHE_SECONDS = 30


def synthetic_market(symbol: str, reason: str) -> dict:
    base = SYMBOL_DEFAULTS.get(symbol, 100.0)
    rng = random.Random(symbol)
    now = int(time.time())
    price = base
    points = []

    for idx in range(120):
        drift = math.sin(idx / 9.0) * 0.18
        shock = rng.uniform(-0.22, 0.22)
        price = max(1.0, price + drift + shock)
        points.append(
            {
                "time": now - (119 - idx) * 60,
                "price": round(price, 2),
                "volume": rng.randint(20_000, 140_000),
            }
        )

    return {
        "source": "synthetic fallback",
        "reason": reason,
        "symbol": symbol,
        "name": f"{symbol} simulated feed",
        "currency": "USD",
        "regularMarketPrice": points[-1]["price"],
        "previousClose": round(base, 2),
        "points": points,
        "fetchedAt": int(time.time()),
    }


def fetch_yahoo_chart(symbol: str) -> dict:
    query = urllib.parse.urlencode({"range": "1d", "interval": "1m"})
    url = f"https://query2.finance.yahoo.com/v8/finance/chart/{symbol}?{query}"
    request = urllib.request.Request(
        url,
        headers={
            "User-Agent": "Mozilla/5.0 (OrderBookClassDemo/1.0)",
            "Accept": "application/json",
        },
    )

    context = ssl._create_unverified_context()
    with urllib.request.urlopen(request, timeout=8, context=context) as response:
        raw = json.loads(response.read().decode("utf-8"))

    result = (raw.get("chart", {}).get("result") or [None])[0]
    if not result:
        error = raw.get("chart", {}).get("error") or "empty Yahoo response"
        raise ValueError(str(error))

    meta = result.get("meta", {})
    quote = (result.get("indicators", {}).get("quote") or [{}])[0]
    timestamps = result.get("timestamp") or []
    closes = quote.get("close") or []
    volumes = quote.get("volume") or []
    points = []

    for idx, ts in enumerate(timestamps):
        if idx >= len(closes) or closes[idx] is None:
            continue
        points.append(
            {
                "time": int(ts),
                "price": round(float(closes[idx]), 2),
                "volume": int(volumes[idx] or 0) if idx < len(volumes) else 0,
            }
        )

    if len(points) < 5:
        raise ValueError("Yahoo response did not include enough chart points")

    return {
        "source": "Yahoo Finance chart API",
        "symbol": meta.get("symbol", symbol),
        "name": meta.get("longName") or meta.get("shortName") or symbol,
        "currency": meta.get("currency", "USD"),
        "regularMarketPrice": round(float(meta.get("regularMarketPrice") or points[-1]["price"]), 2),
        "previousClose": round(float(meta.get("chartPreviousClose") or points[0]["price"]), 2),
        "points": points[-120:],
        "fetchedAt": int(time.time()),
    }


def market_payload(symbol: str) -> bytes:
    symbol = "".join(ch for ch in symbol.upper() if ch.isalnum() or ch in ".-")[:12] or "AAPL"
    cached = CACHE.get(symbol)
    if cached and time.time() - cached[0] < CACHE_SECONDS:
        return cached[1]

    try:
        payload = fetch_yahoo_chart(symbol)
    except Exception as exc:  # Presentation resilience matters more than surfacing stack traces.
        payload = synthetic_market(symbol, str(exc))

    encoded = json.dumps(payload).encode("utf-8")
    CACHE[symbol] = (time.time(), encoded)
    return encoded


class DemoHandler(SimpleHTTPRequestHandler):
    def do_GET(self) -> None:
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/market":
            params = urllib.parse.parse_qs(parsed.query)
            symbol = params.get("symbol", ["AAPL"])[0]
            body = market_payload(symbol)
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Content-Length", str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        if parsed.path == "/":
            self.path = "/index.html"
        super().do_GET()


def main() -> None:
    parser = argparse.ArgumentParser(description="Serve the order book presentation UI.")
    parser.add_argument("--port", type=int, default=8000)
    args = parser.parse_args()

    handler = partial(DemoHandler, directory=str(UI_DIR))
    server = ThreadingHTTPServer(("127.0.0.1", args.port), handler)
    print(f"Order book UI running at http://127.0.0.1:{args.port}")
    print("Press Ctrl+C to stop.")
    server.serve_forever()


if __name__ == "__main__":
    main()
