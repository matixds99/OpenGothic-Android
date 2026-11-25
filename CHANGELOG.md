# Log poprawek – OpenGothic + Gothic II: New Balance

Repo: fork `matixds99/OpenGothic-Android`, gałąź `touch-ui` (na bazie `borsuczyna/opengothic-android`).
Lista plików (co podmienić, co dodać) jest w [docs/NEW_BALANCE.md](docs/NEW_BALANCE.md).

**Status:** ✅ sprawdzone w grze · 🧪 czeka na test · ⏸ wycofane / odłożone

**Numery:** każda poprawka ma swój numer, a komentarze w kodzie (`NB fix N`) i tabela plików odwołują się do tych numerów. Poprawki o tym samym temacie są tu połączone w jeden wpis – wtedy w nagłówku stoi kilka numerów (np. „36–38”). Nowe poprawki dostają kolejny wolny numer (następny: **243**; numery 61, 63–67 są w opisie ognika – poprawka 11).

**🆕** oznacza plik, którego nie ma w projekcie borsuczyny i który trzeba dodać.

---

## NOWE

Pięć ostatnich zmian, od najnowszej. Szczegóły w pełnych wpisach niżej.

1. **239** – Zeld i inni magowie teleportują się krótko do wroga: wybór teleportujących się zgodnie z pluginem (gildia DMT z `AIV_MAGICUSER`=MAGIC_ALWAYS, szkieletowi magowie…) zamiast listy nazw; wycofane naprowadzanie z 238 (Gothic go nie ma) – 🧪
2. **238** – pociski czarów postaci podążają za celem (do 120°/s), a nie lecą w miejsce, gdzie bohater stał (56 z 63 Kul śmierci Zelda w ścianę) – ⏸ wycofane przez 239
3. **237** – bossy NB (`bodystate_interruptable_override`) nie są odrzucane ciosami (golemy przewracały Zelda co cios, nie mógł czarować) – ✅
4. **236** – czary bohatera przelatują przez jego przyzwańców i towarzyszy; `[shot]` w logu: co trafił każdy pocisk czaru; liczniki rzuconych czarów w `[fight]`; flagi trajektorii efektów bez względu na wielkość liter – ✅
5. **235** – śledzenie walki w logu (`[fight]`): co sekundę stan, cel, widoczność, czar, decyzja AI walki, kolejka, ciosy i reakcja na obrażenia każdej postaci walczącej z bohaterem lub przyzwańcami – ✅

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

### 82. Muzyka walk z bossami NB – ✅ (17.11.2026)
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
- **Test (21.11):** działa – muzyka bossa w walce.

### 83. Zachwianie i obalenie w walce jak w NB – ✅ (17.11.2026)
- **Pliki:** `common/world/objects/npc.cpp` (`takeDamage`), `common/game/gamescript.cpp/.h` (`nbBeforeHit`, `nbOnDamage`)
- **Zgłoszenie:** w walce nie ma zachwiania/obalenia przeciwników jak w oryginale.
- **Przyczyna:** na PC plugin przy każdym ciosie woła skrypt `RX_OnDamage`; w OpenGothic nikt go nie wołał (82 dodała z niego tylko muzykę).
- **Poprawka:** z `RX_OnDamage`:
  - **zachwianie** (`RX_ApplyStagger`) – przed decyzją silnika o potknięciu: postać trafiona przez gracza (albo ta, która trafia gracza) dostaje `bodystate_interruptable_override` wg skryptu – siła/zręczność bohatera kontra rodzaj przeciwnika (elitarne orki, szkielety, draconianie… mają progi), bossowie i walczący na arenie się nie chwieją;
  - **obalenie** (`RX_NpcProcs`) – po ciosie gracza człowiek (nie boss, nie ork, poziom < 200, nie w trybie łuku/magii) przewraca się (`T_FALLB_2_FALLENB`) z szansą wg umiejętności broni bohatera (1H/2H, łuk/kusza; pięść 5%);
  - licznik walki `BIT_FIGHT_TIMER_STARTED` dla obu stron (inne mechaniki NB z niego korzystają).
- Efekty perków (`RX_HeroProcs`) i taktyki bossów – nadal później.
- **Do sprawdzenia:** bandyci/strażnicy przewracają się czasem od ciosu miecza (częściej przy wysokiej umiejętności), słabsi przeciwnicy chwieją się od ciosów, mocni (elitarne orki, szkielety) nie przy niskiej sile.
- **Test (21.11):** działa – zachwianie i obalenie od ciosów.

### 84. Magia: pocisk leci za celem – ✅ (17.11.2026)
- **Pliki:** `common/world/bullet.cpp/.h`, `common/world/worldobjects.cpp`, `common/world/objects/npc.cpp` (`commitSpell`)
- **Zgłoszenie:** przy celowaniu magią w potwora pocisk nie trafia.
- **Przyczyna:** pocisk czaru leciał prosto w miejsce, gdzie cel był w chwili rzucenia (`setTarget(nullptr)`), a potwory się ruszają; nisko stojące małe stworzenia (szczury) – pocisk z wysokości piersi bohatera łatwo zahacza o ziemię.
- **Poprawka:** pocisk czaru gracza z celem w fokusie co klatkę skręca w stronę środka celu (jak w Gothicu – pociski magii trafiają namierzony cel), aż do trafienia albo śmierci/upadku celu; bez celu leci prosto jak dotąd. Czary NPC bez zmian.
- **Do sprawdzenia:** ognisty pocisk / lodowa strzała w biegnącego wilka, szczura, kretoszczura – trafia.
- **Test (17.11):** działa – pocisk leci za celem, aż trafi. Na PC (Winlator) pocisk nie skręca, a w ruchome cele (krwiopijcy) i tak trafia – możliwe, że później złagodzimy skręt (np. ograniczony kąt na klatkę), żeby było bliżej oryginału.

### 242. Reakcja bossów na obrażenia: RX_OnDamageBossTactics – 🧪 (26.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie (26.11, skaner 241):** porównanie logów PC i OpenGothic z walki z Im'Arah: na PC po każdym `AI_ONGETDAMAGE_IMARAH` idzie `RX_ONDAMAGEBOSSTACTICS` (9 razy w 33 s), w porcie nigdy.
- **Przyczyna:** plugin wywołuje `RX_OnDamage` przy każdym obrażeniu, a jego ostatni krok to `RX_OnDamageBossTactics(atakujący, cel)`. Port miał z `RX_OnDamage` tylko muzykę bossów (82), zegar walki i `RX_NpcProcs` (83), zachwianie (83/100); reakcji bossów brakowało.
- **Poprawka:** `nbOnDamage` na końcu (przy każdym wyjściu z funkcji) woła `RX_OnDamageBossTactics(atakujący, cel)`. Skrypt sam rozstrzyga, kogo dotyczy (`LEVEL 500` + `GIL_DMT` w rozdziale 6: 8% eksplozja 1000 ognia w atakującego, 25% leczenie 500, 10% kula Adanosa; inne bossy: szał przy 15–30% życia, np. smok z wyspy ognia, Soulkeeper, Oberdementor).
- **Nadal brakuje (osobno):** `RX_HeroProcs` (efekty trafienia bohatera: dźwięki, trucizna – wymaga `RX_ApplyPoison`, niezaimplementowanego) i `RX_UpdateBossSpeed` (szybkość bossów w trybie koszmaru).
- **Do sprawdzenia:** w `nbcalls.txt` `RX_ONDAMAGEBOSSTACTICS self=Im'Arah` po każdym trafieniu bossa; w logu bez `NB: RX_OnDamageBossTactics failed`; Im'Arah ginie szybciej (leczy się 25% trafień).

### 241. Skaner wywołań funkcji: porównanie PC (Winlator) z OpenGothic – 🧪 (26.11.2026)
- **Pliki:** nowe `common/utils/nbcalls.cpp/.h`, `tools/nbcompare.py`, `lib/ZenKit` (`DaedalusVm::on_call`, `patches/zenkit-opengothic.patch`), `common/game/gamescript.cpp`
- **Pomysł (26.11):** skoro `nblog.dll` zapisuje na PC wywołania funkcji skryptów, port ma zapisywać to samo, a program ma je zestawiać.
- **Jak:** z `nblog.cfg` (lub `nbcalls.cfg`) w folderze gry OpenGothic zapisuje `nbcalls.txt` w tym samym formacie co `nblog.txt` (`[ms] NAZWA self=.. other=..`, ta sama lista funkcji, ta sama zasada „nie częściej niż co MINMS”). Bez pliku konfiguracji nic nie robi. `python3 tools/nbcompare.py nblog.txt nbcalls.txt --anchor AI_ONGETDAMAGE_IMARAH --npc "Im'Arah"` liczy od pierwszego trafienia bossa i wypisuje dla każdej funkcji liczbę wywołań, tempo na sekundę, medianę odstępu i czas pierwszego wywołania w obu wersjach, z oznaczeniem funkcji tylko na PC (`<<`), tylko w porcie (`>>`) i o tempie różnym ponad 1,7× (`!`).
- **Do sprawdzenia:** skopiuj `nblog.cfg` do folderu gry na Androidzie, zagraj tę samą walkę (Im'Arah), wyślij `nbcalls.txt`; razem z `nblog.txt` z PC da to porównanie.

### 240. Teleport magów: warunek z pluginu to animacja biegu, nie stan ciała – 🧪 (26.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `tools/pc-logger/` (nowe)
- **Zgłoszenie (26.11):** logger na PC (wtyczka `nblog.dll` w Winlatorze, hooki w `UNION_ABI.DLL`) pokazał prawdziwe teleporty Im'Arah: 4 w jednej walce co 7,7 / 8,2 / 6,7 s (średnio ok. 7,5 s), pierwszy 4,1 s po pierwszym `teleport_eligible` (8 cykli po 0,5 s). Na Androidzie wychodziło ok. 8–10 s.
- **Plugin (`UNION_ABI.DLL`, pętla 0x102b1ba0, teleport 0x102b1410):** po licznikach (widzi wroga ≥ 8, nie widzi ≥ 4) sprawdza, czy gra się jedna z animacji: `T_1HRUNSTRAFEL/R`, `S_1HRUN`, `S_1HRUNL`, `S_MAGRUN`, `S_MAGRUNL`, `T_MAGRUNSTRAFEL/R`. Nie sprawdza stanu ciała.
- **Przyczyna:** port wymagał `BS_RUN`/`BS_WALK`; podczas uników bokiem (strafe) i innych animacji biegu ten warunek nie był spełniony, więc teleport czekał na „dobry” moment.
- **Poprawka:** `nbMageBlinkTick` sprawdza warstwy animacji postaci według powyższej listy.
- **Do sprawdzenia:** w logu `blink: Im'Arah teleports …` co ok. 7–8 s (bez przerw 10+ s), także Zeld; teleport nadal się zdarza, gdy postać biegnie.

### 239. Teleport Zelda i magów zgodnie z pluginem; wycofane naprowadzanie 238 – 🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`, `common/world/bullet.cpp`, `common/world/bullet.h`, `common/world/worldobjects.cpp`
- **Zgłoszenie (25.11):** kąt 120° nie istnieje w Gothic i dodałem go bez zgody; Zeld jest trudny, bo teleportuje się na krótki dystans (fazy bossa).
- **Wycofane:** naprowadzanie pocisków czarów postaci (238). Pociski NPC lecą prosto, jak w Gothic. Zostaje tylko droga pocisku w `[shot]` i stare naprowadzanie pocisku bohatera (84).
- **Plugin (`UNION_ABI.DLL` 0x102b1890, wywołane z pętli 0x102b1ba0 co 500 ms):** teleport (fix 189/190) wykonuje się dla postaci, które wybiera ta funkcja, a nie dla listy nazw z 189. Reguły: nie przy liczniku osłabienia nekromanty/mrocznego maga (`AIVRX_SPELL_*_DEBUFF_COUNTER` > 0); Im'Arah gdy `ImarahHeals` = 0; poza tym nie zamrożona, nie `SKELETON_ARCHEMAGEDUNGEON`, `MADNESS_CONJURER`, `KDF_511_DARON`, `RIGELLION`, `DEMENTOR_UD`, `DMT_DEMENTOR_SCOUTCAV_01`, `AIV_ENEMYOVERRIDE` = 0, a następnie: gildia `GIL_DMT` z `AIV_MAGICUSER` = `MAGIC_ALWAYS` (mroczni magowie, w tym Zeld `DMT_12987_ITUSELD`), `AIV_MM_REAL_ID` = `ID_SKELETON_MAGE`, albo `PAL_6104_RITTER_MAGE`, `AD_BOSS6`, `AL_BOSS7`, `SAURON_INT`. Pomijani: postacie pod kontrolą bohatera (`AIVRX_NPC_DARKCONTROL` = 1).
- **Przyczyna:** 189 wziął nazwy z tej funkcji za listę teleportujących się, a to głównie wyjątki; Zeld nie był na liście, więc w ogóle się nie teleportował. Skrypty nie mają krótkiego teleportu bossów (są tylko `AI_Teleport` do trwałych miejsc) – to funkcja pluginu.
- **Poprawka:** `nbMageBlinkTick` wybiera postacie według powyższych reguł (jak się odpowiada przy `GIL_DMT`/`MAGIC_ALWAYS`, instancje wykluczone i dodane po nazwie).
- **Do sprawdzenia:** w logu `blink: Zeld teleports … cm` podczas walki z Zeldem (co kilka sekund, gdy biegnie do bohatera); Zeld nie teleportuje się przy ucieczce dłużej niż reguły pluginu pozwalają.

### 238. Pociski czarów postaci podążają za celem – ⏸ wycofane przez 239 (25.11.2026)
- **Pliki:** `common/world/bullet.cpp`, `common/world/bullet.h`, `common/world/worldobjects.cpp`, `common/world/objects/npc.cpp`
- **Zgłoszenie (log 25.11, 22:58):** „wciąż on jest za słaby” – `[shot]`: Zeld wystrzelił 63 Kule śmierci, 56 trafiło w ścianę lub ziemię, 1 w bohatera (i 4 w golemy/szkielety).
- **Przyczyna:** pocisk czaru postaci leciał prosto w punkt, gdzie bohater stał w chwili rzutu; z 11–16 m bohater w ruchu zawsze schodził z linii. W Gothic pociski czarów (trajektoria `TARGET` ich efektów) podążają za celem; w porcie podążał tylko pocisk bohatera (84).
- **Poprawka:** pocisk czaru postaci podąża za jej celem, skręcając najwyżej 120° na sekundę – trafia biegnącego bohatera, ale szybki unik w ostatniej chwili wciąż działa. `[shot]` ma też drogę pocisku (skąd, dokąd, ile cm).
- **Uwaga:** obrażenia bohatera w tej walce: Furia Beliara 583 magii − ochrona Zelda 397 = 186 za trafienie (życie i ochrona bossów ×1,5 z ustawienia trudności NB „monster stats … hp ratio 150, prot 150”).
- **Do sprawdzenia:** Kule śmierci Zelda trafiają (w `[shot] Zeld … hits hero` zamiast `the world`).

### 237. Bossy NB nie są odrzucane ciosami – ✅ (25.11.2026)
- **Pliki:** `common/world/objects/npc.cpp`
- **Zgłoszenie (log 25.11, 22:39):** Zeld prawie nie atakuje. `[fight]`: w 330 liniach leżał (`body 12` = BS_LIE) 126 razy i spadał (`body 10`) 25 razy; czar zaczął tylko 7 razy, przy 400 decyzjach AI walki. Ciosy kamiennych golemów mają obrażenia obuchowe + „lot” (`dtype=18` = BLUNT|FLY) – każdy odrzucał go na ziemię.
- **Przyczyna:** `C_NPC.bodystate_interruptable_override` (Zeld ma 1, jak inni bossowie NB) wyłączało w porcie zachwianie (188), ale nie odrzucenie – a odrzucenie z leżeniem to najmocniejsze przerwanie.
- **Poprawka:** postać z `bodystate_interruptable_override` nie jest odrzucana ciosem z „lotem” (obrażenia liczą się jak dotąd).
- **Do sprawdzenia:** Zeld z golemami obok – stoi i rzuca Kule śmierci (w `[fight]` rośnie `shot`, `body` rzadko 10/12).
- **Test (25.11, 22:58):** Zeld już nie leży (`body 12` ani razu, `body 10` raz), rzuca często (`body 28` w 51 liniach, 63 Kule śmierci), ale 56 z nich trafiło w ścianę – 238.

### 236. Czary bohatera przez przyzwańców; dziennik pocisków (`[shot]`) – ✅ (25.11.2026)
- **Pliki:** `common/world/bullet.cpp`, `common/world/bullet.h`, `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `common/graphics/visualfx.cpp`
- **Zgłoszenie (log 25.11, 22:12):** „nie mogę zadać obrażeń” – golemy stały przed Zeldem, a każda runa bohatera trafiała w golema (`DMG2: ranged attacker='' player=1 target='Przywołany kamienny golem' … value=0`) i znikała. Zeld rzucał Kulę śmierci (mana −60, `body 28`), ale żaden jego pocisk nikogo nie trafił.
- **Przyczyna (pierwsza część):** pocisk bohatera zatrzymywał się na każdej postaci; „przyjacielski ogień” liczył się tylko dla pocisków NPC (i nawet wtedy pocisk znikał na sojuszniku, tylko bez obrażeń).
- **Poprawka:**
  - pocisk bohatera przelatuje przez jego przyzwańców (`aivar[91]` = `RX_SUMMON_ID`) i towarzyszy (`AIV_PARTYMEMBER`); pocisk NPC przelatuje przez jego sojuszników zamiast na nich znikać;
  - `[shot] <kto> spell <nr> hits <kogo>` / `hits the world (a wall, the ground)` / `hits nothing (flew away or vanished)` – każdy pocisk czaru w 30 m od bohatera (trafienia najwyżej 6 linii na 2 s);
  - w `[fight]` przy czarze: `begun` (ile razy zaczął rzucać) i `shot` (ile pocisków wystrzelił);
  - flagi trajektorii efektów (`emTrjMode`) bez względu na wielkość liter – „unknown trajectory flag: "fixed"” z poprzednich logów: efekt NB tracił trajektorię.
- **Do sprawdzenia:** runy bohatera trafiają Zelda zza golemów; log walki z Zeldem – linie `[shot] Zeld…` pokażą, gdzie podziewają się jego Kule śmierci (ściana / sojusznik / nic) i czy w ogóle są wystrzeliwane (`shot` w `[fight]`).
- **Test (25.11, 22:39):** runy bohatera trafiają Zelda zza golemów (44 trafienia, życie 37875 → 27197). Zeld wystrzelił 7 Kul śmierci w kilka minut (3 trafiły bohatera, 11 w ścianę/ziemię) – za mało; `[fight]` pokazał dlaczego (237).

### 235. Śledzenie walki w logu (`[fight]`) – ✅ (25.11.2026)
- **Pliki:** `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `common/game/gamescript.cpp`
- **Zgłoszenie:** Zeld, Mistrz Ciemności (zabójca Lothara, wieża Ituseld) nie atakował, a golemy biły go przez cały czas. W logu była o nim jedna linia (`[idle] … sees it 0 … ai [ (empty)]`) i ~150 linii obrażeń od golemów – nic o tym, co robił jego skrypt, AI walki ani czy w ogóle odczuł ciosy. Gracz: „potrzebny nam konkretny logger”.
- **Skrypty (do porównania z logiem):** Zeld w `ZS_Attack` → `B_SelectSpell` → `RX_Tactics_Ituseld` (fazy: przy 45%/15% wzywa pomoc, przy 30% osłabia bohatera, potem `RX_AI_UseMagic` + `RX_ReadySpell(SPL_DEATHBALL)`), atak czarem przez AI walki.
- **Poprawka:** co sekundę linia `[fight]` dla każdej postaci w 30 m od bohatera, która walczy z bohaterem lub jego przyzwańcami, jest przyzwańcem w walce albo jest celem bohatera/przyzwańców (nawet gdy sama nic nie robi – przypadek Zelda). Nowa linia, gdy coś się zmieni, inaczej co 5 s; najwyżej 8 linii na sekundę. W linii:
  - stan AI i licznik wywołań jego pętli, co pętla zwróciła i jakie akcje dała do kolejki (`gave [...]`/`nothing`);
  - cel, odległość, kąt do celu, widzi go `w polu widzenia/bez przeszkód`;
  - życie, mana, broń w ręku, czar i etap rzucania;
  - ostatnia decyzja AI walki (`attack`, `turn`, `wait`… albo `none` – brak instrukcji) i ich licznik;
  - kolejka AI i ile akcji z niej zdjęto, stan ciała;
  - ciosy zadane/otrzymane;
  - percepcja obrażeń (`PERC_ASSESSDAMAGE`): ile razy uruchomiła skrypt, ile razy przepadła i dlaczego („the state is not started”, „out of the range”, „no function for it”).
- **Do sprawdzenia:** walka z Zeldem jeszcze raz i log – z linii `[fight] Zeld…` będzie widać, na którym etapie staje (pętla nie działa / nic nie daje / AI walki bez instrukcji / nie widzi celu / ciosy golemów nie docierają do skryptu).
- **Test (25.11, 22:12):** działa – 102 linie `[fight]`. Zeld: pętla `ZS_Attack` działa (daje `Attack`), AI walki daje `attack`/`strafe`/`wait`, widzi bohatera (`1/1`), Kula śmierci w ręku, wchodzi w rzucanie (`body 28`, mana −60 = czar rzucony), a mimo to ani jeden pocisk nikogo nie trafił (brak `DMG2` od Zelda). Brakowało informacji, co dzieje się z pociskiem – 236.

### 234. Reinar kupuje bronie; start poziomu w nazwanym obiekcie; zamknięcie ekwipunku – 🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/worldobjects.cpp`, `common/world/world.cpp`, `common/world/world.h`, `common/gothic.h`, `common/mainwindow.cpp`
- **Zgłoszenie (log 25.11, 21:39, „Napraw tą grę”):**
  - Reinar (miasto, `DIA_VLK_989_Reinar_SellWeapon`): „not implemented call [RX_FORM_REINAR_SELLWEAPON]” – rozmowa o sprzedaży broni była pusta, tylko „Wróć”;
  - runa Vatrasa do kryjówki zabójcy Lothara (`ItMi_DarkTransfer`, `Wld_ChangeLevel_AB(ITUSELDTOWER.ZEN, ITUSELDTOWER_START)`): „insertPlayer: invalid waypoint, using fallback” i „void under the feet” – punktu o tej nazwie nie ma wśród punktów drogi;
  - „not implemented call [RX_CLOSEPLAYERINVENTORY]” z tej samej runy.
- **Plugin:**
  - `RX_Form_Reinar_SellWeapon` (UNION_ABI.DLL 0x101c0d30): dla każdej broni bohatera z tabeli pluginu (171 broni z cenami, 0x101bdf70; nie założonej) dodaje wybór „'Nazwa' - cena monet (Jednoręczna/Dwuręczna)”; czeladnik Harada (`Player_IsApprentice` 2) nie sprzedaje broni Harada (0x101c0980). Wybór ustawia `RX_ReinarItemId` i `RX_ReinarItemMoney` (0x10347100) i woła `DIA_VLK_989_Reinar_SellWeapon_Common` – skrypty dają złoto, zabierają broń i budują listę od nowa;
  - Gothic szuka startu zmiany poziomu po nazwie obiektu (nie tylko punktu drogi/startu).
- **Poprawka:**
  - `RX_FORM_REINAR_SELLWEAPON` jak wyżej (w porcie wybory zapamiętane po tekście, przed wywołaniem funkcji ustawiane są obie zmienne);
  - `insertPlayer`: gdy nie ma punktu o tej nazwie, bohater staje przy obiekcie świata o tej nazwie (na ziemi pod nim), w logu „the start '…' is a vob”;
  - `RX_CLOSEPLAYERINVENTORY` zamyka ekwipunek.
- **Do sprawdzenia:** u Reinara lista broni z cenami i sprzedaż (złoto, broń znika, lista się odświeża); runa Vatrasa – bohater pojawia się przy wejściu do wieży, nie w środku mapy.

### 233. Stwory utknięte w drodze do nieosiągalnego punktu – 🧪 (25.11.2026)
- **Pliki:** `common/game/movealgo.cpp`, `common/game/gamescript.cpp`
- **Zgłoszenie (log 25.11, 19:36):** „[blocked] Ork wojownik (49004) … from FP_ROAM_ORK_OC_107 to FP_ROAM_ORK_OC_107 … land 1 … noClimb 1 height 753” – 117 razy przez całą grę: ork w obozie orków szedł do punktu wędrówki na skale 3 m wyżej i cały czas wchodził w ścianę. Tak samo chrząszcz (`STUCK 5 s … ai=[ GoToPoint] go2=1`).
- **Poprawka:** gdy stwór poza walką przez 12 s idzie do tego samego punktu drogi i wchodzi w przeszkodę, na którą nie da się wspiąć, kończy tę drogę (`[blocked] … gives up the way to … after 12 s`) i AI idzie dalej – następny punkt wędrówki, dalsza część rutyny. Nie dotyczy pościgu, chodzenia za postacią (przyzwańcy za bohaterem) ani gracza.
- **Do tego:** linia „insert: … moved … cm to a free place” tylko przy przesunięciu ponad 2,5 m – szukanie miejsca zaczyna się 150 cm od punktu (jak w OpenGothic), więc przy nowym rozdziale było 190 linii po 150–175 cm.
- **Do sprawdzenia:** brak setek linii `[blocked]` jednego stwora; potwory w obozie orków chodzą normalnie.

### 232. Logger: zmienne przyzwańców i liczniki; orkowie z bronią; cel po usuniętym przyzwańcu – ✅ (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`
- **Zgłoszenie (log z 25.11, 19:05):**
  - `[var] RX_SUMMONCOUNT: changes all the time, left out` – po 9 przywołaniach i śmierciach golemów w 5 min logger przestał pokazywać licznik przyzwańców;
  - liczniki każdego zabicia (`RX_STATEXP`, `MONSTERKILLED`, `RANKPOINTS`, `*_B1`) wciąż po kilkanaście linii;
  - 40 linii „[Czarny ork wojownik] walks about with a weapon in hand in ZS_STAND_WP, it puts the weapon away” – orkowie NB stoją na straży z bronią, port co 10 s kazał im ją chować, a skrypt od razu ją wyciągał;
  - po wczytaniu zapisu („summons removed: 1”) potwory miały za cel usuniętego golema 690 m dalej (`[AGGRO] … out of range 69358`) i stały, aż smycz po 6 s go zgubiła.
- **Poprawka:**
  - zmienne z `SUMMON`, `ISUP`, `ALIVE`, `_ACTIVE` w nazwie nigdy nie są wyciszane;
  - zmienna, która 6 razy z rzędu tylko rośnie, to licznik – wyciszana z dopiskiem „a counter”; tak samo `aivar` bohatera rosnący o 1 (licznik czarów `aivar[20]`);
  - chowanie broni na spacerze (`tickLooseWeapon`) tylko dla ludzi, nie dla orków;
  - przy usuwaniu przyzwańców (wczytanie, zmiana świata, śmierć bohatera) postacie, które miały któregoś za cel, tracą ten cel;
  - linia `STUCK 5 s` ma też `cast=…/castLevel`, `frozen`, `down`, `policy` – dwa razy golem stał 5 s zaraz po przywołaniu z kolejką `[SetWalkMode]` bez widocznej przyczyny.
- **Do sprawdzenia:** `[var] RX_SUMMONCOUNT` przy każdym przywołaniu; brak serii „puts the weapon away” przy orkach; po wczytaniu potwory od razu zachowują się normalnie.
- **Test (25.11, log 19:36):** `RX_SUMMONCOUNT` przy każdym przywołaniu (9 linii), liczniki wyciszone jako „a counter”, zero linii „puts the weapon away”, po wczytaniu żadnego celu setki metrów dalej.

### 231. Przyzwańcy i potwory stojący na granicy zasięgu – ✅🧪 (25.11.2026)
- **Pliki:** `common/world/objects/npc.cpp`, `common/world/objects/npc.h`
- **Zgłoszenie (log z loggerem 229):** Hubaxis miał cel (polna bestia, czarny goblin, czarny wilk), ale przez długie minuty stał w miejscu, a nie atakował. Linia `[AGGRO] [Hubaxis] [Czarny goblin] is out of range 2802 > 2800` powtórzyła się około 280 razy, zawsze przy 2800–2807 cm, czyli dokładnie przy jego zasięgu zmysłów (`senses_range`). Atakował dopiero na rozkaz („summons: 1 attack …”).
- **Przyczyna:** pościg w porcie przełączał się dokładnie na zasięgu. Bliżej niż zasięg cel za przeszkodą był osiągany po punktach drogi, a dalej – prosto na cel. Droga naokoło skały na chwilę wyprowadzała stwora poza zasięg, więc wracał do biegu prosto, w tę samą przeszkodę, znów w zasięg, znów na drogę… i tak w kółko na granicy.
- **Poprawka:** droga do celu (punkt drogi przy celu) jest kontynuowana także wtedy, gdy na chwilę prowadzi dalej niż zasięg. Gdy stwór jest poza zasięgiem dłużej niż 6 s, i tak gubi cel (`tickAggroLeash`, jak dotąd). Linia `[AGGRO] … is out of range` raz na 10 s dla tego samego celu, a nie przy każdym kroku.
- **Do sprawdzenia:** Hubaxis (i potwory) dochodzi do celu za skałą, ogrodzeniem, na zboczu, zamiast stać w miejscu.
- **Test (25.11, log):** Hubaxis atakuje wilki, gobliny i polne bestie bez rozkazu; ani jednej linii `[AGGRO] [Hubaxis] … out of range` (poprzednio ~280), wszystkich linii `[AGGRO]` 83 zamiast 283.

### 230. Poprawki loggera bohatera – ✅🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamescript.h`
- **Zgłoszenie (log z loggerem 229):**
  - nauka zmiennych (pierwsze 20 s) wyciszyła akurat `RX_DEMONHUB_ACTIVE`, `RX_SUMMONCOUNT`, `RX_STATUSBAR_SUMMON` (gracz przywołał Hubaxisa w tych 20 s), a po wczytaniu przy rozmowie – flagi okien;
  - `[print] Strzały: 1248` (stały licznik strzał na HUD) co 10 s – 70 linii;
  - lista przyzwańców „migała” (0 ↔ 1), gdy Hubaxis był dalej niż 50 m, więc słuszna odmowa „już przyzwany” wyglądała na błąd;
  - liczniki każdego zabicia (`RX_STATEXP`, `MONSTERKILLED`, `RANKPOINTS`, `RANEAT`, `RX_DEMONHUB_EXP`) – po 20 linii;
  - teleport odmówił 4 razy (`[hero] spell refused: Runa Teleportacji`) bez żadnego `[print]`: `Spell_Logic_TeleportXardas` w walce mówi tylko głosem (`B_Say … "$DOESNTWORK"`).
- **Poprawka:**
  - nauka wycisza tylko zmienne zmienione co najmniej 4 razy w 20 s (zegary, co 2–5 s) i robi to przed zapisaniem zmian, bez paczki linii zegarów na koniec nauki;
  - zmienna zmieniona więcej niż 15 razy od wczytania świata też zostaje pominięta (liczniki zabić); dotyczy też `aivar` bohatera;
  - ten sam tekst na ekranie: czas liczony od ostatniego pokazania, więc tekst wyświetlany cały czas jest w logu raz;
  - komunikaty jednej funkcji pokazane naraz – w jednej linii („a | b | c”, np. 12 linii statystyk Hubaxisa);
  - przyzwańcy z całego świata, z odległością: `[hero] summons (1): SUMKHUBI 1080/1080 52 m`;
  - `[hero] says $DOESNTWORK (SPELL_LOGIC_TELEPORTXARDAS <- SPELL_PROCESSMANA)` – co bohater mówi sam (`AI_OutputSVM`), bo to często odmowa skryptu bez tekstu.
- **Do sprawdzenia:** w logu są `[var] RX_DEMONHUB_ACTIVE` przy przywołaniu i śmierci, `Strzały` raz, odmowa teleportu w walce z `[hero] says`.
- **Test (25.11, log):** działa: `[hero] says $DOESNTWORK (AI_OUTPUTSVM <- B_SAY <- SPELL_LOGIC_TELEPORTXARDAS …)`, `Strzały` raz na wczytanie, przyzwańcy z odległością, linie jednej funkcji razem. `RX_SUMMONCOUNT` został jednak wyciszony po 9 przywołaniach i śmierciach golemów w 5 min – poprawka w 232.

### 229. Logger bohatera: komunikaty, stan, przyzwańcy, zmienne skryptów – ✅🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamescript.h`, `common/gothic.cpp`, `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `patches/zenkit-opengothic.patch` (`DaedalusVm::call_chain` publiczne)
- **Zgłoszenie:** propozycja gracza po błędzie z Hubaxisem (228) – „logger na graczu”: jakich ma przyzwańców, jakie zmienne się zmieniają, jakiej runy lub broni używa, jak zachowują się animacje. Błąd z Hubaxisem był widoczny tylko na ekranie (odmowa skryptu), w logu nie było nic.
- **Poprawka:** log zapisuje tylko zmiany, żeby nie urósł:
  - `[print] tekst (FUNKCJA <- FUNKCJA…)` – każdy komunikat skryptów na ekranie (`Print`, `PrintScreen`, `AI_PrintScreen`, `AI_Print*`, `RX_PrintP`, `PrintScreenColor`…) z łańcuchem funkcji, które go pokazały. Odmowy NB („… jest już przyzwany”, „za mało…”) mają od razu nazwę funkcji i warunku do sprawdzenia w skryptach. Ten sam tekst raz na 10 s, najwyżej 20 linii na 10 s.
  - `[hero] spell refused: …` – logika czaru (`Spell_Logic_*`) odmówiła rzucenia (`SPL_SENDSTOP`), z numerem czaru i maną.
  - `[hero] guild / in hands / armor / melee / ranged / transformation` – co 0,5 s, linia przy zmianie: gildia, broń w rękach (runa lub zwój z numerem czaru), zbroja i hełm, bronie przy pasie, przemiana.
  - `[hero] summons (N): Instancja hp/max, …` – lista przyzwańców bohatera (`aivar[91]` = `RX_SUMMON_ID`) przy każdej zmianie składu: przywołany, zginął, zniknął.
  - `[hero] anim: …` – animacje akcji bohatera (ciosy, czary, mikstury, obiekty, trafienia), bez chodu, biegu, stania, skoków i pływania; najwyżej 20 linii na 10 s.
  - `[var] ZMIENNA: a -> b` – co 2 s zmiany wszystkich globalnych zmiennych liczbowych skryptów (około 12,7 tys. wartości, bez zmiennych lokalnych funkcji), np. flagi przyzwańców, etapy zadań, liczniki NB. Pierwsze 20 s po wczytaniu świata to nauka: zmienne, które zmieniły się więcej niż raz (zegary, liczniki pętli), są pomijane (lista w logu: `[var] left out`). Zmienna zmieniająca się więcej niż 8 razy w 5 min też zostaje pominięta (`changes all the time, left out`). Najwyżej 40 linii na raz.
  - `[hero] aivar[N] AIV_…: a -> b` – zmiany zmiennych samego bohatera (`aivar`) z nazwami stałych `AIV_*`/`AIVRX_*`, z tym samym pomijaniem.
- **Do sprawdzenia:** log po kilku minutach gry: czy nie jest zbyt głośny (jakie zmienne zalewają `[var]`), czy przy przywołaniu i śmierci przyzwańca są linie `[hero] summons`, czy odmowa czaru ma `[print]` z nazwą funkcji.
- **Test (25.11):** działa. Odmowa ma od razu funkcję: `[print] Hubaxis jest już przyzwany... (AI_PRINT <- SPELL_LOGIC_SUMKHUB <- SPELL_PROCESSMANA)` + `[hero] spell refused`. Widać przywołania, śmierci i zmiany broni oraz `RX_DEMONHUB_ACTIVE 0 -> 1` itd. Nauka zmiennych wyciszyła jednak właśnie flagi przyzwańca, licznik strzał z HUD szedł co 10 s, a lista przyzwańców migała przy słudze dalej niż 50 m – poprawki w 230.

### 228. „Hubaxis jest już przyzwany…” choć Hubaxisa nie ma – ✅ (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie (zrzut ekranu, bez logu):** czar przywołania Hubaxisa odpowiada „Hubaxis jest już przyzwany...”, a Hubaxisa nigdzie nie ma.
- **Skrypty i plugin:** `Spell_Logic_SumKhub` odmawia, gdy `RX_DemonHub_Active` = 1. Skrypty tylko ustawiają tę flagę (przy przywołaniu), a zeruje ją wyłącznie plugin (`0x10184eb0`), gdy przyzwaniec ginie albo znika ze świata. To samo dla innych zwierzaków i sług: `Crait` → `CraitIsUp`, `SumKhubi` → `RX_DemonHub_Active`, `DSnapper` → `DSnapperIsUp`, `DK_Pet` → `DarkPetIsUp`, `PET_JINA` → `JinaWolfIsUp`, `Summoned_SkeletonUniq` → `SkeletonUniqIsUp`, `Scavenger_Hero_Mount` → `RX_ScavSummoned`, `Summoned_Dragon` → `DragonStaffAlive`. W porcie nikt ich nie zerował – po pierwszej śmierci (albo zmianie świata) danego sługi czar odmawiał na zawsze.
- **Poprawka:** co 2 s – flaga włączona, a w świecie nie ma żywej postaci tej instancji: flaga 0 (w logu `summon flag … off`). Działa też na istniejących zapisach.
- **Do sprawdzenia:** Hubaxis znów daje się przywołać (najpóźniej 2 s po wczytaniu); po jego śmierci – od razu.
- **Test (25.11, log z loggerem 229):** po każdej śmierci Hubaxisa i po wczytaniu flaga jest zerowana (`summon flag RX_DEMONHUB_ACTIVE off`) i przywołanie działa ponownie (4 razy w logu). Dwie odmowy „już przyzwany” były słuszne: Hubaxis żył, tylko był ponad 50 m dalej (zaraz potem `summons: 1 called to the hero`).

### 227. Przyzwańcy nekromanty: leczenie, aury, przywołanie przy celu – 🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/nbtexts.h`, `common/world/objects/npc.cpp`, `common/world/objects/npc.h`
- **Zgłoszenie:** „Zaczynaj” – przegląd przyzwańców: funkcje pluginu, których skrypty używają, a port ich nie miał.
- **Plugin:** przyzwańcy bohatera to postacie z `aivar[91]` = `RX_SUMMON_ID`, a nieumarli to gildie 37 (`GIL_SUMMONED_SKELETON`), 26 (`GIL_SUMMONED_GOBBO_SKELETON`) i 55.
  - `RX_SummonHeal` (`0x101cfd20` → `0x103f70f0`, czar `SPELL_CAST_SUMMON_HEAL`): bohater oddaje swoim nieumarłym 150 punktów życia za każdy krąg magii, a każdy ranny dostaje pięć razy tyle (`SPELLFX_DARKAURAEFFECT`). Gdy życia jest za mało – napis `RX_NecroSummonerNoHealth` z ceną.
  - `RX_ApplyBuffToSummons(tryb)` (`0x101cfce0` → `0x103f6820`), aury nekromanty: przełączane czarami `AURA_SUMMON_03/04`:
    - 0 (`RX_NecroRuneAuraActive_03` i `RX_NecroRuneAura_2`) – nieumarli przyzwańcy szybsi: 100% + 5% za krąg (`AIVRX_NPC_SPEED`); po wyłączeniu – 100%. Przyzwańcy przywołani później dostają aurę w ciągu sekundy;
    - 1 (`RX_NecroRuneAuraActive_04` i `RX_NecroRuneAura_3`) – postacie wokół wolniejsze o `countLearnSpell/2 + 5` % na godzinę (`AIVRX_NPC_SLOW`, `SPELLFX_SLOWDOWN_VIS`); pomijane: przyzwańcy, przyjaciele, drużyna, bossowie, zamrożeni. Po wyłączeniu – koniec spowolnienia.
  - `RX_IsValidTargetToSummon` / `RX_SummonNpcToTarget` (`0x101b8930`, `0x101b8950`): z klawiszem `KEY_NONTARGET_ACTION` plugin pokazuje znacznik na ziemi pod celownikiem i przywołuje tam. Port nie ma znacznika (ani tego klawisza): celem jest postać w fokusie, inaczej 5 m przed bohaterem.
  - `RX_GetArenaSummonCount` – żywi przyzwańcy bohatera; `RX_AgroNpcOnSummon(_Clear)` – tylko w trybie „realmode” pluginu (`IsNewLegFeatures`, w porcie wyłączony), puste.
- **Jeszcze nie:** pupil rycerza śmierci (`RX_ApplyDKRune`, `RX_HealPetDarkRune`, `RX_UpdateDKPet_AST`, `RX_ResetDKRunes` – osobny system pluginu), `RX_NecromantProc_Control` (efekty run nekromanty przy ciosie), `RX_ResetTotems` (arena).
- **Do sprawdzenia:** czar leczenia przyzwańców nekromanty (życie bohatera spada, szkielety się leczą); aura szybkości przyzwańców i aura spowolnienia wrogów (włączanie i wyłączanie tym samym czarem).

### 226. Wilki, gobliny i szczury nie atakowały po imporcie zapisu z PC – ✅ (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie (log):** „Wiesz dlaczego Wilk mnie nie atakował oraz goblin?” – „[idle] Wilk stands 8 s in ZS_MM_ATTACK - target hero at 626 cm, sees it 1, weapon 0”; trace „ZS_MM_Attack | CONTINUE 2” u każdego atakującego potwora (szczur, wilk). Gra wczytana z oryginalnego zapisu z PC („Original save … globals 15514”).
- **Skrypty:** `ZS_MM_Attack_Loop` na początku: `if (RX_GamePaused || RX_GameVoting) return LOOP_CONTINUE` – przy pauzie gry wieloosobowej potwór nic nie robi (nie wyciąga „broni”, nie wydaje `AI_Attack`). Żaden skrypt ani UNION_ABI.DLL tych zmiennych nie ustawia – należą do trybu wieloosobowego. Zapis z PC przyniósł w jednej z nich 1, więc każdy potwór w walce stał.
- **Poprawka:** w grze jednoosobowej obie zmienne są stale 0 (zmiana w logu: `multiplayer flag … was 1`).
- **Do sprawdzenia:** wilki, gobliny i szczury atakują bohatera; w logu brak „CONTINUE 2”.
- **Test (25.11.2026):** potwory znów atakują ✅.

### 225. Status zadań w logu – 🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie (log, test 215):** nauka języka orków („Tajemnica języka orków”) – w logu tylko wpisy dziennika, nie widać, czy zadanie zostało zamknięte.
- **Skrypty:** w NB `LOG_SUCCESS` = 4 (w silniku to „nieaktualne”: misja wykonana, jeszcze nie zamknięta), a `LOG_CLOSED` = 2. `USE_ItWr_OrcLanguage` ustawia 4, potem `B_CloseTopic_Now` (status 4 i zmienna misji 4) zmienia na 2, gra dźwięk `QUEST_DONE` i pokazuje „Zadanie zakończone!”. Bez `Log_GetTopicStatus` (poprawka 215) ten krok nic nie robił.
- **Poprawka:** każda zmiana statusu zadania w logu: `journal status: <temat> -> running / closed / failed / success (4)`.
- **Do sprawdzenia:** po zakończeniu zadania w logu `-> success (4)`, zaraz potem `-> closed`, na ekranie „Zadanie zakończone!”.

### 224. Golemy nie umiały wejść po schodach – krążyły pod bohaterem – 🧪 (25.11.2026)
- **Pliki:** `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `common/game/movealgo.cpp`
- **Zgłoszenie (2 nagrania + log):** „golemy nie umią poruszać się schodami tylko krążą. Na PC bez problemu wchodzą po schodach i szukają celu”. Wieża Xardasa, kręcone schody: log „[blocked] Przywołany kamienny golem … hint 1 from - to -” (podążanie za bohaterem) i „hint 3” (pościg za szkieletem) dziesiątki razy, golem wspinał się z y 1945 do 3236 przez minuty.
- **Przyczyna:**
  - `AI_GotoNpc` (przywołańce idą za bohaterem w `ZS_MM_Rtn_Summoned`) w porcie szło zawsze prosto na postać, a krok był przycinany do odległości w poziomie – golem pod bohaterem dreptał w kółko w klatce schodowej. Silnik oryginału idzie wtedy po punktach drogi;
  - pościg w walce (`implAttack`) szedł po punktach drogi tylko wtedy, gdy promień do celu trafiał w przeszkodę. Cel piętro wyżej widoczny przez otwór klatki = prosto w ściany schodów; punkt przy celu brany tylko z jego stożka widzenia.
- **Poprawka:**
  - `AI_GotoNpc`: gdy druga postać jest na innym poziomie (1,5 m) albo za ścianą – droga po punktach do punktu przy niej (na jej poziomie, widocznego od niej), sprawdzana co 0,8 s; na tym samym poziomie i w zasięgu wzroku – jak dotąd prosto. Krok liczony do następnego punktu drogi, nie do postaci;
  - pościg: cel ponad 2 m wyżej lub niżej – też po punktach drogi; punkt przy celu także za jego plecami (wolna linia wzroku, ten sam poziom).
- **Do sprawdzenia:** golemy w wieży Xardasa idą za bohaterem po schodach na górę i w dół; w walce wbiegają po schodach do szkieletów.

### 223. Przywołane golemy pojawiały się piętro niżej i krążyły pod bohaterem – 🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamescript.h`, `common/world/world.cpp`
- **Zgłoszenie:** „byłem na górze i atakowali szkielety, później przywołałem je a one na dole się pojawiły i mieli problem wejść na górę tylko się kręcili w koło”. Log: wieża Xardasa – bohater na 2. piętrze (y 2838), golemy na niższym (y ≈ 1950) „[blocked] … hint 1 from DT_E2_01 to DT_E2_01” i „stands 8 s … sees it 0”.
- **Przyczyna:**
  - przywołanie z panelu (PRZYWOŁAJ), przejście przez teleport z bohaterem i czar przywołania (`Wld_SpawnNpcRange`) stawiały postać 1–2,5 m od bohatera, a `fixNpcPosition` brał za poziom ziemię pod tym punktem. Za bohaterem była klatka schodowa – ziemią pod nią było niższe piętro, 9 m niżej (drugie przejście szukało też „jakiejkolwiek ziemi”);
  - na dole golem szukał początku drogi do bohatera: punkt drogi tylko w stożku widzenia, a gdy żadnego nie było – najbliższy w ogóle, czyli `DT_E2_01` piętro wyżej, nad jego głową. Szedł do niego prosto i krążył pod nim.
- **Poprawka:**
  - `fixNpcPositionNear`: poziomem jest ziemia pod bohaterem; punkt musi być na tym poziomie (60 cm, potem 2 m – schody), widoczny od bohatera na wysokości kolan (bez ściany, barierki ani krawędzi między nimi) i postać musi się tam zmieścić; inaczej pierwszy taki punkt bez sprawdzania miejsca, w ostateczności przy bohaterze. Nigdy inne piętro;
  - początek drogi (`World::wayTo`): gdy w stożku widzenia nie ma punktu – punkt widoczny za plecami, potem najbliższy na tym samym piętrze (3 m w pionie), dopiero na końcu jakikolwiek.
- **Do sprawdzenia:** przywołanie golemów i PRZYWOŁAJ z panelu na piętrze (wieża, schody, mosty) – stają obok bohatera na jego poziomie; golem na dole znajduje schody do góry.
- **Test (25.11.2026):** golemy przywoływane na piętrze, po schodach dalej krążyły – poprawka 224.

### 222. Przywołany golem między dwoma zębaczami zmieniał cel przy każdym ugryzieniu – 🧪 (25.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`, `common/world/objects/npc.h`
- **Zgłoszenie (log, test 216):** golem wśród dwóch smoczych zębaczy: „turns to Smoczy zębacz that hits it; its target Smoczy zębacz is out of sight (1 m)” kilkanaście razy pod rząd; „STUCK 5 s … ai=[SetWalkMode]”.
- **Przyczyna:** poprawka 198 (przywołaniec zwraca się do wroga, który go gryzie, gdy jego cel jest daleko albo niewidoczny) sprawdzała widoczność z kątem widzenia (±100°). Zębacz, z którym golem walczył, stał za nim w odległości 1–2 m i był „niewidoczny”, więc każde ugryzienie drugiego zębacza zmieniało cel, a potem ugryzienie pierwszego zmieniało go z powrotem. Każda zmiana zaczyna `ZS_MM_Attack` od nowa (czyści kolejkę, przerywa obrót i cios) – golem stał między nimi i rzadko trafiał. To błąd portu, nie skryptów.
- **Poprawka:** „niewidoczny” to teraz brak wolnej linii wzroku (ściana, skała) bez kąta; cel bliżej niż 4 m nigdy nie jest „niewidoczny”; najwyżej jedna zmiana celu na 3 s (poza celem martwym lub powalonym).
- **Log:** linia `STUCK 5 s` tylko wtedy, gdy kolejka AI naprawdę stoi. Wcześniej ta sama akcja dodawana raz za razem od nowa (np. `SetWalkMode` przy każdym ponownym starcie stanu) też wyglądała na zablokowaną kolejkę.
- **Do sprawdzenia:** golem przeciw dwóm lub więcej potworom – trzyma jeden cel i bije regularnie; w logu mniej linii „turns to … out of sight”.

### 221. Skręty templariuszy (zwolnienie czasu), zaciemnienie ekranu, `InitFullSnc`, `Cmd` – 🧪 (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/nbbars.h`, `common/mainwindow.cpp`
- **Zgłoszenie:** „Popraw co się da” – reszta funkcji pluginu z raportu poprawki 213.
- **Plugin (UNION_ABI.DLL z aktualizacji):**
  - `StartChangeWorldTime(n, f)` (`0x101b9fe0` → `0x10160b40`, zegar `0x10160a60`): po skręcie templariusz widzi świat w zwolnionym tempie – czynnik czasu gry (`ztimer`) spada równymi krokami co 100 ms do `f` przez pierwszą połowę z `n·10` kroków i wraca do 1 przez drugą (35 → 35 s, `f` 0,8–0,9). Gdy zwolnienie trwa, kolejny skręt go nie zaczyna od nowa. W porcie mnożnik czasu sesji (jak `ztimer` Marvina);
  - `Ext_DarkScreen(zanik, czas, ?)` (`0x101d1a30`): ekran ciemnieje przez `zanik` ms, jest czarny przez `czas` ms i wraca przez `zanik` ms (sceny wampira: `RX_VampFriendQuest`, picie krwi);
  - `InitFullSnc()` (`0x101d1890`) czyta limity trybu Full SnC+ do samego pluginu (`max1hStr`, `maxHp`…) – port nie ma tego trybu, funkcja nic nie robi; `Cmd(tekst)` – linia konsoli pluginu, do logu (`NB cmd:`, ta sama linia najwyżej 3 razy).
- **Do sprawdzenia:** skręt templariusza (gildia templariuszy) – świat zwalnia na ok. 35 s i wraca; zaciemnienie w scenach wampira.

### 220. Worki NB (`OpenPocket`) – 🧪 (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/ui/inventorymenu.cpp`, `common/ui/inventorymenu.h`, `common/gothic.h`, `common/mainwindow.cpp`
- **Zgłoszenie:** jak 221.
- **Plugin:** `OpenPocket(worek, flagi)` (`0x101a8d90`) nie jest osobnym pojemnikiem: na czas otwartego ekwipunku plugin wyjmuje z niego przedmioty spoza flag worka i same worki (`ITBG_*`), a zostawia przedmioty noszone (`0x40000000`). Worki: zbroje, tarcze i pasy; broń biała, dystansowa i amunicja; mikstury; runy i dokumenty; pierścienie i amulety.
- **Poprawka:** użycie worka otwiera ekwipunek bohatera (albo zostawia otwarty) z filtrem: tylko przedmioty z tymi flagami (`mainflag | flags`) i noszone, bez worków. Zwykłe otwarcie ekwipunku pokazuje znów wszystko.
- **Do sprawdzenia:** każdy z pięciu worków pokazuje tylko swoje przedmioty; po zamknięciu i otwarciu ekwipunku jest pełna lista.
- **Test (25.11.2026):** jeszcze nie – do sprawdzenia przy okazji (użyć worka z ekwipunku).

### 219. Ognik: zbieranie roślin i liczenie roślin, tablic i nieokradzionych postaci – ✅🧪 (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/nbtexts.h`
- **Zgłoszenie:** jak 221.
- **Plugin:** przedmioty świata (pomijane te przy początku układu, bliżej niż 2,5 m od 0,0,0); rośliny to `ITPL_*` z kategorią jedzenia, rzadkie – `dex_herb_01`, `strength_herb_01`, `perm_herb`, `xagitta_herb_mis`, `super_herb`, `cactusflower`; tablice – `ITWR_*` z `STONEPLATE`, `DRUIDPLATE`, `DEMONPLATE`.
  - `FindPlants` (`0x101aafe0`): zwykłe rośliny lokacji idą do bohatera – do 30 naraz, na Starożytnej Wyspie (`currentLevel` 47) do 20; ognik mówi, co przyniósł (`RX_WispFoundInLocation` z `AB_Translation.lbr` + „nazwa xN”);
  - `FindPlantsRare(promień)` (`0x101ab7a0`): wszystkie rośliny i tablice w promieniu bohatera („Znalezione rośliny i tabliczki”);
  - `PlantsInLoc`, `PlantsRareInLoc`, `FindPlates` (`0x101aa3a0`, `0x101aa660`, `0x101ba8b0`): ile zwykłych roślin, rzadkich roślin, tablic zostało w lokacji;
  - `OldWisp_FindStealTargets` (`0x101b8d80`): żywi ludzie lokacji, których bohater jeszcze nie okradł (`AIV_PlayerHasPickedMyPocket` 0), bez przyjaciół (`npcType` 2, 3) i gildii 19.
- **Do sprawdzenia:** dialog ognika: „zbierz rośliny” (za rudę), liczniki roślin, tablic i nieokradzionych.
- **Test (25.11.2026):** zbieranie roślin za rudę działa (w logu „wisp: 30 items of the location to the hero”) ✅. Liczniki – do sprawdzenia.

### 218. Skrzynie i drzwi po nazwie, portale, obrót do obiektu – 🧪 (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/interactive.h`, `common/world/objects/npc.cpp`, `common/world/aiqueue.cpp`, `common/world/aiqueue.h`, `common/game/constants.h`
- **Zgłoszenie:** jak 221.
- **Plugin:**
  - `Mob_SetLockedByName(nazwa, zamknięta)` (`0x101cb140`, `oCMobLockable::SetLocked`): trzy skrzynie świątyni Asztar – wybrana się otwiera, dwie pozostałe są opróżniane i otwierane;
  - `Wld_SetCondFunc(nazwa, funkcja)` (`0x101bcaa0`): funkcja warunku mobów o tej nazwie (`CondFunc_Thief_Chest`…), zapisywana w save;
  - `Mob_RemoveItem_AB(nazwa, przedmiot, ile)` (`0x101a78e0`): przedmioty znikają ze skrzyni (zakład z Orlanem: złoto Rukhara);
  - `Wld_ChangePortal(pokój, gildia)` (`0x101c7a90`): gildia właściciela pokoju portalu, jak `Wld_AssignRoomToGuild` (wieże miasta publiczne dla zwiadowcy);
  - `AI_TurnToVob(npc, nazwa, ?)` (`0x101e1a30`): w kolejce – obrót do punktu, mechanizmu albo moba o tej nazwie (ściana ukrytego pokoju Nefariusa); nowa akcja `AI_TurnToPos`.
- **Do sprawdzenia:** świątynia Asztar (wybór skrzyni); zakład u Orlana; Nefarius przy ścianie.

### 217. Czary potworów w kolejce (`AI_PlayEffect`), zasięg walki królowej pełzaczy, zamrożenie – 🧪 (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/fightalgo.cpp`, `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `common/world/aiqueue.cpp`, `common/world/aiqueue.h`, `common/game/constants.h`
- **Zgłoszenie:** jak 221.
- **Plugin:**
  - `AI_PlayEffect(npc, efekt, cel, obrażenia, typ)` (`0x101c7c50` → `0x10057e00`): wiadomość w kolejce AI – po obrocie i animacji (`T_FISTATTACKMOVE`) efekt leci w cel i rani go jak `Wld_PlayEffect_Vec` (ognisty pocisk drakonian, sieć królowej pełzaczy, lodowy pocisk); efekt na siebie – tylko efekt (rytuał Cronosa). Nowa akcja kolejki `AI_PlayEffect`;
  - `Npc_SetFightRange(npc, r)` (`0x101cf9a0`): zasięg podstawowy tej postaci `r`, pięści i G `r+50` zamiast wartości gildii (królowa pełzaczy: 180);
  - `AST_FreezeNpc(npc, sekundy, efekt)` (`0x101b80c0`): magiczne strzały i bełty, broń mrozu zamrażają ofiarę na tyle sekund (animacja stoi, bez ruchu, ciosu i kolejki AI). Z efektem: lód `SPELLFX_ICESPELL_SENDPERCEPTION_MagicArrow` i nie częściej niż co 3 s. Odporne (`0x10055b80`): trolle, smoki, demony, lodowe golemy, `SKELETON_MI_HIGH_PRIEST`, postać w `ZS_STUN`.
- **Do sprawdzenia:** drakonianie i królowa pełzaczy strzelają czarami; magiczna strzała zamraża potwora na 1 s.

### 216. Potwór stał w walce, gdy cel był za przeszkodą – 🧪 (24.11.2026)
- **Pliki:** `common/world/objects/npc.cpp`
- **Zgłoszenie (log):** „Smoczy zębacz stands 8 s in ZS_MM_ATTACK – target … at 642 cm (reach 300), sees it 0, ai [(empty)]”: zębacze stały w animacji biegu, a golem był 6–8 m dalej za skałą.
- **Przyczyna:** `Npc::implAttack` przy celu niewidocznym (promień trafia w przeszkodę) szukał punktu drogi przy celu i szedł do niego. Gdy potwór już przy tym punkcie stał, dostawał drogę z tego jednego punktu na każdej klatce, od razu ją kończył i stał tak przez całą walkę.
- **Poprawka:** gdy punkt jest bliżej niż 2,5 m albo nie ma do niego drogi, potwór biegnie prosto na cel, jak `AI_Attack` oryginału.
- **Do sprawdzenia:** potwory i przywołańcy za przeszkodą biegną do celu zamiast stać (w logu brak `[idle] … sees it 0`).
- **Test (25.11.2026, log):** zębacze nie stały już przy golemie za przeszkodą; golem między dwoma zębaczami ciągle zmieniał cel – poprawka 222. Zostały: zębacz w płytkiej wodzie przy brzegu, 9–10 m pod bohaterem stojącym wyżej (`[blocked]` o zbocze – bohater poza zasięgiem, jak na PC); zębacz „w powietrzu” 5 s (`inAir=1 body=10`), najpewniej na grzbiecie golema – do obserwacji.

### 215. Zamykanie zadań, zwoje postaci, trofea ze skryptów, mechanizmy – ✅ (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/ui/inventorymenu.cpp`
- **Zgłoszenie:** dalsze funkcje pluginu, które skrypty wołają, a portu nie ma (raport poprawki 213).
- **Plugin (UNION_ABI.DLL z aktualizacji):**
  - `Log_GetTopicStatus(temat)` (`0x101a8280`): status tematu dziennika (1 trwa, 2 ukończone, 3 nieudane, 4 nieaktualne), bez tematu 0. Bez tej funkcji `B_CloseTopic_Now` nigdy nie zamykało zadań – zadania, które NB kończy samo („Zadanie zakończone!”), zostawały otwarte;
  - `Npc_IsDoingRoutine(npc, nazwa)` (`0x101cddf0`): rutyna dnia postaci to `RTN_<nazwa>_<id>` (dialogi Martina, Pustelnika, notatki nekromanty, zwiadowca);
  - `Itm_SetFlagActive(przedmiot, wł.)` (`0x101bc230`): flaga `ITEM_ACTIVE` przedmiotu – trofea zakładane przez skrypty (`USE_ItAt_*`: pazur, kieł, płyta przy pasie) są w ekwipunku oznaczone jak noszone;
  - zwoje postaci: `Npc_HasScroll`, `Npc_GetScroll` (losowy zwój postaci – jego czar, `0x101cfa70`), `Npc_HasScrollExp` (tylko na Drodze Prób: `RX_RoadOfTrialsActiveValue` = 938491, bez zmiennej NB 222 i zwoju przemiany w chrząszcza) – bandyci i inni rzucają czary ze zwojów (`B_SelectSpell`); zwój to przedmiot kategorii run, który się zlicza, nie `ITRU_*` (`0x10088950`);
  - `Wld_OpenMoverIfClosed` / `Wld_CloseMoverIfOpened(nazwa)` (`0x101cb450`, `0x101cb2e0`): mechanizm o tej nazwie dostaje sygnał tylko, gdy jest zamknięty / otwarty (bramy po śmierci smoków, magazyn Azgana, wieża Mrocznego Mówcy);
  - `Npc_CanSeePlayerInRadius(promień)` (`0x101c7dd0`): żywa postać w promieniu widzi bohatera (otwieranie orkowej skrzyni).
- **Jeszcze nie:** worki (`OpenPocket`), `Wld_ChangePortal`, `AI_PlayEffect`, `StartChangeWorldTime`, szukanie roślin przez ognika, `Npc_SetFightRange`, `Mob_SetLockedByName`, `Mob_RemoveItem_AB`, `AI_TurnToVob`, `AST_FreezeNpc`, `Ext_DarkScreen`, `Wld_SetCondFunc`, `InitFullSnc` – zrobione w poprawkach 217–221.
- **Do sprawdzenia:** zadania zamykane przez NB („Zadanie zakończone!”); trofeum z pazura po użyciu z zieloną ramką w ekwipunku.
- **Test (25.11.2026):** pazur po założeniu oznaczony w ekwipunku jako założony ✅. Zamykanie zadań przez NB jeszcze niesprawdzone.
- **Test (25.11.2026, log):** nauka języka orków – wpisy dziennika są, ale log nie pokazywał statusu zadania; dodane w poprawce 225.
- **Test (25.11.2026):** po przeczytaniu „Języka orków” był dźwięk zakończonego zadania – `B_CloseTopic_Now` zamyka zadania ✅.

### 214. Patrole, oczy szkieletów, `Npc_HasMessage`, `Npc_HasSprintMoveset`, karma – 🧪 (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `common/world/world.cpp`, `common/world/world.h`, `common/world/aiqueue.h`
- **Zgłoszenie:** funkcje pluginu, które skrypty wołają, a portu nie ma (raport poprawki 213) – zacząć od patroli i `UpdateKarma`.
- **Plugin (UNION_ABI.DLL z aktualizacji):**
  - `Patrol_Set` / `Patrol_SetPath(npc, "WP1|WP2|…", godzina, minuta, czekanie, stan)` (`0x104567b0`, `0x10455fa0`): patrol zamieniany na wpisy rutyny dnia postaci (jak `TA_*`), cała doba od godziny:minuty. `Patrol_Set` – od pierwszego punktu do każdego innego i z powrotem; `Patrol_SetPath` – wzdłuż ścieżki i z powrotem. Wpis trwa długość drogi po sieci punktów / 833,33 cm na minutę (zaokrąglone) + czekanie, 2–1440 minut; okrążenie powtarzane 1440 / jego długość razy, reszta doby rozdzielona równo (czekanie > 0) albo dodana do ostatniego wpisu. Brak drogi – linia prosta (jak plugin, „U: PATROL: no route”);
  - `Patrol_NextWP(npc)` (`0x10456cf0`): gdy obecny wpis rutyny kończy się za 3 minuty lub mniej, a postać stoi przy jego punkcie – punkt następnego wpisu (`ZS_Patrol_Walk_Loop`), inaczej pusty;
  - `Npc_SetEffect(npc, n)` (`0x100e23d0`): efekt postaci – 1 `SPELLFX_REDEYES`, 2 `SPELLFX_GREENEYES`, 3 `SPELLFX_WHITEEYES`, inaczej brak (`RX_UpdateSkeletonEyes`, opcja koloru oczu szkieletów);
  - `Npc_HasMessage(npc, typ, podtyp)` (`0x1008b890`): wiadomość danego rodzaju w kolejce postaci; skrypty pytają tylko o (2, 2) – idzie do postaci lub przedmiotu (`ZS_MM_Attack_Loop`);
  - `Npc_HasSprintMoveset(npc)` (`0x101cb900`): założone animacje sprintu `HUMANS_SPRINT` / `HUMANS_MAGESPRINT` (`ZS_Attack_Loop`, `ZS_MM_Attack_Loop`);
  - nowa karma (`UpdateKarma` – 130 wywołań, `UpdateKarmaTrophy`, `Karma_Update_Quest`) działa tylko przy zmiennej `varNewKarmaEnabled` ≥ 0 (`0x100ff7b0`); tych skryptów nie ma (`IsNewKarmaEnabled` zwraca FALSE), więc na PC nic nie robią – w porcie tak samo, bez komunikatów o brakującej funkcji; `Karma_GetItemMunition(przedmiot)` – amunicja przedmiotu; `Karma_FanaticPickWeapon`, `Karma_PlayAttackSound` – fanatycy i strażnicy karmy, nieaktywni.
- **Uwaga:** patrole w tych skryptach mają postacie karmy (`NPC_100300_K`, punkty `WP_NW_KARMA_SPAWN_*`), które wstawia wyłączony system karmy – będą działać, gdy NB go włączy.
- **Do sprawdzenia:** szkielety – kolor oczu według opcji; w ataku potworów bez komunikatów o `NPC_HASMESSAGE`, `NPC_HASSPRINTMOVESET`, `UPDATEKARMA`.

### 213. `C_PlayerIsFakeBandit` – postać spoza świata; raport brakujących funkcji, które skrypty wołają – 🧪 (24.11.2026)
- **Pliki:** `lib/ZenKit/src/DaedalusVm.cc`, `lib/ZenKit/include/zenkit/DaedalusVm.hh` (`patches/zenkit-opengothic.patch`), `common/utils/nblog.cpp`, `common/game/savesymbols.cpp`
- **Zgłoszenie (log):** `Accessing member "C_NPC.AIVAR" without an instance set (in C_PLAYERISFAKEBANDIT <- B_ASSESSPLAYER)` – ponad 100 razy w zamku; raport `[scripts]` wymienił 147 funkcji, których port nie ma; około 60 linii `random animation … of S_EAT is not played`.
- **Skrypty NB:** `C_PlayerIsFakeBandit` sprawdza `… && CurrentLevel == ADDONWORLD_ZEN && (Player_HasTalkedToBanditCamp == TRUE || BDT_1071_Addon_Ramon.aivar[14] == TRUE)` – Daedalus liczy całe wyrażenie, więc poza światem dodatku czyta Ramona, którego tam nie ma (jak w oryginalnym G2). Wynik 0 – na PC tak samo. To nie błąd portu.
- **Poprawka:**
  - komunikat brakującej instancji podaje symbol: `…; no instance in BDT_1071_ADDON_RAMON` (ZenKit: symbol ostatniego `GMOVI`);
  - taki komunikat dla postaci (nie parametru funkcji) nie jest liczony w podsumowaniu jako błąd skryptów;
  - raport `[scripts]`: tylko funkcje, które skrypty naprawdę wywołują (reszta to nieużywane funkcje Gothic 1). Na tych skryptach 40, z tego nowe w aktualizacji: `Patrol_SetPath` (61 wywołań), `Patrol_Set` (24), `Patrol_NextWP`, `Npc_SetEffect`, `Karma_FanaticPickWeapon`, `Karma_GetItemMunition`; starsze: `UpdateKarma` (130), `Wld_ChangePortal` (44), `Itm_SetFlagActive` (41), `AI_PlayEffect` (17), `StartChangeWorldTime`, `Npc_IsDoingRoutine`, `OpenPocket`, `Mob_SetLockedByName`, `Wld_Open/CloseMoverIf…`, `AST_FreezeNpc`, `Npc_GetScroll`, `Wld_SetCondFunc` i inne;
  - `random animation … is not played` tylko w pełnym logu.
- **Do sprawdzenia:** w podsumowaniu nie ma `C_PLAYERISFAKEBANDIT`; w logu `[scripts] functions … that the scripts call and the port does not have: 40: …`.

### 212. Aktualizacje skryptów NB bez konfliktów: raport przy starcie, nazwy starych skryptów, postacie po id i imieniu – 🧪 (24.11.2026)
- **Pliki:** `common/game/savesymbols.cpp`, `common/game/savesymbols.h`, `common/game/serialize.cpp`, `common/game/gamescript.cpp`, `lib/ZenKit/include/zenkit/DaedalusVm.hh` (`patches/zenkit-opengothic.patch`)
- **Zgłoszenie:** po aktualizacji skryptów NB było zamieszanie (zapisy, przedmioty, nowe funkcje pluginu) – aktualizacje mają przechodzić bez problemów.
- **Stan po 209–211:** nowe zapisy mają nazwy wszystkich funkcji i instancji (`game/symtab`), także dla odwiedzonych światów (`worlds/<świat>.symmap`), więc kolejna aktualizacja przelicza je po nazwach; nieznany przedmiot jest szukany po wyglądzie. Zostawały: przemianowane postacie, brak informacji, co aktualizacja zmieniła, i nazwy starych skryptów potrzebne zapisom sprzed 209.
- **Poprawka:**
  - **raport przy starcie** (raz na uruchomienie, po wczytaniu skryptów): porównanie z tablicą symboli poprzedniego startu (`nb_symbols_last.txt` + `nb_symbols_last.hash` obok zapisów). Po aktualizacji w logu: `[scripts] the scripts are not the scripts of the last start (… symbols, now …): N new functions and instances, M removed: …` oraz `[scripts] new functions of the plugin: PATROL_SET (not in the port) …`;
  - **nazwy starych skryptów zachowywane same:** przy zmianie skryptów poprzednia tablica zostaje jako `old_symbols_<odcisk>.txt` – zapisy bez `game/symtab` biorą z niej dokładne nazwy (poprawka 209), bez ręcznego wgrywania plików;
  - za każdym startem lista funkcji silnika i pluginu (poza `RX_*`, które już są w logu NB), których port nie ma: `[scripts] functions of the engine or of the plugin that the port does not have: …` (ZenKit: `DaedalusVm::has_external`);
  - **postacie:** postać z nieznanym symbolem (przemianowana instancja albo nieznane miejsce w starych skryptach) jest szukana po id i imieniu z kodu instancji (kod tylko czytany); przyjmowana, gdy pasuje jedna. W nowych skryptach 2290 par id/imię, 2281 jednoznacznych. W logu `[save] '<imię>' (id N) of the save-game is the character <INSTANCJA> of the scripts now`; zły symbol postaci nie przerywa już wczytywania.
- **Czego nie da się przenieść (na PC tak samo):** liczby w zmiennych skryptów, które trzymają numer instancji (`Hlp_GetInstanceID` zapisane w zwykłej zmiennej), oraz to, co aktualizacja usunęła ze skryptów. Nowe funkcje pluginu trzeba dopisać do portu – raport je wymienia.
- **Do sprawdzenia:** pierwszy start – w logu lista funkcji, których port nie ma; obok zapisów pojawia się `nb_symbols_last.txt`. Przy następnej aktualizacji skryptów – raport `[scripts] …` i `old_symbols_<odcisk>.txt`.

### 211. Przedmioty odwiedzonych światów ze starych zapisów, krótszy log – ✅ (24.11.2026)
- **Pliki:** `common/game/savesymbols.cpp`, `common/game/savesymbols.h`, `common/world/objects/item.cpp`, `common/game/inventory.cpp`, `common/world/worldobjects.cpp`, `common/utils/nblog.cpp`, `common/mainwindow.cpp`
- **Zgłoszenie (log):** po teleporcie do Khorinis `Loading unknown item from save` i przedmioty usunięte z ekwipunku: list u Xardasa i Gallahada, tarcze Cedrika i Jarvisa, stare zapiski Ahirona, zbroja upadłego paladyna. Do tego `[hero-ai] the hero moves by himself (333 m/s)` po teleporcie i 1394 linie `is far from the player, it does not walk to the way point` (dwie trzecie logu).
- **Przyczyna:**
  - Khorinis był w zapisie (sprzed aktualizacji skryptów) tylko jako zapisany świat. Jego przedmioty nie miały nazw w zmiennych zapisu (nazwy mają tylko przedmioty świata, w którym bohater był przy zapisie), więc ich numery dało się przeliczyć tylko „po miejscu” (poprawka 209). Przy kilku przedmiotach to miejsce było niejednoznaczne. Te przedmioty są w nowych skryptach (`FAKESCROLL`, `ITAR_SHIELD_07_CEDRIK`, `ITAR_SHIELD_02`, `ITWR_AHIRONNOTE`, `ITAR_PAL_SKEL_P`), tylko pod innymi numerami.
  - Porównanie starej kopii skryptów z nową wersją, „po miejscu”: przedmioty 3743 dobrze, 0 źle, 33 nieznane; postacie 5779 dobrze, 0 źle, 2 nieznane; dialogi 10 589 dobrze; funkcje 41 317 dobrze, 0 źle, 224 nieznane.
  - Detektor `[hero-ai]` porównywał pozycję bohatera przed i po zmianie świata.
- **Poprawka:**
  - przedmiot z nieznanym numerem (tylko w zapisie innych skryptów) jest szukany wśród przedmiotów skryptów po wyglądzie: model, nazwa i wartość, a gdy to nie wystarcza, model i nazwa. Przedmiot jest przyjmowany, gdy pasuje dokładnie jeden; jego funkcje (`on_equip`, `on_unequip`, `on_state`…) idą z jego kodu. Wygląd przedmiotów jest czytany z kodu instancji (prototyp, instancja, wywoływane funkcje do 3 poziomów), kod nie jest uruchamiany; raz, tylko przy takim zapisie (w logu `[save] the looks of N items`). Na nowych skryptach: 3596 przedmiotów, wszystkie 6 utraconych rozpoznane jednoznacznie. W logu `[save] '<nazwa>' (<model>) of the save-game is the item <INSTANCJA> of the scripts now`;
  - komunikat usunięcia: „not found in the scripts now”;
  - przy poziomie logu „normal” bez linii `is far from the player…`, `CsCamera:` i `[zenkit] Parsing …` (są w pełnym logu);
  - `[hero-ai]`: po zmianie świata pomiar od nowa, skoki ponad 30 m (teleport) pomijane.
- **Uwaga:** zapis zrobiony na końcu tej gry ma już Khorinis bez tych 6 przedmiotów. Wczytany na początku zapis (sprzed teleportu) ma Khorinis z mapą starych numerów, więc z tą poprawką przedmioty wrócą.
- **Awaria w pierwszej wersji (24.11.2026):** wygląd przedmiotów był liczony uruchamianiem kodu wszystkich instancji przedmiotów podczas wczytywania świata – po teleporcie `SIGSEGV` w `DaedalusVm::push_reference` (`init_instance` ← `itemByLook` ← `Inventory::implLoad`). Teraz kod jest tylko czytany.
- **Do sprawdzenia:** wczytać zapis sprzed teleportu do Khorinis i przenieść się tam: w logu `… is the item FAKESCROLL …` itd., bez `dropped`.
- **Test (24.11.2026):** po teleporcie do Khorinis wszystkie 6 przedmiotów rozpoznane (`[save] 'List' (Fakescroll.3ds) … is the item FAKESCROLL …`), tabela wyglądu 3596 przedmiotów w 11 ms, bez awarii.

### 210. Aktualizacja skryptów NB z 21.11.2026: `Hlp_GetIntArrayValue`, łańcuch wywołań w błędach skryptów – ✅ (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `lib/ZenKit/src/DaedalusVm.cc`, `lib/ZenKit/include/zenkit/DaedalusVm.hh` (`patches/zenkit-opengothic.patch`)
- **Zgłoszenie:** nowe skrypty NB (gałąź `NewBalance`, `nb-scripts/`) – sprawdzić log: `not implemented call [HLP_GETINTARRAYVALUE] from C_HEROHASSPIRITS ("karma_spirit_count", 0)` (28 razy na minutę), `Accessing member "C_NPC.AIVAR" without an instance set (in C_PLAYERISFAKEBANDIT)`.
- **Plugin (UNION_ABI.DLL z aktualizacji):** `Hlp_GetIntArrayValue(nazwa, indeks)` (`0x101e7540`) – element zmiennej skryptów o tej nazwie (`zCParser::GetSymbol`), bez takiej zmiennej -1; `Hlp_SetIntArrayValue(nazwa, indeks, wartość)` (`0x101e7500`) – zapis elementu. Skrypty pytają o `karma_spirit_count`, `karma_l5_stamina`, `karma_l5_stamina_acc` (duchy i wytrzymałość karmy, `C_HeroHasSpirits`, `RX_RestoreStamina`, `RX_LearnPoisonResist`) – tych zmiennych nie ma w tej wersji skryptów, więc na PC też jest -1 (bez duchów, bez wytrzymałości L5).
- **Poprawka:**
  - obie funkcje jak w pluginie;
  - błąd „Accessing member … without an instance set” pokazuje łańcuch wywołań (do 5 funkcji: `C_PLAYERISFAKEBANDIT <- B_ASSESSPLAYER <- …`) – funkcja ma ten sam kod co przed aktualizacją, a w każdym miejscu skryptów dostaje `self`; następny log pokaże, kto ją woła bez postaci.
- **Nowe funkcje pluginu w tej wersji, jeszcze nie w porcie:** patrole (`Patrol_Set` w 24 rutynach `Rtn_PatrolNW*`, `Patrol_SetPath` w 61 rutynach `Rtn_NW*_100300`, `Patrol_NextWP` w `ZS_Patrol_Walk_Loop`), `Npc_SetEffect` (oczy szkieletów, `RX_UpdateSkeletonEyes`), `Karma_FanaticPickWeapon` i `Karma_GetItemMunition` (`B_SetNpcSettings`), 11 innych `Karma_*` (bez wywołań w skryptach). Log pokaże je jako `not implemented call`.
- **Do sprawdzenia:** brak `HLP_GETINTARRAYVALUE` w podsumowaniu; przy `C_PLAYERISFAKEBANDIT` w logu łańcuch funkcji.
- **Test (24.11.2026):** `HLP_GETINTARRAYVALUE` nie ma w podsumowaniu; błąd pokazał łańcuch `C_PLAYERISFAKEBANDIT <- B_ASSESSPLAYER` (przyczyna – poprawka 213).

### 209. Zapisy po aktualizacji skryptów NB („Cannot init USE_RAWMEAT_MOUNT: not an instance”) – ✅🧪 (24.11.2026)
- **Pliki:** 🆕 `common/game/savesymbols.cpp`, 🆕 `common/game/savesymbols.h`, `common/game/serialize.cpp`, `common/game/serialize.h`, `common/game/gamesession.cpp`, `common/game/gamescript.cpp`, `common/game/inventory.cpp`, `common/game/worldstatestorage.cpp`, `common/game/worldstatestorage.h`, `common/world/objects/item.cpp`, `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `common/world/aiqueue.cpp`, `common/world/worldobjects.cpp`
- **Zgłoszenie:** po wgraniu nowej aktualizacji skryptów NB zapis się nie wczytuje: „Unable to load the game – save-game 'gothicii_quicksave_5.sav': Cannot init USE_RAWMEAT_MOUNT: not an instance”.
- **Przyczyna:** zapis portu trzyma numery symboli skryptów (instancje postaci i przedmiotów, funkcje stanów AI i rutyn, funkcje przedmiotów, znane dialogi). Nowy `GOTHIC.DAT` dodaje symbole, więc numery się przesuwają: numer przedmiotu z zapisu wskazuje teraz funkcję `USE_RAWMEAT_MOUNT`. Silnik na PC zapisuje nazwy, więc tam zapisy przechodzą aktualizacje.
- **Poprawka:**
  - nowe zapisy mają wpis `game/symtab` (nazwy funkcji, prototypów i instancji, odcisk skryptów); przy wczytaniu z innymi skryptami numery są przeliczane po nazwach; te same skrypty – bez zmian;
  - starsze zapisy: nazwy ze zmiennych zapisu (`game/daedalus`: zmienne, parametry i zmienne lokalne funkcji, postacie i przedmioty), funkcja przed swoim pierwszym parametrem („F” przed „F.PAR0”), reszta po miejscu między znanymi symbolami (te same rodzaje symboli, jeden dodany lub usunięty blok); gdy jest plik `old_symbols*.txt` (nazwa symbolu N w wierszu N+1) zgodny ze zmiennymi zapisu – wszystkie nazwy z niego;
  - mapa obejmuje: postacie (`readNpc`, rutyna, stan startowy), przedmioty (instancja, funkcje, właściciel, amunicja), funkcje stanów, percepcji, rutyn i kolejki AI (`ScriptFn`), przedmioty w kolejce AI, aktualny przedmiot i runę, znane dialogi, przedmioty w zmiennych skryptów, odwiedzone światy (wpis `worlds/<świat>.symmap`, łączony przy kolejnych aktualizacjach);
  - czego nie ma w nowych skryptach: przedmiot jest pomijany (wpis `[save] … dropped`), dialog zapomniany (może wrócić), stan rutyny – rutyna postaci tworzona od nowa przy pierwszym ticku; zły symbol przedmiotu nie przerywa już wczytywania.
- **Sprawdzone:** symulacja na tablicy symboli NB (272 601) z losowymi zmianami: 60 zmian – 63 061 funkcji i instancji dobrze, 0 źle, 40 nieznanych; 300 zmian – 3 źle, 671 nieznanych.
- **Do sprawdzenia:** wczytanie `gothicii_quicksave_5.sav`; w logu `[save] the scripts are not the scripts of the save-game …` z liczbami. Z plikiem `old_symbols_pl.txt` (nazwy skryptów z kopii na gałęzi NewBalance) obok zapisów – w logu `the names of old_symbols_pl.txt`. Potem nowy zapis i wczytanie go.
- **Test (24.11.2026):** stary zapis po aktualizacji skryptów wczytuje się, działa. Do sprawdzenia przy następnej aktualizacji: zapis z `game/symtab`.

### 208. Moc magiczna: obrażenia magiczne kosturów i mieczy magicznych, przebicie pancerza przywołańców nekromanty – 🧪 (24.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamescript.h`, `common/game/damagecalculator.cpp`
- **Zgłoszenie:** czy działa moc magiczna (arkusz „Kostury i miecze magiczne”: inteligencja wg kręgu, szansa 5% + moc + umiejętność broni).
- **Moc magiczna** to `CountLearnSpell` (0–150). Skrypty, które jej używają (np. czar Severe Guru, mroczna niewidzialność), działały; nie było części pluginu.
- **Plugin (UNION_ABI.DLL):**
  - `0x100dbb50`, wołane przez hak obrażeń `0x100dcbf0` przy ciosie bohatera bronią jedno- lub dwuręczną (nie w przemianie, nie w przyjaciela): obrażenia magiczne broni = (podstawa broni z tabeli pluginu + inteligencja wg kręgu: bez kręgu 1/8, 1. krąg 1/5 (20%), 2. – 1/4, 3. – 1/2, 4. – 70%, 5. i 6. – 100%) × (0,5 + (5 + moc magiczna + umiejętność broni 1H/2H) / 200), minus ochrona celu przed magią, dodane do ciosu. Podstawy: ostrze adepta 50, ostrze maga 90, ostrze maga (Yark) 145, ostrze zaklęć 250, ostrze posłańca 275, ostrze burzy 300, ostrze zaćmienia i szpon Beliara 350, szpon Beliara (z duszą) 410, ostrze Beliara 475; kostur nowicjusza i druidów 1 – 50, kostury gildyjne (ognia, wody, druida), czarny kostur, kostur druidów 2 – 90, kostur Yarika 150, kostury z Jarkendaru i druidów 3 – 275, kostur z Wyspy 300, wężowy 325, szpon 335, szpon (Senyak) 450, smoczy 410, krwawy 480, demoniczny 430, orkowe 180;
  - `0x100611c8` (hak obrażeń `0x1005d440`): cios przywołańca przechodzi przez część ochrony przed bronią sieczną i obuchową – 5% z `RX_NecroSummonBoostDamageLearned` oraz 5% + moc magiczna / 2 z `RX_NecroRuneAuraActive_02` (nekromanta).
- **Poprawka:** oba – obrażenia magiczne doliczane do ciosu bohatera (w logu co 3 s `[ctx] magic weapon …` z wyliczeniem), przebicie ochrony przy ciosie przywołańca.
- **Jeszcze nie:** moc magiczna w obrażeniach ognia rozchodzącego się po burzy ognia, eksplozji, deszczu meteorów i pirokinezie (`0x10062200`: (100/125/200/150 + INT/5) × (100 + moc/4)% jako podpalenie) – port nie ma jeszcze samego rozchodzenia się ognia; przebicie pancerza przez konkretne bronie (halabardy itp., tabele pluginu przy `0x10060f8c`).
- **Runy:** moc magiczna była już w obrażeniach run (poprawka 126, jak w pluginie `0x10082190`: wartość × (1 + moc/100), przeliczane co 3 s); wpis w logu `spells: the damage counted again` pokazuje teraz też moc magiczną.
- **Do sprawdzenia:** cios kosturem lub mieczem magicznym – większe obrażenia, w logu `magic weapon <broń>: base …, intelligence … (circle …), magic power …, skill … -> … magic, protection … -> +…`.

### 207. Trofea New Balance można założyć – ✅ (24.11.2026)
- **Pliki:** `common/game/inventory.cpp`, `common/game/inventory.h`, `common/game/constants.h`
- **Zgłoszenie (zrzut):** trofeum „Grymuar” (Inteligencja +10, szansa na manę przy ataku magią) – „Załóż” nic nie robi.
- **Skrypty NB:** 23 trofea mają `ITEM_KAT_MAGIC` i flagę `ITEM_TROPHY` (1 << 25, flaga New Balance, nieznana w Gothicu); zakłada je plugin, resztę robią skrypty: `ON_EQUIP` (np. `Equip_ItUt_MagicBookWC`: `B_ClearUseTrophy`, model trofeum na `BIP01 PELVIS`, `RX_GrimuarUsedWC`, inteligencja +10), `ON_UNEQUIP` – odwrotnie. Jedno trofeum naraz.
- **Przyczyna:** w porcie przedmiot magii bez flagi pierścienia, amuletu ani pasa nie miał miejsca do założenia, a trofea nie mają funkcji użycia – „Załóż” kończyło się bez skutku.
- **Poprawka:** osobne miejsce na trofeum (jak pas czy amulet): „Załóż” wywołuje `ON_EQUIP`, założenie innego trofeum najpierw zdejmuje poprzednie (`ON_UNEQUIP`), „Zdejmij” zdejmuje. Miejsce nie jest osobnym wpisem w zapisie – po wczytaniu wraca z przedmiotu oznaczonego jako założony (jak hełm, poprawka 39), więc stare save'y działają.
- **Do sprawdzenia:** grymuar – „Załóż”: inteligencja +10, model przy pasie, przedmiot oznaczony jako założony; drugie trofeum – pierwsze się zdejmuje (inteligencja wraca); zapis i wczytanie – trofeum dalej założone.
- **Test (24.11):** działa – grymuar się zakłada. Szansa na manę przy ataku magią (opis trofeum) – nie znaleziona ani w skryptach, ani w pluginie; do sprawdzenia na PC, czy w ogóle działa.

### 206. „NOWY” tylko przy nieprzeczytanych nowych dokumentach – ✅ (24.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp`, `common/game/gamesession.cpp`
- **Zgłoszenie (zrzut):** w zakładce dokumentów 48 z 49 ma znacznik „NOWY”, także dawno przeczytane; ma być oznaczone tylko to, czego bohater nie przeczytał.
- **Przyczyna:** lista nieprzeczytanych dokumentów (poprawka 122, zapisywana w save'ie) dostawała każdy dokument, którego liczba wzrosła względem migawki ekwipunku. Gdy migawka była zrobiona, zanim wrócił ekwipunek (wczytanie, zmiana świata), cały zbiór dokumentów trafiał na listę jako „nowy” i zostawał w zapisie – znikał dopiero po przeczytaniu każdego z osobna.
- **Poprawka:**
  - nowy jest tylko dokument, którego bohater wcześniej nie miał (druga kopia tego samego nie);
  - więcej niż 8 dokumentów naraz to powrót ekwipunku, nie nowe – bez znaczników (w logu `[newdocs] N documents at once`);
  - każdy oznaczony dokument w logu: `[newdocs] new, not read yet: <instancja>`;
  - lista w save'ie w nowej wersji (`game/nb_newdocs2`): stara, zaśmiecona lista nie jest wczytywana – po wczytaniu save'a znaczników „NOWY” przy dokumentach nie ma, pojawią się przy następnych zdobytych. „Czytaj” zdejmuje znacznik jak dotąd.
- **Do sprawdzenia:** wczytanie save'a – dokumenty bez „NOWY”; podniesienie nowego listu/mapy – „NOWY” tylko przy nim, po „Czytaj” znika; zmiana świata – znaczniki nie wracają.
- **Test (24.11):** działa.

### 205. Zasięg walki do bryły długiego celu – golemy trafiają pustynnego węża – ✅ (24.11.2026)
- **Pliki:** `common/world/objects/npc.cpp`, `common/world/objects/npc.h`, `common/game/fightalgo.cpp`
- **Zgłoszenie:** golemy stoją przy pustynnym wężu i dają się gryźć; na PC go biją.
- **Log (204):** golem w zasięgu: odległość walki 411–418 cm przy zasięgu 420 cm, bez zachwiań (`bodystate_interruptable_override` = 1), 0 trafień; drugi golem 106 cm od węża – też 0. Wąż (6 m długości) gryzie golemy z 417 cm przy swoim zasięgu 500.
- **Przyczyna:** skrypty opisują zasięg walki jako odległość „od Bip01 do bryły” celu (`FAI_W = BASE + broń`). Port mierzy ją między dwoma punktami (środki postaci, przy wężu – środek jego bryły) i dolicza bazę celu jako jego „promień” – dobre dla człowieka czy wilka, ale wąż jest dużo dłuższy niż jego baza (200). Golem przy boku węża stał więc ~4,15 m od jego środka, na samej granicy zasięgu 4,2 m: AI uznawała, że może uderzyć, a cios w chwili trafienia (`commitDamage`) był już poza zasięgiem. Golem zbyt blisko środka uznawany był za „za blisko” (odskok).
- **Poprawka:** odległość walki do celu, którego bryła sięga dalej niż 1,2 × jego baza: od punktu walki atakującego do bryły celu (z jej obrotem i skalą), plus baza celu – sumy baz w skryptach zostają poprawne. Dla zwykłych postaci bez zmian. Dotyczy AI walki, trafień (`commitDamage`) i odległości w logu.
- **Do sprawdzenia:** golemy przy pustynnym wężu – biją go od razu (w znaczniku „blows given” rośnie, bez `[idle] … without a blow`). Przy okazji: bohater też łatwiej trafia długie stwory (jak w Gothicu).
- **Test (24.11):** golemy naprawione – biją pustynnego węża.

### 204. Log: golem w zasięgu bez ciosu, odległość walki w znaczniku, porządki w podsumowaniu – ✅ (24.11.2026)
- **Pliki:** `common/world/objects/npc.cpp/.h`, `common/utils/nblog.cpp/.h`, `common/mainwindow.cpp`, `common/game/gamescript.cpp`
- **Log (24.11, golemy i pustynny wąż):** po „Atak” celem golemów był wąż 26 m dalej (`STUCK … fight=2607/420 see=0`). Gdy wąż podszedł i gryzł golemy co ~1 s, znaczniki 1–2 pokazują golemy obok, w `ZS_MM_ATTACK`, z wężem jako celem i pustą kolejką AI – ale bez ani jednego ciosu; uderzyły dopiero, gdy wąż przerzucił się na bohatera (`reattack: turns to`). Poprawka 198 nie zadziałała, bo wąż już był ich celem. `[idle]` nie wypisał nic – sprawdzał tylko cel poza zasięgiem.
- **Przypuszczenie:** każde ugryzienie przerywa cios golema (zachwianie – w porcie przy ciosie postaci w postać, bez `bodystate_interruptable_override`), więc golem nie kończy żadnego ciosu. Do potwierdzenia nowym wpisem.
- **Poprawka (diagnostyka):**
  - `[idle] … in reach of … for 8 s without a blow` – postać w zasięgu celu, która przez 8 s nie trafia: odległość walki / zasięg, czy widzi cel, ciosy przyjęte i zachwiania w tym czasie, czy trwa animacja ataku, `bodystate_interruptable_override`;
  - w znaczniku przy każdej postaci z celem: odległość walki / zasięg, czy widzi cel, ciosy zadane / przyjęte / zachwiania od startu;
  - podsumowanie: lista `NB: bound` to już informacja, nie błąd; archiwa DLL/ZIP Uniona i `VDFS tool disk` w ostrzeżeniach danych; bez dubli „sound fx” obok pliku dźwięku i bez resztek ścieżek binarnych; `[mem]` z czasem od startu.
- **Do sprawdzenia:** golemy gryzione przez węża – w logu `[idle] … without a blow: … blows taken N, stumbles M`; przy M bliskim N przyczyną jest przerywanie ciosów.
- **Test (24.11, log):** `[idle] … in reach of Wąż pustynny for 8 s without a blow: fight 417 cm (reach 420) … blows taken 3, stumbles 0 … interruptable override 1` – golem się nie chwieje (przypuszczenie o przerywaniu odpada), stoi na samej granicy zasięgu; drugi golem 106 cm, też bez ciosu. Znaczniki: golemy 0 ciosów zadanych przy 4–9 przyjętych. Na PC golemy normalnie biją węża. Dalej: poprawka 205.

### 199–203. Rozbudowany log do zgłoszeń – ✅ (24.11.2026)
- **Pliki:** nowe `common/utils/nblog.cpp` i `nblog.h`; `game/main.cpp`, `common/utils/crashlog.cpp/.h`, `common/world/objects/npc.cpp/.h`, `common/game/damagecalculator.cpp`, `common/mainwindow.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp/.h`
- **Zgłoszenie:** log ma szybciej pokazywać błędy, zwłaszcza takie jak przy golemach, gdzie w logu nie było żadnego błędu, a postacie nic nie robiły; przyda się przy kolejnych bossach.
- **199 – znacznik (przycisk MARK):** nowy przycisk na nakładce (u góry, obok LOG; można go przesunąć lub ukryć w edytorze nakładki). Dotknięcie dopisuje do logu blok `[mark N]`: czas od startu, świat, dzień i godzina gry, pozycja, życie, mana, broń i stan ciała bohatera, jego cel, oraz do 12 postaci w promieniu 30 m – odległość, stan AI, życie, cel, broń, stan ciała, kolejka AI. Na ekranie „Znacznik w logu: N”.
- **200 – awaria w `log.txt`:** dotąd stos wywołań szedł tylko do `crash.log`, a `log.txt` urywał się bez słowa. Teraz przy awarii do `log.txt` trafia blok `==== CRASH ====` (sygnał, adres błędu, GPU, stos wywołań, linie ukryte przez poziom logu, podsumowanie problemów). Przy następnym starcie `crash.log` z poprzedniej awarii jest przepisywany na początek logu (`[crash] the previous start crashed`), raz (`crash.seen`).
- **201 – zapętlone i stojące postacie:**
  - `[thrash]` – postać przy bohaterze (do 40 m), która 10 razy w 5 s zaczyna ten sam stan AI (pętla kończy się od razu, percepcja zaczyna ją znowu): stan, cel, kolejka AI, broń, stan ciała;
  - `[idle]` – postać w walce (`ZS_ATTACK`, `ZS_MM_ATTACK`, do 30 m od bohatera), której cel żyje i jest poza jej zasięgiem, a ona przez 8 s nie rusza się i nie uderza: odległość walki / zasięg, czy widzi cel, kolejka AI, cel drogi, powietrze, tryb chodu – przypadek golemów zostałby pokazany od razu. Raz, do ruchu, ciosu lub zmiany celu.
- **202 – podsumowanie problemów:** każda linia logu jest sprawdzana, a na końcu (wyjście z gry, przejście w tło, awaria) w logu jest blok `==== NB summary ====`: brakujące funkcje pluginu wołane przez skrypty, błędy skryptów, zablokowane postacie (`STUCK`, `[idle]`, `[thrash]`), brakujące pliki (dźwięki, tekstury), inne błędy, ostrzeżenia danych przy wczytywaniu – każdy problem raz, z liczbą wystąpień i czasem pierwszego, najczęstsze na górze.
- **203 – mniej szumu, poziom logu:** nowy wiersz ustawień „Log”: *quiet* (bez śladu AI, `NB trace`, obrażeń i ruchu bohatera; same problemy), *normal* (domyślny), *full* (wszystko jak dotąd). Przy *normal* cios to 1 linia `DMG2` z rodzajami obrażeń, które biorą udział (zamiast 8), `[mem]` co 30 s (było 5 s), `[maps]` co 2 min. Linie ukryte są i tak liczone w podsumowaniu i trafiają do raportu awarii. Wersja ustawień 13.
- **Do sprawdzenia:**
  - przycisk MARK u góry ekranu, dotknięcie – napis i blok `[mark 1]` w logu;
  - wyjście z gry lub przejście do innej aplikacji – na końcu logu `==== NB summary`;
  - Ustawienia → „Log: quiet / normal / full” – log krótszy przy *quiet*;
  - przy stojących postaciach w walce – `[idle]` w logu.
- **Test (24.11, log):** działa – 3 znaczniki `[mark 1–3]` z postaciami w pobliżu, podsumowanie przy wyjściu z gry. Do poprawy (poprawka 204): w „innych błędach” lista funkcji `NB: bound` (zapisana jako błąd) i resztki ścieżek binarnych, archiwa DLL/ZIP Uniona jako „brakujące pliki”, `VDFS tool disk detected` jako błąd skryptu, czas `[mem]` (9223372037106512 s).

### 198. Przywołaniec gryziony przez innego wroga odwraca się do niego – 🧪 (23.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamescript.h`, `common/world/objects/npc.cpp`
- **Zgłoszenie:** golemy stoją i dają się gryźć pustynnemu wężowi; ruszyły dopiero, gdy bohater uderzył węża.
- **Log (197):** celem obu golemów był drugi pustynny wąż, 22 m dalej, na granicy zmysłów (2300), poza zasięgiem wzroku; wąż, który gryzł, nie był ich celem.
- **Przyczyna:** zmianę celu po ciosie robią skrypty (`B_MM_AssessDamage`, `RX_SetReAttackNpc`), ale wroga rozpoznają po `Hlp_GetInstanceID` (`AIV_LASTTARGET`) – dwa węże tej samej instancji są dla nich jednym wrogiem, więc ugryzienia drugiego węża brały za ugryzienia celu i nic nie zmieniały. Cios bohatera dał golemom nowego wroga z perceptu – stąd „naprawili się”.
- **Poprawka:** przywołaniec bohatera trafiony przez wroga bliżej niż 8 m, gdy jego cel nie żyje / leży, jest dalej niż 15 m albo go nie widzi, odwraca się do napastnika (stan walki z nim, jak polecenie „Atak” panelu). Nie w trybie pasywnym panelu, nie na bohatera i nie na innych przywołańców. W logu `[ctx] summon: … turns to … that hits it; its target … is far (N m)`.
- **Nie rozwiązane:** czemu golem w ogóle nie szedł do tego dalekiego celu (kolejka cały czas `[SetWalkMode]`, choć nic jej nie trzyma) – do obserwacji w kolejnych logach.
- **Do sprawdzenia:** golemy przy wężach (kilka tej samej instancji) – gryzione golemy same atakują węża obok, bez ciosu bohatera; w logu `summon: … turns to`.

### 197. Golemy stoją przy wrogu i dają się gryźć – diagnostyka – ✅ (23.11.2026)
- **Plik:** `common/world/objects/npc.cpp`
- **Zgłoszenie (nagranie + log 23.11):** dwa kamienne golemy przy pustynnym wężu stoją obok bohatera, nie atakują, wąż je gryzie (119 obrażeń za ugryzienie); polecenia panelu przywołańców (atak, tryb) nic nie zmieniają.
- **Log:** golemy w `ZS_MM_ATTACK` z celem „Wąż pustynny”, kolejka AI cały czas `[SetWalkMode]` – `STUCK 5 s`; `B_MM_AssessEnemy | !Npc_CanSeeNpc` (golem nie widzi węża). Po zresetowaniu i nowym przywołaniu tuż przy wężu golem uderzył 6 razy (176), potem znów stanął. Ten sam `STUCK` z `SetWalkMode` jest w logach od 22.11 (także przy Im'Arah) – nie po ostatnich poprawkach.
- **Przyczyna (niepotwierdzona):** dopóki kolejka AI nie jest pusta, silnik nie prowadzi walki (`implAttack`), a kolejną akcję bierze tylko, gdy postać nie jest w powietrzu, na nic nie czeka i nie idzie. `ZS_MM_Attack_Loop` dodaje `AI_SetWalkMode(NPC_RUN)` przy każdym przejściu, gdy postać nie biega – jeśli ta akcja nie schodzi, golem nigdy nie zaczyna walki. Z logu nie widać, co ją trzyma.
- **Poprawka:** wpis `STUCK 5 s` mówi teraz, co trzyma kolejkę: `inAir`, stan ruchu, `go2` (cel drogi), pozostałe czasy oczekiwania (`wait`, `ani`, `out`, `fai`), tryb chodu, rzucanie czaru, odległość walki do celu / wymagana i czy golem widzi cel.
- **Do sprawdzenia:** golemy przy wężu (albo innym wrogu, przy którym stoją) – log z wierszem `NB ai: … golem STUCK 5 s: … inAir=… go2=… wait=…`; na tej podstawie właściwa poprawka.
- **Test (23.11, log):** `golem STUCK 5 s: … inAir=0 move=0 go2=0 wait=0 ani=0 out=0 fai=0 walk=0 cast=0 fight=2249/420 see=1` – nic nie blokuje kolejki, ale celem golema jest inny pustynny wąż, 22 m dalej (`[AGGRO] … out of range 2306 > 2300`), a gryzie go wąż stojący obok. Po ciosie bohatera w tego węża golemy zaczęły walczyć. Dalej: poprawka 198.

### 196. `StopPlayerBleeding` – mikstury lecznicze zatrzymują krwawienie – 🧪 (23.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- **Log (23.11):** `not implemented call [STOPPLAYERBLEEDING] from USEITPO_HEALTH_01`.
- **Skrypty NB:** mikstury i esencje lecznicze (`Use_ItPo_Health_*` i inne) wołają `StopPlayerBleeding()` – funkcję pluginu.
- **Plugin (UNION_ABI.DLL `0x1016d610` → `0x1013bdb0`):** z listy obrażeń rozłożonych w czasie usuwa wpisy bohatera typu 4 (`DAM_EDGE` – krwawienie) razem z ich efektem i ustawia `PlayerIsBleeding` = 0.
- **Poprawka:** to samo na liście obrażeń w czasie portu (`RX_ApplyDotDamage`, poprawka 106); w logu `[ctx] bleeding stopped: N`, gdy coś usunięto.
- **Do sprawdzenia:** krwawienie bohatera (np. od przeciwnika z krwawieniem), wypicie esencji leczniczej – krwawienie (ubywanie życia) się kończy, w logu nie ma już `not implemented call [STOPPLAYERBLEEDING]`.
- **Test (23.11, log):** esencja lecznicza wypita – nie ma już `not implemented call [STOPPLAYERBLEEDING]`. Samo zatrzymanie krwawienia jeszcze nie sprawdzone (bohater nie krwawił).

### 195. Launcher: czwarta karta „Gothic II – New Balance” – ✅ (23.11.2026)
- **Pliki:** `android/java/org/opengothic/app/LauncherActivity.java`, nowy `android/launcher-assets/covers/newbalance.webp`
- **Zgłoszenie:** w launcherze dodać czwartą kartę New Balance; tło z obrazka z logo NB, bez liczby 44.
- **Poprawka:**
  - nowa karta „Gothic II / New Balance” obok Gothic, Gothic II i Archolosa, z własnym folderem (`path_NewBalance`) i plikiem `GothicII.ini` – NB nie musi już zajmować karty Gothic II;
  - bez pobierania ze Steam (NB to mod, nie gra na Steam) – karta ma tylko „Wybierz folder” / „Graj”;
  - okładka: obrazek bez czarnych pasów, przycięty tak, że liczby 44 nie ma (bez retuszu), na rozmytym tle w pionie, jak pozostałe okładki.
  - okładka wyższa (640×1000), logo mniejsze i wyżej – na wąskiej karcie mieści się całe i nie wchodzi pod napis „Gothic II / New Balance”.
- **Do sprawdzenia:** launcher – cztery karty w rzędzie; na karcie New Balance „Wybierz folder” → folder `Gothic II NB` → „Graj” uruchamia NB. Karta Gothic II zostaje dla zwykłej Noc Kruka (albo bez folderu).
- **Test (23.11, zrzut):** karta jest i działa („Zainstalowane (GOTOWE)”, „Graj”); pierwsza okładka za szeroka – logo ucięte po bokach („Gothic I”), napis „New Balance” z logo pod tytułem karty. Okładka poprawiona.
- **Test (23.11, drugi zrzut):** okładka poprawna – całe logo nad tytułem karty, karta zaznaczona jako ostatnio grana.

### 194. Ustawienie „Partial focus” – kroki w bok bez krążenia wokół celu – ✅ (23.11.2026)
- **Pliki:** `common/game/playercontrol.cpp`, `common/game/playercontrol.h`, `common/utils/androidinput.cpp`, `common/utils/androidinput.h`, `common/ui/mobilecontrolsmodel.cpp`, `common/ui/mobilecontrolsmodel.h`
- **Zgłoszenie (po 191):** przy krokach w bok focus zaznacza bossa i bohater biega wokół niego, zamiast iść w bok. Na PC (mysz) focus był częściowy: bossa dało się zaznaczyć, gdy bohater stał i rzucał czar albo dobywał broni, a ruch w bok był ruchem w bok.
- **Przyczyna:** w OpenGothic krok w bok (`MoveL`/`MoveR`) z celem w focusie obraca bohatera do celu (`processAutoRotate`), tak samo blokada celu (`lockFace`) zamienia skręt w krok wokół celu – w efekcie krążenie.
- **Poprawka:** nowy wiersz ustawień „Partial focus (free side steps)” (domyślnie OFF – jak dotąd). ON:
  - krok w bok nie obraca bohatera do celu; obraca tylko przycisk akcji (czar, cios) – jak dotąd;
  - blokada celu (także „Auto lock”) trzyma bohatera przodem do celu tylko, gdy stoi; ruch i skręt idą tam, gdzie wskazuje gałka;
  - focus zostaje – cel jest zaznaczony, czary i strzały lecą w niego.
- Wersja ustawień 12 (11 była wycofaną 191; starsze ustawienia dostają OFF).
- **Do sprawdzenia:** Ustawienia → „Partial focus: ON”, walka z Im'Arah z dobytym czarem: kroki w bok idą w bok (bez okrążania), boss dalej zaznaczony; stanie i rzut czaru – bohater obraca się do bossa i trafia. OFF – jak wcześniej.
- **Test (23.11):** działa – kroki w bok idą w bok, boss zostaje zaznaczony, po zatrzymaniu czar leci w bossa.

### 193. Przywołańce przy bossie – diagnostyka w logu – ✅ (23.11.2026)
- **Plik:** `common/game/gamescript.cpp`, `common/game/gamescript.h`
- **Zgłoszenie:** na PC golemy przy Im'Arah znikają (reset), w OpenGothic biją bossa.
- **Skrypty NB:** to nie plugin, a `ZS_MM_Attack_Loop` w `GOTHIC.DAT`: przywołaniec bohatera (poza `SumKhubi`, `Pet_Jina`, `DSnapper`, `DK_Pet`, totemami), który walczy z bossem (`RX_IsBoss`: `aivar[90]` albo `AIVRX_NPC_LP` > 0) lub z postacią nieśmiertelną (nie zwierzęciem), traci całe życie, gdy nie ma bitu `BIT_CAN_AT_BOSS` (`VAR_BIT_MASK`, 16) albo ma go, ale `RX_SummonCount` > `GetSummonCountMax()`. Golemy (`RX_SetGolemStats`) dostają ten bit tylko przy `ATR_INTELLECT` ≥ 300; demony, szkielety, smok, `ShadowBeast_Skeleton_Summon` – zawsze.
- **Przyczyna różnicy:** bohater na PC ma 292 inteligencji – golemy bez bitu, znikają przy bossie. Zapis w OpenGothic ma 363 – golemy z bitem, wg skryptów NB mogą bić bossa. Skrypt jest ten sam w obu wersjach.
- **Poprawka:** bez zmiany zasad (robią to skrypty); raz na przywołańca, gdy jego celem jest boss, wpis `[ctx] summon at a boss: <przywołaniec> -> <boss>, BIT_CAN_AT_BOSS 0/1, summons N/max, ATR_INTELLECT N` z informacją, czy skrypty go odeślą.
- **Do sprawdzenia:** golemy przy Im'Arah – w logu wpis; przy `BIT_CAN_AT_BOSS 0` albo liczbie ponad limit golem powinien zniknąć. Gdy wpis mówi „the scripts send it away”, a golem zostaje – błąd w porcie, do poprawy.
- **Test (23.11):** golemy przy Im'Arah nie znikają. W logu przy obu `BIT_CAN_AT_BOSS 1, summons 2/4` i `4/4, ATR_INTELLECT 363 - it may fight the boss`: według skryptu NB przy inteligencji 363 golemy mogą bić bossa (na PC bohater ma 292 – bez bitu, dlatego tam znikają). Zostaje jak w skryptach: golemy znikają tylko bez bitu albo ponad limitem przywołańców.

### 192. Ataki specjalne Im'Arah – ✅🧪 (23.11.2026)
- **Plik:** `common/game/gamescript.cpp`, `common/game/gamescript.h`
- **Zgłoszenie:** na PC Im'Arah gra animację, świeci na zielono lub czerwono, wystrzeliwuje wachlarz ognistych pocisków, rzuca pioruny z falą (na nagraniu fala zabiła bohatera); w OpenGothic tego nie ma.
- **Plugin (UNION_ABI.DLL):**
  - wybór (`0x1007fe60`, co sekundę): `AIVRX_SPECIAL_ATTACK_TIMER` (103) +1, `AIVRX_SPECIAL_ATTACK_COOLDOWN` (102) −1; bez cooldownu i trwającego ataku co N-tą sekundę: życie ≤ 50% – 35% piorun (2), 35% pociski czaru (3), 30% wachlarz (1), N 8–19; życie ≤ 75% – 60% piorun, 40% wachlarz, N 10–20; inaczej wachlarz, N 10–20; pierwsze N 15–30;
  - obrażenia (`0x1006f4f0`): `AIVRX_SPECIAL_ATTACK_DAMAGE_01–03` (Im'Arah 600/500/500), przy `RX_MonsterPower` ≥ 150 razy moc/100 razy 0,75 (Nightmare: 900/750/750);
  - atak 1 (`0x1006f630`): `S_TESTSHOCKWAVE`, na niej `SPELLFX_IMARAH_GREEN` albo `_RED` (pół na pół; czerwona obraca się do wroga od 10% do 55% animacji); w 55% wachlarz `SPELLFX_IMARAH_FIREBOLT` od −90° do +94° co 8° (24 pociski, ogień); po 3,5 s koniec, cooldown 10 s;
  - atak 2 (`0x10070050`): 1–3 pioruny co 2,5 s; każdy: obrót, `S_LIGHTINGATTACK`, w 70% animacji `SPELLFX_LIGHTNINGFLASH_HEAVENSRAGE` i `SPELLFX_SPECIALATTACK02_CLOUD` w miejscu wroga, po 2 s (wróg dalej niż 20 m: 1,3–2 s) `SPELLFX_SPECIALATTACK02_ELECTROWAVE` z obrażeniami magii; Im'Arah bliżej niż 7 m od miejsca – sama dostaje obrażenia minus ochrona przed magią (zostaje co najmniej 1 życia), dźwięk `IMARAHCURSE`, `T_PALELITE`, chmura na niej, koniec ataku; koniec: cooldown 3 s, 15% (bez klątwy) od razu atak 3;
  - atak 3 (`0x10070860`): obrót, `S_RADIUSATTACK`, w 30% pięć pocisków jej czaru (`SPELLFX_<czar>`, bez czaru `SPELLFX_IMARAH_FIREBOLT`) – w wroga i krok oraz dwa kroki w bok z obu stron (600 cm, przy wrogu dalej niż 15 m mniej), ogień; koniec, cooldown 5 s.
- **Przyczyna:** port miał z tego tylko teleport (atak 11, poprawki 189–190).
- **Poprawka:** wybór ataku co sekundę i trzy ataki jak wyżej (zmienne `AIVRX_SPECIAL_ATTACK*` jak w pluginie, w czasie ataku bez teleportu). Różnice: fala pioruna rani wroga, gdy stoi bliżej niż 7 m od miejsca błysku (w pluginie trafia cel efektu – tu można uciec); wybór nie w czasie leczenia (`ImarahHeals`) ani teleportu; pociski lecą prosto do 32,5 m (jak pociski potworów, poprawka 157) i trafiają też inne postacie po drodze.
- **Do sprawdzenia:** walka z Im'Arah – w logu `[ctx] imarah: special attack N`; wachlarz 24 ognistych pocisków po zielonym/czerwonym świetle; przy mniej niż 75% życia pioruny: błysk i chmura nad bohaterem, po ~2 s fala – ucieczka na bok chroni; przy mniej niż 50% także pięć pocisków jej czaru.
- **Test (23.11):** wachlarz działa – w logu 5 razy `special attack 1`, 24 ogniste pociski po 900 obrażeń, zielona i czerwona wersja; pierwszy wachlarz zabił bohatera. Życie Im'Arah nie spadło poniżej 75% (najniżej 30830/38250), więc pioruny (2) i pociski jej czaru (3) jeszcze się nie pojawiły – do sprawdzenia. Dwa razy `animation 0 ms` – `S_TESTSHOCKWAVE` się nie włączyła (Im'Arah była w innej animacji), pociski poleciały od razu.

### 191. Ustawienie „Fight focus” – walka bez celowania w przeciwników – ⏸ (23.11.2026)
- **Pliki:** `common/world/worldobjects.cpp`, `common/world/worldobjects.h`, `common/utils/androidinput.cpp`, `common/utils/androidinput.h`, `common/ui/mobilecontrolsmodel.cpp`, `common/ui/mobilecontrolsmodel.h`
- **Zgłoszenie:** w walce z Im'Arah focus cały czas zaznacza bossa – bohater nie może ominąć jego czarów; propozycja: przełącznik w ustawieniach.
- **Poprawka:** nowy wiersz ustawień „Fight focus (aim at enemies)” (domyślnie ON – jak dotąd). OFF: z bronią lub czarem w ręce żadna żywa postać nie wchodzi w focus – czary i strzały lecą tam, gdzie patrzy bohater, nic go nie obraca do przeciwnika. Bez broni focus jak zwykle (rozmowy, przeszukiwanie, ciała). Wersja ustawień 11 (starsze ustawienia dostają ON).
- **Do sprawdzenia:** Ustawienia → „Fight focus: OFF”, walka z Im'Arah – brak zaznaczenia nad bossem z dobytym czarem, można biegać w bok i omijać czary; schowanie broni – rozmowa z postacią działa. Z powrotem ON – celowanie jak wcześniej.
- **Test (23.11):** wycofane – przy OFF nie da się namierzyć bossa (na PC focus myszką był częściowy i dało się go zaznaczyć i trafić). Kod usunięty (revert); w ustawieniach zostaje sam wpis `fightfocus` bez znaczenia.

### 190. Teleport magów: animacja, efekt, bez spadania – ✅ (23.11.2026)
- **Plik:** `common/game/gamescript.cpp`, `common/game/gamescript.h`
- **Zgłoszenie (po 189):** Im'Arah teleportuje się, ale momentalnie – bez animacji i efektu.
- **Log (23.11):** 8 teleportów (`blink`); po części Im'Arah w stanie ciała 10 (spadanie) i 12 (leży) – lądował wyżej niż ziemia, spadał i nie rzucał czarów.
- **Plugin (atak specjalny 11, `0x1007b230`):** mag gra animację `S_TESTSHOCKWAVE` (bez niej `T_WARN`), efekt `SPELLFX_TELEPORT` w miejscu docelowym i tam, gdzie stoi; przeniesienie między 45% a 65% animacji, obrót do wroga; `AIVRX_SPECIAL_ATTACK` = 11 do końca animacji.
- **Poprawka:**
  - teleport w trakcie animacji: start – animacja, efekty `SPELLFX_TELEPORT_RING` (niebieski krąg) i `SPELLFX_TELEPORT_CAST` w obu miejscach; przeniesienie w 55% animacji; `AIVRX_SPECIAL_ATTACK` = 11 do końca (nie ma wtedy kolejnego teleportu ani zmian faz walki ze skryptu);
  - miejsce: ziemia szukana promieniem od góry nad magiem i wrogiem, na wysokości maga albo wroga (±3 m), z miejscem na ciało nad ziemią.
- **Log:** `[ctx] blink: Im'Arah teleports … animation N ms` (−1 – bez animacji, 0,9 s).
- **Do sprawdzenia:** przed teleportem gest Im'Arah i niebieskie kręgi w obu miejscach, przeniesienie po chwili; po teleporcie od razu walczy (w logu `NB ai: Im'Arah` bez `body=10`/`body=12`).
- **Test (23.11):** działa – teleport z animacją i efektem.

### 189. Teleport magów do wroga (Im'Arah i inni) – ✅ (23.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamescript.h`
- **Zgłoszenie:** nagrania z PC – Im'Arah z drzwi wieży teleportuje się (niebieski krąg) do bohatera i walczy dalej w terenie; w porcie stał w drzwiach.
- **Plugin (UNION_ABI.DLL):** co 500 ms (`0x102542d0`) dla magów z listy (`0x10253fc0`): Im'Arah (nie gdy `ImarahHeals`), `Skeleton_archemageDungeon`, `Madness_Conjurer`, `KDF_511_Daron`, `Rigellion`, `dementor_ud`, `DMT_Dementor_ScoutCav_01`, `PAL_6104_RITTER_mage`, `AD_Boss6`, `AL_Boss7`, `SAURON_INT`. Warunki: `ZS_Attack`/`ZS_MM_Attack`, broń w ręku, wróg żyje, do 30 m, brak ataku specjalnego (`AIVRX_SPECIAL_ATTACK`). Licznik: wróg widoczny – 8 sprawdzeń (4 s), niewidoczny – 4 (2 s); teleport, gdy mag biegnie (animacje `S_MAGRUN`, `S_1HRUN`, uniki). Miejsce (`0x10253b40`): od maga 500–1500 (wróg dalej niż 15 m) lub 700–1200 cm w stronę wroga, przy wrogu bliżej niż 7 m – 500–1000 (30%: 1000–1500) wzdłuż jego spojrzenia, do 15 prób; miejsce na ziemi, widoczne dla wroga, do 35 m od niego. Sam teleport to atak specjalny 11 (`0x1007b230`): efekt `SPELLFX_TELEPORT` w starym i nowym miejscu, obrót do wroga. Wróg daleko bez łuku i czaru – następny teleport później.
- **Poprawka:** to samo w porcie; bieg = stan ciała `BS_RUN`/`BS_WALK`. Bez animacji ostrzeżenia `T_WARN` przed teleportem (od razu przeniesienie).
- **Log:** `[ctx] blink: Im'Arah teleports N cm (to M cm from the hero)`.
- **Do sprawdzenia:** walka z Im'Arah – oddal się, żeby za tobą pobiegł: niebieski efekt i Im'Arah obok bohatera. Linie `blink` w logu.
- **Test (23.11):** teleportuje się do bohatera (8 razy w walce), ale momentalnie, bez animacji i efektu – poprawka 190.
- **Test (23.11, z 190):** działa.

### 188. Bossowie nie zataczają się od ciosów (`bodystate_interruptable_override`) – 🧪 (23.11.2026)
- **Plik:** `common/world/objects/npc.cpp`
- **Log (23.11, poprawka 187):** Im'Arah w `ZS_ATTACK` z burzą ognia w ręce co chwilę w stanie ciała 21 (zatoczenie po trafieniu) – każdy czar bohatera (413) przerywał mu rzucanie, przez całą walkę trafił 2 razy.
- **Przyczyna:** postać z `BODYSTATEINTERRUPTABLEOVERRIDE = TRUE` (bossowie NB, Im'Arah w instancji; `RX_ApplyStagger` dla bossów nic nie zmienia) nie powinna dać się przerwać ciosem. Port pomijał tylko przerwanie animacji, ale animacja zatoczenia i tak się włączała.
- **Poprawka:** przy `bodystate_interruptable_override` cios nie włącza zatoczenia. Odrzut (`DAM_FLY`) bez zmian.
- **Do sprawdzenia:** Im'Arah pod ostrzałem czarów bohatera dalej rzuca swoje; w logu `NB ai: Im'Arah` bez `body=21`.
- **Test (23.11):** w logu Im'Arah już bez `body=21`, rzuca czary w trakcie walki. Inni bossowie i potwory „nieprzerywalne” (np. elitarne orki przy słabym bohaterze) – nie zataczają się.

### 187. Ślad AI bossów w logu (Im'Arah nie atakuje) – ✅ (23.11.2026)
- **Plik:** `common/world/objects/npc.cpp`
- **Zgłoszenie:** w porcie Im'Arah stoi w drzwiach wieży i prawie nie rzuca czarów, golemy go biją. Na nagraniu z PC teleportuje się do bohatera i walczy w terenie cały czas czarami.
- **Log (23.11):** przez całą walkę Im'Arah trafił 3 razy (bohater 417, golem 2×260); ślad AI (`NB ai:`) obejmował tylko potwory, a Im'Arah to człowiek – nie było widać jego stanu.
- **Zmiana (diagnostyka):** ślad `NB ai:` także dla bossów (`aivar[90]`), z rodzajem broni, czarem w ręce (`spell=`), stanem ciała (`body=`) i kolejką AI; nowa linia przy każdej zmianie stanu, celu, broni lub kolejki.
- **Do sprawdzenia:** walka z Im'Arah – w logu linie `NB ai: Im'Arah ...`, z nich przyczyna, dlaczego nie rzuca czarów.
- **Test (23.11):** działa – log pokazał przyczynę: zatoczenia od czarów bohatera (poprawka 188) i brak teleportu (189).

### 186. Obrażenia przyzwanego kamiennego golema (bez siły) – 🧪 (23.11.2026)
- **Plik:** `common/game/damagecalculator.cpp`
- **Zgłoszenie:** porównanie ze zrzutami z PC (Winlator, Union): golem zadaje w porcie kilka razy więcej.
- **Skrypty NB (`Summoned_Golem`, `RX_SetGolemStats`):** siła 240 + 1,75×inteligencja, obrażenia obuchowe 200 + int/3, odrzutu 100 + int/4 (`DAM_BLUNT|DAM_FLY`), życie 4000 + 3×int.
- **Porównanie (PC, golem 4876 życia → inteligencja 292: obuchowe 297, odrzutu 173; ochrona celów po 150% z Nightmare, odrzut bez zmian):**

  | Cel | Ochrona obuch. / odrzut | PC | Port przed |
  |---|---|---|---|
  | Strzelec budowniczych | 75 / 50 | 345 | ok. 1285 (int 363) |
  | Pustynny pełzacz wojownik | 675 / 40 | 134 | ok. 895 |
  | Pożeracz rudy | 300 / 300 | 20 | – |

- **Przyczyna:** na PC w ciosie przyzwańca nie ma siły – tylko jego obrażenia minus ochrona, osobno dla każdego rodzaju: 345 = (297−75) + (173−50). Port (poprawka 155) dodawał siłę (751–875) dzieloną między rodzaje.
- **Poprawka:** cios przyzwanego stworzenia bez broni, które ma własne obrażenia (`damage`): suma po rodzajach (obrażenia − ochrona); rodzaj zatrzymany przez ochronę daje 1, cały cios co najmniej 20 (pełzacz 1 + 133 = 134, pożeracz rudy 20). Odporność (ochrona −1) jak dotąd. Przyzwańce bez własnych obrażeń i inne postacie – bez zmian (siła, poprawka 155).
- **Do sprawdzenia:** golem na pustyni Adanosa – liczby nad celami zbliżone do PC (przy innej inteligencji inne: obuchowe 200 + int/3, odrzutu 100 + int/4). Inne przyzwańce (szkielety, demony) jak dotąd.

### 185. Życie, siła i ochrona potworów według poziomu trudności (Nightmare) – 🧪 (23.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamescript.h`
- **Zgłoszenie:** na poziomie Nightmare bossowie mają za mało życia (Im'Arah w wieży na pustyni Adanosa).
- **Skrypty NB:** `RX_LoadDiffSettings` ustawia przy nowej grze: Nightmare – `RX_Nightmare`, `RX_MonsterHpRatio` 150, `RX_MonsterProtRatio` 150, `RX_MonsterPower` 200; poziom „Gothic” – według opcji `bOptionMonsterHealth` (50–150%), `bOptionMonsterPower` (100–200%), `bOptionMonsterProt` (100–150%); przełącznik poziomu w pluginie („Set diff GOTHIC”) daje życie i siłę 100%. `RX_ApplyMonsterStats` mnoży raz życie postaci (Nightmare: 150% – `RX_IsLeg`, także bossowie), ochronę (`RX_UpdateMonsterProt`), siłę, zręczność i obrażenia (`RX_MonsterPower`), dodaje 0–4% życia losowo, bossom +3000 ochrony przed odrzutem; `AIVAR[89]` = zrobione. Gracz i przyzwańce bez zmian.
- **Przyczyna:** `RX_ApplyMonsterStats` woła tylko plugin: co sekundę `RX_AST_UpdateMonsterStats` dla każdej postaci świata (UNION_ABI.DLL 0x1023c1a0, SELF = postać). OpenGothic tego nie robił – wszystkie postacie miały wartości z instancji: Im'Arah 25000 życia zamiast 37500 (+0–4%) na Nightmare.
- **Poprawka:** co sekundę `RX_AST_UpdateMonsterStats` dla żywych postaci świata bez `AIVAR[89]` (bez bohatera). Postacie, które już są w zapisie, dostają wartości poziomu trudności przy pierwszym wczytaniu (raz – `AIVAR[89]` jest w zapisie). Jak w oryginale razem z życiem rośnie siła i ochrona, więc na Nightmare potwory biją mocniej niż dotąd.
- **Log:** `[ctx] monster stats: N npcs (hp ratio 150, power 200, prot 150, nightmare 1)`.
- **Do sprawdzenia:** zapis z Nightmare przed Im'Arah – po wczytaniu w logu linia `monster stats`, pasek życia bossa schodzi wolniej (ok. 37500–39000 życia). Zwykłe potwory też silniejsze. Na poziomie „Gothic” wartości zapisane w grze przy jej starcie (preset pluginu „Gothic”: życie i siła 100% – Im'Arah ok. 25000–26000); liczby są w linii `monster stats` w logu.

### 184. Paczka 7 brakujących funkcji (log z pustyni Adanosa) – 🧪 (23.11.2026)
- **Plik:** `common/game/gamescript.cpp`
- **Zgłoszenie:** log z pustyni Adanosa (walka z Im'Arah): `NB missing (save): 1 functions, 139 calls: RX_AST_BLESSING`, `not implemented call [RX_SELECTSPELL] from RX_CHANGESPELL (IMARAH, 38)`.
- **Poprawki:**
  - `RX_AST_Blessing(rodzaj)` – błogosławieństwa Isgarotha (`RX_IsGarothRegen`, `…RegenSta`, `…Mana`), wołane co sekundę z `RX_RegenCommonFunc`. Jak w pluginie, ułamki się sumują: 0 – życie: max(maks. życie, 400) / 360 na sekundę (/ 800, gdy ustawione `XARDAS_KNOWSSACTANOME`); 1 – wytrzymałość (tylko z `RX_OldStamina`): max(`ATR_STAMINA_MAX`×10, 360) / 360; 2 – mana: max(maks. mana, 133) / 400. Wszystko do wartości maksymalnej. Wcześniej błogosławieństwo było widać w panelu, ale nic nie regenerowało.
  - `RX_SelectSpell(postać, czar)` – plugin wkłada runę do księgi czarów postaci i wybiera ją. W OpenGothic runę wybiera `AI_ReadySpell` zaraz po niej w `RX_ChangeSpell` (runę daje wcześniej `RX_CreateSpellRune`, poprawka 149), więc funkcja tylko sprawdza, czy runa jest (w logu `select spell: … has no rune`, gdy nie ma).
- **Nie w tej paczce:** ostrzeżenia `C_NPC.AIVAR without an instance set` (`RX_IsCrabSmall`, `ZS_MM_Attack_Loop`, `C_PlayerIsFakeBandit`, `RX_IsSummonNpc`) – skrypty czytają zmienną z pustej postaci i dostają 0.
- **Do sprawdzenia:** z błogosławieństwem Isgarotha życie / wytrzymałość / mana rosną same; w logu po zapisie bez `RX_AST_BLESSING` w `NB missing`. Im'Arah zmienia czary w fazach walki (burza ognia, mroczna kula, kula śmierci, mroczna włócznia).

### 183. Muzyka NB w lokacjach (BassMusicController, mp3) – ✅🧪 (23.11.2026)
- **Pliki:** `common/gamemusic.cpp`, `common/gamemusic.h`, `common/game/definitions/musicdefinitions.cpp`, `common/game/definitions/musicdefinitions.h`, `CMakeLists.txt`, 🆕 `common/utils/drmp3.cpp`, 🆕 `lib/dr_libs/dr_mp3.h` (+ `LICENSE`)
- **Zgłoszenie:** w niektórych lokacjach nie ma muzyki (log z doliny Adanosa: `unable to load sound: "DarkSagaPirateCamp.sgt"`).
- **Skrypty NB:** plugin BassMusicController czyta `System/Autorun/BASSMUSICCONTROLLER_NB.D` i `_MI.D` (w `AB_Scripts.vdf`): 137 motywów `instance NAZWA(C_MUSICTHEME_…) { file = "a.mp3|b.mp3"; vol = 0.3; }`, 81 plików mp3. Motyw o tej samej nazwie co w `Music.dat` jest grany z mp3 zamiast DirectMusic; 44 motywów nie ma w `Music.dat` wcale (np. `THF_DAY_FGT`, `PIR_NGT_STD`).
- **Przyczyna:** OpenGothic znał tylko motywy z `Music.dat`. `Music.dat` NB wskazuje w części stref pliki `.sgt`, których nie ma w grze (`DSP_DAY_STD` → `DarkSagaPirateCamp.sgt`), więc tam była cisza; motywy walki/zagrożenia dodane przez plugin nie grały wcale. Dekodera mp3 w projekcie nie było.
- **Poprawka:**
  - `MusicDefinitions` czyta pliki `BASSMUSICCONTROLLER*.D` z `/SYSTEM/AUTORUN` (VDFS): komentarze pominięte, `file` dzielone po `|`, nazwa bez rozszerzenia dostaje `.MP3`, `vol` jako głośność motywu. Pliki szukane po nazwie w VDFS (jak plugin przez zFILE). Motyw bez żadnego znalezionego mp3 zostaje z `Music.dat`; brakujące pliki – jedna linia w logu (`NB music: N mp3 files not found`).
  - `GameMusic`: motyw BassMusicController grany przez dekoder dr_mp3 – pliki motywu po kolei, w losowej kolejności przy każdym starcie motywu, pojedynczy plik w pętli; inne próbkowanie niż 44100 Hz przeliczane liniowo, mono na oba kanały. Pliki czytane w osobnym wątku (do wczytania pierwszego – cisza). Zmiana motywu: stary wycisza się przez 2 s, nowy wchodzi przez 2 s; ta sama lista plików w kolejnej strefie gra dalej bez przerwy. Głośność muzyki z opcji i wyciszenia skryptów (116) / wczytywania (174) działają jak dla innych motywów.
  - W logu: `NB music: N BassMusicController themes, …` przy starcie i `NB music: <pliki>` przy zmianie motywu.
- **Do sprawdzenia:** dolina Adanosa (płaskowyż) – gra muzyka z mp3; walka w strefie z motywem walki pluginu; przejście między strefami – płynne wyciszenie; muzyka wyłączona w opcjach – cisza. W `log.txt` liczba motywów i brak linii `mp3 files not found` (jeśli jest – podać, którego pliku brakuje).
- **Test (23.11):** działa – na pustyni Adanosa gra muzyka (w logu `NB music: 143 BassMusicController themes, 83 playlists`). Do dalszego sprawdzania w innych lokacjach i w walce.

### 182. Ognik na czas przemiany bohatera – ✅ (23.11.2026)
- **Pliki:** `mods/nb-wisp/index.js`, `mods/nb-wisp/manifest.json`, `mods/nb-wisp.zip` (wersja 1.3.2)
- **Zgłoszenie:** w czasie przemiany ognik ma być wyłączony, a po przemianie sam wracać.
- **Poprawka (mod):**
  - Przemiana rozpoznawana po instancji bohatera: w przemianie silnik daje mu instancję stworzenia (`BLOODFLY_RUNE`…), bohater NB to `PC_HERO`. Sprawdzane co 0,5 s.
  - Przywołany ognik przy przemianie znika (`ognik: odesłany na czas przemiany` w konsoli moda), po powrocie do człowieka pojawia się obok bohatera (`ognik: wrócił po przemianie`). Ognik odesłany przez gracza nie wraca.
  - W czasie przemiany ognik nie jest przywoływany ponownie (pilnowanie co 3 s, nowy świat). Przywołanie w przemianie (zakładka, Shift+F5, `ognik on`) – „Ognik przyjdzie po przemianie”; odesłanie w tym czasie – nie wraca. Zakładka „Ognik” pokazuje „wróci po przemianie”.
- **Do sprawdzenia:** z przywołanym ognikiem przemiana w krwiopijcę – ognik znika; powrót do człowieka – ognik obok, zbiera dalej. Bez ognika przemiana i powrót – ognik się nie pojawia.
- **Test (23.11):** działa – ognik znika na czas przemiany i wraca po niej.

### 181. Przemiany: stan przemiany prowadzony przez plugin (krwiopijca) – ✅ (23.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`
- **Zgłoszenie:** przemiana w krwiopijcę (runa od Nedtbeka) nie działa.
- **Skrypty NB:** runa `ItRu_TrfBloodFly` używa czaru `SPL_TRFSCAVENGER`; `Spell_Logic_TrfScavenger` przemienia (`Npc_SetActiveSpellInfo(self, BloodFly_Rune)`) tylko przy `!HeroTransOpen`, a przy rzucie ustawia `HeroTransOpen = TRUE`, `RX_BloodFlyTransCheck`, `RX_BloodFlyTimes += 1`. Tak samo inne przemiany. Żaden skrypt nie zeruje `HeroTransOpen` – robi to plugin. Czas zwoju liczy `B_GetTime` (`HeroTRANSCOUNT` do `TempTransTime`, potem ostrzeżenie i utrata życia) tylko przy `HEROTRANS == TRUE`.
- **Plugin NB** (`UNION_ABI.DLL` 0x10223340, co klatkę):
  - bohater stał się stworzeniem (gildia > 21): `HEROTRANS = 1`, `HeroTransOpen = 0`, `HeroTRANSCOUNT`, `TransformWarn`, `RX_BloodFlyTransCheck`, `RX_MagicShieldActive` i tarcze (łowca demonów, wampir) 0, `HeroTransScroll = 0` przy runie, `TempTransTime` = `ATR_INTELLECT` + 2 × poziom + 120;
  - bohater znów człowiekiem: `HEROTRANS`, `HeroTransOpen`, liczniki, `TempTransTime`, `HEROTRANSRUNE`, `HeroTransScroll`, `POISONED`, `RX_HardPoisoned`, `PlayerIsBleeding` 0, `RX_ResetAnimsOnLoad`, sprint sprzed przemiany (`RX_SaveSprintTime` → `RX_ApplySprint`);
  - `HeroTransOpen` przez 3 s bez przemiany (czar nie zadziałał): wszystko 0, krwiopijca dnia nie liczony (`RX_BloodFlyTimes - 1`).
- **Przyczyna:** tej części pluginu nie było. Po pierwszej przemianie `HeroTransOpen` zostawał ustawiony i każdy następny czar przemiany od razu się przerywał; zapis z ustawionym `HeroTransOpen` nie pozwalał na żadną przemianę. Czas zwoju nie był liczony (`HEROTRANS` 0).
- **Poprawka:** `GameScript::nbTransformTick` (co klatkę, według `Npc::isTransformed`) robi to co plugin. W logu `[ctx] transformation: on (scroll …, N s)` / `off` / `HeroTransOpen without a transformation for 3 s - cleared`.
- **Do sprawdzenia:** runa krwiopijcy – przemiana, powrót, druga przemiana zaraz potem; zwój przemiany – licznik „(mm:ss)” na liście efektów i powrót po czasie; zapis, w którym przemiana nie działała, po wczytaniu (po 3 s przemiana znów możliwa).
- **Test (23.11):** działa – przemiana w krwiopijcę.

### 180. Okno zapisów modów Spine (eksport / import) – 🧪 (23.11.2026)
- **Pliki:** `android/java/org/opengothic/app/SaveExport.java`, `SaveImport.java`, `SpineSavesActivity.java` (z projektu borsuczyny, usunięte w 179), `android/AndroidManifest.xml`, `android/java/org/opengothic/app/SpineActivity.java`
- **Zgłoszenie:** przy scaleniu 179 nie weszło okno zapisów Spine – ma być.
- **Przyczyna pominięcia w 179:** okno korzysta z `SaveExport` i `SaveImport`, usuniętych 15.11 razem z eksportem zapisów w launcherze. Te klasy szukają zapisów tylko w plikach aplikacji, a w tym launcherze silnik pracuje w folderze gry wybranym przez gracza (`GameRoots.linked`, pierwsza linia `game.txt`) i tam zapisuje (`<ini>_save_slot_N.sav` obok plików gry).
- **Poprawka:**
  - Przycisk „Zapisy” na liście modów Spine i okno zapisów (lista pobranych modów, dla każdego liczba zapisów, eksport do ZIP, import z ZIP: zastąp / dodaj osobno) – jak u borsuczyny.
  - Eksport i liczenie zapisów biorą też folder gry wybrany w launcherze (tylko sam folder, bez podfolderów gry); w archiwum jako `saves/folder/Gothic2/…`.
  - Import: `saves/folder/<gra>/…` trafia do folderu tej gry (wybranego albo w plikach aplikacji). Zapisy z archiwów launchera borsuczyny (`saves/external/Gothic2/…`) trafiają do wybranego folderu gry, gdy jest wybrany – tam je wczyta silnik.
- **Kompilacja:** pliki Javy sprawdzone parserem `javac`; odwołania do klas launchera (`GameRoots`, `SpineActions`) się zgadzają. Pełna kompilacja (klasy Androida) tylko przy budowie APK.
- **Do sprawdzenia:** Spine → „Zapisy”: przy modzie z zapisami (np. Archolos) liczba zgodna z zapisami w folderze gry; eksport do ZIP i import z niego (zastąp / dodaj osobno) – zapisy widoczne w grze w menu wczytywania.

### 179. Scalenie z aktualizacją borsuczyny (`touch-ui`, 23.11.2026) – 🧪 (23.11.2026)
- **Pliki:** 74 pliki z `borsuczyna/OpenGothic-Android` (29 commitów do `eb943e19`); rozwiązane konflikty: `common/game/inventory.cpp`, `common/graphics/effect.cpp/.h`, `common/graphics/mdlvisual.cpp/.h`, `common/graphics/pfx/pfxbucket.cpp`, `common/world/bullet.cpp`, `common/world/objects/pfxemitter.cpp`, `common/world/objects/npc.cpp/.h`, `common/mainwindow.cpp`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp/.h`, `android/java/org/opengothic/app/LauncherActivity.java`, `patches/zenkit-opengothic.patch`
- **Zgłoszenie:** nowa wersja u borsuczyny – dociągnąć zmiany.
- **Co przyszło (najważniejsze):**
  - deszcz w światach zewnętrznych (krople, dźwięk, pogoda na dzień, ciemniejsze niebo) i ustawienie „Rain” (wył. / lekki / zwykły / ulewa);
  - magiczne i ogniste strzały i bełty lecą ze swoim efektem; efekty broni z siatki całej broni;
  - ważna rozmowa postaci przed wyborem w dialogu (postać nie zaczyna w kółko tej samej rozmowy);
  - przeszukiwanie ciał pokazuje broń noszoną na pasie i plecach;
  - NPC przesunięci z punktu trasy nie lądują na meblach ani w filarach (Hilda, Orik), bohater nie wpada w szczelinę na schodach do Garonda, przeskakuje niskie przeszkody z prześwitami (płoty);
  - przyjaciele nie dołączają do walk bohatera przez nieaktualny powód ataku; ciała oznaczone przez skrypty (piraci Angus i Hank) zostają przy resecie rutyn;
  - animacja natychmiastowych czarów, lodowa lanca, Trąba powietrzna leci od rzucającego do celu;
  - Mali: woda bez teselacji, ustawienie odbić wody („Water reflections”); gamepad – styl lewej gałki („Gamepad movement”);
  - paski życia i many w HUD dotykowym chowają się razem z przyciskami; dokument czytany z ekwipunku zamyka ekwipunek;
  - raport braku pamięci i powodu zamknięcia gry (`lifecycle.txt`); poprawki wczytywania (Velaya, `oCAIArrow` tylko w zapisach) i `CreateInvItems` (handel Samuela).
- **Rozwiązanie konfliktów:**
  - **Efekty przedmiotów (150):** borsuczyna zrobił to samo dla broni (efekt za mieczem i łukiem). Zostaje wersja z poprawki 150 (miecz, łuk/kusza, tarcza, przedmioty w świecie, chowanie z zasięgiem rysowania 146). Od borsuczyny wzięte: wczytywanie siatek emiterów z plików `MRM`/`MMB` (bez tego broń nie dawała siatki emitera i cząsteczki nie szły z ostrza – dotyczy też 150), efekty lecących strzał i bełtów, `PfxEmitter::setMeshEmitter` i rozpoznawanie siatki statycznej w `pfxbucket.cpp`.
  - **Ustawienia dotyku:** obie listy razem – nasze „Peaceful targets” i „Acrobatics” (ukryta, 108) oraz „Water reflections”, „Gamepad movement” i „Rain”. Obie wersje używały numerów wersji ustawień 8–10 do czego innego; w tym repo wersja 10 dokłada odbicia, gałkę i deszcz z domyślnymi wartościami (zapisane ustawienia peaceful/acrobatic zostają).
  - **Numery przycisków:** 19 wierszy ustawień od numeru 11 wchodziło na koło szybkiego wyboru (27), szybki atak (28) i sprint (29) – dotknięcie szybkiego ataku przełączałoby ustawienie. Wiersze ustawień mają teraz numery od 64 (`Special::FirstSetting`, sprawdzane `static_assert`).
  - **Launcher:** eksport/import zapisów modów Spine (`SaveExport`, `SpineSavesActivity`) nie wchodzi – ten launcher nie ma eksportu i importu zapisów (usunięte 15.11). Wymiary banera zostają nasze. Okno zapisów Spine dodane w 180, dopasowane do folderu gry.
  - **ZenKit:** łatka złożona z naszej i borsuczyny (oCAIArrow, Velaya).
  - Podwójna deklaracja `editingHud()` w `androidinput.h` po scaleniu usunięta.
- **Kompilacja:** wszystkie pliki silnika sprawdzone clangiem (`-fsyntax-only -Werror -Wall -Wimplicit-int-float-conversion`, z `OPENGOTHIC_TOUCH_UI`). Niesprawdzone: shadery i Java (brak narzędzi Androida w tym środowisku).
- **Do sprawdzenia:** budowa APK (Java, shadery); ustawienia dotyku – wszystkie wiersze przełączają właściwe ustawienie, szybki atak, sprint i koło działają; deszcz i ustawienie „Rain”; ogniste/magiczne strzały w locie; ogniste bronie (150); ognik i przeszukiwanie ciał (177) przy broni na pasie.

### 178. Mod `nb-wisp`: punkty nauki do testów – ✅ wycofane (22.11.2026)
- **Pliki:** `mods/nb-wisp/index.js`, `mods/nb-wisp/manifest.json`, `mods/nb-wisp.zip`, `common/scripting/game/game_npc.cpp`, `docs/MODS.md`
- **Zgłoszenie:** do testu nauki u nauczycieli (akrobatyka) potrzebne są punkty nauki; prośba o pole z punktami nauki w modzie ognika.
- **Poprawka:**
  - API modów: postać ma `learningPoints` (punkty nauki, `C_Npc.lp`) i `setLearningPoints(n)` (0–1 000 000).
  - Mod `nb-wisp` 1.3.0: zakładka „Punkty nauki” w menu modów – bieżąca liczba, przyciski +5, +10, +50, +100, pole z liczbą i przyciski „Dodaj” / „Ustaw”. Komenda konsoli `pn` (bez liczby – pokazuje, `pn 50` – ustawia, `pn +50` / `pn -10` – dodaje / odejmuje). Na starszej wersji gry zakładka mówi, że brak tej funkcji.
  - `mods/nb-wisp.zip` był w wersji 1.2.1, bez zmiany z poprawki 154 (ognik usuwany i przywoływany dopiero przy 100 m, nie przy 40 m) – paczka zbudowana od nowa z folderu moda.
- **Do sprawdzenia:** instalacja `nb-wisp.zip` (launcher → Mods → Add a mod), zakładka „Punkty nauki”: przyciski i pole zmieniają liczbę, liczba w statystykach postaci taka sama; nauka u nauczyciela zużywa te punkty. Ognik działa jak dotąd.
- **Test (22.11):** działa – punkty nauki posłużyły do nauki akrobatyki (test 86).
- **Wycofane (22.11):** zakładka „Punkty nauki”, komenda `pn` i funkcje `learningPoints` / `setLearningPoints` usunięte (były tylko do testu). Zostaje paczka `mods/nb-wisp.zip` zbudowana od nowa z folderu moda (wersja 1.3.1, z poprawką 154).
- **Test (23.11):** po instalacji `nb-wisp.zip` 1.3.1 zakładki „Punkty nauki” nie ma.

### 177. Ognik: łup z ciała po zdjęciu broni (szkielet pirat) – ✅ (22.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie:** ognik zbiera większość łupów, ale ciała szkieleta pirata nie przeszukał do końca – bohater znalazł w nim jeszcze przedmiot.
- **Log:** ognik doleciał do obu szkieletów zaraz po ich śmierci (`the wisp brings: Kość szkieletu x2 (from the body of Szkielet pirat)`); kordelas drugiego leżał na ziemi (broń w ręku spada przy śmierci). Ciało pierwszego nie zniknęło jako puste i bohater otworzył je w ekwipunku.
- **Skrypty NB:** `ZS_Dead` daje łup śmierci (`B_GiveDeathInv`) i woła `AI_UnequipWeapons(self)`, które wykonuje się później, z kolejki AI. Broń na pasie (szkielet nie walczył bronią w ręku) jest do tego czasu założona, więc nie należy do łupu (przy przeszukaniu założone przedmioty się pomija).
- **Przyczyna:** ognik zabierał łup w chwili śmierci, zanim broń zeszła z pasa, a ciało po przeszukaniu było pomijane przez 10 min. Broń, która potem pojawiła się w łupie (najpewniej kordelas pierwszego szkieletu), została w ciele.
- **Poprawka:**
  - Ognik leci do ciała dopiero 1 s po tym, jak zobaczy je martwe z łupem (`ZS_Dead` i jego kolejka AI zdążą się wykonać).
  - Po przeszukaniu ciało jest pomijane 3 s zamiast 10 min. Gdy później coś w nim przybędzie (broń z pasa, przedmiot ze skryptu), ognik wraca i to zabiera.
- **Do sprawdzenia:** szkielety piratów i inni przeciwnicy z bronią na pasie – ognik przynosi kości i broń, ciało znika jako puste. W logu dwa razy `the wisp found the body: …` dla jednego ciała jest możliwe (drugi raz po broń).
- **Test (22.11):** działa – ognik zabiera cały łup z ciał.

### 176. Panel przywołańców: PRZYWOŁAJ (przywołańce do bohatera), RESET nad AKTYWNE – ✅ (22.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/game/nbtexts.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp/.h`, `common/mainwindow.cpp`
- **Zgłoszenie:** przywołańce czasem gubią się w świecie. Na PC był klawisz, który stawiał je przy bohaterze, z odnowieniem 30 s. Na telefonie go brak.
- **Plugin NB:** klawisz `KEY_SUMMON_TELEPORT` (`UNION_ABI.DLL` 0x10235bae): gdy licznik odnowienia jest zerowy, wszystkie przywołańce bohatera (gdziekolwiek w świecie) stają 2–5 m od niego, a ich AI wraca do podążania (0x10138ac0, `ResetAI() -> FOLLOW`). Odnowienie 30 s, a 15 s, gdy `RX_NecroSummonWay == 179935`; liczy się w czasie gry. Na liście efektów w prawym górnym rogu plugin pokazuje „(mm:ss) `RX_SummonTeleportCD_String`” (0x10256c48).
- **Poprawka:**
  - Panel przywołańców ma dwa rzędy: przy przycisku ATAK i AKTYWNE/PASYWNE, nad nimi PRZYWOŁAJ (nad ATAK) i RESET (nad AKTYWNE).
  - PRZYWOŁAJ (`GameScript::nbSummonsCall`): przywołańce i zwierzaki (bez ognika i totemu) stają w półkolu 2,5 m za bohaterem, ich kolejka AI jest czyszczona, cel zdejmowany, startuje ich stan początkowy (podążanie). Potem odnowienie 30 s / 15 s czasu gry.
  - W czasie odnowienia przycisk jest przygaszony i pokazuje pozostałe sekundy, a dotknięcie wypisuje „Przyzwańce: ponownie za N s”. Na liście efektów linia „(mm:ss) Przywołanie przyzwańców” (tekst z `AB_Translation.lbr`, gdy jest).
  - W logu `[ctx] summons: N called to the hero, again in 30 s`.
- **Do sprawdzenia:** przywołaniec zostawiony daleko (np. za rzeką) → PRZYWOŁAJ: staje przy bohaterze i idzie za nim; drugie dotknięcie od razu – licznik na przycisku i na liście efektów; po 30 s znów działa. RESET, ATAK i AKTYWNE bez zmian.
- **Test (22.11):** działa – PRZYWOŁAJ, RESET, ATAK i AKTYWNE w nowym układzie.

### 175. Ognik: bez kolizji, lot bez spadania, zbieranie przedmiotów i łupów z ciał – ✅ (22.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/world/objects/npc.cpp/.h`, `common/physics/dynamicworld.cpp`
- **Zgłoszenie:** ognik czasem nie zbiera przedmiotów ani łupów z zabitych; od czasu do czasu blokuje bohaterowi drogę (nie powinien mieć kolizji).
- **Przyczyna (logi z poprzednich dni):**
  - Lot ognika (do przedmiotu, do ciała, z powrotem do bohatera) przesuwał go co klatkę, ale ruch postaci dalej liczył grawitację. Nad krawędzią albo zboczem ognik był „w powietrzu”, prędkość spadania rosła co klatkę i po około sekundzie była większa od lotu. W logach: `the wisp did not reach the item: closest 1 cm (height 3649 cm)` – ognik nad przedmiotem, ale 36–287 m niżej (spadł przez świat). Przedmiot był pomijany przez 60 s, ciało zostawało z łupem (w logach 7 ciał znalezionych i nieograbionych).
  - Listy pominiętych przedmiotów i ciał trzymały adres obiektu. Nowa postać (nowy potwór, inny świat) pod adresem ograbionego wcześniej ciała była pomijana przez 10 min.
  - Ognik miał zwykłą kolizję postaci.
- **Poprawka:**
  - Lot ognika bez grawitacji: prędkość spadania zerowana w każdej klatce lotu, także przy przeniesieniu ognika do bohatera.
  - Listy pominiętych po obiekcie (`weak_ptr`), nie po adresie.
  - Ognik bez kolizji z postaciami (`Npc::nbSetNoCollision`): bohater i inni przechodzą przez niego, on przechodzi przez nich, strzały i czary przez niego przelatują. Postać bez kolizji (także martwa) nie zatrzymuje się na innych postaciach.
  - W logu `[ctx] the wisp did not reach the body: …`, gdy ognik nie doleci do ciała w 15 s.
- **Do sprawdzenia:** ognik w wąskim przejściu i w drzwiach – bohater przechodzi przez niego; przedmioty na zboczach, pod krawędzią i na niższym poziomie – ognik je przynosi; łupy z zabitych potworów (także po zmianie świata). W logu nie powinno być `did not reach … (height` z dużą wysokością.
- **Test (22.11):** działa – ognik bez kolizji, zbiera przedmioty i łupy z ciał.

### 174. Dźwięk przy wczytywaniu gry – ✅ (22.11.2026)
- **Pliki:** `common/gamemusic.cpp/.h`, `common/mainwindow.cpp`
- **Zgłoszenie:** przy wczytywaniu (np. po śmierci) dźwięk się zacina.
- **Przyczyna:** w czasie wczytywania wątek wczytywania zajmuje procesor; strumień muzyki i zapętlone dźwięki starego świata (walka, śmierć) nie dostają danych na czas i powtarzają kawałki. Poprawki 36–38 przeniosły wczytywanie muzyki do osobnego wątku, ale strumień grał dalej przez całe wczytywanie.
- **Poprawka:** wczytanie gry albo przejście do innego świata (nie zapis) jest ciche: muzyka wyciszona od początku wczytywania do wejścia do nowego świata (`GameMusic::setLoadMute`), dźwięki starego świata zatrzymane (`WorldSound::stopAllSounds`). Muzyka wraca z nowym światem.
- **Do sprawdzenia:** śmierć → wczytanie zapisu, wczytanie z menu, przejście do innego świata: na ekranie wczytywania cisza, bez zacinania; muzyka gra w nowym świecie; zapis gry nie przerywa muzyki.
- **Test (22.11):** działa – wczytywanie bez zacinania dźwięku.

### 173. Wytrzymałość: pływanie, koniec sprintu bez wytrzymałości – ✅ (22.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/graphics/mesh/animationsolver.cpp`
- **Zgłoszenie:** po wyczerpaniu wytrzymałości bohater raz biega dalej sprintem, a raz nie; podczas pływania wytrzymałość nie spada.
- **Skrypty NB:**
  - `PerFrame` (co drugą klatkę, poza przemianą i śmiercią): przy `RX_SpeedPotion <= 0`, `ATR_STAMINA <= 0` i `SPRINT_ACTIVE` → `Mdl_RemoveOverlayMds(hero, "Humans_MageSprint.MDS")`, `SPRINT_ACTIVE = FALSE`.
  - `Hero_Poisoned`: pływanie zabiera wytrzymałość, gdy nie gra animacja `S_SWIM` (bohater płynie) i jest `BS_DIVE` albo `Npc_GetWaterLevel(hero) == WATER_DIVE` (2); przy zerze zabiera życie.
- **Przyczyna:**
  - `Npc_GetWaterLevel` zwracało 1 przy pływaniu (2 tylko pod wodą), więc skrypt nie widział głębokiej wody.
  - Sprint: bez logu przyczyna niepewna. `Mdl_RemoveOverlayMds` zdejmowało jedną nakładkę, a ten sam plik wczytany pod inną pisownią (przed poprawką 172) był drugą nakładką – sprint mógł zostać mimo zdjęcia.
- **Poprawka:**
  - `Npc_GetWaterLevel`: 2 przy pływaniu i nurkowaniu (głęboka woda), 1 w płytkiej wodzie, jak w silniku Gothica.
  - `Mdl_RemoveOverlayMds` zdejmuje wszystkie nakładki o tej nazwie.
  - Silnik co 200 ms sprawdza to samo co `PerFrame`: brak wytrzymałości, brak mikstury szybkości i założona nakładka sprintu → nakładka zdjęta, `SPRINT_ACTIVE = FALSE`. W logu `[ctx] sprint: no stamina - the sprint ends` (pokazuje, że skrypty same tego nie zrobiły).
- **Do sprawdzenia:** sprint do zera wytrzymałości – za każdym razem koniec sprintu, dalej zwykły bieg; pływanie – wytrzymałość spada, przy zerze spada życie; brodzenie w płytkiej wodzie – bez zmian; z miksturą szybkości sprint trwa.
- **Test (22.11):** działa – wytrzymałość spada przy pływaniu, sprint kończy się bez wytrzymałości.

### 172. Przycisk sprintu bez przycięcia gry – ✅ (22.11.2026)
- **Pliki:** `common/graphics/mesh/animationsolver.cpp/.h`, `common/graphics/mdlvisual.h`, `common/world/objects/npc.cpp`, `common/game/gamescript.cpp`, `common/resources.cpp`
- **Zgłoszenie:** po naciśnięciu przycisku sprintu gra na około sekundę się zacina.
- **Skrypty NB:** `B_Hotkey_Sprint` → `RX_ApplyAnim_Weapons` → `RX_ResetAllOverlays`: zdejmuje około 60 nakładek (style broni, chody, sprint), potem zakłada potrzebne.
- **Przyczyna:**
  - `Mdl_RemoveOverlayMds` wczytywało plik każdej zdejmowanej nakładki (MDS z animacjami), żeby go porównać z założonymi. Przy pierwszym naciśnięciu sprintu wczytywały się animacje kilkudziesięciu nakładek, których bohater nie miał.
  - Pamięć modeli i animacji rozróżniała wielkość liter: ten sam plik pod inną pisownią (w pluginie np. `humans_1hST1.MDS` i `humans_1hST1.mds`) był wczytywany drugi raz.
- **Poprawka:** nakładki zdejmowane i sprawdzane po nazwie (bez wielkości liter i rozszerzenia), bez wczytywania pliku (`Mdl_RemoveOverlayMds`, `Mdl_RemoveOverlayMds_NoUpdate`, `Mdl_HasOverlayMds`). Pamięć modeli i animacji po nazwie pisanej wielkimi literami.
- **Do sprawdzenia:** pierwsze i kolejne naciśnięcia sprintu bez przycięcia; zmiana broni i style walki według talentu bez zmian.
- **Test (22.11):** działa – sprint bez przycięcia gry.

### 171. Zmiana świata: RX_OnChangeLevel_Begin/End, punkt przybycia ze skryptów (Exodus) – ✅ (22.11.2026)
- **Pliki:** `common/game/gamesession.cpp`, `patches/zenkit-opengothic.patch` (`lib/ZenKit/src/Archive.cc`), `patches/README.md`
- **Zgłoszenie:** w zadaniu „Exodus” (eskorta piratów z Zachodniego Wybrzeża) po przejściu do Jarkendaru bohater zamiast przy obozie piratów ląduje w teksturach koło magów wody; piratów nie ma.
- **Skrypty NB:**
  - Plugin (`UNION_ABI.DLL`, `AB_ChangeLevelBegin`) woła przed zmianą świata `RX_OnChangeLevel_Begin`, a po niej `RX_OnChangeLevel_End`. Port ich nie wołał.
  - `RX_OnChangeLevel_Begin`: na Zachodnim Wybrzeżu przy trwającym zadaniu (`MIS_WC_GoToYark`, królowa martwa) ustawia `RX_AD_WC_HeroArrived = 1` i zabiera piratów Flinta i Edwarda z Wybrzeża (`RX_TransferNpc`).
  - `B_Enter_AddonWorld`: przy `RX_AD_WC_HeroArrived = 1` → 2 wstawia `VOBTREE/WP_AD_WC_ENTER_000.ZEN` (punkt przybycia) i piratów w tym punkcie. Oryginalny silnik stawia bohatera w świecie po funkcjach wejścia, więc punkt już jest.
- **Poprawka:**
  - Przed zmianą świata (jeszcze w starym świecie) `RX_OnChangeLevel_Begin`, po wejściu do nowego (po funkcjach wejścia) `RX_OnChangeLevel_End`. W logu `[ctx] change of the world: RX_OnChangeLevel_Begin (to … …)`.
  - Punkt przybycia, którego nie było przy wstawianiu bohatera, a który dodały funkcje wejścia – bohater przenoszony tam. Exodus: gdy `RX_AD_WC_HeroArrived` przeszło z 1 na 2, bohater staje w `WP_AD_WC_ENTER_000`, przy piratach. W logu `[ctx] change of the world: the hero at …`.
- **Test (22.11, log):** `RX_OnChangeLevel_Begin` działa (piraci zabrani z Wybrzeża, `RX_AD_WC_HeroArrived` 1 → 2), ale część świata z punktem przybycia się nie wczytała: `Unknown object type: zCVobWaypoint:zCVob`, `RX_InsertVobTree: the file "WP_AD_WC_ENTER_000.ZEN" is not in the game or is empty`. Piraci w złym miejscu („invalid spawnpoint”), bohater w punkcie startowym świata (w teksturach przy magach wody).
- **Zmiana (ZenKit, `patches/zenkit-opengothic.patch`):** klasa `zCVobWaypoint` (punkt ścieżki zapisany jako obiekt – tak NB zapisał ten plik) czytana jak `zCVobSpot`: wolny punkt z nazwą. Plik się wczytuje, punkt `WP_AD_WC_ENTER_000` jest w świecie przed postawieniem bohatera i piratów.
- **Zapis sprzed poprawki:** przejście już się odbyło bez `RX_OnChangeLevel_Begin` (flaga 0, piraci na Wybrzeżu) – trzeba wczytać zapis z Zachodniego Wybrzeża sprzed przejścia i przejść do Jarkendaru jeszcze raz (z piratami).
- **Do sprawdzenia:** Exodus – przejście z Wybrzeża do Jarkendaru: bohater i piraci przy obozie piratów.
- **Test (22.11):** działa – Exodus: bohater i piraci w Jarkendarze przy obozie piratów.

### 170. Szybki łup bez okna skrzyni – ✅ (22.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp/.h`, `common/mainwindow.cpp`
- **Zgłoszenie:** przy szybkim łupie przez ułamek sekundy widać chowające się okno skrzyni (prośba o ukrycie).
- **Poprawka:** skrzynia otwarta przez szybki łup (do jej opróżnienia, najwyżej 4 s) nie rysuje okna ekwipunku, a sterowanie na ekranie zostaje jak w grze (bez przełączania na układ ekwipunku). Widać tylko listę zabranych przedmiotów.
- **Do sprawdzenia:** przytrzymanie na skrzyni – bez okna i bez zmiany przycisków; krótkie dotknięcie – zwykłe okno.
- **Test (22.11):** działa – szybki łup bez okna skrzyni.

### 169. Szybki łup: okno skrzyni nie wraca – ✅ (22.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp/.h`
- **Zgłoszenie:** po przytrzymaniu przycisku akcji przedmioty są zabrane i wypisane, ale potem i tak otwiera się okno skrzyni.
- **Przyczyna:** szybki łup (poprawka 167) zamyka ekwipunek od razu po zabraniu, a interakcja bohatera ze skrzynią (animacja otwierania) trwa jeszcze chwilę i skrzynia prosi o otwarcie okna drugi raz.
- **Poprawka:** przez 2 s po szybkim łupie ta sama skrzynia nie otwiera okna – jest od razu zamykana, a interakcja kończy się jak przy zamknięciu.
- **Do sprawdzenia:** przytrzymanie na skrzyni – lista zabranych przedmiotów, bez okna skrzyni; po chwili krótkie dotknięcie tej samej skrzyni otwiera ją normalnie (pustą).
- **Test (22.11, nagranie):** nadal źle – po liście zabranych przedmiotów bohater zostaje kilka sekund przy skrzyni (animacja otwierania i zamykania), a na ekranie sterowanie jak przy otwartej skrzyni.
- **Zmiana:** po szybkim łupie bohater odchodzi od skrzyni od razu, bez animacji zamykania (`setInteraction(nullptr, quick)` i zatrzymanie animacji) – także gdy skrzynia w ciągu 2 s prosi o ponowne otwarcie.
- **Test 2 (22.11):** działa – przez ułamek sekundy widać chowające się okno, poza tym bez przerwy.

### 168. Sarkofagi i groby w ziemi w celowniku – ✅ (22.11.2026)
- **Pliki:** `common/world/objects/interactive.cpp`
- **Zgłoszenie:** sarkofagów na wykopaliskach magów wody w Khorinis nie da się wziąć w celownik – jak wcześniej miejsc kopania (poprawka 165).
- **Skrypty NB:** sarkofagi to skrzynie (`Sarg_S1`: licznik otwarć `SARG_DIGG`, w trybie `SBMode` 50% szans na zatrucie). Zawartość z pliku świata; `RX_InsertLoop` dokłada w Khorinis do `SARG_8` kamień zaklinania (obuchowy).
- **Przyczyna:** port uznaje obiekt za widoczny, gdy promień od głowy bohatera dochodzi do jego pudełka bez przeszkód, a potem jeszcze do jednego z jego punktów użycia. Sarkofag wpuszczony w ziemię: promień zatrzymuje się na ziemi przy jego krawędzi, a punkty użycia leżą w ziemi.
- **Poprawka:** promień zatrzymany tuż przy pudełku obiektu (nie dalej niż pół jego przekątnej i 30 cm od środka) nie zasłania go. Skrzynia, do której promień dochodzi bez przeszkód, jest widoczna bez sprawdzania punktów użycia.
- **Do sprawdzenia:** sarkofagi na wykopaliskach – nazwa w celowniku, otwieranie i szybki łup; zwykłe skrzynie za ścianą nadal niewidoczne.
- **Test (22.11):** działa – z grobowców można zabierać przedmioty.

### 167. Szybki łup na skrzyniach – ✅ (22.11.2026)
- **Pliki:** `common/mainwindow.cpp/.h`, `common/utils/androidinput.cpp/.h`, `common/ui/inventorymenu.cpp/.h`
- **Zgłoszenie:** szybki łup (poprawka 159) nie działa – przytrzymanie przycisku akcji na skrzyni od razu otwiera zwykłe okno.
- **Przyczyna:** poprawka 159 brała tylko skrzynie bez właściciela, funkcji skryptu i wyzwalacza (żeby nie pominąć ich skryptów przy otwieraniu). W NB większość skrzyń ma któreś z nich, więc przytrzymanie działało jak zwykłe dotknięcie. Na zwłokach działało.
- **Poprawka:** każda skrzynia bez zamka (albo z otwartym zamkiem) nadaje się do szybkiego łupu. Przytrzymanie 0,5 s otwiera ją jak zwykła akcja gry (animacja, skrypty, właściciel – jak przy zwykłym otwarciu), a ekwipunek, gdy się otworzy (do 4 s), od razu zabiera całą zawartość, pokazuje zabrane przedmioty na środku ekranu i zamyka się. Pusta skrzynia – „Nic tu nie ma” bez otwierania. Zwłoki – jak dotąd (bez okna). Wypisywanie zabranych przedmiotów wspólne: `InventoryMenu::nbPrintTaken`.
- **Do sprawdzenia:** przytrzymanie na kilku skrzyniach (zwykłych i z zadań) – zawartość w ekwipunku, napisy, okno się nie zostaje; krótkie dotknięcie – zwykłe okno.
- **Test (22.11):** działa – szybki łup zbiera ze skrzyń.

### 166. Pierścienie teleportacyjne – ✅ (22.11.2026)
- **Pliki:** `common/world/objects/item.cpp/.h`, `common/game/inventory.cpp`, `common/ui/inventorymenu.cpp`
- **Zgłoszenie:** pierścień teleportacyjny do Świątyni Adanosa nie działa (w ekwipunku przycisk „Załóż”).
- **Skrypty NB:** `ItRi_Teleport_Ring_Adanos` (i inne `ItRi_Teleport_*`) ma flagę pierścienia, ale nie ma funkcji założenia (`on_equip`) – ma funkcję użycia (`on_state` = `Equip_ItRi_Teleport_Ring_Adanos`) i schemat `MAP`. Użycie: w Jarkendarze `AI_Teleport(hero, "RAVENPRAY")`, gdzie indziej „za daleko”, w walce – napis. Port zakładał pierścień na palec, więc funkcja użycia nigdy się nie wykonała.
- **Poprawka:** pierścień albo amulet z funkcją użycia i schematem, bez funkcji założenia (`Item::nbUsableJewel`), jest używany jak mapa: animacja schematu i funkcja `on_state`. W karcie przedmiotu przycisk „Użyj”.
- **Do sprawdzenia:** w Jarkendarze użycie pierścienia z ekwipunku – teleport do świątyni Adanosa.
- **Test (22.11):** działa – teleport po przycisku „Użyj”.

### 165. Skarby do wykopania: wstawianie miejsca i celownik – ✅ (22.11.2026)
- **Pliki:** `common/world/objects/interactive.cpp/.h`, `common/game/gamescript.cpp/.h`, `common/mainwindow.cpp`
- **Zgłoszenie (pierwsze):** skarbu przy krzyżu (niebieskie iskry) nie da się wykopać – nie ma celownika ani ikony.
- **Log (poprawka 159):** `[ctx] mob near (no focus): none in 5 m` – w tym miejscu nie ma żadnego obiektu do użycia.
- **Skrypty NB:** obiekt kopania nie jest częścią świata. `RX_WestCoast_Loop` wstawia go (`RX_InsertVobTree("VOBTREE/VOBTREE_WC_TREASURE_QUEST.ZEN")`) po przeczytaniu notatki martwego pirata (`RX_WC_DeadPirateNote` 1 → 2), jeden raz. Jeśli to się stało, zanim port obsługiwał `RX_InsertVobTree` (poprawka 56), albo pliku nie znalazł, flaga jest już 2, a obiektu nie ma – i skrypt nie wstawi go nigdy więcej.
- **Poprawka:**
  - Wstawianie części świata wydzielone do `nbInsertVobTree`.
  - Naprawa co 5 s: na Zachodnim Wybrzeżu, gdy `RX_WC_DeadPirateNote` = 2, a tej części świata nie ma – wstawiana teraz (raz; w logu `NB: the part of the world "…" was not inserted … - inserted now`). Brak pliku – błąd w logu, bez ponawiania.
- **Sprawdzenie (pierwsze):** wczytanie zapisu na Zachodnim Wybrzeżu po przeczytaniu notatki – przy krzyżu obiekt do kopania. Jeśli notatka nie była czytana, skrypt wstawi go sam po jej przeczytaniu.
- **Drugi skarb Wybrzeża (wyższy kamień runiczny):** to inne miejsce – `PC_HIDDENFIND_Addon_Hour_Info` daje `ItMi_UpRuneBlank` przy `FP_ROAM_SPAWN_415` (do 3 m), po kopaniu w obiekcie z funkcją stanu `HIDDENFIND`. Tego obiektu port przy krzyżu nie ma. Diagnostyka (`common/mainwindow.cpp`): przy dotknięciu przycisku akcji bez celownika log wypisuje raz wszystkie obiekty kopania świata (`[ctx] dig place: … at x y z dist=… m`, `[ctx] dig places in this world: N`, pozycja bohatera) i przedmioty do 5 m – pokaże, czy obiekt jest w innym miejscu, czy go w ogóle nie ma.
- **Zgłoszenie (drugie):** skarbu pirata nie da się wykopać, choć po przeczytaniu notatki skrypt wstawił jego miejsce (log: `part of the world: VOBTREE_WC_TREASURE_QUEST.ZEN (1 objects)`, w świecie 4 miejsca kopania zamiast 3).
- **Przyczyna:** miejsca kopania NB (obiekty z funkcją stanu `HIDDENFIND`, schemat `ORE`, używane z kilofem) leżą w ziemi, pod skarbem, i nie mają nazwy w celowniku. Port sprawdza, czy obiekt widać: promień od głowy bohatera do pudełka obiektu i do jego punktów użycia – przez ziemię nigdy nie dochodził, więc miejsce nigdy nie trafiało do celownika.
- **Poprawka:** miejsce kopania (`HIDDENFIND`) nie wymaga promienia – wystarczy odległość i kąt jak dla innych obiektów. Bez nazwy w celowniku pokazuje „Miejsce do kopania”. Dalej jak w skryptach: z kilofem okno „Kop.” i skarb przy punkcie (np. `FP_ROAM_SPAWN_857` – skarb pirata, `FP_ROAM_SPAWN_415` – wyższy kamień runiczny).
- **Do sprawdzenia:** przy skarbie pirata z kilofem w ekwipunku – „Miejsce do kopania” w celowniku, kopanie, skarb.
- **Test (22.11):** działa – „Miejsce do kopania” w celowniku, skarb pirata wykopany.

### 164. Mikstura ochrony przed następnym ciosem – ✅ (22.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/world/objects/npc.cpp`
- **Zgłoszenie:** nie działa mikstura na następny atak.
- **Skrypty NB:** `Use_ItPo_HitPotion` ustawia tylko `RX_NextHitIgnore = TRUE`. Resztę robi plugin (`UNION_ABI.DLL` 0x1013AB90): przy następnym ciosie w bohatera, gdy `RX_NextHitIgnore` = 1 (a `RX_ImmortalState` nie), flaga wraca do 0, cios nie zadaje obrażeń, napis `sPotionHitProtected` i efekt `SPELLFX_HEAL` na bohaterze. Port tego nie miał – mikstura nic nie dawała (tylko napis w statusie).
- **Poprawka:** `GameScript::nbNextHitIgnore` jak w pluginie: cios w bohatera (broń, czar, pocisk potwora – `Wld_PlayEffect_Vec`) zabrany, flaga 0, napis, efekt leczenia. W logu `[ctx] hit potion: the blow taken away`.
- **Do sprawdzenia:** wypicie mikstury i jeden cios przeciwnika – bez obrażeń, napis; kolejny cios już normalnie.
- **Test (22.11):** działa – mikstura ochroniła przed obrażeniami.

### 163. Towarzysz po walce znowu w celowniku (Fregyal) – ✅ (22.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/world/objects/npc.cpp`, `common/mainwindow.cpp/.h`
- **Zgłoszenie:** po pokonaniu szamana jaszczuroludzi na Zachodnim Wybrzeżu nie da się porozmawiać z Fregyalem – nie da się go wziąć w celownik.
- **Log:** Fregyal 19 cm przed bohaterem, widoczny, towarzysz (`AIV_PARTYMEMBER`=1), ale `noFocus=1`.
- **Skrypty NB:**
  - `ZS_Attack`: towarzysz (lub `NPCTYPE_FRIEND`) walczący z kimś innym niż bohater dostaje `self.noFocus = TRUE` (żeby go nie trafić w walce); `ZS_Attack_End` przywraca `FALSE`.
  - `AI_OnDead_Draconian_ShamanWC_Final`: `Npc_ClearAIQueue` i `AI_StartState(Fregeal, ZS_ReactToDamage, 0, "")` – nowy stan bez zakończenia bieżącego, więc `ZS_Attack_End` się nie wykonuje i `noFocus` zostaje na zawsze.
- **Diagnostyka (zostaje w porcie):**
  - przy próbie rozmowy (raz na 5 s dla postaci): `[ctx] talk: "imię" state=… assessTalk=… talk=… down=… CAMMODEON=… HEROTRANS=… queue=…`;
  - przy dotknięciu przycisku akcji (raz na 3 s): `[ctx] focus: …` i postacie do 5 m (`[ctx] npc near: …` – odległość, kąt, `noFocus`, martwy, powalony, towarzysz, widoczność, broń bohatera, pozycja);
  - pomocnicza `GameScript::nbIntVar` (globalna zmienna int skryptów).
- **Pierwszy log:** rozmowa z Kormakiem działa (`CAMMODEON=0` – tryb kamery walki z szamanem wyłączony), próby rozmowy z Fregyalem nie ma.
- **Poprawka:** co sekundę towarzysze (`AIV_PARTYMEMBER`) do 50 m od bohatera z `noFocus`, którzy nie są w `ZS_Attack`, dostają celownik z powrotem. W logu: `[ctx] focus: … can be in the focus again (out of ZS_Attack)`. Działa także dla zapisów, w których Fregyal już jest bez celownika. W walce (`ZS_Attack`) towarzysz zostaje bez celownika, jak w skryptach.
- **Do sprawdzenia:** wczytanie zapisu po walce z szamanem – Fregyal w celowniku, rozmowa działa.
- **Test (22.11):** działa – rozmowa z Fregyalem po walce z szamanem.

### 161. Towarzysze teleportują się z bohaterem – ✅ (22.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`
- **Zgłoszenie:** gdy bohater ma towarzyszy, po teleporcie powinni pojawić się razem z nim (jak na PC).
- **Poprawka:** port co klatkę porównuje pozycję bohatera. Skok o ponad 30 m w tym samym świecie to teleport (runa, zwój, skrypt). Wtedy towarzysze (`AIV_PARTYMEMBER`) i przywołane stworzenia, które były do 30 m od miejsca, z którego bohater zniknął, stają za nim w półkolu (1,5 m od niego); ich kolejka AI jest czyszczona, dalej idą za bohaterem jak zwykle. Kto czeka gdzie indziej (dalej niż 30 m), zostaje. W logu: `[ctx] teleport: N companions with the hero (… m)`.
- **Nie obejmuje:** zmiany świata (inny plik ZEN) – tam towarzysze zostają, jak w Gothic 2.
- **Do sprawdzenia:** z towarzyszem (np. Blizna) i przywołańcem teleport runą – obaj obok bohatera.
- **Test (22.11):** działa – towarzysze teleportują się z bohaterem.

### 159. Szybki łup (przytrzymanie przycisku akcji); log miejsc bez celownika – ✅ (21.11.2026)
- **Pliki:** `common/mainwindow.cpp/.h`, `common/utils/androidinput.cpp/.h`, `common/world/objects/interactive.h`
- **Zgłoszenie:** NB na PC ma szybki łup (klawisz `KEY_QUICKLOOT`: zwłoki, skrzynie, przedmioty na ziemi), port – nie. Bez nowego przycisku na ekranie. Drugie zgłoszenie: skarbu do wykopania (miejsce z niebieskimi iskrami przy krzyżu) nie da się użyć – nie ma celownika ani ikony.
- **Szybki łup:**
  - Krótkie dotknięcie przycisku akcji działa jak dotąd (otwiera skrzynię, przeszukuje zwłoki). Przytrzymanie przez 0,5 s na zwłokach albo skrzyni zabiera od razu całą zawartość.
  - Skrzynie: tylko otwarte (bez zamka lub z otwartym zamkiem), bez właściciela, bez funkcji skryptu i bez wyzwalacza – ich otwarcie robi coś więcej niż daje przedmioty, więc zostaje zwykłe otwieranie. Zwłoki: tylko martwe postacie, z tym, co pokazuje przeszukiwanie.
  - Jak w NB: nie przy wrogach w pobliżu (15 m, ktoś atakuje bohatera) – napis „Wrogowie w pobliżu!”. Pusto – „Nic tu nie ma”.
  - Zabrane przedmioty: napisy na środku ekranu (nazwa i liczba, do 6 wierszy), dźwięk ekwipunku. Bez broni w ręku.
- **Skarby (diagnostyka):** dotknięcie przycisku akcji bez celownika zapisuje w logu obiekty do użycia w promieniu 5 m: `[ctx] mob near (no focus): …` (nazwa, nazwa celownika, schemat, funkcja stanu, odległość) albo `none in 5 m`. Skrypty NB: kopanie skarbów (`HIDDENFIND`) to obiekt z kilofem, oknem „Kop.” i skarbem wstawianym w ziemię – log pokaże, czy port go w ogóle ma.
- **Do sprawdzenia:** przytrzymanie na zwłokach i na otwartej skrzyni – wszystko w ekwipunku, napisy; krótkie dotknięcie – zwykłe otwarcie; przy skarbie dotknięcie przycisku akcji i log.
- **Test (22.11):** działa – szybkie zbieranie.

### 158. Pociski potworów lecą prosto, bez naprowadzania – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp`
- **Zgłoszenie:** pocisk golema bagiennego (poprawka 157) skręca za bohaterem, aż trafi. W Gothic leci w kierunku, w którym go wystrzelono – krokiem w bok można go uniknąć.
- **Poprawka:** kierunek jest ustalany raz, w chwili rzutu: z punktu startu do miejsca celu (`emTrjTargetNode` lub środek postaci). Pocisk leci prosto z tą samą prędkością i w każdej klatce sprawdza, czy po drodze nie trafia postaci (innej niż rzucający) albo świata (teren, ściany, obiekty). Trafiona postać dostaje efekt trafienia i obrażenia – także inna niż cel, jeśli stanie na drodze. Ściana lub ziemia: efekt trafienia w świat (`emFXCollStat`), bez obrażeń. Gdy nic nie trafi, gaśnie 15 m za miejscem celu.
- **Nie zmienione:** zaklęcia bohatera (poprawka 84) dalej podążają za celem – celowanie dotykiem.
- **Do sprawdzenia:** golem bagienny, zombie lub mumia z daleka – krok w bok w czasie lotu i pocisk przelatuje obok; stojąc w miejscu – trafienie i obrażenia.
- **Test (21.11):** działa – ataki z daleka można ominąć.

### 157. Pociski potworów z daleka (rój, kwas) – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/game/gamesession.cpp`
- **Zgłoszenie:** golem bagienny strzela czymś, gdy bohater jest daleko, i tego nie widać. To samo robią bagienne trutnie, zombie i mumie.
- **Skrypty NB (`ZS_MM_Attack_Loop`):** gdy cel jest dalej (np. 400–3000 cm u zombie, mumii i golema), potwór gra `T_WARN` i woła `Wld_PlayEffect_Vec("SPELLFX_SWARM", self, other, 2, obrażenia, DAM_MAGIC, TRUE)`; bagienne trutnie – `SPELLFX_ACID`. Ostatni argument (`bIsProjectile`) oznacza pocisk lecący do celu.
- **Przyczyna:** port uruchamiał efekt od razu z kluczem `CAST`, którego trajektoria `TARGET` stawia go na celu – nie było lotu, a obrażenia przychodziły w tej samej chwili.
- **Poprawka:** przy `bIsProjectile` efekt jest pociskiem: startuje z `emTrjOriginNode` potwora (lub ze środka postaci), leci do `emTrjTargetNode` celu z prędkością `emTrjEaseVel` klucza `CAST` i podąża za celem. Po dotarciu: efekt trafienia (`emFXCollDyn`), postrzeżenie magii i obrażenia (jak dotąd: minus ochrona przed rodzajem, co najmniej `NPC_MINIMAL_DAMAGE`). Bez celu (zniknął, zginął) lub po 6 s pocisk gaśnie z efektem `emFXCollStat`. Pociski w locie znikają przy zmianie świata i wyjściu z gry. Wywołania bez `bIsProjectile` działają jak dotąd.
- **Do sprawdzenia:** golem bagienny, zombie, mumia i bagienny truteń z daleka – widoczny lecący pocisk, obrażenia przy trafieniu.
- **Test (21.11):** działa – pocisk golema bagiennego widać w locie.

### 156. Karta przedmiotu w ekwipunku: wielkość napisów, zdobyte bonusy – ✅ (21.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp/.h`
- **Zgłoszenie:** napisy na karcie przedmiotu (prawa strona ekwipunku) są za duże (uwaga borsuczyny). Pomysł: zmniejszyć je i w wolnym miejscu pokazywać bonusy z jedzenia. Na zrzucie zakładka wszystkich przedmiotów nazywa się „Other (481)”.
- **Poprawka:**
  - Nazwa przedmiotu mniejsza o ok. 10%, właściwości (obrażenia, wymagania, waga, wartość) o ok. 18%.
  - Po użyciu przedmiotu z ekwipunku (jedzenie, mikstura, zwój…) pojawia się na zielono, co bohater zyskał, np. „Inteligencja +1   Maks. życie +5”. Liczone ze zmiany wartości bohatera: życie, mana, ich maksima, siła, zręczność, inteligencja i maks. wytrzymałość NB. Napis zostaje 6 s od ostatniego użycia, kolejne użycia się sumują.
  - Nazwy zakładek: `TXT_INV_CAT` w NB jest częściowo po angielsku („Other”, „Weapons”, „Potions”), a pierwsza pozycja („Other”) to zakładka wszystkich przedmiotów. Port pokazuje polskie nazwy: Wszystko, Bronie, Mikstury… Polskie nazwy skryptów (Magia, Jedzenie, Dokumenty…) zostają.
- **Do sprawdzenia:** zjedzenie przedmiotu ze stałym bonusem (np. gulasz, eliksir umysłu) – zielony napis z bonusem; czytelność mniejszych napisów; nazwa pierwszej zakładki.
- **Test (21.11):** działa – po wypiciu eliksiru zręczności pod nazwą pojawia się „Zręczność +2”.
- **Zmiana po teście:** napis był prawie czarny (kolor nakładał się na ciemną czcionkę książkową) i ściśnięty pod nazwą. Teraz bonus to pływające powiadomienie na środku ekranu: unosi się i znika w 2,5 s, zielone przy zysku, czerwone przy stracie, z cieniem dla czytelności. Kilka bonusów naraz – jeden pod drugim, z małym odstępem w czasie. Zmiana, która przychodzi później (efekt po animacji jedzenia), daje nowe powiadomienie; wartości są obserwowane 6 s od użycia.
- **Test 2 (21.11):** działa – po eliksirze siły na środku ekranu „Siła +2”.

### 155. Obrażenia z kilkoma rodzajami naraz (kamienny golem) – ✅ (21.11.2026)
- **Pliki:** `common/game/damagecalculator.cpp`
- **Zgłoszenie:** kamienny golem zadaje za duże obrażenia (do porównania ze skryptami).
- **Skrypty NB (`StoneGolem`, `Mst_Default_StoneGolem`):**
  - siła 750;
  - obrażenia obuchowe 250 i obrażenia odrzutu 100 (`DAM_BLUNT|DAM_FLY`).
- **Przyczyna:** OpenGothic dodawał całą siłę do każdego rodzaju obrażeń osobno: 750+250 obuchowych i 750+100 odrzutu, razem ok. 1850 przed pancerzem. W Gothic 2 siła liczy się raz: suma (siła + obrażenia) jest dzielona między rodzaje według ich udziału – u golema ok. 786 obuchowych i 314 odrzutu, razem 1100 przed pancerzem.
- **Poprawka:** przy kilku rodzajach obrażeń siła jest dodawana raz i dzielona proporcjonalnie (dotyczy też trolli i broni z dwoma rodzajami obrażeń). Przy jednym rodzaju bez zmian.
- **Test (21.11):** przyzwany golem zadaje obrażenia jak na PC (smoczy zębacz na kilka ciosów, wcześniej na 2).
- **Do sprawdzenia:** cios golema i trolla – w logu `DMG2: melee attacker='Kamienny golem' ... value=` (przed poprawką ok. 1850 minus pancerz, teraz ok. 1100 minus pancerz); porównanie z PC.

### 154. Ognik i przywołańce po wczytaniu – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/game/gamesession.cpp`, `mods/nb-wisp/index.js` (1.2.2)
- **Zgłoszenie:**
  1. Ognik znika i pojawia się w losowych momentach.
  2. Po wczytaniu zapisu przywołane stworzenie wciąż jest przy bohaterze – na PC go nie ma.
- **Przyczyna:**
  1. Ognik zostawał w tyle za biegnącym bohaterem (leci 8 m/s). Przy 40 m silnik stawiał go obok bohatera, a jednocześnie mod ognika co 3 s go usuwał i przywoływał od nowa – stąd znikanie i pojawianie się. W logu seria `the wisp was 42..54 m away`.
  2. Przywołańce zapisywały się z grą i wracały po wczytaniu. Plugin NB odsyła je przy wczytaniu, jak przy zmianie świata.
- **Poprawka:**
  1. Ognik dalej niż 15 m od bohatera leci dwa razy szybciej. Przy 40 m silnik stawia go obok bohatera od razu (pozycja i transformacja) i przerywa lot powrotny. Mod ognika usuwa i przywołuje go tylko, gdy zgubi się dalej niż 100 m (np. po teleporcie). Log odległości najwyżej raz na 5 s.
  2. Po wczytaniu zapisu przywołańce są odsyłane (jak `RX_ResetSummons`). Zostają zwierzaki (Nixdog, Jina), totem i ognik.
- **Do sprawdzenia:** bieg i sprint z ognikiem – leci za bohaterem i nie miga; wczytanie zapisu z przywołańcem – go nie ma.
- **Test (21.11):** ognik nie gubi się, przyzwany golem znika po wczytaniu.

### 153. Czerwony dym na Zachodnim Wybrzeżu – ✅ (21.11.2026)
- **Pliki:** `common/world/triggers/movetrigger.cpp/.h`, `common/game/gamescript.cpp` (`RX_RemoveVobTree`)
- **Zgłoszenie (nagrania z PC):** w drodze z Fregyalem do Kormaka i przy drugim miejscu w lesie przelatuje czerwony dym („W lesie zauważyliśmy dziwny, czerwony dym...”). W porcie go nie ma.
- **Przyczyna:**
  - Skrypt (`RX_WestCoast_Loop`) przy punktach `FP_ROAM_SPAWN_SWAMP_280` i `FP_ROAM_SPAWN_755` wyłącza muzykę, gra dźwięk i włącza windę `EVT_SMOKE_00` / `EVT_SMOKE_01`. Po kilkunastu sekundach `RX_RemoveVobTree` usuwa ją ze świata.
  - Wygląd tej windy to cząsteczki `SMOKE_SOUL_FOG.pfx` (czerwony dym ze smugą), a nie siatka. OpenGothic próbował wczytać je jako siatkę – w logu `unable to load mesh "SMOKE_SOUL_FOG.pfx"` – i nic nie rysował.
  - `RX_RemoveVobTree` z nazwą obiektu świata była pomijana.
- **Poprawka:**
  - Winda, której wyglądem są cząsteczki (`*.PFX`), rysuje je i niesie po swojej trasie.
  - `RX_RemoveVobTree` z nazwą obiektu świata (nie pliku `.ZEN`) chowa ten obiekt razem z dziećmi, jak `RX_ToggleVobVisible`, i zapisuje to w zapisie gry.
- **Log:** `[ctx] object of the world removed (hidden): EVT_SMOKE_01 (1)`.
- **Do sprawdzenia:** czerwony dym przelatuje w obu miejscach, po kilkunastu sekundach znika; brak `unable to load mesh "SMOKE_SOUL_FOG.pfx"` w logu.
- **Test (21.11, nagranie):** czerwony dym przelatuje przez las razem z wpisem do dziennika.

### 152. Walka z kilkoma przeciwnikami naraz – 🧪 (21.11.2026)
- **Pliki:** `common/world/objects/npc.cpp/.h` (`nbReactToHit`)
- **Zgłoszenie:** towarzysz (Blizna) na farmie Oswalda zaatakowany przez dwa zębacze nie walczył – krążył i obracał się, bez pomocy bohatera by zginął. To samo przy kilku bandytach na jedną postać (obóz bandytów, Brago).
- **Przyczyna:** po każdym trafieniu postać (nie bohater) porzucała plan walki: kolejka ruchów była czyszczona, a następny ruch brany z `MY_W_STRAFE` (krok w bok, parada). Dwa zębacze gryzą na zmianę mniej więcej co pół sekundy, więc postać przez cały czas tylko reagowała na trafienia, obracała się między nimi i nie zdążyła zaatakować.
- **Poprawka:**
  - Reakcję na trafienie wywołuje tylko cios własnego celu postaci (albo dowolny cios, gdy postać nie ma celu lub jej cel leży).
  - Najwyżej raz na 1,5 s.
  - Ciosy pozostałych napastników nie przerywają ataku na cel. Postrzeganie obrażeń (`B_AssessDamage`, zmiana celu przez skrypt) działa jak dotąd.
  - Bohater bez zmian.
- **Do sprawdzenia:** towarzysz przeciw 2–3 zwierzętom (zębacze, wilki) – walczy i zadaje ciosy. Jedna postać przeciw jednemu przeciwnikowi nadal robi uniki po trafieniu.

### 151. Wygląd broni od Gallahada (`RX_ApplyVisualGallahad`) – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp`, `common/graphics/mdlvisual.cpp`
- **Zgłoszenie (log 21.11):** po zaklęciu wyglądu broni u Gallahada (ogień niebieski, elektryczność zielona, intensywność bardzo mocna) na broni nie ma żadnego efektu. W logu: `not implemented call [RX_APPLYVISUALGALLAHAD]`.
- **Przyczyna:** wygląd broni robi plugin (`ItemEffectManager`, UNION_ABI.DLL `0x101843a0`). Kładzie cząsteczki `NB_VIS_<rodzaj>_USUAL` na założoną broń bohatera, w wybranym kolorze i intensywności, i zapamiętuje je przy przedmiocie. W porcie tej funkcji nie było.
- **Poprawka:**
  - `RX_ApplyVisualGallahad(typ)` – broń biała (0) albo dystansowa (1, cząsteczki `_RANGE_`) dostaje cząsteczki z `RX_Gallahad_Visual_Type`, kolor z `RX_Gallahad_Visual_Color` (`ITEMEFFECTMANAGER_RED`…`GREY_1`) i intensywność z `RX_Gallahad_Visual_PPS` (5 stopni).
  - Wygląd jest zapisany w przedmiocie (pole efektu, z zapisem gry) i rysowany na broni w ręku i na plecach, tak jak efekty z poprawki 150.
  - `RX_RemoveVisualGallahad(typ)` zdejmuje go.
- **Uwaga:** odcienie kolorów i stopnie intensywności są dobrane w przybliżeniu – plugin liczy je w swoim kodzie rysowania.
- **Log:** `[ctx] Gallahad: Starożytne Ostrze <- NB_VIS_MELEE_FIRE_USUAL:3:4`.
- **Do sprawdzenia:** zaklęcie wyglądu u Gallahada – efekt od razu na broni, po wczytaniu zapisu nadal jest; zmiana koloru i intensywności.
- **Test (21.11):** działa, oznaczone jako sprawdzone. Pierwsze obserwacje: działa w przybliżeniu – ogień zielony, iskry fioletowe, magia niebieska; cząsteczki widać przy broni na biodrze. Do porównania z PC: położenie i wielkość efektu przy dobytej broni.

### 150. Efekty przedmiotów (C_Item.effect) na broniach i w świecie – 🧪 (21.11.2026)
- **Pliki:** `common/graphics/mdlvisual.cpp/.h`, `common/graphics/effect.cpp/.h`, `common/graphics/pfx/pfxbucket.cpp`, `common/world/objects/pfxemitter.cpp`, `common/world/objects/item.cpp/.h`, `common/world/objects/npc.cpp/.h`, `common/game/inventory.cpp`, `common/game/gamescript.cpp`
- **Zgłoszenie:** bronie z efektami (ogniste miecze, ogniste łuki) nie zostawiają śladu przy ciosie, brakuje też innych efektów wizualnych.
- **Przyczyna:** OpenGothic w ogóle nie odtwarzał efektu przedmiotu (`C_Item.effect`). W Gothic 2 silnik dokłada go do założonej broni (`oCNpc::AddItemEffects`) i do przedmiotu leżącego w świecie (`oCItem::InsertEffect`). W skryptach NB efekt ma ponad 50 przedmiotów, np.:
  - `SPELLFX_FIREBOW` – ogniste łuki;
  - `SPELLFX_FIRESWORDBLACK` – czarne ogniste miecze;
  - `spellfx_swampdragon`, `SPELLFX_STONESPHERE`, `SPELLFX_WATERSPHERE`, `SPELLFX_ENCHANT_ICE`;
  - `SPELLFX_ITEMGLIMMER` – blask przedmiotów;
  - świece i pochodnie.
- **Poprawka:**
  - Efekt przedmiotu idzie za bronią białą, łukiem lub kuszą i tarczą – w ręku i na plecach. Jest wyłączany razem z postacią poza zasięgiem rysowania (146).
  - Efekty z `emAdjustShpToOrigin` (np. ogień miecza) wypuszczają cząsteczki z siatki samej broni, wzdłuż ostrza. Przy ciosie zostaje za nią ślad ognia.
  - Przedmioty leżące w świecie mają swój efekt (blask, ogień) i przesuwa się on razem z nimi.
  - Cząsteczki emitowane z nieruchomej siatki liczone są w układzie przedmiotu (wcześniej odejmowana była pozycja w świecie).
  - Log: każdy efekt skryptów (`Wld_PlayEffect`) raz, z postacią, na której gra – `[ctx] effect: NAZWA on POSTAĆ`. Pomaga znaleźć efekty, których nie widać.
- **Do sprawdzenia:**
  - Ognisty miecz lub łuk w ręku i na plecach, ciosy (ślad ognia).
  - Przedmioty z blaskiem leżące na ziemi.
  - Klatki przy wielu postaciach z takimi broniami.

### 149. Paczka 6 brakujących funkcji (log z Zachodniego Wybrzeża) – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp/.h`, `common/mainwindow.cpp`
- **Zgłoszenie:** log z gry na Zachodnim Wybrzeżu (podsumowania `NB missing` i wpisy `not implemented call`).
- **Poprawki:**
  - `RX_CopyAllDialogsNpc(z, do)` – przenosi dialogi jednej postaci na drugą. `ZS_Talk` przenosi dialogi Fregyala (`NONE_1192_Fregeal`) na Fregyala z Zachodniego Wybrzeża (`NONE_1192_Fregeal_WC`). Bez tej funkcji rozmowa z nim kończyła się pogawędką, bez jego tematów.
  - `RX_CreateSpellRune(postać, czar)` – przed `AI_ReadySpell` postać dostaje runę czaru, jeśli jej nie ma (tabela run z pluginu: 29 czarów, np. 72 – wodna pięść). Bez runy magowie NB (np. druid Kormak) nie mieli czego dobyć i walczyli wręcz.
  - `GetFocusNpc(postać)` i `Npc_ValidFocusNpc(postać)` – postać w celowniku (u bohatera ta, którą pokazuje ekran, u innych ich cel). Używają ich `HandleEvent` i `G_OpenSteal` (klawisze pluginu: kradzież, zamki).
  - `Npc_ModifyBodyState` – skrypty zwracają z niej liczbę. Przy dawnym `void` funkcja trafiała do domyślnej obsługi i w logu było `not implemented call`. Zmiany nadal nie ma, bo OpenGothic nie ma tych modyfikatorów stanu ciała (`BS_MOD_CONTROLLED` przy koncertach).
- **Nie w tej paczce:**
  - `UpdateKarma` – część systemu karmy pluginu, z jego własnymi zmiennymi (osobna, większa praca).
  - `Wld_IsRaining` – stała odpowiedź „nie pada” z OpenGothic.
  - Ostrzeżenia `C_NPC.AIVAR without an instance set` (`C_PlayerIsFakeBandit`, `ZS_Dead`) – skrypty czytają zmienną z pustej postaci i dostają 0, bez skutków w grze.
- **Log:** `[ctx] dialogs: N moved from NONE_1192_Fregeal to NONE_1192_Fregeal_WC`, `[ctx] rune for the spell 72: ITRU_WATERFIST to Kormak`.
- **Do sprawdzenia:**
  - Rozmowa z Fregyalem na Zachodnim Wybrzeżu – są jego tematy.
  - Kormak w walce rzuca wodną pięść.
- **Test (21.11, log):** Kormak dostaje runę wodnej pięści (`rune for the spell 72: ITRU_WATERFIST`) i walczy czarem.
- **Test (21.11):** rozmowa z Fregyalem na Zachodnim Wybrzeżu działa – 25 tematów przeniesionych (`dialogs: 25 moved`), handel otwiera się.
  - Wyłamywanie zamków i kradzież bez `not implemented call` w logu.

### 148. Młot Adanosa przeciw kamiennym strażnikom – ✅ (21.11.2026)
- **Pliki:** `common/game/gamescript.cpp` (`Npc_ChangeAttributeExtra`)
- **Zgłoszenie:** młot Adanosa („Skuteczny przeciwko kamiennym strażnikom”) potrzebuje około 5 uderzeń na kamiennego strażnika, a powinien 1–2.
- **Przyczyna:**
  - W `B_MM_AssessDamage` skrypt sprawdza broń w ręku bohatera. Przy młocie Adanosa zwykły kamienny strażnik dostaje 10 000 obrażeń, a strażnik-boss 50 000; obrażenia zadaje `B_MagicHurtNpc_Immortal`.
  - Ta funkcja odejmuje życie przez `Npc_ChangeAttributeExtra` – funkcję z pluginu NB (UNION_ABI.DLL), której w porcie nie było. Specjalne trafienie nic nie robiło i zostawały tylko zwykłe obrażenia młota.
- **Poprawka:** dodana `Npc_ChangeAttributeExtra` – zmiana atrybutu jak `Npc_ChangeAttribute`, ale z pominięciem nieśmiertelności (jak w pluginie). Gdy postać ginie, zabójcą jest bohater.
- **Przy okazji:** ta sama funkcja działa dla innych specjalnych broni, np. Świętego Młota przeciw magicznemu golemowi i ognistemu golemowi.
- **Log:** `[ctx] ChangeAttributeExtra 'Kamienny strażnik' attr=0 value=-10000 hp=0 (dead)`.
- **Potwierdzenie z logu (21.11):** `not implemented call [NPC_CHANGEATTRIBUTEEXTRA] from B_MAGICHURTNPC_IMMORTAL (SEEKERWC, 0, -812)`. Celem był Starożytny strażnik (SEEKERWC), a zwykłe uderzenie młotem zadawało mu 0 obrażeń.
- **Ile trafień:** według skryptów NB młot zabiera:
  - zwykłemu kamiennemu strażnikowi 10 000 życia – 1 trafienie;
  - kamiennemu strażnikowi-bossowi 50 000 – 1 trafienie;
  - Starożytnemu strażnikowi na Zachodnim Wybrzeżu 1/8 największego życia (812 z 6496) – 8 trafień. Każde trafienie włącza też jego specjalny atak.
- **Do sprawdzenia:** młot Adanosa w ręku, uderzenie w zwykłego kamiennego strażnika – ginie od pierwszego trafienia. Starożytny strażnik traci 1/8 życia na każde trafienie.
- **Test (21.11):** działa – każde trafienie młotem zabiera Starożytnemu strażnikowi 812 życia (`ChangeAttributeExtra ... value=-812`).

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

### 138. Wydobywanie żelaza, złota i magicznej rudy; kolor żyły w celowniku – ✅ (21.11.2026)
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
- **Test (22.11):** działa – żyła na zielono, po wydobyciu biała/szara.

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

### 116. Muzyka gry nie cichnie przy muzyce bossa – ✅ (19.11.2026)
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
- **Test (21.11):** działa – w walce z bossem gra tylko jego muzyka.

### 113. Koło szybkiego wyboru nie reaguje na dotyk – ✅ (18.11.2026)
- **Pliki:** `common/utils/androidinput.cpp` (`onInput`)
- **Zgłoszenie:** z koła nie da się niczego wybrać.
- **Przyczyna:** na Androidzie dotyk szedł prosto do przycisków (`controls.down/move/up`), z pominięciem `pointerDown/Move/Up`, w których koło (111) przejmuje palce. Koło się otwierało, ale nie widziało żadnego dotknięcia.
- **Poprawka:** dotyk Androida idzie przez `pointerDown/Move/Up`; anulowanie dotyku czyści też palec koła.
- **Do sprawdzenia:** dotknięcie pola koła – przedmiot użyty; puste pole – ekwipunek; przytrzymanie – zdjęcie z koła; środek – zamknięcie.
- **Test (21.11):** działa – koło szybkiego wyboru bez problemów.

### 112. Koło szybkiego wyboru: 9 pól bez prążków, przycisk ze strzałkami ⇄ – ✅ (18.11.2026)
- **Pliki:** `common/utils/androidinput.cpp/.h`, `common/game/nbhotbar.h`
- **Zgłoszenie (zrzut 342743, wzór 342745):** koło wygląda, jakby miało za dużo pól – prążki. Ma być 9 pól. Przycisk: dwie strzałki w przeciwnych kierunkach na czarnym kółku.
- **Przyczyna prążków:** każde pole było złożone z 10 półprzezroczystych trójkątów. Painter zaokrągla wierzchołki do pikseli, więc między trójkątami zostawały szpary. Wyglądało to jak kilkadziesiąt pól.
- **Poprawka:**
  - 9 pól po 40° (`NbWheel::Count` = 9, slot 8 nowy).
  - Trójkąty pola lekko na siebie zachodzą i mają pełny (nieprzezroczysty) kolor; granice między polami to czarne linie.
  - Przycisk: czarne kółko, górna strzałka w prawo, dolna w lewo.
- **Do sprawdzenia:** równe 9 pól, podświetlenie, nowy przycisk.
- **Test (21.11):** działa.

### 111. Koło szybkiego wyboru (drugi pasek) – ✅ (18.11.2026)
- **Pliki:** `common/game/nbhotbar.h` (`NbWheel`), `common/utils/androidinput.cpp/.h` (koło: dotyk, rysowanie), `common/ui/mobilecontrolsmodel.cpp/.h` (przycisk `wheel`, `Special::Wheel` = 27), `common/ui/inventorymenu.cpp/.h`, `common/mainwindow.cpp`
- **Zgłoszenie (zrzuty 342740, 342739):** drugi pasek szybkiego wyboru: broń, mikstury, rośliny. Przycisk z kółkiem obok paska otwiera koło, a przedmioty można przypisać i tu (pasek), i tam (koło).
- **Poprawka:**
  - Nowy przycisk HUD (białe kółko, id `wheel`, obok plecaka; można go przesunąć w edytorze HUD).
  - Dotknięcie otwiera koło na środku ekranu: 8 pól z modelami przedmiotów i liczbą sztuk; gra jest wstrzymana. Palec można przesuwać po polach (niebieskie podświetlenie), wybór następuje przy puszczeniu; na środku nazwa przedmiotu z pola pod palcem; puszczenie na środku zamyka koło.
  - Wybrane pole używa przedmiotu (broń zakłada, mikstura/roślina – zużywa). Puste pole otwiera ekwipunek. Przytrzymanie pola (0,6 s) zdejmuje przedmiot z koła.
  - Na karcie przedmiotu w ekwipunku dwa przyciski: **NA PASEK / Z PASKA** (poprawka 110) i **NA KOŁO / Z KOŁA**.
  - Zapis: `[MOBILE_WHEEL] slot0..7` w `Gothic.ini`.
- **Do sprawdzenia:** przypisać mikstury i broń na koło, otworzyć koło w walce – wybór, przytrzymanie, puste pole.
- **Test (21.11):** działa.

### 110. Własne przedmioty na pasku szybkiego wyboru (broń, mikstury, rośliny) – ✅ (18.11.2026)
- **Pliki:** `common/game/nbhotbar.h` 🆕, `common/ui/inventorymenu.cpp/.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.cpp`, `common/mainwindow.cpp`
- **Zgłoszenie:** możliwość wkładania broni lub przedmiotu (mikstura, roślina) na pasek szybkiego wyboru.
- **Poprawka:**
  - Na karcie przedmiotu w ekwipunku nowy przycisk nad ZAŁÓŻ/WYRZUĆ: **NA PASEK SZYBKIEGO WYBORU**, a gdy przedmiot już tam jest, **ZDEJMIJ Z PASKA**. Dotyczy broni białej i dystansowej, jedzenia i mikstur; runy i zwoje trafiają tam same, jak w grze.
  - Przedmiot zajmuje pierwszy wolny slot 3–0 (bez runy i bez innego przedmiotu). Na ekranie napis „Na pasku: N” / „Zdjęte z paska: N” / „Pasek pełny”.
  - Dotknięcie slotu (albo klawisz 3–0) używa przedmiotu: broń zakłada, miksturę pije, roślinę je. Na pasku widać model przedmiotu, dopóki bohater go ma.
  - Zapis w `Gothic.ini`, `[MOBILE_HOTBAR] slotN` = nazwa instancji (wspólne dla wszystkich zapisów gry).
- **Akrobatyka (108):** przełącznik zapisuje `MOBILE/acrobatic` i działa jak w ustawieniach dotyku, ale skrypt `RX_ApplyAcrobatic` zakłada przewrotki tylko bohaterowi, który nauczył się akrobatyki (`NPC_TALENT_ACROBAT`) – bez nauki przełącznik nic nie zmienia.
- **Do sprawdzenia:** mikstura i roślina na pasek, dotknięcie – używa; druga broń na pasek – zakłada.
- **Test (21.11):** działa.

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
- **Test (21.11):** ekran śmierci NB działa; zapis przełączników i akrobatyka z menu jeszcze niesprawdzone.
- **Test (22.11):** akrobatyka z menu NB działa (przewrotki po nauce). Zapis pozostałych przełączników jeszcze niesprawdzony.

### 107. Książka/list czytany z ekwipunku pod ramkami ekwipunku – ✅ (18.11.2026)
- **Pliki:** `common/ui/inventorymenu.cpp`
- **Zgłoszenie (zrzut 18.11):** „Czytaj” w ekwipunku – dokument wyświetla się za ekwipunkiem, widać tylko jego skrawek między ramkami (puste pola, karta przedmiotu na wierzchu).
- **Przyczyna:** gdy dokument jest otwarty, ekwipunek jest „przykryty” (`setCovered`) – chowały się tylko modele przedmiotów, a ramki, pola i karta z poprawki 99 dalej się rysowały nad dokumentem.
- **Poprawka:** przykryty ekwipunek nic nie rysuje (ani tła, ani liczb); po zamknięciu dokumentu wraca.
- **Do sprawdzenia:** „Czytaj” na liście/książce w ekwipunku – dokument na całym ekranie, po zamknięciu znowu ekwipunek.
- **Test (21.11):** działa – czytanie z ekwipunku bez problemów.

### 106. Obrażenia w czasie (na ekranie po lewej), obrażenia NB od broni bohatera, obrażenia obszarowe (splash) – ✅ (18.11.2026)
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
- **Test (21.11):** działa – obrażenia NB i obszarowe w walce.

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

### 86. Ustawienie: akrobatyka NB (przewrotki) – ✅ (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp/.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/game/gamescript.cpp/.h`
- **Zgłoszenie:** przełącznik akrobatyki w ustawieniach – przewrotki w lewo/prawo zamiast kroków w bok (na PC klawisz `KEY_ACROBAT`), potrzebne m.in. do szybkiego ataku (85).
- **Jak to działa na PC:** `HandleEvent` przy klawiszu akrobatyki przełącza `RX_AcrobaticActive` i woła `RX_ApplyAcrobatic(hero)` – nakładka animacji `Humans_Acrobatic.MDS`, tylko gdy bohater ma talent akrobatyki (Cassia).
- **Poprawka:** nowy wiersz ustawień „Acrobatics” → `Acrobatics (rolls): ON/OFF` (domyślnie OFF; `Gothic.ini`, `MOBILE`, `acrobatic`, wersja ustawień 8 → 9). Gra co klatkę porównuje ustawienie z `RX_AcrobaticActive` skryptów; gdy się różnią – ustawia zmienną i woła `RX_ApplyAcrobatic` (bez talentu skrypt nakładki nie zakłada). Na PC (bez ekranu dotykowego) bez zmian – decyduje klawisz.
- Przycisk szybkiego ataku (85) dostał numer `Special::QuickAttack = 28` (26 zajął nowy wiersz ustawień).
- **Uwaga:** jeśli akrobatyka jest włączona, zanim bohater się jej nauczy – po nauce przełącz OFF/ON.
- **Do sprawdzenia:** z talentem akrobatyki: ON → kroki w bok z bronią to przewrotki, szybki atak z przewrotką; OFF → zwykłe kroki; w logu `[ctx] acrobatics: on/off`.
- **Test (22.11):** działa – po nauce akrobatyki przewrotki działają.

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

- [ ] Walka kilku postaci z jednym celem: cel „głupieje” (krąży, nie atakuje) – poprawka 152 do testu; np. towarzysz (Blizna) zaatakowany przez dwa zębacze na farmie Oswalda, obóz bandytów (Brago).
