#pragma once

#include <QColor>

#include "ibs/core/model/Project.hpp"

namespace ibs::gui::palette {

inline const QColor kBackground{0x1e, 0x20, 0x24};
inline const QColor kRowAlternate{0x23, 0x26, 0x2b};
inline const QColor kRuler{0x2b, 0x2e, 0x34};
inline const QColor kText{0xd8, 0xdb, 0xe0};
inline const QColor kDimText{0x8a, 0x90, 0x99};
inline const QColor kBarLine{0x6a, 0x70, 0x7a};
inline const QColor kBeatLine{0x3e, 0x43, 0x4b};
inline const QColor kSubdivisionLine{0x2c, 0x30, 0x36};
inline const QColor kWaveform{0x8f, 0xc1, 0x8f};
inline const QColor kSelection{0xff, 0xff, 0xff};
inline const QColor kPlayhead{0xff, 0x55, 0x55};
inline const QColor kCursor{0xff, 0xd4, 0x4a};
inline const QColor kAutoCut{0x4a, 0xa3, 0xff};
inline const QColor kManualCut{0xff, 0xa9, 0x4a};
inline const QColor kImportedCut{0x9a, 0x9a, 0x9a};
inline const QColor kClippedCut{0xff, 0x4a, 0x4a};
inline const QColor kMissing{0xc0, 0x50, 0x50};

inline QColor cutColor(core::CutOrigin origin, bool attackClipped) {
    if (attackClipped) {
        return kClippedCut;
    }
    switch (origin) {
        case core::CutOrigin::Auto: return kAutoCut;
        case core::CutOrigin::Manual: return kManualCut;
        case core::CutOrigin::Imported: return kImportedCut;
    }
    return kImportedCut;
}

// Why not 配置ごとに色を変える: 同じ素材を何度も置いたとき、どれが同じフレーズかを一目で分かるようにする（SPEC §6.2）
inline QColor assetColor(core::AssetId asset) {
    const int hue = static_cast<int>((asset.value * 137u) % 360u);
    return QColor::fromHsv(hue, 110, 150);
}

}  // namespace ibs::gui::palette
