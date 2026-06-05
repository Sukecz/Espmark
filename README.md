# Espmark

Espmark je komunitni benchmark pro ESP8266 a ESP32-family desky. Cilem je, aby
bezny uzivatel mohl desku pripojit pres USB, spustit test z webu, ziskat
srozumitelny vysledek a publikovat ho do verejne tabulky.

Aktualni testovaci web:

```text
https://espmark.msmeteo.cz
```

Tato instance je vyvojova a verejne vysledky v ni jsou testovaci. Az bude
projekt pripraveny pro domenu 1. radu, databaze vysledku se resetuje a zacne
novy leaderboard.

## Strategicke Rozhodnuti

Espmark bude drzet oficionalni skore oddelene od firmware vystupu:

- firmware posila pouze raw metriky, metadata zarizeni, verze a validacni
  informace,
- web/backend z raw metrik vytvori leaderboard submission objekt,
- oficialni skore pocita backend podle zname `scoring_version`,
- raw metriky se ukladaji trvale, aby slo v budoucnu delat audit, migrace,
  rescore a nove leaderboard buckety bez ztraty puvodnich dat.

To je hlavni architektonicky smer projektu. Firmware nema byt zdrojem pravdy pro
oficialni skore; firmware je merici agent. Backend je zdroj pravdy pro validaci,
verzovani, vypocet skore a spravu vysledku.

## Benchmark Scope

Oficialni headline leaderboard bude pouzivat pouze deterministicky Core
benchmark:

- bez Wi-Fi,
- bez Bluetooth,
- bez board-specific periferii,
- bez zapisu do flash,
- bez mereni browseru, USB bridge nebo host PC,
- s generic firmwarem podle chip family, ne podle konkretni znacky desky.

Metriky zavisle na prostredi budou oddelene jako Optional/Environment metriky.
Mohou byt zajimave v detailu vysledku, ale nesmi vstupovat do `Espmark Score`.

## Rezimy Testu

| Rezim | Ucel | Leaderboard |
|---|---|---|
| `quick` | rychle overeni, onboarding, sdileni nahledu | samostatny preview bucket |
| `full` | oficialni verejny benchmark | hlavni leaderboard |
| `stress` | stabilita, drift, throttling, napajeni | samostatny stability leaderboard |

Hlavni tabulka bude radit pouze `full` vysledky se stejnym `scoring_version`,
`benchmark_profile`, `test_set_id` a s `official_generic_firmware = true`.

## Espmark Score Plan

`Espmark Score` je hlavni brandovane skore projektu. Backend ho pocita z raw
firmware metrik podle zverejnene `scoring_version`.

Kategorie:

| Skore | Co obsahuje | Vaha do `Espmark Score` |
|---|---|---:|
| `CPU Score` | integer micro-suite, sustained mix, float32, deterministicky compute | 40 % |
| `Memory Score` | RAM bandwidth, heap churn, heap fragmentation | 30 % |
| `Flash Score` | read-only flash sequential read | 10 % |
| `Practical IoT Score` | SHA-256, CRC32, JSON roundtrip, string formatting | 20 % |
| `Optional/Environment Score` | PSRAM, Web Serial throughput, boot time, Wi-Fi scan/connect | 0 % |

Subvahy:

| Subkategorie | Vaha do `Espmark Score` |
|---|---:|
| CPU integer micro-suite | 18 % |
| CPU sustained stress | 8 % |
| CPU float/math | 6 % |
| Deterministicky compute test | 8 % |
| Memory/RAM bandwidth | 20 % |
| Heap alloc/free churn | 6 % |
| Heap fragmentation | 4 % |
| Flash read | 10 % |
| SHA-256 software | 7 % |
| CRC32 software | 3 % |
| JSON parse/generate | 7 % |
| String formatting | 3 % |

Stability modifier je samostatna mala penalizace, maximalne -5 %. Stability
neni dalsi velka kategorie, protoze hlavni score ma merit vykon, ne michat
vykon a kvalitu napajeni do jednoho neprehledneho cisla.

## Scoring Rules

Zakladni pravidla vypoctu:

- kazdy test ma jeden warm-up run mimo statistiku,
- `quick` pouziva mene vzorku, `full` minimalne 9 merenych vzorku,
- kanonicka raw hodnota jednoho testu je median,
- `mean`, `min`, `max`, `stdev` a `p95` se ukladaji pro audit a UI,
- raw cas nad fixnim workloadem je preferovana kanonicka metrika,
- throughput typu `ops/s` nebo `MB/s` se uklada jako odvozena uzivatelska
  hodnota,
- jednotlive metriky se normalizuji proti zmrazene referencni sade,
- kategorie i celkove skore se pocitaji vazenym geometrickym prumerem,
- reference, vahy, clamp a penalizace jsou soucasti `scoring_version`,
- live leaderboard se nikdy nepouziva jako reference pro scoring.

Reference pro `scoring_version 1.0.0` bude zverejnena jako `reference_set_id`.
Prakticky plan je pouzit median z medianu nekolika kusu bezne dostupneho
generic ESP32 modulu bez PSRAM.

## Versioning Model

Espmark musi verzovat vrstvy oddelene:

| Pole | Co znamena |
|---|---|
| `schema_version` | tvar firmware raw JSON |
| `submission_schema_version` | tvar backend submission objektu |
| `firmware_version` | konkretni firmware release |
| `benchmark_profile` | sada testu a runtime profil |
| `test_set_id` | povinne testy pro dany leaderboard |
| `scoring_version` | reference, vahy, matematika a penalizace |
| `reference_set_id` | zmrazena referencni sada |
| `official_generic_firmware` | zda byl pouzit oficialni generic build |
| `target_family` | `ESP8266`, `ESP32`, `ESP32-C3`, `ESP32-S3` atd. |

Vysledky jsou primo porovnatelne jen ve stejnem leaderboard bucketu:

```text
mode + benchmark_profile + test_set_id + scoring_version
```

Family leaderboard navic filtruje podle `target_family`.

## Result Data Architecture

Backend bude ukladat dve oddelene vrstvy dat.

Firmware raw result:

- vystup mezi `ESPMARK_RESULT_BEGIN` a `ESPMARK_RESULT_END`,
- metadata zarizeni a buildu,
- raw samples/statistiky,
- checksumy a known-answer validace,
- chyby a warningy,
- bez oficialniho vypoctu skore jako zdroje pravdy.

Leaderboard submission:

- serverem pridelene `result_id`,
- cas prijeti,
- uzivatelsky popisek desky,
- transport/browser metadata,
- validacni stav,
- vypoctene score podle `scoring_version`,
- normalizacni metadata,
- moderation/publication stav.

Minimalni spravovatelny databazovy model:

| Tabulka | Ucel |
|---|---|
| `submissions` | prijate vysledky, publikacni stav, uzivatel, board label |
| `raw_results` | puvodni firmware JSON beze ztraty detailu |
| `computed_scores` | vypoctena skore pro konkretni `scoring_version` |
| `scoring_versions` | vahy, reference, pravidla a aktivni stav |
| `reference_sets` | referencni hodnoty pro metriky |
| `firmware_builds` | oficialni buildy, hash, target family, manifest vazba |
| `board_aliases` | spravovane nazvy desek a mapovani uzivatelskych popisku |
| `moderation_log` | skryti, schvaleni, podezrele vysledky, admin poznamky |

V testovaci fazi muze backend zustat jednoduchy, ale API a uloziste musi byt
navrzene tak, aby pozdeji slo prejit z JSON souboru na SQLite/PostgreSQL bez
zmeny firmware formatu.

## Result Tab Plan

Result tab na webu ma byt hlavni misto, kde uzivatel vysledek pochopi,
zkontroluje a publikuje.

Musi obsahovat:

- prehled `Espmark Score` a dilcich kategorii,
- jasne oznaceni `Preview`, `Official`, `Invalid` nebo `Unverified`,
- raw metriky v rozbalitelnem detailu,
- informace o desce, chip family, firmware verzi a scoring verzi,
- porovnani proti referenci a proti medianu stejne family,
- oddelene Optional/Environment metriky,
- publikacni formular s board labelem a souhlasem se zverejnenim,
- share URL po ulozeni,
- validacni hlasky pred odeslanim,
- upozorneni, pokud vysledek neni vhodny pro hlavni leaderboard.

Publikovany vysledek nesmi byt jen lokalni browser state. Web musi poslat
submission na backend a backend musi vratit canonical `result_id`.

## Firmware Plan

Firmware bude postupne upraven tak, aby mel:

- benchmark registry podle capability, ne podle boardu,
- `quick`, `full` a `stress` mode,
- WDT-safe chunk runner,
- fixed RAM buffers a read-only flash blob,
- software-only referencni implementace pro CRC32 a SHA-256 v Core,
- fixed-buffer string formatting bez `String` concat,
- JSON workload buffer-to-buffer,
- known-answer/checksum validaci kazde metriky,
- metadata o chipu, flashi, PSRAM, Arduino core, SDK a transportu,
- stabilni serial markers:
  - `ESPMARK_READY`
  - `ESPMARK_BENCH_BEGIN`
  - `ESPMARK_RESULT_BEGIN`
  - `ESPMARK_RESULT_END`

## Web And Backend Plan

Web zustava hlavni uzivatelska cesta:

1. uzivatel otevre `https://espmark.msmeteo.cz`,
2. pripoji desku pres Web Serial,
3. pripadne nahraje generic firmware pres ESP Web Tools,
4. spusti benchmark,
5. web zachyti raw JSON,
6. backend vysledek zvaliduje a spocita,
7. uzivatel vysledek publikuje do leaderboardu.

Backend musi umet:

- prijmout raw firmware result,
- validovat schema a povinne metriky,
- overit kompatibilitu `mode`, `benchmark_profile`, `test_set_id` a
  `scoring_version`,
- spocitat score server-side,
- ulozit raw i computed vrstvy,
- vratit canonical submission,
- filtrovat leaderboardy,
- skryt nebo oznacit podezrele vysledky,
- pozdeji prepocitat raw vysledky pro novou `scoring_version`.

## Deployment

Web je servirovan z RPI:

```text
/home/msrpi/espmark
```

Docker compose service na RPI:

```text
espmark-web
```

Aktualni testovaci uloziste vysledku:

```text
/home/msrpi/docker_data/espmark/espmark.sqlite3
```

Public HTTPS URL:

```text
https://espmark.msmeteo.cz
```

LAN URL:

```text
http://192.168.1.4:8095/
```

Nasazeni webu na RPI se dela pres helper:

```bash
scripts/deploy_web.sh
```

Helper pred syncem zkontroluje `web/app.js`, `web/server.py`, JSON manifest,
board katalog a existenci firmware souboru. Pak synchronizuje `web/` do
`/home/msrpi/espmark`, restartuje pouze Docker compose service `espmark-web`
a overi lokalni i verejne endpointy.

Poznamky k deployi:

- `scripts/deploy_web.sh` ma byt preferovany postup misto rucniho `rsync`.
- Zmeny webu se v tomhle projektu nahravaji rovnou na RPI. Deploy helper ma
  povoleno restartovat `espmark-web` jako soucast nasazeni a pouziva jen tento
  uzky restart.
- Python syntax check v helperu kompiluje do docasneho souboru, aby nevznikal
  lokalni `web/__pycache__`.
- Po restartu muze prvni health-check kratce narazit na zavirajici se socket;
  helper tyto prechodne pokusy potichu opakuje a selze az kdyz sluzba
  nenabehne v limitu.
- Pri zmene `web/styles.css` nebo `web/app.js` bumpni query verze v
  `web/index.html` i `web/results.html`, aby verejny web nebral stare assety z
  cache.
- Na MINIPC je dostupny `chromium`, takze pro budoucí vizualni kontrolu lze
  pouzit headless Chromium/Playwright, pokud je v projektu nebo prostredi
  nainstalovany odpovidajici test runner.

Bug report flow:

- Bug report je modal otevirany z horni navigace.
- Backend uklada reporty do SQLite a chrani formular honeypot polem i stejnou
  serverovou matematickou challenge jako publikovani vysledku.

## Roadmap

Roadmapa neni delena na umele faze. Projekt se ma posouvat rovnou k cilove
architekture, jen s tim, ze dokud neni scoring finalne zmrazeny, web vysledky
oznacuje jako preview.

Implementacni plan:

- zafixovat raw-vs-submission architekturu,
- prejit z testovaciho `results.json` na spravovatelnou databazi,
- zavest `schema_version`, `submission_schema_version`, `benchmark_profile`,
  `test_set_id`, `scoring_version` a `reference_set_id`,
- pocitat official score server-side,
- ukladat raw firmware output a computed scores oddelene,
- upravit Result tab pro publikaci, validaci, detail raw metrik a share ID,
- filtrovat leaderboard podle kompatibilnich verzi,
- pridat Quick, Full a Stress mode,
- pridat Mandelbrot fixed-point,
- pridat matrix multiply int16->int32,
- pridat float32 affine/vector transform s nizkou vahou,
- pridat flash sequential read bez zapisu do flash,
- pridat JSON parse + generate,
- pridat string formatting bez `String` concat,
- pridat heap churn + post-state,
- pridat CRC32 software reference,
- pridat SHA-256 software reference,
- pridat stability modifier pres `p95 / median`,
- pridat Optional/Environment sekci mimo headline score,
- pripravit PSRAM optional benchmark,
- pridat spravu board aliasu a zakladni moderation stav,
- publikovat finalni reference set,
- publikovat finalni leaderboard pravidla,
- podepisovat nebo hashovat official generic firmware buildy,
- pripravit family leaderboardy,
- zamknout compatibility policy,
- resetovat testovaci databazi a presunout projekt na finalni verejnou domenu.

## Project Layout

```text
arduino/espmark/espmark.ino      benchmark firmware source
web/                             web UI and lightweight backend
web/assets/espmarklogo_light.png logo used by the web UI in light mode
web/assets/espmarklogo_dark.png  logo used by the web UI in dark mode
web/assets/favicon.png           public favicon
web/boards.json                  generated board catalog for exact-board selection
web/firmware/manifest.json       ESP Web Tools manifest placeholder
docs/                            release and project notes
scripts/                         build, catalog and deploy helpers
```

## Updating Firmware Code

When editing `arduino/espmark/espmark.ino`:

1. Keep serial commands stable where possible.
2. Keep JSON markers stable.
3. Bump `ESPMARK_VERSION`.
4. Build all web flasher binaries and regenerate the manifest:

```bash
scripts/build_firmware.sh
```

5. If JSON fields change, update web parsing, validation and scoring.
6. Deploy and verify:

```bash
scripts/deploy_web.sh
```

7. Re-test through `https://espmark.msmeteo.cz`.

See `docs/release.md` for the full release procedure.

## Updating The Web

Edit files under `web/`.

Local syntax check:

```bash
node --check web/app.js
python3 -m py_compile web/server.py
```

Deploy to RPI:

```bash
scripts/deploy_web.sh
```

Verify from RPI:

```bash
ssh rpi5 'curl -fsS http://127.0.0.1:8095/ | head'
ssh rpi5 'docker ps --filter name=espmark-web --format "{{.Names}}\t{{.Status}}"'
```

## License

Apache-2.0. See `LICENSE`.
