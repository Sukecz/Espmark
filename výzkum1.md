# ESPMark pro komunitní benchmark ESP8266 a ESP32

## Krátké shrnutí nejlepší strategie

Nejlepší strategie pro ESPMark je držet **oficiální hlavní skóre čistě v rovině generického firmwaru a čistě softwarových, deterministických workloadů**, které nevyžadují žádný konkrétní GPIO, LED, tlačítko, senzor, displej, kameru ani rádiové prostředí. To přesně odpovídá tomu, jak dnes funguje webové nasazení přes Web Serial a ESP Web Tools: Web Serial je dostupné jen v části prohlížečů a jen v secure contextu, zatímco ESP Web Tools pracuje s manifestem, umí rozlišit `chipFamily` a automaticky vybrat správný build pro připojený čip. Arduino-ESP32 zároveň oficiálně podporuje generické dev moduly po targetu a Arduino-ESP8266 dokumentace výslovně říká, že když konkrétní board v seznamu není, `Generic ESP82xx` „always work“. To je velmi dobrý základ přesně pro model „jedna generic binárka na target“ místo binárky pro každou brandovanou desku. citeturn27view0turn27view1turn26view0turn21search1turn18search8turn17search8

Důvod, proč musí být ESPMark opatrný, je obrovská heterogenita rodin: ESP8266EX je starší Tensilica L106 SoC do 160 MHz; původní ESP32 je Xtensa LX6 s až dvěma jádry a single-precision FPU; ESP32-S2 je single-core LX7, ale Espressif u něj explicitně uvádí, že `float` je emulovaný v softwaru; ESP32-S3 má naopak single-precision FPU; C3 je RV32IMC, C5 a C6 jsou RV32IMAC a Espressif u C3/C5/C6 výslovně píše, že `float` je emulovaný v softwaru a pomalý; P4 je úplně jiná liga s dual-core RISC-V až 400 MHz, integrovanou PSRAM a single-precision FPU. Mnoho ESP32 rodin má navíc SHA/AES akcelerátory, zatímco ESP8266EX je ve feature listu nemá. Z toho plyne jednoduché pravidlo: **v hlavním skóre používat jen workloady, které jdou provozovat identicky v software na všech targetech**, a hardware-specifické akcelerace držet mimo headline score. citeturn3search1turn1search2turn12search13turn2search43turn12search5turn10search1turn22search5turn22search2turn14search2turn14search4turn1search1turn2search0turn3search0turn3search2

Prakticky tedy doporučuji tento model: **Quick mode** jako rychlý preview test pro běžné uživatele, **Full mode** jako jediný oficiální veřejný benchmark pro leaderboard a **Stress mode** jako samostatný režim pro stabilitu, drift a throttling. Hlavní Core Score by mělo používat jen Full mode; Quick je výborný pro onboarding, ale neměl by míchat leaderboard s kratším profilem a jiným počtem opakování. Stress by měl mít vlastní stabilitní sloupec nebo separátní leaderboard. To je z dlouhodobého hlediska nejčistší pro verzování i pro férovost.

## Doporučené testy pro hlavní firmware

Pro doporučené testy jsou klíčové čtyři technické poznámky. Za prvé, u `float` a hash workloadů je mezi rodinami dramatický rozdíl kvůli FPU a SHA akcelerátorům, takže v Core Score dává smysl jen **nízce vážený portable `float32` test** a **software-only SHA-256**, nikoli benchmark hardwarového crypto bloku. Za druhé, flash výkon je reálně ovlivněný flash režimem a frekvencí a Espressif v performance guide opakovaně uvádí, že QIO/QOUT může být skoro dvojnásobně rychlé proti DIO; zároveň malé mikrobenchy z flashi mohou kolísat podle cache miss patternu. Za třetí, ArduinoJson dokumentuje, že čtení a zápis přes `Stream` jde po bajtech a je výrazně pomalejší než práce s RAM bufferem, takže JSON benchmark musí běžet **z RAM do RAM**. A za čtvrté, ESP8266 dokumentace přímo varuje, že opakované `String` concatenation vede k fragmentaci heapu a pádům; pro oficiální string test se proto musí používat **fixní buffery a `snprintf`/ručně formátované integer stringy**, ne `String`. citeturn13search8turn12search13turn12search5turn22search6turn22search5turn22search2turn1search2turn2search43turn14search2turn3search0turn3search2turn16search0turn16search1turn16search8turn9search0turn9search1turn25search4turn20search0turn20search3

| Test | Co přesně měří | Proč je pro ESPMark užitečný | Hlavní skóre nebo doplněk | Stabilita a reprodukovatelnost | ESP8266 | Rizika zkreslení | Implementační náročnost v Arduino firmware | Doporučená délka | Jednotka | JSON id |
|---|---|---|---|---|---|---|---|---|---|---|
| `cpu_stress_mix` | Dlouhodobý integer throughput v mixu add/xor/rotate/mul/branch se sink checksumem | Doplní tvoje krátké mikrotesty o „sustained“ výkon a lépe odhalí drift, PM a scheduling | **Ano, Core** | Vysoká, pokud běží po oknech a s warm-up | Ano | DFS/light sleep, Wi-Fi background, dead-code elimination | Nízká | Q 0,6 s / F 2 s / S 10 s | `ops/s` | `cpu_stress_mix` |
| `float32_mix` | Single-precision add/mul/div nad fixní sadou hodnot, bez `sin/cos/log` | Reálnější než čistý synthetic FLOPS test; pokrývá filtry a výpočty běžné v hobby IoT | **Ano, ale nízká váha** | Vysoká | Ano | Obrovské rozdíly mezi FPU a soft-float; `-ffast-math` by znehodnotil srovnání | Nízká | Q 0,3 s / F 1 s / S 5 s | `ops/s` | `float32_mix` |
| `mandelbrot_fixed` | Fixní Mandelbrot tile v Q16.16 nebo Q20.12 fixed-pointu | Deterministický výpočet, který v jednom testu kombinuje aritmetiku, větvení a locality | **Ano ve Full/Stress**, Quick vynechat | Středně vysoká | Ano | Volba tile size a iterací může některé čipy zvýhodnit | Střední | Q — / F 1,5 s / S 5 s | `ms` na tile | `mandelbrot_fixed` |
| `crc32_sw` | Standardní CRC32 nad fixním bufferem | Je to srozumitelný, běžný checksum workload; lepší než „CRC-like“ syntetika | **Ano, ale nízká váha** | Vysoká | Ano | Nesmí se omylem použít HW/ROM akcelerace nebo jiná implementace na různých targetech | Nízká | Q 0,2 s / F 0,6 s / S 2 s | `MiB/s` | `crc32_sw` |
| `sha256_sw` | Portable software SHA-256 nad fixním bufferem | Velmi užitečný „practical IoT“ workload pro integritu a podpisy | **Ano** | Vysoká | Ano | Pokud by různé targety volaly různé crypto backendy, score by přestalo být férové | Střední | Q 0,3 s / F 0,8 s / S 3 s | `MiB/s` | `sha256_sw` |
| `json_roundtrip` | Parse → mutate → serialize fixního JSONu z RAM bufferu do RAM bufferu | Reprezentuje telemetrii, konfiguraci a web API payloady mnohem lépe než čistá mikroaritmetika | **Ano** | Středně vysoká | Ano | Rozdíly knihovny, velikost `JsonDocument`, stream vs buffer | Střední | Q 0,3 s / F 0,8 s / S 2 s | `roundtrips/s` | `json_roundtrip` |
| `string_format` | Formátování malých záznamů do fixního bufferu pomocí `snprintf`/integer formatterů | Hodí se pro logování, MQTT payloady, diagnostiku a generování textových stavů | **Ano, ale nízká váha** | Středně vysoká | Ano | `String` concat a `%f` by výsledek zbytečně zkreslily | Nízká | Q 0,2 s / F 0,6 s / S 2 s | `records/s` | `string_format` |
| `flash_seq_read` | Sekvenční read z fixního flash blobu většího než cache, plus checksum sink | Zachytí praktickou propustnost čtení z externí flash bez potřeby externího HW | **Ano, ale středně nízká váha** | Střední | Ano | Flash mode/freq, cache, wiring modulu, image layout | Střední | Q 0,2 s / F 0,8 s / S 2 s | `MiB/s` | `flash_seq_read` |
| `heap_frag_pattern` | Checkerboard alloc/free pattern + before/after largest block + throughput | Lepší obrázek o skutečné použitelnosti heapu než čistý churn malých bloků | **Spíš doplňkově nebo s malou vahou** | Střední | Ano | Rozdílné allocátory ESP8266 vs ESP-IDF, pořadí testů, stav heapu | Střední | Q — / F 0,8 s / S 3 s | `ops/s` + `%` | `heap_frag_pattern` |
| `matmul_i16` | Int16×int16→int32 malá matrix multiply tile | Dobrý sekundární deterministický compute test, pokud nechceš Mandelbrot | **Spíše doplněk** nebo náhrada za Mandelbrot slot | Vysoká | Ano | Tile size může příliš odrážet cache locality | Nízká až střední | Q — / F 1 s / S 3 s | `MAC/s` | `matmul_i16` |
| `decompress_fixed` | Decompress fixního LZ4/Heatshrink bloku + verifikace CRC | Praktické pro OTA-like a asset/config workloady, ale méně univerzální | **Jen doplňkově** | Středně vysoká | Ano | Volba algoritmu a knihovny dominuje výsledku | Střední až vyšší | Q — / F 0,6 s / S 2 s | `MiB/s` | `decompress_fixed` |
| `psram_bw` | PSRAM read/write/strided test jen při auto-detekci dostupnosti | U S3/P4 a některých modulů velmi užitečné, ale není to univerzální vlastnost desky | **Jen Optional/Environment** | Střední | Ne | Přítomnost, velikost, clock a workaroundy PSRAM se liší po rodinách | Střední | Q optional / F 0,8 s / S 3 s | `MiB/s` | `psram_bw` |

Doporučení v tabulce záměrně stojí na tom, že **hash workloady mají být software-only**, JSON má běžet **RAM→RAM**, string test má být **bez `String` concatenation** a flash test má být **jen read-only**. To je nejférovější kompromis mezi architekturami s FPU/HW SHA a bez nich, mezi různými flash režimy a mezi odlišným heap chováním napříč ESP8266 a ESP32 rodinou. PSRAM je už z principu volitelná, protože její podpora a kapacity se mezi rodinami výrazně liší. citeturn3search1turn1search2turn1search1turn2search43turn10search1turn3search0turn3search2turn14search4turn16search0turn16search1turn16search8turn28search4turn28search5turn9search0turn9search1turn25search4turn7search0turn7search6turn7search9turn7search5

## Testy, které nezařazovat do hlavního skóre

Některé workloady vypadají lákavě, ale pro komunitní benchmark přes generic firmware jsou **příliš závislé na prostředí, konkrétním boardu nebo konfiguraci**. Platí to hlavně pro write testy do flash/NVS, pro Wi-Fi a pro browser/USB cestu. Na ESP32 je `Preferences` jen vrstva nad NVS a Espressif výslovně říká, že NVS je key-value storage ve flashi a je nejlepší pro malé hodnoty; na ESP8266 je `EEPROM.commit()` emulováno přepisem flash sektoru a dokumentace přímo varuje před rychlým opotřebením. U Wi-Fi dokumentace ukazuje, že scan dwell time závisí na kanálech a nastavení skenu, a ESP8266 docs píší, že scan trvá stovky milisekund. Boot time je navíc citlivý na bootloader log level a startup logy. A Web Serial je sám o sobě browser/OS/permission feature, nikoli vlastnost MCU. citeturn6search0turn6search1turn6search5turn6search8turn6search3turn15search2turn15search0turn15search3turn23search0turn23search2turn27view0turn27view1

| Test nebo oblast | Proč ne do hlavního skóre | Kam maximálně patří |
|---|---|---|
| `double64_math` | `double` je na části rodin emulovaný v softwaru a výsledek více měří toolchain/libm/ABI než běžné hobby workloady | Jen doplňková orientační metrika |
| Plná komprese | Výsledek je extrémně závislý na zvoleném algoritmu, datech a implementaci; komprese je navíc těžší na footprint | Doplňkový practical test, ne Core |
| `Preferences` / NVS / EEPROM write benchmark | Měří backend flash storage, wear-leveling a partition layout; na ESP8266 navíc zapisuje emulovaný sektor | Jen Optional/Environment |
| `webserial_throughput` | Měří browser, OS driver, USB bridge, kabel a host PC stejně jako MCU | Jen Optional/Environment |
| `boot_ready_time` | Ovlivňuje ho reset obvod, USB cesta, bootloader logy, validace image a host timing | Jen Optional/Environment |
| `wifi_scan` | Zásadně závisí na RF prostředí, počtu AP, kanálech a regulaci | Jen Optional/Environment |
| `wifi_connect` | Ještě víc závislé na AP, RSSI, zabezpečení, DHCP a okolním provozu | Jen Optional/Environment |
| Benchmark přes `Arduino String` concat | Není férový mezi cores a může sám vyrábět fragmentaci, hlavně na ESP8266 | Nepoužívat v oficiálním benchmarku vůbec |
| HW SHA/AES/ROM crypto v hlavním skóre | Není univerzální napříč ESP8266 a ESP32 rodinami a může z hlavního score udělat spíš benchmark periferních akcelerátorů | Samostatná „accelerated“ sekce mimo Core |
| Board-specific IO, LED, tlačítko, baterie, senzory, displeje, kamera, LoRa | Nejsou generické a odporují cíli firmware „per target, not per board“ | Nikam do oficiálního ESPMark |

Moje doporučení je tedy ostré: **hlavní veřejný leaderboard držet jen pro Full mode generického firmwaru a jen pro Core workloady**. Optional/Environment metriky jsou užitečné a zábavné, ale jen jako samostatné sloupce, filtry nebo samostatný detail výsledku.

## Doporučená struktura skóre a přesný návrh výpočtu

Navrhuji tuto strukturu:

| Skóre | Co obsahuje | Váha do `ESPMark Core Score` |
|---|---|---:|
| `CPU Score` | integer mikrotesty, delší CPU stress, float32, deterministický compute test | 40 % |
| `Memory Score` | RAM bandwidth, malloc/free churn, fragmentační pattern | 30 % |
| `Flash Score` | read-only flash sequential read | 10 % |
| `Practical IoT Score` | SHA-256, CRC32, JSON roundtrip, string formatting | 20 % |
| `Optional/Environment Score` | PSRAM, Web Serial throughput, boot time, Wi-Fi scan/connect | 0 % do headline score |

Uvnitř hlavního Core Score doporučuji tyto **finální subváhy**:

| Subkategorie | Váha do Core |
|---|---:|
| CPU integer micro-suite | 18 % |
| CPU sustained stress | 8 % |
| CPU float/math | 6 % |
| Deterministický compute test | 8 % |
| Memory/RAM bandwidth | 20 % |
| Heap alloc/free churn | 6 % |
| Heap fragmentation | 4 % |
| Flash read | 10 % |
| SHA-256 software | 7 % |
| CRC32 software | 3 % |
| JSON parse/generate | 7 % |
| String formatting | 3 % |

Tyto váhy dávají smysl pro hobby benchmark z několika důvodů. CPU a RAM stále dominují typické práci na ESP — od loop logiky přes parsing až po buffer management. Flash má být vidět, ale nesmí být převažující, protože flash mode/frequency a cache umí změnit výkon velmi výrazně. Practical IoT workloady mají dostat reálnou váhu, protože právě JSON, checksumy a hashování odlišují „zábavný syntetický benchmark“ od benchmarku, který je blízko tomu, co na deskách lidi skutečně dělají. A stabilitu nedoporučuji zabalit do běžné aditivní váhy; lepší je mít ji jako **oddělenou kvalitu výsledku** a jen mírně s ní penalizovat Full/Stress score. Rozdíly mezi FPU, SHA akcelerátory, flash režimy a power managementem jsou totiž mezi rodinami objektivně velké. citeturn13search8turn12search13turn12search5turn22search6turn22search5turn22search2turn16search0turn16search1turn24search0turn24search2

Pro výpočet doporučuji tuto matematiku:

Reprezentativní hodnota jednotlivé metriky má být **medián** z měřených oken po jednom warm-up běhu. To je v souladu s tím, že Espressif u mikrobenchmarkingu upozorňuje na cache miss variace a scheduling overhead a doporučuje opakovaná měření; `esp_timer_get_time()` má mikrosekundovou přesnost, ale nenulový overhead, zatímco cycle counter je vhodnější pro velmi krátké úseky. citeturn16search4turn17search2turn19search2turn17search0

Převod raw metriky na skóre, kde vyšší je lepší, dělej takto:

- Pro **časové metriky**: `base_score = 1000 * reference_time / measured_time`
- Pro **propustnostní metriky**: `base_score = 1000 * measured_rate / reference_rate`

Tím dostane referenční zařízení score 1000 v každé metrice. Reference musí být **zmražená** v `scoring_version`; nesmí se počítat z live leaderboardu.

Kategorie i celkový Core Score doporučuji počítat přes **vážený geometrický průměr**, nikoli aritmetický. Důvod je jednoduchý: geometrický průměr je správnější pro kombinaci heterogenních workloadů a neumožní jedné extrémně silné metrice „překřičet“ několik slabších. Aritmetický průměr dávej jen tam, kde má být výsledkem prostý souhrn podobných věcí — typicky u vedlejšího `Optional/Environment Score`, pokud trváš na jednom společném čísle pro environment metriky.

Konkrétně:

- `metric_score = base_score * stability_factor`
- `category_score = 1000 * exp(sum(w_i * ln(metric_score_i / 1000)))`
- `core_score = 1000 * exp(sum(W_cat * ln(category_score_cat / 1000)))`

Doporučený `stability_factor`:

- v **Quick mode**: vždy `1.0`, protože vzorků je málo a quick nemá být trestán šumem,
- ve **Full mode**: použít mírnou penalizaci z `p95 / median`, například  
  `stability_factor = clamp(0.95, 1.00, 1.00 - 0.25 * max(0, p95/median - 1.05))`
- ve **Stress mode**: headline Core Score nech stejné jako ve Full, ale navíc dopočítej samostatný `Stress Stability Score` z driftu výkonu mezi první a poslední třetinou běhu.

Extrémní hodnoty ošetři takto:

- první běh každého testu = warm-up, do score se nezapočítává,
- pokud je `sample_count >= 9`, winsorizuj jen **mean/stdev** o jeden nejvyšší a jeden nejnižší vzorek; **medián nech beze změny**,
- pokud checksum/known-answer test selže, metrika je invalidní a celý Core výsledek má být neplatný.

Pro long-term kompatibilitu doporučuji **jednu oficiální referenci** pro globální srovnání, například generic `ESP32 Dev Module` bez PSRAM s oficiálním generic firmwarem, a vedle toho ještě **family leaderboardy** podle `target_family`. Tím dostaneš stabilní absolutní osu v čase a zároveň férovější pohled „uvnitř rodiny“. Generický `ESP32-XX Dev Module` per target i generic ESP8266 fallback jsou ostatně přesně to, co oficiální Arduino dokumentace podporuje. citeturn18search8turn17search8turn21search1

Režimy doporučuji přesně takto:

| Režim | Doporučená délka | Účel | Leaderboard |
|---|---:|---|---|
| `quick` | 6 s | onboarding, rychlé sdílení, ověření že deska běží | samostatně nebo vůbec neřadit |
| `full` | 24 s | oficiální veřejný benchmark | **ano, hlavní leaderboard** |
| `stress` | 75 s | stabilita, drift, throttling, napájení | separátní stability leaderboard |

Tento poměr je podle mě ideální: Quick je dost krátký na masové použití přes web, Full už má dost času na opakování a mediány a Stress je dost dlouhý, aby začal ukazovat reálný drift, ale ještě není otravný pro běžného hobby uživatele. U ESP8266 navíc delší kontinuální smyčky vyžadují rozumné chunkování a `yield`, pokud nechceš narážet na watchdog a background servis. citeturn25search0turn25search4

## Návrh JSON schématu výsledku pro web a systém verzování

Pro dlouhodobou životnost ESPMark je lepší **oddělit firmware raw output od oficiálního leaderboard submission objektu**. Firmware má přes serial vypsat jen raw metriky a metadata o zařízení. Web nebo backend má z raw metrik spočítat oficiální score. To výrazně zjednoduší budoucí rescore starých výsledků, zavádění nových `scoring_version` a validaci.

Doporučená pravidla verzování jsou tato:

| Pole | Co znamená | Kdy bumpovat |
|---|---|---|
| `schema_version` | tvar JSON a povinná pole | když měníš strukturu JSON |
| `firmware_version` | konkrétní binárka a implementace | při bugfixu, optimalizaci, novém buildu |
| `scoring_version` | reference, váhy, penalizace, matematika | při jakékoli změně score math |
| `benchmark_profile` | které testy a runtime patří do sady | při změně test setu nebo režimu |
| `mode` | `quick`, `full`, `stress` | samostatná osa kompatibility |
| `official_generic_firmware` | zda běžela oficiální generic binárka | boolean, musí být `true` pro official board |
| `target_family` | `ESP8266`, `ESP32`, `ESP32-C3`… | metadata a family leaderboard |
| `board_selected_by_user` | jen štítek od uživatele, ne vstup do score | nebumpuje nic |

Pravidlo pro srovnatelnost má být tvrdé: **výsledky jsou přímo porovnatelné jen tehdy, když mají stejné `scoring_version`, `benchmark_profile` a `mode`**. `schema_version` se může lišit, pokud backend umí migrovat parser. `firmware_version` se lišit může, ale pouze pokud se nezměnila definice raw metrik a `scoring_version` zůstala stejná.

Takto může vypadat **device JSON** emitovaný firmwarem mezi `ESPMARK_RESULT_BEGIN` a `ESPMARK_RESULT_END`:

```json
{
  "schema_version": "1.0.0",
  "firmware_version": "0.3.0",
  "benchmark_profile": "espmark-core-v1",
  "mode": "full",
  "official_generic_firmware": true,
  "target": {
    "chip_family": "ESP32-C3",
    "chip_model": "ESP32-C3",
    "arch": "riscv32",
    "cores": 1,
    "cpu_freq_mhz": 160,
    "flash_size_bytes": 4194304,
    "flash_speed_hz": 40000000,
    "psram_present": false
  },
  "build": {
    "framework": "arduino",
    "core_version": "3.3.8",
    "git_commit": "abc1234",
    "compiler_opt": "-O2"
  },
  "run": {
    "boot_count": 17,
    "warmup_executed": true,
    "sample_policy": {
      "warmup_runs": 1,
      "measured_runs": 9
    }
  },
  "metrics": [
    {
      "id": "cpu_stress_mix",
      "suite": "cpu",
      "core_contributing": true,
      "unit": "ops/s",
      "direction": "higher_is_better",
      "work": {
        "ops_per_iteration": 1024,
        "target_runtime_ms": 2000,
        "actual_runtime_ms": 2017
      },
      "raw": {
        "mean": 18423348.1,
        "median": 18499010.4,
        "min": 18110231.2,
        "max": 18644010.7,
        "stdev": 161224.7,
        "p95": 18610122.8,
        "sample_count": 9
      },
      "validation": {
        "checksum_kind": "u32",
        "checksum_value": "0x6BB1A452",
        "known_answer_ok": true
      }
    },
    {
      "id": "sha256_sw",
      "suite": "practical",
      "core_contributing": true,
      "unit": "MiB/s",
      "direction": "higher_is_better",
      "work": {
        "bytes": 131072,
        "target_runtime_ms": 800,
        "actual_runtime_ms": 817
      },
      "raw": {
        "mean": 1.83,
        "median": 1.84,
        "min": 1.79,
        "max": 1.86,
        "stdev": 0.02,
        "p95": 1.86,
        "sample_count": 9
      },
      "validation": {
        "digest_hex": "7a4d...",
        "known_answer_ok": true
      }
    }
  ]
}
```

A takto může vypadat **submission JSON** pro backend a leaderboard:

```json
{
  "submission_schema_version": "1.0.0",
  "received_at_utc": "2026-06-05T19:32:11Z",
  "scoring_version": "espmark-score-v1",
  "mode": "full",
  "board_selected_by_user": "LOLIN C3 Mini",
  "board_label_trusted": false,
  "transport": {
    "kind": "webserial",
    "browser_name": "Chrome",
    "browser_version": "137",
    "os": "Windows 11"
  },
  "device_result": { "...": "firmware raw object" },
  "scores": {
    "espmark_core_score": 1264.3,
    "cpu_score": 1321.0,
    "memory_score": 1184.2,
    "flash_score": 1097.8,
    "practical_iot_score": 1288.5,
    "optional_environment_score": null,
    "stability_score": 981.2
  },
  "normalization": {
    "reference_profile_id": "espmark-ref-full-v1",
    "family_leaderboard_key": "ESP32-C3|espmark-score-v1|full"
  },
  "validation": {
    "required_fields_ok": true,
    "official_firmware_ok": true,
    "known_answers_ok": true,
    "score_computed_by_backend": true
  }
}
```

U webové vrstvy dává smysl držet se Web Serial / ESP Web Tools reality: HTTPS-only, manifest po `chipFamily`, generické buildy po targetech a host-side metadata odděleně od firmware raw outputu. To přesně sedí na architekturu projektu ESPMark. citeturn26view0turn27view0

## Doporučené změny v aktuálním Arduino firmware a prioritizovaná roadmapa

Na úrovni firmwaru bych měnil co nejméně věcí najednou, ale velmi důsledně bych **zafixoval měřicí prostředí**. Na ESP32 rodině je potřeba během Core benchmarku držet vysokou CPU/APB frekvenci a vypnout automatic light sleep, protože ESP-IDF power management umí měnit CPU/APB frekvenci i přepínat do Light-sleep podle locků a workloadu. Na ESP8266 je nutné delší workloady chunkovat a mezi okny zavolat `yield()` nebo `delay(0)`, protože dokumentace explicitně upozorňuje na problémy s delšími smyčkami bez yieldování a na watchdog. Pro krátké inner loops má smysl na ESP32 použít cycle counter a pro delší bloky `esp_timer_get_time()`; na ESP8266 je k dispozici `ESP.getCycleCount()`. U stringů a JSONu bych všude přešel na prealokované buffery a nikdy nebenchmarkoval `String` concat. A flash benchmark by měl být read-only a kontrolovaně sekvenční, protože přístup na main flash sdílí cestu s instruction/data cache a může ovlivňovat celý systém. citeturn24search0turn24search2turn25search0turn25search4turn19search2turn17search2turn17search0turn25search4turn28search4turn28search5

Dále bych do oficiálních binárek a výsledků zavedl tyto konkrétní zásady. Core testy nesmí inicializovat Wi-Fi ani BT, pokud to není explicitně environment suite. Každý test musí mít warm-up a known-answer/checksum sink proti dead-code elimination. `CRC-like` workload bych nahradil skutečným `crc32_sw`. `SHA-256` bych dodal jako vendored portable referenční implementaci, ne jako volání platform crypto API. JSON benchmark bych sjednotil přes jednu vendored verzi ArduinoJson a měřil čistě buffer→buffer. String benchmark bych držel na integer formattingu a fixačních bufferech. Echo přes Web Serial, boot/ready time a Wi-Fi bych držel mimo Core. A protože chceš webový deploy po targetech, dává smysl už od začátku publikovat oficiální ESP Web Tools manifesty po `chipFamily`; dokumentace ESP Web Tools navíc přímo upozorňuje, že ESP32-family artifacts musejí správně explicitně řešit bootloader/partitions/app a flash mode/freq. citeturn9search0turn9search1turn26view0

Prioritizovaná roadmapa by podle mě měla vypadat takto:

| Milník | Co přesně dodat | Proč právě tehdy |
|---|---|---|
| `ESPMark 0.2` | `scoring_version`, `benchmark_profile`, oddělení raw vs backend score, `cpu_stress_mix`, `float32_mix`, `crc32_sw`, `sha256_sw`, `flash_seq_read`, Quick + Full mode | To je nejmenší změna s největším přínosem; okamžitě vznikne smysluplné Core Score bez rozbití generického konceptu |
| `ESPMark 0.3` | `json_roundtrip`, `string_format`, `mandelbrot_fixed`, `heap_frag_pattern`, Optional/Environment suite, PSRAM optional suite, Stress mode | Rozšíří benchmark z „syntetického“ do „praktického“, ale pořád ještě bez přehnané komplexity |
| `ESPMark 1.0` | zmražené reference pro `scoring_version`, finální leaderboard pravidla, manifesty a binární podpisy pro official generic firmware, family leaderboardy, matrix/decompress jako supplemental suite | Tím se ESPMark stane dlouhodobě udržitelným a verzovatelným benchmarkem, ne jen jednorázovým hobby testerem |

Za mě je velmi důležité, aby **hlavní veřejná tabulka začala až ve chvíli, kdy existuje zamčený `scoring_version` a oficiální generic firmware artifacts**. Do té doby bych leaderboard spíš značil jako preview.

## Rizika a jak se jim vyhnout

Největší riziko ESPMarku není „měříme špatně“, ale spíš „smícháme dohromady věci, které patří do různých vrstev“. Pokud smícháš FPU vs soft-float, HW SHA vs software SHA, flash QIO vs DIO, Wi-Fi scan v různých bytech a Web Serial přes různé USB mosty, vznikne sice zajímavé číslo, ale ne dlouhodobě férový benchmark. Espressif dokumentace sama ukazuje, že výkon silně ovlivňuje power management, flash režim/frekvence, cache misses i startup logy, a browserová vrstva je navíc limitovaná dostupností Web Serial a HTTPS modelem. citeturn24search0turn24search2turn16search0turn16search1turn16search8turn23search0turn23search2turn27view0turn27view1

Proto bych v praxi držel tato obranná pravidla:

| Riziko | Jak mu předejít |
|---|---|
| Architekturní rozdíly mezi rodinami | V Core používat software-only referenční implementace hash/checksum a nízkou váhu `float32`; `double` nechat mimo Core |
| Kolísání kvůli power managementu | Ve Full/Stress držet max CPU/APB, vypnout light sleep, neinicializovat Wi-Fi/BT v Core |
| Šum z flash cache a image layoutu | Malé mikrobenchy opakovat po oknech, warm-up vyřadit, flash test držet jako delší read-only workload |
| Nestabilita ESP8266 při dlouhých smyčkách | Chunkovat workload a mezi okny `yield()` |
| Heap fragmentace vyrobená benchmarkem samotným | Nikdy nepoužívat `String` concat v benchmark implementation; používat fixní buffery |
| Zkreslení přes Wi-Fi, serial a boot | Držet tyto testy jen v `Optional/Environment Score` |
| Flash wear kvůli storage benchmarkům | NVS/EEPROM write testy neřadit do Core a výrazně limitovat počet zápisů |
| Rozbití srovnatelnosti mezi verzemi | Každou změnu test setu, vah nebo reference promítnout do `scoring_version` nebo `benchmark_profile`; nikdy nemíchat rozdílné verze do jednoho rankingu |
| Zneužití leaderboardu upraveným firmwarem | Vyžadovat `official_generic_firmware=true`, ukládat `firmware_version`, `git_commit`, build hash a oficiální manifest/binary fingerprint |
| Ztráta možnosti budoucího rescoringu | Ukládat raw metriky a počítat oficiální score na backendu, ne jen ve firmwaru |

Nejdůležitější závěr je tedy jednoduchý: **ESPMark má být „generic, software-defined, explainable benchmark“**. Jakmile budeš držet headline score jen na Full mode, jen na raw metrikách přepočtených backendem a jen na testech, které neznají konkrétní brandovanou desku ani její okolí, má projekt velmi dobrou šanci být dlouhodobě srozumitelný, férový a komunitně udržitelný.