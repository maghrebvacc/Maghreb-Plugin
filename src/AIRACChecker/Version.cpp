#include "Version.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::string FetchJsonVersionControl()
{
    const std::string url =
        "https://raw.githubusercontent.com/"
        "maghrebvacc/Maghreb-Plugin/main/DATA/version.json";

    const std::string response =
        HttpGet(url);

    if (response.empty())
        return "";

    json data =
        json::parse(
            response,
            nullptr,
            false);

    if (data.is_discarded())
        return "";

    if (!data.contains("version"))
        return "";

    if (data["version"].is_string())
        return data["version"].get<std::string>();

    if (data["version"].is_number_integer())
        return std::to_string(data["version"].get<int>());

    return "";
}