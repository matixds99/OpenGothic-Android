#pragma once

#include <cstdint>
#include <string>

/// NB fix 199-203: the log of the game for the reports of the player.
///  - every line goes through observe(): the problems (missing functions of the plugin, script errors, missing files,
///    characters that are stuck, crashes) are counted for the summary;
///  - filter() keeps the noisy lines out of log.txt by the level of the log (the setting "Log"): quiet, normal, full;
///  - mark() writes a marker with the time, the place and the characters around the hero (the button MARK of the HUD);
///  - summary() / writeSummary(): the list of the problems with their counts, at the end of the log.
namespace NbLog {
  enum Level : int {
    Quiet  = 0,   // no AI trace, no damage lines, the memory once a minute
    Normal = 1,   // the default: one line for a blow, the memory every 30 s
    Full   = 2,   // everything, as before
    };

  void        setLevel(int level);
  int         level();

  /// Counts the problems of a line. Thread-safe.
  void        observe(bool error, const char* text);
  /// False: the line is not written to log.txt (it is counted anyway, and kept for the report of a crash)
  bool        filter(const char* text);

  /// The summary of the problems so far, lines without the end of the line
  std::string summary(bool crash = false);   // crash: does not wait for the lock (the handler of a signal)
  /// The summary into the log (why: "exit", "background", "crash"...)
  void        writeSummary(const char* why);

  /// The lines that the level has kept out of log.txt, the last ones (for the report of a crash)
  std::string recentHidden(bool crash = false);

  /// The marker of the player: writes "[mark N]" with the world, the time, the position of the hero, his target and the
  /// characters near him; returns N
  int         mark();

  /// The crash of the previous start (crash.log newer than the last one seen) goes into the log at the start
  void        reportPreviousCrash();
  }
