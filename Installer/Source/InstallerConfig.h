#pragma once

// The names and the version: CMakeLists.txt is the one place that states them, and the build
// writes them into this header (see Installer/InstallerBuildConfig.h.in).
#include <InstallerBuildConfig.h>

namespace dalune::config
{
    inline constexpr const char* productName = DALUNE_PRODUCT_NAME;       // "D.A LUNE"
    inline constexpr const char* companyName = DALUNE_COMPANY_NAME;       // "D.A Production"
    inline constexpr const char* version     = DALUNE_VERSION_STRING;     // "1.2.0"
}
