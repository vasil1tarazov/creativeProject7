#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include "qcustomplot.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRefreshClicked();
    void onPredictClicked();
    void onVentilationClicked();
    void onACClicked();
    void onHealthClicked();
    void onReplyFinished(QNetworkReply *reply);
    void requestStatus();
    void requestHistory();
    void requestPredict();

private:
    QNetworkAccessManager *networkManager;
    QTimer *updateTimer;
    QTimer *statusTimer;
    QPushButton *refreshButton;
    QPushButton *predictButton;
    QPushButton *ventilationButton;
    QPushButton *acButton;
    QPushButton *healthButton;
    QLabel *statusLabel;
    QCustomPlot *customPlot;
    void setupUI();
    void addDataPoints(const QJsonArray &arr);
    void updatePlot();

    QVector<double> timeValues;
    QVector<double> tempValues;
    QVector<double> co2Values;
    QDateTime lastReceivedTime;
    int lastPointCount;
};

#endif // MAINWINDOW_H