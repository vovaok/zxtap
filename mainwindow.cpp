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


    QToolBar *toolbar = addToolBar("main");
    toolbar->addAction("Open WAV", this, &MainWindow::openWav);
    toolbar->addAction("Process", this, &MainWindow::processWave);
    toolbar->addAction("Process selection", this, &MainWindow::processSelection);

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("Filter freq:"));
    QLineEdit *editFreq = new QLineEdit("3300");
    editFreq->setFixedWidth(60);
    toolbar->addWidget(editFreq);
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

    toolbar->addWidget(new QLabel("Noise:"));
    QLineEdit *editNoise = new QLineEdit("120");
    editNoise->setFixedWidth(60);
    toolbar->addWidget(editNoise);
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

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(m_wave);
    splitter->addWidget(m_blockList);
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
                        int wlen = datalen / fmt.bytes;
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
        QString s = QString("Block (length=%1)").arg(ba.size());
        if (ba.size() == 19 && ba[0] == '\0') // header
        {
            TapHeader *hdr = reinterpret_cast<TapHeader*>(ba.data());
            QString name = QString::fromLatin1(hdr->name, 10);
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
        }
        if (block.bitCount & 7)
        {
            s += " [bad bit count]";
//            qDebug() << "bitCount" << block.bitCount;
        }
        if (block.checksum)
            s += " [bad CRC]";
        m_blockList->addItem(s);
    }
}

void MainWindow::showBlock(int idx)
{
    m_wave->selectBlock(idx);
}

void MainWindow::log(QString text)
{
    qInfo() << text;
}

void MainWindow::error(QString text)
{
    qCritical() << text;
}


