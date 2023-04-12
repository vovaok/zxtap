#ifndef WAVEWIDGET_H
#define WAVEWIDGET_H

#include <graphwidget.h>
#include "structs.h"
#include <QElapsedTimer>

class WaveWidget : public GraphWidget
{
    Q_OBJECT
public:
    WaveWidget();

    WAVEfmt fmt;
    QVector<qreal> m_signal;
    QVector<qreal> m_chans[4];
    QVector<char> m_bits;
    QVector<int> m_bytes;
    QVector<QColor> m_colors;
    QVector<Block> blocks;

    qreal fWin = 3300;
    qreal noise = 120;

    qreal selBegin = 0, selEnd = 0;

    void reset();
    void addSample(Sample smp);

    void plotWave();

    void processWave(int begin, int end);
    void selectBlock(int idx);
    void showBlock(int idx);
    void clearSelection();

    int indexByTime(qreal value);
    qreal timeByIndex(int index);

protected:
    void paintGL() override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
//    void wheelEvent(QWheelEvent *e) override;

private slots:
    void onBoundsChange();

};

#endif // WAVEWIDGET_H
