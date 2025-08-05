# vots
**V**ery **o**ptimized **t**rading **s**ystem (vots) is a trading system
developed in C++ and optimized for low latency operation.

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
