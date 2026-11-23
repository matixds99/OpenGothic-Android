package org.opengothic.app;

import android.content.Context;
import android.net.Uri;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Restores save files from an archive created by SaveExport. */
final class SaveImport {
    static final int OVERWRITE = 0;
    static final int ADD_SEPARATELY = 1;
    private static final int MAX_ENTRIES = 100000;
    private static final long MAX_TOTAL = 2L * 1024 * 1024 * 1024;
    private static final long MAX_FILE = 256L * 1024 * 1024;
    private static final String INTERNAL = "saves/internal/";
    private static final String EXTERNAL = "saves/external/";

    private SaveImport() {
    }

    static int read(Context context, Uri source, int mode, String gameId) throws IOException {
        File internal = context.getFilesDir().getCanonicalFile();
        File external = context.getExternalFilesDir(null);
        if (external != null)
            external = external.getCanonicalFile();
        Map<String, Set<Integer>> usedSlots = new HashMap<>();
        Set<String> destinations = new HashSet<>();
        byte[] buffer = new byte[64 * 1024];
        int count = 0;
        int entries = 0;
        long total = 0;

        try (InputStream raw = context.getContentResolver().openInputStream(source)) {
            if (raw == null) {
                throw new IOException(Texts.t("Could not open the selected archive.",
                        "Nie można otworzyć wybranego archiwum."));
            }
            try (ZipInputStream zip = new ZipInputStream(new BufferedInputStream(raw))) {
                ZipEntry entry;
                while ((entry = zip.getNextEntry()) != null) {
                    if (++entries > MAX_ENTRIES) {
                        throw new IOException(Texts.t("The archive has too many files.",
                                "Archiwum zawiera zbyt wiele plików."));
                    }
                    File original = target(context, entry, internal, external, gameId);
                    if (original == null) {
                        zip.closeEntry();
                        continue;
                    }

                    File destination = original;
                    if (mode == ADD_SEPARATELY)
                        destination = freeSlot(original, usedSlots, gameId);
                    if (!destinations.add(destination.getCanonicalPath())) {
                        throw new IOException(Texts.t("The archive has the same save twice.",
                                "Archiwum zawiera powtórzone zapisy."));
                    }
                    File parent = destination.getParentFile();
                    if (!parent.exists() && !parent.mkdirs()) {
                        throw new IOException(Texts.t("Could not create the folder of the saves.",
                                "Nie można utworzyć katalogu zapisów."));
                    }

                    total = copy(zip, destination, parent, buffer, total);
                    count++;
                    zip.closeEntry();
                }
            }
        }
        return count;
    }

    /** The file that an entry is restored to, null for an entry that is not a save of this game or is not safe. */
    private static File target(Context context, ZipEntry entry, File internal, File external, String gameId) throws IOException {
        if (entry.isDirectory())
            return null;

        String name = entry.getName().replace('\\', '/');
        File root;
        String relative;
        if (name.startsWith(SaveExport.FOLDER)) {
            // NB fix 180: a save of the folder of a game ("saves/folder/Gothic2/archolos_save_slot_1.sav")
            String rest = name.substring(SaveExport.FOLDER.length());
            int slash = rest.indexOf('/');
            if (slash <= 0 || external == null)
                return null;
            String folder = rest.substring(0, slash);
            if (!isGameFolder(folder))
                return null;
            root = GameRoots.of(context, external, folder).getCanonicalFile();
            relative = rest.substring(slash + 1);
        } else if (name.startsWith(INTERNAL)) {
            root = internal;
            relative = name.substring(INTERNAL.length());
        } else if (name.startsWith(EXTERNAL)) {
            if (external == null)
                return null;
            root = external;
            relative = name.substring(EXTERNAL.length());
        } else {
            return null;
        }

        // NB fix 180: the saves of a folder of a game in the files of the application ("saves/external/Gothic2/...", an export of
        // the launcher of borsuczyna) go to the folder of that game that the player has chosen - the engine works there
        int slash = relative.indexOf('/');
        if (slash > 0 && external != null && isGameFolder(relative.substring(0, slash))) {
            File linked = GameRoots.linked(context, relative.substring(0, slash));
            if (linked != null) {
                root = linked.getCanonicalFile();
                relative = relative.substring(slash + 1);
            }
        }

        String fileName = new File(relative).getName();
        if (!SaveExport.isSave(fileName) || !SaveExport.belongsToGame(fileName, gameId) || !safeRelative(relative))
            return null;

        File original = new File(root, relative).getCanonicalFile();
        if (!original.getPath().startsWith(root.getPath() + File.separator))
            return null;
        return original;
    }

    /** NB fix 180: the name of a folder of a game (Gothic1, Gothic2). */
    private static boolean isGameFolder(String folder) {
        for (String f : SpineActions.GAME_FOLDERS) {
            if (f.equals(folder))
                return true;
        }
        return false;
    }

    /** The first slot next to the file that nothing uses yet, so the saves that are there stay untouched. */
    private static File freeSlot(File original, Map<String, Set<Integer>> usedSlots, String gameId) {
        String parentKey = original.getParent();
        Set<Integer> slots = usedSlots.get(parentKey);
        if (slots == null) {
            slots = scanSlots(original.getParentFile(), gameId);
            usedSlots.put(parentKey, slots);
        }
        int next = 0;
        while (slots.contains(next))
            next++;
        slots.add(next);

        // a quick save that is added separately becomes a normal slot: the quick saves that are there stay untouched
        String originalName = original.getName();
        String prefix = SaveExport.prefixOf(originalName);
        if (prefix == null)
            prefix = "";
        else
            prefix = originalName.substring(0, prefix.length());
        return new File(original.getParentFile(), prefix + "save_slot_" + next + ".sav");
    }

    /**
     * Writes the entry to a temporary file and only then puts it in place of the old save, which is kept until the
     * new one is there. Returns the size of everything that has been read from the archive so far.
     */
    private static long copy(ZipInputStream zip, File destination, File parent, byte[] buffer, long total)
            throws IOException {
        File temp = File.createTempFile("save-import-", ".tmp", parent);
        long fileBytes = 0;
        boolean complete = false;
        try {
            try (BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(temp))) {
                int n;
                while ((n = zip.read(buffer)) != -1) {
                    fileBytes += n;
                    total += n;
                    if (fileBytes > MAX_FILE || total > MAX_TOTAL) {
                        throw new IOException(Texts.t("The archive of the saves is too big.",
                                "Archiwum zapisów jest zbyt duże."));
                    }
                    out.write(buffer, 0, n);
                }
            }
            if (fileBytes == 0) {
                throw new IOException(Texts.t("The archive has an empty save file.",
                        "Archiwum zawiera pusty plik zapisu."));
            }

            File backup = null;
            if (destination.exists()) {
                backup = File.createTempFile("save-backup-", ".tmp", parent);
                if (!backup.delete() || !destination.renameTo(backup)) {
                    backup.delete();
                    throw new IOException(Texts.t("Could not back up the save: ",
                            "Nie można zabezpieczyć istniejącego zapisu: ") + destination.getName());
                }
            }
            if (!temp.renameTo(destination)) {
                if (backup != null)
                    backup.renameTo(destination);
                throw new IOException(Texts.t("Could not write the file: ", "Nie można zapisać pliku: ")
                        + destination.getName());
            }
            if (backup != null)
                backup.delete();
            complete = true;
        } finally {
            if (!complete)
                temp.delete();
        }
        return total;
    }

    private static Set<Integer> scanSlots(File dir, String gameId) {
        Set<Integer> slots = new HashSet<>();
        File[] files = dir != null ? dir.listFiles() : null;
        if (files == null)
            return slots;

        for (File file : files) {
            String name = file.getName().toLowerCase(Locale.ROOT);
            if (!SaveExport.belongsToGame(name, gameId))
                continue;
            try {
                int marker = name.lastIndexOf("save_slot_");
                if (marker >= 0)
                    slots.add(Integer.parseInt(name.substring(marker + 10, name.length() - 4)));
            } catch (RuntimeException ignored) {
                // not a numbered slot
            }
        }
        return slots;
    }

    private static boolean safeRelative(String path) {
        if (path.isEmpty() || path.startsWith("/") || path.indexOf(':') >= 0)
            return false;
        for (String part : path.split("/")) {
            if (part.isEmpty() || "..".equals(part) || ".".equals(part))
                return false;
        }
        return true;
    }
}
