#include "rplloader/rpl_notify.h"

#include "rplloader/rpl_log.h"

#ifdef RPL_HAVE_NOTIFICATIONS
#include <notifications/notifications.h>
#endif

namespace Rpl {
namespace Notify {

static bool s_enabled = true;
static bool s_ready = false;

void Init()
{
#ifdef RPL_HAVE_NOTIFICATIONS
    s_ready = NotificationModule_InitLibrary() == NOTIFICATION_MODULE_RESULT_SUCCESS;
    if (!s_ready)
        Log::Printf(Log::WARN, "NotificationModule unavailable; messages go to the log only");
#else
    s_ready = false;
#endif
}

void Deinit()
{
#ifdef RPL_HAVE_NOTIFICATIONS
    if (s_ready)
        NotificationModule_DeInitLibrary();
#endif
    s_ready = false;
}

void SetEnabled(bool enabled) { s_enabled = enabled; }

void Info(const char* text)
{
    Log::Printf(Log::INFO, "notify: %s", text);
#ifdef RPL_HAVE_NOTIFICATIONS
    if (s_ready && s_enabled)
        NotificationModule_AddInfoNotification(text);
#endif
}

void Error(const char* text)
{
    Log::Printf(Log::ERROR, "notify: %s", text);
#ifdef RPL_HAVE_NOTIFICATIONS
    if (s_ready && s_enabled)
        NotificationModule_AddErrorNotification(text);
#endif
}

} // namespace Notify
} // namespace Rpl
