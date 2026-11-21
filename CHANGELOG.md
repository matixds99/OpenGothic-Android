# Log poprawek – OpenGothic + Gothic II: New Balance

Repo: fork `matixds99/OpenGothic-Android`, gałąź `touch-ui` (na bazie `borsuczyna/opengothic-android`).
Lista plików (co podmienić, co dodać) jest w [docs/NEW_BALANCE.md](docs/NEW_BALANCE.md).

**Status:** ✅ sprawdzone w grze · 🧪 czeka na test · ⏸ wycofane / odłożone

**Numery:** każda poprawka ma swój numer, a komentarze w kodzie (`NB fix N`) i tabela plików odwołują się do tych numerów. Poprawki o tym samym temacie są tu połączone w jeden wpis – wtedy w nagłówku stoi kilka numerów (np. „36–38”). Nowe poprawki dostają kolejny wolny numer (następny: **148**; numery 61, 63–67 są w opisie ognika – poprawka 11).

**🆕** oznacza plik, którego nie ma w projekcie borsuczyny i który trzeba dodać.

---

## NOWE

Pięć ostatnich zmian, od najnowszej. Szczegóły w pełnych wpisach niżej.

1. **147** – „Rysowanie świata”: drzewa, płoty i skały rysowane tak daleko jak teren (bez wyskakiwania przed bohaterem) – ✅
2. **146** – zasięg rysowania postaci działa dla całej postaci (głowa, zbroja, broń); nowa opcja zasięgu widzenia świata – ✅
3. **145** – Fregyal pojawia się na łodzi obok bohatera na Zachodnim Wybrzeżu – ✅
4. **144** – towarzysze bohatera wsiadają z nim, gdy rusza łódź lub winda – ✅ (razem z 145)
5. **143** – przycisk „POMIŃ” scen New Balance (rejs łodzią) – ✅

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

### 73. Gra nie zamyka się po wyjściu do ekranu głównego / przy wiadomości – ✅ (17.11.2026)
- **Pliki:** `patches/tempest-opengothic.patch` (Tempest: `Engine/system/api/androidapi.cpp/.h`, `Engine/gapi/vulkan/vswapchain.cpp/.h`), `patches/README.md`
- **Zgłoszenie (film 00:17):** po wyjściu do ekranu głównego (albo gdy przyjdzie wiadomość i trzeba ją otworzyć) gra się zamyka, po powrocie launcher pokazuje „Gra się zamknęła” ze stosem `MainWindow::tick` ← `MainWindow::render` ← `AndroidApi::implProcessEvents`.
- **Przyczyna:** gdy aplikacja idzie w tło, Android zabiera jej powierzchnię (`APP_CMD_TERM_WINDOW`). Tempest traktował to jak zamknięcie gry: wysyłał „zamknij okno” i kończył pętlę – a gra przy zamykaniu jeszcze rysowała klatkę i padała.
- **Poprawka:** tło to nie koniec gry. Okno Tempesta zostaje, zmienia się tylko natywne okno za nim: w tle nic nie jest rysowane (gra czeka), po powrocie `APP_CMD_INIT_WINDOW` daje nową powierzchnię, a zdarzenie zmiany rozmiaru robi swapchain od nowa na niej (Vulkan: nowa `VkSurfaceKHR`, gdy powierzchnia się zmieniła). Grę zamyka tylko `APP_CMD_DESTROY` (zamknięcie aplikacji).
- W logu `android: the window goes away (background)`.
- **Uwaga:** jeśli w tle zabraknie pamięci, Android i tak może ubić grę (to system, nie błąd gry) – wtedy pomaga zapis przed wyjściem.
- **Do sprawdzenia:** wyjście do ekranu głównego i powrót, otwarcie wiadomości z powiadomienia, przełączenie na inną aplikację i z powrotem; czy obraz wraca i czy dotyk działa.
- **Test (17.11):** gra już się nie zamyka, ale po powrocie był czarny ekran, a animacja ikony w launcherze się nie kończyła – poprawione w 74.

### 74. Czarny ekran po powrocie z tła – ✅ (17.11.2026)
- **Pliki:** `common/mainwindow.cpp/.h`, `patches/tempest-opengothic.patch` (Tempest: `androidapi.cpp/.h`, `vswapchain.cpp/.h`), `patches/README.md`
- **Zgłoszenie:** po powrocie do gry czarny ekran, animacja otwierania gry w launcherze się nie kończy. W logu 4× `android: the window goes away (background)` i nic więcej.
- **Przyczyna:** nowe okno po powrocie ma ten sam rozmiar, więc widżet okna nie wysyłał zdarzenia zmiany rozmiaru – swapchain nie był robiony od nowa i gra rysowała dalej na starej, martwej powierzchni. Do tego stary swapchain zostawał na powierzchni, którą Android już zabrał.
- **Poprawka:**
  - przy wyjściu w tło (`APP_CMD_TERM_WINDOW`) gra od razu czeka na GPU i niszczy swapchain z powierzchnią – zanim Android ją zabierze (nowy `AndroidApi::setWindowLostHandler`);
  - przed każdą klatką `MainWindow::render` sprawdza, czy okno Androida się zmieniło (`nativeWindowGeneration`) albo swapchain jest pusty – wtedy robi swapchain na nowej powierzchni i ustawia kamerę; bez okna nie rysuje nic;
  - `VK_ERROR_SURFACE_LOST_KHR` przy pobraniu/pokazaniu klatki też robi nową powierzchnię zamiast błędu.
- W logu: `android: the window comes back` i `android: the swapchain is made anew (szer x wys)`.
- **Crash z `Item::position` ← `Npc::nextAiAction`** (dołączony crash.log) to stary, naprawiony już błąd 63 – w obecnym kodzie to miejsce ma sprawdzenie; launcher pokazuje ostatni zapisany crash.
- **Test (17.11):** działa – gra wraca z tła z obrazem (w logu `the window comes back` i `the swapchain is made anew (2608x1200)`).

### 77. Kocioł pusty, choć bohater zna przepisy na zupy i kompoty – ✅ (17.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie:** w statystykach „Zupy / Kompoty / Ciasta – uczony”, a okno kotła puste. W logu `NB: craft: no item "itfo_pottage_mushroom_black"`, `no item "itfo_compote_00"`, `0 known recipes`.
- **Przyczyna:** przepis był rozpoznany, ale przedmiot wyniku odrzucony: test `is_instance_of<IItem>()` z ZenKita jest prawdziwy tylko dla przedmiotu, którego instancja już powstała (był w świecie albo w czyimś ekwipunku). Nowe przedmioty (gęsty gulasz z czarnych grzybów, kompot) wypadały z listy – tak samo składniki, których nikt jeszcze nie miał.
- **Poprawka:** przedmiot rozpoznawany po klasie `C_ITEM` (przez prototyp), bez względu na to, czy instancja już jest. Dotyczy wszystkich stołów (70–71) i okna sprzedaży (68).
- **Test (17.11):** działa – kotły i piec Thekli pokazują przepisy.

### 78. Ustawienie: celowanie w pokojowe stworzenia (ognik, przywołańce, zwierzęta) – ✅ (17.11.2026)
- **Pliki:** `common/world/worldobjects.cpp/.h`, `common/utils/androidinput.cpp/.h`, `common/ui/mobilecontrolsmodel.cpp/.h`
- **Zgłoszenie:** w walce gracz zaznacza i bije ognika (i przywołane istoty). Potrzebny przełącznik w ustawieniach: włączony – można zaznaczyć ognika (rozmowa), owce, króliki, żółwie (polowanie); wyłączony – pokojowe stworzenia nie są zaznaczane.
- **Poprawka:** nowy wiersz w ustawieniach „Peaceful targets” → `Target peaceful creatures: ON/OFF` (domyślnie ON – jak dotąd; w `Gothic.ini`, sekcja `MOBILE`, `peacefulfocus`, wersja ustawień 8). Przy OFF z celowania gracza (fokus, walka, zaklęcia) wypadają stworzenia (gildie potworów), które nie są do niego wrogie (nastawienie neutralne/przyjazne): ognik, przywołańce, owce, króliki, żółwie… Zostają: ludzie i orkowie (dialogi), martwe ciała (łupy), oraz każde stworzenie, które atakuje gracza albo jest do niego wrogie/rozgniewane.
- **Do sprawdzenia:** OFF – walka obok ognika i przywołańców nie zaznacza ich, owca się nie podświetla; ON – rozmowa z ognikiem, polowanie na owce jak dotąd.
- **Test (17.11):** działa – fokus na ognika wyłącza się i włącza przełącznikiem.

### 79. Otwieranie zamków NB: funkcje pluginu (przycisk kłódki – zastąpiony w 80) – ✅ (17.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/mainwindow.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp/.h`, `android/assets/touch-icons/lockpick.png` (nowa ikona)
- **Zgłoszenie:** w New Balance zamki otwiera się na 3 sposoby: siłą (nauka u Karla – bohater rąbie skrzynię bronią), wytrychami (zręczność – zużywa tyle wytrychów, ile wymaga zamek) i magią (runa od Gallahada – animacja rzucania czaru). Na telefonie nie działało żadne.
- **Jak to działa na PC (UNION_ABI.DLL):** plugin przy każdym klawiszu woła skrypt `HandleEvent(key)`. Klawisz Y (`KEY_BREAKCHEST_STR`): runa `ItRu_Unlock` (75 many, `T_FIBCAST_2_FIBSHOOT`) albo siła (`RX_ForcePickLock`, broń do walki wręcz, siła ≥ 12 × liczba obrotów zamka − 50, `T_1HSBRKCHST`/`T_2HSBRKCHST`, −30% wytrzymałości); Shift+Y (`KEY_BREAKCHEST_DEX`): wytrychy (talent, zręczność ≥ 100, liczba wytrychów zależna od zamka i zręczności, `T_PLUNDER`). Skrypt sprawdza warunki, gra animację i pyta plugin (`Mob_GetBreakNum` – liczba obrotów zamka, `Mob_SetBrakeChest` – otwórz skrzynię w fokusie).
- **Poprawka:**
  - funkcje pluginu: `Mob_GetBreakNum` (długość kodu zamka skrzyni w fokusie), `Mob_SetBrakeChest` (otwiera skrzynię/drzwi w fokusie i woła `UnlockBreakDone`; zamek tylko na klucz → `CantUnlock`), `Mob_SetUnLocked` / `Mob_SetUnLocked_Rune` (zaklęcie otwierania: zwój zużyty, `UnlockDone`), `RX_KeyPressed` (klawisz pluginu – tu tylko przycisk kłódki);
  - nowy przycisk **kłódki** na HUD (nad przyciskiem celu, da się go przesunąć/ukryć w edytorze HUD, id `lockpick`) – widoczny, gdy w fokusie jest zamknięta skrzynia z zamkiem do wyważenia, a bohater nie ma broni w ręku;
  - jeden przycisk zamiast dwóch klawiszy – sposób wybierany sam: magia (runa i ≥ 75 many) albo siła (gdy jej wystarczy) → klawisz Y; inaczej wytrychy → Shift+Y; gdy nic nie zadziała, skrypt mówi dlaczego (za mało many/siły/zręczności, brak talentu, brak wytrychów). Potem `HandleEvent` jak na PC.
- Zwykłe otwieranie wytrychami (minigra lewo/prawo przyciskiem akcji) bez zmian.
- **Test (17.11):** lepsza interakcja jak na PC/z NPC (wyciągnięta broń/runa, skrzynia w fokusie, przycisk akcji) – kłódka usunięta, zostały funkcje pluginu → poprawka 80.

### 80. Otwieranie zamków NB jako interakcja: broń, runa, wytrychy + przycisk akcji – ✅ (17.11.2026)
- **Pliki:** `common/world/world.cpp`, `common/world/worldobjects.cpp/.h`, `common/game/playercontrol.cpp`, `common/game/gamescript.cpp/.h`; usunięte: przycisk kłódki i `lockpick.png` z 79
- **Zgłoszenie:** zamiast osobnego przycisku – jak w oryginale: wyciągnięta broń albo runa, zamknięta skrzynia w fokusie, przycisk akcji (jak rozmowa z NPC).
- **Poprawka:**
  - **fokus:** z bronią do walki wręcz w ręku (gdy Karl nauczył wyważania, `RX_ForcePickLock`) albo z runą otwierania zamków (zaklęcie `SPL_UnlockChest`) zamknięta skrzynia z zamkiem do wyważenia wchodzi w fokus (fokus walki normalnie nie ma skrzyń), gdy w pobliżu nie ma postaci;
  - **przycisk akcji** na takiej skrzyni: broń → siła (Y, runa na ten czas pominięta), runa → magia (Y, siła na ten czas pominięta), puste ręce z talentem i ≥ 100 zręczności → wytrychy (Shift+Y). Dalej skrypt `HandleEvent` jak na PC: warunki, animacja (rąbanie bronią / rzucanie czaru / `T_PLUNDER`), mana, wytrzymałość, wytrychy, teksty. Bez broni i przy zręczności < 100 – zwykła minigra wytrychów jak dotąd.
- **Do sprawdzenia:** miecz w ręku przy zamkniętej skrzyni → fokus na skrzyni → akcja → rąbanie i otwarcie; runa → animacja czaru, −75 many; puste ręce (100+ zręczności) → zużyte wytrychy; komunikaty przy braku siły/many/wytrychów; w logu `[ctx] lock: …` i `lock opened`.
- **Test (17.11):** magia (runa) działa ✅. Siła i wytrychy – do sprawdzenia na innym zapisie (gracz gra magiem).
- **Test (18.11):** potwierdzone – runa, siła (najpierw wyciągnąć miecz, potem interakcja) i zręczność działają.

### 81. Kolory nazw w fokusie: nastawienie postaci, stan skrzyń (jak zUtilities) – ✅ (17.11.2026)
- **Pliki:** `common/mainwindow.cpp/.h` (`focusColor`), `common/game/gamescript.cpp/.h` (`nbIsSummon`)
- **Zgłoszenie:** redesign interfejsu – kolory nastawienia osób jak w pluginie zUtilities, zamknięta skrzynia żółta/zielona, opróżniona szara/biała.
- **Poprawka:** nazwa w fokusie w kolorze (z cieniem, czytelna na każdym tle):
  - **postacie i stworzenia:** czerwony – wrogie, pomarańczowy – rozgniewane, biały – neutralne, zielony – przyjazne, turkusowy – towarzysz (`AIV_PartyMember`) albo przywołana istota NB (`RX_IsSummonNpc`: `aivar[91]==RX_SUMMON_ID`, np. ognik);
  - **martwi:** biały – jest coś do zabrania, szary – przeszukane;
  - **skrzynie i drzwi:** żółty – zamknięte, zielony – coś w środku, szary – pusta;
  - przedmioty i inne obiekty – biały, jak dotąd.
- **Do sprawdzenia:** kolory przy różnych postaciach (strażnik, bandyta, owca, ognik), zamknięta/otwarta/pusta skrzynia, przeszukane ciało.
- **Test (17.11):** działa – skrzynia: żółta (zamknięta) → zielona (otwarta runą) → szara (opróżniona).

### 82. Muzyka walk z bossami NB – 🧪 (17.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/gothic.cpp/.h`, `common/world/objects/npc.cpp`
- **Zgłoszenie:** przy bossach nie gra ich muzyka (osobna dla postaci, osobna dla potworów). Lista bossów: arkusz `SPIS_NB.xlsx`, zakładka „Bossy” (gałąź `NewBalance`, `nb-docs/`).
- **Jak to działa na PC:** plugin przy każdym ciosie woła `RX_OnDamage`; gdy gracz albo jego przywołaniec walczy z bossem (`RX_IsBoss`: `aivar[90]` albo PN za zabicie), `RX_CheckMusicBoss` wybiera utwór – własny dla ok. 50 bossów (Xardas, Kreol, Ahiron, Kruk, smoki, Lordowie szkieletów, demony, awatary…) albo losowy z 54 – wyłącza muzykę gry (`RX_MusicOff`) i woła `RX_StartBossMusic("BATTLE_…")` (dźwięk z `SFX.DAT`, plik `BATTLE_*.wav`). Śmierć bossa (`ZS_Dead`) → `RX_StopBossMusic`. Opcje NB: `bToggleBossMusic` (domyślnie wł.), `bToggleMusicAnimalBoss` – bossowie-zwierzęta (domyślnie wył.).
- **Poprawka:**
  - przy każdym trafieniu (`Npc::takeDamage`) część `RX_OnDamage` od muzyki: gracz/przywołaniec ↔ boss → `RX_CheckMusicBoss` ze skryptów;
  - `RX_StartBossMusic` – utwór grany jako dźwięk (jeden naraz, od początku po zakończeniu), `RX_UpdateBossMusicCoords` – walka trwa, `RX_StopAllSound` – stop; muzyka gry wraca przez `RX_MusicOn`;
  - koniec także, gdy przez 45 s nie ma ciosu w walce z bossem (ucieczka) albo gracz zginie (`RX_StopBossMusic`); po wczytaniu zapisu z walki utwór wraca przy następnym ciosie;
  - wyłączona muzyka w ustawieniach (`musicEnabled`) – bez muzyki bossa.
- Reszta `RX_OnDamage` (zachwianie `RX_ApplyStagger`, efekty `RX_HeroProcs`/`RX_NpcProcs`, taktyki bossów, przyspieszenie bossów) – jeszcze niepodłączona.
- **Do sprawdzenia:** walka z bossem z listy (np. Mięsożerca, Kamieniołap – to zwierzęta: tylko z opcją `bToggleMusicAnimalBoss`; postacie/demony/szkielety – zawsze), w logu `[ctx] boss music: BATTLE_…`; po zabiciu bossa wraca zwykła muzyka.

### 83. Zachwianie i obalenie w walce jak w NB – 🧪 (17.11.2026)
- **Pliki:** `common/world/objects/npc.cpp` (`takeDamage`), `common/game/gamescript.cpp/.h` (`nbBeforeHit`, `nbOnDamage`)
- **Zgłoszenie:** w walce nie ma zachwiania/obalenia przeciwników jak w oryginale.
- **Przyczyna:** na PC plugin przy każdym ciosie woła skrypt `RX_OnDamage`; w OpenGothic nikt go nie wołał (82 dodała z niego tylko muzykę).
- **Poprawka:** z `RX_OnDamage`:
  - **zachwianie** (`RX_ApplyStagger`) – przed decyzją silnika o potknięciu: postać trafiona przez gracza (albo ta, która trafia gracza) dostaje `bodystate_interruptable_override` wg skryptu – siła/zręczność bohatera kontra rodzaj przeciwnika (elitarne orki, szkielety, draconianie… mają progi), bossowie i walczący na arenie się nie chwieją;
  - **obalenie** (`RX_NpcProcs`) – po ciosie gracza człowiek (nie boss, nie ork, poziom < 200, nie w trybie łuku/magii) przewraca się (`T_FALLB_2_FALLENB`) z szansą wg umiejętności broni bohatera (1H/2H, łuk/kusza; pięść 5%);
  - licznik walki `BIT_FIGHT_TIMER_STARTED` dla obu stron (inne mechaniki NB z niego korzystają).
- Efekty perków (`RX_HeroProcs`) i taktyki bossów – nadal później.
- **Do sprawdzenia:** bandyci/strażnicy przewracają się czasem od ciosu miecza (częściej przy wysokiej umiejętności), słabsi przeciwnicy chwieją się od ciosów, mocni (elitarne orki, szkielety) nie przy niskiej sile.

### 84. Magia: pocisk leci za celem – ✅ (17.11.2026)
- **Pliki:** `common/world/bullet.cpp/.h`, `common/world/worldobjects.cpp`, `common/world/objects/npc.cpp` (`commitSpell`)
- **Zgłoszenie:** przy celowaniu magią w potwora pocisk nie trafia.
- **Przyczyna:** pocisk czaru leciał prosto w miejsce, gdzie cel był w chwili rzucenia (`setTarget(nullptr)`), a potwory się ruszają; nisko stojące małe stworzenia (szczury) – pocisk z wysokości piersi bohatera łatwo zahacza o ziemię.
- **Poprawka:** pocisk czaru gracza z celem w fokusie co klatkę skręca w stronę środka celu (jak w Gothicu – pociski magii trafiają namierzony cel), aż do trafienia albo śmierci/upadku celu; bez celu leci prosto jak dotąd. Czary NPC bez zmian.
- **Do sprawdzenia:** ognisty pocisk / lodowa strzała w biegnącego wilka, szczura, kretoszczura – trafia.
- **Test (17.11):** działa – pocisk leci za celem, aż trafi. Na PC (Winlator) pocisk nie skręca, a w ruchome cele (krwiopijcy) i tak trafia – możliwe, że później złagodzimy skręt (np. ograniczony kąt na klatkę), żeby było bliżej oryginału.

### 147. „Rysowanie świata” – obiekty tak daleko jak teren – ✅ (21.11.2026)
- **Pliki:** `common/gothic.cpp` (`settingsObjectDistance`), `common/game/nboptions.h`
- **Zgłoszenie (nagranie):** drzewa, płoty, kamienne stoły i kapliczki pojawiają się dopiero kilkadziesiąt metrów przed bohaterem, a teren za nimi jest pusty. Prośba: połączyć zasięg świata i obiektów w jedno ustawienie „Rysowanie świata”.
- **Przyczyna:** obiekty świata (voby) mają w OpenGothic osobny zasięg: 100 m razy mnożnik z opcji, nie więcej niż teren. Teren był rysowany do 250 m, a obiekty tylko do 100 m – wyskakiwały w trakcie marszu.
- **Poprawka:**
  - Przy New Balance obiekty świata są rysowane tak daleko jak teren (`NB_OG/bVobsWithWorld`, domyślnie włączone).
  - Opcja z poprawki 146 nazywa się teraz „Rysowanie świata” (50–750 m) i ustawia zasięg terenu i obiektów razem.
- **Uwaga:** ten sam zasięg dla obiektów to więcej rysowania niż wcześniej (np. 250 m zamiast 100 m). Gdy spadają klatki, zmniejszyć „Rysowanie świata”, np. do 150 m.
- **Do sprawdzenia:** marsz przez pola i las – drzewa, płoty i kamienie widać od razu z daleka, nic nie wyskakuje. Klatki przy 150 / 250 / 400 m.
- **Test (21.11):** działa; „Rysowanie świata” na 100 m rozwiązuje problem wyskakujących obiektów.

### 146. Zasięg rysowania postaci i świata – ✅ (21.11.2026)
- **Pliki:** `common/world/world.cpp/.h` (`nbNpcRangeTick`), `common/world/objects/npc.h`, `common/graphics/mdlvisual.cpp/.h`, `common/graphics/meshobjects.cpp/.h`, `common/graphics/visualobjects.cpp/.h`, `common/graphics/drawcommands.cpp`, `common/game/gamescript.cpp`, `common/game/nboptions.h`
- **Zgłoszenie:** ustawienie „Zasięg rysowania postaci” (30/70/100 m) chyba nie działa. Prośba o lepszą kontrolę nad rysowaniem świata, postaci i zwierząt.
- **Przyczyna:** poprawka 109 odcinała w przebiegu widoczności GPU tylko części ze szkieletem (ciało). Głowa (siatka z mimiką), hełm, broń i przedmioty na kościach to osobne siatki – zostawały narysowane. Zysk był mały, a daleko mogły wisieć same głowy i broń.
- **Poprawka:**
  - Zasięg postaci liczony na procesorze 4 razy na sekundę: postać dalej od bohatera niż ustawiony zasięg nie jest rysowana w całości – ciało, głowa, zbroja, broń, przedmioty w rękach i na kościach. Dotyczy ludzi, potworów i zwierząt. AI postaci działa dalej.
  - Ukryta część trafia poza każdy widok, więc odpada też z cieni. Na Androidzie ukryte głowy nie liczą mimiki.
  - Nowa opcja na stronie NB (detale): „Zasięg widzenia świata” 50–750 m – to samo ustawienie co „Zasięg widzenia” w opcjach gry (`PERFORMANCE/sightValue`), teraz obok zasięgu postaci. Domyślnie 250 m. Działa od razu.
- **Do sprawdzenia:**
  - Zasięg postaci 30 m w Khorinis: postacie dalej niż 30 m znikają w całości (bez wiszących głów i broni) i pojawiają się po podejściu. Czy przybywa klatek.
  - „Bez limitu” – wszyscy znowu widoczni.
  - Zasięg widzenia świata: mniejszy – bliżej mgła i koniec świata, więcej klatek.
- **Test (21.11):** działa (razem z 147).

### 145. Fregyal obok bohatera na łodzi na Zachodnim Wybrzeżu – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp` (`AI_Teleport`), `common/world/triggers/movetrigger.cpp`
- **Zgłoszenie:** po poprawce 144 Fregyala dalej nie ma przy bohaterze. Propozycja: niech pojawia się obok bohatera.
- **Z logu:** łódź rusza z samym bohaterem (`riders 1`), a w promieniu 50 m nie ma żadnego towarzysza – punkt `WP_START_000`, na który skrypt przenosi Fregyala (`AI_Teleport`), leży w tym świecie daleko od łodzi.
- **Poprawka:**
  - `AI_Teleport` postaci na `WP_START_000` na Zachodnim Wybrzeżu (skrypt `RX_WestCoast_Loop` tuż przed odpłynięciem) stawia ją obok bohatera na pokładzie.
  - Gdy łódź lub winda rusza z bohaterem, na pokład wsiadają wszyscy jego towarzysze w tym świecie, nie tylko ci w promieniu 50 m. W logu jest ich lista z odległością.
- **Do sprawdzenia:** rejs na Zachodnie Wybrzeże – Fregyal stoi obok bohatera na łodzi i dopływa z nim do brzegu. W logu `[ctx] AI_Teleport: Fregyal -> next to the hero`.
- **Test (21.11):** działa. Cała podróż na Zachodnie Wybrzeże sprawdzona (poprawki 139–145): wejście na łódź, „Pora odpływać”, rejs z Fregyalem, „POMIŃ”, wczytanie Zachodniego Wybrzeża i dopłynięcie do brzegu razem z Fregyalem.

### 144. Towarzysze wsiadają z bohaterem na ruchomy obiekt – Fregyal zostawał na Zachodnim Wybrzeżu – ✅ (razem z 145) (21.11.2026)
- **Pliki:** `common/world/triggers/movetrigger.cpp/.h`
- **Zgłoszenie:** po wczytaniu Zachodniego Wybrzeża łódź płynie z bohaterem do brzegu, ale Fregyala przy nim nie ma.
- **Z logu:** Fregyal jest wstawiany na punkcie `WP_START_000`, który nie leży nad łodzią (port przesunął go o 1,5 m na wolne miejsce), a tuż przed odpłynięciem skrypt przenosi go na ten sam punkt (`AI_Teleport`). Łódź rusza bez niego. Skrypt w tej samej chwili robi go towarzyszem bohatera (`AIV_PartyMember`).
- **Poprawka:** gdy ruchomy obiekt (łódź, winda) rusza z bohaterem na pokładzie, jego towarzysze (`AIV_PartyMember`) w promieniu 50 m stają obok niego na pokładzie i płyną razem z nim.
- **Do sprawdzenia:**
  - Rejs na Zachodnie Wybrzeże (także po „POMIŃ”): Fregyal stoi obok bohatera na łodzi i dopływa z nim do brzegu.
  - W logu `[ctx] mover EVT_DRUIDBOAT_WC starts with the hero, companions on board: Fregyal`.
- **Test (21.11):** nie pomogło – w logu `starts with the hero, riders 1`: w promieniu 50 m nie było żadnego towarzysza (poprawka 145).

### 143. Pomijanie rejsu łodzią („POMIŃ”), Fregyal na łodzi na Zachodnim Wybrzeżu – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/mainwindow.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp/.h`
- **Zgłoszenie:** brakuje sposobu na pominięcie rejsu, a po wczytaniu Zachodniego Wybrzeża na łodzi nie ma Fregyala.
- **Jak to działa na PC (UNION_ABI.DLL 0x100bcd90, 0x10235530):** plugin pyta skrypt `RX_IsCutsceneActive`, czy trwa scena do pominięcia (rejs łodzią, koncerty, więzienie). Klawisz G lub R wywołuje wtedy `Script_Use_Button_Cutscene` – przy rejsie od razu przenosi na Zachodnie Wybrzeże.
- **Poprawka:**
  - Przycisk „POMIŃ” na środku ekranu pod górnym rzędem, widoczny tylko w czasie takiej sceny. Klawisze G i R działają tak samo (klawiatura, pad).
  - Postać wstawiana przez skrypt (`Wld_InsertNpc`) na punkcie nad ruchomym obiektem zostaje na punkcie. Wcześniej szukanie wolnego miejsca wokół punktu mogło zrzucić ją z pokładu do morza – tak mógł zniknąć Fregyal, wstawiany na łodzi na Zachodnim Wybrzeżu.
  - Log: `[ctx] insert: … moved N cm`, `[ctx] AI_Teleport: … -> …`, `[ctx] scene skipped`.
- **Do sprawdzenia:**
  - Rejs z Fregyalem: przycisk „POMIŃ” w czasie rejsu przenosi na Zachodnie Wybrzeże.
  - Fregyal jest na łodzi na Zachodnim Wybrzeżu i płynie z bohaterem do brzegu. Jeśli go nie ma – log z linijkami `insert` i `AI_Teleport`.
- **Test (21.11):** „POMIŃ” działa. Fregyala dalej nie ma: log pokazał, że jego punkt `WP_START_000` nie leży nad łodzią (`moved 150 cm`), a `AI_Teleport` przenosi go na ten punkt przed odpłynięciem (poprawka 144).

### 142. Po dopłynięciu łodzi nie wczytuje się Zachodnie Wybrzeże – ✅ (21.11.2026)
- **Pliki:** `common/world/waymatrix.cpp/.h`, `common/game/gamescript.cpp` (`Vob_GetDistToVob`)
- **Zgłoszenie:** łódź dopływa na miejsce i nic się nie dzieje – świat Zachodniego Wybrzeża się nie wczytuje.
- **Przyczyna:**
  - `RX_NewWorld_Loop` zmienia świat (`Wld_ChangeLevel_AB` → `ADDON\WESTCOAST.ZEN`), gdy bohater jest bliżej niż 25 m od punktu `FP_DRUID_CHECKBOAT`. Ten punkt pochodzi z części świata dokładanej przez skrypt (`FP_DRUID_CHECKBOAT.ZEN`, `RX_InsertVobTree`).
  - Punkty dodane po zbudowaniu indeksu punktów świata nie trafiały do indeksu, więc nie dało się ich znaleźć po nazwie i odległość wynosiła zawsze 999999.
  - Dodatkowo takie punkty trafiały do tablicy, która przy rozrastaniu przenosiła wszystkie punkty w pamięci, a indeks trzymał wskaźniki na stare miejsca. To groziło błędami i zamknięciem gry.
- **Poprawka:**
  - Punkty dołożone później są przechowywane osobno, w miejscu, które się nie przesuwa, i dopisywane do indeksów (po nazwie, po położeniu, listy punktów o tej samej nazwie).
  - `Vob_GetDistToVob` znajduje też ruchome obiekty i wyzwalacze o podanej nazwie, a nie tylko punkty.
- **Do sprawdzenia:** rejs z Fregyalem – po dopłynięciu wczytuje się Zachodnie Wybrzeże. Zapis zrobiony na łodzi u celu powinien po wczytaniu od razu przenieść na wybrzeże.
- **Test (21.11):** Zachodnie Wybrzeże się wczytuje. Na łodzi brakowało Fregyala i możliwości pominięcia rejsu (poprawka 143).

### 141. Łódź odpływa bez bohatera – postacie nie jechały z ruchomymi obiektami – ✅ (21.11.2026)
- **Pliki:** `common/world/triggers/movetrigger.cpp`, `common/physics/physicvbo.cpp/.h`, `common/physics/dynamicworld.cpp`, `common/physics/physicmeshshape.cpp`
- **Zgłoszenie:** po „Pora odpływać” łódź popłynęła, a bohater i Fregyal zostali na miejscu.
- **Przyczyna:**
  - Łódź to ruchomy obiekt (mover `EVT_DRUIDBOAT`). W silniku PC postać stojąca na moverze jedzie razem z nim. W porcie mover przesuwał się pod stopami postaci, a one zostawały w miejscu.
  - Niewidzialna osłona łodzi (`NB_MAT_WC_BOAT_ALPHA`) w danych nie ma kolizji. Port takich materiałów w ogóle nie wkładał do siatki kolizji, więc skrypt nie mógł jej włączyć na czas rejsu („no mesh has it now” w logu).
- **Poprawka:**
  - Postacie, które stoją na ruchomym obiekcie (nad jego obrysem, do 2,5 m nad górą), przesuwają się razem z nim w każdej klatce ruchu – łódź, winda, platforma.
  - Materiały New Balance (`NB_MAT_*`) bez kolizji w danych są w siatce kolizji, ale wyłączone. `RX_SetMaterialCollision` może je włączyć (osłona łodzi na czas rejsu) i wyłączyć.
- **Do sprawdzenia:**
  - Rejs z Fregyalem: bohater i Fregyal płyną z łodzią na Zachodnie Wybrzeże.
  - Windy i ruchome platformy w innych miejscach gry dalej działają.
- **Test (21.11):** bohater i Fregyal płyną z łodzią. Po dopłynięciu nie wczytywał się nowy świat (poprawka 142).

### 140. Fregyal na łodzi: brak opcji „Pora odpływać” – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/triggers/abstracttrigger.h`
- **Zgłoszenie:** na łodzi trzeba porozmawiać z Fregyalem i wybrać „Pora odpływać” – tej opcji nie ma.
- **Przyczyna:** warunek tej opcji (`PC_PsionicQuest_TemplateDialog_561_Condition`) sprawdza funkcją pluginu `RX_CheckNpcStandsOnVob`, czy bohater i Fregyal stoją na łodzi (`EVT_DRUIDBOAT`). Port nie miał tej funkcji i zwracał 0, więc opcja nigdy się nie pokazywała.
- **Poprawka:** `RX_CheckNpcStandsOnVob(postać, nazwa)` – na PC plugin patrzy w dół spod postaci i porównuje nazwę obiektu pod nią (UNION_ABI.DLL 0x1017f360, 0x10054b40). Tutaj: postać stoi nad pudełkiem obiektu (podnośnik, wyzwalacz) o tej nazwie – do 2,5 m nad jego górą.
- **Do sprawdzenia:**
  - Wejście z Fregyalem na łódź, rozmowa – opcja „Pora odpływać” i rejs.
  - W logu `[ctx] stands on EVT_DRUIDBOAT: … yes/no`. Dopisek „no object of this name” oznacza, że łodzi nie ma wśród obiektów, które port sprawdza.
- **Test (21.11):** opcja jest, ale łódź odpłynęła bez bohatera i Fregyala (poprawka 141).

### 139. Łódź druida Fregyala: niewidzialna ściana, nie da się popłynąć na Zachodnie Wybrzeże – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/physics/physicvbo.cpp/.h`, `common/physics/dynamicworld.cpp`
- **Zgłoszenie:** nie da się wejść na łódź z Fregyalem – stoi przed nią niewidzialna ściana.
- **Przyczyna:**
  - Łódź ma niewidzialną osłonę z materiału `NB_MAT_WC_BOAT_ALPHA`. Na PC skrypty wyłączają jej kolizję funkcją pluginu `RX_SetMaterialCollision`, a włączają ją tylko na czas rejsu, żeby bohater nie wypadł z płynącej łodzi. Port nie miał tej funkcji.
  - Plugin po pełnym wczytaniu świata (nowa gra, zapis, zmiana świata) wywołuje `RX_OnEnterLoc_<ŚWIAT>` i `RX_OnEnterLocAny` (UNION_ABI.DLL 0x1027eb7c). W `RX_OnEnterLoc_NewWorld` skrypt właśnie wyłącza osłonę łodzi, gdy łódź nie płynie. Port tego nie wywoływał.
- **Poprawka:**
  - `RX_SetMaterialCollision(materiał, wł.)` – jak w pluginie (0x10180ba0) wyłącza lub włącza kolizję wszystkich trójkątów danego materiału: w świecie i w obiektach, także tych wczytanych później.
  - `RX_MoverIsActive(nazwa)` – czy podnośnik (mover) o tej nazwie jest w ruchu (otwiera się albo zamyka), jak w pluginie (0x1008de00). Skrypty sprawdzają tak koniec rejsu łodzi.
  - Po wczytaniu każdego świata wywoływane są `RX_OnEnterLoc_<ŚWIAT>` (NEWWORLD, ADDONWORLD, WESTCOAST…) i `RX_OnEnterLocAny`, jak w pluginie.
- **Do sprawdzenia:**
  - Wejście na łódź z Fregyalem i rejs na Zachodnie Wybrzeże.
  - W logu `[ctx] enter location: RX_ONENTERLOC_NEWWORLD` i `[ctx] material NB_MAT_WC_BOAT_ALPHA collision off`.
- **Test (21.11):** na łódź da się wejść, ale brakowało opcji odpłynięcia w rozmowie z Fregyalem (poprawka 140). W logu materiał osłony nie został znaleziony w żadnej siatce kolizji („no mesh has it now”) – do obserwacji przy rejsie.

### 138. Wydobywanie żelaza, złota i magicznej rudy; kolor żyły w celowniku – 🧪 (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/world/objects/interactive.h`, `common/mainwindow.cpp` (`focusColor`)
- **Zgłoszenie:** wydobywanie surowców nie działa. Żyła z rudą ma mieć zieloną nazwę, pusta – białą lub szarą.
- **Przyczyna:** skrypty NB (`PC_IronHacken_Addon_Hour_Info`, `PC_OreHacken…`, `PC_GoldHacken…`) pytają plugin, ile rudy udało się wydobyć (`RX_InteractMobOre`). Port nie miał tej funkcji i zwracał 0 – nic się nie wydobywało.
- **Jak to działa na PC (UNION_ABI.DLL 0x1016f620, 0x100b0d40):** żyła trzyma zapas rudy w nazwie swojego obiektu. Przy pierwszym uderzeniu dostaje zapas: żelazo 95–105, złoto 79–81, magiczna ruda 9–11. Każde wydobycie zabiera z niego tyle, ile skrypt chce, ale nie więcej, niż zostało.
- **Poprawka:**
  - `RX_InteractMobOre` i `RX_InteractMobOreGetCount` jak w pluginie. Zapas zapisuje się w zapisie gry razem z żyłą.
  - Nazwa żyły w celowniku: zielona, gdy jest w niej ruda (także nieruszona żyła), szara, gdy jest wyczerpana.
- **Do sprawdzenia:**
  - Kopanie żyły żelaza, złota i magicznej rudy – ruda trafia do ekwipunku, aż żyła się wyczerpie.
  - Kolor nazwy przed i po wyczerpaniu.
  - W logu `[ctx] ore: …`.

### 137. Osobny pasek doświadczenia – ✅ (21.11.2026)
- **Pliki:** `common/mainwindow.cpp` (`drawTouchBars`), `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.h`, `common/game/nboptions.h`
- **Zgłoszenie:** licznik doświadczenia był przyczepiony do paska wytrzymałości. Ma być osobnym, przesuwanym paskiem, jak na PC (plugin UNION_XP_BAR), włączanym w przełącznikach gry.
- **Zmiana:**
  - Nowy pasek doświadczenia na HUD, domyślnie na środku nad paskiem wytrzymałości. W edytorze HUD da się go przesunąć, zmienić jego rozmiar i ukryć, tak jak paski życia, wytrzymałości i many.
  - Pasek pokazuje postęp w bieżącym poziomie, jak na PC: na pasku napis „Poz. N  zdobyte/potrzebne (procent)”. Zakres poziomu liczony jak w `RX_CalcNextLevelExp` skryptów NB: 500 × (poziom+1), od poziomu 10 – 600, 20 – 650, 30 – 800, 40 – 900, 50 – 1000, 60 – 1200.
  - Przełącznik „Pasek doświadczenia” w ustawieniach NB (interfejs), domyślnie włączony.
  - Ze stałym paskiem znika stary licznik nad wytrzymałością, a jego przełącznik „XP counter” nie jest pokazywany w ustawieniach dotyku przy New Balance.
- **Do sprawdzenia:**
  - Pasek widoczny nad wytrzymałością, wartości po zdobyciu doświadczenia.
  - Przesunięcie i zmiana rozmiaru w edytorze HUD.
  - Wyłączenie w ustawieniach NB.
- **Test (21.11):** pasek wygląda dobrze.

### 136. Usuwanie pustych ciał postaci i zwierząt – ✅ (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h` (`nbBodiesTick`), `common/game/nboptions.h`, `common/mainwindow.cpp`
- **Zgłoszenie:** trudno odróżnić ciała, które jeszcze coś mają, od pustych (np. gdy ognik nie zebrał łupu z goblina wojownika albo ze szkieletów). Usuwanie pustych ciał pomoże też wydajności.
- **Zmiana:**
  - Ciało martwej postaci lub zwierzęcia, z którego nie ma już czego zabrać (przeszukanie nie pokazałoby żadnego przedmiotu), znika po 2 s.
  - Ciało z przedmiotami zostaje, dopóki nie zostanie opróżnione – wtedy znika po 2 s.
  - Nic nie znika, gdy otwarty jest ekwipunek, przeszukiwanie, dialog, dokument, okno handlu NB lub rzemiosła, ani w trakcie przerywnika.
  - Nie dotyczy bohatera ani postaci nieprzytomnych.
  - Przełącznik „Usuwanie pustych ciał” w ustawieniach NB (rozgrywka), domyślnie włączony.
- **Do sprawdzenia:**
  - Zabicie wilka bez umiejętności skórowania (puste ciało) – znika po kilku sekundach.
  - Zabicie bandyty – ciało zostaje do przeszukania i znika po zabraniu wszystkiego.
  - W logu `[ctx] bodies: N empty removed`.
- **Test (21.11):** działa. Czas skrócony z 6 s do 2 s – 6 s było za długo.

### 135. Szybki atak: bohater biegł dalej sam w stronę celu – ✅ (20.11.2026)
- **Pliki:** `common/game/playercontrol.cpp`
- **Zgłoszenie:** po serii szybkich ataków (przycisk QUICK, czar lub broń) bohater biegł dalej w stronę celu i atakował kolejne postacie, choć ekran nie był dotykany.
- **Przyczyna:** log z poprawki 134 pokazał bieg bez żadnego sterowania: pusta kolejka AI, brak stanu, brak wciśniętego ruchu, a w tym czasie trwał szybki atak. Obsługa szybkiego ataku kończy się przed kodem ruchu, więc animacja biegu z chwili dotknięcia QUICK nigdy nie była zatrzymywana. Obrót do celu w tej samej obsłudze kierował bieg prosto na cel. Przy czarach szybki atak czekał przy tym na postawę stojącą, której bieg nie dawał, więc trwał do końca limitu czasu, a kolejne dotknięcia go odnawiały.
- **Poprawka:** w trakcie szybkiego ataku (czar i broń) bohater bez gałki i strzałek zatrzymuje bieg i stoi. Z wciśniętą gałką dalej biegnie jak dotąd.
- **Do sprawdzenia:** seria QUICK z czarem i z mieczem w biegu, potem puszczenie gałki – bohater staje i atakuje w miejscu.
- **Test (20.11):** działa.

### 134. Diagnostyka: co porusza bohatera, który biegnie sam do celu – ✅ (20.11.2026)
- **Pliki:** `common/world/objects/npc.cpp/.h`, `common/game/playercontrol.cpp/.h`, `common/mainwindow.cpp`, `common/utils/androidinput.h`
- **Zgłoszenie:** po poprawce 133 bohater dalej sam biegnie do celu i go atakuje (czar 508 w obywatela, potem w Thorbena), choć ekran nie jest dotykany.
- **Zmiana:** sam log, bez zmiany zachowania gry. Linijki `[hero-ai]`:
  - `perception N of the hero: FUNKCJA` – skrypt uruchomiony przez percepcję bohatera (np. trafienie);
  - `a command for the hero: act=N target=…` – polecenie AI dla bohatera poza przerywnikiem i dialogiem;
  - `the hero moves by himself (N m/s): …` – co sekundę, gdy bohater przesuwa się o ponad 2 m bez gałki i strzałek. Zawiera kolejkę AI, cel ruchu, stan, cel walki oraz stan sterowania (przód, akcja, szybki atak).
- **Do sprawdzenia:** powtórzyć sytuację i przesłać log z linijkami `[hero-ai]`.
- **Test (20.11):** log wskazał przyczynę – bohater biegł (stan ciała BS_RUN) z pustą kolejką AI, bez stanu i bez wciśniętego ruchu, zawsze w trakcie szybkiego ataku (poprawka 135).

### 133. Bohater biegnie sam do przodu i atakuje – ⏸ nie pomogło (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/mainwindow.cpp`, `common/game/playercontrol.cpp/.h`, `common/utils/androidinput.h`
- **Zgłoszenie:** nagranie i log z miasta – po walce bohater biegł sam do strażnika i rzucał w niego czar, choć ekran nie był dotykany.
- **Przyczyna:** skutek uboczny poprawki 132. Skrypty NB (`B_AssessDamage`) wywołują zmianę celu dla każdej trafionej postaci, także dla bohatera. Gdy bohatera trafił ktoś inny niż jego cel, skrypt uruchamiał na nim stan `ZS_Attack` i bohater był sterowany jak NPC. Na PC silnik nie uruchamia stanów AI na bohaterze, więc tam to nie występuje.
- **Poprawka:**
  - Funkcje zmiany celu (`RX_CanReAttack`, `RX_CanReAttackNow`, `RX_HasReAttackNpc`, `RX_SetReAttackNpc`) nie działają dla bohatera.
  - Dodatkowe zabezpieczenie: co klatkę sprawdzane jest, czy nakładka dotykowa dalej trzyma przód / tył. Jeśli nie, a ruch bohatera jest wciąż włączony, zostaje puszczony. Log: `touch: a movement key without its release was let go (…)`.
- **Do sprawdzenia:** walka w mieście ze strażnikami – po trafieniu bohater nie rusza się sam.
- **Test (20.11):** nie pomogło – bohater dalej biegnie do celu. Log nie pokazał zgubionego klawisza, a OpenGothic nie pozwala uruchomić `ZS_Attack` na bohaterze, więc przyczyna jest gdzie indziej (diagnostyka w 134). Zabezpieczenia zostają.

### 132. Zmiana celu w walce – potwór bił tylko bohatera, choć ranił go przywołaniec – ✅ (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie:** troll atakował tylko bohatera, a przywołany golem bił go i zabrał mu połowę życia.
- **Przyczyna:** skrypty NB (`B_MM_AssessDamage`, `B_AssessDamage`, `ZS_MM_Attack_Loop`, `ZS_Attack_Loop`) zmieniają cel przez funkcje pluginu `RX_CanReAttack`, `RX_CanReAttackNow`, `RX_SetReAttackNpc`, `RX_HasReAttackNpc`, `RX_GetReAttackNpc` i `RX_ClearReAttackNpc` (UNION_ABI.DLL 0x10183dc0–0x10184050). Port ich nie miał, a domyślne 0 trzymało każdą postać przy pierwszym wrogu.
- **Poprawka:**
  - Funkcje działają jak w pluginie. Postać trafiona kolejny raz przez kogoś innego niż jej cel odwraca się do niego od razu. Jeśli akurat zadaje cios, zapamiętuje go i odwraca się po ciosie (pętla ataku).
  - Dotyczy potworów i ludzi, nie tylko przywołańców.
- **Do sprawdzenia:**
  - Walka z trollem razem z golemem: troll po kilku ciosach golema powinien odwrócić się do niego.
  - W logu `[ctx] reattack: …`.
- **Test (20.11):** działa – troll odwraca się do golema.

### 131. Panel przywołańców na ekranie, teleport przywołańców z bohaterem – ✅ (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/game/gamesession.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp/.h`, `common/mainwindow.cpp/.h`
- **Zgłoszenie:** brak sterowania przywołańcami na telefonie; przywołańcy nie przenoszą się z bohaterem po teleporcie.
- **Poprawka:**
  - Nowy przycisk na ekranie (trzy fioletowe postacie), widoczny, gdy bohater ma przywołańców. Ognik i zwierzaki się nie liczą. Przycisk da się przesunąć i ukryć w edytorze HUD.
  - Dotknięcie rozwija trzy opcje po lewej stronie przycisku:
    - **ATAK** – wszyscy przywołańcy atakują cel w celowniku bohatera. Tryb pasywny przełącza się przy tym na aktywny. To odpowiednik `RX_Cmd_Enemy` z pluginu.
    - **AKTYWNE / PASYWNE** – przełącza tryb przez `RX_QuickPanel_SummonMode` ze skryptów NB. W trybie pasywnym przywołańcy nikogo nie atakują. Na przycisku widać bieżący tryb.
    - **RESET** – odsyła wszystkich przywołańców; ognik i zwierzaki zostają.
  - Teleport: gdy bohater w jednej chwili przeniesie się o ponad 12 m w tym samym świecie (runa, kamień teleportacyjny, konsola), przywołańcy i zwierzaki z miejsca, które opuścił, pojawiają się za nim.
  - Zmiana świata (ZEN): przywołańcy znikają i nie zostają w starym świecie. Ognik wraca sam, jeśli był przywołany (mod `nb-wisp`).
- **Do sprawdzenia:**
  - Przywołanie golema; ATAK na wybranego wroga; przełączenie trybu (napis na ekranie i na przycisku); RESET.
  - Teleport runą z przywołańcem.
  - Przejście do innego świata.
  - W logu `[ctx] summons: …`.
- **Test (20.11):** działa – panel, tryby, reset, teleport i zmiana świata.

### 130. Troll dalej nie rzuca kamieniami – ✅ (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp` (`nbTrollTick`)
- **Zgłoszenie:** dwa nagrania z telefonu – troll przy jaskini goni bohatera i bije z bliska, ale z dystansu nie rzuca.
- **Przyczyna (prawdopodobna):** gdy troll był w trakcie animacji, której nie można przerwać (bieg, obrót, cios), start `T_STONETHROW` się nie udawał, a kod brał to za brak animacji w modelu i wstrzymywał tego trolla na minutę.
- **Poprawka:**
  - Brak animacji sprawdzany osobno (`hasAnim`); gdy animacja jest, a nie wystartowała, troll przerywa bieżącą animację i rzuca – jak plugin.
  - Log: `[ctx] troll … starts the throw (N m)` przy rzucie, a co 5 s `[ctx] troll … does not throw: …` z przyczyną (cel za daleko, nie widzi celu, brak animacji w modelu, animacja nie ruszyła).
- **Do sprawdzenia:** walka z trollem z dystansu 9–30 m; jeśli dalej nie rzuca – log z linijkami `troll`.
- **Test (20.11):** troll rzuca kamieniami.

### 129. „Nie możesz przywołać więcej stworzeń” – licznik przywołańców tylko rósł – ✅ (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h` (`nbSummonCountTick`)
- **Zgłoszenie:** przy próbie przywołania kamiennego golema gra pisze, że nie można przywołać więcej stworzeń.
- **Przyczyna:** skrypty NB przy każdym przywołaniu dodają do `RX_SummonCount` miejsca stworzenia (golem i demon 2, wilk i szkielet 1) i porównują je z limitem. Odejmuje plugin, gdy przywołaniec zginie albo zniknie (UNION_ABI.DLL 0x101384b0). Port tego nie miał – licznik tylko rósł, aż blokował każde przywołanie.
- **Poprawka:** co 0,5 s licznik liczony od nowa z żywych przywołańców bohatera. Miejsca jak w skryptach (42 rodzaje stworzeń) i w pluginie: zwierzak mrocznego rycerza 4, Sid 1 albo 2, przywołany golem 2, pozostałe 1. Ognik, zwierzaki (NIXDOG, Jina) i totem nie zajmują miejsc. Zawyżony licznik ze starego zapisu poprawia się sam po wczytaniu.
- **Do sprawdzenia:** przywołanie golema; po jego śmierci kolejne przywołanie; w logu `[ctx] summon places: A -> B`.
- **Test (20.11):** działa.

### 128. Trolle rzucają kamieniami – ✅ (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h` (`nbTrollTick`), `common/world/world.cpp/.h` (`shootBulletFrom`)
- **Zgłoszenie:** trolle nie rzucają kamieniami – to mechanika New Balance, na PC robi ją plugin.
- **Jak to działa na PC (UNION_ABI.DLL 0x103889b0, 0x10389560):** troll (`GIL_TROLL`), który biegnie do wroga dalej niż 8,5 m, czasem staje i gra animację `T_STONETHROW` swojego modelu: dalej niż 12 m z szansą 2% na klatkę, dalej niż 8,5 m z 1%. Po rzucie przerwa 7,5–12,5 s albo 10–16 s. Kamień (`ItTrollStone`) wylatuje z prawej ręki w stronę wroga.
- **Poprawka:** to samo w silniku – sprawdzane 10 razy na sekundę dla trolli w promieniu 40 m od bohatera, gdy widzą cel (8,5–30 m). Kamień leci jak pocisk z prawej ręki trolla, w połowie animacji, z obrażeniami obuchowymi równymi sile trolla.
- **Nie ma:** rzutu goblinem (`T_GOBBOTHROW` – troll łapie pobliskiego goblina), zmiany tempa animacji zależnie od odległości i fizyki kamienia z pluginu (toczenie, dźwięk upadku). Moment wypuszczenia kamienia jest przybliżony – OpenGothic nie czyta zdarzeń `TRO_STONETHROW` z animacji.
- **Do sprawdzenia:** walka z trollem z dystansu (np. łuk) – co kilka sekund rzut kamieniem; w logu `[ctx] troll throws a stone`.

### 127. Bonusy przedmiotów (korona królów +15 many) i manekiny treningowe – ✅ (20.11.2026)
- **Pliki:** `common/game/inventory.cpp/.h` (`applyItemEffects`), `common/game/gamesession.cpp`, `common/world/objects/npc.h`, `common/game/gamescript.cpp/.h` (`nbIsTrainingDummy`), `common/world/worldobjects.cpp`, `common/mainwindow.cpp`
- **Zgłoszenie:** korona królów nie dodaje 15 many; sprawdzić bonusy broni, hełmów, pasów, pierścieni i trofeów. Manekiny przy farmie Onara i przy straży miejskiej są „przyjazne” – bez włączonego celowania w pokojowe stworzenia nie da się na nich trenować.
- **Przyczyna (bonusy):** część przedmiotów ma bonus zapisany w polach `change_atr`/`change_value` – oryginalny silnik dodaje go sam przy zakładaniu (oCNpc::AddItemEffects) i odejmuje przy zdejmowaniu. OpenGothic tego nie robił. W skryptach NB to 183 przedmioty: 159 zbroi i hełmów (np. korona królów `ITAR_SHRECHELM`: +15 maks. many), 20 broni białych, 2 dystansowe, pas i jedno trofeum – głównie maks. zdrowie, 8 z maks. maną. Pierścienie, amulety i większość trofeów (np. kieł Muritana) dają bonusy skryptami przy zakładaniu – te działały.
- **Poprawka:**
  - Bonusy `change_atr` dodawane przy zakładaniu i odejmowane przy zdejmowaniu każdego przedmiotu (zbroja, hełm, pas, broń, tarcza, pierścienie, amulet).
  - Zapisy ze starszej wersji (bez wpisu `game/item_effects`): po wczytaniu bonusy założonych przedmiotów są dodawane raz – bez tego zdjęcie przedmiotu odjęłoby bonus, którego nigdy nie było.
  - Manekiny (`AIM`, `AIM_100/300/600`, `aivar[43] = ID_TRAINING_AIM`) zawsze dają się zaznaczyć, także przy wyłączonym celowaniu w pokojowe stworzenia; nazwa w fokusie biała zamiast zielonej.
- **Do sprawdzenia:** korona królów – maks. mana +15 po założeniu, −15 po zdjęciu; zbroja z bonusem zdrowia – maks. HP; manekin przy farmie Onara z wyłączonym celowaniem w pokojowe stworzenia.
- **Test (20.11):** działa – korona dodaje manę, manekiny zawsze można zaatakować.

### 126. Obrażenia czarów rosną z inteligencją (`RX_DamageCalcRunes`) – ✅ (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h` (`nbDamageCalcRunes`), `common/game/definitions/spelldefinitions.cpp/.h` (`refresh`)
- **Zgłoszenie:** czary nie zadają większych obrażeń po zwiększeniu inteligencji.
- **Przyczyna:** na PC obrażenia i koszt każdego czaru liczy plugin (UNION_ABI.DLL 0x10083400) – skrypty po zmianie inteligencji wołają tylko `RX_DamageCalcRunes`. Port tej funkcji nie miał. Do tego OpenGothic czytał dane czarów raz, na starcie gry, a oryginalny silnik przy każdym rzucie.
- **Poprawka:**
  - Wzory przepisane z DLL dla ok. 140 czarów (`SPL_DAMAGE_*`, `SPL_COST_*`, `STEP_*`): podstawa czaru + część inteligencji. Część zależy od gildii (magowie: 1/4 inteligencji, w szacie maga więcej – do całej) i rośnie wolniej po przekroczeniu progu danego kręgu czaru. Dochodzą: nauczone zaklęcia (`countLearnSpell`, procent), Pełna synchronizacja (−15% części inteligencji), klątwa (−50%), droga szamana (−75%).
  - Czary paladynów i templariuszy (święty pocisk, odpędzenie zła, uderzenia): podstawa + cała inteligencja, u templariuszy z umiejętnością także siła albo zręczność.
  - Po każdej zmianie wartości dane czarów są tworzone od nowa – nowe obrażenia działają od następnego rzutu.
  - Przeliczenie także po wczytaniu gry i co 3 s (szata maga, klątwa, zmiana gildii), bo te wartości nie są w zapisie gry.
- **Nie ma:** tryb pluginu z inną tabelą (specjalna wersja moda) i karma bogów w obrażeniach czarów (`varNewKarmaEnabled` – nie ma jej w tych skryptach); obrażenia Szponu Beliara jako broni.
- **Do sprawdzenia:** Damage Numbers – obrażenia np. kuli ognia przed i po dodaniu inteligencji Marvinem; w logu `[ctx] spells: the damage counted again (intelligence N)`.
- **Test (20.11):** działa – runa bez broni magicznej 447 obrażeń, po założeniu broni (+75 inteligencji) 508.

### 125. Paczka 5: funkcje pluginu z logu z 20.11 – 🧪 (20.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/world.cpp/.h`, `common/world/objects/vob.cpp/.h`
- **Zgłoszenie:** log z 20.11 – „NB missing”: `NPC_HASACTIVEPERC` x234, `RX_CALLQUESTLOOP` x231, `ACTIVATEZSSLOT`/`EXT_PUTINSLOT` x429, `RX_ISNPCIGNORETARGET` x44, `MOB_SCRAMBLECONTAINERS`, `NPC_MODIFYBODYSTATE`, `RX_TOGGLEVOBVISIBLE`, `RX_RESETSUMMONS`, `RX_SETFXNPC`, `RX_DAMAGECALCRUNES`.
- **Poprawka (działanie odczytane z UNION_ABI.DLL):**
  - `RX_CallQuestLoop(nazwa)` – gdy zadanie jest w toku, wywołuje pętlę zadania `RX_QuestLoop_<nazwa>` (np. kamienie Yarka w Jarkendarze). Wcześniej te pętle nie działały wcale.
  - `RX_ToggleVobVisible(nazwa, pokaż)` – ukrywa/pokazuje obiekt świata razem z jego częściami (np. sekret Xardasa w zadaniu psioników). Stan trafia do zapisu gry i wraca po wczytaniu.
  - `Ext_PutInSlot(postać, kość, przedmiot)` – model przedmiotu na kości postaci: mieszek z monetami (`ITSL_GOLDPOCKET`), wędka rybaków (`ITSE_FANGEL`). `ActivateZSSlot` nie jest potrzebne.
  - `Npc_HasActivePerc(postać, percepcja)` – czy postać ma włączoną daną percepcję (np. strażnicy-dementorzy).
  - `Mob_ScrambleContainers()` – po każdym wczytaniu nowe, losowe kombinacje zamków skrzyń (L/R), jak na PC.
  - `RX_ResetSummons()` – po śmierci bohatera przywołane istoty znikają (zwierzaki NIXDOG i PET_JINA zostają).
  - `RX_IsNpcIgnoreTarget`, `Npc_ModifyBodyState`, `RX_SetFXNpc` – obsłużone bez skutków (lista celów pluginu jest pusta, OpenGothic nie ma modyfikatorów stanu ciała, sam zapis postaci dla efektów pluginu).
- **Jeszcze nie ma:** `RX_DamageCalcRunes` – przeliczanie obrażeń czarów po zmianie inteligencji (duża tabela wzorów w DLL, osobna poprawka).
- **Do sprawdzenia:** zadanie z kamieniami Yarka w Jarkendarze; sekret Xardasa w zadaniu psioników (ściana/obiekt znika i zostaje po wczytaniu); mieszek na pasie, wędki rybaków w porcie; w logu brak tych funkcji w „NB missing”.

### 124. „Wróć” w menu kapliczek i stołów NB nie działał – ✅ (19.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie:** przy kapliczce (wymiana karmy na złoto) „...(Wróć)” nie wychodzi z menu – znika, a pozostałe wybory zostają.
- **Przyczyna:** „Wróć” woła `RX_ClearDialog` → `RX_ClearCurrentDialog` (funkcja pluginu), której port nie miał – wybory nie były czyszczone.
- **Poprawka:** `RX_ClearCurrentDialog` czyści wybory dialogu, który właśnie się wykonuje, i menu wraca do swojej listy – dotyczy wszystkich menu NB z „Wróć”.
- **Do sprawdzenia:** kapliczka → wymiana karmy → „Wróć” – powrót do listy kapliczki.
- **Test (20.11):** działa.

### 121–123. Oznaczanie nowych przedmiotów w ekwipunku – ✅ (19.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp/.h`, `common/game/gamesession.cpp/.h`, `common/utils/androidinput.cpp/.h`, `common/mainwindow.cpp`, `common/game/nboptions.h`
- Dodano oznaczanie nowych przedmiotów w ekwipunku: znacznik **NOWY** na przedmiocie i złoty kwadracik na zakładce jego kategorii. Nieprzeczytane dokumenty są zapamiętywane w zapisie gry, a znacznik znika po ich przeczytaniu. Pozostałe przedmioty tracą znacznik po wybraniu i nie są zapamiętywane w zapisie.
- **Ustawienie:** Ustawienia NB → „Oznaczanie nowych przedmiotów w ekwipunku” (domyślnie włączone).
- **Test (20.11):** działa – nowe dokumenty są oznaczone także po wczytaniu gry w nowej sesji.

### 117–120. Ognik przywołany od startu gry, kilka ogników po kolejnych zapisach – 🧪 (19.11.2026)
- **Pliki:** `mods/nb-wisp/index.js`, `mods/nb-wisp/manifest.json` (wersja 1.2.1), gotowy `mods/nb-wisp.zip`; `common/scripting/game/game_events.cpp` (`world:load`), `common/scripting/game/game_npcs.cpp` (`npcs.all`), `common/world/world.h`, `common/game/gamesession.h` (`serialNumber`), `docs/MODS.md`
- **Zgłoszenie:** na starcie gry ognik jest już przywołany, a po kilku zapisach i wczytaniach są 4 ogniki. Ognik ma być od razu odsyłany, ale tak, żeby dało się go potem przywołać modem JS.
- **Przyczyna:**
  - Mod pamiętał „ognik włączony” na stałe (`storage`, poza zapisem gry), więc przywoływał go przy każdym starcie gry.
  - Ognik zapisuje się też w zapisie gry. Po wczytaniu mod szukał go przez `npcs.find`, które daje tylko pierwszą postać tej instancji. Gdy ta była martwa albo jej nie znalazł, dokładał nowego ognika – każde wczytanie dodawało jednego.
  - Wczytanie zapisu tego samego świata nie dawało modom żadnego zdarzenia (`world:change` przychodzi tylko przy innej nazwie świata).
- **Jak działa teraz – ognik na jedną sesję gry:**
  - Pierwsze wczytanie zapisu (albo nowa gra) po włączeniu gry jest bez ognika, także gdy ognik był w zapisie – zamknięcie gry go odesłało.
  - Ognika przywołuje tylko gracz: Shift+F5, zakładka „Ognik” w menu modów albo komenda `ognik`.
  - Przywołany ognik zostaje przy kolejnych wczytaniach i zmianach świata, aż do zamknięcia gry albo odesłania. Gdy wczytany zapis nie ma ognika, mod przywołuje go obok bohatera.
  - Odesłany ognik nie wraca też z wczytanego zapisu, w którym był.
  - Przywołany ognik, którego nie ma (zginął, bohater zginął, wczytany zapis bez ognika), wraca obok bohatera w ciągu 3 s – w konsoli modów `ognik: przywołany ponownie` (poprawka 120).
  - Zawsze najwyżej jeden ognik: przywołanie najpierw odsyła inne, a co 3 s mod usuwa nadmiarowe.
  - Komendy `ognik on` / `ognik off` (samo `ognik` przełącza) – także dla innych skryptów i konsoli (`nb-wisp.ognik on`).
- **Zmiany w silniku dla modów:**
  - Zdarzenie `world:load` (`{world, newGame}`) przy każdym nowym świecie: nowa gra, wczytany zapis (także tego samego świata), zmiana świata. Rozróżnia je numer obiektu świata i numer gry (`serialNumber`).
  - `npcs.all(instance)` – wszystkie postacie danej instancji w świecie, także martwe.
- **Po drodze:** 117 – odsyłanie ognika przy nowej grze i każdym wczytaniu; 118 – wczytany zapis zostawiał swojego ognika; 119 – ognik na jedną sesję gry, jak wyżej; 120 – po śmierci bohatera i wczytaniu zapisu bez ognika trzeba go było przywołać jeszcze raz, teraz wraca sam, dopóki jest przywołany.
- **Instalacja:** nowy build (zdarzenie `world:load` i `npcs.all` są w silniku) oraz nowy `mods/nb-wisp.zip` w launcherze (**Mods → Add a mod (.zip)**) – zastąpi starą wersję.
- **Do sprawdzenia:** włączenie gry i wczytanie zapisu z ognikiem – ognika nie ma (w konsoli modów `ognik: odesłano N`); przywołanie z menu – jest jeden; śmierć i wczytanie – ognik wraca po chwili; kolejne wczytanie – nadal jest jeden; zamknięcie gry, włączenie i wczytanie – znowu bez ognika.

### 116. Muzyka gry nie cichnie przy muzyce bossa – 🧪 (19.11.2026)
- **Pliki:** `common/gamemusic.cpp/.h` (`setScriptMute`, `applyEnabled`), `common/game/gamescript.cpp` (`RX_MusicOff`, `RX_MusicOn`, `RX_StartBossMusic`, `RX_UpdateBossMusicCoords`)
- **Zgłoszenie:** pierwszy boss (ogar koło jaskini zielarza): muzyka bossa ledwo słychać, bo cały czas gra zwykła muzyka gry, a powinna zostać wyciszona.
- **Przyczyna:** `RX_MusicOff` wyłączał muzykę tak samo jak opcja w ustawieniach. Na początku walki skrypty NB zapisują opcję `bCanSaveGame` (`Menu_WriteInt`, blokada zapisu w walce). Każdy zapis opcji wczytuje ustawienia od nowa, a razem z nimi „muzyka włączona” z ustawień – muzyka gry wracała zaraz po wyciszeniu.
- **Jak to działa na PC:** `RX_MusicOff` zapisuje `musicEnabled=0` w Gothic.ini i zatrzymuje muzykę, `RX_MusicOn` włącza ją z powrotem.
- **Poprawka:**
  - `RX_MusicOff`/`RX_MusicOn` ustawiają osobne wyciszenie od skryptów. Zapis innych opcji go nie zdejmuje, a opcja muzyki w ustawieniach zostaje nietknięta.
  - Wyciszenie kończy `RX_MusicOn` (śmierć bossa, ucieczka na 45 s, śmierć bohatera – przez `RX_StopBossMusic`) albo włączenie muzyki w ustawieniach przez gracza.
  - Nowa gra i wczytanie zapisu zaczynają bez wyciszenia; zapis z walki z bossem wycisza muzykę znowu przy następnym ciosie.
  - Gdy utworu bossa nie ma w plikach gry, muzyka gry nie zostaje wyciszona.
- **Do sprawdzenia:** walka z ogarem – słychać tylko muzykę bossa; po zabiciu bossa albo ucieczce wraca zwykła muzyka.

### 113. Koło szybkiego wyboru nie reaguje na dotyk – 🧪 (18.11.2026)
- **Pliki:** `common/utils/androidinput.cpp` (`onInput`)
- **Zgłoszenie:** z koła nie da się niczego wybrać.
- **Przyczyna:** na Androidzie dotyk szedł prosto do przycisków (`controls.down/move/up`), z pominięciem `pointerDown/Move/Up`, w których koło (111) przejmuje palce. Koło się otwierało, ale nie widziało żadnego dotknięcia.
- **Poprawka:** dotyk Androida idzie przez `pointerDown/Move/Up`; anulowanie dotyku czyści też palec koła.
- **Do sprawdzenia:** dotknięcie pola koła – przedmiot użyty; puste pole – ekwipunek; przytrzymanie – zdjęcie z koła; środek – zamknięcie.

### 112. Koło szybkiego wyboru: 9 pól bez prążków, przycisk ze strzałkami ⇄ – 🧪 (18.11.2026)
- **Pliki:** `common/utils/androidinput.cpp/.h`, `common/game/nbhotbar.h`
- **Zgłoszenie (zrzut 342743, wzór 342745):** koło wygląda, jakby miało za dużo pól – prążki. Ma być 9 pól. Przycisk: dwie strzałki w przeciwnych kierunkach na czarnym kółku.
- **Przyczyna prążków:** każde pole było złożone z 10 półprzezroczystych trójkątów. Painter zaokrągla wierzchołki do pikseli, więc między trójkątami zostawały szpary. Wyglądało to jak kilkadziesiąt pól.
- **Poprawka:**
  - 9 pól po 40° (`NbWheel::Count` = 9, slot 8 nowy).
  - Trójkąty pola lekko na siebie zachodzą i mają pełny (nieprzezroczysty) kolor; granice między polami to czarne linie.
  - Przycisk: czarne kółko, górna strzałka w prawo, dolna w lewo.
- **Do sprawdzenia:** równe 9 pól, podświetlenie, nowy przycisk.

### 111. Koło szybkiego wyboru (drugi pasek) – 🧪 (18.11.2026)
- **Pliki:** `common/game/nbhotbar.h` (`NbWheel`), `common/utils/androidinput.cpp/.h` (koło: dotyk, rysowanie), `common/ui/mobilecontrolsmodel.cpp/.h` (przycisk `wheel`, `Special::Wheel` = 27), `common/ui/inventorymenu.cpp/.h`, `common/mainwindow.cpp`
- **Zgłoszenie (zrzuty 342740, 342739):** drugi pasek szybkiego wyboru: broń, mikstury, rośliny. Przycisk z kółkiem obok paska otwiera koło, a przedmioty można przypisać i tu (pasek), i tam (koło).
- **Poprawka:**
  - Nowy przycisk HUD (białe kółko, id `wheel`, obok plecaka; można go przesunąć w edytorze HUD).
  - Dotknięcie otwiera koło na środku ekranu: 8 pól z modelami przedmiotów i liczbą sztuk; gra jest wstrzymana. Palec można przesuwać po polach (niebieskie podświetlenie), wybór następuje przy puszczeniu; na środku nazwa przedmiotu z pola pod palcem; puszczenie na środku zamyka koło.
  - Wybrane pole używa przedmiotu (broń zakłada, mikstura/roślina – zużywa). Puste pole otwiera ekwipunek. Przytrzymanie pola (0,6 s) zdejmuje przedmiot z koła.
  - Na karcie przedmiotu w ekwipunku dwa przyciski: **NA PASEK / Z PASKA** (poprawka 110) i **NA KOŁO / Z KOŁA**.
  - Zapis: `[MOBILE_WHEEL] slot0..7` w `Gothic.ini`.
- **Do sprawdzenia:** przypisać mikstury i broń na koło, otworzyć koło w walce – wybór, przytrzymanie, puste pole.

### 110. Własne przedmioty na pasku szybkiego wyboru (broń, mikstury, rośliny) – 🧪 (18.11.2026)
- **Pliki:** `common/game/nbhotbar.h` 🆕, `common/ui/inventorymenu.cpp/.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp`, `common/mainwindow.cpp`
- **Zgłoszenie:** możliwość wkładania broni lub przedmiotu (mikstura, roślina) na pasek szybkiego wyboru.
- **Poprawka:**
  - Na karcie przedmiotu w ekwipunku nowy przycisk nad ZAŁÓŻ/WYRZUĆ: **NA PASEK SZYBKIEGO WYBORU**, a gdy przedmiot już tam jest, **ZDEJMIJ Z PASKA**. Dotyczy broni białej i dystansowej, jedzenia i mikstur; runy i zwoje trafiają tam same, jak w grze.
  - Przedmiot zajmuje pierwszy wolny slot 3–0 (bez runy i bez innego przedmiotu). Na ekranie napis „Na pasku: N” / „Zdjęte z paska: N” / „Pasek pełny”.
  - Dotknięcie slotu (albo klawisz 3–0) używa przedmiotu: broń zakłada, miksturę pije, roślinę je. Na pasku widać model przedmiotu, dopóki bohater go ma.
  - Zapis w `Gothic.ini`, `[MOBILE_HOTBAR] slotN` = nazwa instancji (wspólne dla wszystkich zapisów gry).
- **Akrobatyka (108):** przełącznik zapisuje `MOBILE/acrobatic` i działa jak w ustawieniach dotyku, ale skrypt `RX_ApplyAcrobatic` zakłada przewrotki tylko bohaterowi, który nauczył się akrobatyki (`NPC_TALENT_ACROBAT`) – bez nauki przełącznik nic nie zmienia.
- **Do sprawdzenia:** mikstura i roślina na pasek, dotknięcie – używa; druga broń na pasek – zakłada.

### 109. Zasięg rysowania postaci (jak suwak NB 3000–7000) – ⏸ zastąpione przez 146 (18.11.2026)
- **Pliki:** `shader/materials/visibility_pass.comp`, `common/graphics/drawcommands.cpp`, `common/game/nboptions.h`, `common/ui/gamemenu.cpp`
- **Zgłoszenie:** na PC w sterowaniu dodatkowym NB był suwak, jak daleko rysować NPC (od 3000). Na małej mapie, np. na Pustyni Adanosa, gracz ustawiał 7000 i widział postacie z daleka. W Khorinis zmniejszał, bo spadały klatki.
- **Poprawka:** nowe ustawienie na stronie NB (kategoria detali) „Zasięg rysowania postaci”: bez limitu / 30 / 40 / 50 / 60 / 70 / 100 m (`NB_OG/npcDistance`, zapisywany numer wyboru). W przebiegu widoczności GPU części modeli ze szkieletem (ludzie, potwory) dalej niż ten zasięg od kamery nie są rysowane. Zasięg widzenia świata i obiektów się nie zmienia. Działa od razu, bez wczytywania gry.
- **Uwaga:** postacie za zasięgiem dalej żyją (AI jak dotąd, OpenGothic liczy je w pełni do 30 m, potem rzadziej). Ustawienie oszczędza rysowanie, nie logikę.
- **Do sprawdzenia:** Khorinis – 30/40 m, czy przybywa klatek; pustynia – 70/100 m, postacie widać z daleka.

### 108. Ustawienia NB: osobne strony, tylko działające przełączniki, akrobatyka, ekran śmierci – 🧪 (18.11.2026)
- **Pliki:** `common/ui/gamemenu.cpp`, `common/game/nboptions.h`, `common/ui/mobilecontrolsmodel.cpp`, `common/mainwindow.cpp`
- **Zgłoszenie (zrzuty 18.11):** „Ustawienia gry” i „Sterowanie dodatkowe” pokazują tę samą listę przełączników NB. Prośba: przełączniki, które działają na Androidzie; akrobatyka w tym menu zamiast ustawień ekranu dotykowego; naprawić ekran śmierci.
- **Przyczyna duplikatu:** strona NB (`MENU_OPT_CONTROLS_NEWOPTIONS`) była rozpoznawana po dwóch pierwszych pozycjach menu, a strona klawiszy dodatkowych (`MENU_OPT_CONTROLS_EXTKEYS`) zaczyna się tak samo. Teraz NB tylko na stronie, która ma te dwie pozycje i nic więcej.
- **Co czytają skrypty (`Menu_ReadInt`, 204 wywołania):** z przełączników menu pluginu skrypty same używają m.in. `bShowKarma`, `bHFTStatus`, `bShowNotice`, `bShowDeadScreen` (PerFrame: muzyka śmierci), `bShowPoisonHero`, `bShowEffectPalUndeadProc`, `bSkipLogo`, `bToggleHardBosses`, `bCanSaveLeg`, `bToggleHeroVoice`, `bToggleBossMusic`, `bToggleMusicAnimalBoss`, `bTogglePartySprint`, `bScaleStr`, `bHidePocketsNpc` i 7 opcji Gniewu Beliara. Reszta menu pluginu zmienia rzeczy, które robi tylko sam plugin.
- **Lista przełączników (kolejność kategorii pluginu):**
  - interfejs: karma, efekty, głód, nazwy lokacji, **ekran śmierci**, zatrucie, efekt Paladyna, pomijanie logo;
  - rozgrywka: trudniejsi bossowie, zapis wszędzie, okrzyki bohatera, **muzyka bossów**, **muzyka bossów-zwierząt**, **sprint drużyny**, **akrobatyka**;
  - detale: skalowanie od siły, sakiewki NPC;
  - Gniew Beliara (5);
  - jasna noc (test).
  - Usunięte z listy: mieszek i torba (modeli na bohaterze jeszcze nie ma). Akrobatyka zniknęła z ustawień ekranu dotykowego (to samo ustawienie `MOBILE/acrobatic`).
- **Ekran śmierci:** plugin (`0x10222E00`) przy martwym bohaterze i `bShowDeadScreen` kładzie `YOU_DIE.TGA` na cały ekran. Teraz to samo, z rozjaśnieniem przez 2 s, aż do wczytania gry (tekstura z danych NB; bez niej nic się nie rysuje).
- **Do sprawdzenia:** strona NB i „Sterowanie dodatkowe” różne; przełączniki zapisują się; akrobatyka z menu działa; śmierć bohatera – obrazek NB.

### 107. Książka/list czytany z ekwipunku pod ramkami ekwipunku – 🧪 (18.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp`
- **Zgłoszenie (zrzut 18.11):** „Czytaj” w ekwipunku – dokument wyświetla się za ekwipunkiem, widać tylko jego skrawek między ramkami (puste pola, karta przedmiotu na wierzchu).
- **Przyczyna:** gdy dokument jest otwarty, ekwipunek jest „przykryty” (`setCovered`) – chowały się tylko modele przedmiotów, a ramki, pola i karta z poprawki 99 dalej się rysowały nad dokumentem.
- **Poprawka:** przykryty ekwipunek nic nie rysuje (ani tła, ani liczb); po zamknięciu dokumentu wraca.
- **Do sprawdzenia:** „Czytaj” na liście/książce w ekwipunku – dokument na całym ekranie, po zamknięciu znowu ekwipunek.

### 106. Obrażenia w czasie (na ekranie po lewej), obrażenia NB od broni bohatera, obrażenia obszarowe (splash) – 🧪 (18.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h` (`nbCallDamage`, `nbSplash`, `nbDotTick`, `RX_ApplyDotDamage*`, `RX_ClearAllDots`), `common/world/objects/npc.cpp/.h` (`nbSplashPct`)
- **Zgłoszenie:** obrażenia w czasie mają działać i być widoczne na ekranie po lewej: od ognia żółte, od trucizny zielone. Do tego obrażenia obszarowe: na PC cios mieczem (i czar) rani też cel obok.
- **Przyczyny (UNION_ABI.DLL):**
  - `RX_ApplyDotDamage(cel, napastnik, ile razy, obrażenia, DAM_*, co ile ms, odnów)` i `RX_ApplyDotDamageNotKill` nie były zrobione – smoki, deszcz ognia bossów, duchy, płonące i zatrute bronie nic nie robiły (31 wywołań).
  - Plugin przy każdym trafieniu bohatera woła `RX_CallDamage` (`0x10314430`) → `RX_MainDamageFunc`: premie z wytrzymałości i pancerza, procy run, efekty broni (`RX_WeaponDamage` – ponad 160 broni NB: ogień, trucizna, specjalne ciosy). Dodatkowe obrażenia wracają w `RX_GlobalAddDamage`. W OpenGothic tego w ogóle nie było.
  - Splash (`0x100BAA70`): cios bronią do walki wręcz rani też wrogów obok celu – w zasięgu broni i przed bohaterem. Kąt i część obrażeń zależą od rodzaju broni, a część rośnie z umiejętnością broni.
- **Poprawka:**
  - Obrażenia w czasie: lista „tików” (co `interval` ms, `ile razy`), obrażenia minus ochrona danego rodzaju (co najmniej 1/10), wersja NotKill zostawia 1 HP, „odnów” podmienia ten sam efekt zamiast dokładać drugi. `RX_ClearAllDots` czyści listę.
  - Na ekranie po lewej (od 33% wysokości, do 6 linii po 3 s) każdy tik na bohaterze albo zadany przez bohatera: „Ogień: -N” żółty, „Trucizna: -N” zielony, „Magia: -N” niebieski, inne czerwone; przy wrogu jego imię w nawiasie.
  - `RX_CallDamage` przy każdym trafieniu bohatera (miecz, łuk, czar) z `RX_LastDamageWasSpell`, `RX_LastSpellId`, `isSplashDamage`. `RX_GlobalAddDamage` dodawane do ciosu.
  - Splash: wrogowie (wrogie nastawienie albo walczący z bohaterem, bez przywołańców) w zasięgu ciosu i w kącie przed bohaterem:
    - miecz 1H – 42° i 45%;
    - topór 1H – 75° i 75%;
    - miecz 2H – 70° i 75%;
    - topór 2H – 80° i 85%.
    Z umiejętnością: 10% + (część − 10%) × umiejętność/100. Liczba trafionych idzie do logu (`splash: …`).
- **Jeszcze nie:** splash czarów (plugin robi go osobno przy zderzeniu czaru; czary z `NONSPLASH` w nazwie efektu go nie mają), koszt wytrzymałości splasha, dokładne tabele kątów pluginu dla wszystkich rodzajów broni (wzięte najczęstsze).
- **Do sprawdzenia:** walka z kilkoma wrogami mieczem/toporem – sąsiedni wróg też traci życie; płonąca/zatruta broń i ataki smoków/bossów – linie po lewej w kolorach.

### 105. Marvin: `add lp N` – punkty nauki do testów – ❌ (18.11.2026)
- **Pliki:** `common/marvin.cpp/.h` (`C_AddLp`)
- **Zgłoszenie:** do testu akrobatyki (86) bohater musi się jej nauczyć, a brakuje punktów nauki. Zręczność dało się dodać przez Marvina, punktów nauki nie (`set var` nie zmienia pól postaci, tylko zmienne skryptu).
- **Poprawka:** nowe polecenie konsoli Marvina `add lp 50` dodaje bohaterowi punkty nauki i wypisuje, ile ma teraz (liczba może być ujemna).
- **Test (18.11):** polecenie nie zadziałało – do sprawdzenia przy następnym teście (czy konsola przyjmuje polecenie, czy punkty nie przybywają). Test akrobatyki (86) przełożony.

### 104. Ekwipunek: ikony kategorii z Hedin's Advanced Inventory (z pliku VDF gracza) – ✅ (18.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp/.h` (`hedin`), `common/utils/androidinput.cpp` (kafelki kategorii)
- **Zgłoszenie:** z `Hedin_Union_Adv_Inventory.vdf` (z niego jest wzór ze zdjęcia 342701) wziąć ikonki i wygląd ekwipunku.
- **Zawartość VDF:** 67 tekstur (`_WORK/DATA/TEXTURES/_COMPILED`, DXT1/DXT3):
  - ikony kategorii `INV_*_SELECT`/`_NOT_SELECT`;
  - pola `INV_NEW_SLOT` i warianty `_HIGHLIGHTED` (turkus), `_EQUIPPED` (zieleń), `_H_SELECT`;
  - tło `INV_SIMPLE_BG_ITEM_1`;
  - strzałki `INV_ARROW_UP`/`_DOWN`;
  - dodatkowo DLL pluginu, `INV_NAMES.CFG` (nazwy kategorii po rosyjsku/angielsku/polsku).
- **Poprawka:** tekstury nie są w repozytorium (to pliki autora pluginu). Gra bierze je z VDF-a, gdy leży w folderze `Data` (OpenGothic czyta wszystkie VDF-y z `Data` i podfolderów). Gdy go nie ma, zostaje wygląd z poprawki 99.
  - Pola listy: `INV_NEW_SLOT`. Wybrane: turkusowe, założone: zielone, wybrane i założone: `INV_NEW_SLOT_H_SELECT`.
  - Kategorie: Wszystko `INV_ALL`, Broń `INV_WEAPON`, Pancerze `INV_ARMOR`, Biżuteria `INV_MAGIC` (pierścień), Magia `INV_MAGIC_RUNE`, Jedzenie `INV_FOOD`, Mikstury `INV_POTIONS`, Dokumenty `INV_DOC`, Inne `INV_OTHER`. Wybrana kategoria czerwona (`_SELECT`).
  - Ramki listy i karty przedmiotu: tło `INV_SIMPLE_BG_ITEM_1` (ciemne, z własną ramką), na karcie biała czcionka gry zamiast czcionki książek.
  - Strzałki przewijania: `INV_ARROW_UP`/`INV_ARROW_DOWN`.
- **Test (18.11):** z VDF-em w `Data` weszły tylko ikony kategorii; pola, tła i strzałki Hedina się nie wczytały (wczytywane w konstruktorze, za wcześnie). Zostaje tak: ikony Hedina + ramki i pergamin z 99. Wczytywanie pól/teł/strzałek wyłączone w kodzie (struktura `hedin` została, opis w komentarzu).

### 103. Szybki atak u maga – ✅ (18.11.2026)
- **Pliki:** `common/game/playercontrol.cpp/.h` (`QuickSpellSpeed`), `common/mainwindow.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.h`
- **Zgłoszenie:** QUICK ma działać też z magią w ręku, ale wolniej niż w walce wręcz. Przy okazji: w walce wręcz jednak 2,5× (2,2× za wolno).
- **Poprawka:** przycisk QUICK jest widoczny także z runą/zwojem w ręku (gdy jest mana). Dotknięcie rzuca czar z ręki `QuickHits` = 3 razy pod rząd, bohater zwrócony do celu. Czary ładowane wychodzą od razu na pierwszym poziomie. Animacje są przyspieszone `QuickSpellSpeed = 1.3f` (walka wręcz `QuickSpeed = 2.5f`). Limit to 4 s, a prędkość wraca do 1 po skończeniu ostatniego czaru. Brak many kończy serię.
- **Do sprawdzenia:** QUICK z runą – 3 czary szybciej niż normalnie. Tempo do ustawienia w `playercontrol.h` (`QuickSpellSpeed`: 1.0 = normalnie).
- **Test (18.11):** działa – czary nie co 2 sekundy, a tempo nie za wolne; `QuickSpellSpeed = 1.3f` zostaje.

### 102. Runa z bliska nie trafia – ✅ (18.11.2026)
- **Pliki:** `common/world/bullet.cpp` (`tickHoming`)
- **Zgłoszenie (film 342714):** czar z runy rzucony z bliska nie trafia: pocisk krąży nad celem.
- **Przyczyna:** naprowadzanie z poprawki 84 kieruje pocisk co klatkę w środek celu. Z bliska pocisk startuje w ciele celu albo do niego dolatuje, a fizyka sprawdza trafienie promieniem, który ze środka ciała niczego nie trafia. Pocisk kręcił się wokół środka, aż przeleciał 100 m.
- **Poprawka:** gdy naprowadzany pocisk jest bliżej środka celu niż ok. 70% połowy szerokości ciała (40–150 cm), trafienie liczone jest od razu (obrażenia, efekt uderzenia) i pocisk się zatrzymuje.
- **Do sprawdzenia:** runy z bliska (wróg tuż przy bohaterze) – trafiają za pierwszym razem; z daleka bez zmian.
- **Test (18.11):** działa – runa z bliska trafia.

### 101. Kruk i nieaktywne fazy bossów: RX_IsWaiting, RX_Stop_AI_Wait, UpdateSpeed, przywołańcy – 🧪 (17.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp/.h` (`nbSpeed`, `nbIsWaiting`, `nbStopWait`)
- **Zgłoszenie:** Kruk – największy boss z fazami.
- **Walka z Krukiem w skryptach:**
  - `BDT_50011_Addon_Raven` (w skryptach „AD_BOSS7”). Walka zaczyna się po dialogu (`PC_PsionicQuest_TemplateDialog_353_Var`).
  - Fazy przy 85, 70 i 40% życia oraz dalsze. Na każdą fazę Kruk staje się nieśmiertelny i czeka (`RX_SetInactive`, `AI_Wait` 500/1000 s), wzywa pomocników (`Wld_InsertNpcAndAttackEnemy`, duch Starego/`AD_OldGhostRavenBattle`), a co 30 s rani bohatera o 20% życia.
  - Potem jest forma demona (`Demon_RavenX`, `Demon_RavenX_Out`).
  - Wszystko przez `AI_OnTick_`/`AI_OnHit_`/`AI_OnGetDamage_`/`AI_OnDead_` (poprawki 100 i 51).
- **Brakowało (dopisane):**
  - `RX_IsWaiting` zawsze zwracało 0. Teraz zwraca, czy postać czeka (`AI_Wait`), więc boss w nieaktywnej fazie nie atakuje.
  - `RX_Stop_AI_Wait` – koniec czekania przy `RX_SetActive`.
  - `UpdateSpeed(npc, 1.0–2.0)` – szybszy boss (61 wywołań w skryptach): mnożnik zegara animacji postaci, także ruchu.
  - `AI_ClearAIQueue_Summoned`, `AI_Wait_Summoned(s)`, `AI_StandUp_Summoned` – przywołańcy bohatera (do 50 m) przerywają walkę, czekają i wstają, gdy boss jest nieśmiertelny.
- **Do sprawdzenia:** walka z Krukiem: kolejne fazy (napisy, pomocnicy, nieśmiertelność i powrót do walki), forma demona.

### 100. Fazy bossów: funkcje walki pluginu (AI_OnTick/OnHit/OnGetDamage/OnBreak) – 🧪 (17.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h` (`nbBossHit`, `nbBossTick`, `nbInstanceFunc`, `nbCallInstanceFunc`, `WLD_INSERTNPCANDATTACKENEMY`)
- **Zgłoszenie:** taktyki/fazy bossów NB (np. Kruk) – odłożone wcześniej jako duża robota.
- **Przyczyna:** fazy bossów są w skryptach (84 × `AI_OnTick_<instancja>`, 72 × `AI_OnHit_`, 137 × `AI_OnGetDamage_`, 9 × `AI_OnBreak_`). Np. `AI_OnTick_AL_Boss3` przy 75/50/15% życia włącza kolejne etapy (`AIVRX_NPC_BOSS_STAGE_*`), atak specjalny i leczenie. Wywołuje je plugin po nazwie instancji – w OpenGothic nikt ich nie wołał (działało tylko `AI_OnDead_`, poprawka 51), więc bossowie walczyli jak zwykłe potwory.
- **Poprawka (wg UNION_ABI.DLL):**
  - przy każdym trafieniu (`0x10054220`): napastnik i ofiara, które mają `AI_OnTick_<instancja>`, dołączają do walki bossów; potem `AI_OnHit_<napastnik>` (SELF napastnik, OTHER ofiara) i `AI_OnGetDamage_<ofiara>` (SELF ofiara, OTHER napastnik);
  - co sekundę (`0x100546A0`): licznik walki (`aivar[95]`, 1..300, zerowany przez `RX_ResetNpcTimer`) +1 i `AI_OnTick_<instancja>(licznik)` z SELF = boss;
  - przerwanie walki (`0x10054030`): boss martwy, 70 m od bohatera albo bohater martwy. Wtedy zmienne NB 35..51 (etapy, liczniki czarów) są zerowane, boss dostaje pełne życie, licznik spada do 0 i wołane jest `AI_OnBreak_<instancja>`;
  - po wczytaniu zapisu walki są odtwarzane z `aivar[95]>0`, jak w pluginie;
  - nowa funkcja `Wld_InsertNpcAndAttackEnemy(instancja, punkt)` (19 wywołań w walkach bossów): pomocnik pojawia się w punkcie i od razu atakuje bohatera (`B_Attack` / `ZS_MM_Attack`).
- **Jeszcze brak (log pokaże, które są potrzebne):** `UpdateSpeed` (prędkość bossa), `RX_ApplyDotDamage`/`RX_ClearAllDots` (obrażenia rozłożone w czasie), `AI_Wait_Summoned`/`AI_StandUp_Summoned`/`AI_ClearAIQueue_Summoned`, `AI_PlayEffect`.
- **Do sprawdzenia:** walka z bossem z listy (`nb-docs/SPIS_NB.xlsx`) – czy zmienia zachowanie przy kolejnych progach życia (napisy, czary, przywołania), czy po ucieczce na 70 m boss wraca do pełnego życia. W logu: `boss fight: … joins` / `broken`.

### 99. Nowy wygląd ekwipunku (dotyk) – ✅ (17.11.2026)
- **Pliki:** `common/ui/mobilecontrolsmodel.cpp/.h` (`inventoryLayout`), `common/ui/inventorymenu.cpp/.h`, `common/utils/androidinput.cpp`
- **Zgłoszenie (zrzut 342701):** przerobić interfejs ekwipunku na wzór ze zdjęcia: po lewej ramka z „Złoto: N” i nazwą kategorii z liczbą przedmiotów, kafelki kategorii nad siatką, siatka w wielu kolumnach; po prawej karta przedmiotu na pergaminie (nazwa, właściwości, obrazek w ramce, przyciski ZAŁÓŻ / WYRZUĆ) i krzyżyk w rogu.
- **Poprawka:**
  - Układ: siatka 9 kolumn (przy skrzyni/handlu 5 kolumn w każdej z dwóch ramek), wiersze dopasowane do wysokości ekranu, przewijanie palcem jak dotąd. Kategorie w jednym rzędzie nad listą gracza.
  - Ramki z metalu z nitami w rogach rysowane w kodzie; lista ciemna, karta przedmiotu pergaminowa.
  - Nagłówek: „Złoto: N” po lewej, nazwa kategorii (ze skryptów moda, bez nich polska/angielska) i liczba przedmiotów po prawej.
  - Karta: nazwa czcionką książek (`FONT_10_BOOK`), obrazek w ciemnym polu po prawej u góry, właściwości po lewej obrazka i pod nim (łamane po słowach, zmniejszane, gdy się nie mieszczą). Na dole przyciski z napisami: ZAŁÓŻ/ZDEJMIJ/UŻYJ/CZYTAJ, WYRZUĆ; przy skrzyni i handlu WEŹ/SCHOWAJ/KUP/SPRZEDAJ i PRZENIEŚ; obok mały przycisk „cały stos”. W NB napisy po polsku, w innych grach po angielsku.
- **Zrzut 342710 (17.11):** wygląda dobrze, ale opis ma za mało miejsca – w samym ekwipunku lista i karta przedmiotu dostały po połowie ekranu (było 34% na kartę).
- **Test (17.11):** działa – więcej widać naraz niż przy 3 kolumnach, zostaje tak. Możliwe drobne poprawki później: większa czcionka opisu, ramka zamiast plamy na wybranym polu, pasek życia nad ramką, czerwone numery skrótów.

### 85–98. Szybki atak – przycisk QUICK – ✅ (17.11.2026)
- **Pliki:** `common/game/playercontrol.cpp/.h` (`quickAttack`, stałe `QuickHits`/`QuickTime`/`QuickSpeed`), `common/world/objects/npc.cpp/.h` (`resetAttack`, `nbQuickSpeed`), `common/mainwindow.cpp`, `common/utils/androidinput.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `android/assets/touch-icons/quick_attack.png`
- **Zgłoszenie:** w NB szybki atak robi się na PC kombinacją klawiszy (Ctrl trzymany, A/D, W klikane). Każde W „resetuje pozycję” bohatera: reszta ciosu jest ucinana i od razu wchodzi następny. Seria szybkich ciosów często obala przeciwnika. Na telefonie nie da się tego wcisnąć, więc potrzebny jest osobny przycisk.
- **Jak działa teraz (98):**
  - Przycisk **QUICK** (ikona miecza z liniami ruchu) nad przyciskiem akcji, widoczny z bronią do walki wręcz w ręku. Można go przesunąć albo ukryć w edytorze HUD (id `quickattack`).
  - Jedno dotknięcie daje serię ciosów do przodu, jak z przycisku miecza (`swingSword`). Bohater obraca się do celu.
  - Zaraz po klatce uderzenia każdy cios jest ucinany (`Npc::resetAttack`, „reset pozycji”) i od razu wchodzi następny.
  - Na czas serii animacje bohatera są przyspieszone (zegar modelu × `QuickSpeed`). Potem prędkość wraca do 1.
  - Liczby w `playercontrol.h`: `QuickHits = 1` cios na dotknięcie, `QuickTime = 2000` ms limitu, `QuickSpeed = 2.3f`, `QuickSpellSpeed = 1.3f` (magia, poprawka 103). Historia tempa: 1,5× za wolno, 2,2× za wolno, 2,5× (18.11), 2,3× (19.11, przy jednym ciosie). Ciosy w serii: 3 → 1 (19.11).
  - **19.11 (poprawka 114) – jedno dotknięcie = jeden cios:** QUICK z bronią białą nie liczy już serii. Dotknięcie zleca jeden cios (×2,3), który gra do końca, potem tempo wraca do 1. Dotknięcie w trakcie ciosu czeka na trafienie, ucina resztę ciosu (reset pozycji jak W na PC) i zaczyna następny – więc kolejne dotknięcia dają kolejne ciosy. Magia bez zmian (`QuickHits`, `QuickSpellSpeed`).
  - **19.11 (poprawka 115) – tempo zależne od stylu walki:** animacje stylów walki NB mają różne własne tempo, więc jedno przyspieszenie nie pasowało do wszystkich broni. Przy broni orkowej ×2,3 jest dobre, szabla była za szybka, a rapier za wolny. QUICK sprawdza nakładkę animacji, którą NB założył bohaterowi (`RX_ApplyAnim_Weapons`): szabla (`HUMANS_1HDEXST1–3`) ×1,15 (`QuickSpeedSaber`, połowa), rapier/szpada (`HUMANS_RAPIER_ST1–3`) ×3,45 (`QuickSpeedRapier`, o połowę więcej). Broń orkowa i pozostałe style zostają przy ×2,3 (`QuickSpeed`). 🧪
  - **19.11 – kilka ciosów zamiast jednego:** cios rusza kilka klatek po `swingSword`, a QUICK wołał `swingSword` w każdej klatce, aż ruszył – zbierało się kilka zleceń i wychodziła seria. Teraz cios zlecany raz, następny najwcześniej po 0,4 s (`quickSwingAt`).
- **Droga do tego (85–97, w skrócie):**
  - 85–94: QUICK udawał klawisze PC w różnych kolejnościach i czasach: A/D → W → Ctrl, Ctrl + A/D + W, cios w bok osobnym klawiszem, sterowanie Gothic 1 na czas serii. Nic nie dawało tego co na PC. Ekran dotykowy ma sterowanie Gothic 2, gdzie A/D/W przy Ctrl to zwykły ruch, a oryginalny silnik czyta trzymane klawisze co klatkę.
  - 95: odkryty „reset pozycji” po ciosie (`resetAttack`).
  - 96: ciosy w bok wykonywane bezpośrednio z resetem.
  - 97: QUICK naciskał przyciski ekranu „< STEP” i miecz razem; przy żadnych czasach atak nie wchodził.
  - 98: gracz zauważył, że sam przycisk miecza robi właściwy cios, tylko za wolno. Przyspieszenie animacji i reset po trafieniu dały szybki atak jak na PC.
- **Test (17.11–19.11):** działa; tempo dopasowane do PC.
- **Pozostałości w kodzie:** `PlayerControl::quickCombo` (94) i `pendingReset` (95) są już nieużywane (zawsze wyłączone), a sekwencer klawiszy `combo` w `androidinput` jest pusty. Do usunięcia przy sprzątaniu.

### 86. Ustawienie: akrobatyka NB (przewrotki) – 🧪 (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp/.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/game/gamescript.cpp/.h`
- **Zgłoszenie:** przełącznik akrobatyki w ustawieniach – przewrotki w lewo/prawo zamiast kroków w bok (na PC klawisz `KEY_ACROBAT`), potrzebne m.in. do szybkiego ataku (85).
- **Jak to działa na PC:** `HandleEvent` przy klawiszu akrobatyki przełącza `RX_AcrobaticActive` i woła `RX_ApplyAcrobatic(hero)` – nakładka animacji `Humans_Acrobatic.MDS`, tylko gdy bohater ma talent akrobatyki (Cassia).
- **Poprawka:** nowy wiersz ustawień „Acrobatics” → `Acrobatics (rolls): ON/OFF` (domyślnie OFF; `Gothic.ini`, `MOBILE`, `acrobatic`, wersja ustawień 8 → 9). Gra co klatkę porównuje ustawienie z `RX_AcrobaticActive` skryptów; gdy się różnią – ustawia zmienną i woła `RX_ApplyAcrobatic` (bez talentu skrypt nakładki nie zakłada). Na PC (bez ekranu dotykowego) bez zmian – decyduje klawisz.
- Przycisk szybkiego ataku (85) dostał numer `Special::QuickAttack = 28` (26 zajął nowy wiersz ustawień).
- **Uwaga:** jeśli akrobatyka jest włączona, zanim bohater się jej nauczy – po nauce przełącz OFF/ON.
- **Do sprawdzenia:** z talentem akrobatyki: ON → kroki w bok z bronią to przewrotki, szybki atak z przewrotką; OFF → zwykłe kroki; w logu `[ctx] acrobatics: on/off`.

### 75. Log: powtarzany błąd „Accessing member … without an instance set” – 🧪 (17.11.2026)
- **Pliki:** `game/main.cpp`
- **Zgłoszenie (log 17.11):** 528 linii `[zenkit] Accessing member "C_NPC.AIVAR" without an instance set (in C_PLAYERISFAKEBANDIT)` w jednej sesji (do tego `ZS_DEAD`, `RX_ISSUMMONNPC`).
- **Przyczyna:** to nie błąd gry – `C_PlayerIsFakeBandit` (skrypt z oryginału) czyta `aivar` Ramona (`BDT_1071_Addon_Ramon`), którego nie ma w Khorinis. Odczyt daje 0, tak jak w oryginale; ZenKit tylko za każdym razem to logował (każda percepcja postaci).
- **Poprawka:** taka sama linia „Accessing member…” trafia do logu raz, potem tylko licznik przy 10, 100, 1000… (`(xN so far)`). Inne błędy ZenKita (wyjątki ze stosem wywołań dla ekranu błędu) bez zmian.

### 76. Brakujące funkcje pluginu: regeneracja many, pochodnia w ręce, rutyny – 🧪 (17.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie (log 17.11, `NB missing`):** `HLP_HASINSLOT` ×186, `AST_GETMANABONUS` ×168, `EXT_REMOVEFROMSLOT`, `RX_CLEARINPUT`, `RX_TERMINATEDIALOG`, `RX_CONTAINSROUTINE`.
- **Napisane według `UNION_ABI.DLL`:**
  - `AST_GetManaBonus` (0x1016a920) – regeneracja many bohatera w `RX_RegenCommonFunc`: maks. mana / 105 na wywołanie (/ 210 z gniewem Beliara), ułamki się sumują. Wcześniej 0 – **mana wracała wolniej niż na PC**;
  - `Hlp_HasInSlot` / `Hlp_GetSlotItem` – przedmiot w slocie modelu (`HERO_POISONED` co chwilę sprawdza płonącą pochodnię w lewej ręce, `FAKELISON`);
  - `Ext_RemoveFromSlot` (oCNpc::RemoveFromSlot bez upuszczania) – gaśnie płonąca pochodnia (skrypt sam daje zgaszoną), ozdoby na kościach znikają;
  - `RX_ContainsRoutine` (0x101837e0) – czy nazwa rutyny postaci zawiera tekst (Pablo i Bartok „GuideMitte”, walczący na arenie „ArenaPlace” nie reagują na broń). Rutyna z `Npc_ExchangeRoutine`, po wczytaniu zapisu – rutyna startowa;
  - `RX_TerminateDialog`, `RX_ClearInput` (po teleporcie runą) – nic więcej nie trzeba, dialog zamyka `AI_StopProcessInfos` przed nimi; teraz bez wpisu w logu.
- **Do sprawdzenia:** czy mana odnawia się szybciej (z czasem, jak na PC); w logu przy zapisie `NB missing` bez tych nazw.

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

### 68. Okno sprzedaży NB (Hanna, Kardif, Halvor, Kreol, Xardas…) – ✅ (15.11.2026)
- **Pliki:** 🆕 `common/ui/nbsalesmenu.cpp`, 🆕 `common/ui/nbsalesmenu.h`, `common/game/gamescript.cpp`, `common/gothic.h`, `common/mainwindow.cpp`, `common/mainwindow.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.h`, `CMakeLists.txt` (`CONFIGURE_DEPENDS` – nowe pliki .cpp trafiają do buildu same)
- **Zgłoszenie:** u Hanny (klejnoty, szkatułki), Kardifa (surowe mięso), Halvora (perły, muszle, kosztowności), Kreola i Xardasa-nekromanty po wybraniu „sprzedaj” nic się nie otwiera.
- **Przyczyna:** NB robi to oknem pluginu: skrypt czyści listę (`RX_Craft_PlayerSales_ClearProductList`), dodaje przedmioty z ceną za sztukę (`RX_Craft_PlayerSales_AddToProductList("ItMi_Ruby", 350)`, 126 wywołań) i otwiera okno (`RX_OpenCraft_PlayerSales(npc)` – UNION_ABI.DLL 0x1017EC40: okno rzemiosła pluginu w trybie 13). Żadnej z tych funkcji nie było.
- **Poprawka:** po zamknięciu dialogu otwiera się własne okno jak okno pluginu (zrzut z PC): lewa i prawa trzecia część ekranu na teksturach pluginu (`AS_BACK_CRAFT_LEFT/RIGHT/RECIPE/RECIPE_DESC/INGRIDIENT/PLUS/MINUS/CREATE.TGA` – bez nich zwykłe panele), środek wolny. Po lewej lista z listy skryptu: pergamin z ceną i ramka „nazwa (xN)” (brak przedmiotu – wyszarzone); po prawej: pergamin z ceną, nazwa, „Maksymalnie: N”, dwa pola „Cena” i „Razem” (tam, gdzie w pluginie są składniki), **−**, ilość, **+**, **Max** i duży przycisk **Sprzedaj**; złoto u góry, X zamyka. Na czas okna znikają przyciski nakładki, paski i cel (po teście 23:32: zębatka, klawiatura i „wstecz” zasłaniały okno). Napisy żółte (zielonej czcionki OpenGothic nie ma). Klawiatura: strzałki, Enter – sprzedaj, Escape – zamknij.
  - sprzedanie zabiera przedmioty i daje złoto według ceny z listy (komunikat „Sprzedano: … (+złoto)”);
  - bez obrazków przedmiotów (w pluginie są na pergaminach) – tam na razie cena.
  - **po teście 23:39:** krzyżyk nie zamykał okna. Dodany przycisk „wstecz” nakładki w prawym górnym rogu (Escape – zamyka okno, nie otwiera menu gry), większe pole dotyku krzyżyka i wpis w logu przy każdym dotknięciu okna (`NB sales window: tap x,y …`) – do sprawdzenia, czemu krzyżyk nie łapał. Pod oknem nie są już rysowane napisy NB (strzały, karma, liczniki), które prześwitywały przez wiersze.
- W logu `[ctx] sales window: N items`.
- **To samo okno w pluginie robi rzemiosło** (`RX_OpenCraft` z `CRAFT_ALCHEMY`, `CRAFT_SMITH`, `CRAFT_RUNE`, `CRAFT_FORCEARMOR`, `CRAFT_BOWS`, `CRAFT_JEWELRY`, `CRAFT_FOOD`, `CRAFT_PIE`, `CRAFT_SOUP_COMPOTE`, `CRAFT_SCROLLS`, `CRAFT_ORE_SMELTING`, `CRAFT_SKINS_ARMOR`, `CRAFT_DEMONRUNE`). Przepisy są tam zaszyte w kodzie pluginu (nie w skryptach) – na razie zostają stare menu dialogowe z poprawki 2; to okno jest pod nie przygotowane.
- **Test (18.11):** działa u wszystkich – handel w nowym oknie.

### 69. Okno sprzedaży u Constantino i Alfreda, napis „Złoto” w ekwipunku – ✅ (16.11.2026, sprawdzone 17.11)
- **Pliki:** `common/game/gamescript.cpp`, `common/ui/inventorymenu.cpp`, `common/game/nbtexts.h`
- **Zgłoszenie:** okno sprzedaży z poprawki 68 nie otwiera się u Constantino (trofea) ani w obozie myśliwych; w ekwipunku jest „Złota moneta : N” zamiast „Złoto”.
- **Przyczyna:** „Przekaż zebrane rośliny i składniki...” (Constantino, czeladnik) i „(Sprzedaj trofea zwierzęce)” (Alfred) nie wołają `RX_OpenCraft_PlayerSales`, tylko `RX_Form_Const_SellItems` / `RX_Form_Alfred_SellItems`. Te dwie funkcje mają listę towarów zaszytą w UNION_ABI.DLL (tabele 0x105692c8 i 0x10568b38) – u nas ich nie było, dialog się zamykał i nic. Funkcje skryptów `DIA_CONSTANTINO_SELLTROPHIES_CALLBACK` itp. to pozostałości – plugin ich nie używa.
- **Poprawka:** obie funkcje otwierają to samo okno z listą wyciągniętą z DLL:
  - Constantino (34 pozycje): mikstury permanentne, rzadkie rośliny, zioła i grzyby, trofea (kraby i żuwaczki mrówek tylko gdy je masz – jak w pluginie), serca golemów;
  - Alfred (23 pozycje): skóry, zęby, pazury, płyty pełzaczy, rogi.
  - w logu `[ctx] Constantino sales window: N items` / `Alfred …`.
- **Złoto:** w NB nagłówek złota w ekwipunku to „Złoto: N” (klucz `itemGold` z AB_Translation.lbr, jak w pluginie).
- **Po teście 03:35 (okno otwiera się u obu):** wszystko było w jednej liście i okno było przezroczyste. Teraz:
  - kategorie z tabel pluginu – Constantino: Mikstury, Rzadkie rośliny, Rośliny, Trofea, Serca golemów; Alfred: Skóry, Zęby i pazury, Rogi. Jak w oryginale są to zakładki nad listą, po dwie w rzędzie (wybrana jasna, reszta ciemna; najpierw był pasek „‹ nazwa ›” na środku – po teście 03:39 przerobione na zakładki). Klawiatura: Tab. Okno otwiera się na pierwszej kategorii, w której coś masz;
  - na środku dwie duże strzałki ↑ ↓ – przewijają listę o stronę (przyciemnione, gdy dalej nic nie ma); zniknął błędny znaczek „>” pod listą;
  - pod lewym i prawym panelem ciemne tło jak w oryginale.
  - Okna ze skryptów (Hanna, Kardif, Halvor…) zostają jedną listą, ale strzałki i tło mają te same.
  - **po teście 03:47 (zakładki, strzałki, tło – OK):** w pergaminach są obrazki przedmiotów 3D jak w ekwipunku (ten sam renderer co ekwipunek – `InventoryRenderer`), cena mała w rogu pergaminu; duży pergamin po prawej też pokazuje przedmiot.
- **Pliki (dodatkowo):** `common/ui/nbsalesmenu.cpp/.h`, `common/gothic.h`, `common/mainwindow.cpp`

### 70. Okno alchemii NB (stół alchemiczny jak na PC) – ✅ (16.11.2026)
- **Sprawdzone (zrzut 23:29):** okno otwiera się przy stole, „Stwórz” robi esencję many, umiejętność alchemii rośnie (77 → 78), skrypty dają doświadczenie i inteligencję.
- **Pliki:** 🆕 `common/game/nbcraft.h`, 🆕 `common/ui/nbcraftmenu.cpp`, 🆕 `common/ui/nbcraftmenu.h`, `common/game/gamescript.cpp`, `common/game/gamescript.h`, `common/gothic.h`, `common/mainwindow.cpp`, `common/mainwindow.h`
- **Zgłoszenie:** przy stole alchemicznym otwierało się stare menu dialogowe skryptów (poprawka 2), a na PC jest okno pluginu: zakładki, lista mikstur, „Umiejętność alchemii: N”, składniki i „Stwórz” (zrzut z PC).
- **Skąd dane:** przepisy są zaszyte w UNION_ABI.DLL (nie w skryptach). Wyciągnięte z kodu pluginu:
  - lista przepisów (0x101b98c0 → lista 0x10568f90): 87 przepisów alchemii – numer, zakładka, wynik, składniki z ilościami, „stopień” dla `RX_RaiseAlchemy` i doświadczenie (`CRAFT_EXP`, skręty);
  - test, które przepisy bohater zna (0x101a5940): mikstury z talentów – `PLAYER_TALENT_ALCHEMY[n]`, reszta – zmienne skryptów jak w pluginie (np. `KNOWSPEED3`, `KNOWPERMINT`, `MakePoisonKnow`, `RX_PoisonKnowLevel` 1–5, `KNOW_JOINTRECIPE`, proszki zwiadowcy `RX_Scout_Skill_Powder_Type_*`), Oko Innosa – przepis z sercem smoka, które bohater ma;
  - tworzenie (0x101b6400, 0x101b4b30): zabiera składniki × ilość, ustawia `CRAFT_COUNT_TRUE`, `CRAFT_EXP`, `CRAFT_ITEM`, `CRAFT_CAT`, `CRAFT_COUNT`, `CRAFT_TAB`, `CRAFT_SKILL`, daje mikstury, woła `RX_RaiseAlchemy(stopień, ilość)` i `RX_CraftCallBack`;
  - zamknięcie: `RX_EndCraft` (koniec `PLAYER_MOBSI_PRODUCTION`), bohater odchodzi od stołu.
- **Poprawka:** `RX_OpenCraft(CRAFT_ALCHEMY)` otwiera nowe okno (po zamknięciu dialogu, jak okno sprzedaży). Wygląd i sterowanie jak okno sprzedaży (68–69):
  - po lewej 6 zakładek po dwie w rzędzie: Regeneracja, Wytrzymałość, Permanentne, Specjalne, Alkohole, Trucizny – przy nazwie „(ile można zrobić / ile znanych)” jak w oryginale; pod nimi przepisy, które bohater zna: obrazek 3D w pergaminie i „nazwa (xN)”, N – ile może zrobić (0 – wyszarzone);
  - na środku u góry „Umiejętność alchemii: N” (`TalentCount_Alchemy`), strzałki ↑ ↓, X zamyka;
  - po prawej: duży pergamin z miksturą, nazwa, „Maksymalnie: N”, kafelki składników po cztery w rzędzie z obrazkiem i „masz/potrzeba”, **−**, ilość, **+**, **Max**, **Stwórz**; pod składnikami komunikat „Stworzono: … xN”. Po stworzeniu lista się odświeża (np. mięśnie pumy Jiry robi się raz).
  - teksty: ze skryptów (`RX_Craft_TabAlc*`, `RX_Craft_Max`, `RX_Craft_Create`, `RX_Craft_Created`), a gdy są po rosyjsku – polskie z zrzutu.
  - klawiatura: strzałki, Enter – stwórz, Tab – następna zakładka, Escape – zamknij.
  - w logu `[ctx] craft window: N known recipes`, `[ctx] craft: nazwa xN (recipe id)`, `[ctx] craft window closed`.
- **Inne stoły** – poprawka 71.
- **Nie ma:** mikstur `ItPo_ASNC_POTION_*` (w skryptach NB nie ma takich przedmiotów); przy „rzece dusz” i proszkach plugin sprawdza jeszcze coś w silniku Gothica – tu tylko zmienne skryptów.
- **Sprawdzone (23:32):** zakładka Alkohole (czysty alkohol z kilku przepisów, x9 naraz), po X bohater odchodzi od stołu.

### 71. Okno rzemiosła NB przy wszystkich stołach – ✅ (16.11.2026)
- **Pliki:** `common/game/nbcraft.h`, `common/ui/nbcraftmenu.cpp/.h`, `common/game/gamescript.cpp`, `common/game/gamescript.h`
- **Co:** to samo okno co alchemia (poprawka 70) przy każdym stole, który w skryptach woła `RX_OpenCraft`:
  | Stół (skrypt) | `CRAFT_*` | Przepisy | Zakładki | Umiejętność u góry |
  |---|---|---|---|---|
  | kowadło (`SMITHWEAPON_S1`) | 1 | 60 | Jednoręczne, Dwuręczne, Rapiery, Broń orków, Inne | kowalstwa |
  | stół runiczny (`MAKERUNE_S1`) | 2 | 91 | Runy ognia, wody, guru, ciemności, ogólne, Kamienie runiczne | tworzenia run |
  | łuki (`MAKEBOWS_S1`) | 3 | 48 | Łuki, Strzały, Bełty, Inne | – |
  | runy demonów (`DEMONCASTER_S1`) | 4 | 12 | Przyzwanie demona, Ulepszenie, Kamienie runiczne | tworzenia run |
  | zbroje (`RX_ARMORFORGED_S1`) | 5 | 75 | Straż/Zwiadowcy, Najemnicy, Strażnicy, Mroczny rycerz, Inne, Tworzenie | kowalstwa |
  | kocioł (`KESSEL_S1`) | 6 | 10 | Zupy, Gulasze, Kompoty | – |
  | skóry (`SKINRAPE_S1`) | 7 | 5 | Skóry (cięciwy) | – |
  | piec (`SMITHFIRE_S1`) | 8 | 22 | Półfabrykaty, Sztabki, Przetapianie broni | – |
  | biżuteria (`TRFMAGIC_S1`) | 9 | 38 | Stalowe, Złote, Z rudy, Z czarnej rudy, Specjalne | – |
  | kuchnia (`HERD_S1`) | 10 | 11 | Mięso, Ciasta | – |
  | zwoje (`MAKESCROLLS_S1`) | 11 | 6 | Zwoje | – |
  | patelnia / ognisko (`HERDPAN_S1`) | 12 | 6 | Mięso | – |
- **Skąd dane:** listy każdego stołu z UNION_ABI.DLL (0x10568f9c, 0x10568fe4, 0x10568fc0…) i test „zna przepis” każdego stołu (0x101a5940): kowadło – `PLAYER_TALENT_SMITH` i zmienne (`KNOWNORESWORD`, `Lovkach_W1..8`, `RX_Rec_Weapon_01..07`, linie broni orków…), runy – `PLAYER_TALENT_RUNES` i wyjątki (przemiany `HeroKnownTrans_*`, runa teleportu, herb żywiołów…), łuki – warunki zapisane przy przepisach („`KNOWHOWTOMAKEARROWS : 1 | MAKEHOLYARROWS : 1`”), zbroje – wzmocnienia według `RX_HeroKnowsReforcement` (1 lub 2) i przepisy (`Know_HuntArmor_*`, zbroja z pełzaczy, zwiadowcy…), kocioł/kuchnia/zwoje/biżuteria/piec – ich zmienne.
- **Tworzenie** jak w pluginie: niektóre przepisy dają kilka sztuk (groty x25, cięciwy x5, strzały ogniste x5…, w oknie „x N” pod „Maksymalnie”); kowadło i zbroje wołają `RX_RaiseSmith(stopień, ilość)`, runy – za pierwszą runę danego rodzaju `PLAYER_MAKE_RUNES[n] = 1` i `TalentCount_Rune += stopień`. Założonej broni/zbroi okno nie bierze jako składnika (jak plugin: `RX_Craft_EquippedItem`).
- W logu `[ctx] craft window N: … known recipes`, `[ctx] craft N: nazwa xN (recipe …)`.
- **Nie ma:** ulepszeń przy kowadle (`UPGRADED_*`) i zaklinania przy łukach (`ENCHANTED_*`) – to osobne okna pluginu (wybór przedmiotu z ekwipunku); kilka warunków silnika Gothica (zbroja kruka – wersja z hełmem, ryby z wysp i suszone mięso – gildia 14 lub 16).
- **Po teście 23:56:** kowadło działa (umiejętność kowalstwa rośnie), stół runiczny pokazuje przepisy. Poprawione:
  - wszystkie runy nazywały się „Runa” – teraz nazwa z opisu przedmiotu (zaklęcie), jak w ekwipunku: „Otwieranie zamków”, „Przywołanie kamiennego golema”;
  - „Umiejętność tworzenia run: N” wchodziło pod X – za długi napis dostaje mniejszą czcionkę.
- Kocioł, zbroje i ciasta były puste, bo bohater nie zna jeszcze tych przepisów (potwierdzone 23:59). **Do sprawdzenia później**, gdy się ich nauczy.
- **Kocioł pusty mimo znanych przepisów (17.11)** – to jednak był błąd, poprawka 77.
- **Test (18.11):** działa.

### 72. Odpowiedzi w dialogach jak na PC, kamera przy menu bohatera (runa teleportu) – ✅ (17.11.2026)
- **Sprawdzone (zrzut 00:15):** pergamin z odpowiedziami przy runie teleportu, kamera za bohaterem.
- **Pliki:** `common/ui/dialogmenu.cpp`, `common/ui/dialogmenu.h`, `common/mainwindow.cpp`
- **Zgłoszenie (film z PC + zrzut 00:07):** na PC wybór (np. runa teleportu: „...do wieży Xardasa.”, „...do portu Khorinis.”) to mały pergamin na dole ekranu, a kamera zostaje za bohaterem. Na telefonie były duże karty na pół ekranu, a kamera patrzyła z góry na głowę bohatera.
- **Odpowiedzi:** we wszystkich dialogach jeden pergamin `DLG_CHOICE.TGA` jak na PC (ok. 60% szerokości ekranu, na dole), odpowiedzi linijka pod linijką. Linijka jest trochę wyższa od tekstu, żeby trafić palcem; wybrana ma czcionkę wyróżnienia i jasny pasek pod spodem. Przy wielu odpowiedziach lista przewija się palcem (pasek przewijania przy prawej krawędzi pergaminu).
- **Kamera:** menu bohatera „z samym sobą” (bez rozmówcy i nie przy stole – runa teleportu i podobne) nie włącza kamery dialogu. Kamera dialogu celowała w bohatera z obu stron naraz, z odległości ~0, więc silnik podnosił ją nad głowę. Teraz zostaje zwykła kamera za bohaterem, jak na PC.

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
- **Od poprawek 70–71** wszystkie stoły rzemiosła mają okno jak na PC (menu skryptów zostaje tylko dla nieznanego stołu).
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
  - **przywołanie (mod JS):** z amuletem wędrującego ognika – **Shift+F5**, przycisk w zakładce **„Ognik”** menu modów albo komenda `ognik`; przywołany zostaje przy wczytaniach i zmianach świata do zamknięcia gry, pierwsze wczytanie po włączeniu gry jest bez ognika (poprawki 117–119);
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
- [ ] Mieszek i torba na bohaterze – `EXT_PutInSlot` działa od poprawki 125, sprawdzić opcje `bShowPocket`/`bShowBag` z poprawki 5.
- [ ] Znikanie pustych ciał (opcja optymalizacji NB).
- [x] Funkcje bossów wołane przez plugin: `AI_OnTick_<instancja>`, `AI_OnHit_…`, `AI_OnGetDamage_…`, `AI_OnBreak_…` – poprawki 100–101 (do testu).
- [x] Obrażenia w czasie (`RX_ApplyDotDamage`, `RX_ClearAllDots`) – poprawka 106 (do testu).
- [ ] Trucizna na broń (`RX_ApplyPoison`), `AI_PlayEffect`, splash czarów.
- [ ] Efekty perków bohatera (`RX_HeroProcs`).
- [ ] Marvin `add lp` nie działa (poprawka 105).
- [ ] System karmy pluginu (`UpdateKarma`, 130 wywołań – wydarzenia, znaki, osiągnięcia); samo wyświetlanie karmy działa (poprawka 62).
- [ ] Klawisze skrótów NB (`RX_KeyPressed`, 100 wywołań – mikstury, strzały, pochodnia, pomijanie scenek).
- [ ] Przywołania (`RX_SummonNpcToTarget`, `RX_ResetSummons`) i okna pluginu (ulepszenia przy kowadle, zaklinanie, poker).
- [ ] Ognik: pokazywanie rzadkich roślin/tabliczek, „zbieranie w pomieszczeniach” (poprawka 11).
- [ ] Napisy: „Dzień: …” w lewym górnym rogu, liczniki wampira i Full SnC+ (poprawka 62).
