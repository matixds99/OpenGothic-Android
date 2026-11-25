# Zmienne skryptów zapisywane przez plugin NB (UNION_ABI.DLL) – przegląd

Stan: 25.11.2026, DLL z aktualizacji NB z 21.11.2026, GOTHIC.DAT PL.

Plugin czasem sam, bez wywołania ze skryptu, wpisuje wartość do zmiennej skryptów. Skrypty polegają na tym, że wartość się zmieni, ale w skryptach nie ma po tym śladu: raport brakujących funkcji tego nie pokaże, a port nie zgłosi błędu. Tak powstały błędy z poprawek 226 (flagi pauzy trybu wieloosobowego) i 228 (flagi „sługa już przyzwany”).

## Metoda

- Każde wywołanie `zCPar_Symbol::SetValue` (`0x100243b0` → `0x7A1E30`) poprzedzone `zCParser::GetSymbol` z nazwą zmiennej, plus zapisy przez wskaźniki do danych zmiennych trzymane przez plugin (`/tmp/nbup/gv.txt`).
- Dla każdej zmiennej: ile razy skrypty ją czytają i zapisują, czy port ją obsługuje, w jakiej funkcji pluginu jest zapis (inne zmienne zapisywane w tej samej funkcji i teksty, których ta funkcja używa).
- Wynik: 261 zmiennych zapisywanych przez plugin, 101 z nich czytają skrypty, a port ich nie zna.

## Wynik

| Grupa | Przykłady | Ocena |
|---|---|---|
| Flagi „sługa już przyzwany” – zerowane, gdy sługa ginie albo znika (`0x10184eb0`) | `RX_DemonHub_Active`, `CraitIsUp`, `JinaWolfIsUp`, `DSnapperIsUp`, `DarkPetIsUp`, `SkeletonUniqIsUp`, `RX_ScavSummoned`, `DragonStaffAlive` | **błąd – poprawka 228** |
| Konsola i kody pluginu (funkcje bez wywołań – tabela komend): „naucz wszystkiego” | `BOWMAKE_*`, `KNOWHOW*`, `RX_SHAMANTOTEM*LEARNED`, `PLAYER_TALENT_*`, `RX_DEBUG`, `RHETORIKSKILLVALUE`, `PICKPOCKETBONUSCOUNT`, `HeroInvisible` (niewidzialność z konsoli) | nie dotyczy rozgrywki |
| Tryb Full SnC+ (`InitFullSnc` i liczniki) | `MAX*`, `SNK*`, `*COUNTSNC`, `DAMAGEALL` | port nie ma tego trybu (poprawka 221) |
| Karma (w tej wersji NB wyłączona) | `RX_KARMA_*`, `KARMA_SKILL_SHIELD_*`, `RX_SummonCountMax` (respec karmy), `PlayerIsSick` / `PlayerIsIgnition` (łaska karmy) | wyłączone na PC |
| Interfejs pluginu | `HideGlobalInterface`, `RX_IconsPosX/Y`, `RX_HelpShown*`, `OgonekIsUp` (tylko ikona w pasku stanu) | port ma własny interfejs |
| Przyspieszenie czasu pluginu | `AccelerationActive`, `AccelerationRatio` | port ma własne przewijanie czasu |
| Kontrola istot (mroczny mag) | `RX_ControlCount` – zerowany przy usunięciu kontrolowanej istoty; `ZS_Dead` zmniejsza go sam | port nie ma kontroli istot z pluginu – licznik zostaje ≤ 0, warunki snu działają |
| Pozostałe | `Use_ArrowType` (wybór strzał), `RX_SaveCounter`, `laresGotoFarmDist` (start nowej gry), `RX_AW_SkillsCooldowns` | bez wpływu albo obsługiwane inaczej |

Poza tym przeglądem: flagi trybu wieloosobowego `RX_GamePaused` i `RX_GameVoting` nie są w tym DLL – ustawia je plugin gry wieloosobowej (poprawka 226).

Następny możliwy przegląd: zmienne skryptów, które plugin **czyta** i od których zależy jego zachowanie (np. `RX_NecroRuneAura_*`, `countLearnSpell`) – czy port czyta je tak samo.
