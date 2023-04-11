#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include <QListWidget>
#include "wavewidget.h"
#include "structs.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    WaveWidget *m_wave;
    QListWidget *m_blockList;

private:
    Ui::MainWindow *ui;

    void onTimer();

    void openWav();
    void processWave();
    void showBlock(int idx);

    void log(QString text);
    void error(QString text);
};

#endif // MAINWINDOW_H
