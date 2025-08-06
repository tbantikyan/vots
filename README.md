# vots
**V**ery **o**ptimized **t**rading **s**ystem (vots) is a trading system
developed in C++ and optimized for low latency operation.

## Optimizations:
* Custom TCP and UDP socket programming for fast communication between trading ecosystem components.
* Concurrent execution framework that avoids context switching overhead.
    * Generic threading library with support for CPU core pinning.
    * Lock-free queues for sharing data between threads without blocking.
* Bespoke memory allocator and manager to avoid dynamic allocations and improve spatial locality.
* Asynchronous logger with efficient string operations to reduce I/O cost.

## Components:
### Trading Exchange
* Matching Engine
    * Limit Order Book
* Order Gateway Server
* Market Data Publisher

### Market Participant
* Trading Engine
    * Feature Engine (tracking Fair Market Price & Aggressive Trade Quantity Ratio)
    * Order Manager
    * Risk Manager (tracking pre-trade risk)
    * Position Keeper
    * Liquidity Taker & Market Maker trading strategies
* Order Gateway Client
* Market Data Consumer
