# Log poprawek – OpenGothic + Gothic II: New Balance

Repo: fork `matixds99/OpenGothic-Android`, gałąź `touch-ui` (na bazie `borsuczyna/opengothic-android`).
Lista plików (co podmienić, co dodać) jest w [docs/NEW_BALANCE.md](docs/NEW_BALANCE.md).

**Status:** ✅ sprawdzone w grze · 🧪 czeka na test · ⏸ wycofane / odłożone

**Numery:** każda poprawka ma swój numer, a komentarze w kodzie (`NB fix N`) i tabela plików odwołują się do tych numerów. Poprawki o tym samym temacie są tu połączone w jeden wpis – wtedy w nagłówku stoi kilka numerów (np. „36–38”). Nowe poprawki dostają kolejny wolny numer (następny: **97**; numery 61, 63–67 są w opisie ognika – poprawka 11).

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

### 73. Gra nie zamyka się po wyjściu do ekranu głównego / przy wiadomości – ✅ (17.11.2026)
- **Pliki:** `patches/tempest-opengothic.patch` (Tempest: `Engine/system/api/androidapi.cpp/.h`, `Engine/gapi/vulkan/vswapchain.cpp/.h`), `patches/README.md`
- **Zgłoszenie (film 00:17):** po wyjściu do ekranu głównego (albo gdy przyjdzie wiadomość i trzeba ją otworzyć) gra się zamyka, po powrocie launcher pokazuje „Gra się zamknęła” ze stosem `MainWindow::tick` ← `MainWindow::render` ← `AndroidApi::implProcessEvents`.
- **Przyczyna:** gdy aplikacja idzie w tło, Android zabiera jej powierzchnię (`APP_CMD_TERM_WINDOW`). Tempest traktował to jak zamknięcie gry: wysyłał „zamknij okno” i kończył pętlę – a gra przy zamykaniu jeszcze rysowała klatkę i padała.
- **Poprawka:** tło to nie koniec gry. Okno Tempesta zostaje, zmienia się tylko natywne okno za nim: w tle nic nie jest rysowane (gra czeka), po powrocie `APP_CMD_INIT_WINDOW` daje nową powierzchnię, a zdarzenie zmiany rozmiaru robi swapchain od nowa na niej (Vulkan: nowa `VkSurfaceKHR`, gdy powierzchnia się zmieniła). Grę zamyka tylko `APP_CMD_DESTROY` (zamknięcie aplikacji).
- W logu `android: the window goes away (background)`.
- **Uwaga:** jeśli w tle zabraknie pamięci, Android i tak może ubić grę (to system, nie błąd gry) – wtedy pomaga zapis przed wyjściem.
- **Do sprawdzenia:** wyjście do ekranu głównego i powrót, otwarcie wiadomości z powiadomienia, przełączenie na inną aplikację i z powrotem; czy obraz wraca i czy dotyk działa.
- **Test (17.11):** gra już się nie zamyka, ale po powrocie był czarny ekran, a ikona w launcherze „świrowała” – poprawione w 74.

### 74. Czarny ekran po powrocie z tła – ✅ (17.11.2026)
- **Pliki:** `common/mainwindow.cpp/.h`, `patches/tempest-opengothic.patch` (Tempest: `androidapi.cpp/.h`, `vswapchain.cpp/.h`), `patches/README.md`
- **Zgłoszenie:** po powrocie do gry czarny ekran, ikona gry w launcherze „świruje” (animacja otwierania się nie kończy). W logu 4× `android: the window goes away (background)` i nic więcej.
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
- **Test (17.11):** gracz woli interakcję jak na PC/z NPC (wyciągnięta broń/runa, skrzynia w fokusie, przycisk akcji) – kłódka usunięta, zostały funkcje pluginu → poprawka 80.

### 80. Otwieranie zamków NB jako interakcja: broń, runa, wytrychy + przycisk akcji – 🧪 (17.11.2026)
- **Pliki:** `common/world/world.cpp`, `common/world/worldobjects.cpp/.h`, `common/game/playercontrol.cpp`, `common/game/gamescript.cpp/.h`; usunięte: przycisk kłódki i `lockpick.png` z 79
- **Zgłoszenie:** zamiast osobnego przycisku – jak w oryginale: wyciągnięta broń albo runa, zamknięta skrzynia w fokusie, przycisk akcji (jak rozmowa z NPC).
- **Poprawka:**
  - **fokus:** z bronią do walki wręcz w ręku (gdy Karl nauczył wyważania, `RX_ForcePickLock`) albo z runą otwierania zamków (zaklęcie `SPL_UnlockChest`) zamknięta skrzynia z zamkiem do wyważenia wchodzi w fokus (fokus walki normalnie nie ma skrzyń), gdy w pobliżu nie ma postaci;
  - **przycisk akcji** na takiej skrzyni: broń → siła (Y, runa na ten czas pominięta), runa → magia (Y, siła na ten czas pominięta), puste ręce z talentem i ≥ 100 zręczności → wytrychy (Shift+Y). Dalej skrypt `HandleEvent` jak na PC: warunki, animacja (rąbanie bronią / rzucanie czaru / `T_PLUNDER`), mana, wytrzymałość, wytrychy, teksty. Bez broni i przy zręczności < 100 – zwykła minigra wytrychów jak dotąd.
- **Do sprawdzenia:** miecz w ręku przy zamkniętej skrzyni → fokus na skrzyni → akcja → rąbanie i otwarcie; runa → animacja czaru, −75 many; puste ręce (100+ zręczności) → zużyte wytrychy; komunikaty przy braku siły/many/wytrychów; w logu `[ctx] lock: …` i `lock opened`.
- **Test (17.11):** magia (runa) działa ✅. Siła i wytrychy – do sprawdzenia na innym zapisie (gracz gra magiem).

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

### 85. Przycisk szybkiego ataku (A/D → W → Ctrl jak na PC) – 🧪 (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp/.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `android/assets/touch-icons/quick_attack.png` (nowa ikona)
- **Zgłoszenie (film 342652):** w NB szybki atak to trzy klawisze po kolei: A albo D (krok w bok, z akrobatyką – przewrotka), W (do przodu) i Ctrl/mysz (atak) – bohater po przewrotce wyprowadza atak w ruchu, który często obala przeciwnika. Na telefonie nie da się tego wcisnąć – potrzebny osobny przycisk (też do testu 83).
- **Poprawka:** nowy przycisk **QUICK** (ikona miecza z liniami ruchu) nad przyciskiem akcji, widoczny z bronią do walki wręcz w ręku; da się go przesunąć/ukryć w edytorze HUD (id `quickattack`). Jedno dotknięcie wysyła klawisze PC po kolei: krok w bok (300 ms; na zmianę w lewo i w prawo), do przodu, atak (po 60 ms, przez 240 ms), puszczenie. Wyjście z gry w trakcie – klawisze są puszczane.
- **Do sprawdzenia:** z mieczem w ręku – przewrotka/krok w bok i atak w ruchu jak na filmie; czy czasy pasują (jeśli atak wchodzi za wcześnie/za późno, zmienię 300/360 ms).

### 98. Szybki atak: cios przycisku miecza szybciej i z resetem pozycji – 🧪 (17.11.2026)
- **Pliki:** `common/game/playercontrol.cpp/.h` (`quickAttack`), `common/world/objects/npc.cpp/.h` (`nbQuickSpeed`), `common/mainwindow.cpp`, `common/utils/androidinput.cpp`
- **Zgłoszenie (zrzut 17.11):** przycisk miecza (między obroną a QUICK) robi to, co trzeba, tylko za wolno. Trzeba go przyspieszyć i resetować pozycję – wtedy będzie szybki atak.
- **Poprawka:** QUICK nie naciska już klawiszy (97). `PlayerControl::quickAttack`: cios do przodu jak z przycisku miecza (`swingSword`), zaraz po trafieniu reset pozycji (`resetAttack`) i od razu następny cios. W tym czasie animacje bohatera są szybsze (zegar modelu × `QuickSpeed`). Liczby w `playercontrol.h`: `QuickHits = 3` ciosy, `QuickTime = 2000` ms limitu, `QuickSpeed = 2.5f` (17.11 gracz: przy 2,5× jest jak na PC; najpierw było 1,5). Po serii prędkość wraca do 1.
- **Do sprawdzenia:** dotknięcie QUICK daje 3 szybkie ciosy do przodu, jeden po drugim. Prędkość i liczbę ciosów da się zmienić w `playercontrol.h`.

### 97. Szybki atak: QUICK = „< STEP” (A) i miecz (W) naciśnięte razem – ❌ (zastąpione w 98) (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp`, `common/mainwindow.cpp`
- **Zgłoszenie (film 342682):** chodzi o animację, nie o przeciwnika. Na filmie gracz ręcznie robi kombinację na ekranie: strzałka w lewo (A) i miecz (W) dotykane razem (ok. 0,25 s, co ok. 1 s), albo strzałka trzymana i miecz klikany. Taką kombinację ma robić QUICK.
- **Poprawka:** QUICK nie wywołuje już `PlayerControl::quickAttack` (96). Jedno dotknięcie naciska dokładnie te przyciski co gracz: „< STEP” (A), po 30 ms ACTION (miecz). ACTION jest puszczany po 300 ms, A po 350 ms (test, czasy wg gracza 17.11). Przy 40/60 ms gra nie widziała W, przy 150/200 ms bohater tylko przesuwał się w lewo bez ataku. Wcześniej 230/260 ms. Szybkie kolejne dotknięcia są ustawiane w kolejce jedno po drugim. Sterowanie Gothic 2 bez zmian, jak przy ręcznym dotykaniu.
- **Do sprawdzenia:** dotknięcie QUICK daje tę samą animację co ręczne dotknięcie strzałki i miecza razem.

### 96. Szybki atak: ciosy w bok wykonywane bezpośrednio, reset zaraz po trafieniu – ❌ (zastąpione w 97) (17.11.2026)
- **Pliki:** `common/game/playercontrol.cpp/.h` (`quickAttack`), `common/mainwindow.cpp`, `common/utils/androidinput.cpp`
- **Zgłoszenie:** po 95 „dramat” – bohater nie oddaje ciosu. Na PC: Ctrl i A trzymane, W klikane – każde kliknięcie W to reset.
- **Przyczyna:** na PC trzymane Ctrl + A silnik czyta co klatkę – po resecie od razu wchodzi kolejny cios w bok. W OpenGothic trzymany klawisz daje jeden cios, a udawanie klawiszy (85–95) nie odtwarzało tego zachowania.
- **Poprawka:** przycisk QUICK nie udaje już klawiszy. `PlayerControl::quickAttack` co klatkę (do 2 s): gdy bohater nie atakuje – cios w bok (strona na zmianę przy kolejnych QUICK), bohater obrócony do celu; gdy cios trafił (po klatce uderzenia) – reset pozycji (`resetAttack`, jak kliknięcie W) i zaraz następny cios; 3 ciosy. Sterowanie Gothic 1 z 94 i reset z 95 nie są już włączane przez QUICK.
- **Do sprawdzenia:** 3 szybkie ciosy w bok pod rząd, każdy trafia.
- **Build (17.11):** `-Werror,-Wunused-private-field` – nieużywane pole `comboRight` w `androidinput.h` usunięte.

### 95. Szybki atak: W „resetuje pozycję” bohatera po ciosie – ❌ (zastąpione w 96) (17.11.2026)
- **Pliki:** `common/world/objects/npc.cpp/.h` (`resetAttack`), `common/game/playercontrol.cpp/.h`, `common/utils/androidinput.cpp`
- **Zgłoszenie:** cała tajemnica: po kombinacji Ctrl/mysz + A/D, W resetuje pozycję bohatera – reszta ciosu jest ucinana i od razu można bić znowu. Tego brakowało we wszystkich próbach.
- **Poprawka:**
  - `Npc::resetAttack()` – gdy cios już trafił (po klatce uderzenia), reszta animacji ataku jest przerywana, bohater od razu w postawie walki;
  - w trakcie QUICK (`quickCombo`) W przy trzymanym Ctrl nie jest ciosem do przodu, tylko takim resetem; W przed uderzeniem czeka na nie do 0,5 s;
  - QUICK: Ctrl trzymany → 3 razy: A/D (cios w bok, 0,08 s) → W (reset, 0,05 s) co 0,42 s → puszczenie Ctrl po 1,35 s.
- **Do sprawdzenia:** szybka seria ciosów w bok, każdy zaraz po poprzednim, jak na PC.

### 94. Szybki atak: klawisze PC czytane jak sterowanie Gothic 1 – ❌ (zastąpione w 96) (17.11.2026)
- **Pliki:** `common/game/playercontrol.cpp/.h` (`quickCombo`), `common/utils/androidinput.cpp`
- **Zgłoszenie:** 93 dalej źle (film 342674). Kombinacja na PC: Ctrl trzymany cały czas, A/D trzymane, W naciskane cały czas.
- **Przyczyna:** na PC przy trzymanym Ctrl klawisze kierunku to ciosy: A/D – cios w bok, W – cios do przodu (kolejne W – kolejne ciosy kombinacji). Ekran dotykowy w OpenGothic ma zawsze sterowanie Gothic 2 (`g2Ctrl`), gdzie bije tylko sam klawisz ataku, a A/D/W przy Ctrl to zwykły ruch – dlatego żadna sekwencja 85–93 nie dawała tego co na PC.
- **Poprawka:** na czas sekwencji QUICK `PlayerControl` czyta klawisze jak w sterowaniu Gothic 1 (`quickCombo`): Ctrl wciśnięty i trzymany → A/D (na zmianę) wciśnięte i trzymane – cios w bok → W stukane co 0,26 s (4 razy) – ciosy do przodu → po 1,35 s puszczenie A/D, po 1,38 s Ctrl; potem znowu sterowanie Gothic 2.
- **Uwaga z filmu 342674:** wytrzymałość spadła do 0/120 – wg gracza w NB brak wytrzymałości tylko spowalnia ruch bohatera (przez to nie wchodzą krytyki), ataków nie blokuje.
- **Do sprawdzenia:** QUICK – cios w bok i seria ciosów do przodu jak na PC.

### 93. Szybki atak: krok w bok i atak w tej samej chwili – ❌ (zastąpione w 94) (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp`
- **Zgłoszenie:** 92 dalej źle; pomysł gracza – krok w lewo/prawo i jednocześnie wyprowadzenie ataku.
- **Poprawka:** QUICK: A/D (na zmianę) i atak (Ctrl) wciśnięte razem → puszczenie ataku po 0,15 s → puszczenie A/D po 0,4 s.

### 92. Szybki atak wg filmu z PC z widocznymi klawiszami – ❌ (zastąpione w 93) (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp`
- **Zgłoszenie (film Video_Project z nakładką klawiszy):** lewy przycisk myszy (atak) trzymany, A albo D razem z W trzymane (bohater idzie do przodu na ukos) i W cały czas klikane – cios za ciosem w ruchu, ok. 1,4 s. Ctrl czy mysz – bez różnicy, chodzi o wyprowadzenie ataku.
- **Poprawka:** QUICK: A/D (na zmianę) i W trzymane razem przez 1,4 s, a w tym czasie atak (Ctrl) stukany co 0,3 s (5 ciosów). Na ekranie dotykowym (sterowanie Gothic 2) cios wychodzi z wciśnięcia klawisza ataku, więc stukany jest atak zamiast W; z trzymanym W to atak w ruchu.
- **Do sprawdzenia:** seria ciosów w ruchu na ukos jak na filmie z PC; tempo (0,3 s) i długość (1,4 s) do dopasowania.

### 91. Szybki atak: powrót do ustawienia z 87 (88–90 cofnięte) – 🧪 (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp`, `common/game/playercontrol.cpp/.h`, `common/world/objects/npc.cpp/.h` (stan z commita `ddfef34`)
- **Zgłoszenie:** 90 też nie działa; najlepiej działało ustawienie z 87 (commit `ddfef34`) – wracamy do niego i będziemy je dopracowywać.
- **Poprawka:** kod szybkiego ataku jak w 87: Ctrl wciśnięty → po 40 ms A/D (na zmianę) i W razem → po 400 ms puszczenie A/D i W → po 420 ms puszczenie Ctrl. Cofnięte: czekanie ciosu do przodu na okno kombinacji (88), czasy z 89 i cios w bok osobnym klawiszem (90).

### 90. Szybki atak: cios w bok osobnym klawiszem, potem atak w ruchu – ❌ (cofnięte w 91) (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp`
- **Zgłoszenie (film 342668):** po 89 jest gorzej – bohater tylko chodzi w bok i do przodu, „brak Ctrl”.
- **Przyczyna:** ekran dotykowy ma sterowanie Gothic 2 (`g2Ctrl`): samo Ctrl to od razu cios do przodu (jeden), a A/D i W z trzymanym Ctrl to tylko ruch – cios w bok jest osobnym klawiszem (`AttackLeft`/`AttackRight`, przyciski „< HIT”/„HIT >”). Ctrl wciśnięty na początku dawał jeden cios, potem 1,2 s chodzenia.
- **Poprawka:** QUICK: cios w bok (na zmianę lewy/prawy, 0,1 s) → po 0,6 s W wciśnięte → po 0,62 s Ctrl → po 1,15 s puszczenie W → po 1,2 s puszczenie Ctrl. Ctrl z trzymanym W to atak w ruchu (`T_1HATTACKMOVE`/`T_2HATTACKMOVE` w biegu), który czeka na okno kombinacji po ciosie w bok (88).
- **Do sprawdzenia:** QUICK – cios w bok, zaraz potem wypad do przodu z ciosem.

### 89. Szybki atak: czasy klawiszy jak na PC – ❌ (cofnięte w 91) (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp`
- **Zgłoszenie:** po 88 ataki do przodu wchodzą, ale za wolno; na PC: Ctrl trzymany cały czas, A/D ok. 0,7 s, potem W ok. 0,5 s.
- **Poprawka:** QUICK: Ctrl wciśnięty → po 40 ms A/D (na zmianę) przez 0,7 s → W przez 0,5 s → puszczenie Ctrl (razem ok. 1,3 s).
- **Do sprawdzenia:** tempo i kolejność ciosów jak na filmie z PC.

### 88. Szybki atak: cios w bok i zaraz cios do przodu (W „resetuje pozycję”) – ❌ (cofnięte w 91) (17.11.2026)
- **Pliki:** `common/game/playercontrol.cpp/.h`, `common/world/objects/npc.cpp/.h` (`swingSword` zwraca, czy cios wszedł), `common/utils/androidinput.cpp`
- **Zgłoszenie (filmy 342665 – telefon, 342652 – PC):** na telefonie po QUICK bohater robi tylko szeroki cios w bok; na PC Ctrl + A/D + W daje cios w bok, a W od razu „resetuje pozycję” – drugi cios do przodu, szybka seria, przeciwnik leci do tyłu.
- **Przyczyna:** silnik oryginału co klatkę czyta trzymane klawisze: W trzymane z Ctrl daje cios do przodu, gdy tylko cios w bok dojdzie do okna kombinacji. W OpenGothic wciśnięcie W to jedna próba – w chwili, gdy cios w bok dopiero się zaczął, próba przepadała.
- **Poprawka:** cios do przodu, który nie wszedł (za wcześnie w trwającym ciosie), czeka do 0,7 s i wchodzi, gdy tylko może – dopóki klawisz ataku jest trzymany (dotyczy też klawiatury, jak w oryginale). QUICK: Ctrl → A/D (krótko) + W → Ctrl trzymany 0,9 s.
- **Do sprawdzenia:** QUICK z mieczem przy przeciwniku – cios w bok i zaraz cios do przodu, jak na filmie z PC.

### 87. Szybki atak: właściwa kolejność klawiszy – ✅ (przywrócone w 91) (17.11.2026)
- **Pliki:** `common/utils/androidinput.cpp`
- **Zgłoszenie:** kolejność na PC jest inna niż podana w 85: najpierw Ctrl (atak) i trzymany, a razem z nim A/D + W naraz.
- **Poprawka:** przycisk QUICK wysyła: Ctrl wciśnięty → po 40 ms A albo D (na zmianę) i W razem → po 400 ms puszczenie A/D i W → po 420 ms puszczenie Ctrl.
- **Do sprawdzenia:** szybki atak jak na filmie; jeśli trzeba dłużej/krócej trzymać – zmienię 400 ms.
- **Test (17.11, filmy 342656/342657):** działa wg gracza – szybkie ataki w walce z Cipherem i towarzyszami, każdy zużywa wytrzymałość jak w NB. Postać grana magiem, więc w zwarciu obrywa – do dopracowania później.

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

### 68. Okno sprzedaży NB (Hanna, Kardif, Halvor, Kreol, Xardas…) – 🧪 (15.11.2026)
- **Pliki:** 🆕 `common/ui/nbsalesmenu.cpp`, 🆕 `common/ui/nbsalesmenu.h`, `common/game/gamescript.cpp`, `common/gothic.h`, `common/mainwindow.cpp`, `common/mainwindow.h`, `common/ui/mobilecontrolsmodel.cpp/.h`, `common/utils/androidinput.h`, `CMakeLists.txt` (`CONFIGURE_DEPENDS` – nowe pliki .cpp trafiają do buildu same)
- **Zgłoszenie:** u Hanny (klejnoty, szkatułki), Kardifa (surowe mięso), Halvora (perły, muszle, kosztowności), Kreola i Xardasa-nekromanty po wybraniu „sprzedaj” nic się nie otwiera.
- **Przyczyna:** NB robi to oknem pluginu: skrypt czyści listę (`RX_Craft_PlayerSales_ClearProductList`), dodaje przedmioty z ceną za sztukę (`RX_Craft_PlayerSales_AddToProductList("ItMi_Ruby", 350)`, 126 wywołań) i otwiera okno (`RX_OpenCraft_PlayerSales(npc)` – UNION_ABI.DLL 0x1017EC40: okno rzemiosła pluginu w trybie 13). Żadnej z tych funkcji nie było.
- **Poprawka:** po zamknięciu dialogu otwiera się własne okno jak okno pluginu (zrzut z PC): lewa i prawa trzecia część ekranu na teksturach pluginu (`AS_BACK_CRAFT_LEFT/RIGHT/RECIPE/RECIPE_DESC/INGRIDIENT/PLUS/MINUS/CREATE.TGA` – bez nich zwykłe panele), środek wolny. Po lewej lista z listy skryptu: pergamin z ceną i ramka „nazwa (xN)” (brak przedmiotu – wyszarzone); po prawej: pergamin z ceną, nazwa, „Maksymalnie: N”, dwa pola „Cena” i „Razem” (tam, gdzie w pluginie są składniki), **−**, ilość, **+**, **Max** i duży przycisk **Sprzedaj**; złoto u góry, X zamyka. Na czas okna znikają przyciski nakładki, paski i cel (po teście 23:32: zębatka, klawiatura i „wstecz” zasłaniały okno). Napisy żółte (zielonej czcionki OpenGothic nie ma). Klawiatura: strzałki, Enter – sprzedaj, Escape – zamknij.
  - sprzedanie zabiera przedmioty i daje złoto według ceny z listy (komunikat „Sprzedano: … (+złoto)”);
  - bez obrazków przedmiotów (w pluginie są na pergaminach) – tam na razie cena.
  - **po teście 23:39:** krzyżyk nie zamykał okna. Dodany przycisk „wstecz” nakładki w prawym górnym rogu (Escape – zamyka okno, nie otwiera menu gry), większe pole dotyku krzyżyka i wpis w logu przy każdym dotknięciu okna (`NB sales window: tap x,y …`) – do sprawdzenia, czemu krzyżyk nie łapał. Pod oknem nie są już rysowane napisy NB (strzały, karma, liczniki), które prześwitywały przez wiersze.
- W logu `[ctx] sales window: N items`.
- **To samo okno w pluginie robi rzemiosło** (`RX_OpenCraft` z `CRAFT_ALCHEMY`, `CRAFT_SMITH`, `CRAFT_RUNE`, `CRAFT_FORCEARMOR`, `CRAFT_BOWS`, `CRAFT_JEWELRY`, `CRAFT_FOOD`, `CRAFT_PIE`, `CRAFT_SOUP_COMPOTE`, `CRAFT_SCROLLS`, `CRAFT_ORE_SMELTING`, `CRAFT_SKINS_ARMOR`, `CRAFT_DEMONRUNE`). Przepisy są tam zaszyte w kodzie pluginu (nie w skryptach) – na razie zostają stare menu dialogowe z poprawki 2; to okno jest pod nie przygotowane.

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

### 71. Okno rzemiosła NB przy wszystkich stołach – 🧪 (16.11.2026)
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
- [ ] Przywołania (`RX_SummonNpcToTarget`, `RX_ResetSummons`) i okna pluginu (ulepszenia przy kowadle, zaklinanie, poker).
- [ ] Ognik: pokazywanie rzadkich roślin/tabliczek, „zbieranie w pomieszczeniach” (poprawka 11).
- [ ] Napisy: „Dzień: …” w lewym górnym rogu, liczniki wampira i Full SnC+ (poprawka 62).
