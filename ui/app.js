"use strict";

const SYMBOLS = ["AAPL", "TSLA", "NVDA"];
const FALLBACK_PRICES = { AAPL: 311.23, TSLA: 347.8, NVDA: 142.5 };

const state = {
  books: {},
  market: {},
  marketIndex: {},
  activeSymbol: "AAPL",
  nextId: 1000,
  processed: 0,
  matches: 0,
  feedTimer: null,
  lastLatency: 0,
  tape: [],
  loadingMarket: false
};

const els = {
  processedCount: document.getElementById("processedCount"),
  matchCount: document.getElementById("matchCount"),
  activeCount: document.getElementById("activeCount"),
  bestBid: document.getElementById("bestBid"),
  bestAsk: document.getElementById("bestAsk"),
  bestBidQty: document.getElementById("bestBidQty"),
  bestAskQty: document.getElementById("bestAskQty"),
  spread: document.getElementById("spread"),
  symbolLabel: document.getElementById("symbolLabel"),
  bidRows: document.getElementById("bidRows"),
  askRows: document.getElementById("askRows"),
  tape: document.getElementById("tape"),
  orderForm: document.getElementById("orderForm"),
  priceInput: document.getElementById("priceInput"),
  quantityInput: document.getElementById("quantityInput"),
  cancelInput: document.getElementById("cancelInput"),
  cancelButton: document.getElementById("cancelButton"),
  resetButton: document.getElementById("resetButton"),
  scenarioButton: document.getElementById("scenarioButton"),
  autoFeedButton: document.getElementById("autoFeedButton"),
  engineState: document.getElementById("engineState"),
  lastLatency: document.getElementById("lastLatency"),
  symbolTabs: document.querySelectorAll(".symbol-tab")
};

function createBook() {
  return {
    bids: new Map(),
    asks: new Map(),
    orders: new Map()
  };
}

function priceKey(price) {
  return Number(price).toFixed(2);
}

function roundPrice(price) {
  return Math.max(0.01, Math.round(Number(price) * 100) / 100);
}

function formatPrice(price) {
  if (!Number.isFinite(price) || price <= 0) return "$0.00";
  return "$" + price.toFixed(2);
}

function formatQty(quantity) {
  return quantity.toLocaleString("en-US") + " shares";
}

function getLevels(book, side) {
  const map = side === "BUY" ? book.bids : book.asks;
  const levels = Array.from(map.values()).filter((level) => level.quantity > 0);
  levels.sort((a, b) => side === "BUY" ? b.price - a.price : a.price - b.price);
  return levels;
}

function getTop(book, side) {
  return getLevels(book, side)[0] || null;
}

function addLevelOrder(book, order) {
  const cleanOrder = { ...order, price: roundPrice(order.price), quantity: Math.max(1, Math.round(order.quantity)) };
  const map = cleanOrder.side === "BUY" ? book.bids : book.asks;
  const key = priceKey(cleanOrder.price);
  if (!map.has(key)) {
    map.set(key, { price: cleanOrder.price, quantity: 0, orders: [] });
  }
  const level = map.get(key);
  level.orders.push(cleanOrder);
  level.quantity += cleanOrder.quantity;
  book.orders.set(cleanOrder.id, cleanOrder);
}

function removeEmptyLevels(book) {
  for (const map of [book.bids, book.asks]) {
    for (const [key, level] of map.entries()) {
      if (level.quantity <= 0 || level.orders.length === 0) map.delete(key);
    }
  }
}

function addTape(kind, message, meta) {
  state.tape.unshift({
    kind,
    message,
    meta,
    time: new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit", second: "2-digit" })
  });
  state.tape = state.tape.slice(0, 24);
}

function syntheticMarket(symbol, reason = "local browser fallback") {
  const base = FALLBACK_PRICES[symbol] || 100;
  let price = base;
  const now = Math.floor(Date.now() / 1000);
  const points = Array.from({ length: 120 }, (_, index) => {
    const drift = Math.sin(index / 8) * 0.13;
    const shock = ((index * 37) % 19 - 9) * 0.018;
    price = roundPrice(price + drift + shock);
    return { time: now - (119 - index) * 60, price, volume: 50000 + ((index * 811) % 90000) };
  });
  return {
    source: "synthetic fallback",
    reason,
    symbol,
    name: `${symbol} simulated feed`,
    currency: "USD",
    regularMarketPrice: points.at(-1).price,
    previousClose: base,
    points,
    fetchedAt: now
  };
}

async function fetchMarket(symbol) {
  try {
    const response = await fetch(`/api/market?symbol=${encodeURIComponent(symbol)}`, { cache: "no-store" });
    if (!response.ok) throw new Error(`HTTP ${response.status}`);
    const payload = await response.json();
    if (!payload.points || payload.points.length < 5) throw new Error("not enough market points");
    return payload;
  } catch (error) {
    return syntheticMarket(symbol, error.message);
  }
}

function seedBookFromMarket(symbol) {
  const book = createBook();
  const market = state.market[symbol] || syntheticMarket(symbol);
  const base = roundPrice(market.regularMarketPrice || market.points.at(-1).price);
  const step = base > 500 ? 0.5 : base > 150 ? 0.25 : 0.05;
  const baseQuantity = symbol === "NVDA" ? 300 : 120;

  for (let level = 1; level <= 7; level += 1) {
    const bidQty = baseQuantity + level * 40 + (level % 3) * 25;
    const askQty = baseQuantity + level * 35 + ((level + 1) % 3) * 30;
    addLevelOrder(book, {
      id: state.nextId++,
      timestamp: performance.now(),
      side: "BUY",
      price: base - step * level,
      quantity: bidQty
    });
    addLevelOrder(book, {
      id: state.nextId++,
      timestamp: performance.now(),
      side: "SELL",
      price: base + step * level,
      quantity: askQty
    });
  }

  state.books[symbol] = book;
  state.marketIndex[symbol] = 0;
}

function processOrder(symbol, order) {
  const start = performance.now();
  const book = state.books[symbol];
  const oppositeSide = order.side === "BUY" ? "SELL" : "BUY";
  const levels = getLevels(book, oppositeSide);
  let remaining = Math.max(1, Math.round(order.quantity));
  let matchedQuantity = 0;

  for (const level of levels) {
    const isMarketable = order.side === "BUY" ? order.price >= level.price : order.price <= level.price;
    if (!isMarketable || remaining <= 0) break;

    while (level.orders.length > 0 && remaining > 0) {
      const restingOrder = level.orders[0];
      const fillQuantity = Math.min(remaining, restingOrder.quantity);
      remaining -= fillQuantity;
      matchedQuantity += fillQuantity;
      restingOrder.quantity -= fillQuantity;
      level.quantity -= fillQuantity;
      state.matches += 1;

      addTape(
        "match",
        `${symbol} matched ${fillQuantity} @ ${formatPrice(level.price)}`,
        `incoming #${order.id} with resting #${restingOrder.id}`
      );

      if (restingOrder.quantity === 0) {
        book.orders.delete(restingOrder.id);
        level.orders.shift();
      }
    }
  }

  removeEmptyLevels(book);

  if (remaining > 0) {
    addLevelOrder(book, { ...order, quantity: remaining });
    addTape(
      "add",
      `${symbol} ${order.side} added ${remaining} @ ${formatPrice(order.price)}`,
      `order #${order.id}${matchedQuantity ? ", residual after fill" : ""}`
    );
  }

  state.processed += 1;
  state.lastLatency = Math.max(4, Math.round((performance.now() - start) * 1000));
}

function cancelOrder(symbol, id) {
  const book = state.books[symbol];
  const order = book.orders.get(id);
  state.processed += 1;

  if (!order) {
    addTape("cancel", `${symbol} cancel rejected`, `order #${id} not active`);
    state.lastLatency = 18;
    return;
  }

  const map = order.side === "BUY" ? book.bids : book.asks;
  const level = map.get(priceKey(order.price));
  if (level) {
    level.orders = level.orders.filter((item) => item.id !== id);
    level.quantity -= order.quantity;
  }

  book.orders.delete(id);
  removeEmptyLevels(book);
  addTape("cancel", `${symbol} canceled order #${id}`, `${order.quantity} shares removed`);
  state.lastLatency = 14;
}

function activeOrders() {
  return SYMBOLS.reduce((sum, symbol) => sum + state.books[symbol].orders.size, 0);
}

function renderRows(container, levels, side) {
  container.innerHTML = "";
  if (levels.length === 0) {
    const empty = document.createElement("div");
    empty.className = "empty-row";
    empty.textContent = "No resting orders";
    container.appendChild(empty);
    return;
  }

  const maxQuantity = Math.max(...levels.map((level) => level.quantity));
  levels.slice(0, 8).forEach((level) => {
    const row = document.createElement("div");
    row.className = "book-row";
    row.style.setProperty("--depth", `${Math.max(10, (level.quantity / maxQuantity) * 100)}%`);
    row.innerHTML = `
      <span class="${side === "BUY" ? "price-buy" : "price-sell"}">${formatPrice(level.price)}</span>
      <span>${level.quantity.toLocaleString("en-US")}</span>
      <span>${level.orders.length}</span>
    `;
    container.appendChild(row);
  });
}

function renderTape() {
  els.tape.innerHTML = "";
  state.tape.forEach((item) => {
    const li = document.createElement("li");
    li.className = item.kind;
    li.innerHTML = `<strong>${item.message}</strong><small>${item.meta} | ${item.time}</small>`;
    els.tape.appendChild(li);
  });
}

function render() {
  const book = state.books[state.activeSymbol];
  if (!book) return;

  const market = state.market[state.activeSymbol];
  const bids = getLevels(book, "BUY");
  const asks = getLevels(book, "SELL");
  const bestBid = bids[0];
  const bestAsk = asks[0];
  const spread = bestBid && bestAsk ? bestAsk.price - bestBid.price : 0;
  const source = market ? market.source : "loading";

  els.processedCount.textContent = state.processed.toLocaleString("en-US");
  els.matchCount.textContent = state.matches.toLocaleString("en-US");
  els.activeCount.textContent = activeOrders().toLocaleString("en-US");
  els.bestBid.textContent = formatPrice(bestBid ? bestBid.price : 0);
  els.bestAsk.textContent = formatPrice(bestAsk ? bestAsk.price : 0);
  els.bestBidQty.textContent = formatQty(bestBid ? bestBid.quantity : 0);
  els.bestAskQty.textContent = formatQty(bestAsk ? bestAsk.quantity : 0);
  els.spread.textContent = formatPrice(spread);
  els.symbolLabel.textContent = `${state.activeSymbol} | ${source}`;
  els.engineState.textContent = state.loadingMarket ? "loading market data" : state.feedTimer ? "market replay running" : "idle";
  els.lastLatency.textContent = `${state.lastLatency} us`;
  els.autoFeedButton.textContent = state.feedTimer ? "Stop Feed" : "Start Feed";

  renderRows(els.bidRows, bids, "BUY");
  renderRows(els.askRows, asks, "SELL");
  renderTape();

  els.symbolTabs.forEach((tab) => {
    tab.classList.toggle("is-active", tab.dataset.symbol === state.activeSymbol);
  });
}

async function resetBooks(loadFreshMarket = false) {
  state.loadingMarket = true;
  state.books = {};
  state.processed = 0;
  state.matches = 0;
  state.lastLatency = 0;
  state.nextId = 1000;
  state.tape = [];
  render();

  for (const symbol of SYMBOLS) {
    if (loadFreshMarket || !state.market[symbol]) {
      state.market[symbol] = await fetchMarket(symbol);
    }
    seedBookFromMarket(symbol);
  }

  state.loadingMarket = false;
  const source = state.market[state.activeSymbol].source;
  addTape("system", "Market data loaded", `${source}; book seeded around latest ${state.activeSymbol} price`);
  setEntryPriceToMid();
  render();
}

function setEntryPriceToMid() {
  const book = state.books[state.activeSymbol];
  const bestBid = getTop(book, "BUY");
  const bestAsk = getTop(book, "SELL");
  const market = state.market[state.activeSymbol];
  const price = bestBid && bestAsk
    ? (bestBid.price + bestAsk.price) / 2
    : market?.regularMarketPrice || FALLBACK_PRICES[state.activeSymbol] || 100;
  els.priceInput.value = roundPrice(price).toFixed(2);
}

function nextMarketTick(symbol) {
  const market = state.market[symbol] || syntheticMarket(symbol);
  const points = market.points.length ? market.points : syntheticMarket(symbol).points;
  const index = state.marketIndex[symbol] % points.length;
  const prev = points[Math.max(0, index - 1)] || points[0];
  const point = points[index];
  state.marketIndex[symbol] = (index + 1) % points.length;
  return { point, prev };
}

function marketReplayOrder(symbol) {
  const book = state.books[symbol];
  const bestBid = getTop(book, "BUY");
  const bestAsk = getTop(book, "SELL");
  const { point, prev } = nextMarketTick(symbol);
  const side = point.price >= prev.price ? "BUY" : "SELL";
  const cross = Math.abs(point.price - prev.price) > 0.02 || Math.random() > 0.45;
  const visiblePrice = roundPrice(point.price);
  let price = visiblePrice;

  if (cross && side === "BUY" && bestAsk) price = bestAsk.price;
  if (cross && side === "SELL" && bestBid) price = bestBid.price;
  if (!cross) price = side === "BUY" ? visiblePrice - 0.03 : visiblePrice + 0.03;

  return {
    id: state.nextId++,
    timestamp: point.time,
    side,
    price: roundPrice(price),
    quantity: Math.max(10, Math.round((point.volume || 50000) / 1400 / 10) * 10)
  };
}

async function loadScenario() {
  if (state.feedTimer) toggleFeed();
  await resetBooks(true);

  const sequence = Array.from({ length: 10 }, () => marketReplayOrder(state.activeSymbol));
  sequence.forEach((order, index) => {
    window.setTimeout(() => {
      processOrder(state.activeSymbol, order);
      render();
    }, index * 360);
  });
}

function toggleFeed() {
  if (state.feedTimer) {
    window.clearInterval(state.feedTimer);
    state.feedTimer = null;
    render();
    return;
  }

  state.feedTimer = window.setInterval(() => {
    processOrder(state.activeSymbol, marketReplayOrder(state.activeSymbol));
    render();
  }, 520);
  render();
}

els.orderForm.addEventListener("submit", (event) => {
  event.preventDefault();
  const form = new FormData(els.orderForm);
  const side = form.get("side");
  const price = Number(form.get("price"));
  const quantity = Number(form.get("quantity"));
  if (!price || !quantity) return;
  processOrder(state.activeSymbol, {
    id: state.nextId++,
    timestamp: performance.now(),
    side,
    price,
    quantity
  });
  render();
});

els.cancelButton.addEventListener("click", () => {
  const id = Number(els.cancelInput.value);
  if (!id) return;
  cancelOrder(state.activeSymbol, id);
  els.cancelInput.value = "";
  render();
});

els.resetButton.addEventListener("click", async () => {
  if (state.feedTimer) toggleFeed();
  await resetBooks(true);
});

els.scenarioButton.addEventListener("click", loadScenario);
els.autoFeedButton.addEventListener("click", toggleFeed);

els.symbolTabs.forEach((tab) => {
  tab.addEventListener("click", () => {
    state.activeSymbol = tab.dataset.symbol;
    setEntryPriceToMid();
    render();
  });
});

resetBooks(true);
