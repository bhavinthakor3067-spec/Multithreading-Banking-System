# 🏦 Multithreading Banking System

A C++ client-server banking simulation built with raw TCP sockets and native multithreading. The server handles multiple clients concurrently — each connection gets its own thread — while per-account mutexes keep balances, transfers, and profile data consistent under concurrent access. Cross-platform support for Windows (Winsock) and Linux/macOS (POSIX sockets) is built in via preprocessor guards.

## Features

- **Concurrent client handling** — the server spawns a detached `std::thread` per incoming connection, so multiple customers can transact at the same time.
- **Thread-safe accounts** — every `BankAccount` owns its own `mutex`, guarding balance changes, profile edits, and card operations.
- **Deadlock-safe transfers** — transferring funds between two accounts locks both mutexes together with `std::lock` (+ `adopt_lock`), avoiding the classic lock-ordering deadlock.
- **Account management** — create an account (auto-generated 12-digit account number), log in with a password, log out.
- **Core banking operations** — deposit, withdraw, check balance, transfer funds.
- **Flexible transfer sources** — send money from your account balance *or* from a linked credit card's available limit.
- **Credit & debit cards** — add cards to an account and list them, each with a randomly generated 12-digit card number.
- **Profile management** — view and edit name, phone (validated to exactly 10 digits), email, and address.
- **Separated logging** — money-moving actions are recorded as *transaction statements*; everything else (profile edits, card additions, failed attempts, logins) goes into *system logs*.
- **Persistence** — all account data is serialized to `accounts.dat` on disk after every mutating operation, and reloaded automatically when the server restarts.

## Architecture

```
 ┌────────────┐        TCP (port 8080)         ┌────────────┐
 │  Client A  │ ─────────────────────────────▶ │            │
 └────────────┘                                │            │
 ┌────────────┐                                │   Server   │──▶ accounts.dat
 │  Client B  │ ─────────────────────────────▶ │  (1 thread │
 └────────────┘                                │  per conn) │
 ┌────────────┐                                │            │
 │  Client C  │ ─────────────────────────────▶ │            │
 └────────────┘                                └────────────┘
```

- **`server.cpp`** — accepts connections on a listening socket, spawns a worker thread per client, and dispatches a simple pipe-delimited text protocol against an in-memory `unordered_map<string, shared_ptr<BankAccount>>` of accounts. A global `accounts_mtx` protects the map itself (e.g. while creating a new account); each `BankAccount` additionally protects its own fields with a private mutex.
- **`Client.cpp`** — a console client that connects to the server, prompts the user through menus, and sends/receives protocol messages over the socket.

### Wire Protocol

Requests are plain text, pipe-delimited, newline-terminated: `COMMAND|arg1|arg2|...\n`. Responses start with `OK` or `ERR` followed by a message.

| Command | Arguments | Description |
|---|---|---|
| `CREATE` | `name\|deposit\|password` | Opens a new account, returns the generated account number |
| `LOGIN` | `accountNumber\|password` | Authenticates and starts a session |
| `LOGOUT` | — | Ends the current session |
| `QUIT` | — | Closes the connection |
| `BALANCE` | — | Returns current balance |
| `DEPOSIT` | `amount` | Credits the account |
| `WITHDRAW` | `amount` | Debits the account (checked against balance) |
| `TRANSFER` | `toAccount\|amount\|source` | Transfers funds from balance or a credit card to another account |
| `GET_SOURCES` | — | Lists available transfer sources (balance + credit cards) |
| `GET_STATEMENT` | — | Returns transaction history |
| `GET_SYSTEM_LOGS` | — | Returns non-monetary account activity logs |
| `GET_PROFILE` | — | Returns profile details |
| `EDIT_PROFILE` | `name\|phone\|email\|address` | Updates profile (phone must be 10 digits) |
| `ADD_CREDIT_CARD` | `limit` | Issues a new credit card |
| `GET_CREDIT_CARDS` | — | Lists linked credit cards |
| `ADD_DEBIT_CARD` | — | Issues a new debit card |
| `GET_DEBIT_CARDS` | — | Lists linked debit cards |

## Project Structure

```
Multithreading_Banking_System/
├── server.cpp   # Multithreaded TCP server, account logic, persistence
├── Client.cpp   # Interactive console client
└── README.md
```

## Getting Started

### Prerequisites

- A C++17-capable compiler (`g++` or MSVC)
- On Windows: no extra install needed — the code links against `ws2_32` (Winsock)
- On Linux/macOS: standard POSIX sockets, no extra libraries beyond pthreads

### Build

**Linux / macOS**
```bash
g++ -std=c++17 -O2 -pthread server.cpp -o server
g++ -std=c++17 -O2 -pthread Client.cpp -o client
```

**Windows (MinGW)**
```bash
g++ -std=c++17 -O2 server.cpp -o server.exe -lws2_32
g++ -std=c++17 -O2 Client.cpp -o client.exe -lws2_32
```

**Windows (MSVC)**
```bash
cl /EHsc /std:c++17 server.cpp ws2_32.lib
cl /EHsc /std:c++17 Client.cpp ws2_32.lib
```

### Run

1. Start the server (it listens on port `8080` and loads any existing `accounts.dat`):
   ```bash
   ./server
   ```
2. In a separate terminal (or on another machine on the same network), start one or more clients:
   ```bash
   ./client
   ```
3. When prompted, enter the server's IP (`127.0.0.1` for local testing) and port (`8080`).
4. From the client menu, create a new account or log in to an existing one, then use the in-session menu to deposit, withdraw, transfer, manage cards, or edit your profile.

You can run multiple client instances at once (even from the same machine) to see the server handle concurrent sessions.

## Data Persistence

Account state — balance, password, profile, transaction statements, system logs, and linked cards — is written to `accounts.dat` in the server's working directory after every mutating request, and reloaded automatically the next time the server starts.

## Notes & Limitations

- This is an educational/demo project — passwords and account data are stored in **plain text** on disk and over the wire, and there is no TLS/encryption on the socket connection. Do not use it to handle real financial data.
- The account map lookup, save, and load routines use fixed field ordering in `accounts.dat`; manually editing that file can corrupt the data.

## License

No license file is currently included — all rights reserved by default until one is added.
