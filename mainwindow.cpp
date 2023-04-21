#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setWindowTitle("ZX tape master");

    m_wave = new WaveWidget();
    m_wave->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
//    m_wave->setAttribute(Qt::WA_AlwaysStackOnTop);
    m_wave->setMinimumSize(600, 200);
//    m_wave->setBackColor(Qt::transparent);

    QSpinBox *boxChan = new QSpinBox();
    boxChan->setRange(0, 3);
    connect(boxChan, QOverload<int>::of(&QSpinBox::valueChanged), [=](int value)
    {
        m_wave->channel = value;
    });

    QLineEdit *editFreq = new QLineEdit("3300");
    editFreq->setFixedWidth(60);
    connect(editFreq, &QLineEdit::editingFinished, [=]()
    {
        bool ok;
        float value = editFreq->text().toInt(&ok);
        qDebug() << value;
        if (ok)
            m_wave->fWin = value;
        else
            editFreq->setText(QString::number(lrintf(m_wave->fWin)));
    });

    QLineEdit *editNoise = new QLineEdit("120");
    editNoise->setFixedWidth(60);
    connect(editNoise, &QLineEdit::editingFinished, [=]()
    {
        bool ok;
        float value = editNoise->text().toInt(&ok);
        qDebug() << value;
        if (ok)
            m_wave->noise = value;
        else
            editNoise->setText(QString::number(lrintf(m_wave->noise)));
    });

    QPushButton *saveTapBtn = new QPushButton("Save TAP");
    connect(saveTapBtn, &QPushButton::clicked, this, &MainWindow::saveTap);

    QPushButton *parseProgBtn = new QPushButton("Parse program");
    connect(parseProgBtn, &QPushButton::clicked, this, &MainWindow::parseProgram);

    QToolBar *toolbar = addToolBar("main");
    toolbar->addAction("Open WAV", this, &MainWindow::openWav);
    toolbar->addAction("Save WAV", this, &MainWindow::saveWav);
    toolbar->addAction("Save selection", this, &MainWindow::saveSelection);
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Channel:"));
    toolbar->addWidget(boxChan);
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Filter freq:"));
    toolbar->addWidget(editFreq);
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Noise:"));
    toolbar->addWidget(editNoise);
    toolbar->addSeparator();
    toolbar->addAction("Process", this, &MainWindow::processWave);
    toolbar->addAction("Process selection", this, &MainWindow::processSelection);
    toolbar->addSeparator();
    toolbar->addAction("Open TAP", this, &MainWindow::openTap);

    addToolBarBreak();
    toolbar = addToolBar("Edit");
    toolbar->addAction("Remove selection", m_wave, &WaveWidget::removeSelection);
    toolbar->addAction("Relax selection", m_wave, &WaveWidget::relaxSelection);
    toolbar->addAction("LPF selection", m_wave, &WaveWidget::lpfSelection);
    toolbar->addAction("Filter selection", m_wave, &WaveWidget::filterSelection);
    toolbar->addAction("Gain selection", [=]()
    {
        m_wave->gainSelection(2.0);
    });
    toolbar->addAction("Swap channels", m_wave, &WaveWidget::swapChannels);
    toolbar->addAction("Draw sync grid", m_wave, &WaveWidget::syncGrid);

//    toolbar->addSeparator();
//    toolbar->addAction("Parse program", this, &MainWindow::parseProgram);

    m_blockList = new QListWidget;
    connect(m_blockList, &QListWidget::clicked, [=](const QModelIndex &index)
    {
        int idx = index.row();
        showBlock(idx);
    });
    connect(m_blockList, &QListWidget::doubleClicked, [=](const QModelIndex &index)
    {
        int idx = index.row();
        m_wave->showBlock(idx);
    });

    QWidget *blockWidget = new QWidget;
    QVBoxLayout *blocklay = new QVBoxLayout;
    blocklay->setContentsMargins(0, 0, 0, 0);
    blockWidget->setLayout(blocklay);
    blocklay->addWidget(m_blockList);
    blocklay->addWidget(parseProgBtn);
    blocklay->addWidget(saveTapBtn);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_wave);
    splitter->addWidget(blockWidget);
    splitter->setStretchFactor(0, 1);

    QGridLayout *vlay = new QGridLayout;
    ui->centralwidget->setLayout(vlay);
    vlay->addWidget(splitter, 0, 0);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::onTimer);
    timer->start();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onTimer()
{
//    m_wave->update();
}

void MainWindow::openWav()
{
    int wlen = 0;
    QString filename = QFileDialog::getOpenFileName(0L, "Open WAV file", QString(), "*.wav");
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
    {
        setWindowTitle(QString("ZX tape master - %1").arg(QFileInfo(filename).fileName()));

        log("File opened");
        m_wave->reset();

        char id[5] = "    ";
        uint32_t rifflen, fmtlen, datalen;
        WAVEfmt &fmt = m_wave->fmt;

        file.read(id, 4);
        if (!strcmp(id, "RIFF"))
        {
            file.read(reinterpret_cast<char*>(&rifflen), 4);
            qDebug() << "RIFF length =" << rifflen;
            do
            {
                if (file.atEnd())
                {
                    error("no WAVEfmt chunk");
                    file.close();
                    return;
                }
                file.read(id, 4);
            } while (strcmp(id, "fmt "));

            log("WAVE format:");
            file.read(reinterpret_cast<char*>(&fmtlen), 4);
            if (fmtlen == sizeof(WAVEfmt))
            {
                file.read(reinterpret_cast<char*>(&fmt), fmtlen);
                log(QString("Type = %1").arg(fmt.type));
                log(QString("Sampling freq = %1").arg(fmt.freq));
                log(QString("Channels = %1").arg(fmt.chan));
                log(QString("Bits per sample = %1").arg(fmt.bits));
                log(QString("Bytes per sample = %1").arg(fmt.bytes));
//                uint32_t mask = (1 << fmt.bits) - 1;
                file.read(id, 4);
                if (!strcmp(id, "data"))
                {
                    log("Reading data...");
                    file.read(reinterpret_cast<char*>(&datalen), 4);
                    if (fmt.type == 0x01)
                    {
                        wlen = datalen / fmt.bytes;
                        log(QString("Sample count = %1").arg(wlen));
                        Sample smp;
                        for (int i=0; i<wlen; i++)
                        {
                            file.read(reinterpret_cast<char*>(&smp), fmt.bytes);
                            m_wave->addSample(smp);
                        }
                    }
                    else
                    {
                        error("format type is not supported");
                    }
                }
                else
                {
                    error("no data chunk");
                }
            }
            else
            {
                error("WAVEfmt size is wrong");
            }
        }
        else
        {
            error("no RIFF signature");
        }

        file.close();
        log("File closed");
    }
    else
    {
        setWindowTitle(QString("ZX tape master"));
    }

    log("WAV file have been read successfully");

    m_wave->plotWave();
    m_wave->showEntireWave();
//    if (wlen)
//        m_wave->setBounds(0, -32768, m_wave->timeByIndex(wlen), 32768);
//    m_wave->update();
}

void MainWindow::openTap()
{
    QString filename = QFileDialog::getOpenFileName(0L, "Open TAP file", QString(), "*.TAP");
    QFile file(filename);
    if (file.open(QIODevice::ReadOnly))
    {
        setWindowTitle(QString("ZX tape master - %1").arg(QFileInfo(filename).fileName()));

        QByteArray ba = file.readAll();
        char *ptr = ba.data();
        char *end = ptr + ba.size();
        file.close();

        log("File opened");
        m_wave->reset();
        m_wave->fmt.type = 0x01;
        m_wave->fmt.freq = 44100;
        m_wave->fmt.chan = 1;
        m_wave->fmt.bits = 16;
        m_wave->fmt.bytes = 2;
        m_wave->fmt.bps = m_wave->fmt.freq * m_wave->fmt.bytes;

        while (ptr < end)
        {
            uint16_t len = *reinterpret_cast<uint16_t*>(ptr);
            ptr += 2;

            log(QString("Block (len=%1)").arg(len));

            int pilotCount = 1500;
            if (len == 19)
                pilotCount = 3000;
            for (int i=0; i<pilotCount; i++)
                m_wave->generateBit('p');
            m_wave->generateBit('s');
            for (int i=0; i<len; i++)
            {
                uint8_t b = *ptr++;
                for (uint8_t mask=0x80; mask; mask>>=1)
                    m_wave->generateBit(b & mask? '1': '0');
            }
            m_wave->generateBit('e');
            m_wave->generateSilence(1.0);
        }
    }
    m_wave->plotWave();
    m_wave->showEntireWave();
    qApp->processEvents();
    processWave();
}

void MainWindow::saveWav()
{
    saveInterval(0, m_wave->lastIndex());
}

void MainWindow::saveSelection()
{
    saveInterval(m_wave->beginIndex(), m_wave->endIndex());
}

void MainWindow::saveInterval(int begin, int end)
{
    int wlen = end - begin;
    if (wlen <= 0)
    {
        QMessageBox::information(0L, "Save selection", "Nothing selected!");
        return;
    }

    QString filename = QFileDialog::getSaveFileName(0L, "Save WAV file", QString(), "*.wav");
    if (filename.isEmpty())
        return;
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly))
    {
        int sampleSize = m_wave->fmt.bytes;
        uint32_t fmtlen = sizeof(WAVEfmt);
        uint32_t datalen = wlen * sampleSize;
        uint32_t rifflen = fmtlen + datalen + 12;
        file.write("RIFF", 4);
        file.write(reinterpret_cast<const char *>(&rifflen), 4);
        file.write("WAVE", 4);
        file.write("fmt ", 4);
        file.write(reinterpret_cast<const char *>(&fmtlen), 4);
        file.write(reinterpret_cast<const char *>(&m_wave->fmt), fmtlen);
        file.write("data", 4);
        file.write(reinterpret_cast<const char *>(&datalen), 4);
        for (int i=begin; i<end; i++)
        {
            Sample smp = m_wave->getSample(i);
            file.write(reinterpret_cast<const char *>(&smp), sampleSize);
        }
        file.close();
    }
}

void MainWindow::processWave()
{
    m_wave->processWave(0, m_wave->m_chans[0].size());
    collectBlocks();
}

void MainWindow::processSelection()
{
    int begin = m_wave->indexByTime(m_wave->selBegin);
    int end = m_wave->indexByTime(m_wave->selEnd);
    if (begin != end)
    {
        m_wave->processWave(begin, end);
        collectBlocks();
    }
}

void MainWindow::collectBlocks()
{
    m_blockList->clear();
    for (Block &block: m_wave->blocks)
    {
        QByteArray ba = block.ba;
        QString s = "Block";
        if (!block.valid)
            s = "Data";
        s += QString(" (length=%1)").arg(ba.size());
        QString name = "";
        if (ba[0] == '\0' && ba.size() >= 19) // header
        {
            TapHeader *hdr = reinterpret_cast<TapHeader*>(ba.data());
            name = QString::fromLatin1(hdr->name, 10);
            switch (hdr->fileType)
            {
            case 0x00:
                s = QString("Program: %1 (Length=%2)").arg(name).arg(hdr->dataLength);
                break;

            case 0x01:
                s = QString("Numeric array: %1").arg(name);
                break;

            case 0x02:
                s = QString("Symbol array: %1").arg(name);
                break;

            case 0x03:
                s = QString("Bytes: %1 (%2:%3)").arg(name).arg(hdr->param).arg(hdr->dataLength);
                break;
            }

            if (ba.size() != 19)
                s += " [non-standard]";
        }
        if (block.bitCount & 7)
        {
            s += " [bad bit count]";
//            qDebug() << "bitCount" << block.bitCount;
        }
        if (block.checksum)
        {
            s += " [bad CRC]";
            qDebug() << "Block" << s << "CRC =" << block.checksum;
        }
        m_blockList->addItem(s);
        m_blockList->item(m_blockList->count() - 1)->setData(Qt::UserRole, name);
    }
}

void MainWindow::showBlock(int idx)
{
    m_wave->selectBlock(idx);
}

void MainWindow::saveTap()
{
    if (m_wave->blocks.isEmpty())
    {
        QMessageBox::information(0L, "Save TAP", "No blocks to save");
        return;
    }

    QString name = m_blockList->item(0)->data(Qt::UserRole).toString();
    QString filename = QFileDialog::getSaveFileName(0L, "Save TAP file", name, "*.TAP");
    if (filename.isEmpty())
        return;
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly))
    {
        for (Block &block: m_wave->blocks)
        {
            uint16_t length = block.ba.size();
            file.write(reinterpret_cast<const char *>(&length), 2);
            file.write(block.ba);
        }
        file.close();
    }
}

void MainWindow::parseProgram()
{
    QStringList list;
    int idx = m_blockList->currentRow();
    if (idx >= 0 && idx < m_wave->blocks.size())
    {
        Block &block = m_wave->blocks[idx];
        char *ptr = block.ba.data() + 1; // skip byte meaning type of the block
        char *end = ptr + block.ba.size() - 2; // chop CRC byte
        QString line;
        while (ptr < end)
        {
            uint16_t lineNumber = ((uint8_t)ptr[0] << 8) | (uint8_t)ptr[1];
            ptr += 2;
            uint16_t length = ((uint8_t)ptr[0] << 8) | (uint8_t)ptr[1];
            ptr += 2;

            line = QString().sprintf("% 4d", lineNumber);

            for (int i=0; /*i<length &&*/ ptr < end; i++)
            {
//                QString tkn;
                uint8_t chr = *ptr++;
                if (chr == 0x0d) // enter
                    break;
                else if (chr == 0x0e) // number
                {
                    ptr += 5;
//                    tkn = m_wave->zxNumber(ptr);
                }
                else if (chr >= 0x20)
                {
                    line += m_wave->zxChar(chr);
                }
            }

            list << line;
        }

    }

    QPlainTextEdit *edit = new QPlainTextEdit(this);
    edit->setWindowFlags(Qt::Tool);
    QFont font("Consolas", 8);
    edit->setFont(font);
    for (QString &line: list)
        edit->appendPlainText(line);
    edit->show();
}

void MainWindow::log(QString text)
{
    qInfo() << text;
}

void MainWindow::error(QString text)
{
    qCritical() << text;
}


