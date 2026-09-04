#include <wups.h>

#include "rplloader/rpl_config.h"
#include "rplloader/rpl_loader.h"
#include "rplloader/rpl_log.h"
#include "rplloader/rpl_notify.h"

WUPS_PLUGIN_NAME("RPL Loader");
WUPS_PLUGIN_DESCRIPTION("Loads the .rpl files in sd:/wiiu/rpl-loader/<title id>/ into the running title and installs the hooks they declare");
WUPS_PLUGIN_VERSION("v0.1.0");
WUPS_PLUGIN_AUTHOR("n0ted");
WUPS_PLUGIN_LICENSE("TBD");

// WUPS_PLUGIN_NAME already declares the wut malloc hooks
WUPS_USE_WUT_DEVOPTAB();
WUPS_USE_STORAGE("rpl_loader");

INITIALIZE_PLUGIN()
{
    Rpl::Log::Init();
    Rpl::Notify::Init();
    Rpl::Config::Load();
    Rpl::Config::InitMenu();
    Rpl::Log::Printf(Rpl::Log::INFO, "plugin initialised");
}

DEINITIALIZE_PLUGIN()
{
    Rpl::Notify::Deinit();
    Rpl::Log::Deinit();
}

ON_APPLICATION_START()
{
    Rpl::Log::Init();
    Rpl::Notify::Init();
    Rpl::Loader::OnApplicationStart();
}

ON_RELEASE_FOREGROUND()
{
    Rpl::Loader::OnReleaseForeground();
}

ON_ACQUIRED_FOREGROUND()
{
    Rpl::Loader::OnAcquiredForeground();
}

ON_APPLICATION_REQUESTS_EXIT()
{
    Rpl::Loader::OnApplicationExit();
}

ON_APPLICATION_ENDS()
{
    Rpl::Loader::OnApplicationEnd();
    Rpl::Notify::Deinit();
    Rpl::Log::Deinit();
}
