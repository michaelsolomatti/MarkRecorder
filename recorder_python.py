
### 1. `recorder_python.py`

```python
# recorder_python.py — диктофон с закладками на Python (Tkinter)

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import threading
import time
import json
import os
import wave
import pyaudio
import numpy as np
from datetime import datetime
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg

class Recorder:
    def __init__(self, root):
        self.root = root
        self.root.title("🎙️ MarkRecorder — Python")
        self.root.geometry("900x650")

        # Параметры записи
        self.chunk = 1024
        self.format = pyaudio.paInt16
        self.channels = 1
        self.rate = 44100
        self.audio = pyaudio.PyAudio()
        self.frames = []
        self.stream = None
        self.is_recording = False
        self.is_playing = False
        self.start_time = 0
        self.current_time = 0
        self.bookmarks = []  # список (time, comment)
        self.current_file = None
        self.recording_thread = None
        self.play_thread = None

        self.create_widgets()
        self.load_recordings()
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def create_widgets(self):
        # Верхняя панель управления
        control_frame = tk.Frame(self.root)
        control_frame.pack(pady=5, fill=tk.X)
        self.record_btn = tk.Button(control_frame, text="Новая запись", command=self.start_recording, bg="red", fg="white", width=15)
        self.record_btn.pack(side=tk.LEFT, padx=5)
        self.stop_btn = tk.Button(control_frame, text="Остановить", command=self.stop_recording, state=tk.DISABLED, width=15)
        self.stop_btn.pack(side=tk.LEFT, padx=5)
        self.bookmark_btn = tk.Button(control_frame, text="Метка", command=self.add_bookmark, state=tk.DISABLED, width=10)
        self.bookmark_btn.pack(side=tk.LEFT, padx=5)
        self.play_btn = tk.Button(control_frame, text="Воспроизвести", command=self.play_selected, width=12)
        self.play_btn.pack(side=tk.LEFT, padx=5)
        self.delete_btn = tk.Button(control_frame, text="Удалить запись", command=self.delete_selected, width=12)
        self.delete_btn.pack(side=tk.LEFT, padx=5)

        # Текущее время и статус
        self.time_label = tk.Label(self.root, text="00:00:00", font=("Arial", 24))
        self.time_label.pack(pady=5)
        self.status_label = tk.Label(self.root, text="Готов", anchor=tk.W)
        self.status_label.pack(fill=tk.X, padx=10)

        # Список записей и метки
        main_frame = tk.Frame(self.root)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=5)
        left_frame = tk.Frame(main_frame, width=250)
        left_frame.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.recordings_listbox = tk.Listbox(left_frame, height=15)
        self.recordings_listbox.pack(fill=tk.BOTH, expand=True)
        self.recordings_listbox.bind('<<ListboxSelect>>', self.on_select_recording)

        right_frame = tk.Frame(main_frame)
        right_frame.pack(side=tk.RIGHT, fill=tk.BOTH, expand=True)
        tk.Label(right_frame, text="Метки:").pack(anchor=tk.W)
        self.bookmarks_listbox = tk.Listbox(right_frame, height=10)
        self.bookmarks_listbox.pack(fill=tk.BOTH, expand=True)
        self.bookmarks_listbox.bind('<<ListboxSelect>>', self.on_select_bookmark)

        # Виджет для визуализации (matplotlib)
        self.fig, self.ax = plt.subplots(figsize=(5, 2))
        self.canvas = FigureCanvasTkAgg(self.fig, master=right_frame)
        self.canvas.get_tk_widget().pack(fill=tk.BOTH, expand=True)
        self.ax.set_ylim(-1, 1)
        self.ax.set_xlim(0, 100)
        self.ax.set_facecolor('#f0f0f0')
        self.line, = self.ax.plot([], [], 'b-', linewidth=1)

        # Нижняя панель: комментарий к метке
        comment_frame = tk.Frame(self.root)
        comment_frame.pack(fill=tk.X, pady=5)
        tk.Label(comment_frame, text="Комментарий к метке:").pack(side=tk.LEFT, padx=5)
        self.comment_entry = tk.Entry(comment_frame, width=40)
        self.comment_entry.pack(side=tk.LEFT, padx=5)
        tk.Button(comment_frame, text="Добавить метку", command=self.add_bookmark).pack(side=tk.LEFT, padx=5)
        tk.Button(comment_frame, text="Экспорт меток", command=self.export_bookmarks).pack(side=tk.LEFT, padx=5)

        self.load_recordings()

    def start_recording(self):
        if self.is_recording:
            return
        self.frames = []
        self.bookmarks = []
        self.current_time = 0
        self.start_time = time.time()
        self.is_recording = True
        self.record_btn.config(state=tk.DISABLED)
        self.stop_btn.config(state=tk.NORMAL)
        self.bookmark_btn.config(state=tk.NORMAL)
        self.status_label.config(text="Идёт запись...")
        self.time_label.config(text="00:00:00")
        # Запуск потока записи
        self.recording_thread = threading.Thread(target=self.record_audio)
        self.recording_thread.start()
        # Обновление таймера
        self.update_time()

    def record_audio(self):
        self.stream = self.audio.open(format=self.format,
                                      channels=self.channels,
                                      rate=self.rate,
                                      input=True,
                                      frames_per_buffer=self.chunk)
        while self.is_recording:
            data = self.stream.read(self.chunk)
            self.frames.append(data)
            self.current_time = time.time() - self.start_time
        self.stream.stop_stream()
        self.stream.close()

    def stop_recording(self):
        if not self.is_recording:
            return
        self.is_recording = False
        if self.recording_thread:
            self.recording_thread.join(timeout=1)
        self.record_btn.config(state=tk.NORMAL)
        self.stop_btn.config(state=tk.DISABLED)
        self.bookmark_btn.config(state=tk.DISABLED)
        self.status_label.config(text="Запись остановлена")
        # Сохраняем файл
        filename = f"запись_{datetime.now().strftime('%Y-%m-%d_%H-%M-%S')}.wav"
        self.current_file = filename
        self.save_wav(filename)
        self.save_bookmarks(filename.replace('.wav', '_marks.json'))
        self.load_recordings()
        self.status_label.config(text=f"Сохранено: {filename}")

    def save_wav(self, filename):
        wf = wave.open(filename, 'wb')
        wf.setnchannels(self.channels)
        wf.setsampwidth(self.audio.get_sample_size(self.format))
        wf.setframerate(self.rate)
        wf.writeframes(b''.join(self.frames))
        wf.close()

    def save_bookmarks(self, filename):
        data = []
        for t, comment in self.bookmarks:
            data.append({"time": t, "comment": comment})
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)

    def add_bookmark(self):
        if self.is_recording:
            comment = self.comment_entry.get().strip()
            if not comment:
                comment = f"Метка {len(self.bookmarks)+1}"
            self.bookmarks.append((self.current_time, comment))
            self.comment_entry.delete(0, tk.END)
            self.status_label.config(text=f"Метка добавлена: {comment} (время {self.format_time(self.current_time)})")
            # Обновить список меток для текущей записи
            self.update_bookmarks_list()

    def update_time(self):
        if self.is_recording:
            self.time_label.config(text=self.format_time(self.current_time))
            self.root.after(100, self.update_time)

    def format_time(self, seconds):
        m, s = divmod(int(seconds), 60)
        h, m = divmod(m, 60)
        return f"{h:02d}:{m:02d}:{s:02d}"

    def load_recordings(self):
        self.recordings_listbox.delete(0, tk.END)
        files = [f for f in os.listdir('.') if f.endswith('.wav')]
        for f in sorted(files):
            self.recordings_listbox.insert(tk.END, f)

    def on_select_recording(self, event):
        selection = self.recordings_listbox.curselection()
        if not selection:
            return
        filename = self.recordings_listbox.get(selection[0])
        self.current_file = filename
        # Загружаем метки
        marks_file = filename.replace('.wav', '_marks.json')
        if os.path.exists(marks_file):
            with open(marks_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            self.bookmarks = [(entry['time'], entry['comment']) for entry in data]
        else:
            self.bookmarks = []
        self.update_bookmarks_list()
        self.status_label.config(text=f"Выбрана запись: {filename}")

    def update_bookmarks_list(self):
        self.bookmarks_listbox.delete(0, tk.END)
        for i, (t, comment) in enumerate(self.bookmarks):
            self.bookmarks_listbox.insert(tk.END, f"{i+1}. {self.format_time(t)} - {comment}")

    def on_select_bookmark(self, event):
        # При воспроизведении можно перейти к метке
        selection = self.bookmarks_listbox.curselection()
        if not selection:
            return
        idx = selection[0]
        if 0 <= idx < len(self.bookmarks):
            t = self.bookmarks[idx][0]
            self.status_label.config(text=f"Переход к метке {idx+1} (время {self.format_time(t)})")
            # Если воспроизводится, можно перемотать

    def play_selected(self):
        if not self.current_file:
            messagebox.showwarning("Предупреждение", "Выберите запись")
            return
        if self.is_playing:
            return
        # Воспроизведение в отдельном потоке
        self.is_playing = True
        self.play_thread = threading.Thread(target=self.play_audio)
        self.play_thread.start()

    def play_audio(self):
        wf = wave.open(self.current_file, 'rb')
        p = pyaudio.PyAudio()
        stream = p.open(format=p.get_format_from_width(wf.getsampwidth()),
                        channels=wf.getnchannels(),
                        rate=wf.getframerate(),
                        output=True)
        data = wf.readframes(self.chunk)
        while data and self.is_playing:
            stream.write(data)
            data = wf.readframes(self.chunk)
        stream.stop_stream()
        stream.close()
        p.terminate()
        self.is_playing = False
        self.status_label.config(text="Воспроизведение завершено")

    def delete_selected(self):
        if not self.current_file:
            return
        if messagebox.askyesno("Подтверждение", f"Удалить {self.current_file} и связанные метки?"):
            os.remove(self.current_file)
            marks_file = self.current_file.replace('.wav', '_marks.json')
            if os.path.exists(marks_file):
                os.remove(marks_file)
            self.current_file = None
            self.bookmarks = []
            self.update_bookmarks_list()
            self.load_recordings()
            self.status_label.config(text="Запись удалена")

    def export_bookmarks(self):
        if not self.bookmarks:
            messagebox.showinfo("Информация", "Нет меток для экспорта")
            return
        filename = filedialog.asksaveasfilename(defaultextension=".txt", filetypes=[("Text files", "*.txt")])
        if filename:
            with open(filename, 'w', encoding='utf-8') as f:
                f.write("Метки для записи:\n")
                for i, (t, comment) in enumerate(self.bookmarks):
                    f.write(f"{i+1}. {self.format_time(t)} - {comment}\n")
            self.status_label.config(text=f"Метки экспортированы в {filename}")

    def on_close(self):
        if self.is_recording:
            self.stop_recording()
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = Recorder(root)
    root.mainloop()
