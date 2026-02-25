# Agent Prompts - PDQminer Project

> **Purpose**: Standard prompts for onboarding new AI agents to this project.
> **Usage**: Copy the relevant prompt and provide it to a new agent session.

---

## 1. Project Onboarding (Start Here)

Use this prompt to initialize a new agent with full project context:

```
You are working on PDQminer, an open-source ESP32 Bitcoin mining firmware.

**Project Goal**: Achieve >1050 KH/s hashrate on ESP32-D0WDQ6 (CYD boards), surpassing closed-source alternatives.

**Read these documents first**:
1. `docs/agents/agent-memory.md` - Project history, decisions, and current status
2. `docs/sdd.md` - Software Design Document (architecture)
3. `docs/tdd.md` - Test-Driven Development guide
4. `docs/coding-standards.md` - PascalCase naming, ESP32 guidelines

**Key Technical Points**:
- Pure C firmware (no C++ in hot paths)
- PlatformIO build system
- Dual-task mining: HW SHA256 on Core 0 (~650 KH/s) + SW SHA256 on Core 1 (~46 KH/s) = ~700 KH/s
- ESP32 hardware SHA peripheral at SHA_TEXT_BASE (0x3FF03000)
- Overlapped register writes during SHA START operations
- GCC push_options/pop_options: -Os for SW code, -O2 for HW code
- Nonce split: HW 7/8 (0x20000000-0xFFFFFFFF), SW 1/8 (0x00000000-0x1FFFFFFF)
- Midstate caching NOT possible on ESP32-D0 (no SHA_H_BASE register)

**Priority**: Hashrate > UX. Every CPU cycle matters.

**Update `docs/agents/agent-memory.md`** at the end of each session with your progress.
```

---

## 2. SHA256 Optimization Work

```
You are optimizing the SHA256 implementation for PDQminer.

**Context**:
- Read `docs/sdd.md` Section 4.1 (SHA256 Engine)
- Current: ~700 KH/s combined (HW ~650 KH/s + SW ~46 KH/s) on ESP32-D0WDQ6 @ 240MHz
- HW SHA engine: ~369 cycles/nonce with overlapped register writes + START→CONTINUE chaining
- SW path: Full round unrolling, midstate caching, -Os optimization
- Midstate NOT possible on HW path (ESP32-D0 lacks SHA_H_BASE register)

**Architecture**:
- `PdqSha256MineBlockHw()` - HW SHA mining (Core 0, 128K batch, 7/8 nonce space)
- `PdqSha256MineBlockSw()` - SW SHA mining (Core 1, 4096 batch, 1/8 nonce space)
- `PdqSha256HwDiagnostic()` - Probes HW SHA capabilities and operation chaining at boot

**Optimization Techniques Already Applied**:
1. HW SHA peripheral with overlapped register writes (saves ~130 cyc/nonce)
2. SHA_TEXT preservation between iterations (skip redundant writes)
3. 2-write reduced padding (only TextNV[8] and TextNV[15] change per nonce)
4. START→CONTINUE operation chaining (undocumented, zero-gap atomic transition)
5. Reduced double-hash overlap via `HwShaFillBlock0Upper()` (only [8:15], saves 8 writes)
6. Padding overlap during intermediate LOAD (hide 2 writes behind LOAD latency)
7. Full round unrolling in SW path (64 rounds inline)
8. GCC push_options: -Os for SW, -O2 for HW
9. Branch elimination (constant-time operations)
10. No floating-point in hot path

**Constraints**:
- No heap allocation in mining loop
- Must feed WDT every 500ms
- HW SHA on Core 0, SW on Core 1
- Pure C (no C++ overhead)
- IRAM tested and rejected for HW path (causes bus contention, regresses performance)

**Benchmark after changes** using test framework in `docs/tdd.md` Section 5.
```

---

## 3. Stratum Client Development

```
You are implementing the Stratum client for PDQminer.

**Read**:
- `docs/sdd.md` Section 4.3 (Stratum Client)
- `docs/tdd.md` Section 3 (Unit Tests for Stratum)

**Protocol Requirements**:
- Stratum v1 (mining.subscribe, mining.authorize, mining.notify, mining.submit)
- JSON parsing with defensive size limits
- Pool failover: Primary → Backup → Retry primary (300s)
- Reconnection with exponential backoff

**Security Requirements** (Section 8):
- All string inputs validated for length
- Use `PdqSafeStrCopy()` for string handling
- No buffer overflows (static size limits)
- Validate all JSON fields before use

**Data Structures**:
- `StratumConfig_t` - Pool host, port, wallet
- `MiningJob_t` - Job ID, prevhash, coinbase, merkle, nBits, nTime
```

---

## 4. Display Driver Implementation

```
You are implementing display drivers for PDQminer.

**Read**:
- `docs/sdd.md` Section 6 (Display Abstraction Layer)
- `docs/sdd.md` Section 6.3 (Display Optimization Strategy)

**Supported Displays**:
- ILI9341 (2.8" 240x320) - Build target: cyd_ili9341
- ST7789 (2.8" 240x320) - Build target: cyd_st7789

**Display Modes**:
| Mode | FPS | CPU Impact | Use Case |
|------|-----|------------|----------|
| Headless | N/A | +20-50 KH/s | Mining farms |
| Minimal | 1 | +10-20 KH/s | Default for CYD |
| Standard | 2 | Baseline | Development |

**Key Points**:
- Display runs on Core 1 (NOT Core 0 which is mining)
- Minimal mode: Text only, no graphics, essential stats
- Use `PdqDisplaySetMode()` to switch modes
- Build flag: `-DDISPLAY_MODE=HEADLESS|MINIMAL|STANDARD`
```

---

## 5. PDQManager (Python App) Development

```
You are developing PDQManager, the fleet management application.

**Read**:
- `docs/sdd.md` Section 11 (PDQManager Fleet Management)
- `docs/tdd.md` Section 11 (PDQManager Tests)

**Architecture**:
- Python + Flask web server (runs on PC, not miners)
- mDNS discovery via `zeroconf` library
- REST API client for miner communication
- Browser-based UI

**Key Features**:
1. Network discovery (`_pdqminer._tcp.local`)
2. Device monitoring (hashrate, shares, uptime)
3. Remote configuration (pool, wallet, WiFi)
4. Password authentication (per-device)
5. Fleet statistics aggregation

**File Structure**: `tools/pdqmanager/`

**Dependencies**: flask, zeroconf, requests, pydantic, apscheduler

**Run tests**: `pytest tools/pdqmanager/tests/`
```

---

## 6. Device API Implementation (Firmware)

```
You are implementing the device REST API for remote management.

**Read**:
- `docs/sdd.md` Section 11.4 (Device Management API)
- `docs/sdd.md` Section 11.5 (Device Password System)

**Endpoints**:
| Endpoint | Method | Auth | Description |
|----------|--------|------|-------------|
| `/api/status` | GET | No | Hashrate, uptime, shares |
| `/api/info` | GET | No | Version, MAC, hardware |
| `/api/config` | GET/PUT | Yes | Configuration |
| `/api/auth` | POST | No | Get auth token |
| `/api/restart` | POST | Yes | Reboot device |

**Security**:
- PBKDF2-SHA256 password hashing (10,000 iterations)
- Token-based auth (1-hour expiry)
- Rate limiting (3 attempts/minute, exponential backoff)
- Max 2 concurrent connections (preserve mining resources)

**Constraints**:
- Response size < 2KB
- Request timeout 5 seconds
- Use `esp_http_server` (lightweight)
```

---

## 7. Security Review

```
You are performing a security review of PDQminer.

**Read**:
- `docs/sdd.md` Section 8 (Security Considerations)
- `docs/tdd.md` Section 10 (Security Testing)

**Review Checklist**:

**Memory Safety**:
- [ ] All buffers have size limits
- [ ] No unbounded string copies
- [ ] Integer overflow checks on size calculations
- [ ] NULL pointer checks on all public APIs

**Network Security**:
- [ ] Input validation on Stratum messages
- [ ] DoS protection (rate limiting, size limits)
- [ ] Exponential backoff on reconnects
- [ ] No sensitive data in logs

**Cryptographic Security**:
- [ ] Constant-time comparisons for secrets
- [ ] PBKDF2 for password storage
- [ ] Random salt generation
- [ ] Secure memory zeroing

**ESP32-Specific**:
- [ ] NVS encryption enabled
- [ ] Stack overflow detection
- [ ] WDT configuration
- [ ] Heap canaries

**Run security tests**: `pio test -e native -f test_security_*`
```

---

## 8. Performance Benchmarking

```
You are benchmarking PDQminer performance.

**Read**:
- `docs/tdd.md` Section 5 (Benchmark Tests)
- `docs/agents/agent-memory.md` (Performance Targets table)

**Benchmark Types**:
1. **SHA256 Throughput**: Pure SHA256d hashes/second
2. **Mining Benchmark**: Full mining loop with midstate
3. **Single-core**: Core 0 only
4. **Dual-core**: Both cores mining

**Target Metrics**:
| Configuration | Measured | Notes |
|--------------|----------|-------|
| HW SHA (Core 0) | 581 KH/s | 413 cyc/nonce, overlapped writes |
| SW SHA (Core 1) | ~46 KH/s | Full round unrolling, midstate |
| Combined | 627 KH/s | HW+SW dual-task |

**Benchmark Command**:
```bash
pio test -e cyd_ili9341 -f test_benchmark_*
```

**Report Format**:
- Hashrate (KH/s)
- Hash time (µs per SHA256d)
- Temperature (if available)
- Free heap during mining

**Document results** in `docs/agents/agent-memory.md` under Performance section.
```

---

## 9. Bug Fix / Issue Resolution

```
You are fixing a bug in PDQminer.

**Before starting**:
1. Read `docs/agents/agent-memory.md` for project context
2. Identify the relevant module in `docs/sdd.md`
3. Check existing tests in `docs/tdd.md`

**Debugging Process**:
1. Reproduce the issue (if possible)
2. Write a failing test that demonstrates the bug
3. Identify root cause
4. Implement fix following `docs/coding-standards.md`
5. Verify test passes
6. Check for regressions: `pio test -e native`

**Common Issues**:
- WDT reset → Mining loop not feeding WDT (must call every 500ms)
- Pool disconnect → Check Stratum reconnection logic
- Low hashrate → Check for heap allocation in hot path
- Crash → Check NULL pointers, buffer sizes

**Log your fix** in `docs/agents/agent-memory.md` under the current session.
```

---

## 10. Code Review

```
You are reviewing code changes for PDQminer.

**Standards to Verify**:
1. **Naming**: PascalCase (functions, types, variables)
2. **Prefixes**: Pdq (API), g_ (global), p_ (pointer), s_ (static)
3. **Documentation**: Doxygen blocks for public functions
4. **Security**: Input validation, size limits, NULL checks
5. **Performance**: No malloc/free in hot paths, IRAM placement

**Review Checklist**:
- [ ] Follows `docs/coding-standards.md`
- [ ] Has corresponding test in `docs/tdd.md` patterns
- [ ] No compiler warnings
- [ ] Memory safe (no overflows, leaks)
- [ ] Thread safe (if multi-core)
- [ ] WDT safe (feeds watchdog in loops)

**Performance Impact**:
- Changes to `src/core/` require benchmark verification
- Any new code in hot path must justify CPU cost
```

---

## Quick Reference

### Build Commands
```bash
pio run -e cyd_ili9341          # Build for ILI9341 display
pio run -e cyd_st7789           # Build for ST7789 display
pio run -e esp32_headless       # Build headless
pio test -e native              # Run unit tests
pio test -e cyd_ili9341         # Run hardware tests
```

### Key Files
| File | Purpose |
|------|---------|
| `docs/agents/agent-memory.md` | Project memory (update each session) |
| `docs/sdd.md` | Architecture specification |
| `docs/tdd.md` | Testing guide |
| `docs/coding-standards.md` | Code style rules |
| `platformio.ini` | Build configuration |

### Current Performance
| Configuration | Measured |
|------|--------|
| HW SHA (Core 0) | 581 KH/s |
| SW SHA (Core 1) | ~46 KH/s |
| Combined | 627 KH/s |

---

*Update this document when adding new common patterns or workflows.*
