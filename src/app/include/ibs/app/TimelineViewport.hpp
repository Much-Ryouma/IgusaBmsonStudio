#pragma once

#include "ibs/core/model/TimingData.hpp"

namespace ibs::app {

enum class SnapMode { Grid, Fine, Off };

class TimelineViewport {
public:
    TimelineViewport(double pulsesPerPixel = 4.0, double minimumPulsesPerPixel = 0.05,
                     double maximumPulsesPerPixel = 512.0);

    [[nodiscard]] double pulsesPerPixel() const noexcept { return pulsesPerPixel_; }
    [[nodiscard]] double originPulse() const noexcept { return originPulse_; }

    [[nodiscard]] double xOf(double pulse) const noexcept;
    [[nodiscard]] double pulseAt(double x) const noexcept;

    void setOrigin(double pulse);
    void scrollByPixels(double pixels);
    void setPulsesPerPixel(double pulsesPerPixel);
    void zoomAround(double x, double factor);
    void fit(double fromPulse, double toPulse, double widthPixels);

    [[nodiscard]] int gridDivisionsFor(int resolution, double minimumSpacingPixels, int finestDivisions) const;

private:
    double pulsesPerPixel_;
    double minimumPulsesPerPixel_;
    double maximumPulsesPerPixel_;
    double originPulse_ = 0.0;
};

[[nodiscard]] core::Pulse snapPulse(const core::TimingData& timing, double pulse, int divisionsPerWholeNote,
                                    SnapMode mode);

}  // namespace ibs::app
