// Gothic II: New Balance - the wisp of Vatras.
// In NB the Union plugin (UNION_ABI.DLL) calls the wisp (WISP_DETECTOR) with L-Shift+F5 when the hero has the amulet
// of the wisp; the scripts have only the character and its AI (ZS_MM_Rtn_Summoned). The mod calls the wisp; the search
// for the items and bringing them (the plugin in NB) is in the engine (NB fix 61, common/game/gamescript.cpp).
//
// NB fix 117-119: the wisp lives for one session of the game. The first world after the game is started (a loaded save
// or a new game) has no wisp - the end of the session has sent it away, also the one in the save. When the player calls
// it (Shift+F5, the tab "Ognik", the command "ognik"), it stays with the next loads and changes of the world until the
// game is closed or the player sends it away. There is never more than one (there were 4 after a few saves).

const WISP     = "WISP_DETECTOR";
const AMULETS  = ["ITAM_ADDON_WISPDETECTORNEW", "ITAM_ADDON_WISPDETECTOR"];

let wisp     = null;    // the character of the wisp that this mod has called
let summoned = false;   // the player has called the wisp in this session (not kept in a save, nor between sessions)
let started  = false;   // the first world of this session has come
let shift    = false;

function hasAmulet() {
    return AMULETS.some(a => player.valid && player.hasItem(a));
}

// every wisp of this world, the dead ones too
function allWisps() {
    if (typeof npcs.all === "function")
        return npcs.all(WISP).filter(w => w && w.valid);
    const w = npcs.find(WISP);
    return w && w.valid ? [w] : [];
}

// sends away every wisp but `keep`; returns how many went
function removeWisps(keep) {
    let n = 0;
    for (const w of allWisps()) {
        if (keep && w.id === keep.id)
            continue;
        if (w.remove())
            ++n;
    }
    return n;
}

function findWisp() {
    if (wisp && wisp.valid && !wisp.dead)
        return wisp;
    wisp = allWisps().find(w => !w.dead) || null;
    return wisp;
}

function spawnWisp(quiet) {
    if (!player.valid || player.dead)
        return false;
    if (!hasAmulet()) {
        if (!quiet) screen.print("Nie masz amuletu wędrującego ognika.");
        return false;
    }
    removeWisps(null);   // never two
    // a bit in front of the hero, at the height of the head
    const r = (player.rotation || 0) * Math.PI / 180;
    const p = player.position;
    const x = p.x + Math.sin(r) * 120;
    const z = p.z + Math.cos(r) * 120;
    const w = npcs.spawn(WISP, x, p.y + 60, z, player.rotation || 0);
    if (!w) {
        if (!quiet) screen.print("Nie udało się przywołać ognika.");
        return false;
    }
    wisp     = w;
    summoned = true;
    if (!quiet) screen.print("Ognik przywołany");
    return true;
}

function dismissWisp(quiet) {
    removeWisps(null);
    wisp     = null;
    summoned = false;
    if (!quiet) screen.print("Ognik odesłany");
}

function toggleWisp() {
    if (findWisp()) dismissWisp(false);
    else spawnWisp(false);
}

// --- L-Shift+F5, as in the plugin (a keyboard connected to the phone, or the computer)
events.on("key:down", ({ key }) => {
    if (key === "Shift") { shift = true; return; }
    if (key === "F5" && (shift || input.isDown("Shift")))
        toggleWisp();
});
events.on("key:up", ({ key }) => {
    if (key === "Shift") shift = false;
});

// --- the phone: a tab in the menu of the mods
ui.tab("Ognik", () => {
    const on = !!findWisp();
    ui.textWrapped("Wędrujący ognik od Vatrasa. Szuka przedmiotów i świeci w ciemności.");
    ui.spacing();
    if (!hasAmulet())
        ui.textColored("#d08040", "Potrzebny amulet wędrującego ognika.");
    ui.text(on ? "Ognik: przywołany" : "Ognik: odesłany");
    if (ui.button(on ? "Odeślij ognika" : "Przywołaj ognika", -1, ui.fontSize() * 2.5))
        toggleWisp();
    ui.spacing();
    ui.textDisabled("Klawiatura: Shift+F5. Konsola: ognik, ognik on, ognik off");
});

// "ognik" switches, "ognik on" / "ognik off" call or send away (for the other scripts: mod.command)
console.registerCommand("ognik", (cmd) => {
    const a = String((cmd && cmd.args && cmd.args[0]) || "").trim().toLowerCase();
    if (a === "on" || a === "1")       { if (!findWisp()) spawnWisp(false); }
    else if (a === "off" || a === "0") dismissWisp(false);
    else                               toggleWisp();
}, "ognik [on|off] - przywołuje albo odsyła wędrującego ognika");

// --- NB fix 117-119: a new object of the world (a new game, a loaded save, a change of the world).
// The first one of the session: no wisp. Later: as the player has chosen in this session - called: one wisp (the one of
// the save, or a new one next to the hero), sent away: none
events.on("world:load", () => {
    wisp = null;
    if (!started) {
        started  = true;
        summoned = false;
    }
    if (!summoned) {
        const removed = removeWisps(null);
        if (removed > 0)
            console.log("ognik: odesłano " + removed);
        return;
    }
    const kept    = allWisps().find(w => !w.dead) || null;
    const removed = removeWisps(kept);
    if (removed > 0)
        console.log("ognik: odesłano nadmiarowe: " + removed);
    wisp = kept;
    if (!kept)
        setTimeout(() => { if (summoned && !findWisp()) spawnWisp(true); }, 1500);
});

// the wisp stays near: if it is lost far away (a teleport), it is called again next to the hero; a second wisp goes
setInterval(() => {
    const w = findWisp();
    if (!w || !player.valid) return;
    removeWisps(w);
    const a = w.position, b = player.position;
    const dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    if (dx * dx + dy * dy + dz * dz > 4000 * 4000) {
        w.remove();
        wisp = null;
        spawnWisp(true);
    }
}, 3000);
