const API_URL = "http://127.0.0.1:8000/api"

async function handlePut() {
    const keyInput = document.getElementById("put-key");
    const key = keyInput.value.trim();

    const valInput = document.getElementById("put-value");
    const value = valInput.value.trim();

    if (!key || !value) return alert("Key or value cannot be empty!");

    try {
        const res = await fetch(
            `${API_URL}/put`, {
            method: "POST",
            headers: {
                "Content-Type": "application/json"
            },
            body: JSON.stringify({ key, value })
        }
        );
        const data = await res.json();
        if (res.ok) {
            alert(data.message);
            // Clearing inputs on success
            keyInput.value = "";
            valInput.value = "";
        }
        else alert("Error: " + data.detail);
    } catch (err) {
        alert("Failed to connect to backend server.");
    }
}

async function handleGet() {
    const key = document.getElementById("get-key").value.trim();
    const resultBox = document.getElementById("get-result");
    if (!key) return alert("Enter a key to lookup!");

    try {
        const res = await fetch(`${API_URL}/get/${key}`);
        const data = await res.json();
        if (res.ok) {
            resultBox.innerText = data.value;
        } else {
            resultBox.innerText = "[-] " + data.detail;
        }
    } catch (err) {
        resultBox.innerText = "[-] Network error reaching backend.";
    }
}

async function handleDelete() {
    const keyInput = document.getElementById("del-key");
    const key = keyInput.value.trim();

    if (!key) return alert("Enter a key to delete!");

    try {
        const res = await fetch(
            `${API_URL}/delete/${key}`, {
            method: "DELETE"
        }
        );
        const data = await res.json();
        if (res.ok) {
            alert(data.message);
            // Clearing input on success
            keyInput.value = "";
        }
        else alert("Error: " + data.detail);
    } catch (err) {
        alert("Failed to connect to backend server.");
    }
}

async function loadStats() {
    try {
        const res = await fetch(`${API_URL}/stats`);
        const data = await res.json();
        document.getElementById("stats-output").innerText = JSON.stringify(data, null, 2);
    } catch (err) {
        document.getElementById("stats-output").innerText = "[-] Failed to fetch database stats.";
    }
}