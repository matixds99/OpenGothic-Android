# NB PC logger (Union plugin)

Zapisuje do `nblog.txt` wywołania wybranych funkcji skryptów NB (czas, nazwa, SELF, OTHER) – żeby porównać z logiem z Androida.

**Instalacja (Winlator):** skopiuj `nblog.dll` i `nblog.cfg` do `Gothic II\System\Autorun\`. Uruchom grę, zagraj walkę.
Log: `Gothic II\System\Autorun\nblog.txt`. Bez `nblog.cfg` DLL nic nie robi. Zła wersja exe → hook się nie zakłada (w logu „unknown exe”).

**nblog.cfg:** `SELF`/`OTHER` = indeksy symboli, `MINMS` = ta sama funkcja nie częściej niż co N ms, dalej linie `pozycja nazwa`.

**Budowa:** `./build.sh` (clang + lld-link, bez Windows SDK). Dla `Gothic2.exe` z symbolami (hook `zCParser::DoStack` 0x791960).

## Porównanie z OpenGothic

`nbcompare.py` zestawia `nblog.txt` (PC) z `nbcalls.txt` (OpenGothic zapisuje go, gdy w folderze gry leży `nblog.cfg` lub `nbcalls.cfg`, ta sama lista funkcji):

```
python3 tools/nbcompare.py nblog.txt nbcalls.txt --anchor AI_ONGETDAMAGE_IMARAH --npc "Im'Arah"
```

## Porównanie całej rozgrywki PC ↔ OpenGothic (scenariusze)

Ta sama lista funkcji działa po obu stronach: **ten sam `nblog.cfg`** kładziesz na PC w `System\Autorun\` (obok `nblog.dll`) i w folderze gry OpenGothic (Android). Wynik: `nblog.txt` (PC) i `nbcalls.txt` (port). Tryb pełny: `nblog.cfg` (860 funkcji, `MINMS 250`). Do walki o dużej częstotliwości: `nblog_all_fast.cfg` (`MINMS 1`, log rośnie szybko – tylko krótkie testy).

1. **Zagraj ten sam scenariusz** na obu wersjach (najlepiej od tego samego zapisu, żeby statystyki były równe), nic poza nim.
2. Po scenariuszu zamknij grę (log zapisuje się na bieżąco, ale zamknięcie kończy plik).
3. Porównaj:
   ```
   python3 tools/nbcompare.py nblog.txt nbcalls.txt --min 2
   ```
   `<<` = funkcję woła tylko PC (w porcie czegoś brakuje), `>>` = tylko port (coś dodatkowego), `!` = inna częstotliwość.
   Dla walki z jednym przeciwnikiem dodaj `--anchor AI_ONGETDAMAGE_<NAZWA> --npc "<Imię>"`.

Scenariusze do porównania (każdy osobno, osobne pliki logu – zmień nazwę `nblog.txt` po każdym):

| # | Scenariusz | Co sprawdza |
|---|-----------|-------------|
| 1 | Walka bronią 1H z jednym wilkiem/ścierwojadem (10 trafień) | obrażenia, runy broni, wytrzymałość |
| 2 | Walka magiem: 5 rzutów runą w jednego wroga | koszt many, `B_SelectSpell`, obrażenia czaru, procenty run (opętanie) |
| 3 | Użycie 3 eliksirów stałych i 2 ciast, otwarcie panelu statystyk | liczniki „Zużyte”, `RefreshStatusMenuData` |
| 4 | Rozmowa z nauczycielem i nauka umiejętności | punkty nauki, nowe `Teach*` |
| 5 | Zabicie bossa z listy osiągnięć | flagi osiągnięć, `RX_Achieve_*` |
| 6 | Kupno/sprzedaż u handlarza, przy kuźni: wykucie broni | kowalstwo, alchemia, `RX_Craft*` |
| 7 | 5 minut zwykłej wędrówki po Khorinis (bez walki) | tło: `Rx_*Tick`, głód, pogoda – „szum” różniący obie wersje |

Wynik każdego scenariusza wklej jako dwa pliki – z listy `<<` i `>>` robię poprawki. Pamiętaj: logger PC łapie tylko funkcje **skryptów**; funkcje samej wtyczki (zewnętrzne `RX_*`) są widoczne tylko w porcie jako `not implemented call [X]` i w podsumowaniu „functions of the plugin missing in the port”.

### Tylko brakujące funkcje (PC = baza)

```
python3 tools/nbcompare.py nblog.txt nbcalls.txt --missing
```
Lista funkcji, które woła PC (`nblog.txt`), a OpenGothic (`nbcalls.txt`) ani razu – od najczęstszych, z czasem pierwszego wywołania i pierwszym SELF/OTHER (wskazówka, gdzie szukać). Pod spodem: funkcje wołane w porcie 3x rzadziej niż na PC oraz linie portu o niezaimplementowanych funkcjach wtyczki (`PLUGIN ... not implemented`). Logi nie muszą pochodzić z identycznej rozgrywki, ale im bliższy scenariusz, tym mniej fałszywych wyników.

**`nblog_fight.cfg`** – lista do porównania walki z bossem/magiem na obu stronach: łańcuch obrażeń i run (`RX_CallDamage`, `B_RuneProcCheck`, `B_MagicHurtNpc`…), `B_SelectSpell`, `RX_ReadySpell`, `AI_On*_IMARAH`, `SPELL_LOGIC_*`. **Na PC i w porcie ten sam plik** (w porcie jako `nbcalls.cfg`) – inaczej brakujące funkcje to tylko skutek innej listy. Port zapisuje też zdarzenia `PLUGIN teleport_eligible` i `PLUGIN TELEPORT` (jak logger PC), więc można porównać rytm teleportów magów.

**`nblog_switch.cfg`** – zmiana celu bossa (np. Zeld i przywołany golem): `B_AssessDamage`, `B_MM_AssessDamage`, `B_Attack`, `ZS_Attack`, `ZS_Attack_End`, `ZS_MM_Attack`, `B_ClearPerceptions`. Z `self`/`other` widać, kto bił bossa po kolei (golem czy bohater) i kiedy boss zmienił cel. Też ten sam plik na PC (`nblog.cfg`) i w porcie (`nbcalls.cfg`).
