#include "core/status.h"

_Post_satisfies_(return != ERROR_SUCCESS)
LSTATUS VReg__FailureStatus(const LSTATUS status)
{
    _Analysis_assume_(status != ERROR_SUCCESS);
    return status;
}
