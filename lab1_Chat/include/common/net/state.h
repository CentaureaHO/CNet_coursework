#ifndef __COMMON_NET_STATE_H__
#define __COMMON_NET_STATE_H__

#include <string>

#define PRO_STATES            \
    X(1, S_BUSY_SERVER)       \
    X(2, S_REJECTED_USERNAME) \
    X(3, S_ACCEPTED_USERNAME)

enum ProState
{
#define X(a, b) b = a,
    PRO_STATES
#undef X
};

char stateCode(ProState state)
{
    switch (state)
    {
#define X(a, b) \
    case b: return a;
        PRO_STATES
#undef X
        default: return 0;
    }
    return 0;
}

#endif