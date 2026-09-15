#pragma once
#include <stddef.h>
struct AudioCaptureLimits {
  static constexpr bool fits(size_t capacity, size_t recorded, size_t chunk) {
    return recorded<=capacity && chunk<=capacity-recorded;
  }
};
