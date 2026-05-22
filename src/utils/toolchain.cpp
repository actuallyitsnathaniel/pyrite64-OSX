/**
* @copyright 2026 - Max Bebök
* @license MIT
*/
#include "toolchain.h"
#include "logger.h"
#include "proc.h"
#include "fs.h"
#include "json.hpp"
#include "prop.h"
#include "json.h"
#include "jsonBuilder.h"
#include <filesystem>
#include <atomic>
#include <thread>

namespace
{
  std::atomic_bool installing{false};
}

void Utils::Toolchain::load()
{
  auto doc = Utils::JSON::loadFile(Utils::Proc::getAppDataPath() / "toolchain.json");
  if(doc.is_object() && doc.contains("toolchainPath")) {
    auto path = doc.value("toolchainPath", std::string{});
    if(!path.empty()) state.toolchainPath = fs::path{path};
  }
}

void Utils::Toolchain::save()
{
  std::string json = Utils::JSON::Builder{}
    .set("toolchainPath", state.toolchainPath.string())
    .toString();
  Utils::FS::saveTextFile(Utils::Proc::getAppDataPath() / "toolchain.json", json);
}

void Utils::Toolchain::scan()
{
  //printf("Scanning for toolchain...\n");
  state = {};
  #if defined(_WIN32)

    // Scan "C:\" directories for anything containing "msys"
    state.mingwPath = fs::path{"C:\\msys64"};
    if(!fs::exists(state.mingwPath)) {
      state.mingwPath.clear();
      return;
    }

    if(state.mingwPath.empty())return;

    const char* n64InstEnv = std::getenv("N64_INST");
    // If N64_INST is defined in the system, the user probably already
    // has a working toolchain installation so try to use it.
    state.toolchainPath = (n64InstEnv != nullptr)?
      fs::path{n64InstEnv} : state.mingwPath / "pyrite64-sdk";

    state.hasToolchain = fs::exists(state.toolchainPath / "bin" / "mips64-elf-gcc.exe")
                       && fs::exists(state.toolchainPath / "bin" / "mips64-elf-g++.exe");

    state.hasLibdragon = fs::exists(state.toolchainPath / "bin" / "n64tool.exe")
                       && fs::exists(state.toolchainPath / "bin" / "mkdfs.exe")
                       && fs::exists(state.toolchainPath / "include" / "n64.mk");

    state.hasTiny3d = fs::exists(state.toolchainPath / "bin" / "gltf_to_t3d.exe")
                    && fs::exists(state.toolchainPath / "include" / "t3d.mk")
                    && fs::exists(state.toolchainPath / "mips64-elf" / "include" / "t3d");

  #else
    #if defined(__APPLE__)
    state.hasHomebrew = fs::exists("/opt/homebrew/bin/brew") || fs::exists("/usr/local/bin/brew");
    #endif
    char* n64InstEnv = getenv("N64_INST");
    if(n64InstEnv) {
      state.toolchainPath = fs::path{n64InstEnv};
    }
    // Fall back to previously saved path — needed when launched as .app from Finder,
    // which doesn't source ~/.zshrc so N64_INST env var is absent.
    if(state.toolchainPath.empty()) {
      load();
    }
    if(state.toolchainPath.empty())return;

    state.hasToolchain = fs::exists(state.toolchainPath / "bin" / "mips64-elf-gcc");
    if(!state.hasToolchain)return;

    state.hasLibdragon = fs::exists(state.toolchainPath / "bin" / "n64tool")
                       && fs::exists(state.toolchainPath / "bin" / "mkdfs")
                       && fs::exists(state.toolchainPath / "include" / "n64.mk");
    state.hasTiny3d = fs::exists(state.toolchainPath / "bin" / "gltf_to_t3d")
                    && fs::exists(state.toolchainPath / "include" / "t3d.mk")
                    && fs::exists(state.toolchainPath / "mips64-elf" / "include" / "t3d");

    // Persist the resolved path so future launches find it without the env var
    if(state.hasToolchain) {
      save();
    }
  #endif

  if(state.hasLibdragon && state.hasTiny3d)
  {
    auto rspqHeader = FS::loadTextFile(state.toolchainPath / "mips64-elf" / "include" / "rspq.h");
    auto t3dHeader = FS::loadTextFile(state.toolchainPath / "mips64-elf" / "include" / "t3d" / "t3d.h");

    state.upToDateLibs = true;
    if(!rspqHeader.contains("rspq_block_begin_reuse")) {
      printf("Libdragon out of date, missing 'rspq_block_begin_reuse' in rspq.h\n");
      state.upToDateLibs = false;
    }
    if(!rspqHeader.contains("rspq_block_set_placeholder")) {
      printf("Libdragon out of date, missing 'rspq_block_set_placeholder' in rspq.h\n");
      state.upToDateLibs = false;
    }
    if(!t3dHeader.contains("t3d_state_set_lighting_mode")) {
      printf("tiny3d out of date, missing 't3d_state_set_lighting_mode' in t3d.h\n");
      state.upToDateLibs = false;
    }
  }
}

namespace
{
#if defined(_WIN32)
  void runInstallScript(fs::path mingwPath, bool forceUpdate) {
    // C:\msys64\usr\bin\mintty.exe --hold=error /bin/env MSYSTEM=MINGW64 /bin/bash -l %self_path%mingw_create_env.sh
    auto minttyPath = mingwPath / "usr" / "bin" / "mintty.exe";
    if (!fs::exists(minttyPath)) {
      printf("Error: mintty.exe not found at expected location: %s\n", minttyPath.string().c_str());
      installing.store(false);
      return;
    }

    std::string envVars = "MSYSTEM=MINGW64 ";
    if (forceUpdate) envVars += "FORCE_UPDATE=true ";
    std::string command = minttyPath.string() + " --hold=error /bin/env " + envVars + "/bin/bash -l ";

    fs::path scriptPath = Utils::Proc::getAppResourcePath() / "data" / "scripts" / "mingw_create_env.sh";
    command += "\"" + scriptPath.string() + "\"";

    auto res = Utils::Proc::runSync(command);
    printf("Res: %s : %s\n", command.c_str(), res.c_str());
    installing.store(false);
  }
#elif defined(__APPLE__)
  void runInstallScript(bool forceUpdate) {
    fs::path scriptPath = Utils::Proc::getAppResourcePath() / "data" / "scripts" / "macos_create_env.sh";

    // Make the script executable in case git didn't preserve the bit
    fs::permissions(scriptPath,
      fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec,
      fs::perm_options::add);

    // build-toolchain.sh lives in the source tree under vendored/, not inside the .app bundle.
    // Pass its absolute path as an env var so the script doesn't need to guess via relative paths.
    fs::path toolchainBuilder = Utils::Proc::getAppResourcePath() / "vendored" / "libdragon" / "tools" / "build-toolchain.sh";
    std::string envVars = "TOOLCHAIN_BUILDER=\\\"" + toolchainBuilder.string() + "\\\" ";
    if (forceUpdate) envVars += "FORCE_UPDATE=true ";

    // Open a new Terminal window, bring it to front, then run the script
    std::string osa =
      "osascript"
      " -e 'tell application \"Terminal\" to activate'"
      " -e 'tell application \"Terminal\" to do script \""
      + envVars
      + "bash \\\"" + scriptPath.string() + "\\\"\"'";

    auto res = Utils::Proc::runSync(osa);
    printf("Launched macOS install script: %s\n", res.c_str());
    installing.store(false);
  }
#endif
}

void Utils::Toolchain::install()
{
  if (installing.load()) {
    printf("Toolchain installation already in progress.\n");
    return;
  }

  installing.store(true);
  bool isInstalled = state.hasToolchain && state.hasLibdragon && state.hasTiny3d;
#if defined(_WIN32)
  std::thread installThread(runInstallScript, state.mingwPath, isInstalled);
#elif defined(__APPLE__)
  std::thread installThread(runInstallScript, isInstalled);
#else
  // Linux: no auto-install supported; should not be reachable from UI
  installing.store(false);
  return;
#endif
  installThread.detach();
}

bool Utils::Toolchain::isInstalling()
{
  return installing.load();
}

bool Utils::Toolchain::runCmdSyncLogged(const std::string &cmd)
{
  #if defined(_WIN32)
    auto minttyPath = state.mingwPath / "usr" / "bin" / "bash.exe";
    //std::string command = minttyPath.string() + " --log - -w hide /bin/env MSYSTEM=MINGW64 " + cmd;
    std::string command = minttyPath.string() + " -lc '" + cmd + "'";
    //std::string command = cmd;
    for(char &c : command) {
      if(c == '\\')c = '/';
    }
    return Utils::Proc::runSyncLogged(command);
    //Utils::Logger::logRaw(run_bash(command));
    //return true;
    
  #else
    return Utils::Proc::runSyncLogged(cmd);
  #endif
}
