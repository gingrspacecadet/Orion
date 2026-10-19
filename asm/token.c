#include "token.h"

#define X(name) [TK_##name] = "TK_"#name,
const char *token_names[] = {
    X_TOKENTYPE
};
#undef X