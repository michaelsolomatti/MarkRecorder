// recorder_rs.rs — диктофон с закладками на Rust (консоль + ffmpeg)

use std::fs::File;
use std::io::{self, Write, BufRead};
use std::process::Command;
use std::time::{Instant, Duration};
use std::thread;
use serde::{Serialize, Deserialize};
use serde_json;

#[derive(Serialize, Deserialize, Clone)]
struct Bookmark {
    time: f64,
    comment: String,
}

struct Recorder {
    is_recording: bool,
    start_time: Option<Instant>,
    bookmarks: Vec<Bookmark>,
    current_file: String,
}

impl Recorder {
    fn new() -> Self {
        Recorder {
            is_recording: false,
            start_time: None,
            bookmarks: Vec::new(),
            current_file: String::new(),
        }
    }

    fn start(&mut self) {
        if self.is_recording {
            println!("Уже идёт запись");
            return;
        }
        // Проверяем наличие ffmpeg или sox
        if let Err(_) = Command::new("ffmpeg").arg("-version").output() {
            println!("ffmpeg не найден. Установите ffmpeg для записи.");
            return;
        }
        let filename = format!("запись_{}.wav", chrono::Local::now().format("%Y-%m-%d_%H-%M-%S"));
        self.current_file = filename.clone();
        self.is_recording = true;
        self.start_time = Some(Instant::now());
        self.bookmarks.clear();
        println!("Запись начата. Файл: {}", filename);
        println!("Команды: stop, mark [комментарий], exit");

        // Запуск записи в отдельном потоке
        let file = filename.clone();
        thread::spawn(move || {
            let output = Command::new("ffmpeg")
                .args(&["-f", "alsa", "-i", "default", "-t", "3600", "-acodec", "pcm_s16le", "-ar", "44100", "-ac", "1", &file])
                .output()
                .expect("Ошибка ffmpeg");
            println!("Запись завершена");
        });
    }

    fn stop(&mut self) {
        if !self.is_recording {
            println!("Нет активной записи");
            return;
        }
        // Убиваем процесс ffmpeg (упрощённо — предложим пользователю нажать Ctrl+C)
        println!("Для остановки нажмите Ctrl+C, затем выполните команду stop.");
        // В реальном проекте нужно найти PID и убить.
        self.is_recording = false;
        self.save_bookmarks();
    }

    fn add_bookmark(&mut self, comment: &str) {
        if !self.is_recording || self.start_time.is_none() {
            println!("Нет активной записи");
            return;
        }
        let elapsed = self.start_time.unwrap().elapsed().as_secs_f64();
        let comment = if comment.is_empty() {
            format!("Метка {}", self.bookmarks.len()+1)
        } else {
            comment.to_string()
        };
        self.bookmarks.push(Bookmark { time: elapsed, comment: comment.clone() });
        println!("Метка добавлена: {} ({:.2} с)", comment, elapsed);
    }

    fn save_bookmarks(&self) {
        if self.current_file.is_empty() {
            return;
        }
        let marks_file = self.current_file.replace(".wav", "_marks.json");
        let json = serde_json::to_string_pretty(&self.bookmarks).unwrap();
        if let Ok(mut f) = File::create(&marks_file) {
            f.write_all(json.as_bytes()).unwrap();
            println!("Метки сохранены в {}", marks_file);
        }
    }

    fn list_recordings(&self) {
        if let Ok(entries) = std::fs::read_dir(".") {
            for entry in entries {
                if let Ok(entry) = entry {
                    let name = entry.file_name().into_string().unwrap();
                    if name.ends_with(".wav") {
                        println!("  {}", name);
                    }
                }
            }
        }
    }

    fn show_bookmarks(&self, filename: &str) {
        let marks_file = filename.replace(".wav", "_marks.json");
        if let Ok(data) = std::fs::read_to_string(&marks_file) {
            if let Ok(marks) = serde_json::from_str::<Vec<Bookmark>>(&data) {
                println!("Метки для {}:", filename);
                for (i, m) in marks.iter().enumerate() {
                    println!("  {}. {:.2} с - {}", i+1, m.time, m.comment);
                }
            } else {
                println!("Не удалось разобрать метки");
            }
        } else {
            println!("Нет меток для {}", filename);
        }
    }
}

fn main() {
    let mut rec = Recorder::new();
    let stdin = io::stdin();
    let mut reader = stdin.lock();
    println!("🎙️ MarkRecorder — Rust Edition");
    println!("Команды: start, stop, mark [комментарий], list, marks <файл>, exit");
    loop {
        print!("> ");
        io::stdout().flush().unwrap();
        let mut line = String::new();
        if reader.read_line(&mut line).is_err() { break; }
        let line = line.trim();
        let parts: Vec<&str> = line.splitn(2, ' ').collect();
        let cmd = parts[0];
        let arg = if parts.len() > 1 { parts[1] } else { "" };
        match cmd {
            "start" => rec.start(),
            "stop" => rec.stop(),
            "mark" => rec.add_bookmark(arg),
            "list" => rec.list_recordings(),
            "marks" => {
                if arg.is_empty() {
                    println!("Укажите имя файла");
                } else {
                    rec.show_bookmarks(arg);
                }
            }
            "exit" => {
                if rec.is_recording {
                    println!("Запись ещё идёт. Остановите её перед выходом.");
                } else {
                    println!("До свидания!");
                    break;
                }
            }
            _ => println!("Неизвестная команда"),
        }
    }
}
