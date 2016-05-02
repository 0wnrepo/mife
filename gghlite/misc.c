#include "misc.h"

int PRINT_TIMERS;
uint64_t T; /* global var for keeping track of time */

void ggh_printf_v(const gghlite_params_t self, const char *msg, ...) {
  va_list lst;
  if (self->flags & GGHLITE_FLAGS_VERBOSE) {
    va_start(lst, msg);
    vfprintf(stdout, msg, lst);
    va_end(lst);
    fflush(0);
  }
}

void ggh_printf(const gghlite_params_t self, const char *msg, ...) {
  va_list lst;
  if (!(self->flags & GGHLITE_FLAGS_QUIET)) {
    va_start(lst, msg);
    vfprintf(stdout, msg, lst);
    va_end(lst);
    fflush(0);
  }
}
