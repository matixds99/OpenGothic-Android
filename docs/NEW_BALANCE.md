# Poprawki OpenGothic dla Gothic II: New Balance – gałąź `test`

Baza: projekt [`borsuczyna/opengothic-android`](https://github.com/borsuczyna/opengothic-android), wersja z 31.10.2026 (commit `31b8115`). Zmiany borsuczyny dociągnięte do 23.11.2026 (commit `eb943e19`, poprawka 179).
Ścieżki plików są takie same jak w tamtym projekcie: plik `opengothic-NB/common/gothic.cpp` z tego repo trafia do `common/gothic.cpp` w projekcie.

Historia wszystkich poprawek: **[CHANGELOG.md](CHANGELOG.md)**

---

## ⚠️ Nowe pliki (nie ma ich w projekcie borsuczyny)

Te pliki trzeba **dodać**, nie podmieniać. Bez nich build się nie uda.

| Nowy plik | Gdzie go wrzucić | Kto go potrzebuje | Od poprawki |
|---|---|---|---|
| `common/game/nboptions.h` | do folderu `common/game/`, obok `gamescript.cpp` | `gamescript.cpp`, `ui/gamemenu.cpp`; przełącznik „Oznaczanie nowych przedmiotów w ekwipunku” (`NB_OG`/`bNewItems`, domyślnie wł.); opcja „Usuwanie pustych ciał”; opcja „Pasek doświadczenia”; opcja „Zasięg widzenia świata”; „Rysowanie świata” (obiekty jak teren) | 5, 44, 49, 53, 62, 121, 122, 123, 136, 137, 146, 147 |
| 🆕 `common/game/nbtexts.h` | do folderu `common/game/`, obok `gamescript.cpp` | `gamescript.cpp` | 62, 69, 176 |
| 🆕 `common/game/nbcraft.h` | do folderu `common/game/`, obok `gamescript.cpp` | `gamescript.h`, `ui/nbcraftmenu.h` (przepisy stołów rzemiosła z UNION_ABI.DLL) | 70, 71 |
| `common/game/nbiteminfo.h` | do folderu `common/game/`, obok `gamescript.cpp` | `gamescript.cpp` | 12, 13 |
| `common/game/nbbars.h` | do folderu `common/game/`, obok `gamescript.cpp` | `gamescript.cpp`, `mainwindow.cpp` (pasek wytrzymałości, otwarty ekwipunek) | 41, 49 |
| `android/assets/touch-icons/quick_attack.png` | do folderu `android/assets/touch-icons/` | `utils/androidinput.cpp` (ikona szybkiego ataku) | 85 |
| `android/assets/touch-icons/sprint.png` | do folderu `android/assets/touch-icons/`, obok `walk.png` | `utils/androidinput.cpp` (ikona sprintu) | 40 |
| 🆕 `common/utils/drmp3.cpp` | do folderu `common/utils/` | `gamemusic.cpp` (dekoder mp3 muzyki NB) | 183 |
| 🆕 `lib/dr_libs/dr_mp3.h` (+ `LICENSE`) | do folderu `lib/dr_libs/` (dr_mp3 0.7.5, domena publiczna / MIT-0) | `utils/drmp3.cpp`, `gamemusic.cpp` | 183 |
| 🆕 `android/launcher-assets/covers/newbalance.webp` | do folderu `android/launcher-assets/covers/`, obok `gothic2.webp` | `LauncherActivity.java` (okładka karty New Balance) | 195 |
| 🆕 `common/utils/nblog.cpp` (+ `.h`) | do folderu `common/utils/` | `main.cpp`, `mainwindow.cpp`, `crashlog.cpp`, `npc.cpp`, `damagecalculator.cpp`, `androidinput.cpp` (znacznik, podsumowanie problemów, poziom logu) | 199–203, 204 |

> `lib/dr_libs` jest dopisany w `CMakeLists.txt` (poprawka 183) – plik `CMakeLists.txt` też trzeba podmienić.
>
> Nagłówki (`.h`) i ikona – nie trzeba niczego dopisywać w `CMakeLists.txt` (ikony są zbierane z folderu automatycznie; przy pierwszym buildzie po dodaniu ikony CMake sam przeładuje listę).

---

## Zmienione pliki (podmień 1:1)

| Plik | Co jest zmienione | Poprawki |
|---|---|---|
| `common/world/triggers/cscamera.cpp` | kamera filmowa: czarny ekran w scenie z Xardasem, kierunek kamery z punktów trasy (wstęp gry) | 1, 55 |
| `common/world/triggers/cscamera.h` | j.w. | 1, 55 |
| `common/game/gamescript.cpp` | obsługa skryptów NB (Copilot) + stoły rzemieślnicze, `Menu_ReadInt/WriteInt`, domyślne opcje, pętle pluginu (`PerFrame`, `B_GetTime`), komunikaty na ekranie (`AI_PrintQuest`…), opisy przedmiotów w ekwipunku, wagi przedmiotów (`RX_SetWeightValueExt`), wygląd komunikatu o wpisie do dziennika, talenty i animacje walki NPC (`RX_ApplyProperOverlaysForNpc`), bandyci wrodzy bohaterowi (stosunek gildii), `Npc_GetGuildAttitude`, NPC dobywają broni w walce (`RX_SelectWeaponNpc`), towarzysz nie czeka po zmianie stanu, szanse trafienia NPC na starych zapisach, wybór broni NPC jak w pluginie (łuk najpierw), zdejmowanie nakładek broni bohatera (`Mdl_*Overlay*_NoUpdate`), style walki NPC wg gildii, wytrzymałość NB (animacje biegu/pływania, `Npc_GetWaterLevel`, paski `Bar_*`), poziom animacji broni bohatera według talentu, komunikaty NB (`AI_PrintItemInfo`, `AI_PrintClr`…), paczka 1 brakujących funkcji (`Wld_PlayEffect_Vec`, `Npc_ClearInv`, `Vob_GetDistToVob`…), paczka 2 z log.txt (`StrCmp`, `GetWalkModeString`, `Hlp_GetHeroStatus`, regeneracja NPC), paczka 3 (notatki i książki NB, `RX_CallOnDead` – funkcje śmierci postaci, `RX_SetDialogNpc`), rozszerzony log (skąd brakujące funkcje, `[ctx]`, podsumowanie `NB missing`, mniej powtórzeń), paczka 4 (fragmenty świata `RX_InsertVobTree` z zapisem, skrzynie, zamki, triggery, towar handlarzy), zegar pluginu 500 ms (prolog), zmiana świata `Wld_ChangeLevel_AB` (teleporty NB), zbroja zostaje po `Mdl_SetVisualBody(…, NO_ARMOR)`, ognik od Vatrasa (szukanie, zbieranie, chodzenie za bohaterem), napisy NB na ekranie (`GamePaused`, liczniki efektów, karma), okno rzemiosła NB przy wszystkich stołach (przepisy, które zna bohater, tworzenie, `RX_EndCraft`); „Wróć” w menu NB (`RX_ClearCurrentDialog`); paczka 5 (pętle zadań, ukrywanie obiektów, przedmioty na kościach, zamki skrzyń, przywołańcy po śmierci); obrażenia czarów z inteligencji (`RX_DamageCalcRunes`); manekiny treningowe (`nbIsTrainingDummy`); trolle rzucają kamieniami (`nbTrollTick`); licznik przywołańców (`nbSummonCountTick`); przywołańcy: rozkaz ataku, tryb, odesłanie, teleport z bohaterem (`nbSummonsAttack`, `nbResetSummons`, `nbSummonsFollowTick`); zmiana celu w walce (`RX_CanReAttack`, `RX_SetReAttackNpc`…) (bez bohatera); usuwanie pustych ciał (`nbBodiesTick`); wydobywanie rudy (`RX_InteractMobOre`); kolizja materiałów, `RX_MoverIsActive`, `RX_OnEnterLoc_*`; `RX_CheckNpcStandsOnVob`; `Vob_GetDistToVob` także do obiektów; pomijanie scen (`RX_IsCutsceneActive`, `Script_Use_Button_Cutscene`), `Wld_InsertNpc` na ruchomym obiekcie; Fregyal obok bohatera na łodzi (`AI_Teleport` na Zachodnim Wybrzeżu); zasięg rysowania postaci co 250 ms; `Npc_ChangeAttributeExtra` (młot Adanosa na kamiennych strażników); paczka 6 (`RX_CopyAllDialogsNpc`, `RX_CreateSpellRune`, `GetFocusNpc`, `Npc_ValidFocusNpc`); wygląd broni od Gallahada (`RX_ApplyVisualGallahad`); `RX_RemoveVobTree` chowa obiekty świata; nakładki zdejmowane po nazwie (`Mdl_RemoveOverlayMds`, `Mdl_HasOverlayMds`); koniec sprintu bez wytrzymałości, `Npc_GetWaterLevel` przy pływaniu; ognik bez kolizji i bez spadania w locie; paczka 7 (`RX_AST_Blessing`, `RX_SelectSpell`); statystyki potworów według poziomu trudności (`RX_AST_UpdateMonsterStats` co sekundę) | 0, 2, 4, 5, 6, 9, 12, 13, 19, 23, 24, 25, 27, 30, 31, 32, 35, 41, 43, 45, 49, 50, 51, 52, 56, 57, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 76, 77, 79, 80, 81, 82, 83, 86, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 136, 138, 139, 140, 142, 143, 145, 146, 148, 149, 151, 153, 154, 157, 158, 161, 163, 164, 165, 172, 173, 175, 176, 177, 181, 184, 185, 189, 190, 192, 193, 196, 198, 204 |
| `common/game/gamescript.h` | okno rzemiosła NB: `nbCraftList`, `nbCraftCreate`, `nbCraftEnd`, `nbCraftSkill`, `nbCraftAvailable`, `nbCraftText`; przywołańcy (`nbHasSummons`, `nbSummonsPassive`, `nbSummonsAttack`, `nbSummonsToggleMode`, `nbResetSummons`); `nbMonsterStatsTick` | 70, 71, 131, 149, 176, 181, 185, 189, 190, 192, 193, 198 |
| `android/java/org/opengothic/app/LauncherActivity.java` | launcher: czwarta karta „Gothic II – New Balance” z własnym folderem (bez pobierania ze Steam) | 195 |
| `common/physics/physicvbo.cpp` (+ `.h`) | wyłączanie kolizji materiału (`RX_SetMaterialCollision`): trójkąty materiału w świecie i obiektach; materiały `NB_MAT_*` bez kolizji w danych – wyłączone, do włączenia skryptem | 139, 141 |
| `common/physics/dynamicworld.cpp` | materiały wyłączone przez NB pominięte przy budowie kolizji świata; materiały `NB_MAT_*` w kolizji świata; postać bez kolizji (ognik) nie zatrzymuje się na innych postaciach | 139, 141, 175 |
| `common/world/objects/interactive.h` | nazwa obiektu żyły (zapas rudy) i funkcja stanu dla wydobywania NB; czy skrzynia nadaje się do szybkiego łupu; miejsca kopania (`HIDDENFIND`) w celowniku bez promienia, nazwa „Miejsce do kopania” (z `interactive.cpp`); obiekty wpuszczone w ziemię (sarkofagi) widoczne dla celownika | 138, 159, 165, 168 |
| `common/world/triggers/abstracttrigger.h` | czy punkt stoi na obiekcie (`nbStandsOn`) – postać na łodzi Fregyala | 140 |
| `common/world/triggers/movetrigger.cpp` (+ `.h`) | postacie stojące na ruchomym obiekcie (łódź, winda) jadą razem z nim; towarzysze bohatera wsiadają z nim | 141, 144, 145 |
| `common/world/waymatrix.cpp` (+ `.h`) | punkty z części świata dokładanych przez skrypty: bezpieczne w pamięci i w indeksach | 142 |
| `common/gothic.cpp` | zasięg obiektów świata równy zasięgowi terenu przy New Balance (`settingsObjectDistance`) | 147 |
| `common/world/world.cpp` (+ `.h`) | zasięg rysowania postaci (`nbNpcRangeTick`) | 146 |
| `common/graphics/mdlvisual.cpp` (+ `.h`), `meshobjects.cpp` (+ `.h`), `visualobjects.cpp` (+ `.h`) | ukrywanie całej postaci (ciało, głowa, zbroja, broń) poza zasięgiem rysowania; efekty przedmiotów na broniach (`setItemFx`), cząsteczki Gallahada (`NB_VIS_*`); nakładka po nazwie (`hasOverlay`) | 146, 150, 151, 172 |
| `common/graphics/effect.cpp` (+ `.h`), `common/graphics/pfx/pfxbucket.cpp`, `common/world/objects/pfxemitter.cpp`, `common/world/objects/item.cpp` (+ `.h`), `common/game/inventory.cpp` | efekty przedmiotów (`C_Item.effect`) na broniach i w świecie | 150, 179 |
| `common/world/triggers/movetrigger.cpp` (+ `.h`) | windy z cząsteczkami jako wyglądem (czerwony dym `EVT_SMOKE_*`) | 153 |
| `common/game/damagecalculator.cpp` | siła liczona raz przy kilku rodzajach obrażeń (golemy, trolle); cios przyzwańca z własnymi obrażeniami bez siły, jak na PC | 155, 186, 203 |
| `mods/nb-wisp/index.js` | ognik nie jest usuwany i przywoływany przy 40 m (tylko przy 100 m); odesłany na czas przemiany bohatera | 117–120, 154, 182 |
| `common/graphics/drawcommands.cpp`, `shader/materials/visibility_pass.comp` | zasięg rysowania postaci w przebiegu widoczności (109), od 146 wyłączony – zasięg liczy `World::nbNpcRangeTick` | 109, 146 |
| `common/physics/physicmeshshape.cpp` | siatka kolizji obiektu także z samymi materiałami `NB_MAT_*` | 141 |
| `common/game/gamescript.cpp` (+ `.h`) | fazy bossów: `AI_OnTick_`/`AI_OnHit_`/`AI_OnGetDamage_`/`AI_OnBreak_` po nazwie instancji, licznik walki `aivar[95]`; `Wld_InsertNpcAndAttackEnemy`; `RX_IsWaiting`, `RX_Stop_AI_Wait`, `UpdateSpeed`, `AI_*_Summoned`; obrażenia w czasie (`RX_ApplyDotDamage*`, napisy po lewej), `RX_CallDamage` przy ciosach bohatera, splash | 100, 101, 106 |
| `common/world/objects/npc.cpp` | zmienne NB postaci, AI potworów (Copilot) + bohater nie wstaje z łóżka przy śnie, efekty krwi przy trafieniu, prędkość ruchu przeskalowanych istot, potwory nie odskakują bez przerwy, zwierzęta odskakują po 2 trafieniach, potwory atakują w tempie Gothic, hełm w węźle `ZS_HELMET` obok zbroi, warunki przedmiotów NB (wytrzymałość, łuk, kusza, tarcza), ślad AI potworów (`NB ai`) tylko przy zmianie stanu, animacja `AI_PlayAni` nie blokuje kolejki AI bez końca (goblin stał w miejscu), `AI_GotoItem` bez przedmiotu nie wywala gry; log stanu postaci przy próbie rozmowy; `hasOverlay` po nazwie; postać bez kolizji (`nbSetNoCollision` w `npc.h`, ognik); ślad AI bossów w logu; bez zatoczenia przy `bodystate_interruptable_override` | 0, 0a, 7, 17, 20, 21, 22, 26, 28, 33, 34, 39, 46, 52, 54, 63, 163, 164, 172, 175, 187, 188, 197, 198, 199, 201, 204 |
| `common/ui/documentmenu.cpp` | okno notatek i książek NB (tekstura pluginu, tekst w kolumnach stron, kolejne strony przyciskiem SKIP/Escape) | 51 |
| `common/ui/documentmenu.h` | j.w. (`Show::nb`) | 51 |
| `common/graphics/sky/sky.cpp` | jasna noc do testów: światło i niebo między 8:00 a 18:00 (`[NB_OG] bNightAsDay`) | 53 |
| `common/game/inventory.cpp` | osobne miejsce na hełm (zbroja z `WEAR_HEAD`), stare zapisy: hełm z miejsca zbroi na swoje; pierścienie teleportacyjne używane, nie zakładane (z `item.cpp/.h`) | 39, 166 |
| `common/game/inventory.h` | j.w. (`currentHelmet`, `isHelmet`) | 39 |
| `common/mainwindow.cpp` | sprint NB: klawisz `~` i przycisk na nakładce wywołują `B_Hotkey_Sprint`; pasek wytrzymałości NB, paski w miejscach i rozmiarach z edytora HUD, okno sprzedaży NB (widżet, tryb menu nakładki, dotyk i klawisze), okno alchemii NB (to samo); Android: swapchain od nowa po powrocie z tła; skrzynia w fokusie dla funkcji zamków NB (`nbSetFocusMob`); kolory nazw w fokusie (`focusColor`); zakładka dokumentów z nieprzeczytanym dokumentem (`hud.inventoryNew`); panel przywołańców (ATAK, TRYB, RESET); puszczanie ruchu bez puszczenia klawisza; `GameScript::nbUiBusy` (okna, w których ciała nie znikają); pasek doświadczenia (`bar_xp`); kolor żyły rudy w celowniku; przycisk „POMIŃ”, klawisze G/R; szybki łup (przytrzymanie przycisku akcji, skrzynie przez zwykłe otwarcie – 167) i log miejsc bez celownika | 40, 41, 42, 45, 49, 68, 69, 70, 74, 79, 81, 121, 131, 133, 136, 137, 138, 143, 149, 159, 163, 167, 176, 199, 203, 204 |
| `common/ui/mobilecontrolsmodel.cpp` | przycisk sprintu na nakładce (obok chodu), da się go przesuwać i ukryć w edytorze HUD; paski życia/wytrzymałości/many w edytorze HUD; ustawienie „Peaceful targets”; przycisk szybkiego ataku (`Special::QuickAttack`, id `quickattack`); ustawienie „Acrobatics”; przycisk przywołańców i trzy opcje obok niego; pasek doświadczenia w edytorze HUD; przycisk „POMIŃ” | 40, 42, 78, 85, 86, 131, 137, 143, 176, 194, 199, 203 |
| `common/ui/mobilecontrolsmodel.h` | j.w. (`Special::Sprint`, `sprint()`, id `sprint`, `hudBarRect`, id `bar_*`, `SettingPeaceful`) (`Special::Summons…`, `summons()`) (`bar_xp`, `xpBar()`) (`Special::SkipScene`) | 40, 42, 78, 131, 137, 143, 176, 179, 194, 199, 203 |
| `common/utils/androidinput.cpp` (+ `.h`) | ikona sprintu, podświetlenie, gdy sprint włączony; ramki pasków w edytorze HUD; ustawienie `peacefulfocus` (wersja ustawień 8); szybki atak – sekwencja klawiszy A/D, W, Ctrl, ikona `quick_attack.png`; ustawienie `acrobatic` (wersja ustawień 9); QUICK = „< STEP” + ACTION naciśnięte razem; znacznik nieprzeczytanego dokumentu na zakładce dokumentów; ikona przywołańców, przyciski ATAK / AKTYWNE-PASYWNE / RESET; przytrzymanie przycisku akcji na zwłokach lub skrzyni – szybki łup | 40, 42, 78, 85, 86, 97, 121, 131, 159, 167, 176, 179, 194, 199, 203 |
| `common/world/worldobjects.cpp` (+ `.h`) | ustawienie „Target peaceful creatures: OFF” – niewrogie stworzenia (ognik, przywołańce, owce, króliki, żółwie) poza celowaniem gracza; `findLockedChest` – zamknięte skrzynie w fokusie z bronią/runą; `tickHoming` pocisków | 78, 80, 84 |
| `common/gothic.cpp` (+ `.h`) | muzyka walki z bossem NB (`playBossMusic`, `stopBossMusic`, `tickBossMusic`) | 82 |
| `common/world/objects/npc.cpp` | `takeDamage` → `nbBeforeHit`/`nbOnDamage` (muzyka bossów, zachwianie, obalenie NB); `commitSpell` – pocisk czaru gracza leci za celem; `resetAttack`, `nbQuickSpeed` (szybki atak); `RX_CallDamage` i splash w `takeDamage` (`nbSplashPct`); `setItemFx` (efekty przedmiotów na broniach); reakcja na trafienie tylko od celu (`nbReactToHit`) | 82, 83, 84, 95, 98, 106, 150, 152 |
| `common/world/bullet.cpp` (+ `.h`) | `setHoming`/`tickHoming` – pocisk czaru gracza skręca do celu; z bliska trafia od razu | 84, 102 |
| `common/world/world.cpp` | fokus: zamknięta skrzynia z bronią w ręku (siła) albo z runą otwierania (magia); ukrywanie obiektów z częściami (`setVobVisualTree`); pocisk z punktu modelu (`shootBulletFrom` – kamień trolla) | 80, 125, 128 |
| `common/world/objects/vob.cpp` (+ `.h`) | `setVobVisualTree` – widoczność obiektu i jego części (`RX_ToggleVobVisible`) | 125 |
| `common/game/definitions/spelldefinitions.cpp` (+ `.h`) | `refresh` – dane czarów tworzone od nowa po zmianie obrażeń (`SPL_DAMAGE_*`) | 126 |
| `common/game/inventory.cpp` (+ `.h`) | bonusy przedmiotów `change_atr` przy zakładaniu i zdejmowaniu (`applyItemEffects`) | 127 |
| `common/game/gamesession.cpp` | wpis `game/item_effects` – bonusy przedmiotów ze starszych zapisów dodawane raz; przywołańcy odsyłani przy zmianie świata (ZEN) i po wczytaniu zapisu; pociski w locie usuwane przed zmianą świata; `RX_OnChangeLevel_Begin/End` przy zmianie świata, punkt przybycia ze skryptów wejścia | 127, 131, 154, 157, 171 |
| `common/world/world.h` | numer obiektu świata (`serialNumber`) – mody widzą wczytanie zapisu tego samego świata | 117 |
| `common/game/gamesession.h` | numer gry (`serialNumber`) – nowa gra/wczytany zapis a zmiana świata; nieprzeczytane dokumenty (`nbNewDocs`) | 117, 122 |
| `common/game/gamesession.cpp` | zapis i wczytanie nieprzeczytanych dokumentów (wpis `game/nb_newdocs`) | 122 |
| `common/scripting/game/game_events.cpp` | zdarzenie modów `world:load` (`{world, newGame}`) | 117 |
| `common/scripting/game/game_npcs.cpp` | `npcs.all(instance)` – wszystkie postacie danej instancji w świecie | 117 |
| `docs/MODS.md` | opis `world:load` i `npcs.all` | 117 |
| `common/game/playercontrol.cpp` (+ `.h`) | przycisk akcji na zamkniętej skrzyni w fokusie: otwieranie zamka NB (siła/magia/wytrychy); `quickCombo` – szybki atak czytany jak sterowanie Gothic 1, W resetuje pozycję po ciosie; `quickAttack` – szybkie ciosy do przodu z resetem (`QuickHits`, `QuickTime`, `QuickSpeed` w `.h`), jedno dotknięcie = jeden cios, tempo zależne od stylu walki (szabla/rapier, `quickMeleeSpeed`); `nbReleaseStuckMove`; szybki atak zatrzymuje bieg bez gałki, log `[hero-ai]` | 80, 94, 95, 98, 114, 115, 133, 134, 135, 194 |
| `common/utils/androidinput.h` | j.w. (`IconSprint`, `hud.sprint`, `hudPlaces`, `stamina`) (`hud.summonPassive`, `summons`); `held()` | 40, 42, 131, 133, 176, 194 |
| `common/gothic.cpp` | ekran wczytywania: obrazki moda `LOADING_<ŚWIAT>_<numer>` | 3, 3a |
| `common/mainwindow.cpp` | ekran wczytywania: świat z nagłówka save'a, czarne tło zamiast białego, pasek wczytywania dla tekstur HD; cisza podczas wczytywania gry (muzyka, dźwięki starego świata) | 3, 3a, 14, 174 |
| `common/ui/gamemenu.cpp` | strona „Ustawienia gry” z przełącznikami NB, okno statystyk postaci (B) z 5 zakładkami, zakładki dziennika (zakończone, bestiariusz), przełącznik „Jasna noc (do testów)” | 5, 8, 10, 15, 53 |
| `common/gamemusic.cpp` (+ `.h`) | muzyka: segment motywu ładowany w wątku gry, nie w wątku dźwięku; wyższy priorytet wątku dźwięku na Androidzie, muzyka ładowana w osobnym wątku dla obu silników muzyki (zacinanie na ekranie wczytywania); wyciszenie muzyki gry przez skrypty NB (`setScriptMute`), którego nie zdejmuje zapis opcji; wyciszenie na czas wczytywania (`setLoadMute`); muzyka BassMusicController (mp3 z `BASSMUSICCONTROLLER_*.D`) | 36, 37, 38, 116, 174, 183 |
| `common/game/definitions/musicdefinitions.cpp` (+ `.h`) | motywy BassMusicController z `System/Autorun/BASSMUSICCONTROLLER_*.D` zastępują motywy z `Music.dat` o tej samej nazwie | 183 |
| `CMakeLists.txt` | `lib/dr_libs` (dekoder mp3) | 183 |
| `game/main.cpp` | dłuższy bufor dźwięku OpenAL (`alsoft.conf`, 6 × 20 ms); powtarzany błąd ZenKita „Accessing member” raz w logu, potem licznik | 37, 75, 200, 202, 203 |
| `common/utils/crashlog.cpp` (+ `.h`) | awaria trafia też do `log.txt` (stos wywołań, ukryte linie, podsumowanie problemów) | 200 |
| `common/game/playercontrol.cpp` | „lepki” focus przeciwnika w walce wręcz (bohater sam się do niego obraca) | 18 |
| `common/ui/gamemenu.h` | nowe funkcje ekranu statystyk i zakładek dziennika | 8, 10, 15 |
| `patches/tempest-opengothic.patch` (+ `patches/README.md`) | Tempest: gra nie zamyka się, gdy idzie w tło (wiadomość, ekran główny) – nowa powierzchnia Vulkana po powrocie, swapchain niszczony przy wyjściu w tło | 73, 74 |
| `common/ui/dialogmenu.cpp` / `.h` | odpowiedzi w dialogach na pergaminie jak na PC (dotyk, przewijanie); menu bohatera bez kamery dialogu | 72 |
| `common/ui/inventorymenu.cpp` | okienko opisu przedmiotu (dotyk): wiersze nie nachodzą na nazwę i nie wychodzą poza okienko; w NB nagłówek „Złoto: N”; nowy wygląd ekwipunku: ramki, siatka 9 kolumn, kategorie w rzędzie, karta przedmiotu na pergaminie z przyciskami; tekstury Hedin's Advanced Inventory z `Hedin_Union_Adv_Inventory.vdf` w `Data`, gdy jest; znacznik „NOWY”: nieprzeczytane dokumenty (w zapisie, znika po przeczytaniu) i inne nowe przedmioty (bez zapisu, znika po wybraniu); mniejsze napisy karty, zdobyte bonusy po użyciu, polskie nazwy zakładek | 12, 69, 99, 104, 121, 122, 123, 156, 167, 169, 170 |
| 🆕 `common/ui/nbsalesmenu.cpp` | okno sprzedaży NB (jak okno pluginu, dotykowe) | 68, 69 |
| 🆕 `common/ui/nbsalesmenu.h` | j.w. | 68, 69 |
| 🆕 `common/ui/nbcraftmenu.cpp` | okno rzemiosła NB (jak okno pluginu na PC, dotykowe) – wszystkie stoły | 70, 71 |
| 🆕 `common/ui/nbcraftmenu.h` | j.w. | 70, 71 |
| `common/mainwindow.h` | okno sprzedaży NB (`nbSales`), okno alchemii NB (`nbCraft`) (`summonMenuOpen`) | 68, 70, 131 |
| `common/gothic.h` | sygnały `onOpenPlayerSales` (okno sprzedaży NB) i `onOpenCraft` (okno alchemii NB) | 68, 69, 70 |
| `common/graphics/mesh/animationsolver.cpp` (+ `.h`) | nakładki zdejmowane i sprawdzane po nazwie (bez wczytywania, bez wielkości liter), wszystkie o tej nazwie | 172, 173 |
| `common/resources.cpp` | pamięć modeli i animacji po nazwie wielkimi literami (jeden plik wczytany raz) | 172 |
| `android/java/org/opengothic/app/SaveExport.java`, `SaveImport.java` | okno zapisów Spine (borsuczyna): zapisy także z folderu gry wybranego w launcherze (`GameRoots.linked`) i wgrywanie do niego | 180 |

---

## Mody JS (nie trzeba budować APK)

Mody w JavaScripcie dla systemu modów projektu borsuczyny (`docs/MODS.md`). Instalacja na telefonie: launcher → **Mods** → **Add a mod (.zip)**, potem włącz przełącznik.

| Mod | Plik do zainstalowania | Co robi | Poprawka |
|---|---|---|---|
| `mods/nb-wisp` | `mods/nb-wisp.zip` (wersja 1.3.2) | wędrujący ognik od Vatrasa: Shift+F5, przycisk w zakładce „Ognik” menu modów albo komenda `ognik` (`ognik on`/`ognik off`); ognik na jedną sesję gry (pierwsze wczytanie po włączeniu gry bez ognika), przywołany wraca po śmierci i wczytaniu, nigdy więcej niż jeden; znika na czas przemiany bohatera i wraca po niej | 11, 117–120, 154, 182 |
| `mods/test-umiejetnosci-broni` | `index.js` do folderu moda „Test umiejętności broni” (podmienić) | test poziomów broni pod NB: procent trafienia jak u nauczyciela, animacje zakłada skrypt NB | 48 |

---

## Pliki potrzebne, których tu nie ma

| Plik | Dlaczego |
|---|---|
| `common/world/objects/npc.h` | `npc.cpp` potrzebuje pól NB (`newBalanceNpcVars`, `newBalanceGetNpcVar`/`newBalanceSetNpcVar`). Wersja z tymi polami jest u autora gałęzi (zmiany Copilota). Bez niej `npc.cpp` się nie skompiluje., 199, 201, 204 |

---

## Zasada

Przy każdej poprawce w `CHANGELOG.md` jest napisane, które pliki zostały **zmienione**, a które są **nowe**. Każdy nowy plik trafia też do tabelki „Nowe pliki” na górze tej strony, razem z miejscem, gdzie go wrzucić.
