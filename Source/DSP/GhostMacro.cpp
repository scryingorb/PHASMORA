#include "GhostMacro.h"
#include <algorithm>
#include <cmath>

GhostMacro::Scales GhostMacro::map(double normalized) noexcept
{
    const auto g = std::isfinite(normalized) ? std::clamp(normalized, 0.0, 1.0) : 0.0;
    const auto squared = g * g;
    const auto curved = g * std::sqrt(g);
    return { squared, curved, g, curved, squared };
}
bool GhostMacro::setAmount(float normalized) noexcept
{
    if (!std::isfinite(normalized)) return false;
    const auto bounded = static_cast<double>(std::clamp(normalized, 0.0f, 1.0f));
    if (bounded == amount.getTargetValue()) return false;
    amount.setTargetValue(bounded);
    return true;
}
void GhostMacro::prepare(double sampleRate) noexcept
{
    amount.reset(sampleRate, 0.15);
    reset();
}
void GhostMacro::reset() noexcept { amount.setCurrentAndTargetValue(amount.getTargetValue()); }
GhostMacro::Scales GhostMacro::next() noexcept { return map(amount.getNextValue()); }
GhostMacro::Scales GhostMacro::targets() const noexcept { return map(amount.getTargetValue()); }
