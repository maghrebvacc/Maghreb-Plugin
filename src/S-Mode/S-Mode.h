#pragma once
#include <windows.h>
#include <cstring>
#include "EuroScopePlugIn.h"

namespace SMode
{
    const int kIndicatorVerticalOffset = 4;
    const int kTrackColorSampleRadius = 2;
    const int kMaximumGroundSpeedKnots = 80;
    const char kSModeIndicatorText[] = "S";

    inline bool TryGetRenderedTrackColor(HDC hDC, const POINT& trackPoint, COLORREF* color)
    {
        const COLORREF background =
            GetPixel(hDC, trackPoint.x + kTrackColorSampleRadius + 1, trackPoint.y + kTrackColorSampleRadius + 3);

        for (int radius = 0; radius <= kTrackColorSampleRadius; ++radius) {
            for (int y = trackPoint.y - radius; y <= trackPoint.y + radius; ++y) {
                for (int x = trackPoint.x - radius; x <= trackPoint.x + radius; ++x) {
                    if (x != trackPoint.x - radius && x != trackPoint.x + radius &&
                        y != trackPoint.y - radius && y != trackPoint.y + radius)
                        continue;
                    const COLORREF sample = GetPixel(hDC, x, y);
                    if (sample != CLR_INVALID && sample != GetBkColor(hDC) && sample != background) {
                        *color = sample;
                        return true;
                    }
                }
            }
        }
        return false;
    }

    inline bool HasModeC(const EuroScopePlugIn::CRadarTarget& target)
    {
        const EuroScopePlugIn::CRadarTargetPositionData position = target.GetPosition();
        return position.IsValid() && position.GetTransponderC();
    }

    inline bool IsKnownGroundState(const char* groundState)
    {
        return groundState != nullptr &&
               (lstrcmpiA(groundState, "STUP") == 0 ||
                lstrcmpiA(groundState, "PUSH") == 0 ||
                lstrcmpiA(groundState, "TAXI") == 0 ||
                lstrcmpiA(groundState, "DEPA") == 0 ||
                lstrcmpiA(groundState, "ARR")  == 0 ||
                lstrcmpiA(groundState, "TAXIIN") == 0 ||
                lstrcmpiA(groundState, "PARK") == 0);
    }

    inline bool IsOnGround(EuroScopePlugIn::CPlugIn* plugin, const EuroScopePlugIn::CRadarTarget& target)
    {
        EuroScopePlugIn::CFlightPlan fp = target.GetCorrelatedFlightPlan();
        if (!fp.IsValid() && plugin != nullptr)
            fp = plugin->FlightPlanSelect(target.GetCallsign());
        return fp.IsValid() && IsKnownGroundState(fp.GetGroundState());
    }

    inline bool LooksLikeGroundTraffic(const EuroScopePlugIn::CRadarTarget& target)
    {
        return target.GetGS() <= kMaximumGroundSpeedKnots;
    }

    inline void DrawIndicator(HDC hDC, EuroScopePlugIn::CPlugIn* plugin,
                               EuroScopePlugIn::CRadarTarget& target,
                               EuroScopePlugIn::CRadarScreen* screen)
    {
        if (!HasModeC(target)) return;

        const EuroScopePlugIn::CRadarTargetPositionData position = target.GetPosition();
        if (IsOnGround(plugin, target) || LooksLikeGroundTraffic(target)) return;

        const POINT trackPoint = screen->ConvertCoordFromPositionToPixel(position.GetPosition());

        COLORREF color = {};
        if (!TryGetRenderedTrackColor(hDC, trackPoint, &color))
            color = GetTextColor(hDC);

        HFONT font = CreateFontA(-12, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                                  ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                  DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "./EuroScope.tff");

        HFONT oldFont = font ? static_cast<HFONT>(SelectObject(hDC, font)) : nullptr;
        const COLORREF oldColor = SetTextColor(hDC, color);
        const int oldBkMode = SetBkMode(hDC, TRANSPARENT);

        SIZE textSize = {};
        GetTextExtentPoint32A(hDC, kSModeIndicatorText, (int)std::strlen(kSModeIndicatorText), &textSize);
        TextOutA(hDC, trackPoint.x - (textSize.cx / 2), trackPoint.y + kIndicatorVerticalOffset,
                 kSModeIndicatorText, (int)std::strlen(kSModeIndicatorText));

        SetBkMode(hDC, oldBkMode);
        SetTextColor(hDC, oldColor);
        if (oldFont) SelectObject(hDC, oldFont);
        if (font) DeleteObject(font);
    }

    inline void DrawAllIndicators(HDC hDC, EuroScopePlugIn::CPlugIn* plugin,
                                   EuroScopePlugIn::CRadarScreen* screen)
    {
        for (EuroScopePlugIn::CRadarTarget target = plugin->RadarTargetSelectFirst();
             target.IsValid();
             target = plugin->RadarTargetSelectNext(target))
        {
            DrawIndicator(hDC, plugin, target, screen);
        }
    }
}