from fastapi._compat import value_is_sequence
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import sys
import os

# Making sure Python can db.py in the parent directory
sys.path.append(os.path.abspath(os.path.join(os.path.dirname(__file__), "../")))
from db import KeyValueStore

app = FastAPI(title="PyC-KVStore API", version="1.0")

# Enabling CORS so JS and React can connect from different ports
app.add_middleware(
    CORSMiddleware,
    allow_origins = ["*"],
    allow_credentials = True,
    allow_methods = ["*"],
    allow_headers = ["*"],
)

# Initializing the C-backed Key-Value Store instance globally
db = KeyValueStore("database.db", "database.wal")

# Pydantic request models for data validation
class KVRequest(BaseModel):
    key: str
    value: str

class UpdateRequest(BaseModel):
    value: str

@app.get("/")
def read_root():
    return {
        "status": "online",
        "message": "PyC-KVStore Storage Engine API is running."
    }

@app.post("/api/put")
def put_key_value(data: KVRequest):
    try:
        db.put(data.key, data.value)
        return {
            "success": True,
            "message": f"Key '{data.key}' stored successfully."
        }
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/get/{key}")
def get_key_value(key: str):
    val = db.get(key)
    if val is None:
        raise HTTPException(status_code=404, detail=f"Key '{key}' not found.")
    return {
        "success": True,
        "key": key,
        "value": val
    }

@app.delete("/api/delete/{key}")
def delete_key_value(key: str):
    try:
        db.update(key)
        return {
            "success": True,
            "message": f"Key '{key}' deleted successfully."
        }
    except KeyError:
        raise HTTPException(status_code=404, detail=f"Key '{key}' not found.")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.put("/api/update/{key}")
def update_key_value(key: str, data: UpdateRequest):
    try:
        db.update(key, data.value)
        return {
            "success": True,
            "message": f"Key '{key}' updated successfully."
        }
    except KeyError:
        raise HTTPException(status_code=404, detail=f"Key '{key}' not found.")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/stats")
def get_database_stats():
    # Getting telemetry info from FSM IndexTable for the front-end
    last_page_id = db.fsm.last_page_id
    return {
        "last_page_id": last_page_id,
        "total_keys": db.index.count,
        "free_bytes": [db.fsm.free_bytes[i] for i in range(last_page_id + 1)]
    }