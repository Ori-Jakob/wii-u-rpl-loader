#include "rplloader/rpl_build.h"

namespace Rpl {

const char* BuildStamp()
{
    return __DATE__ " " __TIME__;
}

} // namespace Rpl
