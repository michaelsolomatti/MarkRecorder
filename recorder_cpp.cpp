// recorder_cpp.cpp — диктофон с закладками на C++ (Qt)

#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QAudioInput>
#include <QAudioOutput>
#include <QAudioFormat>
#include <QBuffer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QTimer>
#include <QThread>
#include <QProgressBar>
#include <QFileInfo>

class Recorder : public QMainWindow {
    Q_OBJECT
public:
    Recorder(QWidget *parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("🎙️ MarkRecorder — C++");
        resize(800, 600);
        createUI();
        loadRecordings();
    }

private slots:
    void startRecording() {
        if (isRecording) return;
        // Настройка аудиоформата
        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QAudioDeviceInfo info = QAudioDeviceInfo::defaultInputDevice();
        if (!info.isFormatSupported(format)) {
            QMessageBox::warning(this, "Ошибка", "Формат не поддерживается");
            return;
        }

        audioInput = new QAudioInput(info, format, this);
        buffer = new QBuffer(this);
        buffer->open(QIODevice::WriteOnly);
        audioInput->start(buffer);

        isRecording = true;
        startTime = QDateTime::currentMSecsSinceEpoch();
        bookmarks.clear();
        recordBtn->setEnabled(false);
        stopBtn->setEnabled(true);
        bookmarkBtn->setEnabled(true);
        statusLabel->setText("Идёт запись...");
        timer->start(100);
    }

    void stopRecording() {
        if (!isRecording) return;
        audioInput->stop();
        buffer->close();
        isRecording = false;
        recordBtn->setEnabled(true);
        stopBtn->setEnabled(false);
        bookmarkBtn->setEnabled(false);
        statusLabel->setText("Запись остановлена");
        timer->stop();

        // Сохраняем файл
        QString filename = QString("запись_%1.wav").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));
        currentFile = filename;
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            // Запись заголовка WAV (упрощённо, для демонстрации)
            // В реальном коде нужен полный заголовок
            file.write(buffer->buffer());
            file.close();
        }
        saveBookmarks(filename.replace(".wav", "_marks.json"));
        loadRecordings();
        statusLabel->setText(QString("Сохранено: %1").arg(filename));
    }

    void addBookmark() {
        if (isRecording) {
            qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
            double seconds = elapsed / 1000.0;
            QString comment = commentEdit->text().trimmed();
            if (comment.isEmpty()) comment = QString("Метка %1").arg(bookmarks.size()+1);
            bookmarks.append({seconds, comment});
            commentEdit->clear();
            statusLabel->setText(QString("Метка добавлена: %1 (%2)").arg(comment).arg(formatTime(seconds)));
            updateBookmarksList();
        }
    }

    void playSelected() {
        if (currentFile.isEmpty()) {
            QMessageBox::warning(this, "Предупреждение", "Выберите запись");
            return;
        }
        // Простейшее воспроизведение через QAudioOutput
        QFile file(currentFile);
        if (!file.open(QIODevice::ReadOnly)) return;
        QByteArray data = file.readAll();
        file.close();

        QAudioFormat format;
        format.setSampleRate(44100);
        format.setChannelCount(1);
        format.setSampleSize(16);
        format.setCodec("audio/pcm");
        format.setByteOrder(QAudioFormat::LittleEndian);
        format.setSampleType(QAudioFormat::SignedInt);

        QAudioOutput *output = new QAudioOutput(format, this);
        QBuffer *playBuffer = new QBuffer(this);
        playBuffer->setData(data);
        playBuffer->open(QIODevice::ReadOnly);
        output->start(playBuffer);
    }

    void deleteSelected() {
        if (currentFile.isEmpty()) return;
        if (QMessageBox::question(this, "Подтверждение", QString("Удалить %1?").arg(currentFile)) == QMessageBox::Yes) {
            QFile::remove(currentFile);
            QString marksFile = currentFile.replace(".wav", "_marks.json");
            QFile::remove(marksFile);
            currentFile.clear();
            bookmarks.clear();
            updateBookmarksList();
            loadRecordings();
            statusLabel->setText("Запись удалена");
        }
    }

    void onSelectRecording() {
        QListWidgetItem *item = recordingsList->currentItem();
        if (!item) return;
        QString filename = item->text();
        currentFile = filename;
        // Загружаем метки
        QString marksFile = filename.replace(".wav", "_marks.json");
        if (QFile::exists(marksFile)) {
            QFile file(marksFile);
            if (file.open(QIODevice::ReadOnly)) {
                QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
                QJsonArray arr = doc.array();
                bookmarks.clear();
                for (const auto &v : arr) {
                    QJsonObject obj = v.toObject();
                    bookmarks.append({obj["time"].toDouble(), obj["comment"].toString()});
                }
                updateBookmarksList();
            }
        }
        statusLabel->setText(QString("Выбрана запись: %1").arg(filename));
    }

    void onSelectBookmark() {
        int row = bookmarksList->currentRow();
        if (row >= 0 && row < bookmarks.size()) {
            double t = bookmarks[row].first;
            statusLabel->setText(QString("Переход к метке %1 (%2)").arg(row+1).arg(formatTime(t)));
        }
    }

    void updateTime() {
        if (isRecording) {
            qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - startTime;
            double seconds = elapsed / 1000.0;
            timeLabel->setText(formatTime(seconds));
        }
    }

private:
    struct Bookmark { double time; QString comment; };
    QList<Bookmark> bookmarks;
    QListWidget *recordingsList;
    QListWidget *bookmarksList;
    QLineEdit *commentEdit;
    QLabel *timeLabel;
    QLabel *statusLabel;
    QPushButton *recordBtn, *stopBtn, *bookmarkBtn;
    QAudioInput *audioInput;
    QBuffer *buffer;
    QTimer *timer;
    bool isRecording = false;
    qint64 startTime;
    QString currentFile;

    void createUI() {
        QWidget *central = new QWidget(this);
        setCentralWidget(central);
        QVBoxLayout *mainLayout = new QVBoxLayout(central);

        // Панель управления
        QHBoxLayout *ctrlLayout = new QHBoxLayout();
        recordBtn = new QPushButton("Новая запись");
        stopBtn = new QPushButton("Остановить");
        bookmarkBtn = new QPushButton("Метка");
        QPushButton *playBtn = new QPushButton("Воспроизвести");
        QPushButton *deleteBtn = new QPushButton("Удалить");
        ctrlLayout->addWidget(recordBtn);
        ctrlLayout->addWidget(stopBtn);
        ctrlLayout->addWidget(bookmarkBtn);
        ctrlLayout->addWidget(playBtn);
        ctrlLayout->addWidget(deleteBtn);
        mainLayout->addLayout(ctrlLayout);

        // Время
        timeLabel = new QLabel("00:00:00");
        timeLabel->setStyleSheet("font-size: 24px; font-weight: bold;");
        mainLayout->addWidget(timeLabel, 0, Qt::AlignCenter);

        statusLabel = new QLabel("Готов");
        mainLayout->addWidget(statusLabel);

        // Две колонки: список записей и метки
        QHBoxLayout *columns = new QHBoxLayout();
        QVBoxLayout *leftCol = new QVBoxLayout();
        leftCol->addWidget(new QLabel("Записи:"));
        recordingsList = new QListWidget();
        connect(recordingsList, &QListWidget::currentItemChanged, this, &Recorder::onSelectRecording);
        leftCol->addWidget(recordingsList);
        columns->addLayout(leftCol, 1);

        QVBoxLayout *rightCol = new QVBoxLayout();
        rightCol->addWidget(new QLabel("Метки:"));
        bookmarksList = new QListWidget();
        connect(bookmarksList, &QListWidget::currentRowChanged, this, &Recorder::onSelectBookmark);
        rightCol->addWidget(bookmarksList);
        columns->addLayout(rightCol, 1);
        mainLayout->addLayout(columns);

        // Комментарий и кнопка
        QHBoxLayout *commentLayout = new QHBoxLayout();
        commentLayout->addWidget(new QLabel("Комментарий:"));
        commentEdit = new QLineEdit();
        commentLayout->addWidget(commentEdit);
        QPushButton *addMarkBtn = new QPushButton("Добавить метку");
        connect(addMarkBtn, &QPushButton::clicked, this, &Recorder::addBookmark);
        commentLayout->addWidget(addMarkBtn);
        QPushButton *exportBtn = new QPushButton("Экспорт меток");
        connect(exportBtn, &QPushButton::clicked, this, &Recorder::exportBookmarks);
        commentLayout->addWidget(exportBtn);
        mainLayout->addLayout(commentLayout);

        // Таймер
        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &Recorder::updateTime);

        stopBtn->setEnabled(false);
        bookmarkBtn->setEnabled(false);
        connect(recordBtn, &QPushButton::clicked, this, &Recorder::startRecording);
        connect(stopBtn, &QPushButton::clicked, this, &Recorder::stopRecording);
        connect(bookmarkBtn, &QPushButton::clicked, this, &Recorder::addBookmark);
        connect(playBtn, &QPushButton::clicked, this, &Recorder::playSelected);
        connect(deleteBtn, &QPushButton::clicked, this, &Recorder::deleteSelected);
    }

    void loadRecordings() {
        recordingsList->clear();
        QDir dir(".");
        QStringList filters;
        filters << "*.wav";
        dir.setNameFilters(filters);
        QStringList files = dir.entryList(QDir::Files);
        for (const QString &f : files) {
            recordingsList->addItem(f);
        }
    }

    void updateBookmarksList() {
        bookmarksList->clear();
        for (int i = 0; i < bookmarks.size(); ++i) {
            const auto &b = bookmarks[i];
            bookmarksList->addItem(QString("%1. %2 - %3").arg(i+1).arg(formatTime(b.time)).arg(b.comment));
        }
    }

    QString formatTime(double seconds) {
        int secs = (int)seconds;
        int h = secs / 3600;
        int m = (secs % 3600) / 60;
        int s = secs % 60;
        return QString("%1:%2:%3").arg(h, 2, 10, QChar('0')).arg(m, 2, 10, QChar('0')).arg(s, 2, 10, QChar('0'));
    }

    void saveBookmarks(const QString &filename) {
        QJsonArray arr;
        for (const auto &b : bookmarks) {
            QJsonObject obj;
            obj["time"] = b.time;
            obj["comment"] = b.comment;
            arr.append(obj);
        }
        QJsonDocument doc(arr);
        QFile file(filename);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
        }
    }

    void exportBookmarks() {
        if (bookmarks.isEmpty()) {
            QMessageBox::information(this, "Информация", "Нет меток для экспорта");
            return;
        }
        QString filename = QFileDialog::getSaveFileName(this, "Экспорт меток", "", "Text files (*.txt)");
        if (!filename.isEmpty()) {
            QFile file(filename);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << "Метки для записи:\n";
                for (int i = 0; i < bookmarks.size(); ++i) {
                    out << i+1 << ". " << formatTime(bookmarks[i].time) << " - " << bookmarks[i].comment << "\n";
                }
                file.close();
                statusLabel->setText("Метки экспортированы");
            }
        }
    }
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    Recorder w;
    w.show();
    return app.exec();
}

#include "recorder_cpp.moc"
