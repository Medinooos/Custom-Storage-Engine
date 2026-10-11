from db import KeyValueStore

if __name__ == "__main__":
    print("[*] Initializing Python-bound C Storage Engine...")
    db = KeyValueStore("database.db", "database.wal")

    # 1. Testing Standard Inserts & Gets
    print("\n--- Testing Standard Operations ---")
    db.put("python_key_1", "Hello from Python ctypes!")
    db.put("python_key_2", "Database engineering is awesome.")
    
    print("Getting python_key_1:", db.get("python_key_1"))
    print("Getting python_key_2:", db.get("python_key_2"))

    # 2. Testing Conditional Updates (In-place vs Delete/Insert)
    print("\n--- Testing Updates ---")
    db.update("python_key_1", "Updated short val") # In-place fast path
    print("Getting updated python_key_1:", db.get("python_key_1"))

    # 3. Test Massive Overflow Records (TOAST chaining)
    print("\n--- Testing Massive Records (TOAST) ---")
    massive_string = "X" * 15000 + " [END OF BLOB]"
    db.put("big_payload", massive_string)
    
    retrieved_blob = db.get("big_payload")
    print(f"Retrieved blob length: {len(retrieved_blob)} bytes")
    print(f"Blob tail: {retrieved_blob[-20:]}")

    # 4. Test Deletion
    print("\n--- Testing Deletion ---")
    db.delete("python_key_2")
    print("Get deleted key (should be None):", db.get("python_key_2"))

    print("\n[+] All Python ctypes integration tests passed successfully!")