# NB PC logger (Union plugin)

Zapisuje do `nblog.txt` wywołania wybranych funkcji skryptów NB (czas, nazwa, SELF, OTHER) – żeby porównać z logiem z Androida.

**Instalacja (Winlator):** skopiuj `nblog.dll` i `nblog.cfg` do `Gothic II\System\Autorun\`. Uruchom grę, zagraj walkę.
Log: `Gothic II\System\Autorun\nblog.txt`. Bez `nblog.cfg` DLL nic nie robi. Zła wersja exe → hook się nie zakłada (w logu „unknown exe”).

**nblog.cfg:** `SELF`/`OTHER` = indeksy symboli, `MINMS` = ta sama funkcja nie częściej niż co N ms, dalej linie `pozycja nazwa`.

**Budowa:** `./build.sh` (clang + lld-link, bez Windows SDK). Dla `Gothic2.exe` z symbolami (hook `zCParser::DoStack` 0x791960).
