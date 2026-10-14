# PyC-KVStore: A High-Performance, ACID-Compliant Embedded Key-Value Store

A lightweight, persistent key-value store engineered from scratch in **C** with a seamless, zero-overhead **Python `ctypes`** binding. It features professional-grade database internals, including a 4KB slotted-page architecture, arithmetic Free Space Management, tombstone recycling, lazy compaction, TOAST-style overflow chaining, and a crash-recoverable Write-Ahead Log (WAL).

---

## 🏗️ Architectural Highlights

* **Slotted-Page Storage Engine**: Manages fixed **4KB pages** (`PAGE_SIZE`) divided into a page header, an expanding growth array of slots, and a shrinking free space pointer for records[cite: 2].
* **$O(1)$ Free Space Map (FSM)**: Tracks available space across up to **1024 pages** (`MAX_PAGES`) using a direct arithmetic ledger, completely eliminating the need for expensive sequential disk scans during writes[cite: 2].
* **Transparent Variable-Length & Large Records**: 
  * Supports standard string payloads (`key|value`)[cite: 6].
  * Automatically handles massive payloads via **TOAST-style multi-page overflow chaining** using fragment headers (`RecordFragmentHeader`) under the hood[cite: 2, 6].
* **Slot Recycling & Lazy Compaction**: Reuses deleted record slots via tombstones and performs on-demand defragmentation (`compact_page`) only when internal page fragmentation prevents an allocation[cite: 6].
* **ACID Durability via Write-Ahead Log (WAL)**: 
  * Enforces a strict logging protocol with binary magic guards (`0xDEADBEEF`) and `fsync` flushing before touching data pages[cite: 6].
  * Automatically replays and reconstructs state on startup (`recover_from_wal`), gracefully truncating corrupted trailing writes[cite: 6].
* **Pythonic `ctypes` Integration**: Exposes the low-level C engine through an intuitive, object-oriented Python API (`KeyValueStore`) that handles memory marshalling safely.

---

## 📂 Project Structure

* **`storage.h`**: Core data structures (`DiskPage`, `Slot`, `IndexTable`, `FreeSpaceMap`, `WALRecordHeader`) and function prototypes[cite: 2].
* **`storage.c`**: Low-level C engine implementation (page management, FSM, lazy compaction, overflow chaining, WAL logging, and crash recovery)[cite: 6].
* **`db.py`**: Python `ctypes` wrapper implementing the high-level `KeyValueStore` class (`put`, `get`, `delete`, `update`)[cite: 1].
* **`test_db.py`**: Comprehensive test suite covering standard operations, conditional in-place updates, massive TOAST blobs, and deletions[cite: 8].

---

## 🚀 Getting Started

### Prerequisites
* A C compiler (`gcc` or `clang`)
* Python 3.x

### 1. Compile the C Shared Library
To build the dynamic library required by Python, run the following command in your terminal:

* **Linux / WSL:**
  ```bash
  gcc -shared -fPIC storage.c -o libstorage.so

* **MacOS:**
  ```bash
  gcc -shared -fPIC storage.c -o libstorage.dylib

* **Windows (MinGW):**
  ```bash
  gcc -shared -fPIC storage.c -o libstorage.dll

---

## 🌐 Full-Stack & Desktop Extensions

The project has evolved from a standalone C core into a multi-interface storage architecture:

* **Backend API Bridge (`/backend`)**: A lightweight **FastAPI** service wrapper enabling RESTful access (`/put`, `/get`, `/delete`, `/update`, `/stats`) with CORS support and Pydantic validation.
* **Vanilla JavaScript Dashboard (`/frontend-vanilla`)**: A lightweight, zero-build web interface using asynchronous `fetch` calls and DOM manipulation.
* **React Dashboard (`/frontend-react`)**: A modern component-based web interface built with **Vite** and React state hooks (`useState`, `useEffect`).
* **PyQt5 Visual Manager (`/frontend-desktop`)**: *[In Progress]* A native desktop database workbench featuring real-time Free Space Map (FSM) analytics, page inspection, and integrated **OpenCV/Matplotlib** data visualization.

---

## 📂 Updated Monorepo Directory Structure

```text
PyC-KVStore/
│
├── storage.h               # C Core headers
├── storage.c               # C Engine implementation
├── libstorage.so           # Compiled shared library
├── db.py                   # Python ctypes wrapper
├── database.db             # Persistent data file
├── database.wal            # Write-Ahead Log
│
├── backend/                # FastAPI REST API Bridge
│   └── main.py
│
├── frontend-vanilla/       # Vanilla JS Web Dashboard
│   ├── index.html
│   ├── style.css
│   └── app.js
│
├── frontend-react/         # Modern React Web Dashboard
│   ├── src/
│   ├── public/
│   └── package.json
│
├── README.md
└── .gitignore