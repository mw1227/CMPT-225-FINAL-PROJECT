# High-Performance Hashing in Practice

**CMPT 225 Final Project**

Authors: Matthew Wong, Ryan Raghani

## Overview

Using benchmark.pp to compare these 4 hash table collision resolution strategies:

1. **Separate Chaining** — Storing in linked list
2. **Linear Probing** — Search for next slot linearly
3. **Robin Hood Hashing** — Reduced probe length and variance, improved linear probing
4. **Cuckoo Hashing** — 2 tables and 2 hash functions for O(1) worst case lookup and deletion

## How to build & run

```bash
make # build
make run # run
```

