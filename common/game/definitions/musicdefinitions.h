#pragma once

#include <zenkit/DaedalusVm.hh>
#include <zenkit/addon/daedalus.hh>

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class MusicDefinitions final {
  public:
    MusicDefinitions();
    ~MusicDefinitions();

    const zenkit::IMusicTheme* operator[](std::string_view name) const;
    struct RecordedTrack {
      zenkit::IMusicTheme theme = {};
      int32_t loopStart = 0;
      int32_t fadeOut = 0;
      int32_t fadeIn = 0;
      };
    const RecordedTrack* recordedTrack(std::string_view file) const;
    const RecordedTrack* recordedTrack(uint32_t instance) const;
    bool hasRecordedMusic() const { return !recorded.empty(); }
    /// NB fix 183: a theme of BassMusicController (System/Autorun/BASSMUSICCONTROLLER_*.D): mp3 files played one after
    /// another. The theme carries the original list in file, the key of this playlist
    struct BassTrack {
      std::vector<std::string> files;
      float vol = 1.f;
      };
    const BassTrack* bassTrack(std::string_view file) const;

  private:
    std::unique_ptr<zenkit::DaedalusVm>  vm;
    std::vector<std::shared_ptr<zenkit::IMusicTheme>> themes;
    void loadRecordedMusic();
    std::unordered_map<std::string,RecordedTrack> recorded;
    std::unordered_map<uint32_t,std::string> recordedIds;
    std::unordered_map<std::string,zenkit::IMusicTheme> recordedThemes;
    void loadBassMusic();
    void parseBassScript(std::string_view src);
    std::unordered_map<std::string,BassTrack> bass;
    std::unordered_map<std::string,zenkit::IMusicTheme> bassThemes;
    std::unordered_set<std::string> bassMissing;
  };
