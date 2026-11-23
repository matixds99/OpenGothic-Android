#pragma once

#include <zenkit/addon/daedalus.hh>

#include <Tempest/SoundDevice>
#include <Tempest/SoundEffect>
#include <optional>

class GameMusic final {
  public:
    GameMusic();
    GameMusic(const GameMusic&)=delete;
    ~GameMusic();

    static GameMusic& inst();

    enum Music : uint8_t {
      SysLoading
      };

    enum Tags : uint8_t {
      Day = 0,
      Ngt = 1<<0,

      Std = 0,
      Fgt = 1<<1,
      Thr = 1<<2
      };

    static Tags mkTags(Tags daytime,Tags mode);

    void      setEnabled(bool e);
    bool      isEnabled() const;
    // NB fix 116: RX_MusicOff/RX_MusicOn of New Balance (the music of the bosses, the concert). The mute stays until the
    // scripts end it: the options written by the scripts (bCanSaveGame at the start of a fight) do not turn the music
    // of the game on again
    void      setScriptMute(bool m);
    bool      isScriptMuted() const { return scriptMute; }
    /// NB fix 174: no music while a game is loaded (after a death too): the synthesizer of the music did not keep up with the
    /// thread of the loading and the sound stuttered. As the original, the loading screen is silent
    void      setLoadMute(bool m);
    void      setMusic(Music m);
    void      setMusic(const zenkit::IMusicTheme &theme, Tags t);
    void      clearZoneMusic();
    void      stopMusic();
    void      overrideTrack(uint32_t instance);
    void      clearTrackOverride();

  private:
    struct MusicProvider;
    struct OpenGothicMusicProvider;
    struct GothicKitMusicProvider;
    struct StreamMusic;
    struct RecordedMusic;
    struct BassMusic;

    void      setupSettings();
    void      playCurrent();
    void      applyEnabled();
    bool      isOn() const { return enabled && !scriptMute && !loadMute; }

    static GameMusic* instance;

    struct {
      zenkit::IMusicTheme theme = {};
      Tags                tags  = Tags::Std;
      } currentMusic;

    int                  provider = -1;
    Tempest::SoundDevice device;
    Tempest::SoundEffect sound;
    MusicProvider*       impl = nullptr;
    bool                 enabled = true;
    bool                 scriptMute = false;   // NB fix 116
    bool                 loadMute   = false;   // NB fix 174
    bool                 playing    = true;    // the music really plays (enabled and not muted)
    int                  lastMusicSetting = -1;
    bool                 zoneMusicActive = true;
    Tempest::SoundEffect recordedSound, outgoingSound;
    StreamMusic*         recordedImpl = nullptr;   // CoM ogg or the mp3 of BassMusicController (NB fix 183)
    std::string          recordedFile;
    int32_t              recordedFadeOut = 0;
    std::optional<zenkit::IMusicTheme> overrideTheme;
  };
