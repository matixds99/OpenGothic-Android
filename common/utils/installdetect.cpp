#include "installdetect.h"

#if defined __ANDROID__
#include <Tempest/Platform>
#include <Tempest/AndroidApi>
#include <Tempest/TextCodec>
#endif

#ifdef __WINDOWS__
#include "windows.h"
#include "shlobj.h"
#include "shlwapi.h"
#endif

#include <cstring>
#include <fstream>
#include "utils/fileutil.h"

InstallDetect::InstallDetect() {
#ifdef __WINDOWS__
  pfiles    = programFiles(false);
  pfilesX86 = programFiles(true);
#endif
#if defined(__OSX__) || defined(__IOS__)
  appDir = applicationSupportDirectory();
#endif
  }

std::u16string InstallDetect::detectG2() {
#if defined(__WINDOWS__)
  auto ret = detectG2(pfiles);
  if(ret.empty())
    ret = detectG2(pfilesX86);
  return ret;
#elif defined(__OSX__) || defined(__IOS__)
  if(FileUtil::exists(appDir))
    return appDir;
  return u"";
#elif defined(__ANDROID__)
  const auto base = Tempest::AndroidApi::externalDataPath();
  if(base.empty())
    return u"";

  std::ifstream in(base/"game.txt");
  std::string name;
  if(!in || !std::getline(in,name))
    return u"";

  if(!name.empty() && name.back()=='\r')
    name.pop_back();

  const std::filesystem::path dir(name);
  std::error_code ec;
  if(!dir.is_absolute() || !std::filesystem::is_directory(dir,ec))
    return u"";

  return Tempest::TextCodec::toUtf16(name);
#else
  return u"";
#endif
  }

#ifdef __ANDROID__
std::string InstallDetect::androidModIni() {
  const auto base = Tempest::AndroidApi::externalDataPath();
  if(base.empty())
    return "";

  std::ifstream in(std::filesystem::path(base)/"game.txt");
  std::string line, ini;
  if(!in || !std::getline(in,line) || !std::getline(in,ini))
    return "";

  if(!ini.empty() && ini.back()=='\r')
    ini.pop_back();

  std::string ret;
  for(char c:ini) {
    const bool ok =
        static_cast<unsigned char>(c)>=0x20 &&
        c!='/' && c!='\\' && c!=':' && c!='*' &&
        c!='?' && c!='"' && c!='<' && c!='>' && c!='|';

    if(!ok)
      return "";

    ret.push_back(c);
    }

  if(ret=="." || ret=="..")
    return "";

  return ret;
  }

std::filesystem::path InstallDetect::androidInternalDataPath() {
  return Tempest::AndroidApi::internalDataPath();
  }
#endif

std::u16string InstallDetect::detectG2(std::u16string pfiles) {
  if(pfiles.empty())
    return u"";

  auto steam = pfiles+u"/Steam/steamapps/common/Gothic II/";
  if(FileUtil::exists(steam))
    return steam;

  auto akela = pfiles+u"/Akella Games/Gothic II/";
  if(FileUtil::exists(akela))
    return akela;

  return u"";
  }

#ifdef __WINDOWS__
std::u16string InstallDetect::programFiles(bool x86) {
  WCHAR path[MAX_PATH]={};

  if(FAILED(SHGetFolderPathW(NULL, (x86 ? CSIDL_PROGRAM_FILESX86 : CSIDL_PROGRAM_FILES), NULL, 0, path)))
    return u"";

  std::u16string ret;
  size_t len=0;
  for(;path[len];++len);

  ret.resize(len);
  std::memcpy(&ret[0],path,len*sizeof(char16_t));
  return ret;
  }
#endif