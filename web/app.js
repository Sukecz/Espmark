const themeButtons = Array.from(document.querySelectorAll("[data-theme-choice]"));
const connectButton = document.querySelector("#connectButton");
const runButton = document.querySelector("#runButton");
const jsonButton = document.querySelector("#jsonButton");
const submitButton = document.querySelector("#submitButton");
const clearLogButton = document.querySelector("#clearLogButton");
const refreshResultsButton = document.querySelector("#refreshResultsButton");
const serialLog = document.querySelector("#serialLog");
const connectionState = document.querySelector("#connectionState");
const resultState = document.querySelector("#resultState");
const resultSummary = document.querySelector("#resultSummary");
const resultsTable = document.querySelector("#resultsTable");
const contributorName = document.querySelector("#contributorName");
const exactBoard = document.querySelector("#exactBoard");
const boardSuggestions = document.querySelector("#boardSuggestions");
const customBoardToggle = document.querySelector("#customBoardToggle");
const companyName = document.querySelector("#companyName");
const botQuestion = document.querySelector("#botQuestion");
const botCheck = document.querySelector("#botCheck");
const submitStatus = document.querySelector("#submitStatus");
const connectionHint = document.querySelector("#connectionHint");
const reportContact = document.querySelector("#reportContact");
const reportMessage = document.querySelector("#reportMessage");
const reportCompany = document.querySelector("#reportCompany");
const reportButton = document.querySelector("#reportButton");
const reportStatus = document.querySelector("#reportStatus");
const reportDialog = document.querySelector("#reportDialog");
const openReportDialogButton = document.querySelector("#openReportDialog");
const closeReportDialogButton = document.querySelector("#closeReportDialog");
const reportBotQuestion = document.querySelector("#reportBotQuestion");
const reportBotCheck = document.querySelector("#reportBotCheck");
const sortButtons = Array.from(document.querySelectorAll("[data-sort-key]"));

const hiddenSerialLines = new Set([
  "Press 'j' then Enter to print JSON for sharing.",
  "Press Enter to run the benchmark again.",
  "Click Run benchmark in the web UI, or press Enter to run again.",
]);
const emptySerialMessage = "Connect an ESP board to begin.";

let port;
let writer;
let currentResult;
let currentSubmission;
let currentPreviewRequest = 0;
let boardCatalog;
let textBuffer = "";
let jsonCapture = "";
let capturingJson = false;
let autoRequestingJson = false;
let autoJsonTimer;
let challengeToken = "";
let reportChallengeToken = "";
let formRenderedAt = Date.now();
let reportFormRenderedAt = Date.now();
let loadedResults = [];
let resultsSortKey = "submitted_at";
let resultsSortDirection = "desc";

const benchmarkTests = {
  "cpu.integer.add_mul.u32": {
    category: "cpu",
    label: "Basic math",
    detail: "Measures simple 32-bit integer arithmetic used in normal control loops and counters.",
    baselineUs: 1000,
    scoreWeight: 4.5,
  },
  "cpu.integer.div_mod.u32": {
    category: "cpu",
    label: "Hard math",
    detail: "Measures slower integer division and modulo operations that often appear in parsers and conversions.",
    baselineUs: 1000,
    scoreWeight: 4.5,
  },
  "cpu.integer.branch.u32": {
    category: "cpu",
    label: "Decision speed",
    detail: "Measures branch-heavy code where the processor has to make many small decisions.",
    baselineUs: 1000,
    scoreWeight: 4.5,
  },
  "cpu.integer.crc_like.u32": {
    category: "cpu",
    label: "Data crunching",
    detail: "Measures bit-level integer processing similar to checksums and protocol work.",
    baselineUs: 1000,
    scoreWeight: 4.5,
  },
  "memory.ram.memcpy.seq": {
    category: "memory",
    label: "RAM copy",
    detail: "Measures sequential copying of RAM buffers.",
    baselineUs: 1000,
    scoreWeight: 6.67,
  },
  "memory.ram.memset.seq": {
    category: "memory",
    label: "RAM fill",
    detail: "Measures how quickly the board fills RAM buffers with fixed values.",
    baselineUs: 1000,
    scoreWeight: 6.67,
  },
  "memory.ram.read.strided": {
    category: "memory",
    label: "RAM read",
    detail: "Measures strided reads across RAM, which is sensitive to memory/cache behavior.",
    baselineUs: 1000,
    scoreWeight: 6.67,
  },
  "memory.heap.malloc_free.128b": {
    category: "memory",
    label: "Small allocations",
    detail: "Measures repeated small heap allocations and frees.",
    baselineUs: 1000,
    scoreWeight: 6,
  },
  "memory.heap.fragmentation": {
    category: "memory",
    label: "Heap fragmentation",
    detail: "Measures a mixed allocation/free pattern that can reveal heap fragmentation overhead.",
    baselineUs: 1000,
    scoreWeight: 4,
  },
  "cpu.sustained.mix": {
    category: "cpu",
    label: "Sustained CPU",
    detail: "Measures a longer mixed integer workload with watchdog-safe yield points.",
    baselineUs: 1000,
    scoreWeight: 8,
  },
  "cpu.float32.affine": {
    category: "cpu",
    label: "Float32",
    detail: "Measures single-precision floating point math used by filters and sensor calculations.",
    baselineUs: 1000,
    scoreWeight: 6,
  },
  "cpu.mandelbrot.q16": {
    category: "cpu",
    label: "Mandelbrot",
    detail: "Measures a deterministic fixed-point compute workload, closer to a real algorithm than a microtest.",
    baselineUs: 1000,
    scoreWeight: 8,
  },
  "cpu.matrix.i16": {
    category: "cpu",
    label: "Matrix",
    detail: "Supplemental integer matrix multiply workload; shown for detail, not part of the headline score.",
    baselineUs: 1000,
    scoreWeight: 0,
  },
  "flash.read.seq": {
    category: "flash",
    label: "Flash read",
    detail: "Measures read-only sequential access to data stored in flash.",
    baselineUs: 1000,
    scoreWeight: 10,
  },
  "practical.crc32.sw": {
    category: "practical_iot",
    label: "CRC32",
    detail: "Measures portable software CRC32 over a RAM buffer.",
    baselineUs: 1000,
    scoreWeight: 3,
  },
  "practical.sha256.sw": {
    category: "practical_iot",
    label: "SHA-256",
    detail: "Measures portable software SHA-256 without using hardware crypto acceleration.",
    baselineUs: 1000,
    scoreWeight: 7,
  },
  "practical.json.roundtrip": {
    category: "practical_iot",
    label: "JSON",
    detail: "Measures generating and reading a fixed IoT-style JSON payload.",
    baselineUs: 1000,
    scoreWeight: 7,
  },
  "practical.string.format": {
    category: "practical_iot",
    label: "Strings",
    detail: "Measures fixed-buffer text formatting for logs, diagnostics and payloads.",
    baselineUs: 1000,
    scoreWeight: 3,
  },
};

const categoryWeights = {
  cpu: 40,
  memory: 30,
  flash: 10,
  practical_iot: 20,
};

const categoryCopy = {
  cpu: {
    title: "CPU score",
    subtitle: "Processor work: math, branching and bit processing.",
  },
  memory: {
    title: "Memory score",
    subtitle: "RAM and heap work: copy, fill, read and small allocations.",
  },
  flash: {
    title: "Flash score",
    subtitle: "Read-only flash workload kept separate from storage writes.",
  },
  practical_iot: {
    title: "Practical IoT score",
    subtitle: "Payload, checksum and text workloads common in embedded projects.",
  },
};

const requiredCoreTests = [
  "cpu.integer.add_mul.u32",
  "cpu.integer.div_mod.u32",
  "cpu.integer.branch.u32",
  "cpu.integer.crc_like.u32",
  "cpu.sustained.mix",
  "cpu.float32.affine",
  "cpu.mandelbrot.q16",
  "memory.ram.memcpy.seq",
  "memory.ram.memset.seq",
  "memory.ram.read.strided",
  "memory.heap.malloc_free.128b",
  "memory.heap.fragmentation",
  "flash.read.seq",
  "practical.sha256.sw",
  "practical.crc32.sw",
  "practical.json.roundtrip",
  "practical.string.format",
];

function setTheme(theme) {
  document.documentElement.dataset.theme = theme;
  localStorage.setItem("espmark.theme", theme);

  for (const button of themeButtons) {
    const isActive = button.dataset.themeChoice === theme;
    button.setAttribute("aria-pressed", String(isActive));
  }
}

function initTheme() {
  const savedTheme = localStorage.getItem("espmark.theme");
  setTheme(["auto", "light", "dark"].includes(savedTheme) ? savedTheme : "auto");
}

async function loadChallenge(target) {
  try {
    const response = await fetch("/api/challenge", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    return response.json();
  } catch (error) {
    target.textContent = "Unavailable";
    throw error;
  }
}

async function setupBotCheck() {
  if (!botQuestion) {
    return;
  }
  botQuestion.textContent = "Loading...";
  challengeToken = "";
  formRenderedAt = Date.now();

  try {
    const challenge = await loadChallenge(botQuestion);
    challengeToken = challenge.token;
    botQuestion.textContent = challenge.question;
  } catch (error) {
    botQuestion.textContent = "Unavailable";
    submitStatus.textContent = `Bot check could not load: ${error.message}`;
  }
}

async function setupReportBotCheck() {
  if (!reportBotQuestion) {
    return;
  }
  reportBotQuestion.textContent = "Loading...";
  reportChallengeToken = "";
  reportFormRenderedAt = Date.now();

  try {
    const challenge = await loadChallenge(reportBotQuestion);
    reportChallengeToken = challenge.token;
    reportBotQuestion.textContent = challenge.question;
  } catch (error) {
    reportBotQuestion.textContent = "Unavailable";
    if (reportStatus) {
      reportStatus.textContent = `Bot check could not load: ${error.message}`;
    }
  }
}

function setSerialSupport() {
  if (!connectButton || !connectionHint) {
    return;
  }
  if ("serial" in navigator) {
    connectionHint.textContent = "Attach the board over USB, choose the serial port, then run the benchmark.";
    return;
  }

  connectionHint.textContent = "Web Serial is unavailable here. Use Chrome or Edge over HTTPS.";
  connectButton.disabled = true;
}

function appendLogLine(line) {
  if (!serialLog) {
    return;
  }
  const trimmed = line.trim();
  if (!trimmed || hiddenSerialLines.has(trimmed) || capturingJson || trimmed.startsWith("ESPMARK_RESULT_")) {
    return;
  }

  if (serialLog.textContent === emptySerialMessage) {
    serialLog.textContent = "";
  }
  serialLog.textContent += `${line.replace(/\r$/, "")}\n`;
  serialLog.scrollTop = serialLog.scrollHeight;
}

function appendSystemLog(message) {
  if (!serialLog) {
    return;
  }
  if (serialLog.textContent === emptySerialMessage) {
    serialLog.textContent = "";
  }
  serialLog.textContent += `[web] ${message}\n`;
  serialLog.scrollTop = serialLog.scrollHeight;
}

function metricLabel(testId) {
  return benchmarkTests[testId]?.label || testId;
}

function metricDescription(metric) {
  return benchmarkTests[metric.test_id]?.detail || "raw benchmark timing";
}

function metricUnit(metric) {
  return metric.unit === "us" ? "us" : metric.unit;
}

function formatMetricTime(metric) {
  const value = Number.isFinite(metric.median) ? metric.median : metric.mean;
  if (!Number.isFinite(value)) {
    return "-";
  }
  if (metricUnit(metric) === "us" && value >= 1000) {
    const ms = value / 1000;
    return `${ms.toFixed(ms >= 10 ? 0 : 1).replace(/\.0$/, "")} ms`;
  }
  return `${Math.round(value)} ${metricUnit(metric)}`;
}

function metricValue(result, testId) {
  const metric = result.results.find((item) => item.test_id === testId);
  return metric ? formatMetricTime(metric) : "-";
}

function metricsByCategory(result, category) {
  return result.results.filter((metric) => {
    return metric.category === category || benchmarkTests[metric.test_id]?.category === category;
  });
}

function categoryScore(result, category) {
  const scores = metricsByCategory(result, category)
    .map((metric) => {
      const test = benchmarkTests[metric.test_id];
      const baselineUs = test?.baselineUs;
      const weight = test?.scoreWeight ?? 1;
      if (weight <= 0) {
        return undefined;
      }
      if (!baselineUs || !Number.isFinite(metric.mean) || metric.mean <= 0) {
        return undefined;
      }
      return { ratio: baselineUs / metric.mean, weight };
    })
    .filter((score) => Number.isFinite(score?.ratio) && score.ratio > 0 && score.weight > 0);

  if (scores.length === 0) {
    return undefined;
  }

  const totalWeight = scores.reduce((sum, score) => sum + score.weight, 0);
  const geometricMean = Math.exp(scores.reduce((sum, score) => sum + score.weight * Math.log(score.ratio), 0) / totalWeight);
  return Math.round(geometricMean * 1000);
}

function formatScore(score) {
  return Number.isFinite(score) ? String(score) : "-";
}

function formatScore1(score) {
  return Number.isFinite(score) ? String(Math.round(score)) : "-";
}

function formatScoreNumber(score) {
  return Number.isFinite(score) ? String(Math.round(score)) : "-";
}

function formatScoreValue(record, key) {
  const value = scoreFromRecord(record, key);
  if (!Number.isFinite(value)) {
    return "-";
  }
  if (key === "stability_factor") {
    return value.toFixed(3);
  }
  return String(Math.round(value));
}

function rawResultFromRecord(record) {
  return record?.raw_result || record?.result;
}

function scoreFromRecord(record, key) {
  const value = record?.scores?.[key];
  return typeof value === "number" ? value : undefined;
}

function localEspmarkScore(result) {
  if (missingRequiredTests(result).length > 0) {
    return undefined;
  }
  const inputs = ["cpu", "memory", "flash", "practical_iot"]
    .map((category) => ({ score: categoryScore(result, category), weight: categoryWeights[category] }))
    .filter((item) => Number.isFinite(item.score) && item.score > 0 && item.weight > 0);
  if (inputs.length === 0) {
    return undefined;
  }
  const totalWeight = inputs.reduce((sum, item) => sum + item.weight, 0);
  return Math.round(Math.exp(inputs.reduce((sum, item) => sum + item.weight * Math.log(item.score / 1000), 0) / totalWeight) * 1000);
}

function missingRequiredTests(result) {
  const submitted = new Set((result?.results || []).map((metric) => metric.test_id));
  return requiredCoreTests.filter((testId) => !submitted.has(testId));
}

function metricStatsLine(metric) {
  return `Higher score is better. Median runtime: ${formatMetricTime(metric)}.`;
}

function normalizeSoc(soc) {
  const normalized = String(soc || "").trim().toUpperCase().replace(/^ESP32([A-Z])/, "ESP32-$1");
  if (/^ESP32($|-D|-U|-PICO)/.test(normalized)) {
    return "ESP32";
  }
  return normalized;
}

function boardsForResult(result) {
  const soc = normalizeSoc(result?.board?.soc || result?.board?.module);
  return boardCatalog?.families?.[soc] || [];
}

function selectedBoard() {
  if (customBoardToggle.checked) {
    const name = exactBoard.value.trim();
    if (name.length < 2) {
      return undefined;
    }
    const soc = normalizeSoc(currentResult?.board?.soc || currentResult?.board?.module);
    return {
      id: `custom:${soc}:${name.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "")}`,
      name,
      source: "user",
      source_id: "",
      vendor: "User supplied",
      custom: true,
    };
  }

  const value = exactBoard.value.trim().toLowerCase();
  return boardsForResult(currentResult).find((board) => {
    if (board.name.toLowerCase() === value) {
      return true;
    }
    return (board.aliases || []).some((alias) => alias.name.toLowerCase() === value);
  });
}

function setBoardSelectWaiting(message) {
  if (!exactBoard || !customBoardToggle || !boardSuggestions) {
    return;
  }
  exactBoard.value = "";
  exactBoard.placeholder = message;
  exactBoard.disabled = true;
  customBoardToggle.checked = false;
  customBoardToggle.disabled = true;
  boardSuggestions.innerHTML = "";
}

function populateBoardSelect(result) {
  if (!exactBoard || !customBoardToggle || !boardSuggestions) {
    return;
  }
  const boards = boardsForResult(result);
  boardSuggestions.innerHTML = "";
  exactBoard.value = "";
  exactBoard.setAttribute("list", "boardSuggestions");
  customBoardToggle.checked = false;
  customBoardToggle.disabled = false;

  if (!boardCatalog) {
    exactBoard.placeholder = "Enter board name";
    exactBoard.disabled = false;
    return;
  }

  if (boards.length === 0) {
    customBoardToggle.checked = true;
    exactBoard.placeholder = `Enter ${normalizeSoc(result.board.soc) || "board"} name`;
    exactBoard.disabled = false;
    return;
  }

  exactBoard.placeholder = `Search ${normalizeSoc(result.board.soc)} boards`;

  for (const board of boards) {
    const option = document.createElement("option");
    option.value = board.name;
    const aliasNames = (board.aliases || []).map((alias) => alias.name).join(", ");
    option.label = [board.vendor, aliasNames].filter(Boolean).join(" - ");
    boardSuggestions.appendChild(option);
  }

  const currentName = String(result.board.name || "").toLowerCase();
  const matchingBoard = boards.find((board) => board.name.toLowerCase() === currentName);
  exactBoard.value = matchingBoard?.name || "";
  exactBoard.disabled = false;
}

async function loadBoardCatalog() {
  try {
    const response = await fetch("/boards.json", { cache: "no-store" });
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}`);
    }
    boardCatalog = await response.json();
    if (currentResult) {
      populateBoardSelect(currentResult);
    } else {
      setBoardSelectWaiting("Run a benchmark first");
    }
  } catch (error) {
    boardCatalog = undefined;
    setBoardSelectWaiting("Board catalog unavailable");
    if (submitStatus) {
      submitStatus.textContent = `Board catalog could not load: ${error.message}`;
    }
  }
}

function scheduleAutoJsonRequest() {
  if (autoRequestingJson || currentResult) {
    return;
  }

  clearTimeout(autoJsonTimer);
  autoJsonTimer = window.setTimeout(() => {
    if (autoRequestingJson || currentResult) {
      return;
    }
    autoRequestingJson = true;
    resultState.textContent = "Reading result";
    jsonButton.disabled = true;
    sendSerial("j\n");
  }, 800);
}

function resultWithExactBoard() {
  const board = selectedBoard();
  if (!currentResult || !board) {
    return undefined;
  }

  return {
    ...currentResult,
    board: {
      ...currentResult.board,
      generic_name: currentResult.board.name,
      id: board.id,
      name: board.name,
      catalog_source: board.source,
      catalog_source_id: board.source_id,
      vendor: board.vendor,
      board_label_trusted: !board.custom,
      board_custom_label: board.custom ? board.name : "",
    },
  };
}

function updateCustomBoardMode() {
  if (!currentResult) {
    return;
  }
  if (customBoardToggle.checked) {
    exactBoard.removeAttribute("list");
    exactBoard.value = "";
    exactBoard.placeholder = "Enter board name";
    exactBoard.disabled = false;
  } else {
    exactBoard.setAttribute("list", "boardSuggestions");
    populateBoardSelect(currentResult);
  }
}

function setResultWaiting(message) {
  if (!resultSummary || !submitButton || !resultState) {
    return;
  }
  currentResult = undefined;
  currentSubmission = undefined;
  clearTimeout(autoJsonTimer);
  autoRequestingJson = false;
  submitButton.disabled = true;
  resultState.textContent = "Waiting";
  submitStatus.textContent = "A completed benchmark result is required.";
  resultSummary.classList.add("empty");
  resultSummary.textContent = message;
  setBoardSelectWaiting("Run a benchmark first");
}

async function scorePreview(result) {
  const response = await fetch("/api/score-preview", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ result }),
  });
  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: `HTTP ${response.status}` }));
    throw new Error(error.error);
  }
  return response.json();
}

function renderCurrentResult(result, submission) {
  currentResult = result;
  currentSubmission = submission;
  clearTimeout(autoJsonTimer);
  resultState.textContent = "Ready";
  submitStatus.textContent = "Enter your name and answer the server check to publish.";
  submitButton.disabled = false;
  jsonButton.disabled = false;
  autoRequestingJson = false;
  populateBoardSelect(result);

  renderSubmissionView(submission, { published: false });
}

function renderPublishedSubmission(submission) {
  currentSubmission = submission;
  resultState.textContent = "Published";
  renderSubmissionView(submission, { published: true });
}

function renderSubmissionView(submission, { published }) {
  resultSummary.innerHTML = "";
  resultSummary.classList.remove("empty");

  const result = rawResultFromRecord(submission);
  const header = document.createElement("div");
  header.className = "result-hero-card";
  const text = document.createElement("div");
  const board = document.createElement("strong");
  board.textContent = result?.board?.name || submission.board_selected_by_user || "Unknown board";
  const meta = document.createElement("span");
  meta.textContent = `${submission.scoring_version} · ${submission.mode} · firmware ${result?.firmware_version || "-"}`;
  text.append(board, meta);

  const scoreBox = document.createElement("div");
  scoreBox.className = "score-box";
  const label = document.createElement("span");
  label.textContent = "Espmark Score";
  const score = document.createElement("strong");
  score.textContent = formatScoreValue(submission, "espmark_core_score");
  scoreBox.append(label, score);
  header.append(text, scoreBox);
  resultSummary.appendChild(header);

  const sections = [
    ["CPU", "cpu_score"],
    ["Memory", "memory_score"],
    ["Flash", "flash_score"],
    ["Practical IoT", "practical_iot_score"],
    ["Stability factor", "stability_factor"],
  ];
  const scoreGrid = document.createElement("div");
  scoreGrid.className = "score-grid";
  for (const [labelText, key] of sections) {
    const card = document.createElement("div");
    card.className = "score-mini";
    const labelNode = document.createElement("span");
    labelNode.textContent = labelText;
    const valueNode = document.createElement("strong");
    valueNode.textContent = formatScoreValue(submission, key);
    card.append(labelNode, valueNode);
    scoreGrid.appendChild(card);
  }
  resultSummary.appendChild(scoreGrid);

  if (!submission.validation?.valid_for_leaderboard) {
    const missing = submission.validation?.missing_metrics || [];
    const warning = document.createElement("div");
    warning.className = "result-warning";
    const title = document.createElement("strong");
    title.textContent = "Incomplete result";
    const text = document.createElement("span");
    text.textContent = `This firmware output is missing ${missing.length} required metrics. Flash Espmark 0.2.1 or newer before publishing to the main leaderboard.`;
    warning.append(title, text);
    resultSummary.appendChild(warning);
  }

  const rawResult = rawResultFromRecord(submission);
  for (const category of ["cpu", "memory", "flash", "practical_iot"]) {
    const metrics = metricsByCategory(rawResult, category);
    if (metrics.length === 0) {
      continue;
    }
    resultSummary.appendChild(renderMetricDetails(category, metrics, false, submission.metric_scores));
  }
}

function renderRecordDetail(record) {
  const rawResult = rawResultFromRecord(record);
  const wrapper = document.createElement("div");
  wrapper.className = "result-detail";

  const scores = document.createElement("div");
  scores.className = "score-grid";
  const scoreItems = [
    ["Espmark", "espmark_core_score"],
    ["CPU", "cpu_score"],
    ["Memory", "memory_score"],
    ["Flash", "flash_score"],
    ["Practical IoT", "practical_iot_score"],
    ["Stability", "stability_factor"],
  ];
  for (const [labelText, key] of scoreItems) {
    const card = document.createElement("div");
    card.className = "score-mini";
    const labelNode = document.createElement("span");
    labelNode.textContent = labelText;
    const valueNode = document.createElement("strong");
    valueNode.textContent = formatScore1(scoreFromRecord(record, key));
    card.append(labelNode, valueNode);
    scores.appendChild(card);
  }
  wrapper.appendChild(scores);

  if (rawResult) {
    const meta = document.createElement("div");
    meta.className = "result-detail-meta";
    meta.textContent = [
      `Firmware ${rawResult.firmware_version || "-"}`,
      `Scoring ${record.scoring_version || "-"}`,
      `Mode ${record.mode || "-"}`,
      `CPU ${rawResult.config?.cpu_freq_mhz || "-"} MHz`,
      `Flash ${rawResult.config?.flash_size_bytes ? Math.round(rawResult.config.flash_size_bytes / 1024 / 1024) + " MB" : "-"}`,
    ].join(" · ");
    wrapper.appendChild(meta);

    for (const category of ["cpu", "memory", "flash", "practical_iot"]) {
      const metrics = metricsByCategory(rawResult, category);
      if (metrics.length === 0) {
        continue;
      }
      wrapper.appendChild(renderMetricDetails(category, metrics, false, record.metric_scores));
    }
  }

  return wrapper;
}

function renderMetricDetails(category, metrics, openByDefault, metricScores = {}) {
  const section = document.createElement("details");
  section.className = "metric-section";
  section.open = openByDefault;
  const sectionHeader = document.createElement("summary");
  sectionHeader.className = "metric-section-header";
  const text = document.createElement("div");
  const title = document.createElement("strong");
  title.textContent = categoryCopy[category].title;
  const subtitle = document.createElement("span");
  subtitle.textContent = categoryCopy[category].subtitle;
  text.append(title, document.createElement("br"), subtitle);
  const score = document.createElement("strong");
  score.className = "score-value";
  const syntheticResult = { results: metrics };
  score.textContent = formatScore(categoryScore(syntheticResult, category));
  sectionHeader.append(text, score);
  section.appendChild(sectionHeader);

  for (const metric of metrics) {
    const card = document.createElement("div");
    card.className = "metric-card";
    const cardText = document.createElement("div");
    const label = document.createElement("strong");
    label.textContent = metricLabel(metric.test_id);
    const meta = document.createElement("span");
    meta.textContent = `${metricDescription(metric)} ${metricStatsLine(metric)}`;
    cardText.append(label, document.createElement("br"), meta);
    const value = document.createElement("strong");
    value.className = "metric-value";
    value.textContent = formatScoreNumber(metricScores?.[metric.test_id]?.score);
    card.append(cardText, value);
    section.appendChild(card);
  }

  return section;
}

async function renderCapturedJson(jsonText) {
  const requestId = ++currentPreviewRequest;
  const result = JSON.parse(jsonText);
  resultState.textContent = "Scoring";
  const preview = await scorePreview(result);
  if (requestId !== currentPreviewRequest) {
    return;
  }
  renderCurrentResult(result, preview.submission);
}

function parseSerialLine(line) {
  const trimmed = line.trim();

  if (trimmed === "ESPMARK_RESULT_BEGIN") {
    capturingJson = true;
    jsonCapture = "";
    resultState.textContent = "Reading JSON";
    clearTimeout(autoJsonTimer);
    return;
  }

  if (trimmed === "ESPMARK_RESULT_END" && capturingJson) {
    capturingJson = false;
    try {
      renderCapturedJson(jsonCapture).catch((error) => {
        resultState.textContent = "Scoring failed";
        appendSystemLog(`Score preview failed: ${error.message}`);
      });
    } catch (error) {
      resultState.textContent = "JSON failed";
      appendSystemLog(`JSON parse failed: ${error.message}`);
    }
    return;
  }

  if (trimmed === "Press 'j' then Enter to print JSON for sharing.") {
    scheduleAutoJsonRequest();
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
    connectionState.textContent = "Disconnected";
    connectButton.disabled = false;
    runButton.disabled = true;
    jsonButton.disabled = true;
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
  if (port.setSignals) {
    await port.setSignals({ dataTerminalReady: false, requestToSend: false }).catch(() => {});
  }
  writer = port.writable.getWriter();
  connectButton.disabled = true;
  runButton.disabled = false;
  jsonButton.disabled = false;
  connectionState.textContent = "Connected";
  resultState.textContent = "Idle";
  appendSystemLog("Connected. Run the benchmark when the board is ready.");
  readLoop();
}

async function fetchResults() {
  const response = await fetch("/api/results", { cache: "no-store" });
  if (!response.ok) {
    throw new Error(`HTTP ${response.status}`);
  }
  return response.json();
}

async function submitBugReport() {
  if (!reportMessage || !reportButton || !reportStatus) {
    return;
  }

  const message = reportMessage.value.trim();
  const answer = reportBotCheck?.value.trim() || "";
  if (message.length < 10) {
    reportStatus.textContent = "Write a little more detail before sending.";
    return;
  }
  if (!reportChallengeToken || !answer) {
    reportStatus.textContent = "Complete the bot check.";
    return;
  }

  reportButton.disabled = true;
  reportStatus.textContent = "Sending report...";

  const response = await fetch("/api/bug-reports", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      contact: reportContact?.value.trim() || "",
      message,
      bot_field: reportCompany?.value || "",
      challenge_token: reportChallengeToken,
      challenge_answer: answer,
      form_elapsed_ms: Date.now() - reportFormRenderedAt,
      page: window.location.pathname,
      user_agent: navigator.userAgent || "",
    }),
  });

  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: `HTTP ${response.status}` }));
    reportStatus.textContent = `Report failed: ${error.error}`;
    reportButton.disabled = false;
    await setupReportBotCheck();
    if (reportBotCheck) {
      reportBotCheck.value = "";
    }
    return;
  }

  const saved = await response.json();
  reportMessage.value = "";
  if (reportBotCheck) {
    reportBotCheck.value = "";
  }
  reportStatus.textContent = `Report saved. Reference: ${saved.id}`;
  reportButton.disabled = false;
  await setupReportBotCheck();
}

async function submitResult() {
  if (!currentResult) {
    return;
  }

  const name = contributorName.value.trim();
  const answer = botCheck.value.trim();

  if (name.length < 2) {
    submitStatus.textContent = "Enter a name or nickname.";
    return;
  }
  if (!challengeToken || !answer) {
    submitStatus.textContent = "Complete the bot check.";
    return;
  }
  if (!selectedBoard()) {
    submitStatus.textContent = customBoardToggle.checked
      ? "Enter a board name before publishing."
      : "Choose an exact board from the suggestions before publishing, or use Not in list.";
    return;
  }

  submitButton.disabled = true;
  submitStatus.textContent = "Publishing result...";

  const response = await fetch("/api/results", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      contributor: name,
      bot_field: companyName.value,
      challenge_token: challengeToken,
      challenge_answer: answer,
      form_elapsed_ms: Date.now() - formRenderedAt,
      result: resultWithExactBoard(),
      browser_name: navigator.userAgentData?.brands?.[0]?.brand || navigator.appName || "",
      browser_version: navigator.userAgentData?.brands?.[0]?.version || "",
      os: navigator.platform || "",
    }),
  });

  if (!response.ok) {
    const error = await response.json().catch(() => ({ error: `HTTP ${response.status}` }));
    submitStatus.textContent = `Publish failed: ${error.error}`;
    submitButton.disabled = false;
    await setupBotCheck();
    botCheck.value = "";
    return;
  }

  const saved = await response.json();
  submitStatus.textContent = `Result published: ${saved.id}`;
  if (saved.submission) {
    renderPublishedSubmission(saved.submission);
  }
  await setupBotCheck();
  botCheck.value = "";
  await renderResultsTable();
}

function resultSortValue(record, key) {
  if (key === "submitted_at") {
    return Date.parse(record.submitted_at || "") || 0;
  }
  const score = scoreFromRecord(record, key);
  return Number.isFinite(score) ? score : -Infinity;
}

function sortedResults(rows) {
  const direction = resultsSortDirection === "asc" ? 1 : -1;
  return [...rows].sort((left, right) => {
    return (resultSortValue(left, resultsSortKey) - resultSortValue(right, resultsSortKey)) * direction;
  });
}

function updateSortButtons() {
  for (const button of sortButtons) {
    if (button.dataset.sortKey !== resultsSortKey) {
      button.removeAttribute("aria-sort");
      continue;
    }
    button.setAttribute("aria-sort", resultsSortDirection === "asc" ? "ascending" : "descending");
  }
}

function drawResultsTable(rows) {
  if (!resultsTable) {
    return;
  }

  const limit = resultsTable.dataset.resultsLimit === "all"
    ? rows.length
    : Number(resultsTable.dataset.resultsLimit || 10);
  const visibleRows = sortedResults(rows).slice(0, limit);

  resultsTable.innerHTML = "";
  if (visibleRows.length === 0) {
    resultsTable.innerHTML = `<tr class="empty-row"><td colspan="11">No submitted results yet.</td></tr>`;
    return;
  }

  for (const rowResult of visibleRows) {
    const rawResult = rawResultFromRecord(rowResult);
    const row = document.createElement("tr");
    row.className = "result-row";
    row.tabIndex = 0;
    row.setAttribute("aria-expanded", "false");
    const cells = [
      ["Name", rowResult.contributor],
      ["Board", rawResult?.board?.name || rowResult.board_selected_by_user || "-"],
      ["Chip", `${rawResult?.board?.soc || "-"} r${rawResult?.board?.revision ?? "-"}`],
      ["Espmark score", formatScore1(scoreFromRecord(rowResult, "espmark_core_score"))],
      ["CPU score", formatScore1(scoreFromRecord(rowResult, "cpu_score"))],
      ["Memory score", formatScore1(scoreFromRecord(rowResult, "memory_score"))],
      ["Flash score", formatScore1(scoreFromRecord(rowResult, "flash_score"))],
      ["Practical IoT", formatScore1(scoreFromRecord(rowResult, "practical_iot_score"))],
      ["CPU clock", `${rawResult?.config?.cpu_freq_mhz || "-"} MHz`],
      ["Firmware", rawResult?.firmware_version || "-"],
      ["Submitted", new Date(rowResult.submitted_at).toLocaleString()],
    ];
    for (const [label, cellText] of cells) {
      const cell = document.createElement("td");
      cell.dataset.label = label;
      cell.textContent = cellText;
      row.appendChild(cell);
    }
    resultsTable.appendChild(row);

    const detailRow = document.createElement("tr");
    detailRow.className = "result-detail-row";
    detailRow.hidden = true;
    const detailCell = document.createElement("td");
    detailCell.colSpan = 11;
    detailCell.appendChild(renderRecordDetail(rowResult));
    detailRow.appendChild(detailCell);
    resultsTable.appendChild(detailRow);

    const toggle = () => {
      detailRow.hidden = !detailRow.hidden;
      row.setAttribute("aria-expanded", String(!detailRow.hidden));
    };
    row.addEventListener("click", toggle);
    row.addEventListener("keydown", (event) => {
      if (event.key === "Enter" || event.key === " ") {
        event.preventDefault();
        toggle();
      }
    });
  }
}

async function renderResultsTable() {
  if (!resultsTable) {
    return;
  }
  resultsTable.innerHTML = `<tr class="empty-row"><td colspan="11">Loading results...</td></tr>`;

  try {
    loadedResults = await fetchResults();
    updateSortButtons();
    drawResultsTable(loadedResults);
  } catch (error) {
    resultsTable.innerHTML = `<tr class="empty-row"><td colspan="11">Could not load results: ${error.message}</td></tr>`;
  }
}

for (const button of themeButtons) {
  button.addEventListener("click", () => setTheme(button.dataset.themeChoice));
}

connectButton?.addEventListener("click", () => {
  connectSerial().catch((error) => {
    connectionState.textContent = "Failed";
    resultState.textContent = "Connection failed";
    appendSystemLog(`Connection failed: ${error.message}`);
  });
});

runButton?.addEventListener("click", () => {
  setResultWaiting("Waiting for benchmark output from the board.");
  resultState.textContent = "Running";
  sendSerial("\n");
});

jsonButton?.addEventListener("click", () => {
  sendSerial("j\n");
});

submitButton?.addEventListener("click", () => {
  submitResult().catch((error) => {
    submitStatus.textContent = `Publish failed: ${error.message}`;
    submitButton.disabled = false;
  });
});

clearLogButton?.addEventListener("click", () => {
  serialLog.textContent = "";
});

refreshResultsButton?.addEventListener("click", renderResultsTable);
customBoardToggle?.addEventListener("change", updateCustomBoardMode);
openReportDialogButton?.addEventListener("click", async () => {
  if (!reportDialog) {
    return;
  }
  reportDialog.showModal();
  await setupReportBotCheck();
});
closeReportDialogButton?.addEventListener("click", () => {
  reportDialog?.close();
});
reportButton?.addEventListener("click", () => {
  submitBugReport().catch((error) => {
    reportStatus.textContent = `Report failed: ${error.message}`;
    reportButton.disabled = false;
  });
});

for (const button of sortButtons) {
  button.addEventListener("click", () => {
    const key = button.dataset.sortKey;
    if (resultsSortKey === key) {
      resultsSortDirection = resultsSortDirection === "desc" ? "asc" : "desc";
    } else {
      resultsSortKey = key;
      resultsSortDirection = "desc";
    }
    updateSortButtons();
    drawResultsTable(loadedResults);
  });
}

initTheme();
setupBotCheck();
setSerialSupport();
setResultWaiting("Run the benchmark to capture board metadata, firmware details and the full Espmark metric set.");
loadBoardCatalog();
renderResultsTable();
