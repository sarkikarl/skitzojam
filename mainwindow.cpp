// mainwindow.cpp
#include "mainwindow.h"

#include <QVBoxLayout>
#include <QPainter>
#include <QMenu>
#include <QDir>
#include <QCoreApplication>
#include <QIcon>

// ---------------------------------------------------------------------
// BackgroundWidget
// ---------------------------------------------------------------------
void BackgroundWidget::paintEvent(QPaintEvent * /*event*/) {
    QPainter painter(this);
    if (!background_.isNull()) {
        painter.drawPixmap(rect(), background_.scaled(size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
    } else {
        painter.fillRect(rect(), QColor("#0e0c1e"));
    }
}

// ---------------------------------------------------------------------
// MainWindow
// ---------------------------------------------------------------------
MainWindow::MainWindow(bool startMinimized, QWidget *parent) : QMainWindow(parent) {
    setWindowTitle("8D Audio Effect");
    resize(420, 520);

    buildUi();
    applyStyles();

    trayIcon_ = new QSystemTrayIcon(QIcon(":/icons/app_icon.png"), this);
    QMenu *trayMenu = new QMenu(this);
    trayMenu->addAction("Show", this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    trayMenu->addAction("Exit", qApp, &QCoreApplication::quit);
    trayIcon_->setContextMenu(trayMenu);
    trayIcon_->setToolTip("8D Audio Effect");
    connect(trayIcon_, &QSystemTrayIcon::activated, this, &MainWindow::onTrayActivated);
    trayIcon_->show();

    connect(&engine_, &AudioEngine::statusChanged, this, &MainWindow::onStatusChanged);

    loadSettings();
    populateDevices();
    restoreDeviceSelection();
    updateSliderLabels();
    checkStartup_->setChecked(isRunAtStartupEnabled());

    onDeviceChanged(); // auto-start if the restored selection is valid

    if (!startMinimized) {
        show();
    }
}

void MainWindow::buildUi() {
    central_ = new BackgroundWidget(this);
    central_->setBackgroundImage(QPixmap(":/icons/background.png"));

    auto *layout = new QVBoxLayout(central_);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(6);

    auto *labelInputTitle = new QLabel("Input (capture) device:", central_);
    labelInputTitle->setObjectName("titleLabel");
    layout->addWidget(labelInputTitle);

    comboInput_ = new QComboBox(central_);
    comboInput_->setObjectName("deviceCombo");
    layout->addWidget(comboInput_);
    layout->addSpacing(10);

    auto *labelOutputTitle = new QLabel("Output (render) device:", central_);
    labelOutputTitle->setObjectName("titleLabel");
    layout->addWidget(labelOutputTitle);

    comboOutput_ = new QComboBox(central_);
    comboOutput_->setObjectName("deviceCombo");
    layout->addWidget(comboOutput_);
    layout->addSpacing(16);

    labelSpeed_ = new QLabel("Rotation speed: 0.35 Hz", central_);
    labelSpeed_->setObjectName("speedLabel");
    layout->addWidget(labelSpeed_);
    sliderSpeed_ = new QSlider(Qt::Horizontal, central_);
    sliderSpeed_->setObjectName("sliderSpeed");
    sliderSpeed_->setRange(0, 200); // 0.00 - 2.00 Hz
    layout->addWidget(sliderSpeed_);
    layout->addSpacing(10);

    labelDepth_ = new QLabel("Pan depth: 0.70", central_);
    labelDepth_->setObjectName("depthLabel");
    layout->addWidget(labelDepth_);
    sliderDepth_ = new QSlider(Qt::Horizontal, central_);
    sliderDepth_->setObjectName("sliderDepth");
    sliderDepth_->setRange(0, 100);
    layout->addWidget(sliderDepth_);
    layout->addSpacing(10);

    labelFloor_ = new QLabel("Quiet-side floor: 0.45", central_);
    labelFloor_->setObjectName("floorLabel");
    layout->addWidget(labelFloor_);
    sliderFloor_ = new QSlider(Qt::Horizontal, central_);
    sliderFloor_->setObjectName("sliderFloor");
    sliderFloor_->setRange(0, 100);
    layout->addWidget(sliderFloor_);
    layout->addSpacing(10);

    labelRandom_ = new QLabel("Randomization: 0%", central_);
    labelRandom_->setObjectName("randomLabel");
    layout->addWidget(labelRandom_);
    sliderRandom_ = new QSlider(Qt::Horizontal, central_);
    sliderRandom_->setObjectName("sliderRandom");
    sliderRandom_->setRange(0, 100);
    layout->addWidget(sliderRandom_);
    layout->addSpacing(16);

    labelStatus_ = new QLabel("Status: pick an input and output device", central_);
    labelStatus_->setObjectName("statusLabel");
    layout->addWidget(labelStatus_);
    layout->addSpacing(10);

    checkStartup_ = new QCheckBox("Run at Windows startup", central_);
    layout->addWidget(checkStartup_);

    layout->addStretch();

    setCentralWidget(central_);

    connect(comboInput_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDeviceChanged);
    connect(comboOutput_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onDeviceChanged);
    connect(sliderSpeed_, &QSlider::valueChanged, this, &MainWindow::onSliderChanged);
    connect(sliderDepth_, &QSlider::valueChanged, this, &MainWindow::onSliderChanged);
    connect(sliderFloor_, &QSlider::valueChanged, this, &MainWindow::onSliderChanged);
    connect(sliderRandom_, &QSlider::valueChanged, this, &MainWindow::onSliderChanged);
    connect(sliderSpeed_, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    connect(sliderDepth_, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    connect(sliderFloor_, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    connect(sliderRandom_, &QSlider::sliderReleased, this, &MainWindow::onSliderReleased);
    connect(checkStartup_, &QCheckBox::toggled, this, &MainWindow::onStartupToggled);
}

void MainWindow::applyStyles() {
    // Neon palette pulled from the app icon, same colors used in the
    // Win32 version: cyan, magenta, orange, yellow.
    const QString qss = R"(
        QLabel#titleLabel { color: #e8e8ff; font-weight: bold; background: transparent; }
        QLabel#statusLabel { color: #e8e8ff; background: transparent; }
        QLabel#speedLabel { color: #3bccf0; background: transparent; }
        QLabel#depthLabel { color: #c63f9b; background: transparent; }
        QLabel#floorLabel { color: #e06a41; background: transparent; }
        QLabel#randomLabel { color: #f4d361; background: transparent; }
        QCheckBox { color: #e8e8ff; background: transparent; }

        QComboBox#deviceCombo {
            background-color: #ff69b4;
            color: white;
            border: 1px solid #ff1493;
            border-radius: 4px;
            padding: 4px 8px;
        }
        QComboBox#deviceCombo::drop-down {
            border: none;
            background-color: #ff1493;
            width: 24px;
        }
        QComboBox#deviceCombo QAbstractItemView {
            background-color: #2a1030;
            color: white;
            selection-background-color: #ff69b4;
            selection-color: white;
            outline: none;
        }

        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border: 1px solid #ff1493;
            border-radius: 3px;
            background-color: #ff69b4;
        }
        QCheckBox::indicator:checked {
            background-color: #ff1493;
        }

        QSlider::groove:horizontal {
            height: 6px;
            border-radius: 3px;
            background: #28203c;
        }
        QSlider::handle:horizontal {
            width: 16px;
            margin: -6px 0;
            border-radius: 8px;
            border: 1px solid white;
        }
        QSlider#sliderSpeed::handle:horizontal, QSlider#sliderSpeed::sub-page:horizontal { background: #3bccf0; }
        QSlider#sliderDepth::handle:horizontal, QSlider#sliderDepth::sub-page:horizontal { background: #c63f9b; }
        QSlider#sliderFloor::handle:horizontal, QSlider#sliderFloor::sub-page:horizontal { background: #e06a41; }
        QSlider#sliderRandom::handle:horizontal, QSlider#sliderRandom::sub-page:horizontal { background: #f4d361; }
        QSlider::sub-page:horizontal { border-radius: 3px; }
    )";
    setStyleSheet(qss);
}

void MainWindow::populateDevices() {
    auto devices = AudioEngine::listDevices();
    comboInput_->clear();
    comboOutput_->clear();
    for (const auto &d : devices) {
        comboInput_->addItem(d.name, d.id);
        comboOutput_->addItem(d.name, d.id);
    }
}

void MainWindow::restoreDeviceSelection() {
    int capIdx = comboInput_->findData(savedCaptureId_);
    comboInput_->setCurrentIndex(capIdx >= 0 ? capIdx : 0);

    int rendIdx = comboOutput_->findData(savedRenderId_);
    comboOutput_->setCurrentIndex(rendIdx >= 0 ? rendIdx : 0);
}

void MainWindow::loadSettings() {
    QSettings s;
    savedCaptureId_ = s.value("CaptureDeviceId").toString();
    savedRenderId_ = s.value("RenderDeviceId").toString();
    engine_.setLfoHz(s.value("Speed", 0.35).toDouble());
    engine_.setDepth(s.value("Depth", 0.7).toDouble());
    engine_.setMinGain(s.value("Floor", 0.45).toDouble());
    engine_.setRandomAmount(s.value("Random", 0.0).toDouble());

    sliderSpeed_->setValue((int)(engine_.lfoHz() * 100));
    sliderDepth_->setValue((int)(engine_.depth() * 100));
    sliderFloor_->setValue((int)(engine_.minGain() * 100));
    sliderRandom_->setValue((int)(engine_.randomAmount() * 100));
}

void MainWindow::saveSettings() {
    QSettings s;
    s.setValue("CaptureDeviceId", comboInput_->currentData().toString());
    s.setValue("RenderDeviceId", comboOutput_->currentData().toString());
    s.setValue("Speed", engine_.lfoHz());
    s.setValue("Depth", engine_.depth());
    s.setValue("Floor", engine_.minGain());
    s.setValue("Random", engine_.randomAmount());
}

void MainWindow::updateSliderLabels() {
    labelSpeed_->setText(QString("Rotation speed: %1 Hz").arg(engine_.lfoHz(), 0, 'f', 2));
    labelDepth_->setText(QString("Pan depth: %1").arg(engine_.depth(), 0, 'f', 2));
    labelFloor_->setText(QString("Quiet-side floor: %1").arg(engine_.minGain(), 0, 'f', 2));
    labelRandom_->setText(QString("Randomization: %1%").arg((int)(engine_.randomAmount() * 100)));
}

void MainWindow::onDeviceChanged() {
    QString captureId = comboInput_->currentData().toString();
    QString renderId = comboOutput_->currentData().toString();
    engine_.restart(captureId, renderId);
    saveSettings();
}

void MainWindow::onSliderChanged() {
    engine_.setLfoHz(sliderSpeed_->value() / 100.0);
    engine_.setDepth(sliderDepth_->value() / 100.0);
    engine_.setMinGain(sliderFloor_->value() / 100.0);
    engine_.setRandomAmount(sliderRandom_->value() / 100.0);
    updateSliderLabels();
}

void MainWindow::onSliderReleased() {
    saveSettings();
}

void MainWindow::onStartupToggled(bool checked) {
    setRunAtStartup(checked);
}

void MainWindow::onStatusChanged(const QString &status) {
    labelStatus_->setText("Status: " + status);
}

void MainWindow::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        raise();
        activateWindow();
    }
}

bool MainWindow::isRunAtStartupEnabled() const {
    QSettings runKey("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    return runKey.contains("EightDAudioEffect");
}

void MainWindow::setRunAtStartup(bool enable) {
    QSettings runKey("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
    if (enable) {
        QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        runKey.setValue("EightDAudioEffect", QString("\"%1\" --minimized").arg(exePath));
    } else {
        runKey.remove("EightDAudioEffect");
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    // Closing the window hides it to tray rather than exiting outright,
    // so the effect keeps running in the background. Use the tray
    // icon's "Exit" option to actually quit.
    hide();
    event->ignore();
}

void MainWindow::changeEvent(QEvent *event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange && isMinimized()) {
        hide();
    }
}
