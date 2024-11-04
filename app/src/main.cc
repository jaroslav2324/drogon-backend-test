#include <cstdlib>
#include <utility>
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>
#include <drogon/drogon.h>

using namespace drogon;
using namespace drogon::orm;

typedef std::function<void(const HttpResponsePtr &)> Callback;

const HttpResponsePtr & formDbUnavailableresponse(){
    Json::Value jsonBody;
    jsonBody["message"] = "Database is not available";
    auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
    resp->setStatusCode(k503ServiceUnavailable);
    return resp;
}

void helloHandler(const HttpRequestPtr &request, Callback&& callback){
    Json::Value jsonBody;
    jsonBody["message"] = "Hello!";
    auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
    resp->setStatusCode(k200OK);
    callback(resp);
}

void citiesGetHandler(const HttpRequestPtr &request, Callback &&callback) {

    auto dbClient = app().getDbClient();
    if (!dbClient->hasAvailableConnections()){
        Json::Value jsonBody;
        jsonBody["message"] = "Database is not available";
        auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
        resp->setStatusCode(k503ServiceUnavailable);
        callback(resp);
        return;
    }
    
    dbClient->execSqlAsync(
        "SELECT name FROM cities",
        [callback](const Result &result) {
            Json::Value cityArray(Json::arrayValue);
            
            for (const auto &row : result) {
                cityArray.append(row["name"].as<std::string>());
            }

            auto resp = HttpResponse::newHttpJsonResponse(cityArray);
            resp->setStatusCode(k200OK);
            callback(resp);
        },
        [callback](const DrogonDbException &e) {
            Json::Value jsonBody;
            jsonBody["message"] = "Error fetching city names: " + std::string(e.base().what());
            auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        }
    );
    
}

void weatherPostHandler(const HttpRequestPtr &request, Callback &&callback) {

    auto requestBody = request->getJsonObject();
    if (requestBody == nullptr || !requestBody->isMember("city")) {
        Json::Value jsonBody;
        jsonBody["message"] = "Parameter 'city' is required";
        auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    if (!dbClient->hasAvailableConnections()){
        Json::Value jsonBody;
        jsonBody["message"] = "Database is not available";
        auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
        resp->setStatusCode(k503ServiceUnavailable);
        callback(resp);
        return;
    }
    
    std::string city = (*requestBody)["city"].asString();
    dbClient->execSqlAsync(
        "SELECT w.date, w.temperature, w.cloudiness "
        "FROM forecasts AS w "
        "JOIN cities AS c ON w.city_id = c.id "
        "WHERE c.name = $1;",

        [callback, city](const Result &result) {
            Json::Value weatherJson;
            weatherJson["city"] = city;
            Json::Value daysArray(Json::arrayValue);

            for (const auto &row : result) {
                Json::Value day;
                day["date"] = row["date"].as<std::string>();
                day["temperature"] = row["temperature"].as<double>();
                day["cloudiness"] = row["cloudiness"].as<std::string>();
                daysArray.append(day);
            }

            weatherJson["days"] = daysArray;

            auto resp = HttpResponse::newHttpJsonResponse(weatherJson);
            resp->setStatusCode(k200OK);
            callback(resp);
        },

        [callback](const DrogonDbException &e) {
            Json::Value jsonBody;
            jsonBody["message"] = "Database error: " + std::string(e.base().what());
            auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        city
    );
    
}

void cloudinessPostHandler(const HttpRequestPtr &request, Callback &&callback) {

    auto requestBody = request->getJsonObject();
    if (!requestBody || !requestBody->isMember("cloudiness")) {
        Json::Value jsonBody;
        jsonBody["message"] = "Parameter 'cloudiness' is required";
        auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    std::string cloudiness = (*requestBody)["cloudiness"].asString();
    static const std::set<std::string> validCloudinessValues = {"Облачно", "Солнечно", "Переменная облачность"};
    if (validCloudinessValues.find(cloudiness) == validCloudinessValues.end()) {
        Json::Value jsonBody;
        jsonBody["message"] = "Invalid value for 'cloudiness'. Must be one of: Облачно, Солнечно, Переменная облачность";
        auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    auto dbClient = app().getDbClient();
    if (!dbClient->hasAvailableConnections()){
        Json::Value jsonBody;
        jsonBody["message"] = "Database is not available";
        auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
        resp->setStatusCode(k503ServiceUnavailable);
        callback(resp);
        return;
    }

    dbClient->execSqlAsync(
        "SELECT c.name AS city, f.date, f.temperature, f.cloudiness "
        "FROM forecasts AS f "
        "JOIN cities AS c ON f.city_id = c.id "
        "WHERE f.cloudiness = $1 "
        "ORDER BY c.name;",

        [callback](const Result &result) {
            Json::Value responseJson(Json::arrayValue);

            std::string currentCity;
            Json::Value cityJson;
            Json::Value daysArray(Json::arrayValue);

            for (const auto &row : result) {
                std::string city = row["city"].as<std::string>();

                if (city != currentCity && !currentCity.empty()) {
                    cityJson["days"] = daysArray;
                    responseJson.append(cityJson);
                    daysArray.clear();
                }

                if (city != currentCity) {
                    cityJson["city"] = city;
                    currentCity = city;
                }

                Json::Value day;
                day["date"] = row["date"].as<std::string>();
                day["temperature"] = row["temperature"].as<double>();
                day["cloudiness"] = row["cloudiness"].as<std::string>();
                daysArray.append(day);
            }

            if (!currentCity.empty()) {
                cityJson["days"] = daysArray;
                responseJson.append(cityJson);
            }
            auto resp = HttpResponse::newHttpJsonResponse(responseJson);
            resp->setStatusCode(k200OK);
            callback(resp);
        },

        [callback](const DrogonDbException &e) {
            Json::Value jsonBody;
            jsonBody["message"] = "Database error: " + std::string(e.base().what());
            auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
            resp->setStatusCode(k500InternalServerError);
            callback(resp);
        },
        cloudiness
    );
}

int main() {
    app()
    .loadConfigFile("../config.json")
    .registerHandler("/", &helloHandler, {Get})
    .registerHandler("/cities", &citiesGetHandler, {Get})
    .registerHandler("/weather", &weatherPostHandler, {Post})
    .registerHandler("/weather/cloudiness", &cloudinessPostHandler, {Post})
    .run();
    return 0;
}