// recorder_js.js — диктофон с закладками на JavaScript (Electron)

const { app, BrowserWindow, Menu, dialog, ipcMain } = require('electron');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

let mainWindow;
let recorderState = {
    isRecording: false,
    isPlaying: false,
    startTime: 0,
    bookmarks: [],
    currentFile: null,
    audioChunks: []
};
let mediaRecorder = null;
let audioContext = null;
let analyser = null;

function createWindow() {
    mainWindow = new BrowserWindow({
        width: 800,
        height: 600,
        webPreferences: {
            nodeIntegration: true,
            contextIsolation: false
        }
    });
    mainWindow.loadFile('index.html');
    Menu.setApplicationMenu(Menu.buildFromTemplate([
        { label: 'Файл', submenu: [{ role: 'quit' }] }
    ]));

    // IPC handlers
    ipcMain.handle('start-recording', async () => {
        try {
            const stream = await navigator.mediaDevices.getUserMedia({ audio: true });
            const options = { mimeType: 'audio/webm' };
            mediaRecorder = new MediaRecorder(stream, options);
            recorderState.audioChunks = [];
            mediaRecorder.ondataavailable = (e) => {
                if (e.data.size > 0) recorderState.audioChunks.push(e.data);
            };
            mediaRecorder.onstop = () => {
                const blob = new Blob(recorderState.audioChunks, { type: 'audio/webm' });
                const filename = `запись_${new Date().toISOString().replace(/[:.]/g, '-')}.webm`;
                const filePath = path.join(app.getPath('userData'), filename);
                const buffer = Buffer.from(await blob.arrayBuffer());
                fs.writeFileSync(filePath, buffer);
                recorderState.currentFile = filePath;
                saveBookmarks(filePath.replace('.webm', '_marks.json'));
                mainWindow.webContents.send('recording-stopped', filePath);
            };
            mediaRecorder.start();
            recorderState.isRecording = true;
            recorderState.startTime = Date.now();
            recorderState.bookmarks = [];
            // Обновляем время
            const updateInterval = setInterval(() => {
                if (recorderState.isRecording) {
                    const elapsed = (Date.now() - recorderState.startTime) / 1000;
                    mainWindow.webContents.send('time-update', elapsed);
                } else {
                    clearInterval(updateInterval);
                }
            }, 100);
            return { success: true };
        } catch (e) {
            return { success: false, error: e.message };
        }
    });

    ipcMain.handle('stop-recording', () => {
        if (mediaRecorder && recorderState.isRecording) {
            mediaRecorder.stop();
            recorderState.isRecording = false;
            return { success: true };
        }
        return { success: false };
    });

    ipcMain.handle('add-bookmark', (event, comment) => {
        if (recorderState.isRecording) {
            const elapsed = (Date.now() - recorderState.startTime) / 1000;
            if (!comment) comment = `Метка ${recorderState.bookmarks.length+1}`;
            recorderState.bookmarks.push({ time: elapsed, comment });
            return { success: true, bookmark: { time: elapsed, comment } };
        }
        return { success: false };
    });

    ipcMain.handle('get-bookmarks', () => {
        return recorderState.bookmarks;
    });

    ipcMain.handle('list-recordings', () => {
        const files = fs.readdirSync(app.getPath('userData')).filter(f => f.endsWith('.webm'));
        return files;
    });

    ipcMain.handle('play-recording', (event, filename) => {
        const filePath = path.join(app.getPath('userData'), filename);
        if (fs.existsSync(filePath)) {
            // Воспроизведение через системный плеер (упрощённо)
            const cmd = process.platform === 'win32' ? 'start' : (process.platform === 'darwin' ? 'open' : 'xdg-open');
            exec(`${cmd} "${filePath}"`);
            return { success: true };
        }
        return { success: false };
    });

    ipcMain.handle('delete-recording', (event, filename) => {
        const filePath = path.join(app.getPath('userData'), filename);
        if (fs.existsSync(filePath)) {
            fs.unlinkSync(filePath);
            const marksFile = filePath.replace('.webm', '_marks.json');
            if (fs.existsSync(marksFile)) fs.unlinkSync(marksFile);
            return { success: true };
        }
        return { success: false };
    });

    ipcMain.handle('load-bookmarks', (event, filename) => {
        const marksFile = path.join(app.getPath('userData'), filename.replace('.webm', '_marks.json'));
        if (fs.existsSync(marksFile)) {
            const data = fs.readFileSync(marksFile, 'utf8');
            return JSON.parse(data);
        }
        return [];
    });

    ipcMain.handle('export-bookmarks', (event, filename) => {
        const marksFile = path.join(app.getPath('userData'), filename.replace('.webm', '_marks.json'));
        if (!fs.existsSync(marksFile)) return { success: false, error: 'Нет меток' };
        const data = fs.readFileSync(marksFile, 'utf8');
        const bookmarks = JSON.parse(data);
        const txtFile = filename.replace('.webm', '_marks.txt');
        const txtPath = path.join(app.getPath('userData'), txtFile);
        let content = 'Метки для записи:\n';
        bookmarks.forEach((b, i) => {
            content += `${i+1}. ${formatTime(b.time)} - ${b.comment}\n`;
        });
        fs.writeFileSync(txtPath, content);
        return { success: true, file: txtFile };
    });

    function saveBookmarks(marksFile) {
        fs.writeFileSync(marksFile, JSON.stringify(recorderState.bookmarks, null, 2));
    }

    function formatTime(seconds) {
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = Math.floor(seconds % 60);
        return `${h.toString().padStart(2,'0')}:${m.toString().padStart(2,'0')}:${s.toString().padStart(2,'0')}`;
    }

    mainWindow.on('closed', () => {
        mainWindow = null;
    });
}

app.whenReady().then(createWindow);

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') app.quit();
});
