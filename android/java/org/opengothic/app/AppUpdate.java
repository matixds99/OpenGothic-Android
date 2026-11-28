package org.opengothic.app;

import android.app.DownloadManager;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Environment;
import android.provider.Settings;

import java.io.IOException;

/**
 * Updates the application from its release on GitHub. The APK is downloaded by the system (DownloadManager, a notification of the system),
 * and the system installs it when the player touches it in the list of the downloads: the application does not install anything by itself,
 * so it needs no permission to install packages (a permission that Google Play Protect takes for a sign of an application that updates itself
 * from outside of the store). Android always asks the player to confirm an installation of an application, and the new APK has to be signed
 * with the same key as the installed one: the system refuses it otherwise and says why.
 */
final class AppUpdate {
    private AppUpdate() {
    }

    static final String ACTION = "org.opengothic.app.UPDATE_STATUS";

    /** 0..100 while the APK is downloaded, 101 when it is ready to install, -1 when nothing runs. */
    static volatile int percent = -1;
    /** The reason of the last failure, empty when there was none. */
    static volatile String error = "";

    static boolean running() {
        return percent >= 0;
    }

    /** The system installs the file, not the application: there is nothing to allow. */
    static boolean canInstall(Context c) {
        return true;
    }

    /** The page of the settings where the player allows the installation (not needed now, kept for the callers). */
    static Intent permissionIntent(Context c) {
        return new Intent(Settings.ACTION_SECURITY_SETTINGS);
    }

    /** Starts the download in its own thread; {@code changed} is called (on that thread) at every step. */
    static void start(Context context, UpdateCheck.Release release, Runnable changed) {
        if (running() || release.apkUrl.isEmpty())
            return;
        final Context app = context.getApplicationContext();
        percent = 0;
        error = "";
        new Thread(() -> {
            try {
                download(app, release, changed);
            } catch (Exception | OutOfMemoryError e) {
                error = e.getMessage() != null ? e.getMessage() : e.getClass().getSimpleName();
            } finally {
                percent = -1;
                changed.run();
            }
        }, "app-update").start();
    }

    private static void download(Context app, UpdateCheck.Release release, Runnable changed) throws IOException, InterruptedException {
        DownloadManager dm = (DownloadManager) app.getSystemService(Context.DOWNLOAD_SERVICE);
        if (dm == null)
            throw new IOException("The system has no download manager.");

        String version = release.version == null ? "" : release.version.replaceAll("[^0-9A-Za-z._-]", "");
        DownloadManager.Request request = new DownloadManager.Request(Uri.parse(release.apkUrl))
                .setTitle("OpenGothic " + version)
                .setMimeType("application/vnd.android.package-archive")
                .setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)
                .setDestinationInExternalPublicDir(Environment.DIRECTORY_DOWNLOADS, "OpenGothic-" + version + ".apk");
        request.addRequestHeader("User-Agent", "OpenGothic-Android");
        final long id = dm.enqueue(request);

        final long started = System.currentTimeMillis();
        int last = -1;
        while (true) {
            if (System.currentTimeMillis() - started > 60L * 60 * 1000) {
                dm.remove(id);
                throw new IOException("The download took too long.");
            }
            try (Cursor c = dm.query(new DownloadManager.Query().setFilterById(id))) {
                if (c == null || !c.moveToFirst())
                    throw new IOException("The download was cancelled.");
                int status = c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_STATUS));
                long done = c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_BYTES_DOWNLOADED_SO_FAR));
                long total = c.getLong(c.getColumnIndexOrThrow(DownloadManager.COLUMN_TOTAL_SIZE_BYTES));
                if (total <= 0)
                    total = release.apkSize;
                if (status == DownloadManager.STATUS_SUCCESSFUL)
                    break;
                if (status == DownloadManager.STATUS_FAILED)
                    throw new IOException("The download failed (code " + c.getInt(c.getColumnIndexOrThrow(DownloadManager.COLUMN_REASON)) + ").");
                int p = total > 0 ? (int) Math.min(100, done * 100 / total) : 0;
                if (p != last) {
                    last = p;
                    percent = p;
                    changed.run();
                }
            }
            Thread.sleep(500);
        }

        percent = 101;
        changed.run();
        // The list of the downloads of the system: the player touches the file there and the system installer asks him to confirm.
        // (The notification of the system does the same when the download is done.)
        try {
            app.startActivity(new Intent(DownloadManager.ACTION_VIEW_DOWNLOADS).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK));
        } catch (RuntimeException ignored) {
            // the notification is there
        }
    }
}
