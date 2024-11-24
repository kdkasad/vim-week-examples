#include <stdbool.h>

#define MAX_ROUTE_LEN (50)
#define ROUTE_TERMINATOR ('.')

/*
 * Checks if a route is a subset of another route.
 *
 * Parameters:
 *   sub - Possible subset route.
 *   super - Superset route.
 *
 * Returns:
 *   true if sub is a subset of super.
 *   false otherwise.
 */

bool route_is_subset(char sub[MAX_ROUTE_LEN], char super[MAX_ROUTE_LEN]) {
  int i = 0;
  int j = 0;
  while ((i < MAX_ROUTE_LEN) && (j < MAX_ROUTE_LEN) &&
         (sub[i] != ROUTE_TERMINATOR) && (super[j] != ROUTE_TERMINATOR)) {
    if (sub[i] == super[j]) {
      i++;
      j++;
    }
    else {
      j++;
    }
  }
  return (i == MAX_ROUTE_LEN) || (sub[i] == ROUTE_TERMINATOR);
} /* route_is_subset() */
