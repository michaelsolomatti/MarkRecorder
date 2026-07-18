// recorder_go.go — диктофон с закладками на Go (консоль + запись через arecord)

package main

import (
	"bufio"
	"encoding/json"
	"fmt"
	"os"
	"os/exec"
	"strconv"
	"strings"
	"time"
)

type Bookmark struct {
	Time    float64 `json:"time"`
	Comment string  `json:"comment"`
}

type Recorder struct {
	isRecording bool
	startTime   time.Time
	bookmarks   []Bookmark
	currentFile string
}

func NewRecorder() *Recorder {
	return &Recorder{
		bookmarks: []Bookmark{},
	}
}

func (r *Recorder) startRecording() {
	if r.isRecording {
		fmt.Println("Уже идёт запись")
		return
	}
	// Проверяем наличие arecord или sox
	_, err := exec.LookPath("arecord")
	if err != nil {
		fmt.Println("Утилита arecord не найдена. Установите ALSA-utils (Linux) или используйте sox.")
		return
	}
	r.isRecording = true
	r.startTime = time.Now()
	r.bookmarks = []Bookmark{}
	filename := fmt.Sprintf("запись_%s.wav", time.Now().Format("2006-01-02_15-04-05"))
	r.currentFile = filename
	fmt.Printf("Запись начата. Файл: %s\n", filename)
	fmt.Println("Команды: stop, mark [комментарий], exit")
	// Запуск записи в фоне
	go func() {
		cmd := exec.Command("arecord", "-d", "0", "-f", "cd", "-t", "wav", "-q", filename)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr
		err := cmd.Run()
		if err != nil {
			fmt.Println("Ошибка записи:", err)
		}
		r.isRecording = false
		fmt.Println("Запись остановлена")
	}()
}

func (r *Recorder) stopRecording() {
	if !r.isRecording {
		fmt.Println("Нет активной записи")
		return
	}
	// Убиваем процесс arecord
	// В реальном коде нужно найти PID и убить, но упростим — пользователь нажмёт Ctrl+C
	fmt.Println("Для остановки нажмите Ctrl+C, затем выполните команду stop.")
	// В данном примере мы не можем остановить arecord программно без сигнала.
	// Поэтому пользователь должен остановить запись вручную.
}

func (r *Recorder) addBookmark(comment string) {
	if !r.isRecording {
		fmt.Println("Нет активной записи")
		return
	}
	elapsed := time.Since(r.startTime).Seconds()
	if comment == "" {
		comment = fmt.Sprintf("Метка %d", len(r.bookmarks)+1)
	}
	r.bookmarks = append(r.bookmarks, Bookmark{Time: elapsed, Comment: comment})
	fmt.Printf("Метка добавлена: %s (%.2f с)\n", comment, elapsed)
}

func (r *Recorder) saveBookmarks() {
	if r.currentFile == "" {
		return
	}
	marksFile := strings.Replace(r.currentFile, ".wav", "_marks.json", 1)
	data, _ := json.MarshalIndent(r.bookmarks, "", "  ")
	os.WriteFile(marksFile, data, 0644)
	fmt.Println("Метки сохранены в", marksFile)
}

func (r *Recorder) listRecordings() {
	files, _ := os.ReadDir(".")
	for _, f := range files {
		if strings.HasSuffix(f.Name(), ".wav") {
			fmt.Println("  " + f.Name())
		}
	}
}

func (r *Recorder) showBookmarks(filename string) {
	marksFile := strings.Replace(filename, ".wav", "_marks.json", 1)
	data, err := os.ReadFile(marksFile)
	if err != nil {
		fmt.Println("Нет меток для", filename)
		return
	}
	var marks []Bookmark
	json.Unmarshal(data, &marks)
	fmt.Printf("Метки для %s:\n", filename)
	for i, m := range marks {
		fmt.Printf("  %d. %.2f с - %s\n", i+1, m.Time, m.Comment)
	}
}

func main() {
	rec := NewRecorder()
	scanner := bufio.NewScanner(os.Stdin)
	fmt.Println("🎙️ MarkRecorder — Go Edition")
	fmt.Println("Команды: start, stop, mark [комментарий], list, marks <файл>, exit")
	for {
		fmt.Print("> ")
		if !scanner.Scan() {
			break
		}
		line := strings.TrimSpace(scanner.Text())
		parts := strings.SplitN(line, " ", 2)
		cmd := parts[0]
		arg := ""
		if len(parts) > 1 {
			arg = parts[1]
		}
		switch cmd {
		case "start":
			rec.startRecording()
		case "stop":
			rec.stopRecording()
			rec.saveBookmarks()
		case "mark":
			rec.addBookmark(arg)
		case "list":
			rec.listRecordings()
		case "marks":
			if arg == "" {
				fmt.Println("Укажите имя файла, например: marks запись_2025-01-27_10-30.wav")
			} else {
				rec.showBookmarks(arg)
			}
		case "exit":
			if rec.isRecording {
				fmt.Println("Запись ещё идёт. Остановите её перед выходом.")
			} else {
				fmt.Println("До свидания!")
				return
			}
		default:
			fmt.Println("Неизвестная команда")
		}
	}
}
