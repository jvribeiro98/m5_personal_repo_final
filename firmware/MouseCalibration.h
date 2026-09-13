#pragma once

// A window of samples tolerates sensor noise and estimates a constant bias.
// No single noisy sample should restart an otherwise stationary calibration.
struct MouseCalibration {
  static constexpr unsigned target = 80;
  unsigned count = 0;
  float mean[3] = {}, m2[3] = {};
  bool ready = false;
  constexpr void reset() {
    count = 0; ready = false;
    for (int i=0;i<3;++i) mean[i]=m2[i]=0;
  }
  constexpr bool add(float x, float y, float z, float gravitySquared) {
    if (ready) return true;
    if (!(gravitySquared > .5f && gravitySquared < 1.7f) ||
        !(x > -40 && x < 40 && y > -40 && y < 40 && z > -40 && z < 40)) {
      reset(); return false;
    }
    float rates[3] = {x,y,z};
    ++count;
    for (int i=0;i<3;++i) {
      float delta = rates[i]-mean[i];
      mean[i] += delta/count;
      m2[i] += delta*(rates[i]-mean[i]);
    }
    if (count < target) return false;
    for (int i=0;i<3;++i) {
      if (m2[i]/count > 4.0f) { reset(); return false; }
    }
    ready = true;
    return true;
  }
};
