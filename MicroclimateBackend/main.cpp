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
#include <vector>
#include <thread>
#include <random>

using json = nlohmann::json;

struct Measurement {
    std::string time;
    double temperature;
    double humidity;
    int co2;
    bool people_present;
};

struct RoomData {
    std::vector<Measurement> history;
    bool ventilation_on = false;
    bool ac_on = false;
};

std::unordered_map<std::string, RoomData> database;
std::mutex db_mutex;
bool running = true;

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

void add_measurement(const std::string& room, const Measurement& m) {
    std::lock_guard<std::mutex> lock(db_mutex);
    auto& roomData = database[room];
    roomData.history.push_back(m);
    if (roomData.history.size() > 200) {
        roomData.history.erase(roomData.history.begin());
    }

    // Автоматика вентиляции по CO2
    if (m.co2 > 900 && !roomData.ventilation_on) {
        roomData.ventilation_on = true;
        std::cout << "Auto: ventilation ON for " << room << " (CO2=" << m.co2 << ")" << std::endl;
    } else if (m.co2 < 600 && roomData.ventilation_on) {
        roomData.ventilation_on = false;
        std::cout << "Auto: ventilation OFF for " << room << " (CO2=" << m.co2 << ")" << std::endl;
    }

    // Автоматика кондиционера по температуре
    if (m.temperature > 25.0 && !roomData.ac_on) {
        roomData.ac_on = true;
        std::cout << "Auto: AC ON for " << room << " (temp=" << m.temperature << ")" << std::endl;
    } else if (m.temperature <= 22.0 && roomData.ac_on) {
        roomData.ac_on = false;
        std::cout << "Auto: AC OFF for " << room << " (temp=" << m.temperature << ")" << std::endl;
    }
}

int predict_co2(const std::string& room) {
    std::lock_guard<std::mutex> lock(db_mutex);
    auto it = database.find(room);
    if (it == database.end() || it->second.history.size() < 5) return -1;
    const auto& hist = it->second.history;
    int n = std::min(10, (int)hist.size());
    double sum_x = 0, sum_y = 0, sum_xy = 0, sum_x2 = 0;
    for (int i = 0; i < n; ++i) {
        double x = i;
        int y = hist[hist.size() - n + i].co2;
        sum_x += x; sum_y += y; sum_xy += x*y; sum_x2 += x*x;
    }
    double slope = (n*sum_xy - sum_x*sum_y) / (n*sum_x2 - sum_x*sum_x);
    double intercept = (sum_y - slope*sum_x) / n;
    int next = (int)(slope*n + intercept);
    if (next < 0) next = 0;
    return next;
}

void data_generator() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> hum_dist(40.0, 60.0);
    std::uniform_int_distribution<> co2_var(-5, 10);
    std::uniform_real_distribution<> noise(-0.1, 0.1);

    double co2_value = 450.0;
    double temperature = 22.0;
    double target_temp = 22.0;
    int counter = 0;
    double target_co2 = 450.0;
    const double step = 3.0;

    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        counter++;
        bool people = (counter / 12) % 2 == 0;

        // Получаем состояния устройств
        bool vent_on = false;
        bool ac_on = false;
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            auto it = database.find("A101");
            if (it != database.end()) {
                vent_on = it->second.ventilation_on;
                ac_on = it->second.ac_on;
            }
        }

        // --- CO₂ с учётом вентиляции ---
        if (vent_on) {
            if (co2_value > 450.0) {
                co2_value -= 12.0;
                if (co2_value < 450.0) co2_value = 450.0;
            } else {
                co2_value += co2_var(gen);
                if (co2_value < 400.0) co2_value = 400.0;
                if (co2_value > 1300.0) co2_value = 1300.0;
            }
        } else {
            if (people) {
                target_co2 += 20.0;
                if (target_co2 > 1200.0) target_co2 = 1200.0;
            } else {
                target_co2 -= 15.0;
                if (target_co2 < 400.0) target_co2 = 400.0;
            }
            if (co2_value < target_co2) {
                co2_value += step;
                if (co2_value > target_co2) co2_value = target_co2;
            } else if (co2_value > target_co2) {
                co2_value -= step;
                if (co2_value < target_co2) co2_value = target_co2;
            }
            co2_value += co2_var(gen);
            if (co2_value < 300.0) co2_value = 300.0;
            if (co2_value > 1300.0) co2_value = 1300.0;
        }

        // --- Температура с учётом кондиционера и плавной динамики ---
        if (ac_on) {
            // Кондиционер активно снижает температуру к 22.0
            if (temperature > 22.0) {
                temperature -= 0.2;
                if (temperature < 22.0) temperature = 22.0;
            }
            target_temp = 22.0;
        } else {
            // Без кондиционера: цель медленно меняется в зависимости от присутствия людей
            if (people) {
                target_temp += 0.05;
                if (target_temp > 25.0) target_temp = 25.0;
            } else {
                target_temp -= 0.03;
                if (target_temp < 20.0) target_temp = 20.0;
            }
            // Плавное движение текущей температуры к цели
            if (temperature < target_temp) {
                temperature += 0.1;
                if (temperature > target_temp) temperature = target_temp;
            } else if (temperature > target_temp) {
                temperature -= 0.1;
                if (temperature < target_temp) temperature = target_temp;
            }
            // Добавляем малый шум
            temperature += noise(gen);
            if (temperature < 18.0) temperature = 18.0;
            if (temperature > 26.0) temperature = 26.0;
        }

        double humidity = hum_dist(gen);

        Measurement m;
        m.time = current_time_iso();
        m.temperature = temperature;
        m.humidity = humidity;
        m.co2 = static_cast<int>(co2_value);
        m.people_present = people;
        add_measurement("A101", m);

        // Опциональный вывод (можно включить для отладки)
        // std::cout << "Generated: CO2=" << co2_value << " temp=" << temperature << " vent=" << vent_on << " ac=" << ac_on << " people=" << people << std::endl;
    }
}

int main() {
    std::thread generator(data_generator);
    httplib::Server svr;

    // POST /api/data - приём данных с датчиков
    svr.Post("/api/data", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);
            std::string room = j["room"];
            Measurement m;
            m.time = current_time_iso();
            m.temperature = j["temp"];
            m.humidity = j["humidity"];
            m.co2 = j["co2"];
            m.people_present = j["people_present"];
            add_measurement(room, m);
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // POST /api/control - управление устройствами
    svr.Post("/api/control", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);
            std::string room = j["room"];
            {
                std::lock_guard<std::mutex> lock(db_mutex);
                auto& roomData = database[room];
                if (j.contains("ventilation_on")) {
                    roomData.ventilation_on = j["ventilation_on"];
                    std::cout << "Manual: ventilation " << (roomData.ventilation_on ? "ON" : "OFF") << " for " << room << std::endl;
                }
                if (j.contains("ac_on")) {
                    roomData.ac_on = j["ac_on"];
                    std::cout << "Manual: AC " << (roomData.ac_on ? "ON" : "OFF") << " for " << room << std::endl;
                }
            }
            res.set_content("{\"status\":\"ok\"}", "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // GET /api/status - текущее состояние
    svr.Get("/api/status", [](const httplib::Request&, httplib::Response& res) {
        json response = json::object();
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            for (const auto& [room, roomData] : database) {
                if (!roomData.history.empty()) {
                    const auto& last = roomData.history.back();
                    response[room] = {
                        {"temperature", last.temperature},
                        {"humidity", last.humidity},
                        {"co2", last.co2},
                        {"people_present", last.people_present},
                        {"last_update", last.time},
                        {"ventilation_on", roomData.ventilation_on},
                        {"ac_on", roomData.ac_on}
                    };
                }
            }
        }
        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/history - история измерений
    svr.Get("/api/history", [](const httplib::Request& req, httplib::Response& res) {
        std::string room = req.get_param_value("room");
        int limit = 200;
        if (req.has_param("limit")) limit = std::stoi(req.get_param_value("limit"));
        json response = json::array();
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            auto it = database.find(room);
            if (it != database.end()) {
                const auto& history = it->second.history;
                int start = (int)history.size() - limit;
                if (start < 0) start = 0;
                for (size_t i = start; i < history.size(); ++i) {
                    json point;
                    point["time"] = history[i].time;
                    point["temperature"] = history[i].temperature;
                    point["co2"] = history[i].co2;
                    point["humidity"] = history[i].humidity;
                    response.push_back(point);
                }
            }
        }
        res.set_content(response.dump(2), "application/json");
    });

    // GET /api/predict - прогноз CO2
    svr.Get("/api/predict", [](const httplib::Request& req, httplib::Response& res) {
        std::string room = req.get_param_value("room");
        int predicted = predict_co2(room);
        json resp;
        if (predicted == -1) resp["error"] = "Not enough data";
        else {
            resp["room"] = room;
            resp["predicted_co2_5min"] = predicted;
        }
        res.set_content(resp.dump(2), "application/json");
    });

    // POST /api/check_person - контроль здоровья
    svr.Post("/api/check_person", [](const httplib::Request& req, httplib::Response& res) {
        try {
            json j = json::parse(req.body);
            std::string room = j["room"];
            double person_temp = j["person_temp"];
            bool allowed = (person_temp <= 37.2);
            json response;
            response["room"] = room;
            response["person_temp"] = person_temp;
            response["allowed"] = allowed;
            response["message"] = allowed ? "Доступ разрешён" : "Температура выше нормы, доступ запрещён";
            res.set_content(response.dump(2), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content("{\"error\":\"Invalid JSON\"}", "application/json");
        }
    });

    // GET /api/ping
    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content("{\"pong\":true}", "application/json");
    });

    std::cout << "Server started at http://localhost:8080" << std::endl;
    svr.listen("localhost", 8080);

    running = false;
    generator.join();
    return 0;
}