#include <cstdlib>
#include <drogon/HttpController.h>
#include <drogon/orm/DbClient.h>
#include <drogon/drogon.h>

using namespace drogon;
using namespace drogon::orm;

typedef std::function<void(const HttpResponsePtr &)> Callback;

void helloHandler(const HttpRequestPtr &request, Callback &&callback){
    Json::Value jsonBody;
    jsonBody["message"] = "Hello!";

    auto resp = HttpResponse::newHttpJsonResponse(jsonBody);
    resp->setStatusCode(k200OK);
    callback(resp);
}

void citiesGetHandler(const HttpRequestPtr &request, Callback &&callback) {

    auto dbClient = app().getDbClient();

    if (dbClient == nullptr){
        Json::Value jsonBody;
        jsonBody["message"] = "Db client is not connected!";

        auto errorResp = HttpResponse::newHttpJsonResponse(jsonBody);
        errorResp->setStatusCode(k503ServiceUnavailable);
        callback(errorResp);
    }
    else{
        dbClient->execSqlAsync("SELECT name FROM cities",
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
                auto errorResp = HttpResponse::newHttpJsonResponse(
                    Json::Value("Error fetching city names: " + std::string(e.base().what()))
                );
                errorResp->setStatusCode(k500InternalServerError);
                callback(errorResp);
            }
        );
    }
}

void weatherPostHandler(const HttpRequestPtr &request, Callback &&callback) {

    auto dbClient = app().getDbClient();
    if (dbClient == nullptr){
        Json::Value jsonBody;
        jsonBody["message"] = "Db client is not connected!";

        auto errorResp = HttpResponse::newHttpJsonResponse(jsonBody);
        errorResp->setStatusCode(k503ServiceUnavailable);
        callback(errorResp);
    }
    else{
        auto requestBody = request->getJsonObject();
        if (requestBody == nullptr || !requestBody->isMember("city")) {
            Json::Value jsonBody;
            jsonBody["status"] = "error";
            jsonBody["message"] = "city is required";

            auto response = HttpResponse::newHttpJsonResponse(jsonBody);
            response->setStatusCode(HttpStatusCode::k400BadRequest);

            callback(response);
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
                auto errorResp = HttpResponse::newHttpResponse();
                errorResp->setStatusCode(k500InternalServerError);
                errorResp->setContentTypeCode(CT_TEXT_PLAIN);
                errorResp->setBody("Database error: " + std::string(e.base().what()));
                callback(errorResp);
            },
            city
        );
    }
}

void cloudinessPostHandler(const HttpRequestPtr &request, Callback &&callback) {

    auto requestBody = request->getJsonObject();
        if (!requestBody || !requestBody->isMember("cloudiness")) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Parameter 'cloudiness' is required");
            callback(resp);
            return;
        }

        std::string cloudiness = (*requestBody)["cloudiness"].asString();
        static const std::set<std::string> validCloudinessValues = {"Облачно", "Солнечно", "Переменная облачность"};
        if (validCloudinessValues.find(cloudiness) == validCloudinessValues.end()) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("Invalid value for 'cloudiness'. Must be one of: Облачно, Солнечно, Переменная облачность");
            callback(resp);
            return;
        }

        auto dbClient = drogon::app().getDbClient();

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
                callback(resp);
            },
            [callback](const DrogonDbException &e) {
                auto errorResp = HttpResponse::newHttpResponse();
                errorResp->setStatusCode(k500InternalServerError);
                errorResp->setContentTypeCode(CT_TEXT_PLAIN);
                errorResp->setBody("Database error: " + std::string(e.base().what()));
                callback(errorResp);
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