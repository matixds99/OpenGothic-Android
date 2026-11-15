# Log poprawek – OpenGothic + Gothic II: New Balance

Repo: fork `matixds99/OpenGothic-Android`, gałąź `touch-ui` (na bazie `borsuczyna/opengothic-android`).
Lista plików (co podmienić, co dodać) jest w [docs/NEW_BALANCE.md](docs/NEW_BALANCE.md).

**Status:** ✅ sprawdzone w grze · 🧪 czeka na test · ⏸ wycofane / odłożone

**Numery:** każda poprawka ma swój numer, a komentarze w kodzie (`NB fix N`) i tabela plików odwołują się do tych numerów. Poprawki o tym samym temacie są tu połączone w jeden wpis – wtedy w nagłówku stoi kilka numerów (np. „36–38”). Nowe poprawki dostają kolejny wolny numer (następny: **68**; numery 61, 63–67 są w opisie ognika – poprawka 11).

**🆕** oznacza plik, którego nie ma w projekcie borsuczyny i który trzeba dodać.

---

## Spis

1. [Podstawy: skrypty NB, opcje, pętle pluginu, log](#1-podstawy-skrypty-nb-opcje-pętle-pluginu-log)
2. [Brakujące funkcje pluginu (paczki)](#2-brakujące-funkcje-pluginu-paczki)
3. [Prolog, kamery, wczytywanie, zapisy](#3-prolog-kamery-wczytywanie-zapisy)
4. [Interfejs: menu, statystyki, dziennik, opisy, napisy](#4-interfejs-menu-statystyki-dziennik-opisy-napisy)
5. [Bohater: sprint, wytrzymałość, paski, animacje broni](#5-bohater-sprint-wytrzymałość-paski-animacje-broni)
6. [Walka i AI postaci](#6-walka-i-ai-postaci)
7. [Świat: rzemiosło, zbroje, teleport, ognik](#7-świat-rzemiosło-zbroje-teleport-ognik)
8. [Dźwięk](#8-dźwięk)
9. [Do zrobienia](#9-do-zrobienia)

---

## 1. Podstawy: skrypty NB, opcje, pętle pluginu, log

### 0. Obsługa skryptów New Balance (Copilot, przed tym logiem) – ✅
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`
- Zastępniki funkcji `RX_*` z pluginu Union (zmienne NB postaci, maski, teksty, losowanie, wywołania zwrotne). Dzięki nim działają teksty i cykle skryptów.
- Zmienne NB postaci (`RX_GetNpcVar` / `RX_SetNpcVar`) są zapisywane też w save'ach.

### 4. Opcje moda z Gothic.ini – ✅ (12.11.2026, sprawdzone na `bSkipLogo=1`)
- **Plik:** `common/game/gamescript.cpp` (`Menu_ReadInt`, `Menu_WriteInt`)
- **Przyczyna:** skrypty NB czytają ok. 70 opcji z `Gothic.ini`, sekcja `[AST]` (np. `bSkipLogo`, `bCanSaveGame`, `bShowKarma`). `Menu_ReadInt` / `Menu_WriteInt` dostarczał plugin Union – każda opcja dawała 0, a zapisy przepadały.
- **Poprawka:** obie funkcje czytają i zapisują ustawienia OpenGothic (`Gothic.ini`).

### 5, 44. Menu „Ustawienia gry” i domyślne opcje pluginu – ✅ (12.11.2026)
- **Pliki:** `common/ui/gamemenu.cpp`, `common/game/gamescript.cpp`, 🆕 `common/game/nboptions.h`
- **Przyczyna:** w `MENU.DAT` moda ta strona to pusta rama (`MENU_OPT_CONTROLS_NEWOPTIONS`), wypełnia ją plugin. Z kodu pluginu odczytana pełna lista: ok. 140 opcji, ich klucze, typy i wartości domyślne.
- **Poprawka:**
  - strona pokazuje przełączniki, które czytają same skrypty NB (nazwy i „tak/nie” ze skryptów gry, po polsku). Reszty opcji pluginu nie ma, bo sterują funkcjami samego Uniona;
  - domyślne wartości opcji NB (`[AST]`, `[BELIARS_RAGE]`) są dopisywane do `Gothic.ini`, gdy ich tam nie ma – jak robi silnik z Unionem (np. `bCanSaveLeg=1`, `bToggleHardBosses=1`);
  - **44:** `IsEnabledDexSwordAnims=1` – opcja czytana przez plugin poza menu; bez niej szable i inne bronie zręcznościowe dostawały animacje zwykłych mieczy.
- **Przełączniki** (sprawdzone w skryptach NB):

  | Opcja | Co robi skrypt | Działa? |
  |---|---|---|
  | Pomiń ekran powitalny (`bSkipLogo`) | pomija prolog | ✅ |
  | Status karmy u bogów (`bShowKarma`) | ikonki karmy Innosa/Beliara | ✅ (poprawka 62) |
  | Lista efektów (`bShowBuffInfo`, „SHOW_BUFFS”) | liczniki mikstur i błogosławieństw | ✅ (poprawka 62) |
  | Głód/pragnienie/zmęczenie (`bHFTStatus`) | potrzeby bohatera | ✅ powinno |
  | Nazwy lokacji (`bShowNotice`) | tablice z nazwami miejsc | ✅ powinno |
  | Skalowanie postaci od siły (`bScaleStr`) | `Mdl_SetModelScale` bohatera | ✅ powinno |
  | Ekran śmierci (`bShowDeadScreen`) | dźwięk i ekran po śmierci | ✅ powinno |
  | Efekt zatrucia (`bShowPoisonHero`) | `SPELLFX_UNDEAD_SCREENBLEND` | ✅ powinno |
  | Efekt ataków Paladyna (`bShowEffectPalUndeadProc`) | efekt przy obrażeniach nieumarłym | ✅ powinno |
  | Trudniejsi bossowie (`bToggleHardBosses`) | umiejętności bossów w AI | ✅ powinno |
  | Okrzyki bohatera (`bToggleHeroVoice`) | dźwięki bohatera w walce | ✅ powinno |
  | Gniew Beliara (5 opcji) | zmienne trybu Gniewu Beliara | ✅ powinno |
  | Zapis w dowolnym miejscu (`bCanSaveLeg`) | blokada zapisu w NB | ❓ OpenGothic chyba nie blokuje zapisu |
  | Ukryj sakiewki NPC (`bHidePocketsNpc`) | sakiewki na postaciach | ❓ zależy od slotów |
  | Mieszek / torba (`bShowPocket`, `bShowBag`) | `EXT_PutInSlot` / `ActivateZSSlot` | ❌ jeszcze nie ma |
  | Jasna noc (do testów) | tylko OpenGothic, poprawka 53 | ✅ |

- Usunięte ze strony: `AutoAIM` (skrypty same ustawiają go na 0) i „Wytrzymałość przy otwartym ekwipunku” (funkcja ekwipunku Uniona).

### 6, 57. Pętle pluginu: `PerFrame`, `RX_Main_CycleTrigger`, `B_GetTime` – ✅ / tempo prologu 🧪 (12. i 15.11.2026)
- **Plik:** `common/game/gamescript.cpp` (`GameScript::tick`)
- **Przyczyna:** wiele zdarzeń świata (np. wpis „Kamienie runiczne” przy wieży Xardasa) daje skrypt `B_GetTime`, a w NB woła go plugin po nazwie – w OpenGothic nie wołał go nikt.
- **Poprawka (zegar pluginu, UNION_ABI.DLL 0x10261040):**
  - co klatkę `PerFrame` (z `RX_FrameTime` = czas klatki w ms);
  - co 500 ms na zmianę `RX_Main_CycleTrigger` i `B_GetTime` – każde raz na sekundę; przy `PrologCount==0` (pierwsza chwila nowej gry) co 16 ms;
  - co 5 s `B_CheckMenuOption` (odczyt opcji moda).
- **57 – prolog (`bSkipLogo=0`):** wcześniej zegar tykał co 1000 ms (każda funkcja co 2 s), więc prolog trwał ~2 minuty i kamera wieży (60 s) kończyła się w połowie mowy Xardasa. `Hero_Poisoned` zwiększa `PrologCount` raz na wywołanie: 13 – mowa Xardasa, 53 – napis „Khorinis – wieża Xardasa”, 58 – koniec prologu. Teraz tempo jak w oryginale (~58 s). **Do sprawdzenia:** czy prolog mieści się w kamerze; jeśli coś w grze dzieje się „za szybko”, to znak, że wcześniej było za wolno.

### 52. Rozszerzony log – ✅ (14.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`
- **Brakujące funkcje:** `not implemented call [NAZWA] from FUNKCJA_SKRYPTU (argumenty)`; co 5 minut i przy zapisie `NB missing (…)` – ile razy każda była wołana.
- **`[ctx]` – co robił gracz:** opcja dialogu, użyty przedmiot, wpis w dzienniku, użyty obiekt, śmierć postaci i zabójca, notatka/książka, zapis, zmiana świata, ognik.
- **Mniej śmieci:** ta sama linia `RX_WriteLog` tylko 3 razy (potem przy 100/1000), `Mdl_*OverlayMds` tylko przy błędzie, `NB ai` tylko przy zmianie stanu lub celu potwora.

### 53. Jasna noc (do testów) – ✅ (14.11.2026)
- **Pliki:** `common/graphics/sky/sky.cpp`, `common/game/nboptions.h`, `common/ui/gamemenu.cpp`
- Przełącznik **„Jasna noc (do testów)”** na końcu strony „Ustawienia gry” (`[NB_OG] bNightAsDay=1`): światło i niebo zostają między 8:00 a 18:00, czas gry płynie normalnie (NPC śpią, nocne zdarzenia są jak w nocy). O północy słońce przeskakuje z 18:00 na 8:00 – przy testach bez znaczenia.

---

## 2. Brakujące funkcje pluginu (paczki)

Funkcje zewnętrzne z UNION_ABI.DLL wybrane według liczby wywołań w GOTHIC.DAT (735 funkcji zewnętrznych, ok. 230 wołanych przez skrypty i nieobecnych w OpenGothic) i według logów.

### 49. Paczka 1: najczęściej wołane funkcje + ukryte opcje pluginu – 🧪 (14.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/nbbars.h`, `common/game/nboptions.h`, `common/mainwindow.cpp`
- `Wld_PlayEffect_Vec` – czary potworów (pajęcza sieć, lodowa włócznia, ognista kula, kwas): efekt od rzucającego do celu i obrażenia minus ochrona (wcześniej bez skutku).
- `Npc_ClearInv`, `Vob_GetDistToVob`, `Wld_GetTimeHour/Min`, `Hlp_GetNpcByInstInRadius`, `Npc_GetItemById`, `Hlp_InventoryIsOpen`, `StrCmp`, `RX_MusicOn/Off/ToggleMusic`, `CreateInvItemsByString`, `Npc_EquipItem`, `Npc_UnequipItem`, `Itm_OneHanded`, `Npc_CanSeeNpc2`.
- **Ukryte opcje pluginu** (trudność nowej gry, wybierana w oknie pluginu): `bOptionLP` 10 (punkty nauki za poziom!), `bOptionHealth` 3, `bOptionMonsterExp` 2, `bOptionMonsterHealth` 3 (0 = potwory z 25% życia), `bOptionMonsterPower` 2, `bOptionMonsterProt` 2, `FULLSNC_MENU_OPTION` 19, `bToggleQuickLootMessage` 1.

### 50. Paczka 2: funkcje z log.txt – 🧪 (14.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- `StrCmp` – w NB TRUE dla takich samych napisów (bez wielkości liter), nie 0 jak w C;
- `GetWalkModeString` (WALK/RUN/SNEAK/SWIM/DIVE), `Hlp_GetHeroStatus` (0 spokój, 1 zagrożenie, 2 walka);
- `RX_RestoreNpcHealthSlow/Stop` – gdy bohater ucieknie (ponad 2000), NPC chowa broń i odzyskuje 2% życia na sekundę;
- wyciszone (zawsze 0, jak w tej wersji moda): `IsNewLegFeatures`, `RX_IsLegAst`, `RX_IsMultActive`, `RX_RoadOfTrialsActive`, `RX_IsHeroInsideOtherNpc`, `RX_Mult_SetPartyNpc`, `Mdl_SetHeroAniFps`, `InfoStatAdd`.

### 51. Paczka 3: notatki i książki, śmierć postaci, dialogi przypisywane NPC – 🧪 (14.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/ui/documentmenu.cpp`, `common/ui/documentmenu.h`
- **Notatki i książki** (`RX_OpenNote`, `RX_ToggleBookManagerWithSize`, `RX_ToggleBookManager`): 147 listów i 52 książki/pulpity otwierały się bez niczego. Teraz tekstura NB, rozmiar z `RX_ScrollSizeX/Y`, tekst w miejscach stron (książka – dwie kolumny), czcionka książek Gothica, znaczniki `[POS=…]`, `[COL=…]` usunięte. Długi tekst – kolejne strony: **SKIP / Escape** przewraca stronę, na ostatniej zamyka; numer strony pod książką.
- **Śmierć postaci** (`RX_CallOnDead`): woła `AI_OnDead_<instancja>` (310 funkcji – zadania, bossowie) i `AI_OnDead_Common`, raz na postać; po śmierci handlarza `RX_AST_RemoveTradeGoods`.
- **Dialog dla innego NPC** (`RX_SetDialogNpc`, 67 wywołań): poker (Kardif, Orlan, Salandril…), maska śniącego u Xardasa i inne – wcześniej te opcje się nie pojawiały.
- **Inne:** `Hlp_IsDexSword`, `Game_Play3DSound`; wyciszone `UpdateItemsInfoInInv`, `RX_IsRemoteSummon`.

### 56. Paczka 4: fragmenty świata, skrzynie, zamki, triggery, towar handlarzy – 🧪 (15.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- **`RX_InsertVobTree` / `RX_RemoveVobTree`:** skrypty dokładają fragmenty świata z `VOBTREE/*.ZEN` (łóżka, skrzynie, dekoracje, przedmioty z zadań). Lista jest w zapisie (`nb/vobtrees`) i po wczytaniu wstawiana ponownie – bez przedmiotów i zawartości skrzyń (te są już w zapisie). Usuwane mogą być tylko fragmenty wstawione przez skrypty.
- `Mob_RemoveAllItems` (opróżnia skrzynię), `RX_LockChest` / `RX_LockDoor` (otwarcie/zamknięcie przez skrypt), `RX_EnableTrigger` (np. przejście na tajemniczą wyspę z kluczem Hagena), `RX_RemoveItemsOnNpcBeat` (towar zmarłego handlarza znika), `RX_IsItemCraft`, `RX_NpcIsStrafing`, `RX_IsWaiting` (zawsze 0).
- **Odłożone:** obrażenia w czasie i trucizny (`RX_ApplyDotDamage`, `RX_ApplyPoison`), przywołania (`RX_SummonNpcToTarget`, `RX_ResetSummons`), okna pluginu (handel, ryby, ostrygi), `RX_SetDoorKeyByCoords`, `RX_ToggleVobVisible`, `RX_CreateSpellRune`, `RX_SelectSpell`, `RX_Strafe` i inne.

---

## 3. Prolog, kamery, wczytywanie, zapisy

### 1, 55. Kamera na początku gry (`EVT_START_CAM`): czarny ekran, potem patrzenie w ściany – ✅ (11. i 15.11.2026)
- **Pliki:** `common/world/triggers/cscamera.cpp`, `common/world/triggers/cscamera.h`
- **1 – czarny ekran:** po pierwszym dialogu z Xardasem zostawały same napisy. Kamera miała błędne czasy klatek kluczowych (oryginał je przelicza), wychodziło dzielenie przez zero i pozycja NaN. Teraz błędne czasy są rozkładane po trasie; przy złej pozycji scena jest przerywana.
- **55 – patrzenie w ściany:** kamera (15 s, 8 punktów, bez punktów celu) leciała dobrą trasą, ale patrzyła w kierunku ruchu, czyli w ściany. Teraz kamera bez punktów celu patrzy na bohatera, gdy jest blisko (do 5000) – krąży wokół pentagramu z Xardasem i bohaterem w kadrze, jak w oryginale (sprawdzone na nagraniu). W logu `CsCamera: … lerp= keyDirections= focus=`.
- Prolog z kamerą wieży – zob. poprawka 57 w sekcji 1.

### 3, 3a, 14. Ekran wczytywania: białe tło, obrazek innego świata, gruby pasek – ✅ (12.11.2026)
- **Pliki:** `common/gothic.cpp`, `common/mainwindow.cpp`
- **Obrazek:** `LOADING.TGA` w NB to biała zaślepka – plugin losuje obrazki `LOADING_<ŚWIAT>_<numer>`. Teraz tak samo; przy wczytywaniu save'a świat jest brany z nagłówka save'a. Kolejność: losowy obrazek tego świata → obrazek świata bez numeru → losowy z dowolnego. Zaślepki mniejsze niż 64×64 pomijane, bez obrazka tło czarne. W logu `loading screen: …`.
- **Pasek (14):** NB ma tekstury paska HD 2× większe, a OpenGothic rysował je w rozmiarze tekstury z marginesami na sztywno – pasek był za gruby i wypełnienie wychodziło za ramkę. Teraz pasek w rozmiarze oryginału, marginesy skalowane (też na ekranie zapisu).

### 58. Import zapisów z PC z New Balance (Union) – ✅ (15.11.2026)
- **Pliki:** `patches/zenkit-opengothic.patch`, `common/game/originalsave.cpp`, `common/game/gamescript.cpp`, `common/game/gamescript.h`, `common/world/objects/npc.cpp`, `docs/ORYGINALNE_ZAPISY.md`, `patches/README.md`
- Postacie i przedmioty Uniona (`oCNpcEx` / `oCItemEx`) czytane jak `oCNpc` / `oCItem` (łatka ZenKit) – wcześniej import kończył się błędem „no player”.
- Ludzkie gildie NB (`GIL_SEPERATOR_HUM = 21`, gildie 16–20 też ludzkie) i `Gil_Values` brane ze skryptów – bohater z zapisu nie ślizga się i ma ludzkie animacje.
- Zakładane tylko przedmioty faktycznie założone (`ITEM_ACTIVE`), zbroja i broń po biżuterii; postacie ze spawnu (pozycja 0,0,0) wracają na swoje miejsce zamiast tłoczyć się w porcie.

---

## 4. Interfejs: menu, statystyki, dziennik, opisy, napisy

### 8, 10. Statystyki postaci (B) – okno NB z 5 zakładkami – ✅ (12.11.2026)
- **Pliki:** `common/ui/gamemenu.cpp`, `common/ui/gamemenu.h`
- **Przyczyna:** statystyki to okno pluginu; `MENU_STATUS` moda ma tylko pusty nagłówek, więc był ciemny ekran.
- **Poprawka:** ekran z zakładkami jak w pluginie. Przy każdym otwarciu woła skrypt NB `RefreshStatusMenuData` (wartości po polsku, ze skryptów):
  1. **Postać:** gildia, rozdział, poziom, doświadczenie, PN, dzień, ranga, karma, gildie złodziei/zabójców/kupców, zabici przeciwnicy, poziom trudności;
  2. **Umiejętności:** rzemiosło, górnictwo, strzały, myślistwo, gotowanie, złodziejstwo, magia, języki, retoryka, regeneracja, dary bogów;
  3. **Atrybuty:** siła, zręczność, mana, inteligencja, życie, wytrzymałość (×10 jak w NB), władanie bronią w %, tarcza, dwie bronie;
  4. **Ochrona:** sieczna, obuchowa, pociski, ogień, magia, przewrócenie, trucizny, zimno, żar;
  5. **Osiągnięcia:** pokonani bossowie (39).
- Lista przewijana palcem; język rozpoznawany też po polskich literach w tekstach gry. Opisy pozycji napisane od nowa (plugin je rysuje), wartości oryginalne.

### 9, 19, 45. Komunikaty pluginu: wpis w dzienniku i inne – ✅ (12. i 14.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- **Dziennik (9, 19):** `B_LogEntry` pokazuje komunikat przez `AI_PrintQuest` / `AI_PrintQuestComplete` / `AI_PrintQuestFail` pluginu. Teraz jak w pluginie: po lewej, na ok. 29% wysokości, tytuł czcionką `RX_QuestTitleFont` (nowy – żółty, nieudany – czerwony, zakończony – biały; zielonego OpenGothic nie ma), pod nim temat, przez 5 s.
- **Inne komunikaty (9, 45):** `AI_Print`, `AI_PrintRed`, `AI_PrintBonus`, `AI_NoticePrint`, `AI_FastPrintSkillCraft`, `AI_PrintItemInfo`, `AI_PrintClr` i pozostałe – wcześniej np. odmowa u nauczyciela („za mało punktów nauki”) była niema.

### 12, 13. Opisy przedmiotów w ekwipunku i waga – ✅ (12.11.2026)
- **Pliki:** 🆕 `common/game/nbiteminfo.h`, `common/game/gamescript.cpp`, `common/ui/inventorymenu.cpp`
- **Przyczyna:** w NB większość broni i pancerzy ma w skrypcie tylko „Wartość”, resztę dopisuje plugin w chwili pokazania przedmiotu (pola przedmiotu + teksty `RX_INV_DESC*`).
- **Poprawka – kod pluginu przepisany z UNION_ABI.DLL:**
  - broń biała: obrażenia, „Długość broni: 40 | Jednoręczna”, wymagania; łuki i kusze: obrażenia, typ, wymagania; pancerze: ochrony; pierścienie i amulety: „Ulepszenia: …”;
  - dopiski z bitów `C_ITEM.HP`: „| Broń orków” / „Broń jaszczuroczłeków”, „| Przeklęta”, „| Magiczna”, „| Naostrzona”;
  - **„Waga: N”** (broń, łuki, pancerze, hełmy, tarcze) i obsługa `RX_SetWeightValueExt` / `RX_GetWeightValue` (czytają je też skrypty);
  - okienko opisu: wiersze zmniejszają się, gdy nie mieszczą się pod nazwą, za długi wiersz dostaje mniejszą czcionkę.
- **Nie będzie:** premii z pieczęci (osobne okno pluginu).

### 15. Dziennik: puste „Zakończone zadania”, bestiariusz pod „Handel i rzemiosło” – ✅ (12.11.2026)
- **Pliki:** `common/ui/gamemenu.cpp`, `common/ui/gamemenu.h`
- NB zmienia stałe dziennika (`LOG_SUCCESS` = 4, bestiariusz jako `LOG_FAILED` = 3), a zakładki na telefonie miały kolejność z Gothica II. Teraz każda zakładka bierze to, co pokazuje, ze swojego przycisku w MENU.DAT; w NB „Zakończone” = status 2 i 4, „Bestiariusz” = status 3.

### 62. Napisy NB na ekranie: liczniki efektów, karma bogów, strzały – ✅ (15.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, 🆕 `common/game/nbtexts.h`, `common/game/nboptions.h`
- **Sprawdzone (zrzut):** „(02:10) Napój wytrzymałości”, „Ochrona na następny atak”, ikonki karmy (3 i czerwone 1 8), „Strzały: 1113”.
- **Przyczyna:** rysowały to dwie rzeczy, których tu nie było:
  - funkcja skryptów `GamePaused` (plugin woła ją co klatkę) – karma bogów (ikonki z czcionki `FONT_OLD_10_WHITE_HO`, czerwone bez dzisiejszej modlitwy), licznik strzał, głód/pragnienie/zmęczenie, ikonki stanu;
  - sam plugin (0x10254b20) – lista „(mm:ss) nazwa”: 21 zmiennych z sekundami (przyspieszenie, niewidzialność, regeneracje, odporności, serca golemów, błogosławieństwa Isgarotha…) oraz tarcza magiczna, ognista tarcza, przemiana zwojem, medytacje, aury nekromanty, zimno/upał.
- **Poprawka:**
  - `GamePaused` co 250 ms; kolor z `PrintScreenColor` (czerwony/żółty);
  - lista efektów jak w pluginie; teksty z `AB_Translation.lbr` z folderu gry (`System` lub `System/Autorun`), a bez niego – wbudowane polskie. Od 6% wysokości, co 3% (w pluginie 2,4%), żeby na telefonie nie zachodziła na karmę;
  - pokazuje się przy „informacjach na ekranie” skryptów (`MoreInfoOnScreen`, włącza rozmowa z Xardasem) i opcji `bShowBuffInfo` (w menu: „SHOW_BUFFS”);
  - `RX_PrintCutscene` wyłączone – pisało „Pomiń, wciśnij G lub R”, a pominięcie wymaga `RX_KeyPressed`, którego nie ma.
- **Jeszcze nie ma:** liczników wampira, Full SnC+, umiejętności karmy i spowolnienia czasu, „Dzień: …” w lewym górnym rogu.

---

## 5. Bohater: sprint, wytrzymałość, paski, animacje broni

### 40, 41, 42, 45. Sprint, wytrzymałość NB i paski w edytorze HUD – ✅ (14.11.2026)
- **Pliki:** 🆕 `android/assets/touch-icons/sprint.png`, 🆕 `common/game/nbbars.h`, `common/game/gamescript.cpp`, `common/mainwindow.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp/.h`
- **Sprint (40):** plugin pod `~` woła `B_Hotkey_Sprint` (włącza `SPRINT_ACTIVE` i `Humans_MageSprint.MDS`, sprawdza wytrzymałość). Teraz klawisz `~` i nowy przycisk na nakładce (podświetlony przy sprincie, tylko w NB, do przesunięcia/ukrycia w edytorze HUD).
- **Wytrzymałość (41):** `Hero_Poisoned` zabiera ją przy sprincie, skoku, wspinaczce, bloku i nurkowaniu, ale brakowało `RX_IsRunAniActive`, `RX_IsAniActive`, `Npc_IsAnimationActive`, `Npc_GetWaterLevel` i `Bar_*` – sprint nic nie kosztował, paska nie było. Teraz są, a pasek (`BAR_Stamina.tga`, przy zatruciu `BAR_Poison.tga`) jest rysowany między życiem a maną.
- **Paski w edytorze HUD (42, 45):** paski życia, wytrzymałości i many da się przesuwać, zmieniać im rozmiar (5 kroków) i ukrywać w edytorze HUD (`bar_hp`, `bar_stamina`, `bar_mana`); przy liczbach nad paskiem wytrzymałości `wartość/maks.` liczone jak w skryptach (`ATR_STAMINA` / `ATR_STAMINA_MAX*10`).

### 46. Broń z wymaganą wytrzymałością nie dawała się założyć – ✅ (14.11.2026)
- **Plik:** `common/world/objects/npc.cpp`
- Warunki przedmiotów NB wskazują też zmienne pluginu: `AIVRX_NPC_ATR_BOW` (89), `_SHIELD` (90), `_CROSSBOW` (91), `_STAMINA` (92) – 207 przedmiotów wymaga wytrzymałości. `Npc::attribute` zwracał 0 dla indeksów ≥ `ATR_MAX` („Za mało wytrzymałości…” przy 780/650). Teraz wartość to zmienna NB postaci (wytrzymałość ×10).

### 32. Bohater walczył nożem i pałką jak rapierem – ✅ (13.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- Przy zmianie broni skrypty zdejmują ok. 60 nakładek funkcją pluginu `Mdl_RemoveOverlayMds_NoUpdate`, której nie było – raz założony rapier zostawał na zawsze. Dodane `Mdl_RemoveOverlayMds_NoUpdate`, `Mdl_ApplyOverlayMds_NoUpdate`, `Mdl_HasOverlayMds`.

### 43, 47, 48. Poziom animacji broni bohatera i mod testowy – ✅ (14.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`, `mods/test-umiejetnosci-broni/index.js`
- **43:** `RX_LastAnim_1h/2h` to poziom wybrany w szybkim panelu pluginu (−1 = według talentu). Panelu nie ma, więc zostawało 0 (amator) i każda broń, także u mistrza, miała animacje amatora. Teraz po nauce talentu wraca −1 i `B_CheckMenuOption` zakłada animacje poziomu talentu.
- **47 – wyjaśnione:** „inne animacje szpady” brały się z tego, że bohater miał naprawdę 6% (poziom amatora, `Humans_Pirate`), a stary mod testowy zakładał zwykłe nakładki miecza. Szpada wojownika (`Rapier_ST2`) od 60%, mistrza (`Rapier_ST3`) od 90%. Diagnostyka `[anim] hero overlay …` i `[anim] hero attack …` w logu.
- **48:** mod „Test umiejętności broni” ustawia procent (10/30/60/90%) przez `B_RaiseFightTalent` i woła `RX_ApplyAnim_Weapons`, więc animacje zakłada skrypt NB.

---

## 6. Walka i AI postaci

### 0a, 54. Potwory nie atakowały / czarny goblin stał w miejscu – ✅ (przed logiem, 14.11.2026)
- **Pliki:** `common/world/objects/npc.cpp`, `common/game/gamescript.cpp`
- **0a:** `ZS_MM_AllScheduler` robi `AI_RemoveWeapon` + `AI_StartState`, a potwory nie mają animacji chowania broni – kolejka AI stała bez końca i potwór nie dostawał percepcji. Teraz potwór chowa broń od razu.
- **54:** goblin z maczugą schowaną do snu nie mógł jej dobyć (brak animacji) i próbował w kółko zamiast atakować. Teraz potwór, który przez 1 s nie dobędzie broni, dostaje ją od razu do ręki; animacja `AI_PlayAni`, której potwór nie może zacząć, jest pomijana po 1,5 s (ludzie czekają jak dotąd).
- **Log:** `NB ai: …` (stan, kolejka, broń, percepcje potwora przy bohaterze), `STUCK 5 s`, `cannot draw its weapon - it takes it at once`.

### 23. NPC bez talentów i animacji walki (cały świat NB) – ✅ (13.11.2026, sprawdzone 15.11)
- **Plik:** `common/game/gamescript.cpp`
- Każda postać NB woła `B_SetFightSkills` → `RX_ApplyProperOverlaysForNpc` pluginu. Bez niego talenty wszystkich NPC były 0%: animacje nowicjusza, brak trafień krytycznych. Przepisany kod z UNION_ABI.DLL (0x10198B20): procent do szansy trafienia, nakładka ST1/ST2/ST3 od 30/60/90% (łuk, kusza – T1/T2), poziom umiejętności.

### 29, 35. Style walki NPC z pluginu według gildii – ✅ (14.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- Plugin co 100 ms dobiera walczącym ludziom z bronią w ręku styl walki według gildii, szansy trafienia i losowania (nowe losowanie po 4–8 s).
- **29 – ⏸ wycofane:** wersja dla gildii bez własnej tabeli (Lares, VLK, BAU…) – gra działała lepiej bez niej; te gildie zostają przy stylach z poprawki 23.
- **35 – zrobione:** gildie z własną tabelą w pluginie: bandyci (Nestor, Franco, Esteban, Dexter, Gor Boba…), najemnicy (Sentenza, Buster, Fester, Bullco, Raoul…), straż, paladyni, łowcy smoków, templariusze, piraci (Alligator Jack), banici. „Rapier” = broń z bitem `BIT_ITEM_PIERCE_DAMAGE`; styl „zręcznościowy” w 80% zamieniany na zwykły.
- **Jeszcze nie:** szkielety (`GIL_SKELETON`, długa tabela po nazwach).

### 24. Bandyci nie atakowali bezimiennego w zwykłej zbroi – ✅ (13.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- NB daje każdemu człowiekowi osobisty stosunek do bohatera „neutralny”, a OpenGothic stawiał go ponad gildią. Teraz przy „neutralnym” wygrywa wroga gildia (jak w NB). Dodane `Npc_GetGuildAttitude`.
- Błąd `Accessing member "C_NPC.AIVAR" without an instance set (in C_PLAYERISFAKEBANDIT)` jest nieszkodliwy (zmienne Ramona z Jarkendaru; w oryginale tak samo).

### 25, 31. Wybór broni NPC: dobywanie broni, łuk przed mieczem – ✅ (13.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- `B_SelectWeapon` kończy się `RX_SelectWeaponNpc` pluginu (UNION_ABI.DLL 0x1017F840) – bez niego NPC nie dobywali broni. Teraz zasady pluginu:
  - z łukiem/kuszą i strzałami NPC strzela, chyba że wróg jest bliżej niż 600; do krwiopijców i harpii zawsze; do odpornych na strzały tylko „łucznik-potwór”;
  - strzelający nie zmienia łuku na miecz przy zwierzętach; po zmianie broni następna dopiero po 6 s; bez broni białej – pięści;
  - strzały liczone do założonego łuku (nie tylko do broni w ręku).
- Lares strzela z łuku i kładzie zwierzęta jednym trafieniem.

### 26, 28, 22. Tempo ataków i odskoki – ✅ (13.11.2026)
- **Plik:** `common/world/objects/npc.cpp`
- **26:** potwory po ataku czekały tylko do okna kombinacji (`DEF_HIT_END`) i gryzły ~1,5× szybciej niż w Gothicu. Teraz w walce pięściami/kłami czekają do końca animacji.
- **28, 22:** „czekanie przy wrogu = odskok” działało też na ludzi z bronią i co drugą decyzję. Teraz tylko dla walczących pięściami/kłami i najwyżej raz na 2,5 s.

### 33, 34. Rytm walki ze zwierzętami jak w Gothicu – ✅ (14.11.2026, sprawdzone 15.11)
- **Plik:** `common/world/objects/npc.cpp`
- Reakcja na zamach (`FA_ENEMY_PREHIT`) była sprawdzana co ~0,2 s, więc np. ścierwojad odskakiwał od pierwszego zamachu. Teraz zwierzęta z odskokiem w taktyce (wilk, krwiopijca, waran, szczur, zębacz, cieniostwór, harpia, ścierwojad…) odskakują dopiero po 2 trafieniach od poprzedniego odskoku.

### 27. Towarzysz (Lares) stał 0,25–3 s po każdej walce i dialogu – ✅ (13.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- `B_CheckHealth` woła `B_Npc_Desynchronize` (`AI_WaitMS` 250–3000 ms) przy każdej zmianie stanu, a towarzysz nigdy nie osiąga swojego miejsca. Dla członków drużyny (`AIV_PARTYMEMBER`) to czekanie jest pomijane.

### 30. NPC bez trafień krytycznych na starym zapisie – ✅ (13.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- Szanse NPC ustawia poprawka 23 przy tworzeniu postaci, więc w starych zapisach były 0%. NPC przy bohaterze z szansami 0 i procentami w `AIVAR[84..87]` dostaje szanse z nich. Lares znowu kładzie zwierzęta jednym ciosem.

### 17, 18, 20, 21. Wcześniejsze poprawki walki bez osobnego opisu – ✅
- Z tabeli plików (szczegółowy opis się nie zachował): `npc.cpp` – efekty krwi przy trafieniu, prędkość ruchu przeskalowanych istot; `playercontrol.cpp` (18) – „lepki” focus przeciwnika w walce wręcz (bohater sam się do niego obraca).

---

## 7. Świat: rzemiosło, zbroje, teleport, ognik

### 2. Stoły rzemieślnicze nic nie robiły – ✅ (12.11.2026)
- **Plik:** `common/game/gamescript.cpp` (`RX_OpenCraft`)
- NB otwiera stoły graficznym oknem pluginu. Teraz `RX_OpenCraft` otwiera stare menu dialogowe, które nadal jest w skryptach NB (przepisy i składniki ze skryptów).
- **Sprawdzone:** stół alchemiczny, runiczny, demonologiczny, stół z tabliczkami, pulpity z księgami (inteligencja 5 → 6). **Do sprawdzenia:** zwoje, kowadło, piec; kuchnia, łuki, biżuteria i runy demoniczne mogą nie mieć starego menu.

### 7. Łóżko: po przespaniu bohater nagle stał, a potem wstawał z łóżka – ✅ (12.11.2026)
- **Plik:** `common/world/objects/npc.cpp` (`Npc::resetPositionToTA`)
- Sen przewija czas (`Wld_SetTime`), a OpenGothic przy tym zatrzymywał animację bohatera. Teraz bohater w łóżku zostaje i wstaje normalną animacją łóżka.

### 16, 39. Paladyni bez zbroi (Lothar, Lord Andre – sama głowa) – ✅ (14.11.2026)
- **Pliki:** `common/game/inventory.cpp`, `common/game/inventory.h`, `common/world/objects/npc.cpp`
- **16 – diagnostyka:** zbroja wczytywała się poprawnie, ale zaraz po niej postać zakładała hełm (`ITEM_KAT_ARMOR` z `WEAR_HEAD`).
- **39 – poprawka:** OpenGothic miał jedno miejsce na zbroję i hełm ją zastępował. Teraz hełm ma osobne miejsce (węzeł `ZS_HELMET`, a bez niego `BIP01 HEAD`), zbroja zostaje; „najlepsza zbroja” nie wybiera hełmu; stare zapisy są poprawiane przy wczytaniu (format zapisu bez zmian).

### 59, 60. Teleport między światami i zbroja po teleporcie – teleport 🧪, zbroja ✅ (15.11.2026)
- **Pliki:** `common/game/gamescript.cpp` (+ diagnostyka w `common/game/inventory.cpp`)
- **59 – teleport:** NB zmienia świat funkcją pluginu `Wld_ChangeLevel_AB(plik ZEN, punkt)` (kamienie i runy teleportacji, przedmioty przeniesienia, runa Galoma, świątynia Azgalora, dolina Adanosa…). Nie było jej, więc teleport zabierał manę i nic się nie działo. Teraz zmiana świata jak przy przejściu między światami. W logu `[ctx] change of the world: … -> …`.
- **60 – zbroja:** `RX_ApplyHeroHead` ustawia ciało bohatera przez `Mdl_SetVisualBody(…, NO_ARMOR)`. W Gothicu to znaczy „nie zakładaj nowej zbroi”, a w tej wersji OpenGothic (dla Archolos) zdejmowało założoną. W NB zbroja teraz zostaje. Naprawiło to też koniec prologu – bezimienny ma łachmany. Diagnostyka `[ctx] the hero takes off …` zostaje.

### 11. Ognik od Vatrasa – ✅ (12–15.11.2026; połączone poprawki 11, 61, 63–67)
- **Pliki:** 🆕 `mods/nb-wisp/` (`manifest.json`, `index.js`, gotowy `mods/nb-wisp.zip`), `common/game/gamescript.cpp`, `common/game/nbtexts.h`, `common/world/objects/npc.cpp`
- **Sprawdzone (15.11.2026):** ognik lata, szuka, przynosi przedmioty i łup z ciał, uczy się umiejętności, nie wywala gry.
- **Przyczyna:** w NB całego ognika (`WISP_DETECTOR`) prowadzi plugin Union: przywołanie pod L-Shift+F5, szukanie i zbieranie (UNION_ABI.DLL 0x102740c0), latanie wokół bohatera. Skrypty mają tylko postać, umiejętności i dialog; `B_MM_WispDetect` nie jest nigdzie wywoływane, a `ZS_MM_Rtn_Summoned` prowadzi do bohatera wszystkie przywołane stwory oprócz ognika.
- **Jak działa teraz:**
  - **przywołanie (mod JS):** z amuletem wędrującego ognika – **Shift+F5**, przycisk w zakładce **„Ognik”** menu modów albo komenda `ognik`; po zmianie świata i wczytaniu gry wraca;
  - **latanie:** nie trzyma się bohatera, tylko lata i szuka; leci prosto (8 m/s, z umiejętnością szybkości 15 m/s); bez zajęcia wraca do bohatera, gdy jest dalej niż 8 m; dalej niż 40 m – staje od razu obok;
  - **szukanie:** co 0,1 s najbliższy przedmiot w promieniu 10 m od bohatera (20 m z umiejętnością obszaru), nie wyżej/niżej niż 5 m; wszystko do 3,5 m od ognika trafia do ekwipunku od razu (do 5 naraz); gdy nic nie leży – łup z najbliższego martwego (z umiejętnością „Zbieranie przedmiotów z przeciwników”);
  - **umiejętności** (dialog ognika „Umiejętności zbierania przedmiotów”, zmienne skryptów `WISPSKILL0..9`): 0 – łup z ciał, 1 – broń i amunicja, 2 – mikstury i kamienne tablice, 3 – jedzenie i rośliny, 4 – pancerze, 5 – pierścienie i amulety, 6 – runy i zwoje, 7 – dokumenty (od początku gry), 8 – większy obszar, 9 – szybkość. Rzeczy bez kategorii (mieszki, złoto, ruda, trofea, wytrychy…) zbiera zawsze. Wartości obszaru i szybkości dobrane na oko (pluginu nie da się w tym odczytać);
  - **czego nie bierze:** przedmiotów z nienauczonej kategorii, kluczy (jak plugin), cudzych przedmiotów, gdy właściciel albo ktoś z jego gildii jest w promieniu 20 m (antykradzież), a przy wyłączonym w dialogu „zbieraniu wartościowych przedmiotów” – zbroi i mikstur; czego nie dosięgnie w 15 s, odpuszcza na minutę;
  - **napisy jak na PC:** po prawej „Szukanie...” / „Coś znaleziono!”, po lewej lista „Ognik: nazwa xN” (ostatnie 7, po 10 s; na PC zielona, tu żółta – OpenGothic nie ma zielonej czcionki).
- **Crash (63):** gra padała (`SIGSEGV` w `Item::position` ← `Npc::nextAiAction`), gdy przedmiot był zebrany, zanim postać zaczęła swoje „idź do przedmiotu”. Teraz `AI_GotoItem` bez przedmiotu nic nie robi – dotyczy każdej postaci.
- **Log:** co 10 s `[ctx] wisp search: … (not learned …, owner near …, skipped …, important …, keys …, height …)` oraz `the wisp found …`, `the wisp found the body …`, `the wisp brings …`, `the wisp takes an item of an owner …`, `the wisp did not reach …`, `the wisp was … cm away`.
- **Jeszcze nie ma:** „zbieranie w pomieszczeniach”, pokazywanie rzadkich roślin i tabliczek (`RX_ShowWispRare`, `RX_ShowWispStone`, `FindPlates`).

---

## 8. Dźwięk

### 36–38. Zacinanie się dźwięku przy wczytywaniu – ✅ (14.11.2026)
- **Pliki:** `common/gamemusic.cpp`, `game/main.cpp` (folder `game/` w głównym katalogu projektu)
- **Przyczyna:** muzykę ładował wątek miksowania OpenAL – pod blokadą zasobów, którą w tym czasie trzyma wczytywanie świata; do tego na Androidzie wątek dźwięku ma zwykły priorytet i krótki bufor (60 ms).
- **Poprawka:**
  - **36, 38:** oba silniki muzyki (domyślny „OpenGothic” i „GothicKit”) ładują nowy motyw w osobnym wątku; wątek dźwięku zaczyna go dopiero po załadowaniu, motyw, który już gra, nie jest ładowany ponownie;
  - **37:** priorytet wątku miksowania podnoszony do priorytetu audio Androida (−16, gdy się nie da −10); bufor OpenAL 120 ms (`alsoft.conf` obok `log.txt`: `period_size = 960`, `periods = 6`). Jeśli dźwięki wydają się spóźnione – zmniejszyć `periods` do 4–5.

---

## 9. Do zrobienia

- [ ] **Animacje broni NB:** sprawdzić, czy zestawy rapiera/szabli/włóczni mają w .MDS ciosy, które OpenGothic odtwarza inaczej (kilka pchnięć w animacji, kombinacje, zasięg) – potrzebne nagranie walki rapierem i mieczem; kod wyboru NPC z innymi broniami (ok. 0x1024AE00 w DLL).
- [ ] Potwory w walce za często odskakują/krążą – problem silnika OpenGothic (widać też w Renovation); `FA_ENEMY_PREHIT` do porównania z oryginałem.
- [ ] Walka kilku postaci z jednym celem: cel „głupieje” (krąży, nie atakuje) – nagranie z obozu bandytów, Brago.
- [ ] Style walki szkieletów (`GIL_SKELETON`) – poprawka 35.
- [ ] Mieszek i torba na bohaterze (`EXT_PutInSlot`, `ActivateZSSlot`) – opcje z poprawki 5.
- [ ] Znikanie pustych ciał (opcja optymalizacji NB).
- [ ] Funkcje bossów wołane przez plugin: `AI_OnTick_<instancja>`, `AI_OnHit_…`, `AI_OnGetDamage_…`, `AI_OnBreak_…`, razem z obrażeniami w czasie i truciznami (`RX_ApplyDotDamage`, `RX_ApplyPoison`).
- [ ] System karmy pluginu (`UpdateKarma`, 130 wywołań – wydarzenia, znaki, osiągnięcia); samo wyświetlanie karmy działa (poprawka 62).
- [ ] Klawisze skrótów NB (`RX_KeyPressed`, 100 wywołań – mikstury, strzały, pochodnia, pomijanie scenek).
- [ ] Przywołania (`RX_SummonNpcToTarget`, `RX_ResetSummons`) i okna pluginu (handel, rzemiosło z listą, poker).
- [ ] Ognik: pokazywanie rzadkich roślin/tabliczek, „zbieranie w pomieszczeniach” (poprawka 11).
- [ ] Napisy: „Dzień: …” w lewym górnym rogu, liczniki wampira i Full SnC+ (poprawka 62).
