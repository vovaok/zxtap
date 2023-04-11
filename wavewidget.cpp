#include "wavewidget.h"
#include <QApplication>

WaveWidget::WaveWidget()
{
    setMaxPointCount(1024*1024*32);
//    setAutoBoundsEnabled(true);
    setXlabel("с");
    addGraph("chan0", Qt::red);
    addGraph("chan1", Qt::blue);
    addGraph("filter", Qt::black);
    addGraph("proc", Qt::cyan);

    connect(this, &GraphWidget::boundsChanged, this, &WaveWidget::onBoundsChange);
}

void WaveWidget::reset()
{
    for (auto &ch: m_chans)
        ch.clear();

    GraphWidget::clear();
    resetBounds();
}

void WaveWidget::addSample(Sample smp)
{
    for (int c=0; c<fmt.chan; c++)
    {
        switch (fmt.bits)
        {
        case 8: m_chans[c] << ((int8_t)(smp.data8[c] - 0x80) * 256); break;
        case 16: m_chans[c] << (smp.data16[c]); break;
        case 32: m_chans[c] << (smp.data32[c]); break;
        }
    }
}

void WaveWidget::paintGL()
{
    GraphWidget::paintGL();

    QPainter painter;
    painter.begin(this);

    QMatrix4x4 comb = viewTransform();
    QMatrix4x4 inv = comb.inverted();

    QRectF b = bounds();

    if (selBegin && selEnd)
    {
        QRectF sel = b;
        sel.setLeft(selBegin);
        sel.setRight(selEnd);
        sel = comb.mapRect(sel);
        painter.setPen(QColor(0, 128, 128, 128));
        painter.setBrush(QColor(0, 255, 255, 64));
        painter.drawRect(sel);
    }

    for (int i=0; i<width(); i++)
    {
        int idx = inv.map(QPointF(i, 0)).x() * fmt.freq;
        if (idx >= 0 && idx < m_colors.size())
        {
            QColor c = m_colors[idx];
            painter.setPen(c);
            painter.drawLine(i, 35, i, 55);
        }
    }

    painter.setPen(Qt::black);

    qreal z = b.width() / width();
    if (z < 5e-5f)
    {
        int cnt = m_bits.size();
        int begin = qMax((int)lroundf(b.left() * fmt.freq), 0);
        int end = qMin((int)lroundf(b.right() * fmt.freq), cnt);
        for (int i=begin; i<end; i++)
        {
            char bit = m_bits[i];
            if (bit != ' ')
            {
                qreal x = i / (qreal)fmt.freq;
                x = comb.map(QPointF(x, 0)).x();
                painter.drawLine(x, 15, x, 35);
                painter.drawText(x+1, 30, QString(bit));
            }
        }
    }
    if (z < 5e-5f * 8)
    {
        int cnt = m_bytes.size();
        int begin = qMax((int)lroundf(b.left() * fmt.freq), 0);
        int end = qMin((int)lroundf(b.right() * fmt.freq), cnt);
        for (int i=begin; i<end; i++)
        {
            int b = m_bytes[i];
            QByteArray ba;
            ba.append(b);
            if (b >= 0)
            {
                qreal x = i / (qreal)fmt.freq;
                x = comb.map(QPointF(x, 0)).x();
                painter.drawLine(x, 55, x, 75);
                if (b >= 0x20 && b < 0x80)
                    painter.drawText(x+1, 70, ba);
                else
                    painter.drawText(x+1, 70, ba.toHex());
            }
        }
    }

//    for (int i=0; i<m_colors.size(); i++)
//    {
//        QColor c = m_colors[i];
//        if (c.rgba())
//        {
//            qreal x = i / (qreal)fmt.freq;
//            x = comb.map(QPointF(x, 0)).x();
//            QColor c = m_colors[i];
//            painter.setPen(c);
//            painter.drawLine(x, 50, x, 100);
//        }
//    }

    painter.end();
}

void WaveWidget::onBoundsChange()
{
    QRectF b = bounds();
    int begin = lroundf(b.left() * fmt.freq);
    int end = lroundf(b.right() * fmt.freq);
//    processWave(begin, end);
}

void WaveWidget::plotWave()
{
    GraphWidget::clear();

//    QElapsedTimer etimer;
//    etimer.start();

    for (int c=0; c<4; c++)
    {
        if (m_chans[c].isEmpty())
            continue;

        int sz = m_chans[c].size();
        if (c == 0)
        {
            setBounds(0, -32768, timeByIndex(sz), 32768);
        }

        QString name = QString("chan%1").arg(c);
        Graph *g = graph(name);
        if (!g)
            continue;
        qreal *samples = m_chans[c].data();

        for (int i=0; i<sz; i++)
        {
            qreal x = timeByIndex(i);
            qreal y = samples[i];
            g->addPoint(x, y);

//            if (etimer.elapsed() > 100)
//            {
//                qApp->processEvents();
//                etimer.restart();
//            }
        }
    }
}

void WaveWidget::processWave(int begin, int end)
{
    if (m_chans[0].isEmpty())
        return;

    int cnt = m_chans[0].size();
    m_signal.resize(cnt);
    m_bits.resize(cnt);
    m_bytes.resize(cnt);
    m_colors.resize(cnt);
    qreal *signal = m_signal.data();
    char *bits = m_bits.data();
    QColor *colors = m_colors.data();

    begin = qMax(begin, 0);
    end = qMin(end, cnt);

    int win = fWin? fmt.freq / fWin: 1; // HPF 4000 Hz
    //        int win2 = fmt.freq / 20000;
    //        qreal oldy = 0;

    Graph *fg = graph("filter");
    Graph *proc = graph("proc");
    fg->clear();
    proc->clear();
    qreal *samples = m_chans[0].data();

    for (int i=begin; i<end; i++)
    {
        qreal x = timeByIndex(i);
        qreal y = samples[i];

        qreal avg = 0;
        if (i > win && i < cnt - win)
        {
            for (int k=-win; k<=win; k++)
                avg += samples[i+k];
            avg /= (win * 2);
        }
        //                qreal avg2 = 0;
        //                if (i > win2 && i < sz - win2)
        //                {
        //                    for (int k=-win2; k<=win2; k++)
        //                        avg2 += samples[i+k];
        //                    avg2 /= (win2 * 2);
        //                }
        qreal y1 = y - avg;
        signal[i] = y1;

        //                qreal dy = y - oldy;
        //                oldy = y;

        //                qreal Ky = 0.9f;
        //                y1 = Ky * y1 + (1.f - Ky) * y;
        //                qreal Kf = 0.5f;
        //                f = Kf * f + (1.0f - Kf) * y;
        //                qreal yf = y1 - f;
        fg->addPoint(x, y1);

//        if (etimer.elapsed() > 100)
//        {
//            qApp->processEvents();
//            etimer.restart();
//        }
    }

    update();

    qreal T = 0;
    char bit = ' ';
    int oldi = 0;
//    qreal oldf = 0;

    qreal f = 0;

    for (int i=begin; i<end; i++)
    {
//        qreal x = timeByIndex(i);
        qreal y = signal[i];
        bits[i] = ' ';
        if (y > noise)
        {
            if (f <= 0)
            {
                T = (i - oldi) * 1000000.0 / (qreal)fmt.freq; // period in us
                if (T < 300)//400)
                {
                    T = 0;
                    bit = ' ';
                }
                if (T < 750)
                {
                    if (bit == 'p')
                        bit = 's';
                    else if (bit != ' ')
                        bit = '0';
                }
                else if (T < 1100)
                {
                    if (bit != ' ')
                        bit = '1';
                }
                else if (T < 1500)
                {
                    bit = 'p';
                }
                else
                {
                    T = 0;
                    if (bit == '0' || bit == '1')
                        bit = 'e';
                    else
                        bit = ' ';
                }

                bits[oldi] = bit;
                proc->addPoint(timeByIndex(oldi), T);
                proc->addPoint(timeByIndex(i), T);

                for (int j=oldi; j<i; j++)
                {
                    qreal s = signal[j];
                    QColor c = Qt::transparent;
                    switch (bit)
                    {
                    case 'p':
                    case 's':
                    case 'e':
                        c = s>0? Qt::red: Qt::cyan;
                        break;
                    case '0':
                    case '1':
                        c = s>0? Qt::yellow: Qt::blue;
                        break;
                    }
                    colors[j] = c;
                }

                oldi = i;
            }
            f = 10000;
        }
        if (y < -noise)
            f = -10000;
//        proc->addPoint(x, f);
    }

    update();

    blocks.clear();

    Block block;
    uint8_t byte = 0;
    uint8_t mask = 0x80;
    int pilotCnt = 0;
    oldi = 0;
    enum {Idle, Pilot, Data} state = Idle;
    for (int i=begin; i<end; i++)
    {
        m_bytes[i] = -1;

        char bit = bits[i];
        if (bit == ' ')
            continue;

        qreal T = (i - oldi) * 1000000.f / fmt.freq; // period in us
        if (T > 1500)
            state = Idle;

        switch (state)
        {
        case Idle:
            pilotCnt = 0;
            if (block.ba.size())
            {
                block.bitCount--;
                block.end = oldi;
                blocks << block;
            }
            block.clear();
            byte = 0;
            mask = 0x80;
            if (bit == 'p')
            {
                state = Pilot;
//                qDebug() << "Pilot";
            }
            break;

        case Pilot:
            if (bit == 's' && pilotCnt > 20)
                state = Data;
            else if (bit == 'p')
                pilotCnt++;
            else
                state = Idle;
            break;

        case Data:
            if (bit == '1')
            {
                byte |= mask;
                block.bitCount++;
            }
            else if (bit == '0')
            {
                byte &= ~mask;
                block.bitCount++;
            }
            else
            {
                if (block.ba.size())
                {
                    if ((block.bitCount & 7) == 1)
                    {
                        block.bitCount--;
                        block.end = oldi;
                    }
                    else
                    {
                        block.end = i;
                    }
                    blocks << block;
                    block.clear();
                }
                state = Idle;
            }

            if (block.byteOffset.isEmpty())
                block.begin = i;

            if (mask == 0x80)
                block.byteOffset << i;

            mask >>= 1;

            if (!mask)
            {
//                qDebug() << "Byte: " << std::hex << byte;
                block.ba.append(byte);
                int bo = block.byteOffset.last();
                m_bytes[bo] = byte;
                block.checksum ^= byte;
                mask = 0x80;
            }
            break;
        }

        oldi = i;
    }

    update();
}

void WaveWidget::highlightBlock(int idx)
{
    if (idx < 0 || idx >= blocks.size())
        return;
    Block &block = blocks[idx];
//    qDebug() << "begin" << block.begin << "end" << block.end;
    selBegin = timeByIndex(block.begin);
    selEnd = timeByIndex(block.end);
//    setXmin(selBegin);
//    setXmax(selEnd);
    update();
}

void WaveWidget::showBlock(int idx)
{
    if (idx < 0 || idx >= blocks.size())
        return;
    Block &block = blocks[idx];
    setXmin(timeByIndex(block.begin));
    setXmax(timeByIndex(block.end));
    update();
}

int WaveWidget::indexByTime(qreal value)
{
    return qBound(0, (int)lroundf(value * fmt.freq), m_signal.size());
}

qreal WaveWidget::timeByIndex(int index)
{
    return index / (qreal)fmt.freq;
}
