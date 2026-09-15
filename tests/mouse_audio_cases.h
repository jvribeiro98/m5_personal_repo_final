#pragma once
#include <cmath>
#include <limits>
#include "../firmware/MouseCalibration.h"
#include "../firmware/AudioCaptureLimits.h"

void runMouseAudioCases() {
  // Real fallback allocations are smaller than the 30-second PSRAM target.
  for (size_t seconds : {size_t(4), size_t(8)}) {
    const size_t capacity = 16000 * seconds;
    const size_t chunk = 512;
    check(AudioCaptureLimits::fits(capacity, capacity-chunk, chunk),
          "audio chunk ending exactly at the allocated capacity fits");
    check(!AudioCaptureLimits::fits(capacity, capacity-chunk+1, chunk),
          "audio chunk exceeding a 4s/8s fallback buffer is rejected");
    check(!AudioCaptureLimits::fits(capacity, capacity, chunk),
          "full fallback buffer cannot accept another audio chunk");
  }
  check(!AudioCaptureLimits::fits(0, 0, 1), "unallocated audio buffer rejects input");
  check(!AudioCaptureLimits::fits(32, 33, 0), "recorded count beyond capacity is rejected");
  check(!AudioCaptureLimits::fits(std::numeric_limits<size_t>::max(),
        std::numeric_limits<size_t>::max()-2, 8), "audio bounds do not overflow size_t");

  MouseCalibration calibration;
  bool premature = false;
  for (int i=0; i<79; ++i) {
    const float noise = i%2 ? 1.1f : -1.1f;
    premature |= calibration.add(8+noise, -3+noise, 1+noise, 1.0f);
  }
  check(!premature && !calibration.ready && calibration.count==79,
        "noisy stationary calibration waits for all 80 samples");
  check(calibration.add(9.1f, -1.9f, 2.1f, 1.0f) && calibration.ready,
        "stationary sensor bias of 8 with plus/minus 1.1 noise calibrates");
  check(std::fabs(calibration.mean[0]-8.0f)<.001f &&
        std::fabs(calibration.mean[1]+3.0f)<.001f &&
        std::fabs(calibration.mean[2]-1.0f)<.001f,
        "calibration estimates all three stationary biases");
  calibration.reset();
  check(!calibration.ready && calibration.count==0 && calibration.mean[0]==0 &&
        calibration.m2[0]==0 && calibration.mean[1]==0 && calibration.m2[1]==0 &&
        calibration.mean[2]==0 && calibration.m2[2]==0,
        "reset clears readiness and all accumulated statistics");

  bool movingReady = false;
  for (int i=0; i<160; ++i)
    movingReady |= calibration.add(i%2 ? 10.0f : -10.0f, 0, 0, 1.0f);
  check(!movingReady && !calibration.ready && calibration.count==0,
        "alternating motion cannot be mistaken for stationary zero bias");

  const float nan = std::numeric_limits<float>::quiet_NaN();
  for (int axis=0; axis<4; ++axis) {
    calibration.add(0, 0, 0, 1.0f);
    float sample[4] = {0, 0, 0, 1.0f}; sample[axis] = nan;
    check(!calibration.add(sample[0], sample[1], sample[2], sample[3]) &&
          calibration.count==0 && !calibration.ready,
          "NaN in any sensor axis or gravity rejects and resets calibration");
  }
}
