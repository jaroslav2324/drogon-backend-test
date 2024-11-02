#include <cstdlib>
#include <drogon/HttpAppFramework.h>

typedef std::function<void(const drogon::HttpResponsePtr &)> Callback;

void indexHandler(const drogon::HttpRequestPtr &request, Callback &&callback) {
    Json::Value jsonBody;
    jsonBody["message"] = "Hello, world";

    auto response = drogon::HttpResponse::newHttpJsonResponse(jsonBody);
    callback(response);
}

int main() {
    // drogon::app();
    drogon::app()
    .addListener("0.0.0.0", 82)
    .registerHandler("/", &indexHandler, {drogon::Get})
    .run();
    return 0;
}