# Cat-M1/NB-IoT cellulárismodem-alrendszer integrálása hordozható NFC-azonosító eszközbe

## Tervezési és megvalósítási dokumentáció — SIM7080G modem-lap, „Beléptető Portable" (P1.0m hardvervariáns)

*Szakdolgozat-stílusú műszaki leírás. A dokumentum a `modem.kicad_sch` kapcsolási lap teljes tervét mutatja be és indokolja; a gyors bekötéshez a [F1] és [F2] belső dokumentumok szolgálnak, jelen írás a döntések mérnöki hátterét adja, minden állítást adatlap-hivatkozással alátámasztva.*

---

## Kivonat

A Beléptető Portable egy hordozható, NFC-kártyás felhasználó-azonosító terminál, amely a meglévő beléptetőrendszer adatbázisára támaszkodik, de ajtóvezérlés nélkül, önálló, mobil eszközként működik. A készülék negyedik hálózati csatlakozási módjaként — a Wi-Fi, az Ethernet és az USB mellé — LTE Cat-M1/NB-IoT cellulárismodem került a tervbe. Jelen dokumentum a SIMCom SIM7080G modul integrálásának teljes áramköri tervét írja le és indokolja: a modulválasztást, a tápellátást, a be- és kikapcsolásvezérlést, az 1,8 V-os és 3,3 V-os logikai tartományok közötti szintillesztést diszkrét MOSFET-ekkel, az USB-alapú firmware-frissítési útvonalat, a SIM-interfészt, a rádiófrekvenciás illesztést és az állapotjelzést. A szintillesztő megoldás sajátossága, hogy invertáló fokozatokból épül fel, amit az ESP32-C6 UART-perifériájának natív vonal-inverziója kompenzál; a dokumentum adatlap-paraméterekből levezetett számítással igazolja, hogy a választott Diotec MMFTN138 tranzisztor a célzott 115 200 baudos sebességen bőséges tartalékkal megfelel. Minden tervezési döntés mellett szerepel a vonatkozó adatlaphely (fejezet, ábra, táblázat, oldalszám).

---

## A dokumentum felépítése

Az 1–3. fejezet a kontextust, a követelményeket és a modulválasztást tárgyalja. A 4. fejezet a rendszerarchitektúrát és a lap illesztési felületeit írja le — ez a fejezet rögzíti azokat a peremfeltételeket, amelyekre a további fejezetek épülnek. Az 5–11. fejezet az egyes funkcionális blokkok részletes terve és indoklása (tápellátás, vezérlőjelek, szintillesztés, USB, SIM, RF, állapotjelzés). A 12. fejezet a kritikus tiltásokat és a nem használt lábak kezelését, a 13. a verifikációt, a 14. a firmware-vonatkozásokat foglalja össze. A 15. fejezet az eredmények értékelése, a korlátok és a továbbfejlesztési irányok. A dokumentumot számozott irodalomjegyzék zárja; a szövegközi hivatkozások formátuma: [sorszám, hely], például [1, Table 7, 21. o.].

---

## 1. Bevezetés és kontextus

A schbme csapat beléptetőrendszere köré épült eszközcsalád eddigi tagjai fix telepítésű ajtóvezérlők: kártyaolvasót kezelnek, nyitómágnest hajtanak, digitális be- és kimeneteket felügyelnek, és Ethernet- vagy Wi-Fi-kapcsolaton keresztül kommunikálnak a központi adatbázissal. A gyakorlat során azonban rendre felmerültek olyan felhasználási esetek, amelyekben nem ajtót kell nyitni, hanem pusztán *azonosítani* kell egy személyt a kártyája vagy RFID-karszalagja alapján: táborhelyszíni regisztrációs pult, klubszobai „becskassza" fogyasztás-nyilvántartás, nyomtatási sor kártyás feloldása (print holding), kulcsszéf naplózott hozzáférés-engedélyezése, illetve versenyeknél az induló és az időmérő eszköz által rögzített eredmény automatikus összerendelése. Ezekre a feladatokra készül a **Beléptető Portable**: a teljes értékű vezérlő lecsupaszított, hordozható származéka, amely egyetlen NFC-olvasót kezel, és a beléptető-adatbázist használja azonosításra.

A hordozhatóság új követelményt hoz be: az eszköznek olyan helyszíneken is működnie kell, ahol se vezetékes hálózat, se megbízható Wi-Fi nincs — tipikusan szabadtéri táborokban, ideiglenes rendezvényhelyszíneken. Erre a problémára a mobilhálózati kapcsolat a természetes válasz, az IoT-eszközök forgalmi profiljához (ritka, kisméretű tranzakciók) pedig az LTE Cat-M1 és NB-IoT technológiák illeszkednek. A hardver négy beültetési variánsban készül — alap (csak Wi-Fi), Ethernet, modem, illetve teljes kiépítés —, a revíziószámozás P1.0[e][m] sémát követ, ahol az `e` az Ethernet-, az `m` a modemvariánst jelöli. Jelen dokumentum tárgya a modemvariáns `modem.kicad_sch` hierarchikus lapja.

## 2. Követelmények

A modem-alrendszerrel szemben támasztott funkcionális követelmények a következők. Az alrendszer biztosítson kétirányú adatkapcsolatot LTE Cat-M1, tartalékként NB-IoT hálózaton; legyen az ESP32-C6 főprocesszor felől AT-parancsokkal vezérelhető az `esp_modem` komponensen keresztül, hardveres adatfolyam-vezérléssel (RTS/CTS); legyen szoftveresen be- és kikapcsolható, valamint kézzel is resetelhető; a modul firmware-e legyen frissíthető a készülék meglévő USB-C csatlakozóján keresztül, külön szervizcsatlakozó nélkül; a SIM-kártya nanoSIM formátumban, szerszám nélkül legyen cserélhető; az eszköz jelezze vizuálisan a hálózati állapotot; végül az alrendszer teljes egészében elhagyható legyen a modem nélküli variánsokban anélkül, hogy a többi áramkör működését befolyásolná.

A nem-funkcionális követelmények közül a meghatározóak: a beültetendő alkatrésztípusok számának minimalizálása (gyártási és beszerzési egyszerűsítés — ez a csapat kifejezett döntési szempontja volt a szintillesztő megoldásnál, lásd 7. fejezet és [F3]); a meglévő gyökér-lapi infrastruktúra (táp, USB-elosztás, hierarchikus buszok) változtatás nélküli felhasználása; valamint a SIMCom hardvertervezési útmutatójának [1] követése minden interfészen, dokumentált eltérésekkel ott, ahol a rendszerkörnyezet ezt indokolja (5.2. és 8.4. alfejezet).

## 3. Modulválasztás: SIM7080G

A modemmodul kiválasztásánál két jelölt maradt versenyben: a SIMCom SIM7070G és a SIM7080G. A kettő azonos Qualcomm MDM9205 alapchipre épül, AT-parancskészletük és így szoftveres illesztésük azonos; az érdemi különbség, hogy a SIM7070 tud 2G/GPRS tartalék-hálózatot, a SIM7080G viszont nem, cserébe kisebb (17,6 × 15,7 × 2,4 mm [1, 1.2. fej., 10. o.]), olcsóbb és jobban beszerezhető. A csapat mérlegelte, hogy szükség van-e 2G-fallbackre olyan helyszínekre, ahol nincs LTE-lefedettség, és a döntés — figyelembe véve a hazai Cat-M1/NB-IoT lefedettséget és a 2G-hálózatok kivezetését — a 2G elhagyása lett [F3]. Ezzel a SIM7080G egyértelmű győztes.

A modul képességei a követelményekhez bőven elegendők: Cat-M1 módban 589 kbps letöltési és 1119 kbps feltöltési, NB2 módban 136/150 kbps sebességet nyújt [1, Table 3, 12. o.], ami kártyaazonosítási tranzakciókhoz több nagyságrendnyi tartalék. A rádió adóteljesítménye 5-ös teljesítményosztályú, 0,125 W (21 dBm) [1, Table 3, 12. o.], energiagazdálkodási módjai (alvó mód 1,2 mA, PSM 3 µA [1, Table 3, 12. o.; Table 36, 59. o.]) hordozható eszközhöz illeszkednek. A modul kizárólag 1,8 V-os SIM-kártyát támogat [1, 3.5. fej., 32. o.], és teljes hardveres integrációs útmutatóval rendelkezik [1], amely minden interfészhez referencia-kapcsolást ad — a jelen terv ezekre a referencia-kapcsolásokra épül.

## 4. Rendszerarchitektúra és illesztési felületek

### 4.1. A hierarchikus felépítés és a lap határai

A projekt gyökérlapja (`portable.kicad_sch`) tartalmazza az ESP32-C6 modult, az USB-C csatlakozót, a tápellátást és két hierarchikus allapot: az Ethernet PHY-t és a modemet. A modemlap három hierarchikus interfészt kap: a `MODEM{NRST NINT RXD TXD RTS CTS}` vezérlő- és UART-buszt, a `MODEM_PROG{USB_D- USB_D+ EN}` programozó-buszt, valamint a `MODEM_ANT` rádiófrekvenciás jelet, amely a gyökérlapon egy U.FL koaxiális csatlakozóra fut. A lap tervének első lépése ezeknek a felületeknek a pontos jellemzése volt, mert minden további döntés ezekből következik.

### 4.2. A meghatározó peremfeltétel: a gyökérlap 3,3 V-os tartománya

A gyökérlap átvizsgálása során megállapítást nyert, hogy az ESP-oldali `MODEM.*` jelek **3,3 V-os logikai tartományban** vannak — a gyökérlapon szintillesztő nincs, csupán soros ellenállások az ESP kimenő vonalain (`MODEM_TX_R`, `MODEM_RTS_R` netek). Ez a felelősséget egyértelműen a modemlapra helyezi: a SIM7080G teljes digitális interfésze 1,8 V-os [1, Table 11, 29. o.; Table 34, 55. o.], a digitális lábak abszolút maximális feszültsége pedig mindössze **2,1 V** [1, Table 32, 54. o.], ezért a két tartomány közötti minden jelútvonalon kötelező az illesztés (7. fejezet).

### 4.3. Az USB-elosztás és az `EN` jel

A készülék egyetlen USB-C csatlakozóját a gyökérlapon egy Diodes PI3USB221 nagysebességű USB 2.0 multiplexer [8] osztja el az ESP32-C6 natív USB-je és a modem között. A választóvonalat a `MODEM_PROG.EN` jel adja, amelynek forrása egy jumper: a gyökérlapban nyomon követett áramút szerint a +3,3 V sín egy 100 Ω-os ellenálláson (R77) át a JP1 jumperre, onnan az `EN` netre, majd egy 10 kΩ-os ellenálláson (R41) át a földre fut. Ebből két, a modemlap tervét meghatározó tény következik: az `EN` jel **aktív magas** (a jumper behelyezése emeli 3,3 V-ra, alapállapotban a 10 kΩ a földön tartja), és **3,3 V-os** — tehát közvetlenül nem érintkezhet a modul 1,8 V-os lábaival (4.2. pont). A jumper behelyezése egyszerre váltja át az USB-multiplexert a modemre és élesíti a modul kényszerített letöltési (forced download) módját (8.2. alfejezet); enélkül az USB-C az ESP konzolja.

Megjegyzendő, hogy a korai tervváltozatban Toshiba TC7USB40 multiplexer szerepelt [F3]; a megvalósult gyökérlap PI3USB221-et használ, amelynek a jelen terv szempontjából lényeges tulajdonsága a kapcsolóutakba integrált ESD-védelem [8] — erre a 8.1. alfejezet épít.

### 4.4. Antenna-csatlakozás

A korai koncepció NYÁK- vagy chipantennát irányzott elő [F3]; a megvalósult gyökérlapon U.FL csatlakozó van, amely külső, kábeles antennát fogad. A modemlap szempontjából a két megoldás azonos topológiát igényel (π-illesztőhely, DC-leválasztás, ESD-védelem — 10. fejezet), kizárólag az illesztőelemek később behangolandó értékei különböznek, ezért a lap terve antennaválasztás-független.

## 5. Tápellátás

### 5.1. A VBAT sín követelményei és a referencia-kapcsolás

A modul VBAT tápbemenete (34. és 35. láb [1, Table 4, 14. o.]) 2,7 és 4,8 V között működik, tipikusan 3,8 V-on, és rádiós adás közben tipikusan **0,5 A-es csúcsáramokat** vesz fel [1, Table 7, 21. o.]. A gyártó tervezési útmutatója kiemeli, hogy a tápellátást úgy kell méretezni, hogy a VBAT a pillanatnyi maximális áramfelvételnél se essen 2,7 V alá, különben a modul feszültséghiány miatt lekapcsol [1, 3.1.1. fej., 21. o.]. A referencia-kapcsolás [1, Fig. 5, 22. o.] a tápvonalba ferritgyöngyöt, a modul lábaihoz pedig 100 nF + 1 µF + 3 × 100 µF hidegítő- és puffersort ír elő, ESD-védelemként TVS-diódával, továbbá legalább 1 mm széles VBAT-nyomtatással az impedancia minimalizálására [1, 22. o.].

A terv ezt a receptet követi: a +3,3 V sínről ferritgyöngyön (FB40) át jut a táp a `VBAT_M` netre, ahol közvetlenül a lábaknál 2 × 100 µF alacsony ESR-ű puffer (C40, C41), 1 µF (C42) és 100 nF (C43) hidegítés, valamint TVS-dióda (D44) helyezkedik el; a TVS típusára a gyártó ajánlási listája ad választékot [1, Table 8, 22. o.]. Az elrendezési szabály (lábközeli kondenzátorok, széles nyomtatás) a lap dokumentált layout-követelménye.

### 5.2. Tudatos eltérés: 3,3 V-os VBAT

A referencia-környezet 3,8 V-ot feltételez; a jelen terv a rendszer meglévő 3,3 V-os sínjét használja. Ez a megengedett tartományon belül van [1, Table 7, 21. o.], de a tartomány alsó harmadában: a 0,5 A-es adási csúcsok soros ellenállásokon és a tápforráson eső feszültsége a 2,7 V-os alsó határ felé közelítheti a lábfeszültséget. A döntés indoka az egyszerűség — nem kell külön 3,8 V-os tápág a modemvariánshoz —, kockázatkezelése kettős: egyrészt a lábközeli, nagy értékű puffersor (a csúcsot elsődlegesen a kondenzátoroknak kell adniuk, nem a tápforrásnak), másrészt a 13. fejezetben előírt méréses validáció (oszcilloszkópos VBAT-megfigyelés adás közben). Amennyiben a mérés 3,0 V alá süllyedést mutatna, a gyártó buck-referenciakapcsolása [1, 3.1.2. fej., Fig. 6, 23. o.] jelenti a tartalék megoldást. A tipikus áramfelvétel Cat-M1 adásnál sávtól függően 91–164 mA [1, Table 37, 60–61. o.], ami önmagában nem kritikus; a méretezést a tüskék határozzák meg.

A gyártó két üzemeltetési megkötése is a tápellátáshoz kapcsolódik, és a firmware-követelmények közé került (14. fejezet): a modul VBAT-ját működés közben tilos egyszerűen lekapcsolni, mert a belső fájlrendszer sérülhet — előbb PWRKEY-vel vagy `AT+CPOWD=1` paranccsal kell szabályosan leállítani [1, 22. o. és 3.2.2. fej., 26. o.].

### 5.3. VDD_EXT: a modul 1,8 V-os referenciakimenete

A modul beépített LDO-kimenete, a VDD_EXT (40. láb) 1,8 V-ot ad legfeljebb 50 mA terheléssel, és a gyártó kifejezetten külső GPIO-felhúzásra, illetve szintillesztő áramkör táplálására szánja [1, Table 6, 15. o.; 3.11. fej., Table 17, 41. o.]. A terv pontosan erre használja: ez a sín adja a szintillesztő fokozatok modul-oldali felhúzóit és a kényszerített letöltési mód felhúzási forrását (7–8. fejezet). A kimenet a bekapcsolási esemény után 64 ms-mal áll fel [1, Fig. 26, 41. o.], ami bőven megelőzi az első lehetséges AT-kommunikációt (1,8 s, 6. fejezet), így a szintillesztő tápja garantáltan kész, mire jelforgalom indul. A lábra 100 nF hidegítés (C44) és — a gyártó kifejezett kérésére — mérőpont (TP1) kerül [1, 18. o. megjegyzés; 32. o. kiemelt megjegyzés]. A terhelésmérleg: az öt felhúzó ellenállás legfeljebb néhány száz µA-t, a fokozatok kapacitív töltése elhanyagolható átlagáramot vesz fel, a fejlett tartalék az 50 mA-hez képest többszörös.

## 6. Be- és kikapcsolásvezérlés: a PWRKEY interfész

A modul be- és kikapcsolása a PWRKEY láb (39.) földre húzásával történik; a láb belül diódán át 1,8 V-ra (lebegve 1,5 V-ra) van felhúzva, aktív alacsony [1, Table 6, 16. o.; 3.2.1. fej., 24. o.]. Közvetlen GPIO-hajtás a feszültségtartomány-eltérés miatt kizárt, ezért a gyártó referenciakapcsolása tranzisztoros lehúzót ír elő 1 kΩ-os soros ellenállással, és nyomatékosan ajánlja ESD-védő dióda elhelyezését a láb közelében [1, Fig. 7, 24. o.]. A terv ennek MOSFET-es megfelelője: a `MODEM.NRST` jel egy N-csatornás MOSFET (Q40, típusáról a 7.4. alfejezet) gate-jét hajtja, 100 kΩ-os gate-lehúzóval (R41m) a határozott alapállapotért; a tranzisztor drainje 1 kΩ-on (R40) át éri el a PWRKEY lábat; a lábbal párhuzamosan nyomógomb (SW40) teszi lehetővé a kézi vezérlést (firmware-frissítésnél nélkülözhetetlen), és ESD-dióda (D40) védi a kivezetett csomópontot. Az így kialakuló logika: `MODEM.NRST` magas szintje nyitja a tranzisztort, amely földre húzza a PWRKEY-t.

Az időzítési követelmények az adatlapból: a bekapcsoló impulzus minimum **1 s**, a kikapcsoló minimum **1,2 s** [1, Table 9, 25. o.; Table 10, 27. o.]. Kritikus, a tervezés során a csapatkommunikációban is pontosított részlet, hogy a **12,6 s nem a bekapcsolási idő, hanem a beépített reset-időzítő küszöbe**: ha a PWRKEY ennél tovább marad földön, a modul automatikusan újraindul, ezért a láb tartós földre kötése tilos [1, Table 6, 16. o.; 3.2.1. fej., 24. o.]. A bekapcsolási esemény után a STATUS, az UART és az USB portok 1,8 s-mal válnak késszé [1, Table 9, 25. o.], két be-/kikapcsolási művelet között pedig legalább 2 s pufferidő tartandó [1, Table 10, 27. o.].

A STATUS láb (42.) — amely a bekapcsolt, kész állapotot jelzi magas szinttel [1, Table 6, 17. o.; 27. o. megjegyzés] — tudatosan nincs bekötve: a gyökérlapi `MODEM` busz nem tartalmazza, a készenlét pedig firmware-oldalon megbízhatóan detektálható az adatlapi 1,8 s-os időzítés kivárásával és az első AT-parancs visszhangjával. Ez egy GPIO-t és egy szintillesztő fokozatot takarít meg; ára, hogy a firmware időzítés-alapú, nem esemény-alapú készenlét-detektálást használ (14. fejezet).

## 7. Az UART-interfész és a szintillesztés

### 7.1. A feladat és az alternatívák

A modul elsődleges vezérlőfelülete a teljes funkciós UART1 (TXD, RXD, RTS, CTS az 1–4. lábakon), kiegészítve a RI megszakításjelzéssel (7. láb) [1, Table 4, 14. o.; 3.3. fej., 27. o.]. Az interfész 1,8 V-os: a bemeneti magas szint minimuma 1,17 V, a bemeneti alacsony maximuma 0,63 V [1, Table 11, 29. o.], a lábak abszolút maximuma 2,1 V [1, Table 32, 54. o.]. Az ESP-oldal 3,3 V-os (4.2. pont), tehát öt jelvonalon kell kétirányú tartomány-illesztést megvalósítani (négy UART-vonal két irányban, plusz a RI a modul felől).

A gyártó két referencia-megoldást kínál. Az első egy TXB0108 automatikus irányfelismerésű szintillesztő IC [1, Fig. 12, 29. o.; 7]; a második egy diszkrét, két tranzisztoros kapcsolás [1, Fig. 13, 29. o.], amelyhez az adatlap nagysebességű tranzisztort, konkrétan MMBT3904-et ajánl [1, 30. o. megjegyzés]. Harmadik, az iparban elterjedt lehetőség az NXP AN10441 alkalmazási jegyzet átvezető-FET-es (pass-FET), nem invertáló topológiája [10].

### 7.2. A döntés: diszkrét, közös source-ú MOSFET-fokozatok

A csapat a diszkrét MOSFET-es megoldást választotta, vonalanként egy N-csatornás tranzisztorral, közös source-ú kapcsolóüzemben: a gate a bemenet, a source a föld, a drain a kimenet, felhúzó ellenállással a gate-en és a drainen [F3]. A döntés elsődleges indoka a **BOM-konszolidáció**: a kiválasztott tranzisztor a tervben már jelen van (a gyökérlap Q3 pozíciójában, az RGB LED 3,3→5 V-os szintillesztőjeként), így a szintillesztés nem hoz be új alkatrésztípust, szemben a TXB0108-cal, amely új, több lábú, drágább IC lenne — a csapat megfogalmazásában „plusz egy féle alkatrész… plusz komplexitás, plusz figyelem a beültetésnél" [F3]. A döntést műszaki érv is támogatja: a TXB0108 gyenge, ±20 µA-es szinttartó kimenetei érzékenyek a vonalakra kötött terhelésekre, az adatlap kifejezetten óv az erős felhúzókkal terhelt alkalmazástól [7, 8.3.1. és 10.1. fej.], és ezzel összhangban a SIMCom is legalább 47 kΩ-os felhúzókat követel meg a szintillesztő IC használatakor [1, 30. o. megjegyzés] — a diszkrét megoldásnál ilyen kényes illesztési feltétel nincs. A választott topológia lényegében a gyártó Fig. 13 szerinti diszkrét referenciájának MOSFET-es megfelelője, bázisellenállások nélkül. Az AN10441-es átvezető-FET-es változat [10] azonos alkatrészköltségű, nem invertáló alternatíva maradt; a közös source-ú változat mellett a meglévő tervmintával (Q3) való azonosság és a csapatdöntés szólt, az inverzió költsége pedig — mint a 7.3. pont megmutatja — hardveresen nulla.

### 7.3. Az inverzió kezelése: vonal-inverzió az ESP32-C6 UART-jában

A közös source-ú fokozat invertál: a gate magas szintje nyitja a tranzisztort, amely a kimenetet földre húzza. Egyfokozatú felépítésnél tehát minden jel fizikailag fordított polaritással jelenik meg a túloldalon. Ezt az ESP32-C6 UART-perifériája hardveresen kompenzálja: a TXD, RXD, RTS és CTS vonalak polaritása egyenként invertálható a `uart_set_line_inverse()` hívással (`UART_SIGNAL_TXD_INV | UART_SIGNAL_RXD_INV | UART_SIGNAL_RTS_INV | UART_SIGNAL_CTS_INV`) [6]. A nyugalmi állapotok helyessége levezethető: az invertált ESP-adó nyugalmi fizikai szintje alacsony, így a fokozat tranzisztora zár, és a modul RXD-jét a VDD_EXT-re kötött felhúzó tartja a szabványos nyugalmi magas szinten; a modul adója nyugalomban 1,8 V-ot ad, a fokozat nyitva az ESP RXD-jét fizikai alacsonyra húzza, amit az `RXD_INV` beállítás logikai magassá értelmez. A hardveres adatfolyam-vezérlés vonalai funkcionálisan egyenesen kötendők (ESP RTS → modul RTS-bemenet, modul CTS-kimenet → ESP CTS), a gyártó teljes modemes bekötési ábrája szerint [1, Fig. 10, 28. o.], polaritásukat ugyanaz az inverzió rendezi. A RI jel — amely nyugalomban magas, és eseménykor (SMS, URC) 120 ms-os alacsony impulzust ad, előzetes `AT+CFGRI=1` engedélyezés után [1, 3.3.2. fej., Fig. 14, 30. o.] — a fokozaton át fordítottan, azaz nyugalmi alacsony szintként és felfutó élű eseményimpulzusként jelenik meg az ESP `MODEM.NINT` bemenetén; a firmware megszakítás-konfigurációja ehhez igazodik (14. fejezet). Az inverzió tehát tisztán konfigurációs kérdés — elmulasztása viszont a teljes kommunikáció némaságát okozza, ezért a kapcsolási rajzon figyelmeztető szövegmező rögzíti.

### 7.4. A tranzisztor azonosítása: Diotec MMFTN138

A tervben „BSS138" néven hivatkozott tranzisztor a gyökérlap Q3 pozíciójának tanúsága szerint valójában a **Diotec MMFTN138**: a KiCad `Transistor_FET:BSS138` szimbóluma alá MMFTN138 érték, beágyazott Diotec-adatlap és 637-MMFTN138 Mouser-cikkszám van felvéve. Az MMFTN138 a BSS138 másodforrása: azonos SOT-23 tok és lábkiosztás (1 = gate, 2 = source, 3 = drain [3, tokrajz]), azonos fejadatok (50 V, 220 mA, RDS(on) ≤ 3,5 Ω VGS = 10 V-nál [3, 1–2. o.; 4]), és a gyártó a tipikus alkalmazások között tételesen felsorolja a logikai szintillesztést („Logic level converter") [3, 1. o.]. Az egyetlen paraméterbeli eltérés a klasszikus BSS138-hoz képest a küszöbfeszültség felső határa: 1,6 V az 1,5 V helyett [3, 2. o.; 4] — a 7.6. pont worst-case elemzése ezért az 1,6 V-os sarokkal számol. A modemlap mind a nyolc tranzisztorpozíciója (Q40, Q50–Q56) erre a típusra épül, így a teljes lap egyetlen félvezető-típussal valósul meg.

### 7.5. A fokozatok kapcsolása és a felhúzósín-szabály

Az öt vonal azonos sémára épül; a különbség a felhúzók sínje, amelyet két szabály határoz meg. A gate-felhúzó annak a tartománynak a sínjére megy, amely a bemenetet hajtja (ESP-hajtott gate-nél +3,3 V, modul-hajtott gate-nél VDD_EXT); a drain-felhúzó pedig a fogadó fél tartományának sínjére (modul bemenete felé VDD_EXT, ESP bemenete felé +3,3 V). A modul-oldali gate-felhúzóknál a szabály nem választás, hanem kényszer: a modul kimeneti lábán ülő csomópont 3,3 V-ra húzása sértené a 2,1 V-os abszolút maximumot [1, Table 32, 54. o.], ezért ezek kizárólag a VDD_EXT-re köthetők. Az értékek: gate-felhúzók 100 kΩ (feladatuk csupán a hajtatlan állapot definiálása), drain-felhúzók 10 kΩ (ezek a jelformálók, méretezésüket a 7.6. pont adja). Az ESP-oldali kimenő vonalak meglévő soros ellenállásai (4.2. pont) a gate-eket hajtják; ez a konfiguráció kifogástalan, a gate-töltési időt a 7.6. pont számszerűsíti. Az elrendezés mellékhatása, hogy amíg az ESP resetben (nagyimpedanciás állapotban) van, az ESP-oldali gate-felhúzók nyitva tartják a fokozatokat, és a modul RXD-je break-állapotban áll — ez ártalmatlan (a modul ilyenkor jellemzően kikapcsolt), de dokumentált jelenség; ha valaha zavarna, e két gate-ellenállás földre kötése (lehúzóvá alakítása) a nyugalmi állapotot invertált adóéhoz igazítaná.

### 7.6. Sebesség- és szintigazolás adatlap-paraméterekből

A méretezés kiindulópontja, hogy a topológia sebességkorlátja nem a tranzisztor saját kapcsolási ideje — az az adatlap szerint nanoszekundumos: td(on) = 2,4 ns, tr = 18 ns, td(off) = 10,5 ns, tf = 34,5 ns tipikusan [3, 2. o.] —, hanem a **felfutó él**: záró tranzisztornál a kimeneti csomópontot egyedül a drain-felhúzó tölti, exponenciális RC-görbével.

A csomóponti kapacitás összetevői: a tranzisztor kimeneti kapacitása, Coss = 3,5 pF [3, 2. o.], a fogadó láb bemeneti kapacitása (≈5 pF) és a nyomtatás (≈3–6 pF), együtt C ≈ 12–15 pF, pesszimista becsléssel 25 pF. Az időállandó τ = R·C = 10 kΩ · 15 pF = 150 ns. A vevő küszöbének eléréséig szükséges idő t = τ·ln(V_pu/(V_pu − V_IH)): a modul bemenetén (1,8 V-os sín, V_IH = 1,17 V [1, Table 11, 29. o.]) 1,05·τ ≈ 158 ns, az ESP bemenetén (3,3 V-os sín, V_IH = 0,75·3,3 V = 2,475 V [5]) 1,39·τ ≈ 208 ns. A lefutó él ezzel szemben gyors: a gate-t a soros ellenálláson át a Ciss = 20 pF [3, 2. o.] tölti (1 kΩ-os ellenállással is csak 20 ns), a csatorna pedig néhány ohmos RDS(on)-nal süti ki a csomópontot; összesen 20–40 ns. A fel- és lefutó él küszöbidejének különbsége — az impulzusszélesség-torzítás — így legfeljebb ≈ 180 ns. Az UART-vevő a bit közepén mintavételez, a mérnöki hüvelykujjszabály szerint a torzításnak a bitidő 10%-a alatt kell maradnia (25% a kemény határ). Az eredmények:

| Sebesség | Bitidő | Torzítás (10 kΩ, 15 pF) | Torzítás (10 kΩ, 25 pF) | Értékelés |
|---|---|---|---|---|
| 115 200 baud | 8,68 µs | ≈ 2 % | ≈ 4 % | megfelel, nagy tartalékkal |
| 460 800 baud | 2,17 µs | ≈ 8 % | ≈ 15 % | megfelel / határeset |
| 921 600 baud | 1,09 µs | ≈ 17 % | ≈ 29 % | csak 4,7 kΩ-os drain-felhúzóval (≈ 6–12 %) |

Mivel a modul automatikus sebességfelismerése a 9600–115 200 baud tartományra korlátozott, és az `esp_modem` alapesetben 115 200 baudon üzemel [1, 3.3. fej., 27–28. o.], a tervezett 10 kΩ-os felhúzók a célsebességen bőséges tartalékot adnak; a RI vonal 120 ms-os impulzusához [1, Fig. 14, 30. o.] pedig bármely érték elegendő. A magasabb sebességek igénye esetén a drain-felhúzók cseréje 4,7 kΩ-ra dokumentált módosítási út.

A statikus szintek worst-case ellenőrzése: a küszöbfeszültség definíciója szerint (I_D = 1 mA a V_GS = V_DS = V_GS(th) pontban [3, 2. o.]) a legrosszabb, 1,6 V küszöbű példány 1,8 V-os gate-vezérléssel is legalább 1 mA levezetésére képes, miközben a fokozatnak legfeljebb 3,3 V / 10 kΩ = 0,33 mA-t kell nyelnie; a kimeneti alacsony szint így bizonyosan a vevők küszöbe alatt marad (ESP: V_IL = 0,25·3,3 V = 0,825 V [5]; modul: 0,63 V [1, Table 11, 29. o.]). A 3,3 V-ról hajtott fokozatoknál (1,5 V feletti túlvezérlés) az RDS(on) az adatlapi néhány ohmos tartományba esik [3, 2. o.], a kimeneti nulla millivoltos. Peremfeltételként rögzítendő, hogy a küszöbfeszültség alacsony hőmérsékleten nő (az adatlapi normalizált görbe szerint −25 °C-on mintegy 1,1-szeres [3, 3. o.]), így egy maximális küszöbű példány téli-kültéri környezetben az 1,8 V-os vezérlésű irányban hajszálvékony túlvezérlést kapna; beltéri célkörnyezetben ez nem kockázat, kültéri változatnál mintadarabos hidegteszt indokolt (13. fejezet).

## 8. Az USB-interfész és a firmware-frissítési útvonal

### 8.1. USB 2.0 adatvonalak

A modul USB 2.0 interfésze a szoftverfrissítés és -hibakeresés csatornája [1, 3.4. fej., 31. o.]. A `MODEM_PROG.USB_D+/-` jelek közvetlenül a modul USB_DP (25.) és USB_DM (26.) lábaira futnak; a vezetékpárt 90 Ω ± 10% differenciális impedanciával kell vezetni [1, 32. o. megjegyzés]. A gyártó a D± vonalakra 3 pF alatti kapacitású ESD-védelmet ír elő [1, Fig. 15 és kísérőszöveg, 31. o.]; a jelen rendszerben ezt a funkciót a gyökérlapi PI3USB221 multiplexer beépített ESD-védelme látja el [8], így a modemlapon a védelem duplikálása szükségtelen — sőt kerülendő, mert minden hozzáadott pF rontja a nagysebességű jelminőséget.

### 8.2. A kényszerített letöltési mód: USB_BOOT

A modul akkor lép kényszerített USB-letöltési módba, ha a bekapcsolás (PWRKEY-lehúzás) előtt az USB_BOOT láb (20.) a VDD_EXT-re van húzva; normál rendszerindításnál a lábnak szabadon kell maradnia — az adatlap kiemelt, tiltó megfogalmazásával: „DO NOT PULL UP DURING NORMAL POWER UP" [1, Table 6, 18. o.; 15. o. megjegyzés; 3.4.2. fej., 32. o.]. A gyári referencia a lábat 10 kΩ-on át köti a VDD_EXT-hez, mérőponttal és TVS-védelemmel, a felhúzást kézi beavatkozásra bízva [1, Fig. 16, 32. o.].

A terv ezt a kézi beavatkozást váltja ki elektronikusan, az `EN` jellel vezérelve, egyetlen tranzisztorral: a Q55 (MMFTN138) drainje a VDD_EXT-re, gate-je az `EN`-re csatlakozik, source-a 10 kΩ-on (R42) át éri el az USB_BOOT lábat. A kapcsolás forráskövetőként működik: `EN` alacsony állapotában (a gyökérlapi 10 kΩ-os lehúzó által garantáltan, 4.3. pont) a tranzisztor zár, a lábat a belső lehúzó (a láb DI,PD típusú [1, Table 6, 18. o.]) tartja alacsonyan — normál boot; `EN` = 3,3 V-nál a source a drain-feszültség, azaz a VDD_EXT által korlátozva 1,7–1,8 V-ra emelkedik, ami a legrosszabb küszöbű tranzisztorral is a láb 1,17 V-os bemeneti magas küszöbe felett van [1, Table 34, 55. o.; 3, 2. o.]. A megoldás kulcsa, hogy a 3,3 V-os `EN` így soha nem érintkezik közvetlenül a 2,1 V abszolút maximumú lábbal [1, Table 32, 54. o.]: a követő kimenete fizikailag nem tud a VDD_EXT fölé menni. Az időzítési lánc a gyári referenciáéval azonos — a felhúzási forrás maga a VDD_EXT, amely a bekapcsolási esemény után 64 ms-mal áll fel [1, Fig. 26, 41. o.], pontosan úgy, ahogy a Fig. 16 szerinti jumperes megoldásnál. A lábat TVS (D41) védi és mérőpont (TP2) egészíti ki, mindkettő adatlapi előírás [1, Fig. 16, 32. o.; 18. o. megjegyzés].

### 8.3. USB_VBUS: állandó 5 V és következménye

A modul az USB-csatlakozást a VBUS-on érzékeli, amelynek érvényes tartománya 3,5–5,25 V [1, 3.4. fej., 31. o.; Table 33, 54. o.]. A terv — az eredeti koncepcióval összhangban [F3] — a VBUS-t állandóan a rendszer +5 V-jára köti, ferritgyöngyön (FB41) át, 100 nF hidegítéssel (C48). Az egyszerűség ára ismert és dokumentált: a modul alvó módjának adatlapi feltétele az USB-táp leválasztása („Connected USB can't enter into sleep mode… it must disconnect the power supply for USB_VBUS first" [1, 5.3.2. fej., 57. o.]), tehát ebben a konfigurációban a modem energiatakarékos módjai nem érhetők el. USB-ről táplált pult-eszköznél ez érdektelen; akkumulátoros változatnál a VBUS-t az `EN` jellel kapuzó P-csatornás kapcsoló jelentené a megoldást, amelyet a terv tudatosan nem tartalmaz, mert új alkatrésztípust hozna be a jelenlegi igény nélkül (2. fejezet, nem-funkcionális követelmények).

## 9. A SIM-interfész

A modul kizárólag 1,8 V-os SIM-kártyát támogat, kártyacsere-érzékelés (hot-swap) nélkül [1, 3.5. fej., 32. o.; 33. o. megjegyzés]; az elektromos szintek a Table 12 szerintiek [1, 33. o.]. A terv a gyári referenciakapcsolást követi [1, Fig. 17, 33. o.]: a SIM_VDD (18. láb) közvetlenül a foglalat VCC-jére fut, a foglalathoz közel elhelyezett 100 nF hidegítéssel (C47); a SIM_CLK, SIM_RST és SIM_DATA vonalak (16., 17., 15. láb) egyenként 22 Ω-os soros ellenálláson (R45–R47) át érik el a foglalatot; a foglalat oldalán négycsatornás ESD-tömb (D43, pl. ST ESDA6V1W5 [9] vagy onsemi SMF15C, mindkettő a gyártó ajánlása [1, 3.5.1. fej., 33. o.]) védi a vonalakat. A védőelem kiválasztásának kemény korlátja a parazita kapacitás: a SIM_CLK él-idejének 40 ns alatt kell maradnia, ezért a TVS kapacitása 50 pF alatt [1, 34. o.], a vezetékezési irányelvek szigorúbb előírása szerint legfeljebb 15 pF lehet [1, 3.5.2. fej., 35. o.] — a terv ez utóbbit követi. Külső felhúzó a SIM_DATA vonalra nem kerülhet: a modul belül 20 kΩ-mal húzza a SIM_VDD-re, és az adatlap a külső felhúzást kifejezetten tiltja [1, Table 6, 16. o.]. Az elrendezési szabályok — a foglalat távoltartása az antennától és a nagysebességű jelektől, a SIM_CLK önálló földkeretezése, rövid, el nem ágazó vonalvezetés — szintén adatlapi előírások [1, 3.5.2. fej., 35. o.], és a lap layout-követelményei közé kerültek.

## 10. A rádiófrekvenciás illesztés

A modul RF_ANT antennakimenete (32. láb) és a gyökérlapi U.FL csatlakozó közé a gyártó ajánlott illesztőlánca kerül [1, 4.2. fej., Fig. 28, 47. o.]: π-topológiájú illesztőhely, amelyben a soros elem alapkiépítésben 0 Ω (R48), a két söntkondenzátor (C50, C51) pedig beültetetlen — értéküket az antennagyártó, illetve a kész eszközön végzett hangolás adja majd [1, 48. o.] —, ezt követi a kötelező 100 pF-os soros DC-leválasztó kondenzátor (C49), amely egyben az ESD-védelem része [1, 48. o.], végül az antennacsomópontot ultraalacsony kapacitású TVS (D42) védi; a gyártó ajánlott típusa a 0,05 pF-os BILLSEMI BLE5V0CR05UB [1, Table 28, 48. o.]. A vonalvezetés 50 Ω-os hullámimpedanciájú, a beiktatási csillapítás sávonkénti célértékei a Table 27 szerintiek [1, 47. o.]; az elrendezési szabályok — rövid vonal, tört szögek kerülése, sűrű földátkötések, távolság a nagysebességű jelektől, ép referencia-földsík — a 4.4.1. fejezet előírásai [1, 51. o.].

A modul GNSS-vevője kihasználatlan marad: a GNSS_ANT láb (68.) szabadon hagyandó. A döntést a felhasználási igény hiányán túl chipset-korlát is alátámasztja: a Qualcomm-lapkakészlet miatt a mobilhálózati és a GNSS-vétel közös hardverelemeken osztozik, egyidejű működésük nem támogatott [1, 4.3.2. fej. megjegyzés, 49. o.], tehát folyamatos hálózati jelenlét mellett a helymeghatározás amúgy sem lenne elérhető.

## 11. Hálózati állapotjelzés: NETLIGHT

A modul NETLIGHT kimenete (41. láb) a hálózati állapotot kódolja villogási mintázatokkal: 64 ms be / 800 ms ki regisztrálatlan, 64/3000 ms regisztrált hálózatot, 64/300 ms adatforgalmat jelez, a folyamatos kikapcsolt állapot pedig kikapcsolt vagy PSM-módú modult [1, Table 15, 40. o.] — hibakereséskor ez az első számú vizuális diagnosztika. A gyári referencia NPN-tranzisztoros LED-hajtást mutat 4,7 kΩ / 47 kΩ bázisosztóval [1, Fig. 25, 39. o.]; a terv ennek MOSFET-es megfelelőjét használja (Q56, MMFTN138), amivel a bázisellenállások elhagyhatók és — a 2. fejezet konszolidációs követelményével összhangban — nem kerül be új tranzisztortípus. A NETLIGHT közvetlenül a gate-et hajtja, 100 kΩ-os lehúzóval (R50); a LED (LED40) és soros ellenállása (R51, ≈510 Ω) a VBAT_M sínről táplálkozik, a gyári ábra topológiájával egyezően. Az ellenállásérték a LED karakterisztikájának függvénye — ezt maga az adatlap is így rögzíti [1, 40. o. megjegyzés] —; 510 Ω-mal, ≈2 V nyitófeszültségű vörös LED-del az áram ≈2,4 mA, amit az 1,8 V-os gate-vezérlésű tranzisztor a 7.6. pont szintelemzése alapján biztonsággal kapcsol.

## 12. Nem használt lábak és kritikus tiltások

A modul 77 lábából a terv 19-et használ jelként, 23-at köt földre, 35-öt hagy szabadon; a szabadon hagyás minden érintett lábnál a gyártó „if unused, keep open" előírását követi [1, Table 6, 15–18. o.]. Két láb körül azonban nem semleges mellőzésről, hanem aktív tiltásról van szó, és ezek a terv legfontosabb csapdái. Az **SPI_MOSI (49. láb)** a rendszerindítás előtt gyorsindítási (fast boot) funkciót hordoz: ha a boot pillanatában magas szinten van, a modul nem indul el — az adatlap két helyen is kiemeli, hogy a láb bekapcsolás előtt nem húzható fel [1, 15. o. megjegyzés; 3.8. fej. megjegyzés, 39. o.]; a lábra ezért semmilyen áramköri elem nem csatlakozhat. Az **USB_BOOT (20. láb)** ugyanezen megjegyzés másik alanya — kezelését a 8.2. alfejezet írja le. A DTR láb (6.) szabadon hagyása a DTR-alapú alvásvezérlés tudatos mellőzését jelenti: a modul alapállapotban (`AT+CSCLK=0`) nem reagál a DTR-re [1, 3.3.2. fej., 30. o.], az alvó mód pedig a VBUS-döntés miatt egyébként sem elérhető (8.3. pont). Az UART2 (22–23. láb) a rendszerindítási naplót adja ki [1, Table 6, 17. o.]; mérőpont elhelyezése rajta opcionális diagnosztikai kényelem. A modul környezetében 3 mm-es beültetési védőtávolság tartandó a javíthatóság érdekében [1, 2.3. fej. megjegyzés, 19. o.], a footprint pedig gyártás előtt a gyári ajánlással vetendő össze [1, Fig. 4, 20. o.].

Általános elvként a lapról kifelé vezető minden csomópont (PWRKEY, USB_BOOT, SIM-vonalak, antenna) külső ESD-védelmet kapott; ennek szükségességét a modul saját ESD-tűrése indokolja, amely a nem-RF lábakon mindössze ±1 kV kontakt kisülés [1, Table 40, 62. o.].

## 13. Verifikáció és validáció

A terv háromszintű ellenőrzésre épül. Az első a **statikus tervellenőrzés**: a kapcsolási rajz ERC-futtatása (a VDD_EXT neten szükség szerint PWR_FLAG-gel), a lábmérleg egyeztetése (19 + 23 + 35 = 77), a szintillesztő felhúzósín-szabályának vonalankénti ellenőrzése (7.5. pont), valamint a gyártó saját tervezési ellenőrzőlistáinak végigvitele — a sematikus lista [1, Table 48, 75. o.] és az elrendezési lista [1, Table 49, 75–76. o.] tételei közül a terv kettőtől tér el tudatosan: a 3,3 V-os VBAT-tól (5.2. pont) és az aktív antenna hiányától (passzív antenna esetén a vonatkozó tétel tárgytalan).

A második szint a **számításos igazolás**, amelyet a 7.6. pont tartalmaz: a szintillesztő időzítési és szintmarzsai adatlap-paraméterekből levezetve, a legrosszabb gyártási sarokra is.

A harmadik szint a **méréses validáció** az első prototípuson: oszcilloszkópos VBAT-megfigyelés rádiós adás közben (elfogadási határ: a lábfeszültség ne süllyedjen 3,0 V alá, vö. 5.2. pont és [1, Table 7, 21. o.]); az UART-jelalakok ellenőrzése 115 200 baudon (felfutási idő és torzítás összevetése a 7.6. pont számított értékeivel); a kényszerített letöltési mód végigpróbálása (jumper be → PWRKEY → a modul USB-eszközként jelentkezik be); a NETLIGHT-mintázatok megfigyelése hálózatkeresés és regisztráció közben [1, Table 15, 40. o.]; kültéri célváltozat esetén a modul-hajtott szintillesztő fokozatok hidegtesztje (7.6. pont záró megjegyzése).

## 14. Firmware-vonatkozások

A hardverterv öt kötelezettséget hárít a firmware-re, amelyek a kapcsolási rajzon jegyzetként is rögzültek. Az első és legkritikusabb az **UART-vonalinverzió**: a `uart_set_line_inverse()` hívás mind a négy vonalra, az `esp_modem` DTE létrehozása után [6]; enélkül a kommunikáció teljes egészében néma (7.3. pont). A második a **RI-kezelés**: az eseményjelzés az ESP oldalán felfutó él, előzetes `AT+CFGRI=1` engedélyezéssel [1, Fig. 14, 30. o.; 2]. A harmadik a **PWRKEY-szekvencia**: bekapcsoláshoz legalább 1 s, de 12,6 s-nál rövidebb impulzus, kikapcsoláshoz legalább 1,2 s, két művelet között legalább 2 s szünet, az első AT-parancs előtt pedig legalább 1,8 s várakozás [1, Table 9–10, 25–27. o.]. A negyedik a **rögzített 115 200 baudos** üzem, az automatikus sebességfelismerés tartományán belül [1, 3.3. fej., 27–28. o.], összhangban a szintillesztő méretezésével (7.6. pont). Az ötödik a **szabályos leállítás** áramtalanítás előtt: PWRKEY vagy `AT+CPOWD=1`, a modul fájlrendszerének védelmében [1, 3.2.2. fej., 26. o.; 2].

## 15. Összegzés, korlátok, továbbfejlesztés

A bemutatott terv a SIM7080G modult a gyártói integrációs útmutató [1] referencia-kapcsolásaira építve, három dokumentált eltéréssel illeszti a Beléptető Portable rendszerkörnyezetébe. Az eltérések: a VBAT a rendszer 3,3 V-os sínjéről táplálkozik a névleges 3,8 V helyett (megengedett, de méréses validációt igénylő döntés — 5.2. pont); a szintillesztés a gyártó IC-s referenciája helyett a diszkrét referencia MOSFET-es megfelelőjével valósul meg, aminek árát — az inverziót — az ESP32-C6 hardveres vonalinverziója nullára csökkenti (7. fejezet); a VBUS állandó 5 V-os táplálása pedig az egyszerűségért feláldozza a modul alvó módjait (8.3. pont). A terv legfontosabb minőségi jellemzője a radikális alkatrész-konszolidáció: a lap mind a nyolc tranzisztorfunkciója — öt szintillesztő fokozat, a PWRKEY-hajtás, az USB_BOOT-kapuzás és a LED-hajtás — egyetlen, a projektben már bevezetett típussal (Diotec MMFTN138 [3]) valósul meg, és a méretezés minden pontja adatlap-paraméterből levezetett, worst-case sarkokra is igazolt.

A terv ismert korlátai egyben a továbbfejlesztés irányai. Az akkumulátoros üzem a VBUS kapuzását (8.3. pont) és a DTR-alapú alvásvezérlés bevezetését igényelné; extrém hidegkörnyezeti célnál a modul-hajtott szintillesztő fokozatok küszöbmarzsa szűkül, amire alacsonyabb küszöbű tranzisztortípus vagy az AN10441 szerinti nem invertáló topológia [10] adna tartalékot; a 3,3 V-os VBAT pedig, ha a méréses validáció határeseti eredményt hozna, dedikált 3,8 V-os buck-átalakítóval váltható ki a gyártó referenciája szerint [1, Fig. 6, 23. o.]. E módosítások egyike sem érinti a lap illesztési felületeit, így a jelen terv stabil alapja a variánscsalád további fejlesztésének.

---

## Irodalomjegyzék

[1] SIMCom Wireless Solutions Limited: *SIM7080G Hardware Design*, V1.05, 2023-06-14. (A szövegközi oldalszámok a dokumentum „n / 82" lábjegyzetszámozását követik.)

[2] SIMCom Wireless Solutions Limited: *SIM7080 Series AT Command Manual*, V1.02. (Az [1] dokumentum Table 1 jegyzéke szerinti [1]-es hivatkozott kézikönyv; az `AT+CFGRI`, `AT+CSCLK`, `AT+CPOWD`, `AT+CBATCHK` parancsok referenciája.)

[3] Diotec Semiconductor AG: *MMFTN138 — N-Channel Enhancement Mode Field Effect Transistor*, adatlap, Version 2025-11-20. Elérhető: https://diotec.com/tl_files/diotec/files/pdf/datasheets/mmftn138.pdf („Typical Applications" — 1. o.; statikus és dinamikus jellemzők — 2. o.; hőmérsékletfüggési grafikonok — 3. o.)

[4] onsemi (korábban Fairchild Semiconductor): *BSS138 — N-Channel Logic Level Enhancement Mode Field Effect Transistor*, adatlap. Elérhető: https://www.onsemi.com/pub/Collateral/BSS138-D.PDF (Az MMFTN138-cal való egyenértékűség összevetéséhez.)

[5] Espressif Systems: *ESP32-C6 Series Datasheet* — DC Characteristics (V_IH = 0,75·VDD, V_IL = 0,25·VDD).

[6] Espressif Systems: *ESP-IDF Programming Guide* — UART driver: `uart_set_line_inverse()`, `UART_SIGNAL_TXD_INV`, `UART_SIGNAL_RXD_INV`, `UART_SIGNAL_RTS_INV`, `UART_SIGNAL_CTS_INV`.

[7] Texas Instruments: *TXB0108 8-Bit Bidirectional Voltage-Level Translator with Auto Direction Sensing*, adatlap (SCES643). Elérhető: https://www.ti.com/lit/ds/symlink/txb0108.pdf (A kimenetterhelési és felhúzó-ellenállási korlátozások az elvetés műszaki indokai között.)

[8] Diodes Incorporated: *PI3USB221 — Low-Power, High-Speed USB 2.0 Multiplexer/Demultiplexer with Integrated ESD Protection*, adatlap. (A gyökérlapi U1; az USB D± vonalak ESD-védelmének forrása.)

[9] STMicroelectronics: *ESDA6V1W5 — Quad Transil array for ESD protection*, adatlap. (A SIM-interfész védőtömbjének gyártói ajánlás szerinti típusa, vö. [1, 33. o.].)

[10] NXP Semiconductors: *AN10441 — Level shifting techniques in I²C-bus design*, alkalmazási jegyzet, 2007. (A nem invertáló, átvezető-FET-es szintillesztő topológia referenciája.)

### Belső dokumentumok

[F1] *modem_bekotes_v2.md* — vázlatos bekötési útmutató blokkonkénti hivatkozásokkal (14. fejezetében a részletes adatlap-hivatkozásjegyzékkel).

[F2] *modem_huzalozas_lepesrol_lepesre.md* — csomópont-szintű huzalozási utasítás a `modem.kicad_sch` megvalósításához.

[F3] Csapat-döntésnapló (Discord, 2026. június): modulválasztás (SIM7070G ↔ SIM7080G, 2G-fallback elvetése), szintillesztő-topológia döntés (diszkrét MOSFET a TXB0108 helyett), USB-elosztási és VBUS-koncepció.

*A gyökérlapra (`portable.kicad_sch`) vonatkozó megállapítások — az `EN` jel áramútja, a Q3 alkatrészfelvétele, a MODEM_TX_R/MODEM_RTS_R soros ellenállások és az U.FL csatlakozó — a projektfájl közvetlen elemzéséből származnak.*
