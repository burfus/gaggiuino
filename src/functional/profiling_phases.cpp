#include "profiling_phases.h"

//----------------------------------------------------------------------//
//------------------------------ Phase ---------------------------------//
//----------------------------------------------------------------------//
float Phase::getTarget(unsigned long timeInPhase) const {
  long transitionTime = fmax(0L, target.time > 0L ? target.time : stopConditions.time);
  return mapRange(timeInPhase, 0.f, transitionTime, target.start, target.end, 1, target.curve);
}

float Phase::getRestriction() const {
  return restriction;
}

bool Phase::isStopConditionReached(SensorState& currentState, unsigned long timeInShot, ShotSnapshot stateAtPhaseStart) const {
  return stopConditions.isReached(currentState, timeInShot, stateAtPhaseStart);
}

//----------------------------------------------------------------------//
//-------------------------- StopConditions ----------------------------//
//----------------------------------------------------------------------//
bool PhaseStopConditions::isReached(SensorState& state, long timeInShot, ShotSnapshot stateAtPhaseStart) const {
  float flow = state.weight > 0.4f ? state.weightFlow : state.smoothedPumpFlow;
  float stopDelta = flow / 2.f;

  return (time >= 0L && timeInShot - stateAtPhaseStart.timeInShot >= time) ||
    (weight > 0.f && state.shotWeight > weight - stopDelta) ||
    (pressureAbove > 0.f && state.pressure > pressureAbove) ||
    (pressureBelow > 0.f && state.pressure < pressureBelow) ||
    (waterPumpedInPhase > 0.f && state.waterPumped - stateAtPhaseStart.waterPumped > waterPumpedInPhase - stopDelta) ||
    (flowAbove > 0.f && state.smoothedPumpFlow > flowAbove) ||
    (flowBelow > 0.f && state.smoothedPumpFlow < flowBelow);
}

bool GlobalStopConditions::isReached(SensorState& state, long timeInShot) {
  float flow = state.weight > 0.4f ? state.weightFlow : state.smoothedPumpFlow;
  float stopDelta = flow / 2.f;

  return (weight > 0.f && state.shotWeight > weight - stopDelta) ||
    (waterPumped > 0.f && state.waterPumped > waterPumped) ||
    (time >= 0L && timeInShot >= time);
}

//----------------------------------------------------------------------//
//--------------------------- CurrentPhase -----------------------------//
//----------------------------------------------------------------------//
CurrentPhase::CurrentPhase(int index, const Phase& phase, unsigned long timeInPhase) : index(index), phase{ &phase }, timeInPhase(timeInPhase) {}
CurrentPhase::CurrentPhase(const CurrentPhase& currentPhase) : index(currentPhase.index), phase{ currentPhase.phase }, timeInPhase(currentPhase.timeInPhase) {}

Phase CurrentPhase::getPhase() { return *phase; }

PHASE_TYPE CurrentPhase::getType() { return phase->type; }

int CurrentPhase::getIndex() { return index; }

long CurrentPhase::getTimeInPhase() { return timeInPhase; }

float CurrentPhase::getTarget() { return phase->getTarget(timeInPhase); }

float CurrentPhase::getRestriction() { return phase->getRestriction(); }

void CurrentPhase::update(int index, const Phase& phase, unsigned long timeInPhase) {
  CurrentPhase::index = index;
  CurrentPhase::phase = &phase;
  CurrentPhase::timeInPhase = timeInPhase;
}

//----------------------------------------------------------------------//
//-------------------------- PhaseProfiler -----------------------------//
//----------------------------------------------------------------------//

PhaseProfiler::PhaseProfiler(Phases& phases) : phases(phases) {}

void PhaseProfiler::updatePhase(long timeInShot, SensorState& state) {
  short phaseIdx = currentPhaseIdx;
  unsigned long timeInPhase = timeInShot - phaseChangedSnapshot.timeInShot;

  if (phaseIdx >= phases.count || globalStopConditions.isReached(state, timeInShot)) {
    currentPhaseIdx = phases.count;
    phaseIdx = phases.count - 1;
    currentPhase.update(phaseIdx, phases.phases[phaseIdx], timeInPhase);
    return;
  }

  if (!phases.phases[phaseIdx].isStopConditionReached(state, timeInShot, phaseChangedSnapshot)) {
    currentPhase.update(phaseIdx, phases.phases[phaseIdx], timeInPhase);
    return;
  }

  currentPhaseIdx += 1;
  phaseChangedSnapshot = ShotSnapshot{timeInShot, state.pressure, state.pumpFlow, state.temperature, state.shotWeight, state.waterPumped};
  updatePhase(timeInShot, state);
}

// Gets the profiling phase we should be in based on the timeInShot and the Sensors state
CurrentPhase& PhaseProfiler::getCurrentPhase() {
  return currentPhase;
}

bool PhaseProfiler::isFinished() {
  return currentPhaseIdx >= phases.count;
}

void PhaseProfiler::reset() {
  currentPhaseIdx = 0;
  phaseChangedSnapshot = ShotSnapshot{0, 0, 0, 0, 0, 0};
  currentPhase.update(0, phases.phases[0], 0);
}

void PhaseProfiler::updateGlobalStopConditions(float weight, long time, float waterVolume) {
  globalStopConditions.weight = weight;
  globalStopConditions.time = time;
  globalStopConditions.waterPumped = waterVolume;
}
