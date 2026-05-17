#include "mainwindow.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include <QVBoxLayout>
#include <QWidget>
#include <QDateTime>
#include <QMessageBox>
#include <cstdlib>
#include <ctime>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , networkManager(new QNetworkAccessManager(this))
    , updateTimer(new QTimer(this))
    , statusTimer(new QTimer(this))
    , refreshButton(new QPushButton("Обновить данные", this))
    , predictButton(new QPushButton("Прогноз CO₂", this))
    , ventilationButton(new QPushButton("Переключить вентиляцию", this))
    , acButton(new QPushButton("Переключить кондиционер", this))
    , healthButton(new QPushButton("Проверить здоровье (датчик на двери)", this))
    , statusLabel(new QLabel("Вентиляция: ? | Кондиционер: ?", this))
    , customPlot(new QCustomPlot(this))
    , lastPointCount(0)
{
    // Инициализация генератора случайных чисел для симуляции датчика
    srand(static_cast<unsigned>(time(nullptr)));

    resize(1000, 700);
    setupUI();

    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onReplyFinished);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::requestHistory);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::requestStatus);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshClicked);
    connect(predictButton, &QPushButton::clicked, this, &MainWindow::onPredictClicked);
    connect(ventilationButton, &QPushButton::clicked, this, &MainWindow::onVentilationClicked);
    connect(acButton, &QPushButton::clicked, this, &MainWindow::onACClicked);
    connect(healthButton, &QPushButton::clicked, this, &MainWindow::onHealthClicked);

    // Настройка графика
    customPlot->addGraph();
    customPlot->graph(0)->setPen(QPen(Qt::red));
    customPlot->graph(0)->setName("Температура, °C");
    customPlot->addGraph();
    customPlot->graph(1)->setPen(QPen(Qt::blue));
    customPlot->graph(1)->setName("CO₂, ppm");
    customPlot->graph(1)->setValueAxis(customPlot->yAxis2);

    customPlot->xAxis->setLabel("Номер измерения (по порядку)");
    customPlot->yAxis->setLabel("Температура, °C");
    customPlot->yAxis2->setLabel("CO₂, ppm");
    customPlot->yAxis2->setVisible(true);
    customPlot->legend->setVisible(true);

    updateTimer->start(5000);
    statusTimer->start(3000);
    requestHistory();
    requestStatus();
}

MainWindow::~MainWindow() {}

void MainWindow::setupUI()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    layout->addWidget(statusLabel);
    layout->addWidget(refreshButton);
    layout->addWidget(predictButton);
    layout->addWidget(ventilationButton);
    layout->addWidget(acButton);
    layout->addWidget(healthButton);
    layout->addWidget(customPlot);
    layout->setStretchFactor(customPlot, 1);
    setCentralWidget(central);
}

void MainWindow::onRefreshClicked() { requestHistory(); }
void MainWindow::onPredictClicked() { requestPredict(); }

void MainWindow::onVentilationClicked()
{
    QUrl url("http://localhost:8080/api/status");
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);
    reply->setProperty("action", "toggle_ventilation");
}

void MainWindow::onACClicked()
{
    QUrl url("http://localhost:8080/api/status");
    QNetworkRequest request(url);
    QNetworkReply *reply = networkManager->get(request);
    reply->setProperty("action", "toggle_ac");
}

void MainWindow::onHealthClicked()
{
    // Симуляция датчика температуры на двери (случайное значение от 36.0 до 38.5)
    double temp = 36.0 + (static_cast<double>(rand()) / RAND_MAX) * 2.5;
    QJsonObject json;
    json["room"] = "A101";
    json["person_temp"] = temp;
    QJsonDocument doc(json);
    QUrl url("http://localhost:8080/api/check_person");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QNetworkReply *reply = networkManager->post(request, doc.toJson());
    reply->setProperty("action", "health_check");
}

void MainWindow::requestHistory()
{
    QUrl url("http://localhost:8080/api/history?room=A101&limit=200");
    QNetworkRequest request(url);
    networkManager->get(request);
}

void MainWindow::requestStatus()
{
    QUrl url("http://localhost:8080/api/status");
    QNetworkRequest request(url);
    networkManager->get(request);
}

void MainWindow::requestPredict()
{
    QUrl url("http://localhost:8080/api/predict?room=A101");
    QNetworkRequest request(url);
    networkManager->get(request);
}

void MainWindow::addDataPoints(const QJsonArray &arr)
{
    if (arr.isEmpty()) return;
    if (lastPointCount == 0) {
        timeValues.clear();
        tempValues.clear();
        co2Values.clear();
        lastReceivedTime = QDateTime();
    }
    for (const auto &item : arr) {
        QJsonObject obj = item.toObject();
        QString timeStr = obj["time"].toString();
        QDateTime dt = QDateTime::fromString(timeStr, Qt::ISODate);
        if (!dt.isValid()) continue;
        if (!lastReceivedTime.isValid() || dt > lastReceivedTime) {
            double idx = timeValues.size();
            timeValues.append(idx);
            tempValues.append(obj["temperature"].toDouble());
            co2Values.append(obj["co2"].toInt());
            lastReceivedTime = dt;
        }
    }
    lastPointCount = timeValues.size();
    updatePlot();
}

void MainWindow::updatePlot()
{
    if (timeValues.isEmpty()) return;
    customPlot->graph(0)->setData(timeValues, tempValues);
    customPlot->graph(1)->setData(timeValues, co2Values);
    if (timeValues.size() > 100) {
        double maxX = timeValues.last();
        double minX = maxX - 100;
        customPlot->xAxis->setRange(minX, maxX);
    } else {
        customPlot->rescaleAxes();
    }
    customPlot->yAxis->rescale(true);
    customPlot->yAxis2->rescale(true);
    customPlot->replot();
}

void MainWindow::onReplyFinished(QNetworkReply *reply)
{
    QUrl url = reply->url();
    if (reply->error() != QNetworkReply::NoError) {
        statusLabel->setText("Ошибка: " + reply->errorString());
        reply->deleteLater();
        return;
    }
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QString action = reply->property("action").toString();

    if (url.toString().contains("/history")) {
        if (doc.isArray()) addDataPoints(doc.array());
        else statusLabel->setText("Ошибка истории");
    }
    else if (url.toString().contains("/predict")) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("predicted_co2_5min"))
                statusLabel->setText("Прогноз CO₂: " + QString::number(obj["predicted_co2_5min"].toInt()) + " ppm");
            else statusLabel->setText("Недостаточно данных");
        }
    }
    else if (url.toString().contains("/status")) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("A101")) {
                QJsonObject roomObj = obj["A101"].toObject();
                bool vent = roomObj["ventilation_on"].toBool();
                bool ac = roomObj["ac_on"].toBool();
                statusLabel->setText(QString("Вентиляция: %1 | Кондиционер: %2")
                                         .arg(vent ? "ВКЛ" : "ВЫКЛ")
                                         .arg(ac ? "ВКЛ" : "ВЫКЛ"));
                if (action == "toggle_ventilation") {
                    bool newState = !vent;
                    QJsonObject cmd; cmd["room"] = "A101"; cmd["ventilation_on"] = newState;
                    QJsonDocument docCmd(cmd);
                    QUrl controlUrl("http://localhost:8080/api/control");
                    QNetworkRequest cr(controlUrl);
                    cr.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                    networkManager->post(cr, docCmd.toJson());
                    statusLabel->setText("Вентиляция переключена");
                } else if (action == "toggle_ac") {
                    bool newState = !ac;
                    QJsonObject cmd; cmd["room"] = "A101"; cmd["ac_on"] = newState;
                    QJsonDocument docCmd(cmd);
                    QUrl controlUrl("http://localhost:8080/api/control");
                    QNetworkRequest cr(controlUrl);
                    cr.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
                    networkManager->post(cr, docCmd.toJson());
                    statusLabel->setText("Кондиционер переключён");
                }
            } else statusLabel->setText("Нет данных о комнате");
        } else statusLabel->setText("Ошибка статуса");
    }
    else if (action == "health_check") {
        if (doc.isObject()) {
            bool allowed = doc.object()["allowed"].toBool();
            QString msg = doc.object()["message"].toString();
            double personTemp = doc.object()["person_temp"].toDouble();
            if (!allowed) {
                QMessageBox::warning(this, "Доступ ограничен",
                                     QString("Температура: %1°C\n%2").arg(personTemp).arg(msg));
                statusLabel->setText("Доступ запрещён: температура выше нормы");
            } else {
                QMessageBox::information(this, "Доступ разрешён",
                                         QString("Температура: %1°C\n%2").arg(personTemp).arg(msg));
                statusLabel->setText("Здоровье в норме, доступ разрешён");
            }
        } else {
            statusLabel->setText("Ошибка проверки здоровья");
        }
    }
    reply->deleteLater();
}