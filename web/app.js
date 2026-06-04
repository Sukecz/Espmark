const themeButton = document.querySelector("#themeButton");
const connectButton = document.querySelector("#connectButton");
const runButton = document.querySelector("#runButton");
const jsonButton = document.querySelector("#jsonButton");
const submitButton = document.querySelector("#submitButton");
const clearLogButton = document.querySelector("#clearLogButton");
const refreshResultsButton = document.querySelector("#refreshResultsButton");
const serialLog = document.querySelector("#serialLog");
const resultState = document.querySelector("#resultState");
const resultSummary = document.querySelector("#resultSummary");
const resultsTable = document.querySelector("#resultsTable");
const contributorName = document.querySelector("#contributorName");
const companyName = document.querySelector("#companyName");
const botQuestion = document.querySelector("#botQuestion");
const botCheck = document.querySelector("#botCheck");
const submitStatus = document.querySelector("#submitStatus");
const connectionHint = document.querySelector("#connectionHint");

const hiddenSerialLines = new Set([
  "Press 'j' then Enter to print JSON for sharing.",
  "Press Enter to run the benchmark again.",
]);

let port;
let writer;
let currentResult;
let textBuffer = "";
let jsonCapture = "";
let capturingJson = false;
let autoRequestingJson = false;
let botAnswer = 0;
let formRenderedAt = Date.now();

function applyTheme(theme) {
  document.documentElement.dataset.theme = theme;
  localStorage.setItem("espmark.theme", theme);
  themeButton.textContent = theme === "auto" ? "Auto" : theme === "dark" ? "Dark" : "Light";
}

function initTheme() {
  applyTheme(localStorage.getItem("espmark.theme") || "auto");
}

function nextTheme() {
  const current = document.documentElement.dataset.theme || "auto";
  applyTheme(current === "auto" ? "dark" : current === "dark" ? "light" : "auto");
}

function setupBotCheck() {
  const a = 2 + Math.floor(Math.random() * 8);
  const b = 2 + Math.floor(Math.random() * 8);
  botAnswer = a + b;
  botQuestion.textContent = `${a} + ${b} =`;
  formRenderedAt = Date.now();
}

function setSerialSupport() {
  if ("serial" in navigator) {
    connectionHint.textContent = "Connect the board over USB, then start the benchmark.";
    return;
  }

  connectionHint.textContent = "Web Serial is unavailable. Use Chrome or Edge over HTTPS.";
  connectButton.disabled = true;
}

function appendLogLine(line) {
  const trimmed = line.trim();
  if (!trimmed || hiddenSerialLines.has(trimmed) || capturingJson || trimmed.startsWith("ESPMARK_RESULT_")) {
    return;
  }

  if (serialLog.textContent === "Connect an ESP32 board to begin.") {
    serialLog.textContent = "";
  }
  serialLog.textContent += `${line.replace(/\r$/, "")}\n`;
  serialLog.scrollTop = serialLog.scrollHeight;
}

function appendSystemLog(message) {
  if (serialLog.textContent === "Connect an ESP32 board to begin.") {
    serialLog.textContent = "";
  }
  serialLog.textContent += `[web] ${message}\n`;
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
  resultState.textContent = "Ready to submit";
  submitStatus.textContent = "Enter your name and bot check, then save the result.";
  submitButton.disabled = false;
  jsonButton.disabled = false;
  autoRequestingJson = false;

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
      appendSystemLog(`JSON parse failed: ${error.message}`);
    }
    return;
  }

  if (trimmed === "Press 'j' then Enter to print JSON for sharing.") {
    if (!autoRequestingJson) {
      autoRequestingJson = true;
      resultState.textContent = "Reading result";
      jsonButton.disabled = true;
      sendSerial("j\n");
    }
    return;
  }

  if (capturingJson) {
    jsonCapture += line + "\n";
  }
}

function handleSerialText(text) {
  textBuffer += text;

  let newlineIndex = textBuffer.indexOf("\n");
  while (newlineIndex !== -1) {
    const line = textBuffer.slice(0, newlineIndex);
    textBuffer = textBuffer.slice(newlineIndex + 1);
    parseSerialLine(line);
    appendLogLine(line);
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
  appendSystemLog("Connected. Start the benchmark from the control panel.");
  readLoop();
}

async function fetchResults() {
  const response = await fetch("/api/results");
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

async function submitResult() {
  if (!currentResult) {
    return;
  }

  const name = contributorName.value.trim();
  const answer = Number(botCheck.value.trim());

  if (name.length < 2) {
    submitStatus.textContent = "Enter a name or nickname.";
    return;
  }
  if (answer !== botAnswer) {
    submitStatus.textContent = "Bot check answer is incorrect.";
    return;
  }

  submitButton.disabled = true;
  submitStatus.textContent = "Saving result to server...";

  const response = await fetch("/api/results", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      contributor: name,
      bot_field: companyName.value,
      form_elapsed_ms: Date.now() - formRenderedAt,
      result: currentResult,
    }),
  });

  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: `HTTP ${response.status}` }));
    submitStatus.textContent = `Save failed: ${error.error}`;
    submitButton.disabled = false;
    return;
  }

  submitStatus.textContent = "Saved to espmark server.";
  setupBotCheck();
  botCheck.value = "";
  await renderResultsTable();
}

async function renderResultsTable() {
  resultsTable.innerHTML = `<tr><td colspan="9">Loading results...</td></tr>`;

  try {
    const rows = await fetchResults();

    resultsTable.innerHTML = "";
    if (rows.length === 0) {
      resultsTable.innerHTML = `<tr><td colspan="9">No submitted results yet.</td></tr>`;
      return;
    }

    for (const rowResult of rows) {
      const row = document.createElement("tr");
      row.innerHTML = `
        <td>${rowResult.contributor}</td>
        <td>${rowResult.result.board.name}</td>
        <td>${rowResult.result.board.soc} r${rowResult.result.board.revision}</td>
        <td>${rowResult.result.config.cpu_freq_mhz} MHz</td>
        <td>${metricValue(rowResult.result, "cpu.integer.add_mul.u32")}</td>
        <td>${metricValue(rowResult.result, "cpu.integer.div_mod.u32")}</td>
        <td>${metricValue(rowResult.result, "cpu.integer.branch.u32")}</td>
        <td>${metricValue(rowResult.result, "cpu.integer.crc_like.u32")}</td>
        <td>${new Date(rowResult.submitted_at).toLocaleString()}</td>
      `;
      resultsTable.appendChild(row);
    }
  } catch (error) {
    resultsTable.innerHTML = `<tr><td colspan="9">Could not load results: ${error.message}</td></tr>`;
  }
}

themeButton.addEventListener("click", nextTheme);

connectButton.addEventListener("click", () => {
  connectSerial().catch((error) => {
    resultState.textContent = "Connection failed";
    appendSystemLog(`Connection failed: ${error.message}`);
  });
});

runButton.addEventListener("click", () => {
  currentResult = undefined;
  submitButton.disabled = true;
  resultState.textContent = "Benchmark running";
  submitStatus.textContent = "A completed benchmark result is required.";
  resultSummary.classList.add("empty");
  resultSummary.textContent = "Waiting for benchmark output from the board.";
  sendSerial("\n");
});

jsonButton.addEventListener("click", () => {
  sendSerial("j\n");
});

submitButton.addEventListener("click", () => {
  submitResult().catch((error) => {
    submitStatus.textContent = `Save failed: ${error.message}`;
    submitButton.disabled = false;
  });
});

clearLogButton.addEventListener("click", () => {
  serialLog.textContent = "";
});

refreshResultsButton.addEventListener("click", renderResultsTable);

initTheme();
setupBotCheck();
setSerialSupport();
renderResultsTable();
