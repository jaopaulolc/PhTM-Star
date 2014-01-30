/**
 * Initialize msr lib variables.
 * MUST BE called only once.
 */
void msrInit();

/**
 * @return: energy counter value
 */
unsigned int msrGetCounter();

/**
 * @return: difference, in Joules, between two energy counter's values.
 */
double msrDiffCounter(unsigned int before,unsigned int after);
