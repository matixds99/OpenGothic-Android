# Skrypty New Balance (kopia)

Pliki z folderu `Data` gry Gothic II: New Balance, spakowane w ZIP (`AB_Scripts.vdf` ma 188 MB, a GitHub przyjmuje pliki do 100 MB):

| Plik | W środku | Rozmiar po rozpakowaniu |
|---|---|---|
| `AB_Scripts.zip` | `AB_Scripts.vdf` – skrypty (GOTHIC.DAT), plugin `System/Autorun/UNION_ABI.DLL`, tekstury, animacje, dźwięki | 188 MB |
| `AB_Scripts_PL.zip` | `AB_Scripts_PL.vdf` – polska wersja | 50 MB |

Z `UNION_ABI.DLL` wyciągnięte są m.in. przepisy stołów rzemiosła (poprawki 70–71 w `CHANGELOG.md` na gałęzi `touch-ui`).

## Wersje

| Data | GOTHIC.DAT (PL) | Uwagi |
|---|---|---|
| 16.11.2026 | 272 601 symboli | pierwsza kopia |
| 24.11.2026 | 268 842 symbole | aktualizacja skryptów z 21.11.2026 (pliki VDF): system karmy L5 (strażnicy, fanatycy, źródła – `Karma_*`), patrole (`Patrol_Set`, `Patrol_SetPath`, `Patrol_NextWP` zamiast `Patrol_Commit`, `Patrol_DefinePoints`, `Patrol_DefineAuto`), `Npc_SetEffect`, wierzchowiec padlinożerca, nowe dialogi i przedmioty |

Nazwy symboli wersji z 16.11.2026 (do wczytania starych zapisów portu): `old_symbols_pl.txt`, `old_symbols_ru.txt` (poprawka 209).
