import { useState } from 'react';
import './App.css';

const API_URL = "http://127.0.0.1:8000/api";

function App() {
  // State for forms
  const [putKey, setPutKey] = useState("");
  const [putVal, setPutVal] = useState("");
  const [getKey, setGetKey] = useState("");
  const [getResult, setGetResult] = useState("Result will appear here...");
  const [delKey, setDelKey] = useState("");
  const [stats, setStats] = useState("Click refresh to load storage metrics...");

  const handlePut = async () => {
    if (!putKey || !putVal) return alert("Key and value cannot be empty!");
    try {
      const res = await fetch(`${API_URL}/put`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ key: putKey, value: putVal })
      });
      const data = await res.json();
      if (res.ok) {
        alert(data.message);
        setPutKey("");
        setPutVal("");
      } else {
        alert("Error: " + data.detail);
      }
    } catch {
      alert("Failed to connect to backend server.");
    }
  };

  const handleGet = async () => {
    if (!getKey) return alert("Enter a key to lookup!");
    try {
      const res = await fetch(`${API_URL}/get/${getKey}`);
      const data = await res.json();
      if (res.ok) {
        setGetResult(data.value);
      } else {
        setGetResult("[-] " + data.detail);
      }
    } catch {
      setGetResult("[-] Network error reaching backend.");
    }
  };

  const handleDelete = async () => {
    if (!delKey) return alert("Enter a key to delete!");
    try {
      const res = await fetch(`${API_URL}/delete/${delKey}`, { method: "DELETE" });
      const data = await res.json();
      if (res.ok) {
        alert(data.message);
        setDelKey("");
      } else {
        alert("Error: " + data.detail);
      }
    } catch {
      alert("Failed to connect to backend server.");
    }
  };

  const loadStats = async () => {
    try {
      const res = await fetch(`${API_URL}/stats`);
      const data = await res.json();
      setStats(JSON.stringify(data, null, 2));
    } catch {
      setStats("[-] Failed to fetch database stats.");
    }
  };

  return (
    <div className="container">
      <h1>PyC-KVStore Dashboard</h1>
      <p className="subtitle">React Component-Based Interface</p>

      <div className="grid">
        {/* Insert / Update Card */}
        <div className="card">
          <h2>Insert / Update</h2>
          <input 
            type="text" 
            placeholder="Key" 
            value={putKey} 
            onChange={(e) => setPutKey(e.target.value)} 
          />
          <textarea 
            placeholder="Value (supports massive blobs)" 
            value={putVal} 
            onChange={(e) => setPutVal(e.target.value)} 
          />
          <button onClick={handlePut}>Save Record</button>
        </div>

        {/* Lookup Card */}
        <div className="card">
          <h2>Lookup Record</h2>
          <input 
            type="text" 
            placeholder="Key" 
            value={getKey} 
            onChange={(e) => setGetKey(e.target.value)} 
          />
          <button onClick={handleGet}>Fetch Value</button>
          <div className="result-box">{getResult}</div>
        </div>

        {/* Delete Card */}
        <div className="card">
          <h2>Delete Record</h2>
          <input 
            type="text" 
            placeholder="Key" 
            value={delKey} 
            onChange={(e) => setDelKey(e.target.value)} 
          />
          <button onClick={handleDelete} className="btn-danger">Delete Key</button>
        </div>
      </div>

      {/* Telemetry & FSM Stats */}
      <div className="card full-width">
        <h2>Storage Telemetry & FSM Stats</h2>
        <button onClick={loadStats}>Refresh Database Stats</button>
        <pre>{stats}</pre>
      </div>
    </div>
  );
}

export default App;