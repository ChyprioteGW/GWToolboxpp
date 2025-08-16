#pragma once

#define GWCA_VERSION_MAJOR 1
#define GWCA_VERSION_MINOR 5
#define GWCA_VERSION_PATCH 3
#define GWCA_VERSION_BUILD 0
#define GWCA_VERSION "1.5.3.0"

namespace GWCA {
    constexpr int VersionMajor = GWCA_VERSION_MAJOR;
    constexpr int VersionMinor = GWCA_VERSION_MINOR;
    constexpr int VersionPatch = GWCA_VERSION_PATCH;
    constexpr int VersionBuild = GWCA_VERSION_BUILD;
    constexpr const char* Version = GWCA_VERSION;
}
