// recorder_java.java — диктофон с закладками на Java (Swing)

import javax.swing.*;
import javax.swing.event.*;
import java.awt.*;
import java.awt.event.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.List;
import javax.sound.sampled.*;
import com.google.gson.*; // требуется Gson

public class RecorderJava extends JFrame {
    private static final int SAMPLE_RATE = 44100;
    private static final int SAMPLE_SIZE_IN_BITS = 16;
    private static final int CHANNELS = 1;
    private static final boolean SIGNED = true;
    private static final boolean BIG_ENDIAN = false;
    private AudioFormat format;
    private TargetDataLine line;
    private ByteArrayOutputStream audioBuffer;
    private boolean isRecording = false;
    private long startTime;
    private List<Bookmark> bookmarks = new ArrayList<>();
    private String currentFile = null;
    private Timer timer;

    private JList<String> recordingsList;
    private DefaultListModel<String> listModel;
    private JList<String> bookmarksList;
    private DefaultListModel<String> bookmarksModel;
    private JTextField commentField;
    private JLabel timeLabel, statusLabel;
    private JButton recordBtn, stopBtn, bookmarkBtn;

    public RecorderJava() {
        setTitle("🎙️ MarkRecorder — Java");
        setSize(800, 600);
        setDefaultCloseOperation(EXIT_ON_CLOSE);
        setLayout(new BorderLayout());

        format = new AudioFormat(SAMPLE_RATE, SAMPLE_SIZE_IN_BITS, CHANNELS, SIGNED, BIG_ENDIAN);
        createUI();
        loadRecordings();
        timer = new Timer(100, e -> updateTime());
    }

    private void createUI() {
        // Панель управления
        JPanel ctrlPanel = new JPanel();
        recordBtn = new JButton("Новая запись");
        stopBtn = new JButton("Остановить");
        bookmarkBtn = new JButton("Метка");
        JButton playBtn = new JButton("Воспроизвести");
        JButton deleteBtn = new JButton("Удалить");
        ctrlPanel.add(recordBtn);
        ctrlPanel.add(stopBtn);
        ctrlPanel.add(bookmarkBtn);
        ctrlPanel.add(playBtn);
        ctrlPanel.add(deleteBtn);
        add(ctrlPanel, BorderLayout.NORTH);

        // Время и статус
        JPanel infoPanel = new JPanel(new GridLayout(2,1));
        timeLabel = new JLabel("00:00:00", SwingConstants.CENTER);
        timeLabel.setFont(new Font("Arial", Font.BOLD, 24));
        infoPanel.add(timeLabel);
        statusLabel = new JLabel("Готов", SwingConstants.CENTER);
        infoPanel.add(statusLabel);
        add(infoPanel, BorderLayout.CENTER);

        // Две колонки
        JPanel columns = new JPanel(new GridLayout(1,2));
        // Список записей
        JPanel left = new JPanel(new BorderLayout());
        left.add(new JLabel("Записи:"), BorderLayout.NORTH);
        listModel = new DefaultListModel<>();
        recordingsList = new JList<>(listModel);
        recordingsList.addListSelectionListener(e -> {
            if (!e.getValueIsAdjusting()) onSelectRecording();
        });
        left.add(new JScrollPane(recordingsList), BorderLayout.CENTER);
        columns.add(left);

        // Метки
        JPanel right = new JPanel(new BorderLayout());
        right.add(new JLabel("Метки:"), BorderLayout.NORTH);
        bookmarksModel = new DefaultListModel<>();
        bookmarksList = new JList<>(bookmarksModel);
        bookmarksList.addListSelectionListener(e -> {
            if (!e.getValueIsAdjusting()) onSelectBookmark();
        });
        right.add(new JScrollPane(bookmarksList), BorderLayout.CENTER);
        columns.add(right);
        add(columns, BorderLayout.SOUTH);

        // Панель комментария
        JPanel commentPanel = new JPanel(new FlowLayout());
        commentPanel.add(new JLabel("Комментарий:"));
        commentField = new JTextField(20);
        commentPanel.add(commentField);
        JButton addMarkBtn = new JButton("Добавить метку");
        addMarkBtn.addActionListener(e -> addBookmark());
        commentPanel.add(addMarkBtn);
        JButton exportBtn = new JButton("Экспорт меток");
        exportBtn.addActionListener(e -> exportBookmarks());
        commentPanel.add(exportBtn);
        add(commentPanel, BorderLayout.AFTER_LAST_LINE);

        stopBtn.setEnabled(false);
        bookmarkBtn.setEnabled(false);

        recordBtn.addActionListener(e -> startRecording());
        stopBtn.addActionListener(e -> stopRecording());
        bookmarkBtn.addActionListener(e -> addBookmark());
        playBtn.addActionListener(e -> playSelected());
        deleteBtn.addActionListener(e -> deleteSelected());
    }

    private void startRecording() {
        if (isRecording) return;
        try {
            DataLine.Info info = new DataLine.Info(TargetDataLine.class, format);
            if (!AudioSystem.isLineSupported(info)) {
                JOptionPane.showMessageDialog(this, "Микрофон не поддерживается");
                return;
            }
            line = (TargetDataLine) AudioSystem.getLine(info);
            line.open(format);
            line.start();

            audioBuffer = new ByteArrayOutputStream();
            isRecording = true;
            startTime = System.currentTimeMillis();
            bookmarks.clear();
            recordBtn.setEnabled(false);
            stopBtn.setEnabled(true);
            bookmarkBtn.setEnabled(true);
            statusLabel.setText("Идёт запись...");
            timer.start();
            // Запись в отдельном потоке
            new Thread(() -> {
                byte[] buffer = new byte[1024];
                while (isRecording) {
                    int bytesRead = line.read(buffer, 0, buffer.length);
                    if (bytesRead > 0) {
                        audioBuffer.write(buffer, 0, bytesRead);
                    }
                }
            }).start();
        } catch (LineUnavailableException ex) {
            JOptionPane.showMessageDialog(this, "Ошибка доступа к микрофону");
        }
    }

    private void stopRecording() {
        if (!isRecording) return;
        isRecording = false;
        timer.stop();
        line.stop();
        line.close();

        recordBtn.setEnabled(true);
        stopBtn.setEnabled(false);
        bookmarkBtn.setEnabled(false);
        statusLabel.setText("Запись остановлена");

        // Сохраняем файл
        String filename = "запись_" + java.time.LocalDateTime.now().format(java.time.format.DateTimeFormatter.ofPattern("yyyy-MM-dd_HH-mm-ss")) + ".wav";
        currentFile = filename;
        saveWav(filename);
        saveBookmarks(filename.replace(".wav", "_marks.json"));
        loadRecordings();
        statusLabel.setText("Сохранено: " + filename);
    }

    private void saveWav(String filename) {
        try (FileOutputStream fos = new FileOutputStream(filename)) {
            byte[] audioData = audioBuffer.toByteArray();
            // Запись WAV заголовка (упрощённо)
            fos.write(audioData);
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void saveBookmarks(String filename) {
        JsonArray arr = new JsonArray();
        for (Bookmark b : bookmarks) {
            JsonObject obj = new JsonObject();
            obj.addProperty("time", b.time);
            obj.addProperty("comment", b.comment);
            arr.add(obj);
        }
        try (FileWriter fw = new FileWriter(filename)) {
            fw.write(arr.toString());
        } catch (IOException e) {}
    }

    private void addBookmark() {
        if (isRecording) {
            double seconds = (System.currentTimeMillis() - startTime) / 1000.0;
            String comment = commentField.getText().trim();
            if (comment.isEmpty()) comment = "Метка " + (bookmarks.size()+1);
            bookmarks.add(new Bookmark(seconds, comment));
            commentField.setText("");
            statusLabel.setText("Метка добавлена: " + comment + " (" + formatTime(seconds) + ")");
            updateBookmarksList();
        }
    }

    private void playSelected() {
        if (currentFile == null) {
            JOptionPane.showMessageDialog(this, "Выберите запись");
            return;
        }
        try {
            File file = new File(currentFile);
            AudioInputStream ais = AudioSystem.getAudioInputStream(file);
            SourceDataLine line = (SourceDataLine) AudioSystem.getLine(new DataLine.Info(SourceDataLine.class, format));
            line.open(format);
            line.start();
            byte[] buffer = new byte[4096];
            int bytesRead;
            while ((bytesRead = ais.read(buffer)) != -1) {
                line.write(buffer, 0, bytesRead);
            }
            line.drain();
            line.close();
            ais.close();
        } catch (Exception e) {
            JOptionPane.showMessageDialog(this, "Ошибка воспроизведения");
        }
    }

    private void deleteSelected() {
        if (currentFile == null) return;
        int confirm = JOptionPane.showConfirmDialog(this, "Удалить " + currentFile + "?", "Подтверждение", JOptionPane.YES_NO_OPTION);
        if (confirm == JOptionPane.YES_OPTION) {
            new File(currentFile).delete();
            String marksFile = currentFile.replace(".wav", "_marks.json");
            new File(marksFile).delete();
            currentFile = null;
            bookmarks.clear();
            updateBookmarksList();
            loadRecordings();
            statusLabel.setText("Запись удалена");
        }
    }

    private void onSelectRecording() {
        String filename = recordingsList.getSelectedValue();
        if (filename == null) return;
        currentFile = filename;
        // Загружаем метки
        String marksFile = filename.replace(".wav", "_marks.json");
        bookmarks.clear();
        if (new File(marksFile).exists()) {
            try (Reader reader = new FileReader(marksFile)) {
                JsonArray arr = JsonParser.parseReader(reader).getAsJsonArray();
                for (JsonElement el : arr) {
                    JsonObject obj = el.getAsJsonObject();
                    bookmarks.add(new Bookmark(obj.get("time").getAsDouble(), obj.get("comment").getAsString()));
                }
            } catch (IOException e) {}
        }
        updateBookmarksList();
        statusLabel.setText("Выбрана запись: " + filename);
    }

    private void onSelectBookmark() {
        int idx = bookmarksList.getSelectedIndex();
        if (idx >= 0 && idx < bookmarks.size()) {
            double t = bookmarks.get(idx).time;
            statusLabel.setText("Переход к метке " + (idx+1) + " (" + formatTime(t) + ")");
        }
    }

    private void updateBookmarksList() {
        bookmarksModel.clear();
        for (int i = 0; i < bookmarks.size(); i++) {
            Bookmark b = bookmarks.get(i);
            bookmarksModel.addElement((i+1) + ". " + formatTime(b.time) + " - " + b.comment);
        }
    }

    private void updateTime() {
        if (isRecording) {
            double seconds = (System.currentTimeMillis() - startTime) / 1000.0;
            timeLabel.setText(formatTime(seconds));
        }
    }

    private String formatTime(double seconds) {
        int secs = (int)seconds;
        int h = secs / 3600;
        int m = (secs % 3600) / 60;
        int s = secs % 60;
        return String.format("%02d:%02d:%02d", h, m, s);
    }

    private void loadRecordings() {
        listModel.clear();
        File dir = new File(".");
        String[] files = dir.list((d, name) -> name.endsWith(".wav"));
        if (files != null) {
            Arrays.sort(files);
            for (String f : files) listModel.addElement(f);
        }
    }

    private void exportBookmarks() {
        if (bookmarks.isEmpty()) {
            JOptionPane.showMessageDialog(this, "Нет меток для экспорта");
            return;
        }
        JFileChooser chooser = new JFileChooser();
        if (chooser.showSaveDialog(this) == JFileChooser.APPROVE_OPTION) {
            File file = chooser.getSelectedFile();
            try (PrintWriter pw = new PrintWriter(file)) {
                pw.println("Метки для записи:");
                for (int i = 0; i < bookmarks.size(); i++) {
                    Bookmark b = bookmarks.get(i);
                    pw.println((i+1) + ". " + formatTime(b.time) + " - " + b.comment);
                }
                statusLabel.setText("Метки экспортированы");
            } catch (IOException e) {}
        }
    }

    static class Bookmark {
        double time;
        String comment;
        Bookmark(double time, String comment) { this.time = time; this.comment = comment; }
    }

    public static void main(String[] args) throws Exception {
        UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName());
        SwingUtilities.invokeLater(() -> new RecorderJava().setVisible(true));
    }
}
