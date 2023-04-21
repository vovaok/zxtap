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

    setFocusPolicy(Qt::ClickFocus);

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
        case 32: m_chans[c] << (smp.data32[c] >> 16); break;
        }
    }
}

Sample WaveWidget::getSample(int index)
{
    Sample smp;
    memset(&smp, 0, sizeof(Sample));
    if (index > 0 && index < m_chans[0].size())
    {
        for (int c=0; c<fmt.chan; c++)
        {
            float v = m_chans[c][index];
            switch (fmt.bits)
            {
            case 8: smp.data8[c] = qBound(-0x80, (int)lrintf(v / 256), 0x7F) + 0x80; break;
            case 16: smp.data16[c] = qBound(-0x7FFF, (int)lrintf(v), 0x7FFF); break;
            case 32: smp.data32[c] = qBound(-0x7FFFFFFF, (int)lrintf(v) << 16, 0x7FFFFFFF); break;
            }
        }
    }
    return smp;
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

    for (Block &block: blocks)
    {
        qreal begin = timeByIndex(block.begin);
        qreal end = timeByIndex(block.end);
        QRectF br = b;
        br.setLeft(begin);
        br.setRight(end);
        if (br.intersects(b))
        {
            br = comb.mapRect(br);
            br.setTop(15);
            br.setBottom(75);
            painter.setPen(QColor(128, 128, 0, 128));
            painter.setBrush(QColor(255, 255, 0, 64));
            painter.drawRect(br);
            if (!block.thumb.isNull() && br.width() >= 256)
                painter.drawImage(br.left(), 75, block.thumb);
        }
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
            if (b >= 0)
            {
                qreal x = i / (qreal)fmt.freq;
                x = comb.map(QPointF(x, 0)).x();
                painter.drawLine(x, 55, x, 75);
                painter.drawText(x+1, 70, zxChar(b));
//                if (b >= 0x20 && b < 0x80)
//                    painter.drawText(x+1, 70, ba);
//                else
//                    painter.drawText(x+1, 70, ba.toHex());
            }
        }
    }

    QRectF tr = b;
    tr.setBottom(T_pilot * 0.9);
    tr.setTop(T_pilot * 1.1);
    painter.setPen(Qt::transparent);
    painter.setBrush(QColor(255, 0, 0, 64));
    painter.drawRect(comb.mapRect(tr));
    tr.setBottom(T_pilot * 0.6);
    tr.setTop(T_pilot * 0.9);
    painter.setBrush(QColor(255, 255, 0, 64));
    painter.drawRect(comb.mapRect(tr));
    tr.setBottom(T_pilot * 0.2);
    tr.setTop(T_pilot * 0.6);
    painter.setBrush(QColor(0, 0, 255, 64));
    painter.drawRect(comb.mapRect(tr));

    if (syncT && z < 5e-5f)
    {
        painter.setPen(Qt::black);
        for (qreal t=syncOffset; t<b.right(); t+=syncT)
        {
            if (t < b.left())
                continue;
            QPointF p1(t, -10000);
            QPointF p2(t, 10000);
            painter.drawLine(comb.map(p1), comb.map(p2));
        }
        for (qreal t=syncOffset; t>b.left(); t-=syncT)
        {
            if (t > b.right())
                continue;
            QPointF p1(t, -10000);
            QPointF p2(t, 10000);
            painter.drawLine(comb.map(p1), comb.map(p2));
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

void WaveWidget::mousePressEvent(QMouseEvent *event)
{
    GraphWidget::mousePressEvent(event);
    if (event->buttons() & Qt::RightButton)
    {
        QMatrix4x4 T = viewTransform().inverted();
        QPointF p = T.map(QPointF(event->pos()));
        selBegin = selEnd = p.x();
        update();
    }
}

void WaveWidget::mouseMoveEvent(QMouseEvent *event)
{
    GraphWidget::mouseMoveEvent(event);
    if (event->buttons() & Qt::RightButton)
    {
        QMatrix4x4 T = viewTransform().inverted();
        QPointF p = T.map(QPointF(event->pos()));
        selEnd = p.x();
        update();
    }
}

void WaveWidget::mouseReleaseEvent(QMouseEvent *event)
{
    GraphWidget::mouseReleaseEvent(event);
    if (selEnd < selBegin)
        qSwap(selEnd, selBegin);
    update();
}

void WaveWidget::keyPressEvent(QKeyEvent *event)
{
    GraphWidget::keyPressEvent(event);

    int begin = beginIndex();
    int end = endIndex();
    int len = end - begin;

    if (event->modifiers() & Qt::ControlModifier)
    {

        if (event->key() == Qt::Key_X)
        {
            for (int i=0; i<4; i++)
            {
                if (!m_chans[i].isEmpty())
                {
                    m_clipboard[i] = m_chans[i].mid(begin, len);
                    m_chans[i].remove(begin, end);
                }
                else
                    m_clipboard[i].clear();
            }
            selEnd = selBegin;
        }
        else if (event->key() == Qt::Key_C)
        {
            for (int i=0; i<4; i++)
            {
                if (!m_chans[i].isEmpty())
                    m_clipboard[i] = m_chans[i].mid(begin, len);
                else
                    m_clipboard[i].clear();
            }
        }
        else if (event->key() == Qt::Key_V)
        {
            bool insert = false;
            if (event->modifiers() & Qt::ShiftModifier)
                insert = true;
            int len = m_clipboard[0].size();
            for (int i=0; i<4; i++)
            {
                if (!m_clipboard[i].isEmpty())
                {
                    int len = m_clipboard[i].size();
                    for (int j=0; j<len; j++)
                    {
                        if (j >= m_clipboard[i].size())
                            break;
                        if (insert)
                            m_chans[i].insert(j + begin, m_clipboard[i][j]);
                        else
                            m_chans[i][j + begin] = m_clipboard[i][j];
                    }
                }
            }
            selEnd = selBegin + timeByIndex(len);
        }
        else
        {
            return;
        }
        plotWave();
        update();
    }
    else if (event->key() == Qt::Key_Delete)
    {
        removeSelection();
    }
    else if (event->key() == Qt::Key_0)
    {
        if (begin >= 0 && begin < lastIndex())
        {
            int end = begin + indexByTime((T_pilot * 0.4) * 1e-6);
            for (int i=begin; i<end && m_bits[i]==' '; i++)
                m_colors[i] = m_signal[i] > 0? Qt::yellow: Qt::blue;
            m_bits[begin] = '0';
        }
        collectBytes(0, lastIndex());
    }
    else if (event->key() == Qt::Key_1)
    {
        if (begin >= 0 && begin < lastIndex())
        {
            int end = begin + indexByTime((T_pilot * 0.8) * 1e-6);
            for (int i=begin; i<end && m_bits[i]==' '; i++)
                m_colors[i] = m_signal[i] > 0? Qt::yellow: Qt::blue;
            m_bits[begin] = '1';
        }
        collectBytes(0, lastIndex());
    }
    else if (event->key() == Qt::Key_Space)
    {
        if (begin >= 0 && begin < lastIndex())
        {
            int idx = begin;
            for (idx=begin; idx<end && m_bits[idx]==' '; idx++);
            m_bits[idx] = ' ';

            for (int i=begin; i<lastIndex() && m_bits[i]==' '; i++)
                m_colors[i] = Qt::transparent;
        }
        collectBytes(0, lastIndex());
    }
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

    for (int c=0; c<4; c++)
    {
        if (m_chans[c].isEmpty())
            continue;

        int sz = m_chans[c].size();
//        if (c == 0)
//        {
//            setBounds(0, -32768, timeByIndex(sz), 32768);
//        }

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
        }
    }
}

void WaveWidget::processWave(int begin, int end)
{
    if (channel < 0 || channel >= 4)
        return;

    filterWave(begin, end);

    analyseBits(begin, end);

    collectBytes(begin, end);

    drawScreenData();
}

void WaveWidget::filterWave(int begin, int end)
{
    QVector<qreal> &chan = m_chans[channel];

    if (chan.isEmpty())
        return;

    int cnt = chan.size();
    m_signal.resize(cnt);
    m_bits.resize(cnt);
    m_bytes.resize(cnt);
    m_colors.resize(cnt);
    qreal *signal = m_signal.data();

    begin = qMax(begin, 0);
    end = qMin(end, cnt);

    int win = fWin? fmt.freq / fWin: 1; // HPF 4000 Hz
    //        int win2 = fmt.freq / 20000;
    //        qreal oldy = 0;

    Graph *fg = graph("filter");
    fg->clear();
    qreal *samples = chan.data();

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
}

void WaveWidget::analyseBits(int begin, int end)
{
    QVector<qreal> &chan = m_chans[channel];
    qreal *samples = chan.data();
    qreal *signal = m_signal.data();
    char *bits = m_bits.data();
    QColor *colors = m_colors.data();

    Graph *proc = graph("proc");
    proc->clear();

    qreal T = 0;
    char bit = ' ';
    int oldi = 0;
    //    qreal oldf = 0;

    qreal f = 0;
    T_pilot = 1300; // average interval of pilot-tone in microseconds
    // T_one = 0.8 * T_pilot
    // T_zero = 0.4 * T_pilot

    int bitcnt = 0;

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
                if (T < T_pilot * 0.3)//300 //400)
                {
                    continue;
                    //                    T = 0;
                    //                    bit = ' ';
                }
                else if (T < T_pilot * 0.6)
                {
                    if (bit == 'p')
                        bit = 's';
                    else //if (bit != ' ')
                    {
                        bit = '0';
                        bitcnt++;
                    }
                }
                else if (T < T_pilot * 0.9)
                {
                    //if (bit != ' ')
                    bit = '1';
                    bitcnt++;
                }
                else if (T < T_pilot * 1.1)
                {
                    bit = 'p';
                    T_pilot = T_pilot * 0.99 + T * 0.01;
                    bitcnt = 0;
                }
                else
                {
                    T = 0;
                    if ((bit == '0' || bit == '1') && bitcnt >= 8)
                    {
                        if (samples[oldi] > 0)
                        {
                            bit = 'e';
                        }
                        else
                        {
                            bit = ' ';
                            for (int j=i-1; j; --j)
                            {
                                colors[j] = signal[j]>0? Qt::red: Qt::cyan;
                                if (bits[j] != ' ')
                                {
                                    bits[j] = 'e';
                                    break;
                                }
                            }
                        }
                    }
                    else
                        bit = ' ';
                    //                    T_pilot = 1300; // reset pilot-tone interval
                    bitcnt = 0;
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
}

void WaveWidget::collectBytes(int begin, int end)
{
    blocks.clear();
    char *bits = m_bits.data();

    Block block;
    uint8_t byte = 0;
    uint8_t mask = 0x80;
    int pilotCnt = 0;
    int oldi = 0;
    enum {Idle, Pilot, Data} state = Idle;
    for (int i=begin; i<end; i++)
    {
        m_bytes[i] = -1;

        char bit = bits[i];
        if (bit == ' ')
            continue;

        qreal T = (i - oldi) * 1000000.f / fmt.freq; // period in us
        if (T > 1500)
        {
            bit = ' ';
            state = Idle;
        }

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
            else if (bit == 'e')
            {
                //                qDebug() << "unsdelano data";
                // backwards parsing...
                int oldj = i - 1;
                int bitcount = 0;
                int bti = i;
                for (int j=oldj; j; --j)
                {
                    qreal T = (oldj - j) * 1000000.f / fmt.freq; // period in us
                    if (T > 1500 || bits[j] == 'p' || bits[j] == 's' || bits[j] == 'e')
                    {
                        //                        qDebug() << "bits" << bitcount;
                        if (bitcount >= 40)
                        {
                            oldi = i = bti - 1;
                            state = Data;
                            break;
                        }
                        else
                        {
                            oldi = i;
                            break;
                        }
                    }
                    if (bits[j] == ' ')
                        continue;
                    bitcount++;
                    if (!(bitcount & 7))
                    {
                        bti = j;
                    }
                    oldj = j;
                }
                continue;
            }
            break;

        case Pilot:
            if (bit == 's' && pilotCnt > 20)
            {
                state = Data;
                block.valid = true;
            }
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
                    //                    if ((block.bitCount & 7) == 1)
                    //                    {
                    //                        block.bitCount--;
                    //                        block.end = oldi;
                    //                    }
                    //                    else
                    //                    {
                    block.end = i;
                    //                    }
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

void WaveWidget::drawScreenData()
{
    for (Block &block: blocks)
    {
        if (block.ba.size() == 6914)
        {
            ZxScreen scr(block.ba.data() + 1);
            block.thumb = scr.toImage();
        }
    }
    update();
}

void WaveWidget::selectBlock(int idx)
{
    if (idx < 0 || idx >= blocks.size())
        return;
    Block &block = blocks[idx];
    selBegin = timeByIndex(block.begin);
    selEnd = timeByIndex(block.end);
//    setXmin(selBegin);
//    setXmax(selEnd);
    update();
}

void WaveWidget::showEntireWave()
{
//    qreal lim = 1 << (fmt.bits - 1);
    qreal lim = 32768;
    qreal end = timeByIndex(m_chans[0].size());
    setBounds(0, -lim, end, lim);
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

void WaveWidget::clearSelection()
{
    selBegin = selEnd = 0;
    update();
}

void WaveWidget::removeSelection()
{
    int begin = beginIndex();
    int end = endIndex();
    for (int c=0; c<4; c++)
    {
        if (end > m_chans[c].size())
            continue;
        m_chans[c].remove(begin, end - begin);
    }

    selEnd = selBegin;

    plotWave();
    update();
}

void WaveWidget::relaxSelection()
{
    int begin = beginIndex();
    int end = endIndex();
    qreal win = end - begin;
    for (int c=0; c<4; c++)
    {
        auto &chan = m_chans[c];
        if (chan.isEmpty())
            continue;
        int cnt = chan.size();
        qreal *samples = chan.data();
        QVector<qreal> newSamples;
        for (int i=begin; i<=end; i++)
        {
            qreal s1 = samples[begin] * (end - i) / win;
            qreal s2 = samples[end] * (i - begin) / win;
            qreal s = s1 + s2;
            newSamples << s;
        }

        cnt = newSamples.size();
        for (int i=0; i<cnt; i++)
        {
            samples[i+begin] = newSamples[i];
        }
    }

    plotWave();
    update();
}

void WaveWidget::filterSelection()
{
    int begin = beginIndex();
    int end = endIndex();
    for (int i=begin; i<end; i++)
    {
        m_chans[channel][i] = m_signal[i];
    }
    plotWave();
    update();
}

void WaveWidget::lpfSelection()
{
    int begin = beginIndex();
    int end = endIndex();
    qreal win = 4;
    for (int c=0; c<4; c++)
    {
        auto &chan = m_chans[c];
        if (chan.isEmpty())
            continue;
        int cnt = chan.size();
        qreal *samples = chan.data();
        QVector<qreal> newSamples;

        for (int i=begin; i<end; i++)
        {
            qreal v = 0;
            for (int j=-win; j<=win; j++)
                v += samples[i+j];
            v /= win * 2 + 1;
            newSamples << v;
        }

        cnt = newSamples.size();
        for (int i=0; i<cnt; i++)
        {
            samples[i+begin] = newSamples[i];
        }
    }

    plotWave();
    update();
}

void WaveWidget::gainSelection(qreal value)
{
    int begin = beginIndex();
    int end = endIndex();
    for (int c=0; c<4; c++)
    {
        if (m_chans[c].size() < end)
            continue;
        for (int i=begin; i<end; i++)
        {
            m_chans[c][i] *= value;
        }
    }
    plotWave();
    update();
}

void WaveWidget::swapChannels()
{
    int begin = beginIndex();
    int end = endIndex();
    for (int i=begin; i<end; i++)
    {
        qSwap(m_chans[0][i], m_chans[1][i]);
    }
    plotWave();
    update();
}

void WaveWidget::syncGrid()
{
    setGridColor(QColor(0, 0, 0, 32));
    int begin = indexByTime(bounds().left());
    int end = indexByTime(bounds().right());
    qDebug() << begin<<end;
    qreal T0 = 0;//T_pilot * 0.4;
    int Tcnt = 0;
    qreal f = 0;
    syncOffset = 0;
    int oldi = begin;
    for (int i=begin; i<end; i++)
    {
        if (i<0 || i>= m_signal.size())
            continue;

        qreal y = m_signal[i];

        if (y > noise)
        {
            if (f < 0)
            {
                float T = (i - oldi) * 1000000.0 / (qreal)fmt.freq; // period in us
                if (T < 0.3 * T_pilot)
                    continue;
                else if (T < 0.6 * T_pilot)
                {
                    T0 += T;
//                    qDebug() << T;
                    Tcnt++;
                    if (!syncOffset)
                        syncOffset = timeByIndex(i);
                }
                else if (T < 0.9 * T_pilot)
                {
//                    qDebug() << T;
                    T0 += T/2;
                    Tcnt++;
                }
                oldi = i;
            }
            f = 10000;
        }
        else if (y < -noise)
        {
            f = -10000;
        }
    }

    if (Tcnt)
    {
        T0 /= Tcnt;
        T0 = 476.19;
        syncT = T0 * 1e-6;
        qDebug() << "syncT =" << syncT << "syncOffset =" << syncOffset;
    }

    update();
}

void WaveWidget::generateBit(char bit)
{
    qreal T = 0;
    switch (bit)
    {
    case 'p': T = T_pilot; break;
    case 's': T = T_pilot * 0.3; break;
    case '0': T = T_pilot * 0.4; break;
    case '1': T = T_pilot * 0.8; break;
    case 'e': T = T_pilot; break;
    default: return;
    }

    QVector<qreal> &chan = m_chans[channel];

    int wlen = indexByTime(T * 1e-6);
    if (wlen)
    {
        qreal mag = 1 << (fmt.bits - 2);
        for (int i=0; i<wlen; i++)
        {
            qreal value = mag * sin(i*2*M_PI/wlen);
            chan << value;
        }
    }
    else // place silence
    {
        for (int i=0; i<10000; i++)
            m_chans[0] << 0.0;
    }

}

void WaveWidget::generateSilence(qreal time)
{
    QVector<qreal> &chan = m_chans[channel];
    int wlen = indexByTime(time);
    for (int i=0; i<wlen; i++)
        chan << 0.0;
}

int WaveWidget::indexByTime(qreal value) const
{
    return qBound(0, (int)lroundf(value * fmt.freq), m_chans[0].size());
}

qreal WaveWidget::timeByIndex(int index) const
{
    return index / (qreal)fmt.freq;
}

QString WaveWidget::zxChar(uint8_t code)
{
    switch (code)
    {
    case 0x06: return "<comma>";
    case 0x08: return "<left>";
    case 0x09: return "<right>";
    case 0x0d: return "<enter>";
    case 0x0e: return "<number>";
    case 0x10: return "<INK>";
    case 0x11: return "<PAPER>";
    case 0x12: return "<FLASH>";
    case 0x13: return "<BRIGHT>";
    case 0x14: return "<INVERSE>";
    case 0x15: return "<OVER>";
    case 0x16: return "<AT>";
    case 0x17: return "<TAB>";
    case 0x20: return " ";
    case 0x21: return "!";
    case 0x22: return "\"";
    case 0x23: return "#";
    case 0x24: return "$";
    case 0x25: return "%";
    case 0x26: return "&";
    case 0x27: return "'";
    case 0x28: return "(";
    case 0x29: return ")";
    case 0x2a: return "*";
    case 0x2b: return "+";
    case 0x2c: return ",";
    case 0x2d: return "-";
    case 0x2e: return ".";
    case 0x2f: return "/";
    case 0x30: return "0";
    case 0x31: return "1";
    case 0x32: return "2";
    case 0x33: return "3";
    case 0x34: return "4";
    case 0x35: return "5";
    case 0x36: return "6";
    case 0x37: return "7";
    case 0x38: return "8";
    case 0x39: return "9";
    case 0x3a: return ":";
    case 0x3b: return ";";
    case 0x3c: return "<";
    case 0x3d: return "=";
    case 0x3e: return ">";
    case 0x3f: return "?";
    case 0x40: return "@";
    case 0x41: return "A";
    case 0x42: return "B";
    case 0x43: return "C";
    case 0x44: return "D";
    case 0x45: return "E";
    case 0x46: return "F";
    case 0x47: return "G";
    case 0x48: return "H";
    case 0x49: return "I";
    case 0x4a: return "J";
    case 0x4b: return "K";
    case 0x4c: return "L";
    case 0x4d: return "M";
    case 0x4e: return "N";
    case 0x4f: return "O";
    case 0x50: return "P";
    case 0x51: return "Q";
    case 0x52: return "R";
    case 0x53: return "S";
    case 0x54: return "T";
    case 0x55: return "U";
    case 0x56: return "V";
    case 0x57: return "W";
    case 0x58: return "X";
    case 0x59: return "Y";
    case 0x5a: return "Z";
    case 0x5b: return "[";
    case 0x5c: return "\\";
    case 0x5d: return "]";
    case 0x5e: return "↑";
    case 0x5f: return "_";
    case 0x60: return "£";
    case 0x61: return "a";
    case 0x62: return "b";
    case 0x63: return "c";
    case 0x64: return "d";
    case 0x65: return "e";
    case 0x66: return "f";
    case 0x67: return "g";
    case 0x68: return "h";
    case 0x69: return "i";
    case 0x6a: return "j";
    case 0x6b: return "k";
    case 0x6c: return "l";
    case 0x6d: return "m";
    case 0x6e: return "n";
    case 0x6f: return "o";
    case 0x70: return "p";
    case 0x71: return "q";
    case 0x72: return "r";
    case 0x73: return "s";
    case 0x74: return "t";
    case 0x75: return "u";
    case 0x76: return "v";
    case 0x77: return "w";
    case 0x78: return "x";
    case 0x79: return "y";
    case 0x7a: return "z";
    case 0x7b: return "{";
    case 0x7c: return "|";
    case 0x7d: return "}";
    case 0x7e: return "~";
    case 0x7f: return "©";
    case 0x80: return " ";
    case 0x81: return "▝";
    case 0x82: return "▘";
    case 0x83: return "▀";
    case 0x84: return "▗";
    case 0x85: return "▐";
    case 0x86: return "▚";
    case 0x87: return "▜";
    case 0x88: return "▖";
    case 0x89: return "▞";
    case 0x8a: return "▌";
    case 0x8b: return "▛";
    case 0x8c: return "▄";
    case 0x8d: return "▟";
    case 0x8e: return "▙";
    case 0x8f: return "█";
    case 0x90: return "A";
    case 0x91: return "B";
    case 0x92: return "C";
    case 0x93: return "D";
    case 0x94: return "E";
    case 0x95: return "F";
    case 0x96: return "G";
    case 0x97: return "H";
    case 0x98: return "I";
    case 0x99: return "J";
    case 0x9a: return "K";
    case 0x9b: return "L";
    case 0x9c: return "M";
    case 0x9d: return "N";
    case 0x9e: return "O";
    case 0x9f: return "P";
    case 0xa0: return "Q";
    case 0xa1: return "R";
    case 0xa2: return "S";
    case 0xa3: return "T";
    case 0xa4: return "U";
//    case 0x90: return "🄰";
//    case 0x91: return "🄱";
//    case 0x92: return "🄲";
//    case 0x93: return "🄳";
//    case 0x94: return "🄴";
//    case 0x95: return "🄵";
//    case 0x96: return "🄶";
//    case 0x97: return "🄷";
//    case 0x98: return "🄸";
//    case 0x99: return "🄹";
//    case 0x9a: return "🄺";
//    case 0x9b: return "🄻";
//    case 0x9c: return "🄼";
//    case 0x9d: return "🄽";
//    case 0x9e: return "🄾";
//    case 0x9f: return "🄿";
//    case 0xa0: return "🅀";
//    case 0xa1: return "🅁";
//    case 0xa2: return "🅂";
//    case 0xa3: return "🅃";
//    case 0xa4: return "🅄";
    case 0xa5: return "RND";
    case 0xa6: return "INKEY$";
    case 0xa7: return "PI";
    case 0xa8: return "FN ";
    case 0xa9: return "POINT ";
    case 0xaa: return "SCREEN$ ";
    case 0xab: return "ATTR ";
    case 0xac: return "AT ";
    case 0xad: return "TAB ";
    case 0xae: return "VAL$ ";
    case 0xaf: return "CODE ";
    case 0xb0: return "VAL ";
    case 0xb1: return "LEN ";
    case 0xb2: return "SIN ";
    case 0xb3: return "COS ";
    case 0xb4: return "TAN ";
    case 0xb5: return "ASN ";
    case 0xb6: return "ACS ";
    case 0xb7: return "ATN ";
    case 0xb8: return "LN ";
    case 0xb9: return "EXP ";
    case 0xba: return "INT ";
    case 0xbb: return "SQR ";
    case 0xbc: return "SGN ";
    case 0xbd: return "ABS ";
    case 0xbe: return "PEEK ";
    case 0xbf: return "IN ";
    case 0xc0: return "USR ";
    case 0xc1: return "STR$ ";
    case 0xc2: return "CHR$ ";
    case 0xc3: return "NOT ";
    case 0xc4: return "BIN ";
    case 0xc5: return " OR ";
    case 0xc6: return " AND ";
    case 0xc7: return "<=";
    case 0xc8: return ">=";
    case 0xc9: return "<>";
    case 0xca: return " LINE ";
    case 0xcb: return " THEN ";
    case 0xcc: return " TO ";
    case 0xcd: return " STEP ";
    case 0xce: return " DEF FN ";
    case 0xcf: return " CAT ";
    case 0xd0: return " FORMAT ";
    case 0xd1: return " MOVE ";
    case 0xd2: return " ERASE ";
    case 0xd3: return " OPEN # ";
    case 0xd4: return " CLOSE # ";
    case 0xd5: return " MERGE ";
    case 0xd6: return " VERIFY ";
    case 0xd7: return " BEEP ";
    case 0xd8: return " CIRCLE ";
    case 0xd9: return " INK ";
    case 0xda: return " PAPER ";
    case 0xdb: return " FLASH ";
    case 0xdc: return " BRIGHT ";
    case 0xdd: return " INVERSE ";
    case 0xde: return " OVER ";
    case 0xdf: return " OUT ";
    case 0xe0: return " LPRINT ";
    case 0xe1: return " LLIST ";
    case 0xe2: return " STOP ";
    case 0xe3: return " READ ";
    case 0xe4: return " DATA ";
    case 0xe5: return " RESTORE ";
    case 0xe6: return " NEW ";
    case 0xe7: return " BORDER ";
    case 0xe8: return " CONTINUE ";
    case 0xe9: return " DIM ";
    case 0xea: return " REM ";
    case 0xeb: return " FOR ";
    case 0xec: return " GO TO ";
    case 0xed: return " GO SUB ";
    case 0xee: return " INPUT ";
    case 0xef: return " LOAD ";
    case 0xf0: return " LIST ";
    case 0xf1: return " LET ";
    case 0xf2: return " PAUSE ";
    case 0xf3: return " NEXT ";
    case 0xf4: return " POKE ";
    case 0xf5: return " PRINT ";
    case 0xf6: return " PLOT ";
    case 0xf7: return " RUN ";
    case 0xf8: return " SAVE ";
    case 0xf9: return " RANDOMIZE ";
    case 0xfa: return " IF ";
    case 0xfb: return " CLS ";
    case 0xfc: return " DRAW ";
    case 0xfd: return " CLEAR ";
    case 0xfe: return " RETURN ";
    case 0xff: return " COPY ";
    default: return QByteArray(reinterpret_cast<const char*>(&code), 1).toHex();
    }
}

QString WaveWidget::zxNumber(uint8_t *data)
{
    if (!data[0]) // this is small integer
    {
        int value = *reinterpret_cast<uint16_t*>(data + 2);
        if (data[1] == 0xFF)
            value = -value;
        return QString::number(value);
    }
    else // this is 40-bit float
    {
        int e = (int)data[0] - 128;
        uint32_t im = *reinterpret_cast<uint32_t*>(data + 1);
        qreal sign = im & 0x80000000? -1.0: 1.0;
        im |= 0x80000000;
        qreal value = sign * (im / (qreal)(1LL << 32)) * (1 << e);
        return QString::number(value);
    }
}
