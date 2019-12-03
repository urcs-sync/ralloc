/*
 * Copyright (C) 2019 Ricardo Leite. All rights reserved.
 * Licenced under the MIT licence. See COPYING file in the project root for details.
 */

#include "tcache.h"

// thread cache, uses tsd/tls
// one cache per thread
__thread TCacheBin TCache[MAX_SZ_IDX];

