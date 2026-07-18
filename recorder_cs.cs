// recorder_cs.cs — диктофон с закладками на C# (WPF)

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Media;
using System.Windows.Threading;
using NAudio.Wave;
using Newtonsoft.Json;

namespace MarkRecorderWPF
{
    public partial class MainWindow : Window
    {
        private WaveInEvent waveIn;
        private WaveFileWriter writer;
        private bool isRecording = false;
        private DateTime startTime;
        private List<Bookmark> bookmarks = new List<Bookmark>();
        private string currentFile = null;
        private DispatcherTimer timer;

        private ListBox recordingsList, bookmarksList;
        private TextBox commentBox;
        private Label timeLabel, statusLabel;
        private Button recordBtn, stopBtn, bookmarkBtn;

        public MainWindow()
        {
            InitializeComponent();
            CreateUI();
            LoadRecordings();
            timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(100) };
            timer.Tick += (s, e) => UpdateTime();
        }

        private void CreateUI()
        {
            Title = "🎙️ MarkRecorder — C#";
            Width = 800;
            Height = 600;
            var grid = new Grid();
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });
            grid.RowDefinitions.Add(new RowDefinition { Height = new GridLength(1, GridUnitType.Star) });
            grid.RowDefinitions.Add(new RowDefinition { Height = GridLength.Auto });

            // Панель управления
            var ctrlPanel = new StackPanel { Orientation = Orientation.Horizontal };
            recordBtn = new Button { Content = "Новая запись", Width = 100, Background = Brushes.Red, Foreground = Brushes.White };
            stopBtn = new Button { Content = "Остановить", Width = 100, IsEnabled = false };
            bookmarkBtn = new Button { Content = "Метка", Width = 80, IsEnabled = false };
            var playBtn = new Button { Content = "Воспроизвести", Width = 100 };
            var deleteBtn = new Button { Content = "Удалить", Width = 100 };
            ctrlPanel.Children.Add(recordBtn);
            ctrlPanel.Children.Add(stopBtn);
            ctrlPanel.Children.Add(bookmarkBtn);
            ctrlPanel.Children.Add(playBtn);
            ctrlPanel.Children.Add(deleteBtn);
            Grid.SetRow(ctrlPanel, 0);
            grid.Children.Add(ctrlPanel);

            // Время и статус
            var infoPanel = new StackPanel();
            timeLabel = new Label { FontSize = 24, FontWeight = FontWeights.Bold, Content = "00:00:00", HorizontalAlignment = HorizontalAlignment.Center };
            statusLabel = new Label { Content = "Готов", HorizontalAlignment = HorizontalAlignment.Center };
            infoPanel.Children.Add(timeLabel);
            infoPanel.Children.Add(statusLabel);
            Grid.SetRow(infoPanel, 1);
            grid.Children.Add(infoPanel);

            // Список записей и меток
            var columns = new Grid();
            columns.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            columns.ColumnDefinitions.Add(new ColumnDefinition { Width = new GridLength(1, GridUnitType.Star) });
            var leftPanel = new StackPanel();
            leftPanel.Children.Add(new Label { Content = "Записи:" });
            recordingsList = new ListBox();
            recordingsList.SelectionChanged += (s, e) => OnSelectRecording();
            leftPanel.Children.Add(recordingsList);
            Grid.SetColumn(leftPanel, 0);
            columns.Children.Add(leftPanel);
            var rightPanel = new StackPanel();
            rightPanel.Children.Add(new Label { Content = "Метки:" });
            bookmarksList = new ListBox();
            bookmarksList.SelectionChanged += (s, e) => OnSelectBookmark();
            rightPanel.Children.Add(bookmarksList);
            Grid.SetColumn(rightPanel, 1);
            columns.Children.Add(rightPanel);
            Grid.SetRow(columns, 2);
            grid.Children.Add(columns);

            // Комментарий и кнопки
            var commentPanel = new StackPanel { Orientation = Orientation.Horizontal };
            commentPanel.Children.Add(new Label { Content = "Комментарий:" });
            commentBox = new TextBox { Width = 200 };
            commentPanel.Children.Add(commentBox);
            var addMarkBtn = new Button { Content = "Добавить метку" };
            addMarkBtn.Click += (s, e) => AddBookmark();
            commentPanel.Children.Add(addMarkBtn);
            var exportBtn = new Button { Content = "Экспорт меток" };
            exportBtn.Click += (s, e) => ExportBookmarks();
            commentPanel.Children.Add(exportBtn);
            Grid.SetRow(commentPanel, 3);
            grid.Children.Add(commentPanel);

            Content = grid;

            recordBtn.Click += (s, e) => StartRecording();
            stopBtn.Click += (s, e) => StopRecording();
            bookmarkBtn.Click += (s, e) => AddBookmark();
            playBtn.Click += (s, e) => PlaySelected();
            deleteBtn.Click += (s, e) => DeleteSelected();
        }

        private async void StartRecording()
        {
            if (isRecording) return;
            try
            {
                waveIn = new WaveInEvent();
                waveIn.DeviceNumber = 0;
                waveIn.WaveFormat = new WaveFormat(44100, 16, 1);
                waveIn.DataAvailable += (s, e) => writer?.Write(e.Buffer, 0, e.BytesRecorded);
                waveIn.RecordingStopped += (s, e) => { writer?.Dispose(); writer = null; };

                string filename = $"запись_{DateTime.Now:yyyy-MM-dd_HH-mm-ss}.wav";
                currentFile = filename;
                writer = new WaveFileWriter(filename, waveIn.WaveFormat);
                waveIn.StartRecording();
                isRecording = true;
                startTime = DateTime.Now;
                bookmarks.Clear();
                recordBtn.IsEnabled = false;
                stopBtn.IsEnabled = true;
                bookmarkBtn.IsEnabled = true;
                statusLabel.Content = "Идёт запись...";
                timer.Start();
            }
            catch (Exception ex)
            {
                MessageBox.Show("Ошибка доступа к микрофону: " + ex.Message);
            }
        }

        private void StopRecording()
        {
            if (!isRecording) return;
            isRecording = false;
            timer.Stop();
            waveIn.StopRecording();
            recordBtn.IsEnabled = true;
            stopBtn.IsEnabled = false;
            bookmarkBtn.IsEnabled = false;
            statusLabel.Content = "Запись остановлена";
            SaveBookmarks(currentFile.Replace(".wav", "_marks.json"));
            LoadRecordings();
            statusLabel.Content = $"Сохранено: {currentFile}";
        }

        private void SaveBookmarks(string filename)
        {
            var data = bookmarks.Select(b => new { b.Time, b.Comment }).ToList();
            File.WriteAllText(filename, JsonConvert.SerializeObject(data, Formatting.Indented));
        }

        private void AddBookmark()
        {
            if (isRecording)
            {
                double seconds = (DateTime.Now - startTime).TotalSeconds;
                string comment = commentBox.Text.Trim();
                if (string.IsNullOrEmpty(comment)) comment = $"Метка {bookmarks.Count+1}";
                bookmarks.Add(new Bookmark { Time = seconds, Comment = comment });
                commentBox.Clear();
                statusLabel.Content = $"Метка добавлена: {comment} ({FormatTime(seconds)})";
                UpdateBookmarksList();
            }
        }

        private void PlaySelected()
        {
            if (string.IsNullOrEmpty(currentFile)) { MessageBox.Show("Выберите запись"); return; }
            try
            {
                using (var reader = new WaveFileReader(currentFile))
                using (var output = new WaveOutEvent())
                {
                    output.Init(reader);
                    output.Play();
                    while (output.PlaybackState == PlaybackState.Playing)
                        System.Threading.Thread.Sleep(100);
                }
            }
            catch (Exception ex) { MessageBox.Show("Ошибка воспроизведения: " + ex.Message); }
        }

        private void DeleteSelected()
        {
            if (string.IsNullOrEmpty(currentFile)) return;
            if (MessageBox.Show($"Удалить {currentFile}?", "Подтверждение", MessageBoxButton.YesNo) == MessageBoxResult.Yes)
            {
                File.Delete(currentFile);
                string marksFile = currentFile.Replace(".wav", "_marks.json");
                if (File.Exists(marksFile)) File.Delete(marksFile);
                currentFile = null;
                bookmarks.Clear();
                UpdateBookmarksList();
                LoadRecordings();
                statusLabel.Content = "Запись удалена";
            }
        }

        private void OnSelectRecording()
        {
            string filename = recordingsList.SelectedItem as string;
            if (filename == null) return;
            currentFile = filename;
            string marksFile = filename.Replace(".wav", "_marks.json");
            bookmarks.Clear();
            if (File.Exists(marksFile))
            {
                var data = JsonConvert.DeserializeObject<List<Bookmark>>(File.ReadAllText(marksFile));
                bookmarks = data ?? new List<Bookmark>();
            }
            UpdateBookmarksList();
            statusLabel.Content = $"Выбрана запись: {filename}";
        }

        private void OnSelectBookmark()
        {
            int idx = bookmarksList.SelectedIndex;
            if (idx >= 0 && idx < bookmarks.Count)
            {
                double t = bookmarks[idx].Time;
                statusLabel.Content = $"Переход к метке {idx+1} ({FormatTime(t)})";
            }
        }

        private void UpdateBookmarksList()
        {
            bookmarksList.Items.Clear();
            for (int i = 0; i < bookmarks.Count; i++)
            {
                var b = bookmarks[i];
                bookmarksList.Items.Add($"{i+1}. {FormatTime(b.Time)} - {b.Comment}");
            }
        }

        private void UpdateTime()
        {
            if (isRecording)
            {
                double seconds = (DateTime.Now - startTime).TotalSeconds;
                timeLabel.Content = FormatTime(seconds);
            }
        }

        private string FormatTime(double seconds)
        {
            var ts = TimeSpan.FromSeconds(seconds);
            return ts.ToString(@"hh\:mm\:ss");
        }

        private void LoadRecordings()
        {
            recordingsList.Items.Clear();
            var files = Directory.GetFiles(".", "*.wav").Select(Path.GetFileName).OrderBy(f => f);
            foreach (var f in files) recordingsList.Items.Add(f);
        }

        private void ExportBookmarks()
        {
            if (bookmarks.Count == 0) { MessageBox.Show("Нет меток для экспорта"); return; }
            var dialog = new Microsoft.Win32.SaveFileDialog { Filter = "Text files (*.txt)|*.txt" };
            if (dialog.ShowDialog() == true)
            {
                using (var sw = new StreamWriter(dialog.FileName))
                {
                    sw.WriteLine("Метки для записи:");
                    for (int i = 0; i < bookmarks.Count; i++)
                    {
                        sw.WriteLine($"{i+1}. {FormatTime(bookmarks[i].Time)} - {bookmarks[i].Comment}");
                    }
                }
                statusLabel.Content = "Метки экспортированы";
            }
        }

        public class Bookmark
        {
            public double Time { get; set; }
            public string Comment { get; set; }
        }

        [STAThread]
        static void Main()
        {
            var app = new Application();
            app.Run(new MainWindow());
        }
    }
}
