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
    QVector<float> m_signal;
    QVector<float> m_chans[4];
    QVector<char> m_bits;
    QVector<int> m_bytes;
    QVector<QColor> m_colors;
    QVector<Block> blocks;

    float fWin = 3300;
    float noise = 120;

    float selBegin = 0, selEnd = 0;

    void reset();
    void addSample(Sample smp);

    void plotWave();

    void processWave(int begin, int end);
    void highlightBlock(int idx);

protected:
    void paintGL() override;
//    void wheelEvent(QWheelEvent *e) override;

private slots:
    void onBoundsChange();

};

#endif // WAVEWIDGET_H
