# Architecture Diagrams

## System Overview

```
┌────────────────────────────────────────────────────────────────────┐
│                   High-Performance Order Book System                │
└────────────────────────────────────────────────────────────────────┘

┌─────────────────────┐                    ┌──────────────────────┐
│  Market Feed Thread │                    │  Matching Engine     │
│   (Producer)        │                    │  Thread (Consumer)   │
│                     │                    │                      │
│  - Generate orders  │                    │  - Process orders    │
│  - Timestamp        │                    │  - Match/Add/Cancel  │
│  - Push to buffer   │                    │  - Update book       │
│                     │                    │                      │
│  [Core 0 / Pinned]  │                    │  [Core 1 / Pinned]   │
└──────────┬──────────┘                    └──────────▲───────────┘
           │                                          │
           │  Lock-free Communication                 │
           │                                          │
           ▼                                          │
    ┌──────────────────────────────────────────┐     │
    │       SPSC Ring Buffer (8192 slots)      │─────┘
    │                                          │
    │  Cache Line 0: [head atomic + padding]   │
    │  Cache Line 1: [tail atomic + padding]   │
    │  Cache Lines 2+: [Order buffer...]       │
    │                                          │
    │  • No locks                              │
    │  • Atomic operations only                │
    │  • Power-of-2 size                       │
    │  • ~5-10ns per operation                 │
    └──────────────────────────────────────────┘
                          │
                          │ Orders flow
                          ▼
    ┌──────────────────────────────────────────┐
    │           Limit Order Book               │
    │                                          │
    │  ┌────────────┐      ┌─────────────┐    │
    │  │  Buy Side  │      │  Sell Side  │    │
    │  │  (Bids)    │      │  (Asks)     │    │
    │  │            │      │             │    │
    │  │ Price ▼    │      │ Price ▲     │    │
    │  │ 100.50 ──┐ │      │ 100.70 ──┐  │    │
    │  │ 100.40 ──┤ │      │ 100.80 ──┤  │    │
    │  │ 100.30 ──┤ │      │ 100.90 ──┤  │    │
    │  │   ...    │ │      │   ...    │  │    │
    │  └────────────┘      └─────────────┘    │
    │                                          │
    │  Memory Pool: [100,000 order slots]     │
    │  Hash Map: order_id → pool_index        │
    └──────────────────────────────────────────┘
```

## Memory Layout

```
┌───────────────────────────────────────────────────────────┐
│                    Order Structure (64 bytes)              │
├──────────────┬──────────┬────────────────────────────────┤
│ Offset       │ Size     │ Field                          │
├──────────────┼──────────┼────────────────────────────────┤
│ 0            │ 8 bytes  │ order_id                       │
│ 8            │ 8 bytes  │ timestamp                      │
│ 16           │ 8 bytes  │ price                          │
│ 24           │ 4 bytes  │ quantity                       │
│ 28           │ 1 byte   │ type (ADD/CANCEL/MATCH)        │
│ 29           │ 1 byte   │ side (BUY/SELL)                │
│ 30           │ 2 bytes  │ padding                        │
│ 32           │ 32 bytes │ padding (unused)               │
└──────────────┴──────────┴────────────────────────────────┘
                    ▲
                    │
                    └─ Exactly 1 cache line (64 bytes)
                       Prevents cache line splitting!
```

## Price Level Organization

```
Buy Side (std::map<price, PriceLevel>, descending order)
═══════════════════════════════════════════════════════

Price: 100.50 (Best Bid) ◄─── map::begin() = O(1)
├─ Order 1: 100 shares, ts=1000
├─ Order 2: 250 shares, ts=1050
└─ Order 3: 150 shares, ts=1100
   Total: 500 shares, 3 orders

Price: 100.40
├─ Order 4: 200 shares, ts=1200
└─ Order 5: 300 shares, ts=1250
   Total: 500 shares, 2 orders

Price: 100.30
└─ Order 6: 100 shares, ts=1300
   Total: 100 shares, 1 order

...


Sell Side (std::map<price, PriceLevel>, ascending order)
═══════════════════════════════════════════════════════

Price: 100.70 (Best Ask) ◄─── map::begin() = O(1)
├─ Order 7: 150 shares, ts=1010
└─ Order 8: 200 shares, ts=1080
   Total: 350 shares, 2 orders

Price: 100.80
├─ Order 9: 300 shares, ts=1150
└─ Order 10: 100 shares, ts=1220
   Total: 400 shares, 2 orders

...
```

## Order Processing Flow

```
┌─────────────┐
│ Order Arrives│
└──────┬──────┘
       │
       ▼
┌──────────────────┐
│ type == ADD?     │──No──┐
└──────┬───────────┘      │
      Yes                 │
       │                  ▼
       ▼           ┌──────────────┐
┌──────────────┐   │ type==CANCEL?│
│ Try Match    │   └──────┬───────┘
│ Against      │         Yes
│ Opposite Side│          │
└──────┬───────┘          ▼
       │           ┌─────────────┐
       │           │ Hash Lookup │
       │           │ Remove Order│
       │           └─────────────┘
       ▼
┌─────────────────┐
│ Fully Filled?   │──Yes──┐
└──────┬──────────┘        │
      No                   │
       │                   │
       ▼                   │
┌─────────────────┐        │
│ Add Remaining   │        │
│ to Book         │        │
│                 │        │
│ • Get/Create    │        │
│   Price Level   │        │
│ • Alloc from    │        │
│   Memory Pool   │        │
│ • Add to Linked │        │
│   List (tail)   │        │
│ • Update Stats  │        │
└─────────────────┘        │
       │                   │
       └───────────────────┘
                │
                ▼
          ┌──────────┐
          │  Done!   │
          └──────────┘
```

## Cache Line Layout (SPSC Ring Buffer)

```
Memory Address Space:

0x0000  ┌────────────────────────────────────────┐
        │  head atomic (8 bytes)                 │
        │  padding (56 bytes)                    │
0x0040  ├────────────────────────────────────────┤ ◄── Cache Line 1
        │  tail atomic (8 bytes)                 │
        │  padding (56 bytes)                    │
0x0080  ├────────────────────────────────────────┤ ◄── Cache Line 2
        │  buffer[0] = Order (64 bytes)          │
0x00C0  ├────────────────────────────────────────┤ ◄── Cache Line 3
        │  buffer[1] = Order (64 bytes)          │
0x0100  ├────────────────────────────────────────┤ ◄── Cache Line 4
        │  buffer[2] = Order (64 bytes)          │
        │  ...                                   │
        │  buffer[8191] = Order (64 bytes)       │
        └────────────────────────────────────────┘

Producer writes ONLY to head (Cache Line 0)
Consumer writes ONLY to tail (Cache Line 1)
    ➜ No false sharing! ➜ No cache line bouncing!
```

## Matching Algorithm (Price-Time Priority)

```
Incoming: BUY order, price=100.80, qty=350

Step 1: Check opposite side (Sell Side)
┌─────────────────────────────────────┐
│ Best Ask: 100.70  (350 shares)      │  ◄── Price crosses!
│   Order 7: 150 @ ts=1010            │
│   Order 8: 200 @ ts=1080            │
└─────────────────────────────────────┘

Step 2: Match at 100.70 (resting order price, time priority)
┌─────────────────────────────────────┐
│ Match Order 7: 150 shares           │  ◄── Oldest order first
│   Incoming qty: 350 - 150 = 200     │
│   Order 7 filled, remove from book  │
└─────────────────────────────────────┘

Step 3: Continue matching at same price level
┌─────────────────────────────────────┐
│ Match Order 8: 200 shares           │  ◄── Next in time priority
│   Incoming qty: 200 - 200 = 0       │
│   Order 8 filled, remove from book  │
│   Incoming order fully filled!      │
└─────────────────────────────────────┘

Step 4: Update statistics
┌─────────────────────────────────────┐
│ Remove price level 100.70 (empty)   │
│ Total matches: +2                   │
│ Done!                               │
└─────────────────────────────────────┘
```

## Thread Communication Pattern

```
Time ─────────────────────────────────────────────────▶

Producer Thread (Core 0):
  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐
  │P1│  │P2│  │P3│  │P4│  │P5│  │P6│  │P7│  ...
  └┬─┘  └┬─┘  └┬─┘  └┬─┘  └┬─┘  └┬─┘  └┬─┘
   │     │     │     │     │     │     │
   ▼     ▼     ▼     ▼     ▼     ▼     ▼
  ┌───────────────────────────────────────┐
  │      SPSC Ring Buffer (lock-free)     │
  └───────────────────────────────────────┘
   │     │     │     │     │     │     │
   ▼     ▼     ▼     ▼     ▼     ▼     ▼
  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐  ┌──┐
  │C1│  │C2│  │C3│  │C4│  │C5│  │C6│  │C7│  ...
  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘  └──┘
Consumer Thread (Core 1):

Busy polling: No sleep, 100% CPU utilization
              for minimum latency
```

## Performance Hotspots

```
CPU Time Distribution:
══════════════════════

Ring Buffer Pop       ████████░░░░░░░░░░  15%  (~30 cycles)
Order Matching        ████████████░░░░░░  35%  (~105 cycles)
├─ Price Level Lookup ██████░░░░░░░░░░░░  15%  (~45 cycles)
├─ Hash Map Lookup    ████░░░░░░░░░░░░░░  10%  (~30 cycles)
└─ Linked List Ops    ████░░░░░░░░░░░░░░  10%  (~30 cycles)
Add to Book           ██████░░░░░░░░░░░░  15%  (~45 cycles)
Cancel Order          ████░░░░░░░░░░░░░░  10%  (~30 cycles)
Busy Waiting          ████████░░░░░░░░░░  20%  (spinning)
Other                 ██░░░░░░░░░░░░░░░░   5%  (~15 cycles)

Total per order: ~180 cycles @ 3.5GHz = ~51ns average
```

## Memory Access Pattern

```
Optimal Case (Cache Hits):
═══════════════════════════
Order arrives
   │
   ▼
L1 Cache (64 bytes)  ◄── Order structure (1 cache line)
   │ Hit! (~1ns)
   ▼
Hash Map Lookup
   │
   ▼
L1 Cache             ◄── Hash map bucket (likely in cache)
   │ Hit! (~1ns)
   ▼
Price Level Access
   │
   ▼
L2 Cache (256 bytes) ◄── Price level + linked list head
   │ Hit! (~3ns)
   ▼
Process Order
   │
   ▼
Total: ~40-60ns


Worst Case (Cache Misses):
═══════════════════════════
Order arrives
   │
   ▼
L3 Cache Miss
   │ Miss! (~15ns)
   ▼
Main Memory
   │ Access (~100ns)
   ▼
Hash Map Cold
   │
   ▼
Main Memory
   │ Access (~100ns)
   ▼
Price Level Cold
   │
   ▼
Main Memory
   │ Access (~100ns)
   ▼
Total: ~300-500ns
```

This visual documentation helps understand the system architecture at a glance!
