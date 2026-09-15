#pragma once
#include <cmath>

// A window of samples tolerates sensor noise and estimates a constant bias.
// Transient noise penalizes count gently, while active motion and NaNs reset.
struct MouseCalibration {
  static constexpr unsigned target = 80;
  unsigned count = 0;
  unsigned index = 0;
  float samples[3][80] = {};
  float mean[3] = {};
  float m2[3] = {};
  bool ready = false;

  void reset() {
    count = 0;
    index = 0;
    ready = false;
    for (int i = 0; i < 3; ++i) {
      mean[i] = 0.0f;
      m2[i] = 0.0f;
    }
  }

  bool add(float x, float y, float z, float gravitySquared) {
    if (ready) return true;

    // Reject NaNs and non-finite sensor values immediately
    if (!std::isfinite(gravitySquared) || !std::isfinite(x) ||
        !std::isfinite(y) || !std::isfinite(z)) {
      reset();
      return false;
    }

    // Check bounds for stationary device
    if (!(gravitySquared > 0.5f && gravitySquared < 1.7f) ||
        !(x > -40.0f && x < 40.0f && y > -40.0f && y < 40.0f && z > -40.0f && z < 40.0f)) {
      if (count > 0) count--;
      return false;
    }

    samples[0][index] = x;
    samples[1][index] = y;
    samples[2][index] = z;
    index = (index + 1) % target;
    if (count < target) count++;

    if (count < target) return false;

    // Validate variance across all 3 axes
    for (int i = 0; i < 3; ++i) {
      float sum = 0.0f;
      for (unsigned j = 0; j < target; ++j) sum += samples[i][j];
      float m = sum / target;
      float varSum = 0.0f;
      for (unsigned j = 0; j < target; ++j) {
        float d = samples[i][j] - m;
        varSum += d * d;
      }
      m2[i] = varSum;
      if (varSum / target > 4.0f) {
        // Active motion detected: reset count
        count = 0;
        return false;
      }
      mean[i] = m;
    }

    ready = true;
    return true;
  }
};
