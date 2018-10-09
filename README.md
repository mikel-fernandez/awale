# awale
Multi-thread AI for Awale, uses memoization tables.

## Compile
Tested with g++-7. Requires --std=c1z flag. Type `make` to build the binary.
> make

## Play
Run with `./borjilator`, input your movements selecting a space, where the right-most space is 1 and the left-most is 6.

## Configurable parameters
Some parameters are hardcoded into the cpp file. You may modify them

- TIME_BUDGET_HINT: selects the amount of time the AI will play unrestricted, in milliseconds. After the configured time has past, the AI will hurry to finish.
- TIME_BUDGET_MAX: selects the amount of time the AI will play, in milliseconds. After the configured time has past, the AI will almost instantly finish execution.
- MEMOIZE_MAX_SIZE: configures the maximum amount of items to be stored in the memoization table in memory.
