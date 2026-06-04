Benchmark projekt pro komunitní porovnání ESP32 desek
Výkonné shrnutí
Pro komunitní benchmark ESP32 nedává smysl stavět „jedno univerzální skóre“. Rodiny ESP32 se dnes výrazně liší architekturou CPU, počtem jader, bezdrátovými rádii i periferiemi: původní ESP32 je Xtensa LX6 s Wi‑Fi 4 a Bluetooth Classic + LE, ESP32-S2 je jednojádrový LX7 bez Bluetooth, ESP32-C3 je jednojádrový RISC‑V s Wi‑Fi 4 a BLE 5, ESP32-C6 přidává Wi‑Fi 6 a 802.15.4, ESP32-C5 posouvá rodinu do 2.4/5 GHz dual‑band Wi‑Fi 6 a CAN FD, zatímco ESP32-S3 přidává dvoujádrový LX7, FPU, SIMD/PIE a USB OTG. Z toho plyne, že projekt musí být capability-aware: některé testy jsou povinné pro všechny cíle, jiné se automaticky zapínají jen tam, kde je SoC nebo deska skutečně podporuje. [1]
Architektura projektu by měla být SoC-centrická, ale board-aware. Výsledek musí nést identitu family / soc / module / board / revision / sdk / toolchain / flash / psram / clock / power setup / ambient. To je zásadní zejména u spotřeby a RF. Espressif výslovně upozorňuje, že přímé měření proudu na vývojové desce je často zavádějící kvůli dalším obvodům na desce; pro seriózní power benchmark je potřeba oddělit alespoň „board-level“ a „module-only“ lane. [2]
Doporučený minimální rozsah první veřejné verze je: CPU, RAM/PSRAM, flash/XIP, boot/wake, spotřeba, základní I/O, RTOS latence a kompatibilita SDK/toolchainu. Druhá vlna má přidat Wi‑Fi/BLE throughput a coexistence, třetí OTA, secure boot, flash encryption, CAN/Ethernet/USB a akcelerované AI/NN inference. Toto pořadí maximalizuje hodnotu pro komunitu a současně snižuje riziko, že se projekt zablokuje na drahé RF a power laboratoři. [3]
Repozitář má být postaven na oficiálních nástrojích Espressif: ESP-IDF, idf.py set-target, oficiální Docker image espressif/idf, volitelně CONFIG_APP_REPRODUCIBLE_BUILD, a host-side automatizaci přes pytest-embedded. Build a validace schémat mohou běžet na GitHub-hosted runners, ale hardware-in-the-loop testy mají běžet jen na izolovaných self-hosted runners, ideálně mimo neověřené pull requesty; GitHub na rizika self-hosted runnerů ve veřejných repozitářích výslovně upozorňuje. [4]
Webová část má sloužit jako kanonický registry výsledků, nikoli jen jako výpis CI artefaktů. GitHub Actions artefakty jsou užitečné pro transientní logy, binárky a raw výstupy, ale jejich retence je standardně časově omezená; pro dlouhodobé porovnávání mezi boardy, SDK verzemi a konfiguracemi je lepší vlastní databázová vrstva nad validovaným JSON schématem, s moderací, karanténou a auditní stopou. [5]
Kontext a principy návrhu
Officialita a rozsah podpory se v rodině ESP32 rychle mění, proto je rozumné navrhnout benchmark ve třech vrstvách: společné minimum, rodinné rozšíření, board-specific extension. Jako společné minimum se hodí CPU integer, RAM, flash, GPIO, UART, boot, deep-sleep, RTOS a build compat. Rodinná rozšíření aktivují testy jako dual-core scaling, FPU/SIMD, USB OTG, Ethernet, CAN FD, Wi‑Fi 6, 5 GHz Wi‑Fi, 802.15.4 nebo PSRAM. Board-specific vrstva pak doplní např. integrovaný Ethernet PHY, externí CAN transceiver, USB konektor na nativním PHY, anténní konfiguraci nebo měřicí jumper pro proud. Tento přístup odpovídá tomu, že Espressif a navazující ekosystém dnes pokrývají více cílů i více SDK vrstev, včetně ESP-IDF a Arduino-ESP32. [6]
Rodina	CPU a paměť	Rádia a periférie relevantní pro benchmark	Dopad na testovací plán	Zdroj
ESP32	Xtensa LX6, single/dual-core, až 240 MHz, 520 KB SRAM	Wi‑Fi 4, Bluetooth 4.2 Classic + LE, Ethernet MAC, TWAI, DAC	Povinné multi-core, Classic BT, Ethernet, DAC	[7]
ESP32-S2	Xtensa LX7, single-core, až 240 MHz, 320 KB SRAM	Wi‑Fi 4, USB OTG, bez Bluetooth	Bez BT lane; USB a low-power lane důležité	[8]
ESP32-C3	RISC‑V, single-core, až 160 MHz, 400 KB SRAM, bez externí RAM	Wi‑Fi 4, BLE 5, USB Serial/JTAG, TWAI	Jednojádrové baseline, USB CDC/JTAG lane, bez PSRAM lane	[9]
ESP32-C6	RISC‑V HP + LP core, 2.4 GHz Wi‑Fi 6, 512 KB HP SRAM + 16 KB LP SRAM	BLE 5.x, Thread/Zigbee, 2× TWAI, USB Serial/JTAG	Wi‑Fi 6, 802.15.4, coexistence a LP lane jsou povinné	[10]
ESP32-C5	RISC‑V single-core; oficiální modulová dokumentace dnes ukazuje až 240 MHz, proto má benchmark ukládat skutečnou runtime frekvenci místo hardcoded family default	Dual-band Wi‑Fi 6 2.4/5 GHz, BLE, Zigbee/Thread, CAN FD, USB Serial/JTAG, externí PSRAM	Samostatný 5 GHz lane, CAN FD lane, PSRAM lane; metadata musí explicitně nést f_cpu	[11]
ESP32-S3	Xtensa LX7 dual-core, až 240 MHz, 512 KB SRAM, FPU, SIMD/PIE	Wi‑Fi 4, BLE 5, USB OTG, USB Serial/JTAG, externí flash/RAM	Povinné multi-core, float/FPU, SIMD/DSP, USB OTG a AI-accelerated lane	[12]
Z pohledu benchmarku je zásadní odlišit rodinu SoC od konkrétní desky. Například spotřebu výrazně mění LDO, USB-UART bridge, nabíjecí čip, LED indikace, senzorové příslušenství a způsob napájení desky. Espressif pro current-consumption měření přímo doporučuje neměřit „běžnou dev board“ jako náhradu za modulový profil, pokud nejsou obvody desky odpojené nebo pokud deska nemá měřicí infrastrukturu navrženou pro current profiling. Z toho plyne, že výsledky je vhodné zveřejňovat alespoň ve dvou lanech: integrated board profile a isolated module profile. [2]
Kompletní taxonomie testů a metrik
Testovací sada má mapovat to, co Espressif oficiálně vystavuje přes ESP-IDF a datasheety: CPU a performance tuning, ADC oneshot/continuous/calibration, LEDC PWM, SPI flash read/write/mmap, startup flow, deep-sleep wake stubs, OTA, secure boot, flash encryption, RF coexistence, external RAM, FreeRTOS scheduler a high-priority interrupts. Tím se benchmark neopírá o „syntetické triky“, ale o subsystémy, které uživatelé skutečně nasazují a které jsou zároveň dobře automatizovatelné z firmware i host-side harnessu. [13]
Přehled testů pro CPU, paměť a flash
Oblast	Povinné subtesty	Primární metriky	Doporučené provedení
CPU core	CoreMark, Embench-IoT, memcpy/memset, branch-heavy kernels	score, score/MHz, ns/op, cycles/op	Ukládat raw cycles i wall time; běh při více frekvencích
CPU multi-core	parallel task scaling, dual-core contention, pinned vs unpinned tasks	speedup, efficiency, scheduler overhead	Jen pro dual-core cíle
Integer performance	8/16/32/64-bit add/mul/div, CRC, bit ops, table lookups	ops/s, cycles/op	Volit data sizes L1-hot i cache-cold
Float/FPU	scalar float add/mul/div, DSP-like accumulate, transcendentals	MFLOPS, cycles/op	Automaticky skip, pokud target nemá HW FPU
DSP / ISA extensions	FIR, IIR, FFT, vector add/mul, quantized GEMM	samples/s, cycles/sample	Samostatný lane pro Xtensa SIMD/PIE a další ISA-specific fast paths
Kryptografie	AES, SHA, HMAC, RSA/ECC sign/verify, RNG throughput	MB/s, ops/s, latency/op	Oddělit HW-accelerated a SW/reference běh
AI / NN inference	quantized conv, depthwise conv, FC, mobilenet-like micro model, keyword spotting, tiny vision model	latency/inference, inferences/s, peak RAM, model load time	Dva lane: portable reference a accelerated lane
Interní RAM	memcpy, strided read/write, random access	GB/s, ns/load, cycles/byte	Různé velikosti bufferů pro cache hot/cold
PSRAM	sequential throughput, random access, DMA-backed transfers	MB/s, ns/access, miss penalty	Jen kde je PSRAM přítomná; metadata musí nést typ a takt
Heap / allocátor	malloc/free, fragmentation stress, multi-task alloc contention	alloc/s, p95 latency, fragmentation ratio	Důležité pro reálné firmware scénáře
Flash raw I/O	read, write, erase sektor/blok, small random I/O	MB/s, ms/op, erase throughput	Měřit na vyhrazené benchmark partition
XIP / mmap	execute/read from mapped flash, cache thrash, code vs data fetch	cycles/op, slowdown vs IRAM/RAM	Důležité oddělit aligned a unaligned regiony
Filesystems	LittleFS/FATFS mount, create/read/append/delete	ops/s, p95 latency	Volitelné, ale velmi užitečné pro komunitu
Přehled testů pro I/O, rádio, napájení, real-time a bezpečnost
Oblast	Povinné subtesty	Primární metriky	Doporučené provedení
GPIO	max toggle rate, ISR pulse capture, simultaneous pin activity	MHz, jitter, missed events	Externě ověřit logic analyzerem/oscilloskopem
PWM	frequency sweep, duty resolution sweep, edge jitter	max stable frequency, duty resolution, jitter RMS	Fixní body např. 1 kHz / 20 kHz / 1 MHz / max
ADC	oneshot, continuous, calibration on/off, ENOB-style sweep	samples/s, INL/DNL proxy, absolute error, noise RMS	Nutná známá reference a kalibrace
DAC	DC level accuracy, waveform synthesis, DMA continuous mode	error, THD-proxy, max update rate	Jen cíle s DAC
SPI	master throughput, full-/half-duplex, DMA, short transfer overhead	MB/s, CPU load, latency	Loopback nebo fixture board
I2C	100/400/1000 kHz transfer, repeated start, error recovery	kb/s effective, retry count, clock stretch handling	Fixture se známým slave
UART	sustained throughput, framing error stress, USB CDC comparison	bit/s, error rate, CPU load	Test přes nativní UART i nativní USB CDC kde existuje
USB	CDC, DFU, host/device transfer	MB/s, enumeration time, flash time	Jen tam, kde je nativní USB dostupné
CAN / TWAI	classic CAN, CAN FD, bus saturation, recovery	fps, bus utilization, error counters	C5 má samostatný FD lane
Ethernet	iperf TCP/UDP, bring-up, OTA-over-Ethernet	Mb/s, RTT, packet loss	Jen desky s MAC+PHY nebo SPI-Ethernet
Wi‑Fi	STA/AP/STA+AP, TCP/UDP throughput, RSSI stability, roam/reconnect	Mb/s, RTT, jitter, reconnect time	Oddělit 2.4 a 5 GHz, kanál, AP model
BLE / BT	GATT throughput, scan/connect latency, notify/write performance, Classic BT kde existuje	kb/s, ms, packet loss	Classic BT jen pro původní ESP32
Coexistence	Wi‑Fi + BLE, Wi‑Fi + 802.15.4, BT/BLE + power save	throughput drop, latency inflation, loss rate	Povinné pro C5/C6/S3/C3 lane
Napájení	active, modem-sleep, light-sleep, deep-sleep, wake burst	avg current, peak current, energy/op	Zveřejnit board-level i module-level lane
Teplota a stabilita	prolonged stress, thermal ramp, brownout margin	error count, throttling, crash-free duration	30–120 min soak test dle kategorie
Boot a wake	reset-to-bootloader, reset-to-app_main, deep-sleep wake-to-stub, wake-to-ready	ms, cycles, p95	Externí GPIO marker + interní timer
OTA	HTTPS OTA, partition switch, rollback path, OTA under encryption	MB/min, success rate, reboot-to-ready	Povinné pro web registry „production lane“
Bezpečnost	secure boot enablement, flash encryption impact, signed image verify	boot time delta, flash R/W delta, pass/fail matrix	Oddělit functional a performance lane
RTOS / real-time	context switch, task wake latency, ISR latency, timer jitter	µs, p95/p99, missed deadlines	Oddělit no-load a stressed system
SDK / toolchain compat	ESP-IDF, Arduino-ESP32, PlatformIO/env, build warnings, size	pass/fail, binary size, startup regressions	Minimálně compile matrix, ideálně smoke-run matrix
Pro CPU a real-time doporučuji neomezit se na vlastní mikrokernely. CoreMark je vhodný pro „core-only“ pohled, Embench-IoT dává širší embedded workload bez OS a s omezenou pamětí, ULPMark-CM je vhodný pro spojení výkonu a energie a embench-rt se hodí pro interrupt latency a context switching. Tato kombinace je silnější než jediný headline benchmark. [14]
Pro AI/NN je vhodné zavést dva oddělené lane. Reference lane musí být přenositelný a stejnoměrný přes rodiny. Accelerated lane může využít ESP-DL tam, kde to dává smysl. Oficiální dokumentace ESP-DL dlouhodobě zmiňuje podporu pro ESP32, ESP32-S2, ESP32-S3 a ESP32-C3, zatímco aktuální getting-started a model-zoo komunikace se soustředí hlavně na S3/P4 a explicitně říká, že klasické ESP32 poběží pomaleji. Pro komunitní benchmark je proto rozumné začít akcelerovanou AI lane na S3 a teprve později rozšiřovat rozsah podle zralosti oficiální podpory. [15]
Měřicí metodika, opakovatelnost a kontrola chyb
Uvnitř firmware je nejvhodnější používat esp_timer pro mikrosekundové wall-clock měření a současně ukládat i raw cycle counts, aby šlo oddělit vliv DFS, sleep a preempce. Pro boot a wake čas je lepší měřit externě: při co nejčasnějším dosažitelném bodu firmware nebo wake stubu nastavit GPIO marker a čas vybírat na osciloskopu nebo logic analyzeru. Espressif navíc u deep-sleep wake stubu dokumentuje jak interní odhad přes cycle counter, tak přímé měření od wake-up do stub execution. [16]
Reprodukovatelnost by měla být vlastnost projektu, ne přídavek. ESP-IDF umí reproducible builds přes CONFIG_APP_REPRODUCIBLE_BUILD a Espressif zároveň poskytuje oficiální IDF Docker image, takže benchmark může vynutit deterministický build path, stabilní toolchain a přesný záznam verzí. Host-side automatizaci je výhodné stavět na pytest-embedded, protože to je cesta používaná i v ESP-IDF CI a je přímo dokumentovaná pro on-target testy. [17]
U ADC je nutné vynutit kalibrační režim, nebo alespoň publikovat, zda byl použit. Espressif v dokumentaci výslovně uvádí, že nominální reference 1100 mV se mezi čipy liší a může se pohybovat přibližně od 1000 do 1200 mV; benchmark bez informace o kalibraci by proto směšoval kvalitu analog front-endu s rozptylem výrobních tolerancí. U PWM je zase třeba testovat v definovaných bodech, protože u LEDC jsou frekvence a duty resolution vzájemně svázané. U flash/XIP je nutné evidovat, zda běh probíhá z IRAM/RAM nebo přes spi_flash_mmap/esp_partition_mmap, protože Espressif mapuje flash po 64 KB stránkách a mapování je určeno jen pro čtení. [18]
Doporučená metodika, statistika a kontrola chyb
Téma	Doporučení	Minimum pro veřejné sdílení	Poznámka
Build identita	Uložit ESP-IDF/Arduino/PlatformIO verzi, git SHA, Docker tag, target, sdkconfig hash	Povinné	Bez toho jsou výsledky neporovnatelné
Warm-up	3–5 zahřívacích běhů před měřením	Povinné	Nutné pro cache, branch predictor a RF stack stabilization
Opakování	20–50 opakování pro krátké mikroběhy, 5–10 pro dlouhé throughput testy	Povinné	U dlouhých testů více váží čas a teplota
Statistika	publish mean, median, stdev, p95, p99, min/max	Povinné	Jediná průměrná hodnota nestačí
Odlehlé hodnoty	Označit, ne skrývat; doporučený Hampel/IQR flag	Povinné	U RF a USB je jitter sám o sobě informace
Ambient	Teplota vzduchu, napájecí napětí, AP kanál/modulace, RF vzdálenost	Povinné pro RF a power	Bez prostředí není fairness
Power lane	Oddělit board-level a module-level	Doporučeno	Espressif pro řadu cílových desek nedoporučuje přímé srovnání jako modulový profil
ADC	Uvést reference source, attenuation, sample count, calibration mode	Povinné	Jinak nelze porovnat přesnost
Flash/XIP	Uvést partition layout, flash mode, XIP/mmap vs copy-to-RAM	Povinné	XIP není totéž co RAM execution
RF	Používat stejný AP/router, stejný kanál, fixní vzdálenost, fixní šířku pásma	Povinné	Zvlášť důležité pro C5/C6
Bezpečnost	Vést separátní lane „security off“ a „security on“	Povinné pro security testy	Secure boot a encryption mají výkonový dopad
Publikace raw dat	Ukládat raw JSON, trace CSV, volitelně waveform ZIP	Silně doporučeno	Usnadní audit a reprocessing
Pro power měření jsou přiměřeně dostupné a dobře zdokumentované dvě cesty. Joulescope JS220 nabízí současné měření proudu a napětí s rozsahem až ±3 A kontinuálně, 0.5 nA rozlišením, 2 MS/s a 300 kHz bandwidth. Nordic Power Profiler Kit II měří od 500 nA do 1 A, má 100 kS/s a automaticky přepíná měřicí rozsahy. Pro rychlé digitální periferie je vhodný například logic analyzer třídy Saleae Logic Pro 8 s až 500 MS/s digitálně a 50 MS/s analogově; pro hrubší, ale levný lab baseline stačí i malý USB osciloskop, například třída PicoScope 2000, která jde až do 1 GS/s a 100 MHz u vyšších modelů série. [19]
Architektura GitHub repozitáře a CI
ESP-IDF je oficiální framework pro ESP32/ESP32-S/C/H/P rodiny a podporuje více targetů přes idf.py set-target. To je ideální základ pro benchmark repo. Nad tím má smysl postavit vrstvu target manifestů, board manifestů a capability profilů. Buildy mají používat oficiální espressif/idf image a zapnuté reproducible builds, aby se minimalizoval šum mezi maintainerem, CI a komunitními přispěvateli. On-target automatizace má používat pytest-embedded, protože je to standardní cesta, kterou Espressif už popisuje i pro vlastní testy. [20]
 

Doporučená struktura repozitáře
esp32-bench/
  README.md
  LICENSE
  CONTRIBUTING.md
  CODE_OF_CONDUCT.md
  SECURITY.md

  boards/
    espressif/
      esp32-devkitc-v4.yaml
      esp32-c3-devkitm-1.yaml
      esp32-c6-devkitc-1.yaml
      esp32-c5-devkitc-1.yaml
      esp32-s3-devkitc-1.yaml
    vendors/
      seeed-xiao-esp32s3.yaml
      adafruit-feather-esp32s3.yaml

  soc_caps/
    esp32.yaml
    esp32s2.yaml
    esp32c3.yaml
    esp32c6.yaml
    esp32c5.yaml
    esp32s3.yaml

  sdk_profiles/
    esp-idf-v6.0.1.yaml
    esp-idf-v5.5.yaml
    arduino-esp32-stable.yaml
    platformio-espressif32.yaml

  tests/
    cpu/
    memory/
    flash/
    io/
    radio/
    power/
    security/
    rtos/
    startup/
    ota/
    sdk_compat/

  harness/
    pytest/
    schema/
    tooling/
    instrument_drivers/

  schemas/
    result-bundle.schema.json
    board-manifest.schema.json

  docker/
    Dockerfile.idf
    Dockerfile.harness

  .github/
    workflows/
      build.yml
      hilt.yml
      schema.yml
      publish-results.yml
    ISSUE_TEMPLATE/
    PULL_REQUEST_TEMPLATE.md
Doporučený CI model
Vrstva CI	Kde běží	Co dělá	Poznámka
Lint + schema	GitHub-hosted runner	JSON schema validace, markdown lint, YAML lint	Levné a rychlé
Build matrix	GitHub-hosted runner	idf.py set-target, compile, size report, smoke boot artifacts	Vhodné i pro PR
Harness unit tests	GitHub-hosted runner	test Python harnessu bez HW	Zabraňuje regresím infrastruktury
Hardware-in-the-loop	Self-hosted runner	flash, run, collect raw traces, upload bundle	Jen chráněné branche / schválené labely
Publish	GitHub-hosted nebo backend job	podpis, ingest, moderation queue	Oddělit od HIL
GitHub-hosted runners jsou vhodné pro build a rychlou validaci; self-hosted runners dávají kontrolu nad fyzickým HW a periferiemi, ale GitHub současně doporučuje vysokou opatrnost ve veřejných repozitářích, protože neověřený PR může na self-hosted stroji spustit škodlivý kód. Pro hardware laboratorní lane je proto vhodný model: veřejný repo, ale hardware workflow jen po maintainer approval nebo jen po merge do chráněné branche. Workflow artefakty by měly ukládat logy, binárky, raw JSON a trace ZIP; GitHub popisuje artefakty jako vhodný mechanismus pro sdílení build/test outputů mezi joby i po doběhu workflow. [21]
 

Návrh formátu výsledků
Výsledek má být bundle, ne jediný JSON dokument. Doporučené části:
{
  "schema_version": "1.0.0",
  "run_id": "uuid",
  "submitted_at": "2026-06-04T12:00:00Z",
  "board": {
    "vendor": "Espressif",
    "board_name": "ESP32-S3-DevKitC-1",
    "soc": "esp32s3",
    "module": "ESP32-S3-WROOM-1",
    "revision": "revX"
  },
  "build": {
    "sdk": "esp-idf",
    "sdk_version": "v6.0.1",
    "toolchain": "xtensa-esp32s3-elf",
    "docker_image": "espressif/idf:v6.0.1",
    "git_sha": "..."
  },
  "config": {
    "f_cpu_hz": 240000000,
    "flash_mode": "qio",
    "flash_freq_mhz": 80,
    "psram": true,
    "psram_freq_mhz": 80,
    "security_lane": "off"
  },
  "environment": {
    "ambient_c": 24.1,
    "supply_v": 3.300,
    "ap_model": "..."
  },
  "results": [
    {
      "test_id": "cpu.coremark.single",
      "unit": "coremark",
      "value": 1329.92,
      "samples": 20,
      "mean": 1329.92,
      "median": 1330.10,
      "stdev": 1.20,
      "p95": 1331.80
    }
  ],
  "attachments": [
    {"kind": "uart_log", "path": "logs/uart.txt"},
    {"kind": "power_trace_csv", "path": "traces/power.csv"}
  ]
}
Repozitář by měl mít pro přispěvatele dvě šablony: board manifest template a result submission template. Licence pro kód by měla být co nejflexibilnější, typicky Apache-2.0 nebo MIT, zatímco dokumentace a webový obsah mohou být pod CC BY 4.0. Pokud chcete maximalizovat kompatibilitu se směrem ESP-IDF a Arduino-ESP32, Apache-2.0 je pragmatická hlavní licence. ESP-IDF i Arduino-ESP32 jsou veřejně vedené open-source projekty a Arduino-ESP32 dnes explicitně komunikuje širokou podporu včetně ESP32, C3, C5, C6, H2, P4, S2 a S3. [22]
Návrh webu a backendu
Webová vrstva má mít tři role: registry výsledků, compare UI, moderation console. Protože GitHub Actions artefakty nejsou ideální jako trvalé úložiště benchmark historie, doporučuji vlastní backend, který uloží normalizované metriky, raw bundle, provenance a audit trail. GitHub artefakty lze dál používat jako krátkodobý transport mezi workflow joby a jako pomocný debug kanál. [5]
Doporučené databázové schéma
Tabulka	Klíčová pole	Účel
soc_family	id, name, arch, radio_caps, periph_caps	Kanonický popis capability rodin
board	id, vendor, name, soc_family_id, module, flash_mb, psram_mb	Desky a moduly
sdk_profile	id, name, sdk, version, toolchain, docker_image	Build profily
test_definition	id, test_id, category, unit, applicability_rule, version	Definice testů
run	id, board_id, sdk_profile_id, git_sha, submitted_by, status	Jedna submission
run_env	run_id, ambient_c, supply_v, rf_setup, operator_notes	Prostředí měření
metric_value	run_id, test_definition_id, value, mean, median, stdev, p95, p99	Normalizované výsledky
artifact	run_id, kind, uri, sha256, size	Raw logy a trace
moderation_event	run_id, actor, action, reason, at	Auditní stopa
submission_signature	run_id, method, publisher_id, verified	Důvěra a provenance
Doporučené API
Endpoint	Metoda	Účel
/v1/boards	GET	Seznam desek a capability filtrů
/v1/tests	GET	Test definitions a units
/v1/results	GET	Filtrované výsledky
/v1/results/{run_id}	GET	Detail jednoho běhu
/v1/uploads/presign	POST	Vygenerovat upload slot pro bundle
/v1/submissions	POST	Odeslat metadata a zaregistrovat běh
/v1/moderation/queue	GET	Fronta čekajících submission
/v1/moderation/{run_id}	POST	Approve / reject / quarantine
/v1/compare	GET	Agregovaný compare payload pro UI
Doporučené UI
UI by mělo mít tři hlavní pohledy. Board Compare porovná více desek napříč kategoriemi a lane. Run Explorer ukáže detail jedné submission včetně raw logů, trace a provenance. Regression View umožní sledovat posun stejné desky napříč verzemi SDK a toolchainu.
U tabulek je důležitější přesnost než „marketingové“ skóre. Filtry by měly umět minimálně: rodina SoC, konkrétní deska, SDK, verze SDK, bezpečnostní lane, ambient, napájení, RF setup, přítomnost PSRAM, zapnutí DFS, frekvence CPU a datum běhu.
Následující graf je ilustrativní ukázka toho, jak může web zobrazovat normalizované srovnání jedné kategorie; nejde o naměřená data:
 

Pro komunitní nahrávání výsledků doporučuji třífázový ingest: schema validation, capability validation, moderation. V první fázi se kontroluje JSON schéma a hash příloh. Ve druhé fázi se ověřuje, že submission netvrdí nemožnou kombinaci, například PSRAM lane na cíli bez PSRAM, Classic BT na S3 nebo dual-core scaling na C3. Ve třetí fázi maintainer schválí nebo zamítne submission s auditní stopou.
Priority implementace a roadmapa
První milník má dodat hodnotu i bez drahé lab techniky. To znamená spustit repo, build matrix, CPU/memory/flash lane, základní boot/wake, základní power lane a JSON/web ingest. Druhý milník má přidat I/O a RTOS. Třetí milník bezdrát a coexistence. Čtvrtý milník security/OTA. Pátý milník AI lane a širší komunitní onboarding. Tento sled je praktický i proto, že oficiální dokumentace a podpora nejsou ve stejném stupni zralosti pro všechny rodiny; například ESP32-C5 měl v některých starších stabilních větvích dokumentace explicitní varování, že obsah ještě není aktualizován, zatímco stabilní novější dokumentace už C5 běžně popisuje. [23]
Doporučené priority
Priorita	Deliverable	Odhad	Výstup
Nejvyšší	Repo skeleton, board manifests, result schema, build CI, Docker, reproducible build	1–2 týdny	Funkční základ projektu
Nejvyšší	CPU, memory, flash, boot, wake, RTOS basic lane	2–4 týdny	První veřejně užitečné srovnání
Vysoká	Power lane board-level + module-level, instrument drivers	2–4 týdny	Důvěryhodná spotřeba
Vysoká	GPIO/PWM/ADC/UART/SPI/I2C lane	3–5 týdnů	Praktické I/O porovnání
Střední	Wi‑Fi/BLE throughput, coexistence, Ethernet, USB	4–6 týdnů	Síťový benchmark
Střední	OTA + secure boot + flash encryption lane	2–3 týdny	Production-oriented lane
Střední až vysoká	Web UI, moderation, compare view, public API	3–5 týdnů	Sdílení výsledků
Nižší	AI accelerated lane, advanced thermal lane, CAN FD extension	4–8 týdnů	Rozšíření pro power users
 

Prakticky bych doporučil vydat v0.1 po dokončení build matrix, CPU/memory/flash/startup/RTOS a základního JSON uploadu. v0.2 po spotřebě a I/O. v0.3 po rádiu a coexistence. v1.0 až ve chvíli, kdy bude stabilní web, moderace a security lane. U AI lane je rozumné neblokovat roadmapu: oficiální ESP-DL dokumentace je dnes nejpřesvědčivější pro S3 a historicky pro ESP32/S2/S3/C3, takže C5/C6 je lepší nasadit později, než benchmark znečitelnit poloviční podporou. [24]
Doporučené zdroje a omezení
Doporučené zdroje
Kategorie	Doporučený zdroj	Poznámka
Oficiální SDK	ESP-IDF Programming Guide	Primární zdroj pro API, startup, sleep, security, OTA, FreeRTOS, RF coexistence. [25]
Datasheety SoC	ESP32 / S2 / C3 / C5 / C6 / S3 datasheety	Primární zdroj pro CPU, SRAM, periferie, rádia, current consumption. [26]
Build reproducibility	Reproducible Builds + IDF Docker Image	Nutné pro srovnatelnost komunitních výsledků. [27]
HW automatizace	ESP-IDF Tests with Pytest Guide	Nejlepší oficiální základ pro host harness. [28]
Power measurement	Espressif current-consumption guides, Joulescope, Nordic PPK2	Kombinace metodiky a nástrojů. [29]
CPU benchmarky	CoreMark, Embench-IoT, ULPMark-CM	Dobré minimum pro CPU a performance-per-energy. [30]
Real-time benchmarky	embench-rt + ESP-IDF high-priority interrupts/FreeRTOS docs	Dobré pro ISR/context-switch lane. [31]
AI/NN	ESP-DL docs a repo	Pro accelerated lane na podporovaných cílech. [32]
CI a provoz	GitHub Actions docs	Runner model, artefakty, self-hosted rizika. [33]
Otevřené otázky a omezení
První omezení je zralost a konzistence dokumentace pro ESP32-C5. V novější oficiální dokumentaci je C5 již běžně popsán, ale starší stabilní větev v době dostupných zdrojů ještě sama varovala, že obsah není plně aktualizován. Proto je vhodné v benchmarku nevycházet z implicitních family defaults, ale vždy ukládat reálné runtime parametry a verzi SDK. [34]
Druhé omezení je AI lane mimo ESP32-S3 a historicky podporované cíle ESP-DL. Oficiální přístupové materiály ESP-DL v aktuální podobě zdůrazňují S3/P4 jako hlavní nástupní platformy, byť release dokumentace dlouhodobě zmiňuje i ESP32/S2/S3/C3. Pro C5/C6 je proto lepší v první fázi držet jen reference inference lane a accelerated lane přidat až po jasné oficiální podpoře. [24]
Třetí omezení je porovnatelnost power a RF mezi různými vendor deskami. Zvlášť u komunitních výsledků může stejný SoC na jiné desce změnit regulator efficiency, LED leakage, USB bridge overhead, anténní zisk nebo USB napájecí šum. Proto je kritické publikovat raw environment metadata a držet samostatné lane pro module-only a board-level výsledky. [35]
Čtvrté omezení je bezpečnost HIL infrastruktury. Pokud bude hardware CI napojený na veřejný GitHub repo, musí být runner izolovaný a chráněný před neověřeným kódem. To není volitelný detail, ale architektonická podmínka pro celý projekt. [36]
Celkově je nejrigoróznější cesta postavit projekt jako modulární benchmark framework s explicitní capability mapou, separovanými lane, plně verzovaným JSON schématem a moderovaným webovým registry. Takový návrh je technicky realistický, dobře kompatibilní s Espressif toolchainem a dostatečně robustní pro komunitní GitHub provoz i dlouhodobé webové sdílení výsledků. [37]
________________________________________
[1] [7] [26] ESP32 Datasheet
https://documentation.espressif.com/esp32_datasheet_en.html?utm_source=chatgpt.com
[2] [29] [35] Current Consumption Measurement of Modules - ESP32 - — ESP-IDF Programming Guide v6.0.1 documentation
https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/current-consumption-measurement-modules.html?utm_source=chatgpt.com
[3] [13] Performance - ESP32 - — ESP-IDF Programming Guide v4.4.8 documentation
https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32/api-guides/performance/index.html?utm_source=chatgpt.com
[4] [6] [20] [25] [37] ESP-IDF Programming Guide - ESP32 - — ESP-IDF Programming Guide release-v5.5 documentation
https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32/?utm_source=chatgpt.com
[5] https://docs.github.com/en/actions/concepts/workflows-and-actions/workflow-artifacts?apiVersion=2022-11-28
https://docs.github.com/en/actions/concepts/workflows-and-actions/workflow-artifacts?apiVersion=2022-11-28
[8] ESP32-S2-WROOM & ESP32-S2-WROOM-I Datasheet | Espressif Documentation
https://documentation.espressif.com/esp32-s2-wroom_esp32-s2-wroom-i_datasheet_en.html?utm_source=chatgpt.com
[9] https://documentation.espressif.com/esp32-c3_datasheet_en.html
https://documentation.espressif.com/esp32-c3_datasheet_en.html
[10] https://documentation.espressif.com/projects/esp-at/zh_CN/latest/esp32c2/AT_Command_Examples/network_provisioning_examples.html
https://documentation.espressif.com/projects/esp-at/zh_CN/latest/esp32c2/AT_Command_Examples/network_provisioning_examples.html
[11] https://documentation.espressif.com/esp32-c5_datasheet_en.html
https://documentation.espressif.com/esp32-c5_datasheet_en.html
[12] documentation.espressif.com
https://documentation.espressif.com/esp32-s3_datasheet_en.pdf
[14] [30] https://www.eembc.org/coremark/
https://www.eembc.org/coremark/
[15] [24] https://docs.espressif.com/projects/esp-dl/en/release-v1.1/esp32s3/index.html
https://docs.espressif.com/projects/esp-dl/en/release-v1.1/esp32s3/index.html
[16] ESP Timer (High Resolution Timer) - ESP32 - — ESP-IDF Programming Guide latest documentation
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/esp_timer.html?utm_source=chatgpt.com
[17] [27] Reproducible Builds - ESP32 - — ESP-IDF Programming Guide latest documentation
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/reproducible-builds.html?utm_source=chatgpt.com
[18] https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc/adc_calibration.html
https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/adc/adc_calibration.html
[19] https://download.joulescope.com/products/JS220/JS220-K000/description.html
https://download.joulescope.com/products/JS220/JS220-K000/description.html
[21] [33] https://docs.github.com/actions/reference/runners/github-hosted-runners
https://docs.github.com/actions/reference/runners/github-hosted-runners
[22] https://github.com/espressif/esp-idf
https://github.com/espressif/esp-idf
[23] [34] ESP-IDF Programming Guide - ESP32-C5 - — ESP-IDF Programming Guide v5.3.2 documentation
https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32c5/index.html?utm_source=chatgpt.com
[28] ESP-IDF Tests with Pytest Guide - ESP32 - — ESP-IDF Programming Guide latest documentation
https://docs.espressif.com/projects/esp-idf/en/latest/contribute/esp-idf-tests-with-pytest.html?utm_source=chatgpt.com
[31] https://github.com/embench/embench-rt
https://github.com/embench/embench-rt
[32] https://docs.espressif.com/projects/esp-dl/en/latest/introduction/readme.html
https://docs.espressif.com/projects/esp-dl/en/latest/introduction/readme.html
[36] https://docs.github.com/en/actions/hosting-your-own-runners/adding-self-hosted-runners
https://docs.github.com/en/actions/hosting-your-own-runners/adding-self-hosted-runners
