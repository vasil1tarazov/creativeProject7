#include <httplib.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <unordered_map>
#include <mutex>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using json = nlohmann::json;

struct SensorData {
    double temperature;
    double humidity;
    int co2;
    bool people_present;
    std::string last_update;
};

std::unordered_map<std::string, SensorData> database;
std::mutex db_mutex;

std::string current_time_iso() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt, &in_time_t);
#else
    localtime_r(&in_time_t, &bt);
#endif
    std::ostringstream oss;
    oss << std::put_time(&bt, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

int main() {
    httplib::Server svr;

    // POST /api/data
    svr.Post("/api/data", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);
            std::string room = j["room"];
            SensorData data;
            data.temperature = j["temp"];
            data.humidity = j["humidity"];
            data.co2 = j["co2"];
            data.people_present = j["people_present"];
            data.last_update = current_time_iso();

            {
                std::lock_guard<std::mutex> lock(db_mutex);
                database[room] = data;
            }

            std::cout << "Data from " << room << ": temp=" << data.temperature
                      << " CO2=" << data.co2 << std::endl;
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // GET /api/status
    svr.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        json response = json::object();  // инициализируем как пустой объект, а не null
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            for (const auto& [room, data] : database) {
                response[room] = {
                    {"temperature", data.temperature},
                    {"humidity", data.humidity},
                    {"co2", data.co2},
                    {"people_present", data.people_present},
                    {"last_update", data.last_update}
                };
            }
        }
        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/ping
    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"pong\":true}", "application/json");
    });

    std::cout << "Server started at http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);

    return 0;
}