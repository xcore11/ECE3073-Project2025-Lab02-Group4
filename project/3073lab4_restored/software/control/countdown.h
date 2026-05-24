#ifndef COUNTDOWN_H_
#define COUNTDOWN_H_

#include <stdint.h>

void countdown_init(void);
<<<<<<< Updated upstream
int run_synchronized_countdown(void); // Returns 0 on success, 1 on abort
=======
int run_synchronized_countdown(void); // Changed to int: Returns 0 on success, 1 on abort
>>>>>>> Stashed changes
int is_countdown_finished(void);

#endif /* COUNTDOWN_H_ */
