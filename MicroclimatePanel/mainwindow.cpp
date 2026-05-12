#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QUrl>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    connect(networkManager, &QNetworkAccessManager::finished,
            this, &MainWindow::onReplyFinished);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_pushButton_clicked()
{
    ui->textEdit_Output->setText("Запрос отправлен...");
    QUrl url("http://localhost:8080/api/status");
    QNetworkRequest request(url);
    networkManager->get(request);
}

void MainWindow::onReplyFinished(QNetworkReply *reply)
{
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isNull() && doc.isObject()) {
            QJsonObject obj = doc.object();
            QString formatted;
            if (obj.isEmpty()) {
                formatted = "Нет данных от датчиков.";
            } else {
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    QString room = it.key();
                    QJsonObject roomData = it.value().toObject();
                    formatted += QString("Комната: %1\n")
                                     .arg(room);
                    formatted += QString("  Температура: %1 °C\n")
                                     .arg(roomData["temperature"].toDouble());
                    formatted += QString("  Влажность: %1 %\n")
                                     .arg(roomData["humidity"].toDouble());
                    formatted += QString("  CO₂: %1 ppm\n")
                                     .arg(roomData["co2"].toInt());
                    formatted += QString("  Присутствие: %1\n")
                                     .arg(roomData["people_present"].toBool() ? "Да" : "Нет");
                    formatted += QString("  Обновлено: %1\n\n")
                                     .arg(roomData["last_update"].toString());
                }
            }
            ui->textEdit_Output->setText(formatted);
        } else {
            ui->textEdit_Output->setText("Ошибка: ответ не является JSON объектом");
        }
    } else {
        ui->textEdit_Output->setText("Ошибка: " + reply->errorString());
    }
    reply->deleteLater();
}