# ESPMark pro komunitní benchmark ESP8266 a ESP32

## Shrnutí a doporučená strategie

Nejlepší strategie pro ESPMark je držet **hlavní skóre jen na deterministických, čistě on-chip testech**, které nepotřebují znalost konkrétní desky: CPU integer, algoritmické workloady bez periferií, RAM, čtení z flash a několik praktických workloadů typu JSON/string. Naopak testy silně závislé na prostředí nebo konfiguraci — Wi‑Fi, boot time, Web Serial throughput, NVS/EEPROM write, PSRAM — patří mimo hlavní skóre do oddělené sekce `Optional/Environment Score`. Tím se nejlépe naplní cíl „férového komunitního srovnání“ bez laboratorních nároků a bez vazby na konkrétní board features. Tento přístup je v souladu i s tím, jak se v průmyslových benchmarcích oddělují reprodukovatelné výpočetní workloady od environmentálních metrik a jak se pro agregaci výkonu běžně používá geometrický průměr a referenční normalizace. citeturn44search0turn44search4turn44search5

Pro webový deployment je váš směr správný: **ESP Web Tools už dnes umí vybrat správný build podle chip family z manifestu** a podporuje i rodiny jako ESP8266, ESP32, ESP32‑C5, ESP32‑C6, ESP32‑P4, ESP32‑S2 a ESP32‑S3. Pro ESP32-family ale vyžaduje u IDF v4+ předem připravený merged binary, protože flash mode / size / frequency neumí patchovat za běhu tak jako `esptool`. To znamená, že „generic firmware per chip family“ je reálně proveditelný, ale musíte mít **manifest po chip family** a u některých rodin případně ještě **konzervativní variantu flash mode**, nikoli firmware po značkových deskách. citeturn40search0turn40search1turn25search2

Z hlediska architektur je důležité, aby ESPMark nepřestřelil váhu float testů. ESP32 classic má FPU podporu, ESP32‑S3 má single-precision FPU a vektorové instrukce, zatímco datasheety ESP32‑C3, ESP32‑C5 a ESP32‑C6 popisují jádra RV32IMC/RV32IMAC bez RISC‑V `F` extension. Espressif zároveň výslovně uvádí, že i na targetech s FPU je `double` bez HW akcelerace a je emulované softwarem. Proto je správné dát **float32 jen malou váhu** a **double vyřadit z core score**. citeturn16search0turn16search30turn15view0turn15view1turn14view2turn38search0

Prakticky tedy doporučuji model:

- **Core score**: pouze deterministické testy běžící bez sítě, bez board-specific periferií a bez zápisu do flash.
- **Full mode** jako jediný „oficiální leaderboard mode“.
- **Quick mode** pro onboarding a sdílení.
- **Stress mode** odděleně pro throttling/stabilitu.
- **Stability** ne jako samostatnou kategorii s velkou váhou, ale jako **malou penalizaci** postavenou na `p95/median`.
- **Versioning** rozdělit na `schema_version`, `firmware_version`, `scoring_version` a `benchmark_profile`, aby šly výsledky porovnávat dlouhodobě bez rozbití starších leaderboardů. citeturn44search5turn40search0turn41search0

## Doporučená sada testů

Níže je nejlepší kombinace **dalších** testů nad vaším současným stavem. Záměrně jsem preferoval workloady, které fungují bez pinů, LED, tlačítek, senzorů, displejů, LoRa, kamery a dalších board-specific závislostí.

### Doporučené testy pro ESPMark

| Test | Co přesně měří a proč je užitečný | Hlavní skóre nebo doplněk | Stabilita a podpora | Rizika zkreslení | Implementace, čas, jednotka a JSON |
|---|---|---|---|---|---|
| **Mandelbrot fixed-point Q16.16** | Měří kombinaci integer ALU, větvení, cache/RAM locality a dlouhého deterministického algoritmu. Je mnohem „realističtější“ než čistý add/mul loop a dobře odděluje architektury i kompilátory. | **Ano, Core Score** | **Velmi stabilní**, funguje na **ESP8266 i celé ESP32 family**. Díky fixed-point se vyhne rozdílům FPU/libm. | Musí být fixní rozlišení, fixní počet iterací a fixní viewport. Jinak bude test neporovnatelný. | **Střední náročnost**. Quick: ~0.8–1.2 s, Full: ~1.5–2.5 s. Primární jednotka `us`, sekundární `pixels_per_s`. JSON: `metrics.cpu.mandelbrot_q16` |
| **Matrix multiply int16→int32** | Měří integer MAC-like workload, RAM reuse, jednoduchý „DSP-like“ pattern a citlivost na cache. Výborně doplňuje vaše existující microbenchy. | **Ano, Core Score** | **Stabilní**, funguje na **ESP8266 i ESP32**. | Velikost matice nesmí být moc malá, jinak dominuje overhead; nesmí být moc velká, jinak z ní bude spíš RAM benchmark. Doporučuji fixní blokovaný formát. | **Střední náročnost**. Quick: ~0.6–1.0 s, Full: ~1.5–2.0 s. Jednotka `us` a odvozeně `mmul_ops_s`. JSON: `metrics.cpu.matmul_i16` |
| **Float32 affine/vector transform** | Měří výkon `float` operací bez drahých transcendental funkcí. Je to užitečné, protože část hobby projektů používá filtry, IMU, jednoduchou signal processing logiku nebo grafiku. | **Ano, ale s nízkou vahou** | **Středně stabilní**. Funguje na **ESP8266 i ESP32**, ale výsledky se mezi architekturami budou výrazně lišit kvůli rozdílné HW podpoře float. | Na některých targetech je `float` HW-urychlený, na některých ne; `double` je softwarové a do core nepatří. Nepoužívat `sin/cos/log`, jen add/mul/div na fixních datech. | **Nízká až střední náročnost**. Quick: ~0.4–0.8 s, Full: ~1.0–1.5 s. Jednotka `us` a `float_ops_s`. JSON: `metrics.cpu.float32_affine` citeturn16search0turn16search30turn15view0turn15view1turn14view2turn38search0 |
| **CRC32 nad fixním RAM bufferem** | Měří praktický byte-stream workload běžný v embedded komunikaci, OTA, storage a protokolech. Je lépe srozumitelný pro komunitu než „CRC-like workload“. | **Ano, spíš v Practical IoT Score** | **Velmi stabilní**, funguje na **ESP8266 i ESP32**. | Pokud použijete ROM/HW varianty na jedněch targetech a soft varianty na jiných, srovnání bude méně fér. Pro core doporučuji jednu referenční software implementaci. | **Nízká náročnost**. Quick: ~0.3–0.6 s, Full: ~0.8–1.2 s. Jednotka `MB/s` i `us` nad fixní velikostí. JSON: `metrics.practical.crc32_ram` citeturn37search0turn37search1 |
| **SHA‑256 nad fixním RAM bufferem** | Měří praktický crypto/hash workload relevantní pro OTA, integritu, podpisy a cloud/IoT scénáře. | **Ano, ale nízká váha nebo doplněk** | **Stabilní**, funguje na **ESP8266 i ESP32**, pokud použijete jednotnou SW implementaci. | Espressif dokumentace výslovně uvádí HW SHA akceleraci v mbedTLS a také to, že může být někdy rychlejší a někdy pomalejší; pokud výkon HW/SW nebude explicitně označen, zkreslí to leaderboard. | **Střední náročnost**. Quick: ~0.3–0.6 s, Full: ~0.8–1.2 s. Jednotka `MB/s` nebo `us`. JSON: `metrics.practical.sha256_ram` plus `implementation: "soft_ref"` nebo `"mbedtls_hw"`. citeturn22search0turn22search2turn22search7 |
| **JSON parse + generate** | Měří velmi reálný embedded workload: parsování konfigurace a generování payloadů. Je to přesně typ práce, který hobby uživatelé dělají. | **Ano, Practical IoT Score** | **Dobře reprodukovatelné**, funguje na **ESP8266 i ESP32**. | Výsledek závisí na vybrané knihovně a payloadu. Pro fairness je nutný fixní JSON dokument a fixní knihovna/verze. | **Nízká náročnost**. Quick: ~0.6–1.0 s, Full: ~1.5–2.0 s. Jednotka `ops/s` nebo `us/op`. JSON: `metrics.practical.json_parse`, `metrics.practical.json_generate` citeturn20search0turn20search1turn20search3 |
| **String formatting workload** | Měří `snprintf`, concat a práci se stringy, tedy běžné logování, web payloady a textové protokoly. | **Ano, Practical IoT Score** | **Středně stabilní**, funguje na **ESP8266 i ESP32**. | Silněji odráží libc/toolchain než čistý hardware. Proto má být váha menší než u CPU/RAM. Nepoužívat `String` realloc chaos bez kontroly kapacity. | **Nízká náročnost**. Quick: ~0.4–0.7 s, Full: ~1.0–1.5 s. Jednotka `ops/s`. JSON: `metrics.practical.string_format` |
| **Heap churn + post-test fragmentation** | Měří rychlost alokace/dealokace malých a středních bloků a současně „zdraví“ heapu po workloadu. V hobby embedded světě je to velmi praktické. | **Ano, ale nízká váha** | **Stabilní**, funguje na **ESP8266 i ESP32**. | Samotná rychlost alloc/free není všechno; důležité je reportovat i post-state. Na ESP32 je vhodné přidat `largest_free_block`; na ESP8266 stačí alespoň free heap před/po a count failů. | **Střední náročnost**. Quick: ~0.5–0.8 s, Full: ~1.0–1.8 s. Jednotka `ops/s` + stavové metriky v `bytes` a `percent`. JSON: `metrics.memory.heap_churn`, `metrics.memory.heap_post`. citeturn36search0turn36search2turn19search0 |
| **Flash sequential read** | Měří čtení z interní flash, tedy praktické načítání assetů, konfigurace a embedded data. Je to generic a nemusí zapisovat do flash. | **Ano, Flash Score** | **Dobře stabilní**, funguje na **ESP8266 i ESP32**. | Flash speed/mode a mapování se mezi moduly liší. Na ESP32 je vhodné číst ze zvláštní benchmark partition nebo z fixního read-only blobu; na ESP8266 z PROGMEM/read-only blobu. | **Střední náročnost**. Quick: ~0.5–0.8 s, Full: ~1.5–2.0 s. Jednotka `MB/s` i `us`. JSON: `metrics.flash.read_seq` plus `bytes` a `source: "partition|progmem"`. citeturn28search0turn32search1turn33search0 |
| **Mixed sustained CPU stress loop** | Měří dlouhodobé chování bez periferií: sustained CPU, jitter, watchdog discipline a případný throttling/brownout. Je užitečný hlavně pro Stress mode. | **Ne do Core Score, ale ano do Stress mode** | **Stabilní jako stress test**, funguje na **ESP8266 i ESP32**, pokud je workload chunkovaný a pravidelně yielduje. | Bez chunkování může spouštět watchdog. Na ESP8266 dlouhé smyčky bez `delay/yield` zhoršují běh Wi‑Fi stacku; na ESP32 existuje Task Watchdog pro tasky běžící příliš dlouho bez yield. | **Nízká implementační náročnost**. Stress: 60 s celkem, report po oknech po 5 s. Jednotka `score_window` / `ops/s`. JSON: `metrics.stress.mixed_cpu_sustain` with `windows[]`. citeturn19search1turn18search0turn18search1 |
| **PSRAM seq read/write/copy** | Měří external RAM, která je pro část ESP32 boardů prakticky důležitá. | **Jen optional** | Funguje jen na **ESP32 s automaticky dostupnou PSRAM**. | PSRAM sdílí cache region s flash a při velkých blocích může výkon padat; navíc ne všechny boardy PSRAM mají a přístup má specifická omezení. | **Střední náročnost**. Full optional: ~1–2 s. Jednotka `MB/s`. JSON: `metrics.optional.psram_seq` plus `psram_size_bytes`. citeturn23search2turn23search8turn24search0 |

### Co bych z této tabulky skutečně zařadil do první stabilní verze

Do **ESPMark Full v1** bych doporučil zařadit:

- Mandelbrot fixed-point
- Matrix multiply int16→int32
- Float32 affine transform s malou vahou
- CRC32 RAM
- JSON parse + generate
- String formatting
- Heap churn + heap post-state
- Flash sequential read

To je nejlepší poměr mezi praktičností, pochopitelností a generic kompatibilitou. SHA‑256 bych dal do **v0.3 nebo v1.0 s nízkou váhou** až ve chvíli, kdy budete mít pevně rozhodnuto, zda benchmark měří **software-only hash**, nebo **reálný platform hash včetně HW akcelerace**. PSRAM bych od začátku držel mimo core. citeturn22search0turn22search2turn23search2

## Testy, které nezařazovat do hlavního skóre

Tady je sada testů, které jsou sice technicky možné, ale pro **hlavní leaderboard** jsou příliš závislé na prostředí, konkrétní desce nebo konfiguraci.

| Test | Proč ne do hlavního skóre | Kam s ním místo toho |
|---|---|---|
| **Wi‑Fi scan** | Scan čas závisí na okolních AP, kanálech, country nastavení, scan mode a dwell time per channel; Espressif dokumentace přímo ukazuje, že scan chování a délky se liší podle konfigurace a prostředí. | `Optional/Environment Score` citeturn26search1turn27search0 |
| **Wi‑Fi connect** | Connect zahrnuje scan, autentizaci, asociaci a DHCP; závisí na AP, heslu, RSSI, congestion i timeout chování. | `Optional/Environment Score` citeturn27search1turn27search3 |
| **Serial throughput přes Web Serial** | Záleží na browseru, HTTPS contextu, Web Serial implementaci, USB‑CDC vs USB‑UART bridge, kabelu a host OS; navíc u internal USB‑CDC nemusí baud rate reálně určovat throughput. | Samostatná „Host/Transport“ metrika citeturn41search0turn31search0turn39search0turn39search5turn38search8 |
| **Boot/ready time** | Výsledek míchá ROM/bootloader/app start, USB enumeraci, DTR/RTS reset chování a browser timing. Web Serial navíc může pracovat se signály DTR/RTS explicitně. | Samostatná „Boot UX“ metrika, ne Core Score citeturn31search0turn31search1turn18search0 |
| **NVS / Preferences read-write** | Na ESP32 je Preferences jen wrapper nad NVS; NVS je log-structured storage po stránkách sektoru. Latence se mění se stavem partition a historií zápisů. | Volitelný storage test mimo core citeturn42search0turn42search6turn42search4 |
| **EEPROM write/commit na ESP8266** | ESP8266 EEPROM emulace používá jeden flash sektor za FS a `commit()` přepisuje flash; dokumentace výslovně upozorňuje na rychlé opotřebení. To je pro veřejný benchmark špatný trade-off. | Jen explicitně opt-in test s varováním citeturn43search0 |
| **PSRAM** | Není přítomná na všech deskách, má cache/flash omezení a nedává smysl ji míchat s interní RAM do jedné hlavní známky. | Volitelný `psram_score` citeturn23search2turn23search8 |
| **Filesystem benchmark** | Layout a velikost FS se mezi ESP8266 boardy liší podle flash size a board mapping; na ESP32 je zase rozhodující partition table. | Volitelný storage/profile test citeturn33search1turn30search0 |
| **Double precision workload** | Espressif uvádí, že `double` je na targetech s FPU stále bez HW akcelerace; spíš testuje soft emulaci a toolchain. | Doplňková orientační metrika, ne core citeturn38search0turn38search3 |
| **Komprese/dekomprese** | Je použitelná, ale výsledek bude extrémně závislý na zvolené knihovně a parametrech. `heatshrink` cílí na embedded low-memory, `miniz` je zlib-like single-file knihovna; bez pevného standardu by leaderboard měřil spíš výběr knihovny než čip. | Experimentální profil až od v0.3+ citeturn21search1turn21search0 |
| **Periferie typu ADC, baterie, displej, kamera, LoRa, senzory** | Board-specific hardware. Jde přímo proti principu generic firmware. | Nezařazovat vůbec do ESPMark Core |

## Doporučená struktura skóre a váhy

### Navržená stromová struktura

```text
ESPMark Core Score
├─ CPU Score
│  ├─ Integer Micro Score
│  ├─ Mandelbrot Score
│  ├─ Matrix Score
│  └─ Float32 Score
├─ Memory Score
│  ├─ RAM Micro Score
│  └─ Heap Score
├─ Flash Score
│  └─ Flash Read Score
├─ Practical IoT Score
│  ├─ CRC32 Score
│  ├─ JSON Score
│  ├─ String Score
│  └─ SHA256 Score
└─ Stability Modifier
```

```text
Optional/Environment Score
├─ Stress Sustain Score
├─ Serial Transport Score
├─ Boot Ready Score
├─ WiFi Scan Score
├─ WiFi Connect Score
├─ PSRAM Score
└─ NVS/EEPROM Score
```

Toto rozdělení dává smysl proto, že odděluje **čistý výpočetní výkon** od **praktických workloadů** a zároveň drží stranou vše, co už není vlastností samotného SoC + generic firmware. SPEC-style benchmarky také oddělují dílčí benchmarky, normalizují je vůči referenci a agregují je geometrickým průměrem, nikoli jedním součtem „všech sekund dohromady“. citeturn44search0turn44search4turn44search2

### Doporučené váhy kategorií

| Kategorie / dílčí složka | Váha v Core Score | Proč dává smysl |
|---|---:|---|
| **CPU integer microtests** | **18 %** | Už je máte hotové a jsou nejméně sporné napříč rodinami. |
| **Mandelbrot fixed-point** | **8 %** | Přidává „reálný algoritmus“, ne jen syntetický microbench. |
| **Matrix multiply int16→int32** | **8 %** | Reprezentuje DSP/practical integer math a práci s RAM/cache. |
| **Float32 affine transform** | **8 %** | Je relevantní, ale kvůli architekturám má být nízko. |
| **RAM microtests** | **20 %** | U MCU je RAM často stejně důležitá jako ALU; memcpy/memset/strided read jsou pro hobby workloads praktické. |
| **Heap churn + post-state** | **6 %** | Malá, ale užitečná váha; pomáhá odlišit „papírově rychlé“ a „prakticky použitelné“. |
| **Flash sequential read** | **12 %** | Čtení z flash je běžné a generic; zápis ale do core nepatří. |
| **CRC32 RAM** | **4 %** | Praktický stream/hash workload s výbornou stabilitou. |
| **JSON parse + generate** | **9 %** | Velmi praktický embedded/web/IoT workload pro běžné uživatele. |
| **String formatting** | **3 %** | Reálné, ale značně závislé na libc/toolchain, proto nízká váha. |
| **SHA‑256** | **4 %** | Praktické pro integritu a OTA; držet nízko kvůli HW/SW rozdílům. |
| **Stability modifier** | **max −5 % penalizace** | Nestabilní run nemá mít stejné skóre jako stabilní run, ale penalizace má být malá a čitelná. |

To dohromady dává 100 % Core Score plus samostatný stability modifier. Prakticky to znamená, že **CPU + Memory** tvoří 68 %, což odpovídá tomu, že benchmark je pořád hlavně o čipu a jeho základní paměťové cestě, ne o síti nebo host PC. Flash a Practical IoT dohromady tvoří 32 %, takže skóre není sterilní synthetic-only číslo. citeturn44search0turn44search5

## Přesný návrh výpočtu skóre

### Kanonická raw metrika

Doporučuji, aby **kanonická raw metrika byla téměř vždy čas** nad fixním workloadem:

- `us` pro krátké CPU a memory testy,
- `ms` pro delší testy,
- u throughput testů ukládat **obojí**:
  - kanonicky `time_us`,
  - uživatelsky `MB/s` nebo `ops/s`.

To je důležité kvůli agregaci. Když máte fixní workload, čas je univerzální a jde snadno normalizovat vůči referenci. Pro ESP32 je vhodné měřit přes `esp_timer_get_time()`, které dává čas v mikrosekundách a Espressif jej doporučuje pro fine-grained timing; pro ESP8266 je k dispozici `micros()`. citeturn35search8turn19search1

### Agregace v rámci jednoho testu

Pro každý test doporučuji:

- **1 warm-up run** mimo statistiku,
- potom
  - **Quick**: 5 měření,
  - **Full**: 9 měření,
  - **Stress**: okna po 5 s nebo 10 s.

Pro **kanonický výsledek testu** použijte **medián**.  
`mean`, `min`, `max`, `stdev` a `p95` pouze ukládejte a zobrazujte. Medián je transparentní a odolný proti jednorázovým výkyvům; SPEC také dlouhodobě pracuje s geometrickými agregacemi a v některých suite/reporting pravidlech výslovně zdůrazňuje median runtime jako reportovatelnou a smysluplnou hodnotu. citeturn44search0turn44search2

### Kdy použít medián, aritmetický a geometrický průměr

- **Medián**: kanonická raw hodnota **uvnitř jednoho testu**.
- **Aritmetický průměr**: jen pro pomocné dashboardy a průměry **stejného typu metrik**, například „průměrná MB/s v rámci několika bloků jednoho flash testu“.
- **Geometrický průměr**: pro agregaci **normalizovaných dílčích skóre** do kategorií a z kategorií do `ESPMark Core Score`. To je nejčistší způsob, jak nesčítat nesouměřitelné jednotky a nenechat jeden extrémní test zcela převálcovat ostatní. citeturn44search0turn44search2turn44search4

### Normalizace vůči referenční desce

Doporučuji publikovat **oficiální referenční sadu** pro každý `scoring_version`, například:

- `reference_set_id: "espmark-ref-1"`
- referenční hw: 3 kusy stejného, běžně dostupného ESP32 modulu
- referenční číslo pro každý test = **medián z mediánů** těchto tří kusů.

Pak pro každý test `j`:

\[
r_j = clamp\left(\frac{t_{ref,j}}{t_{dut,j}}, 0.25, 4.0\right)
\]

kde

- `t_ref,j` = referenční medián času,
- `t_dut,j` = medián času testovaného zařízení,
- `clamp()` omezuje extrémy, aby jeden rozbitý nebo zcela atypický dílčí test nedeformoval celý výsledek.

Prakticky to znamená:

- **vyšší `r_j` je lepší**,
- test s polovičním časem má zhruba 2× lepší poměr,
- test s dvojnásobným časem má zhruba 0.5× poměr.

`0.25…4.0` je rozumný kompromis: stále umožní velké generační rozdíly mezi ESP8266 a modernějšími ESP32 variantami, ale zabrání tomu, aby outlier udělal z celého skóre nesmysl. To je doporučení, ne průmyslový standard; důležité je, aby bylo veřejně zveřejněné a fixní v daném `scoring_version`. citeturn44search4turn44search5

### Výpočet category score a Core Score

Pro kategorii `C` s testy `j` a vahami `w_j`:

\[
score_C = 1000 \times \exp\left(\frac{\sum_j w_j \ln(r_j)}{\sum_j w_j}\right)
\]

Pro celý `ESPMark Core Score` s kategoriemi `C` a vahami `W_C`:

\[
ESPMarkCore = 1000 \times \exp\left(\frac{\sum_C W_C \ln(score_C / 1000)}{\sum_C W_C}\right) \times stability\_factor
\]

Skóre `1000` pak odpovídá referenční sadě.  
Skóre `1500` znamená zhruba o 50 % lepší agregovaný výkon než reference.  
Skóre `700` znamená zhruba 30 % pod referencí.

To je pro leaderboard srozumitelnější než „nižší je lepší“, ale raw times musí zůstat v JSON, aby šla pravidla později auditovat.

### Penalizace nestability přes p95

Doporučuji **malou, čitelnou penalizaci**, nikoli tvrdé škrtání výsledků.

Pro každý test:

\[
s_j = \frac{p95_j}{median_j}
\]

Pak jako agregovaný stability ratio:

\[
S = \exp\left(\frac{\sum_j w_j \ln(s_j)}{\sum_j w_j}\right)
\]

A stability modifier:

- pokud `S <= 1.10`, potom `stability_factor = 1.00`
- jinak

\[
stability\_factor = max(0.95, 1.00 - 0.25 \times (S - 1.10))
\]

Tedy:

- do **10 % inflace p95 vůči mediánu** bez trestu,
- potom jemná penalizace,
- stropem je **−5 %**.

Tím odměníte reprodukovatelné běhy, ale nezničíte score kvůli jednomu drobnému jitteru.

### Jak ošetřit extrémní hodnoty

Doporučení je jednoduché a transparentní:

- **neprovádět skryté mazání outlierů**,
- oddělit **warm-up** od měřených běhů,
- kanonický výsledek = **medián**,
- `mean/stdev/p95` uložit pro audit,
- pokud měření spadne na watchdog, alloc failure nebo storage/network error, označit sample jako `invalid`, nesmí se tvářit jako běžná hodnota.

To je férovější než agresivní trimming a lépe se to vysvětluje komunitě.

## JSON schéma výsledku a verzování

### Doporučený versioning model

Dlouhodobou kompatibilitu udržíte, když rozdělíte verze na čtyři vrstvy:

| Pole | Co verzuje | Kdy změnit |
|---|---|---|
| `schema_version` | JSON strukturu | když změníte shape výsledku |
| `firmware_version` | implementaci firmware | při každém release firmware |
| `scoring_version` | váhy, reference, matematiku skóre | **jen když se mění srovnatelnost** |
| `benchmark_profile` | Quick / Full / Stress | vždy součást identity runu |

Dále doporučuji přidat:

- `test_set_id` — která sada testů je povinná,
- `reference_set_id` — vůči čemu se skóre normalizuje,
- `official_generic_firmware` — bool,
- `target_family` — `ESP8266`, `ESP32`, `ESP32-C3` atd.,
- `board_user_label` — text od uživatele, **nikdy ne vstup do scoringu**.

Leaderboard by měl míchat jen výsledky se stejným:

- `scoring_version`
- `benchmark_profile`
- `test_set_id`

a pro „main leaderboard“ navíc jen tehdy, pokud `official_generic_firmware = true` a run neobsahuje chyby v core metrikách. citeturn44search5

### Doporučená JSON struktura

Níže je návrh, který je validovatelný, auditovatelný a zároveň dobře použitelný pro web i leaderboard.

```json
{
  "schema_version": "1.0.0",
  "firmware_version": "0.3.0",
  "scoring_version": "1.0.0",
  "benchmark_profile": "full",
  "test_set_id": "espmark-core-1",
  "reference_set_id": "espmark-ref-1",
  "official_generic_firmware": true,

  "run": {
    "result_id": "uuid-or-hash",
    "timestamp_utc": "2026-06-05T12:34:56Z",
    "duration_ms": 22140,
    "valid_for_leaderboard": true,
    "errors": [],
    "warnings": []
  },

  "device": {
    "target_family": "ESP32-C3",
    "chip_model": "ESP32-C3",
    "chip_revision": 3,
    "cpu_freq_mhz": 160,
    "cores": 1,
    "flash_size_bytes": 4194304,
    "flash_speed_hz": 40000000,
    "flash_mode": "dio",
    "psram_size_bytes": 0,
    "arduino_core": "3.3.8",
    "sdk": "esp-idf 5.5",
    "board_user_label": "LOLIN C3 Mini",
    "transport": "usb_serial_jtag"
  },

  "environment": {
    "web_runner": "espmark-web 0.8.0",
    "browser_family": "Chromium",
    "browser_version": "136",
    "webserial": true,
    "https_context": true
  },

  "scores": {
    "espmark_core": 1187.4,
    "cpu": 1215.8,
    "memory": 1098.2,
    "flash": 1044.6,
    "practical_iot": 1239.0,
    "stability_factor": 0.987,
    "optional_environment": null
  },

  "metrics": {
    "cpu": {
      "mandelbrot_q16": {
        "unit": "us",
        "direction": "lower_is_better",
        "workload_id": "mandelbrot_q16_v1",
        "samples": 9,
        "warmup_samples": 1,
        "stats": {
          "mean": 231404,
          "median": 229991,
          "min": 227880,
          "max": 236214,
          "stdev": 2421,
          "p95": 235871
        },
        "derived": {
          "pixels_per_s": 445245.7,
          "normalized_ratio": 1.184,
          "score": 1184.0
        }
      },

      "matmul_i16": {
        "unit": "us",
        "direction": "lower_is_better",
        "workload_id": "matmul_i16_v1",
        "samples": 9,
        "stats": {
          "mean": 158432,
          "median": 157981,
          "min": 156440,
          "max": 161004,
          "stdev": 1419,
          "p95": 160842
        },
        "derived": {
          "ops_per_s": 4219830.1,
          "normalized_ratio": 1.091,
          "score": 1091.0
        }
      },

      "float32_affine": {
        "unit": "us",
        "direction": "lower_is_better",
        "workload_id": "float32_affine_v1",
        "implementation": "float32_scalar_ref",
        "samples": 9,
        "stats": {
          "mean": 119330,
          "median": 118902,
          "min": 117870,
          "max": 121440,
          "stdev": 1080,
          "p95": 121100
        },
        "derived": {
          "float_ops_s": 8401090.2,
          "normalized_ratio": 0.944,
          "score": 944.0
        }
      }
    },

    "memory": {
      "heap_churn": {
        "unit": "ops/s",
        "direction": "higher_is_better",
        "workload_id": "heap_churn_v1",
        "samples": 9,
        "stats": {
          "mean": 8201.4,
          "median": 8188.2,
          "min": 8064.4,
          "max": 8333.1,
          "stdev": 75.2,
          "p95": 8312.9
        },
        "post_state": {
          "free_heap_bytes": 258112,
          "largest_free_block_bytes": 212992,
          "alloc_fail_count": 0
        },
        "derived": {
          "normalized_ratio": 1.032,
          "score": 1032.0
        }
      }
    },

    "flash": {
      "read_seq": {
        "unit": "MB/s",
        "direction": "higher_is_better",
        "workload_id": "flash_read_seq_v1",
        "source": "partition",
        "bytes": 262144,
        "samples": 9,
        "stats": {
          "mean": 11.4,
          "median": 11.3,
          "min": 11.1,
          "max": 11.6,
          "stdev": 0.16,
          "p95": 11.58
        },
        "raw_time_us": {
          "mean": 23014,
          "median": 23192,
          "min": 22596,
          "max": 23617,
          "stdev": 319,
          "p95": 23588
        },
        "derived": {
          "normalized_ratio": 1.045,
          "score": 1045.0
        }
      }
    },

    "practical": {
      "json_parse": {
        "unit": "ops/s",
        "direction": "higher_is_better",
        "library": "ArduinoJson7",
        "payload_id": "iot_doc_v1",
        "samples": 9,
        "stats": {
          "mean": 1220.9,
          "median": 1215.0,
          "min": 1191.1,
          "max": 1240.3,
          "stdev": 15.1,
          "p95": 1238.2
        },
        "derived": {
          "normalized_ratio": 1.267,
          "score": 1267.0
        }
      }
    },

    "optional": {
      "stress": null,
      "serial_throughput": null,
      "wifi_scan": null,
      "wifi_connect": null,
      "psram_seq": null,
      "nvs_rw": null,
      "boot_ready": null
    }
  }
}
```

### Co musí web validovat

Web UI by mělo před přijetím výsledku zkontrolovat alespoň:

- `schema_version` známá,
- `benchmark_profile` ∈ `quick|full|stress`,
- `official_generic_firmware == true` pro main leaderboard,
- přítomnost všech povinných core metrik pro daný `test_set_id`,
- `errors.length == 0`,
- `target_family` odpovídá firmware buildu,
- `scoring_version` a `reference_set_id` jsou na serveru známé.

To je důležité pro dlouhodobou čistotu leaderboardu.

## Návrh změn v Arduino firmware a ve web vrstvě

### Změny ve firmware

Největší hodnotu teď přinese těchto devět změn:

První změna je zavést **benchmark registry** podle capability, ne podle boardu. Každý test by měl mít metadata typu:

- `id`
- `category`
- `required_caps`
- `default_in_quick`
- `default_in_full`
- `default_in_stress`
- `unit`
- `direction`
- `implementation_id`

Tím si připravíte půdu pro C5/C6/P4 bez přepisování celé architektury.

Druhá změna je přidat **fixní read-only data blobs**:

- RAM buffer seedovaný deterministicky pro CRC/SHA/JSON/string testy,
- flash blob pro `flash_read_seq`,
- volitelně PSRAM blob při detekované PSRAM.

Na ESP32 je vhodné pro flash test použít **dedikovanou benchmark data partition**, protože partition API umí spolehlivě najít a číst data partition a partition table je pro to určený mechanismus. Na ESP8266 je praktičtější read-only blob v PROGMEM/flash, protože EEPROM/FS layout je board a mapping dependent. citeturn32search1turn30search0turn33search0turn33search1

Třetí změna je držet **software reference implementations** tam, kde by HW akcelerace nebo SDK backend měnily význam výsledku:

- `crc32_scalar_ref`
- `sha256_soft_ref`
- `float32_scalar_ref`

To neznamená, že nemůžete logovat i „real platform hash“, ale **pro main score** je lepší mít nejprve srovnatelný referenční path. Espressif totiž u SHA akcelerace výslovně upozorňuje, že hardware může být v některých situacích rychlejší a v jiných pomalejší, takže bez explicitního označení implementace by metrika byla nejasná. citeturn22search0turn22search2

Čtvrtá změna je zavést **WDT-safe chunk runner**. Na ESP8266 dokumentace doporučuje nepouštět dlouhé smyčky bez `delay()`/`yield()`, jinak nedostane Wi‑Fi/TCP stack prostor a hrozí problémy. Na ESP32 je Task Watchdog určen k detekci tasků, které příliš dlouho neběží s yieldem. Prakticky tedy spusťte každý workload v kratších úsecích, například 5–20 ms na ESP8266 a 20–50 ms na ESP32, a mezi chunkami proveďte lehký yield. Tím získáte stabilnější benchmark i méně „mysteriózních“ resetů. citeturn19search1turn18search0turn18search1

Pátá změna je udělat **jednoznačné markers pro web**:

- `ESPMARK_READY`
- `ESPMARK_BENCH_BEGIN`
- `ESPMARK_RESULT_BEGIN`
- `ESPMARK_RESULT_END`

A k nim přidat krátký header JSON s identitou běhu ještě před samotnými metrikami.

Šestá změna je přidat **heap post-state**. Na ESP32 je to přímo podpořené přes `heap_caps_get_free_size()` a `heap_caps_get_largest_free_block()`, což Espressif doporučuje i jako způsob detekce fragmentace. Na ESP8266 bych to držel jednodušeji: `free_heap_before`, `free_heap_after`, `alloc_fail_count`. citeturn36search0turn36search2

Sedmá změna je přidat **transport a capability flags** do výstupu:

- `transport`: `uart|usb_cdc|usb_serial_jtag|unknown`
- `native_usb`
- `psram_present`
- `wifi_enabled`
- `bt_enabled`

U části ESP32 boardů vede host spojení přes USB‑CDC/JTAG přímo v SoC, jinde přes externí USB‑UART bridge. To je důležité pro environment metriky a debugging, i když ne pro core score. citeturn39search0turn39search5

Osmá změna je přidat **separate benchmark partition** pro ESP32 generic firmware. Partition tables a Preferences API to umožňují čistěji než opakované zápisy do default NVS. Pokud byste někdy zkoušeli storage write benchmark, dělejte to jen mimo core a ideálně v oddělené partition. citeturn30search0turn42search1turn42search6

Devátá změna je logovat **Arduino core a SDK verze**. Arduino‑ESP32 dnes dokumentuje aktuální verzi dokumentace a supported SoCs, což je pro dlouhodobé porovnání důležitý údaj. citeturn34search2turn25search2

### Změny ve web vrstvě

Web runner musí od začátku dělat tři věci dobře.

První je **browser and security gating**. Web Serial je stále „limited availability“ a funguje jen v secure contextu. Web runner tedy musí ještě před benchmarkem zkontrolovat HTTPS a podporu `navigator.serial`; jinak nabídnout fallback instrukce. ESP Web Tools má stejnou podmínku a staví právě na Web Serial + manifestu. citeturn41search0turn41search4turn40search0

Druhá je **manifest-based flashing po chip family**. ESP Web Tools už dnes umí z manifestu vybrat build podle `chipFamily`, takže pro ESPMark je ideální mít jeden manifest s buildy pro `ESP8266`, `ESP32`, `ESP32-C3`, `ESP32-C5`, `ESP32-C6`, `ESP32-S2`, `ESP32-S3` a později `ESP32-P4`. U ESP32-family ale připravte merged bin a držte se konzervativních flash variant, protože ESP Web Tools neumí runtime patching flash parametrů jako `esptool`. citeturn40search0turn40search1

Třetí je **oddělení leaderboardů**:

- `quick` leaderboard
- `full` leaderboard
- `stress` leaderboard
- `optional/environment` leaderboard

To zlepší srozumitelnost a sníží tlak míchat nepoměřitelné runy.

## Roadmapa, rizika a otevřené otázky

### Doporučená roadmapa

#### ESPMark 0.2

V této verzi bych zamrazil základní architekturu projektu a přidal jen to, co vytvoří první opravdu použitelný benchmark:

- merged generic firmware + manifesty pro ESP8266, ESP32, C3, C6, S2, S3,
- `schema_version 1.0.0-beta`,
- Mandelbrot fixed-point,
- matrix multiply int16→int32,
- flash sequential read,
- JSON parse + generate,
- string formatting,
- Quick + Full mode,
- server-side validaci výsledků,
- první veřejný referenční set,
- leaderboard filtrování podle `benchmark_profile`, `scoring_version`, `official_generic_firmware`.

To už dá komunitě srozumitelný benchmark, aniž byste se teď zadrhli na storage write nebo Wi‑Fi environment metrikách.

#### ESPMark 0.3

Tady bych přidal praktické a volitelné vrstvy:

- heap churn + post-state,
- SHA‑256,
- optional PSRAM benchmark,
- optional NVS/EEPROM benchmark s explicitním warningem,
- Stress mode,
- stability penalty přes `p95/median`,
- host-side capture `transport/browser/webserial`,
- first-pass support pro C5 a přípravu P4.

#### ESPMark 1.0

Na `1.0` bych zamrazil srovnatelnost:

- `scoring_version 1.0.0`,
- finální váhy,
- finální reference set,
- dokumentované rules pro leaderboard,
- signed official generic firmware builds,
- explicitní compatibility policy:
  - staré výsledky se nikdy nepřepočítávají,
  - nový `scoring_version` = nový leaderboard bucket,
  - staré leaderboardy zůstávají čitelné.

P4 bych do „mainstream“ ESPMarku zařadil až tehdy, když bude vyřešen stejný generic flashing/serial flow jako u ostatních rodin a nebude potřeba board-specific zacházení. Arduino‑ESP32 už P4 jako supported SoC uvádí, takže technická cesta existuje. citeturn25search2

### Největší rizika a jak se jim vyhnout

Největší technické riziko je, že se z „generic per chip“ nenápadně stane „skrytě per board“. Typický příklad je flash mode a merged binaries u ESP Web Tools. Tomu se vyhnete tak, že budete mít buildy po **chip family** a jen tam, kde je to opravdu nutné, po **chip-family subvariantě flash/USB**, nikoli po značkové desce. citeturn40search0

Druhé riziko je, že float a crypto zaberou příliš velkou váhu a benchmark začne měřit spíš ISA/FPU/SDK detaily než „férový hobby výkon“. Tomu se vyhnete malou vahou `float32`, úplným vyřazením `double` z core a jasným označením `implementation_id` u SHA. citeturn38search0turn22search0

Třetí riziko je storage wear a „dirty partition state“. ESP32 Preferences/NVS i ESP8266 EEPROM emulace jsou skvělé pro reálné aplikace, ale špatné jako povinný veřejný benchmark. Proto nezapisovat do flash v Core Score. citeturn42search0turn42search4turn43search0

Čtvrté riziko je watchdog a nekonzistence běhů. Dlouhý benchmark bez yieldu je na ESP8266 i ESP32 zbytečný hazard. Řešení je chunk runner, warm-up, medián jako canonical a malá stability penalizace místo agresivního filtrování outlierů. citeturn19search1turn18search0

Páté riziko je promíchání environment metrik do hlavního skóre. Wi‑Fi scan/connect, boot time a Web Serial throughput budou komunitu lákat, protože jsou „cool“, ale pro hlavní leaderboard jsou toxické. Udržte je v separátní sekci, a naopak v UI je prezentujte jako zajímavé doplňky, ne jako součást `ESPMark Core Score`. citeturn26search1turn27search0turn41search0turn39search5

### Otevřené otázky a limity

Několik bodů je rozumné nechat otevřených do prvních public beta běhů. První je, zda budete chtít u ESP32-family držet jednu skutečně univerzální flash variantu, nebo dvě konzervativní varianty podle typu flash/USB cesty; dokumentace ESP Web Tools ukazuje, že u merged bin je to praktické téma. citeturn40search0

Druhá otevřená věc je komprese/dekomprese. Technicky je to zajímavá practical metrika, ale až po výběru jedné pevné embedded-friendly knihovny a parametrů. Do `1.0` bych ji nedával. citeturn21search1turn21search0

Třetí je přesná role P4 v prvním veřejném leaderboardu. Cesta je připravená, ale projekt podle vašeho zadání dává větší smysl nejdřív stabilizovat na ESP8266 + běžné ESP32 Wi‑Fi rodině a teprve potom rozšířit záběr. citeturn25search2

Celkově tedy doporučuji: **udělat z ESPMarku nejdřív malý, přísně deterministický a dobře verzovaný benchmark**, a teprve až potom přidávat atraktivní, ale méně férové doplňky. To je nejkratší cesta k leaderboardu, kterému bude komunita věřit.