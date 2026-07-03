# C3SAT-OBC — Hardveres összeépítés és tesztelés (breadboard)

Ez az útmutató végigvezet a fizikai összeépítésen, a flashelésen, és pontosan
megmondja, **mit kell látnod**, hogy tudd: tényleg az történik, amit a firmware
csinál. A fokozatos (etapos) megközelítés a lényeg — előbb csupaszon, majd
alkatrészenként bővítve.

> **Aranyszabály:** a firmware minden szenzor nélkül is teljesen működik
> (szimulált adatok). Ezért **először kösd be CSAK a panelt + kijelzőt**,
> ellenőrizd, hogy fut, és csak utána add hozzá a szenzorokat egyesével. Így
> mindig tudni fogod, melyik bekötés okozott hibát.

---

## 0. Amire szükséged lesz

**Kötelező:**
- ESP32-C6-DevKitC-1-N8
- Breadboard + jumper drótok (lehetőleg dupont F-M és M-M)
- USB-C kábel (a DevKit „USB" portjához — ezen megy a konzol/log + flash)

**Erősen ajánlott (a grafikus rész miatt):**
- ILI9341 vezérlős TFT kijelző SPI módban (lásd a 4.2 pont fontos megjegyzését)

**Opcionális (a szimuláció helyett valódi adat + kézi hibainjektálás):**
- INA219 modul (EPS — tápfeszültség/áram)
- MPU6050 / GY-521 modul (ADCS — IMU)
- BME280 modul (Thermal — hőmérséklet/nyomás)
- DS3231 modul (RTC — küldetésóra)
- 10 kΩ potméter (kézi „napelem-feszültség" az ADC-re)
- 2 db 4,7 kΩ ellenállás (I2C felhúzók, ha a modulokon nincs)
- Külön USB-UART adapter (a földi állomáshoz — vagy használd a DevKit 2. portját)

**Műszer:** egy multiméter (folytonosság/feszültség méréshez) sokat segít a
bekötés ellenőrzésében bekapcsolás előtt.

---

## 1. A két USB-C port — fontos!

Az ESP32-C6-DevKitC-1 **két** USB-C csatlakozóval rendelkezik:

| Felirat | Mire való | Itt mi megy rajta |
|---------|-----------|-------------------|
| **USB** | Natív USB-Serial-JTAG (GPIO12/13) | **Konzol/log + flash** (ide nézz a kimenetért) |
| **UART**| Onboard USB↔UART híd (GPIO16/17) | A firmware **földi rádió-linkje** (UART1) |

A firmware a logokat a beépített USB-Serial-JTAG-en adja ki (lásd
`sdkconfig.defaults`: `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y`), tehát a flasheléshez
és a `idf.py monitor`-hoz az **USB** portot használd.

A telekomand/telemetria link az **UART1**-en megy a GPIO16 (TX) / GPIO17 (RX)
lábakon — és pont ezek a DevKit „UART" portjának lábai. Ezért a **földi
állomáshoz külön adapter nélkül** is csatlakozhatsz: dugd be a második USB-C-t a
„UART" portba, és a `groundstation.py`-t arra a soros portra indítsd.

---

## 2. Tápsínek a breadboardon

1. A DevKit **3V3** lábát kösd a breadboard **+ (piros)** sínjére.
2. A DevKit **GND** lábát kösd a breadboard **– (kék)** sínjére.
3. Minden modul VCC-je a + sínről, GND-je a – sínről kap tápot.

> A kijelző és a szenzorok **3,3 V logikájúak**. Ne adj 5 V-ot a jelvonalakra!
> A DevKit 3V3 LDO-ja bőven elég áramot ad a kijelző háttérvilágításának is.

---

## 3. Teljes bekötési táblázat

A pinkiosztás egyetlen forrása: `components/bsp/include/bsp_pins.h`. Ha máshova
kötöd, ott írd át.

### Kijelző — CSAK SPI (a DB0..DB17 párhuzamos buszt NEM használjuk)

> **FONTOS — a TFT Proto tábla feliratai mások, mint a „nyers" ILI9341 nevek!**
> A táblán **NINCS „SCLK"/„SCK" nevű láb.** A soros órajel a **`WR`** lábon van
> (a silkscreen-en `WR` felülvonással, mellette kis `SCL` felirat). Az alábbi
> jobb oszlop a **tábla tényleges feliratait** mutatja, ahogy a 2x20 fejlécen
> megtalálod.

| ESP32-C6 GPIO | Funkció | TFT Proto fejléc felirata |
|---|---|---|
| 6  | SPI órajel (SCLK) | **`WR`** (= SCL — NEM „SCLK"!) |
| 7  | SPI adat ki (MOSI) | **`SDI`** |
| —  | SPI adat be (MISO) | **`SDO`** — **bekötetlen** (a GPIO2-t a touch kapja) |
| 10 | LCD CS | **`CS`** |
| 11 | LCD D/C | **`RS`** |
| 18 | LCD RESET | **`RST`** |
| 19 | Háttérvilágítás (+) | **`LED-A`** (soros R-en/tranzisztoron át) |
| GND | Háttérvilágítás (–) | **`LED-K`** |
| 3V3 | táp | **`3.3V`** |
| GND | föld | **`GND`** |

> **Miért nincs MISO?** Ez a firmware sosem olvas vissza a panelről, a `SDO`
> csak regiszter-visszaolvasáshoz kellene. Az ESP32-C6-DevKitC-1-en viszont
> kevés a szabad, nem-strapping, nem-USB láb (pl. a **GPIO14 nincs is kivezetve**
> a headerére — a belső flash-buszé). Ezért **két** lépéssel spórolunk lábat:
> (1) a **`SDO`-t bekötetlenül hagyjuk**, így felszabadul a **GPIO2** (ADC-képes,
> ideális a touch-hoz); (2) az **`IM1` és `IM2` közös GPIO-ra** megy (mindkettő
> ugyanaz a logikai 1), így egy láb elég kettő helyett — ez adja a touch `Y-`-nak
> a GPIO21-et. A kép ettől bitre ugyanúgy rajzolódik.

#### Interfész-mód strappelés — KÖTELEZŐ a SPI-hoz ⚠️ (most GPIO hajtja)

A panel alapból a párhuzamos buszra van állítva. SPI-hoz az **`IM0..IM3`** lábakat
kell „4-vezetékes 8-bites soros I" módra állítani (ILI9341 = `IM[3:0] = 0b0110`).
Mivel a DevKiten **csak egy 3V3 láb van** (kell a panel/szenzor VCC-hez), az
`IM1`/`IM2`-t **nem** kötjük 3V3 sínre, hanem **egy GPIO tartja folyamatosan
magasban** az egész futás alatt (a firmware `bsp_display_straps_high()`-ban
állítja be, még a panel reset előtt, így a strap érvényes, amikor az ILI9341
belövi az interfész-módot). Mivel `IM1` és `IM2` **ugyanaz a logikai 1**, a panel
mindkét lábát **ugyanarra a GPIO20-ra** kötöd (egy kimenet két nagyimpedanciás
strap-bemenetet elbír) — így egy láb megmarad a touch `Y-`-nak. `IM0`/`IM3`
GND-n marad (GND láb van bőven).

| TFT Proto láb | Hova kösd | Bit |
|---|---|---|
| **`IM0`** | **GND** | 0 |
| **`IM1`** | **GPIO20** (firmware tartja HIGH) | 1 |
| **`IM2`** | **GPIO20** (ugyanaz a láb, mint IM1) | 1 |
| **`IM3`** | **GND** | 0 |

Strappelés nélkül a kijelző a párhuzamos buszon marad, és a SPI vonalak **nem
csinálnak semmit** (fehér/üres kép).

#### Érintőképernyő (4-vezetékes rezisztív — MOST MŰKÖDIK) 👆

A panel **4-vezetékes rezisztív** érintőfelülettel van fedve; a nyers analóg
vonalak (**`X+`, `X-`, `Y+`, `Y-`**) közvetlenül a fejlécre jönnek ki, **nincs a
táblán érintővezérlő IC** (nem kell XPT2046). A firmware közvetlenül olvassa az
**ESP32-C6 ADC-jével + GPIO-átkapcsolással**: egy tengely olvasásához az adott
réteg két szélét digitális kimenetként meghajtja (egyik HIGH, másik LOW), és a
szemközti réteget ADC-vel mintavételezi. Ezért 3 vonal ADC-képes lábon ül.

| ESP32-C6 GPIO | Touch vonal | ADC | Szerep |
|---|---|---|---|
| 1  | **`X+`** | ADC1_CH1 | analóg mintavétel |
| 2  | **`Y+`** | ADC1_CH2 | analóg mintavétel (ez a volt-MISO láb) |
| 3  | **`X-`** | ADC1_CH3 | analóg mintavétel + meghajtás |
| 21 | **`Y-`** | —        | csak digitális meghajtás (az IM-megosztás szabadította fel) |

> A leképezés (nyers ADC → képpont) a `components/drivers/touch.h`
> `TOUCH_CAL_*` konstansaiban van; panel-/bekötésfüggő, **kalibráld a saját
> hardveredre** (lásd a 10. pont hibakeresését). Ha nincs touch panel bekötve,
> a nyomás-kapu (pull-down) miatt a rendszer nyugton marad — nem lesz fantom
> érintés. Touch panel nélkül a menü a `TOUCH_SIMULATE 1` kapcsolóval mutatható
> be (a touch task ilyenkor szkriptelt koppintásokkal végigjárja a menüt).

### Szenzorok — közös I2C busz

| ESP32-C6 GPIO | Jel | Eszközök (7-bites cím) |
|---|---|---|
| 22 | SDA | INA219 `0x40`, MPU6050 `0x69`, BME280 `0x76`, DS3231 `0x68` |
| 23 | SCL | (4,7 kΩ felhúzó 3V3-ra, ha a moduljaidon nincs) |

### Potméter (kézi hibainjektálás)

| Potméter láb | Hova |
|---|---|
| 1. szélső | 3V3 |
| középső (nyák) | GPIO0 (ADC) |
| 2. szélső | GND |

### Földi link — UART1 (a DevKit „UART" portja kezeli)

| ESP32-C6 GPIO | Jel |
|---|---|
| 16 | TX (a hídnak) |
| 17 | RX (a hídtól) |

> Az alábbi lábakat szándékosan **nem** használjuk boot-kritikus bekötéshez:
> GPIO4/5/8/9/15 (strapping), GPIO12/13 (USB). A GPIO8 a beépített RGB LED
> (csak reseten mintavételezett, kimenetként utána ártalmatlan) — ezt
> mód-jelzésre használjuk. A **GPIO4/5** teljesen üresen marad.
>
> **Foglalt lábak összegzése:** kijelző 6,7,10,11,18,19 + IM 20 (IM1+IM2 közös);
> touch 1,2,3,21; I2C 22,23; UART 16,17; RGB LED 8; napelem-ADC 0.
> (A **GPIO14 nincs kivezetve** a DevKitC-1-en, ezért nem használjuk.)

---

## 4. Lépésről lépésre huzalozás

Mindig **áramtalanítva** (USB kihúzva) köss!

### 4.1 Tápsínek
Kösd be a 3V3 → + sín és GND → – sín vezetékeket (2. pont).

### 4.2 Kijelző (SPI) — a legkényesebb lépés ⚠️

A te paneled konkrétan a **MikroE „TFT Proto" (MIKROE-495)**, rajta a 2,83"
**MI0283QT-9A** panellel, amit az **ILI9341** vezérlő hajt. A 2x20-as fejléc
feliratai a tábla silkscreenjén olvashatók — **figyelj, mert ezek mások, mint a
nyers ILI9341 lábnevek** (ezért nem találtál „SCLK"-t!).

**Két lépés kell:**

**1) Kösd az `IM0..IM3` strap-lábakat soros módra:**
`IM0→GND, IM3→GND`, majd **`IM1` és `IM2` EGYÜTT a `GPIO20`-ra** (a firmware
tartja folyamatosan HIGH-on — így nem kell a második 3V3, amiből nincs is, és
egy láb megmarad a touch `Y-`-nak). Ez kapcsolja a vezérlőt 4-vezetékes soros
(SPI) módba. Enélkül a SPI vonalak hatástalanok.

**2) Köss a soros jeleket** a tábla feliratai szerint a fenti GPIO-kra:

| TFT Proto felirat | ESP32-C6 GPIO |
|---|---|
| **`WR`** (= a Soros órajel!) | 6 |
| **`SDI`** | 7 |
| **`SDO`** | — (**hagyd bekötetlenül**, a GPIO2 a touché) |
| **`RS`** | 11 |
| **`CS`** | 10 |
| **`RST`** | 18 |
| **`3.3V`** / **`GND`** | 3V3 / GND |

**3) Köss az érintő 4 vonalát** (ha használni akarod a menüt):

| TFT Proto felirat | ESP32-C6 GPIO |
|---|---|
| **`X+`** | 1 |
| **`Y+`** | 2 |
| **`X-`** | 3 |
| **`Y-`** | 21 |

> A leggyakoribb hiba itt: az órajelet az `SDI`/`SDO` közelében keresni. A táblán
> **a `WR` láb a soros órajel** — kösd azt a GPIO6-ra. A `SDO`-t most **nem**
> kötjük be (a GPIO2 a touch `Y+`-a lett). Az `IM1`/`IM2` immár **közösen a
> GPIO20-ra** megy (nem 3V3-ra), a felszabaduló GPIO21 pedig a touch `Y-`-a.

**Háttérvilágítás (`LED-A` / `LED-K`):** a panelnek külön anód (`LED-A`) és katód
(`LED-K`) háttérvilágítás-lába van. Köss **`LED-K`→GND**, és **`LED-A`→3V3 egy kis
soros ellenálláson (~22–33 Ω) át** (mindig világít). Ha a firmware vezérelte
ki/be kapcsolás kell, akkor `LED-A`-t egy NPN tranzisztoron át hajtsd a GPIO19-ről
— ne kösd a 4 LED-et közvetlenül egy GPIO-ra (túl nagy áram). Bench-teszthez a
„`LED-A`→3V3 soros R-en át" a legegyszerűbb, a GPIO19-et hagyd szabadon.

> **Alternatíva (ha az IM strappelés körülményes):** breadboardon kényelmesebb
> lehet egy olcsó, **általános ILI9341 SPI modul** (9 tüske: VCC GND CS RESET DC
> SDI SCK LED SDO), amin a SPI mód gyárilag be van állítva. A firmware **bitre
> ugyanaz** marad. De a te TFT Proto táblád is tökéletesen jó a fenti
> strappeléssel.

### 4.3 I2C szenzorok

- Mindegyik VCC → 3V3, GND → GND, SDA → GPIO22, SCL → GPIO23 (párhuzamosan
  ugyanarra a két jelvonalra köthető mind a négy modul).
- **Felhúzók:** a legtöbb breakout-on már van SDA/SCL felhúzó. Ha nincs, tegyél
  egy-egy 4,7 kΩ-t SDA→3V3 és SCL→3V3 közé.
- **⚠️ MPU6050 címütközés:** a GY-521 alapból `0x68`, ami ütközik a DS3231-gyel.
  Kösd az MPU6050 **AD0 lábát 3V3-ra** → így `0x69` lesz (a kód ezt várja). Ha
  nincs DS3231-ed, és inkább `0x68`-on hagynád, írd át a `bsp_pins.h`-ban a
  `BSP_I2C_ADDR_MPU6050`-t `0x68`-ra.
- **BME280:** SDO→GND = `0x76` (ezt várja a kód). Ha SDO→VCC, akkor `0x77`.

### 4.4 Potméter (opcionális, de látványos)
Kösd a 3. pont szerint: szélső lábak 3V3 és GND, középső láb GPIO0. Ezzel
**kézzel állítod a szimulált tápfeszültséget**, és kiváltod az alulfeszültség-
hibát (lásd Etap E). Csak akkor aktív, ha **nincs valódi INA219** bekötve.

### 4.5 Földi link
Vagy a DevKit „UART" portját használod (semmi extra bekötés, csak a 2. USB
kábel), vagy egy külső USB-UART adaptert: adapter RX → GPIO16, adapter TX →
GPIO17, GND → GND. **Ne** használd egyszerre mindkettőt.

---

## 5. Bekapcsolás előtti ellenőrzés (multiméter)

USB nélkül, folytonosság-méréssel:
1. 3V3 sín és GND sín **NINCS rövidzárban** (ez a leggyakoribb hiba!).
2. Minden modul VCC-je a 3V3-on, GND-je a GND-n van.
3. SDA (22) és SCL (23) nem ér GND-hez vagy 3V3-hoz közvetlenül (csak a
   felhúzókon át).
4. A kijelző CS/DC/RST/SCLK/MOSI a megfelelő GPIO-kon.

Csak ha ez rendben, dugd be az USB-t.

---

## 6. Fordítás és flashelés

```bash
cd C3SAT-OBC
idf.py set-target esp32c6        # csak első alkalommal
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

(A port Linuxon tipikusan `/dev/ttyACM0` az USB-Serial-JTAG-hez; CLion-ban az
ESP-IDF műveletekkel ugyanez megy.)

A `monitor`-ból `Ctrl+]` -dal lépsz ki.

---

## 7. Mit kell látnod induláskor

### A) Beépített RGB LED (GPIO8) — mód szerinti szín
- **fehér** = BOOT (épp indul)
- **kék** = SAFE
- **zöld** = NOMINAL (egészséges, minden alrendszer megy)
- **cián** = PAYLOAD
- **piros** = FAULT

Tipikus menet bekapcsolás után: fehér → (pár másodperc) → kék → zöld.

### B) Konzol (a `monitor`-ban) — kulcs sorok
Ezeket a sorokat keresd (a `(...)` az időbélyeg):

```
I (...) main: === C3SAT-OBC boot ===
I (...) bsp: boot #1
I (...) hal_i2c: I2C0 up on SDA=22 SCL=23 @ 400000 Hz
I (...) hal_spi: SPI bus up: SCLK=6 MOSI=7 MISO=2
I (...) ili9341: panel up 320x240
I (...) ina219: not found @0x40 -> simulated EPS      <- ha nincs szenzor
I (...) mpu6050: not found -> simulated ADCS
I (...) bme280: not found -> simulated thermal
I (...) ds3231: no RTC -> esp_timer clock
I (...) storage: mass memory mounted: 0/... KB used
I (...) main: all subsystems ready -> leaving BOOT     <- a startup barrier átment!
I (...) cdh: beacon: mode=2 V=3.95 SoC=79% T=21.5C rate=2.1 f=0x00
```

- A **„all subsystems ready -> leaving BOOT"** azt jelenti, hogy az event-group
  startup barrier rendben lezárult (minden task elérte a futási ciklusát).
- A **beacon** sor **5 másodpercenként** ismétlődik. `mode=2` = NOMINAL,
  `f=0x00` = nincs hiba.
- Kb. **10 másodpercenként** egy FDIR egészség-sor: `tasks=.. free-heap=..`.

### C) Kijelző — a dashboard
Sötét háttér, sárga **C3SAT-OBC** fejléc, középen a **mód-szalag** (NOMINAL =
zöld), a **jobb felső sarokban a `MENU` gomb**. Panelek: **EPS POWER** (VBUS,
IBUS, SoC sáv), **ADCS ATTITUDE** (RATE, RPY, DETUMBLED/TUMBLING), **THERMAL**
(T, P, HEATER), **FAULTS** (ALL NOMINAL zölden), és alul az **EVENT LOG** (görgő
üzenetek, pl. „BOOT complete", „MODE BOOT->SAFE").

Ha mindezt látod, az alaprendszer **bizonyítottan fut**. Az érintőmenü
használatát lásd a **7.5 pontban**.

---

## 7.5 Az érintőképernyős menü — mikor mit látsz, és hogyan használd

A kijelző egy **nézet-állapotgép** (view state machine). Minden nézet tetején
ugyanaz a fejléc: bal oldalt a cím, jobb felül egy gomb, ami a **dashboardon
`MENU`**, minden más nézetben **`BACK`**. Egy koppintás mindig a gomb közepére a
legbiztosabb.

**Hogyan működik belül (FreeRTOS):** egy külön **`touch` task** 50 Hz-en
mintavételezi az érintőt, pergésmentesít, a nyers ADC-t képpontra konvertálja,
és **koppintásonként egy üzenetet tesz egy FreeRTOS queue-ba**. A `gui` task ezt
a queue-t üríti minden képkockában, és a koppintást a nézet gombjaira illeszti.
Így a (időzítésre érzékeny) beolvasás és a rajzolás **szét van választva** — ez
a queue-alapú, task-ok közti kommunikáció iskolapéldája (ezért van értelme itt
külön tasknak, nem a GUI-ból pollozunk).

### A nézetek

| Nézet | Hogyan jutsz ide | Mit látsz |
|---|---|---|
| **DASHBOARD** | induláskor ez az alap; bármely nézetből `BACK` → `MENU` → `BACK` | a teljes műszerfal (7/C), jobb felül `MENU` |
| **MENU** | dashboard `MENU` gomb | 5 nagy gomb: `SENSORS`, `MODE CTRL`, `EVENT LOG`, `ABOUT`, `BACKLIGHT ON/OFF` |
| **SENSORS** | MENU → `SENSORS` | I2C busz-állapot (INA219/MPU6050/BMx280/DS3231 = **ONLINE** zölden vagy **SIMULATED** narancs) + élő értékek a blackboardról |
| **MODE CTRL** | MENU → `MODE CTRL` | 3 gomb: `SAFE` / `NOMINAL` / `PAYLOAD`; alul „NOW: <mód>”, az aktív gomb kiemelve |
| **EVENT LOG** | MENU → `EVENT LOG` | teljes képernyős, görgő eseménynapló (14 sor) |
| **ABOUT** | MENU → `ABOUT` | projekt-infó: boot-szám, uptime, szabad heap, a C3S link |

### Navigáció lépésről lépésre (amit látnod kell)

1. **Bekapcsolás után** a **DASHBOARD** jön be. Jobb felül ott a **`MENU`** gomb.
2. Koppints a **`MENU`**-re → megjelenik az **5 gombos főmenü**.
3. Koppints a **`SENSORS`**-ra → látod, melyik szenzor **ONLINE** (zöld) és melyik
   **SIMULATED** (narancs). Ha csak a **BMP280**-at kötötted be, a `BMx280` sor
   **ONLINE**, a többi **SIMULATED**. Lejjebb élő EPS/ADCS/THRM értékek.
   Ujjal melegítve a BMP280-at a `THRM ... C` érték emelkedik — **élőben** látod.
4. **`BACK`** (jobb felül) → vissza a menübe. **`MODE CTRL`** → 3 mód-gomb. Koppints
   pl. **`PAYLOAD`**-ra: a queue-n át a CDH kapja a parancsot (mint egy földi
   telekomand), az „NOW:" mező **PAYLOAD**-ra vált, és az EVENT LOG-ban megjelenik
   `UI req mode PAYLOAD`. **Fontos:** ha aktív hiba van, az FDIR **visszaránthat
   SAFE-be** — ez helyes (az autonómia felülbírálja a menüt).
5. **`EVENT LOG`** → a teljes napló; **`ABOUT`** → projekt-infó; **`BACKLIGHT`** →
   ki/be kapcsolja a háttérvilágítást (a gomb felirata mutatja az állapotot,
   a menüben maradsz).
6. **`BACK`** a menüből → vissza a **DASHBOARD**-ra.

### Ha nincs érintőpanel bekötve
A menü nem nyílik meg magától (nincs bemenet) — ez normális, a dashboard fut.
Ha **touch panel nélkül** akarod bemutatni a menüt, fordítsd `1`-re a
`components/drivers/touch.h` `TOUCH_SIMULATE` makróját: ekkor a `touch` task
~3,5 mp-enként **szkriptelt koppintásokkal** végigjárja a menüt (MENU → SENSORS
→ MODE CTRL → ABOUT → vissza), így minden nézet magától megmutatkozik.

---

## 8. Fokozatos verifikáció (etapok)

### Etap A — csupasz panel (csak DevKit, semmi más)
Cél: a firmware és a toolchain rendben.
- **Elvárt:** a 7/B konzol-sorok megjelennek, LED fehér→kék→zöld, beacon pörög.
  (Kijelző nélkül is fut, csak grafikát nem látsz.)
- **Pass:** látod a „leaving BOOT" sort és az ismétlődő beacont.

### Etap B — kijelző hozzáadása
- **Elvárt:** `I ili9341: panel up 320x240`, és a dashboard kirajzolódik, a
  mezők frissülnek (a szimulált értékek lassan mozognak).
- **Ha fehér/fekete/szemetes a kép:** lásd Hibakeresés (kijelző).

### Etap C — szenzorok egyenként
Mindig **áramtalanítva** köss be egy újabb modult, majd indítsd újra.
- A konzolon a kulcs az **„online"** vs **„simulated/not found"**:
  - INA219 jó: `I ina219: INA219 online @0x40`
  - MPU6050 jó: `I mpu6050: MPU6050 online`
  - BME280 jó: `I bme280: BME280 online`
  - DS3231 jó: `I ds3231: DS3231 online`
- **Pass kritérium:** az adott modul „online"-t ír, és a dashboard megfelelő
  értéke a **valós** környezetre reagál (pl. a BME280-nál ujjal melegítve a
  THERMAL T° emelkedik; az MPU6050-et mozgatva a RATE/RPY változik).

### Etap D — földi állomás (telekomand/telemetria)
Csatlakoztasd a 2. USB-t a „UART" portra (vagy a külső adaptert), majd:
```bash
pip install pyserial
python tools/groundstation.py --port /dev/ttyUSB0 listen
```
- **Elvárt:** kb. 5 mp-enként `[OK ] TM len=... {...}` sorok (beacon, jó CRC-vel).
- Parancsok:
  ```bash
  python tools/groundstation.py --port /dev/ttyUSB0 ping
  ```
  → a kijelző **EVENT LOG**-jában és a konzolon megjelenik `RX op=0x01` majd
  `PONG`.
  ```bash
  python tools/groundstation.py --port /dev/ttyUSB0 set-mode PAYLOAD
  ```
  → a mód-szalag **PAYLOAD** (cián) lesz, LED cián. (Ha hiba aktív, az autonómia
  visszaránthatja SAFE-be — ez helyes viselkedés!)
- **Pass:** a `[OK]` CRC, a PONG megjelenése, és a mód-váltás a kijelzőn.

### Etap E — hibainjektálás (a lényeg: „tényleg az történik?")

**E1 — Alulfeszültség (potméterrel, szimulált EPS módban):**
- Tekerd a potmétert lassan lefelé. A dashboard **VBUS** értéke csökken.
- Amikor **VBUS < 3,30 V**: a **FAULTS** panelen piros **EPS-UV**, az EVENT
  LOG-ban `FAULT set 0x01`, a mód **SAFE**-re vált (LED kék), a konzolon
  `mode ... -> SAFE`.
- Tekerd vissza **3,70 V fölé**: a hiba törlődik, és amint „detumbled", az
  autonómia visszavisz **NOMINAL**-ba (LED zöld).
- **Ez bizonyítja:** szenzor→hibadetektálás→FDIR→mód-állapotgép lánc működik.

**E2 — Pörgés/tumbling (valódi MPU6050-zel):**
- Forgasd gyorsan a panelt (kézzel). Ha a **RATE > 25 dps**, az ADCS panel
  **TUMBLING!** (piros) lesz, **TUMBLE** hiba, mód → SAFE.
- Tartsd mozdulatlanul: pár másodperc múlva **DETUMBLED** (zöld), és visszatér.

**E3 — Túlmelegedés (valódi BME280-nal):**
- Melegítsd a szenzort (ujj, hajszárító óvatosan) **60 °C fölé** → **T-HOT**
  hiba, mód → SAFE.

**E4 — Fűtés-parancs:**
```bash
python tools/groundstation.py --port /dev/ttyUSB0 heater on
```
→ a THERMAL panelen **HEATER ON** (narancs).

**E5 — Watchdog (haladó):** ha kísérletként egy task ciklusába `vTaskDelay`-jel
hosszú blokkolást teszel, az FDIR `WATCHDOG missed` eseményt ad és SAFE-be visz.

---

## 9. „Jól működik" — konkrét pass/fail kritériumok

| Funkció | Bizonyíték (mit látsz) |
|---|---|
| Toolchain + boot | `=== C3SAT-OBC boot ===` és `leaving BOOT` a konzolon |
| FreeRTOS taskok élnek | 5 mp-enkénti beacon + 10 mp-enkénti FDIR health sor |
| Startup barrier (event group) | `all subsystems ready` üzenet |
| Kijelző (SPI) | 320x240 dashboard kirajzol, mezők frissülnek |
| I2C + valódi szenzor | adott driver `online`, érték reagál a környezetre |
| Mutex/blackboard | a dashboard és a beacon **ugyanazt** az értéket mutatja |
| UART telekomand | `ping` → `PONG` az event logban |
| Telemetria + CRC | `groundstation listen` → `[OK]` keretek |
| Tárolás (flash) | `dump-log` parancs CSV sorokat ír a konzolra |
| FDIR + mód-FSM | potméter le → EPS-UV → SAFE; vissza → NOMINAL |
| Mód-LED | szín követi a módot (kék/zöld/cián/piros) |

Ha ezek mind kipipálva, a projekt minden meghirdetett képessége **igazoltan**
működik.

---

## 10. Hibakeresés (tünet → ok → megoldás)

| Tünet | Valószínű ok | Megoldás |
|---|---|---|
| Semmi a konzolon | rossz port / rossz USB csatlakozó | az **USB** (nem UART) portot használd, `idf.py -p ... monitor` |
| `i2c ... timeout`, szenzor „not found" | rossz SDA/SCL, hiányzó felhúzó, rossz cím | ellenőrizd 22/23-at, tegyél 4,7 kΩ felhúzót, nézd a címet |
| MPU6050 és DS3231 közül egyik se látszik | `0x68` címütközés | MPU6050 **AD0 → 3V3** (0x69) |
| Kijelző fehér/üres | nincs IM strap (panel párhuzamos módban), nincs háttérvilágítás, rossz CS/RS/RST | `IM1+IM2→GPIO20` (firmware tartja HIGH), `IM0/IM3→GND`; `LED-A`→3V3 soros R-en; ellenőrizd `CS`=10, `RS`=11, `RST`=18 |
| Kijelző fehér, pedig IM GPIO-ra van kötve | a `bsp_display_straps_high()` nem futott le a panel reset ELŐTT | ellenőrizd, hogy a `bsp_init()` legelső hívása; a **GPIO20** tényleg a fizikai `IM1` **és** `IM2` lábra megy (mindkettő ugyanoda); mérd meg multiméterrel, hogy 3V3-at ad |
| Kijelző szemetes/torz | SPI láb felcserélve, vagy az órajelet rossz lábra kötötted | a **soros órajel a `WR` láb → GPIO6** (NEM „SCLK"!); `SDI`→7; nézd át az IM strapeket (4.2) |
| Kijelző tükrözött/elforgatott | rotáció | `gfx_init()`-ben az `ILI9341_ROT_*` érték állítható |
| Folyton FAULT (piros) | valós alacsony tápfesz, vagy potméter alul | tekerd a potmétert fel, vagy nézd a VBUS-t |
| `groundstation` nem lát semmit | rossz soros port / nem a „UART" portra kötve | a 2. USB-C-t a **UART** portba; helyes `--port` |
| Boot-loop / brownout | háttérvilágítás/kijelző túl sok áram | külön 3V3 táp a kijelzőnek, vagy erősebb USB-port |
| Érintés nem reagál | rossz 4-vezetékes bekötés, vagy túl magas `TOUCH_Z_THRESHOLD` | ellenőrizd `X+`=1, `Y+`=2, `X-`=3, `Y-`=21; csökkentsd a `TOUCH_Z_THRESHOLD`-ot a `touch.h`-ban |
| Érintés van, de rossz helyre üt | kalibráció / orientáció | állítsd a `TOUCH_CAL_*_MIN/MAX` és `TOUCH_INVERT_X/Y`, `TOUCH_SWAP_XY` értékeket a `touch.h`-ban (nézd a soros logban a nyers rx/ry-t) |
| „Fantom" érintések panel nélkül | nyitott ADC-lábak lebegnek | a `read_z_raw` belső pulldownnal védve; ha mégis van, tesztelj `TOUCH_SIMULATE 1`-gyel (nincs ADC-olvasás) |
| Menü nem nyílik, csak a dashboard | touch task nem indult, vagy üres a tap-queue | nézd a logban a `touch ADC ready` sort; ellenőrizd hogy `touch_start()` lefutott az `app_main`-ben |

---

### Tipp a felvételihez
Vidd magaddal demózni: élőben mutatható, hogy a potméter elcsavarásával a
műhold autonóm módon SAFE-be megy, majd visszaáll — ez pontosan az FDIR + mód-
állapotgép + esemény-vezérelt FreeRTOS architektúra, amit a C3S keres.
