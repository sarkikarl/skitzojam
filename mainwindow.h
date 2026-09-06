// mainwindow.h
#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QPixmap>
#include <QComboBox>
#include <QSlider>
#include <QLabel>
#include <QCheckBox>
#include <QSystemTrayIcon>
#include <QSettings>
#include <QCloseEvent>

#include "audioengine.h"

// A plain widget that paints a background image behind whatever child
// widgets sit on top of it. Qt composites child widgets over this
// automatically and correctly on every repaint — no manual erase/blit
// bookkeeping needed (unlike the raw Win32 version, where getting this
// right without ghosting artifacts took real effort).
class BackgroundWidget : public QWidget {
    Q_OBJECT
public:
    using QWidget::QWidget;
    void setBackgroundImage(const QPixmap &pm) { background_ = pm; update(); }

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap background_;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(bool startMinimized, QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void onDeviceChanged();
    void onSliderChanged();
    void onSliderReleased();
    void onStartupToggled(bool checked);
    void onStatusChanged(const QString &status);
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void buildUi();
    void applyStyles();
    void populateDevices();
    void restoreDeviceSelection();
    void loadSettings();
    void saveSettings();
    void updateSliderLabels();
    bool isRunAtStartupEnabled() const;
    void setRunAtStartup(bool enable);

    AudioEngine engine_;

    BackgroundWidget *central_ = nullptr;
    QComboBox *comboInput_ = nullptr;
    QComboBox *comboOutput_ = nullptr;
    QSlider *sliderSpeed_ = nullptr;
    QSlider *sliderDepth_ = nullptr;
    QSlider *sliderFloor_ = nullptr;
    QSlider *sliderRandom_ = nullptr;
    QLabel *labelSpeed_ = nullptr;
    QLabel *labelDepth_ = nullptr;
    QLabel *labelFloor_ = nullptr;
    QLabel *labelRandom_ = nullptr;
    QLabel *labelStatus_ = nullptr;
    QCheckBox *checkStartup_ = nullptr;
    QSystemTrayIcon *trayIcon_ = nullptr;

    QString savedCaptureId_;
    QString savedRenderId_;
};
