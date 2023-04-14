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
    int channel = 0;

    float T_pilot = 1300; // average interval of pilot-tone in microseconds
    qreal selBegin = 0, selEnd = 0;

    void reset();
    void addSample(Sample smp);
    Sample getSample(int index);

    void plotWave();

    void processWave(int begin, int end);
    void selectBlock(int idx);
    void showBlock(int idx);
    void clearSelection();

    void removeSelection();
    void relaxSelection();

    int indexByTime(qreal value) const;
    qreal timeByIndex(int index) const;

    int beginIndex() const {return indexByTime(selBegin);}
    int endIndex() const {return indexByTime(selEnd);}

    QString zxChar(uint8_t code);
    QString zxNumber(uint8_t *data);

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
