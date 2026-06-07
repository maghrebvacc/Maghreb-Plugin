#include <windows.h>
#include <string>
#include <objidl.h>   
#include <gdiplus.h>
#include "EuroScopePlugIn.h"
#include "TopSkyFunctions.h"
#include "S-Mode.h"
#include "IndraApcInterop.h"
#include <map>

#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <set>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(linker, "/EXPORT:EuroScopePlugInInit=_EuroScopePlugInInit")
#pragma comment(linker, "/EXPORT:EuroScopePlugInExit=_EuroScopePlugInExit")

using namespace EuroScopePlugIn;
using namespace Gdiplus;
using namespace std;

int state = 0; 

#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;
HINSTANCE g_hDllInstance = nullptr;

std::string HttpGet(const std::string& url)
{
    std::wstring wurl(url.begin(), url.end());

    URL_COMPONENTS uc{};
    uc.dwStructSize = sizeof(uc);

    wchar_t host[256] = {};
    wchar_t path[2048] = {};

    uc.lpszHostName = host;
    uc.dwHostNameLength = _countof(host);

    uc.lpszUrlPath = path;
    uc.dwUrlPathLength = _countof(path);

    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc))
        return "";

    HINTERNET hSession = WinHttpOpen(
        L"MaghrebPlugin/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!hSession)
        return "";

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        host,
        uc.nPort,
        0);

    if (!hConnect)
    {
        WinHttpCloseHandle(hSession);
        return "";
    }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS)
        ? WINHTTP_FLAG_SECURE
        : 0;

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        path,
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags);

    if (!hRequest)
    {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    BOOL success =
        WinHttpSendRequest(
            hRequest,
            WINHTTP_NO_ADDITIONAL_HEADERS,
            0,
            WINHTTP_NO_REQUEST_DATA,
            0,
            0,
            0)
        &&
        WinHttpReceiveResponse(
            hRequest,
            nullptr);

    if (!success)
    {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return "";
    }

    std::string response;

    DWORD available = 0;

    do
    {
        available = 0;

        if (!WinHttpQueryDataAvailable(hRequest, &available))
            break;

        if (available == 0)
            break;

        std::vector<char> buffer(available);

        DWORD downloaded = 0;

        if (!WinHttpReadData(
            hRequest,
            buffer.data(),
            available,
            &downloaded))
        {
            break;
        }

        response.append(buffer.data(), downloaded);

    } while (available > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return response;
}

json FetchJsonDataStars(
    const std::string& airport,
    const std::string& runway,
    const std::string& fix)
{
    std::string url =
        "https://raw.githubusercontent.com/"
        "maghrebvacc/Maghreb-Plugin/main/build/stars/" +
        airport + ".json";

    std::string response = HttpGet(url);

    if (response.empty())
        return json();

    json data = json::parse(response, nullptr, false);

    if (data.is_discarded())
        return json();

    if (!data.contains(runway))
        return json();

    if (!data[runway].contains(fix))
        return json();

    return data[runway][fix];
}

void FillRoundedRect(Graphics& g, Brush& brush, int x, int y, int w, int h, int radius)
{
    GraphicsPath path;
    path.AddArc(x,                   y,                   radius * 2, radius * 2, 180, 90);
    path.AddArc(x + w - radius * 2,  y,                   radius * 2, radius * 2, 270, 90);
    path.AddArc(x + w - radius * 2,  y + h - radius * 2,  radius * 2, radius * 2,   0, 90);
    path.AddArc(x,                   y + h - radius * 2,  radius * 2, radius * 2,  90, 90);
    path.CloseFigure();
    g.FillPath(&brush, &path);
}

void FindCorrectStar(char Airport, char Fix) {
}

ULONG_PTR g_gdiplusToken = 0;
int Auto_Star = 0;
int Coordination_cross = 0;
int FMP_Panel = 0;
int S_mode = 0;
int Indra_Panel = 1;

int Indra_sent_drawn = 0;

const int OBJECT_AUTOSTAR = 2;
const int OBJECT_COORDINATION_CROSS = 3;
const int OBJECT_FMP_PANEL = 4;
const int OBJECT_S_MODE_PANEL = 5;
const int OBJECT_INDRA_PANEL = 6;

const int STAR_TAG_ID = 100;
const int STAR_TAG_FUNCTION = 101;

const int RTE_CHECKER_ID = 110;
const int RTE_CHECKER_FUNCTION = 111;

std::map<std::string, int> starState;
std::map<std::string, std::string> CorrectStar;

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_hDllInstance = hModule;
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        GdiplusShutdown(g_gdiplusToken);
    }
    return TRUE;
}

const int OBJECT_CIRCLE = 10;

class MyRadarScreen : public CRadarScreen
{
    Gdiplus::Image* m_pImage = nullptr;
    int m_X = -1;
    int m_Y = -1;
    int m_Set_draw = 0;
    int m_traffic_load = 0;
    int m_indra_draw = 1;

public:
    MyRadarScreen()
    {
        wchar_t dllPath[MAX_PATH] = {};
        GetModuleFileNameW(g_hDllInstance, dllPath, MAX_PATH);

        wchar_t* lastSlash = wcsrchr(dllPath, L'\\');
        if (lastSlash)
            *(lastSlash + 1) = L'\0';
        wcscat_s(dllPath, L"image.png");

        m_pImage = Gdiplus::Image::FromFile(dllPath);

        if (m_pImage && m_pImage->GetLastStatus() != Ok)
        {
            delete m_pImage;
            m_pImage = nullptr;
        }
    }

    ~MyRadarScreen()
    {
        delete m_pImage;
    }

    virtual void OnRefresh(HDC hDC, int Phase) override
    {
        if (Phase != REFRESH_PHASE_AFTER_TAGS)
            return;

        if (!m_pImage)
            return;

        int w = (int)m_pImage->GetWidth();
        int h = (int)m_pImage->GetHeight();

        if (m_X == -1)
        {
            RECT radarArea = GetRadarArea();
            m_X = radarArea.right - w / 2 - 10;
            m_Y = (radarArea.top + 200) + h / 2 + 10;
        }

        Graphics graphics(hDC);
        graphics.DrawImage(m_pImage, m_X - w / 2, m_Y - h / 2, w, h);

        RECT imgRect = {
            m_X - w / 2,
            m_Y - h / 2,
            m_X + w / 2,
            m_Y + h / 2
        };

        AddScreenObject(OBJECT_CIRCLE, "MyImage", imgRect, true, "");

        // Declare RECTs outside the if block so they are in scope for AddScreenObject
        RECT autoStarRect = {};
        RECT CoordinationCrossRect = {};
        RECT FMP_PanelRect = {};
        RECT Indra_PanelRect = {};

        if (m_Set_draw)
        {
            int panelX      = imgRect.left - 200;
            int panelY      = imgRect.top;
            int panelWidth  = 200;
            int panelHeight = imgRect.bottom - imgRect.top + 200;

            // Assign RECTs
            autoStarRect = {
                panelX + 10,
                panelY + 40,
                panelX + 10 + 200,
                panelY + 40 + 14
            };
            CoordinationCrossRect = {
                panelX + 10,
                panelY + 80,
                panelX + 10 + 200,
                panelY + 80 + 14
            };
            FMP_PanelRect = {
                panelX + 10,
                panelY + 120,
                panelX + 10 + 200,
                panelY + 120 + 14
            };
            Indra_PanelRect = {
                panelX + 10,
                panelY + 160,
                panelX + 10 + 200,
                panelY + 160 + 14
            };

            // Grey settings window
            SolidBrush greyBrush(Color(210, 50, 50, 50));
            FillRoundedRect(graphics, greyBrush, panelX, panelY, panelWidth, panelHeight, 4);

            // Settings title
            SolidBrush textBrush(Color(255, 255, 255, 255));
            FontFamily fontFamily(L"EuroScope");
            Font font(&fontFamily, 18, FontStyleRegular, UnitPixel);
            PointF textPoint(panelX + 5, panelY + 5);
            graphics.DrawString(L"Settings", -1, &font, textPoint, &textBrush);

            SolidBrush textBrush2(Color(255, 180, 184, 181));
            FontFamily fontFamily1(L"EuroScope");
            Font font2(&fontFamily1, 12, FontStyleRegular, UnitPixel);

            // Auto Star box
            SolidBrush AutoStarColour(Color(235, 64, 64, 69));
            FillRoundedRect(graphics, AutoStarColour, panelX + 5, panelY + 35.5, panelWidth - 15, 25, 6);
            PointF textPoint2(panelX + 10, panelY + 40);
            graphics.DrawString(L"Auto assign STAR", -1, &font2, textPoint2, &textBrush2);
            if (Auto_Star)
            {
                SolidBrush AutoStarOnColour(Color(235, 0, 200, 0));
                graphics.FillRectangle(&AutoStarOnColour, panelX + 160, panelY + 44.5, 7.5, 7.5);
            }
            else
            {
                SolidBrush AutoStarOffColour(Color(235, 250, 67, 67));
                graphics.FillRectangle(&AutoStarOffColour, panelX + 160, panelY + 44.5, 7.5, 7.5);
            }

            // S-Mode box
            SolidBrush CoordinationColour(Color(235, 64, 64, 69));
            FillRoundedRect(graphics, CoordinationColour, panelX + 5, panelY + 75.5, panelWidth - 15, 25, 6);
            PointF textPoint3(panelX + 10, panelY + 80);
            graphics.DrawString(L"S-Mode", -1, &font2, textPoint3, &textBrush2);
            if (S_mode)
            {
                SolidBrush CoordOnColour(Color(235, 0, 200, 0));
                graphics.FillRectangle(&CoordOnColour, panelX + 160, panelY + 84.5, 7.5, 7.5);
            }
            else
            {
                SolidBrush CoordOffColour(Color(235, 250, 67, 67));
                graphics.FillRectangle(&CoordOffColour, panelX + 160, panelY + 84.5, 7.5, 7.5);
            }

            // FMP Panel box
            SolidBrush FMPColour(Color(235, 64, 64, 69));
            FillRoundedRect(graphics, FMPColour, panelX + 5, panelY + 115.5, panelWidth - 15, 25, 6);
            PointF textPoint4(panelX + 10, panelY + 120);
            graphics.DrawString(L"FMP Panel", -1, &font2, textPoint4, &textBrush2);
            if (FMP_Panel)
            {
                SolidBrush FMPOnColour(Color(235, 0, 200, 0));
                graphics.FillRectangle(&FMPOnColour, panelX + 160, panelY + 124.5, 7.5, 7.5);
            }
            else
            {
                SolidBrush FMPOffColour(Color(235, 250, 67, 67));
                graphics.FillRectangle(&FMPOffColour, panelX + 160, panelY + 124.5, 7.5, 7.5);
            }

            // Indra Panel box
            SolidBrush IndraColour(Color(235, 64, 64, 69));
            FillRoundedRect(graphics, IndraColour, panelX + 5, panelY + 155.5, panelWidth - 15, 25, 6);
            PointF textPoint5(panelX + 10, panelY + 160);
            graphics.DrawString(L"Indra Panel", -1, &font2, textPoint5, &textBrush2);
            if (Indra_Panel)
            {
                SolidBrush IndraOnColour(Color(235, 0, 200, 0));
                graphics.FillRectangle(&IndraOnColour, panelX + 160, panelY + 164.5, 7.5, 7.5);
                if (!Indra_sent_drawn){
                IndraApcInterop::Undraw();
                int indra_sent_drawn = 1;
                }
            }
            else
            {
                SolidBrush IndraOffColour(Color(235, 250, 67, 67));
                graphics.FillRectangle(&IndraOffColour, panelX + 160, panelY + 164.5, 7.5, 7.5);
                if (Indra_sent_drawn){
                IndraApcInterop::Draw();
                int indra_sent_drawn = 0;
                }
            }
        } // end if (m_Set_draw)

        // S-Mode indicators drawn regardless of settings panel state
        if (S_mode)
        {
            SMode::DrawAllIndicators(hDC, GetPlugIn(), this);
        }

        // Screen objects registered regardless of settings panel state
        AddScreenObject(OBJECT_AUTOSTAR,           "AutoStar",   autoStarRect,          false, "Toggle Auto Star");
        AddScreenObject(OBJECT_COORDINATION_CROSS, "CoordCross", CoordinationCrossRect, false, "Toggle S-Mode");
        AddScreenObject(OBJECT_FMP_PANEL,          "FMPPanel",   FMP_PanelRect,         false, "Toggle FMP Panel");
        AddScreenObject(OBJECT_INDRA_PANEL,        "IndraPanel", Indra_PanelRect,       false, "Toggle Indra Panel");
    }

    virtual void OnClickScreenObject(
        int ObjectType,
        const char* sObjectId,
        POINT Pt,
        RECT Area,
        int Button) override
    {
        if (ObjectType == OBJECT_CIRCLE && Button == BUTTON_LEFT)
        {
            m_Set_draw = 1 - m_Set_draw;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_CIRCLE && Button == BUTTON_RIGHT)
        {
            CRadarTarget rt = GetPlugIn()->RadarTargetSelectASEL();
            if (rt.IsValid())
            {
                StartTagFunction(
                    rt.GetCallsign(),
                    "TopSky plugin",
                    0,
                    "",
                    "TopSky plugin",
                    TopSky::TOGGLE_ROUTE_DRAW_MTCD_SAP,
                    Pt,
                    Area
                );
            }
            RequestRefresh();
        }

        if (ObjectType == OBJECT_CIRCLE && Button == BUTTON_MIDDLE)
        {
            m_Set_draw = 1 - m_Set_draw;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_AUTOSTAR && Button == BUTTON_LEFT)
        {
            Auto_Star = 1 - Auto_Star;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_AUTOSTAR && Button == BUTTON_RIGHT)
        {
            Auto_Star = 1;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_COORDINATION_CROSS && Button == BUTTON_LEFT)
        {
            S_mode = 1 - S_mode;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_COORDINATION_CROSS && Button == BUTTON_RIGHT)
        {
            S_mode = 1;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_FMP_PANEL && Button == BUTTON_LEFT)
        {
            FMP_Panel = 1 - FMP_Panel;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_FMP_PANEL && Button == BUTTON_RIGHT)
        {
            FMP_Panel = 1;
            RequestRefresh();
        }

        if (ObjectType == OBJECT_INDRA_PANEL && Button == BUTTON_LEFT)
        {
            Indra_Panel = 1 - Indra_Panel;
            RequestRefresh();
        }
    }

    virtual void OnMoveScreenObject(
        int ObjectType,
        const char* sObjectId,
        POINT Pt,
        RECT Area,
        bool Released) override
    {
        if (ObjectType != OBJECT_CIRCLE)
            return;

        m_X = Pt.x;
        m_Y = Pt.y;

        RequestRefresh();
    }

    virtual void OnAsrContentToBeClosed() override {}
};

void change_route(CFlightPlanData& fpData,
                  const std::string& old_route,
                  const std::string& new_star,
                  const std::string& runway)
{
    std::string route = old_route;

    size_t pos = route.find_last_of(' ');
    if (pos != std::string::npos)
    {
        route = route.substr(0, pos);
    }

    route += " " + new_star + "/" + runway;
    fpData.SetRoute(route.c_str());
}

class MyPlugIn : public CPlugIn
{
    MyRadarScreen* m_Screen = nullptr;
public:
    MyPlugIn()
        : CPlugIn(
            COMPATIBILITY_CODE,
            "Mag Plugin",
            "0.0.1",
            "Maghreb vACC",
            "GPL V3"
        )
    {
        RegisterTagItemType("Star TAG Display", STAR_TAG_ID);
        RegisterTagItemFunction("STAR Click", STAR_TAG_FUNCTION);

        RegisterTagItemType("RTE TAG Display", RTE_CHECKER_ID);
        RegisterTagItemFunction("RTE Click", RTE_CHECKER_FUNCTION);

        DisplayUserMessage(
            "Maghreb vACC",
            "Mag Plugin",
            "Plugin loaded successfully",
            true, true, false, false, false
        );
    }

    virtual void OnTime() {}

    virtual void OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area) override
    {
        if (FunctionId == STAR_TAG_FUNCTION)
        {
            CRadarTarget rt = RadarTargetSelectASEL();
            if (!rt.IsValid()) return;

            CFlightPlan fp = rt.GetCorrelatedFlightPlan();
            if (!fp.IsValid()) return;

            CFlightPlanData fpData = fp.GetFlightPlanData();

            std::string callsign = rt.GetCallsign();
            CSectorElement secElm = CSectorElement();

            std::string StarName = fpData.GetStarName();
            std::string airport  = fpData.GetDestination();
            std::string runway   = fp.GetFlightPlanData().GetArrivalRwy();
            std::string fix      = StarName.substr(0, 5);

            std::string new_star = FetchJsonDataStars(airport, runway, fix).get<std::string>();

            CorrectStar[callsign] = new_star;

            int& state = starState[callsign];
            if (state == 0) {
                state = 1;
            } else if (state == 1) {
                state = 2;
            } else if (state == 2) {
                state = 1;
            }
        }

        if (FunctionId == RTE_CHECKER_FUNCTION)
        {
            CRadarTarget rt = RadarTargetSelectASEL();
            DisplayUserMessage("Debug", "Debug", rt.IsValid() ? "rt valid" : "rt invalid", true, true, true, true, true);
        }
    }

    virtual void OnGetTagItem(
        CFlightPlan FlightPlan,
        CRadarTarget rt,
        int ItemCode,
        int TagData,
        char sItemString[16],
        int* pColorCode,
        COLORREF* pRgbColor,
        double* pFontSize) override
    {
        if (ItemCode == STAR_TAG_ID)
        {
            if (!rt.IsValid())
                return;

            std::string callsign = rt.GetCallsign();
            CFlightPlanData fpData = FlightPlan.GetFlightPlanData();

            std::string StarName = fpData.GetStarName();
            std::string airport  = fpData.GetDestination();
            std::string runway   = FlightPlan.GetFlightPlanData().GetArrivalRwy();
            std::string fix      = StarName.substr(0, 5);

            auto new_star = CorrectStar.find(callsign);
            strncpy_s(sItemString, 16, StarName.c_str(), _TRUNCATE);
            *pRgbColor  = RGB(255, 92, 103);
            *pColorCode = TAG_COLOR_RGB_DEFINED;

            std::string old_route = fpData.GetRoute();

            if (new_star == CorrectStar.end())
                return;

            auto it = starState.find(callsign);
            if (it != starState.end())
            {
                state = it->second;
                *pColorCode = TAG_COLOR_RGB_DEFINED;
                *pRgbColor  = RGB(255, 92, 103);

                switch (state)
                {
                    case 0:
                        strncpy_s(sItemString, 16, StarName.c_str(), _TRUNCATE);
                        *pRgbColor = RGB(255, 92, 103);
                        break;
                    case 1:
                        strncpy_s(sItemString, 16, new_star->second.c_str(), _TRUNCATE);
                        *pRgbColor = RGB(237, 186, 57);
                        change_route(fpData, old_route, new_star->second, runway);
                        break;
                    case 2:
                        strncpy_s(sItemString, 16, new_star->second.c_str(), _TRUNCATE);
                        *pRgbColor = RGB(99, 255, 107);
                        break;
                }
            }
        }

        if (ItemCode == RTE_CHECKER_ID)
        {
            if (!FlightPlan.IsValid())
                return;

            strncpy_s(sItemString, 16, "RTE", _TRUNCATE);
            *pColorCode = TAG_COLOR_RGB_DEFINED;
            *pRgbColor  = RGB(255, 92, 103);
        }
    }

    virtual CRadarScreen* OnRadarScreenCreated(
        const char* sDisplayName,
        bool NeedRadarContent,
        bool GeoReferenced,
        bool CanBeSaved,
        bool CanBeCreated) override
    {
        m_Screen = new MyRadarScreen();
        return m_Screen;
    }

    virtual void OnRadarTargetPositionUpdate(CRadarTarget rt) override
    {
        if (!rt.IsValid())
            return;

        const char* callsign = rt.GetCallsign();
        if (!callsign)
            return;

        CFlightPlan fp = rt.GetCorrelatedFlightPlan();
        if (!fp.IsValid())
            return;

        if (m_Screen)
            m_Screen->RequestRefresh();
    }

    virtual ~MyPlugIn() {}
};

MyPlugIn* gPlugin = nullptr;

__declspec(dllexport) void EuroScopePlugInInit(CPlugIn** ppPlugIn)
{
    gPlugin = new MyPlugIn();
    *ppPlugIn = gPlugin;
}

__declspec(dllexport) void EuroScopePlugInExit()
{
}