const browserStatus = document.querySelector("#browserStatus");
const connectButton = document.querySelector("#connectButton");
const runButton = document.querySelector("#runButton");
const jsonButton = document.querySelector("#jsonButton");
const submitButton = document.querySelector("#submitButton");
const clearLogButton = document.querySelector("#clearLogButton");
const clearResultsButton = document.querySelector("#clearResultsButton");
const serialLog = document.querySelector("#serialLog");
const resultState = document.querySelector("#resultState");
const resultSummary = document.querySelector("#resultSummary");
const resultsTable = document.querySelector("#resultsTable");

const storageKey = "espmark.localResults.v1";

let port;
let writer;
let currentResult;
let textBuffer = "";
let jsonCapture = "";
let capturingJson = false;

function setBrowserStatus() {
  if ("serial" in navigator) {
    browserStatus.textContent = "Web Serial supported";
    browserStatus.classList.add("ok");
    return;
  }

  browserStatus.textContent = "Use Chrome or Edge";
  browserStatus.classList.add("bad");
  connectButton.disabled = true;
}

function appendLog(text) {
  if (serialLog.textContent === "Connect an ESP32 board to begin.") {
    serialLog.textContent = "";
  }
  serialLog.textContent += text;
  serialLog.scrollTop = serialLog.scrollHeight;
}

function metricLabel(testId) {
  const labels = {
    "cpu.integer.add_mul.u32": "add/mul",
    "cpu.integer.div_mod.u32": "div/mod",
    "cpu.integer.branch.u32": "branch",
    "cpu.integer.crc_like.u32": "crc-like",
  };
  return labels[testId] || testId;
}

function metricValue(result, testId) {
  const metric = result.results.find((item) => item.test_id === testId);
  return metric ? metric.mean.toFixed(3) : "-";
}

function renderCurrentResult(result) {
  currentResult = result;
  resultState.textContent = "Ready to save";
  submitButton.disabled = false;

  resultSummary.classList.remove("empty");
  resultSummary.innerHTML = "";

  const header = document.createElement("div");
  header.className = "metric-card";
  header.innerHTML = `
    <div>
      <strong>${result.board.name}</strong><br>
      <span>${result.board.soc} rev ${result.board.revision}, ${result.config.cpu_freq_mhz} MHz</span>
    </div>
    <span>${result.build.sdk_version}</span>
  `;
  resultSummary.appendChild(header);

  for (const metric of result.results) {
    const card = document.createElement("div");
    card.className = "metric-card";
    card.innerHTML = `
      <div>
        <strong>${metricLabel(metric.test_id)}</strong><br>
        <span>mean, lower is better</span>
      </div>
      <strong>${metric.mean.toFixed(3)} ${metric.unit}</strong>
    `;
    resultSummary.appendChild(card);
  }
}

function parseSerialLine(line) {
  const trimmed = line.trim();

  if (trimmed === "ESPMARK_RESULT_BEGIN") {
    capturingJson = true;
    jsonCapture = "";
    resultState.textContent = "Reading JSON";
    return;
  }

  if (trimmed === "ESPMARK_RESULT_END" && capturingJson) {
    capturingJson = false;
    try {
      renderCurrentResult(JSON.parse(jsonCapture));
    } catch (error) {
      resultState.textContent = "JSON parse failed";
      appendLog(`\n[web] JSON parse failed: ${error.message}\n`);
    }
    return;
  }

  if (capturingJson) {
    jsonCapture += line + "\n";
  }
}

function handleSerialText(text) {
  appendLog(text);
  textBuffer += text;

  let newlineIndex = textBuffer.indexOf("\n");
  while (newlineIndex !== -1) {
    const line = textBuffer.slice(0, newlineIndex);
    textBuffer = textBuffer.slice(newlineIndex + 1);
    parseSerialLine(line);
    newlineIndex = textBuffer.indexOf("\n");
  }
}

async function readLoop() {
  const decoder = new TextDecoderStream();
  port.readable.pipeTo(decoder.writable).catch(() => {});
  const reader = decoder.readable.getReader();

  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) {
        break;
      }
      if (value) {
        handleSerialText(value);
      }
    }
  } finally {
    reader.releaseLock();
  }
}

async function sendSerial(text) {
  if (!writer) {
    return;
  }
  await writer.write(new TextEncoder().encode(text));
}

async function connectSerial() {
  port = await navigator.serial.requestPort();
  await port.open({ baudRate: 115200 });
  writer = port.writable.getWriter();
  connectButton.disabled = true;
  runButton.disabled = false;
  jsonButton.disabled = false;
  resultState.textContent = "Connected";
  appendLog("[web] Connected. If the board asks for Enter, click Start benchmark.\n");
  readLoop();
}

function loadSavedResults() {
  try {
    return JSON.parse(localStorage.getItem(storageKey) || "[]");
  } catch {
    return [];
  }
}

function saveResults(results) {
  localStorage.setItem(storageKey, JSON.stringify(results));
}

function renderResultsTable() {
  const rows = loadSavedResults();
  resultsTable.innerHTML = "";

  if (rows.length === 0) {
    const row = document.createElement("tr");
    row.innerHTML = `<td colspan="8">No saved local results yet.</td>`;
    resultsTable.appendChild(row);
    return;
  }

  for (const rowResult of rows) {
    const row = document.createElement("tr");
    row.innerHTML = `
      <td>${rowResult.board.name}</td>
      <td>${rowResult.board.soc} r${rowResult.board.revision}</td>
      <td>${rowResult.config.cpu_freq_mhz} MHz</td>
      <td>${metricValue(rowResult, "cpu.integer.add_mul.u32")}</td>
      <td>${metricValue(rowResult, "cpu.integer.div_mod.u32")}</td>
      <td>${metricValue(rowResult, "cpu.integer.branch.u32")}</td>
      <td>${metricValue(rowResult, "cpu.integer.crc_like.u32")}</td>
      <td>${new Date(rowResult.saved_at).toLocaleString()}</td>
    `;
    resultsTable.appendChild(row);
  }
}

connectButton.addEventListener("click", () => {
  connectSerial().catch((error) => {
    resultState.textContent = "Connection failed";
    appendLog(`[web] Connection failed: ${error.message}\n`);
  });
});

runButton.addEventListener("click", () => {
  sendSerial("\n");
});

jsonButton.addEventListener("click", () => {
  sendSerial("j\n");
});

submitButton.addEventListener("click", () => {
  if (!currentResult) {
    return;
  }

  const rows = loadSavedResults();
  rows.unshift({ ...currentResult, saved_at: new Date().toISOString() });
  saveResults(rows.slice(0, 50));
  renderResultsTable();
  resultState.textContent = "Saved locally";
});

clearLogButton.addEventListener("click", () => {
  serialLog.textContent = "";
});

clearResultsButton.addEventListener("click", () => {
  localStorage.removeItem(storageKey);
  renderResultsTable();
});

setBrowserStatus();
renderResultsTable();
