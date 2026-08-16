#pragma once

// Boot-time recovery for an incorrectly wired PSX controller bus.
// Tests permutations of GPIO 4,5,6,7,8 and keeps the first valid response.
bool psxPinSweep();
