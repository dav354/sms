## 🛠️ Basic Minicom Setup to Talk to ESP32

### 🚀 Start Minicom:

Getting the correct USB Port:

```bash
sudo dmesg | grep ttyUSB
```

and connecting to the esp:

```bash
minicom -D /dev/ttyUSB0 -b 115200q
```

- `-D /dev/ttyUSB0`: device file of your ESP32 (adjust if needed)
- `-b 115200`: baud rate (match this with your ESP32's UART config)

sending a message:

```bash

```

---

## ⚙️ Helpful Minicom Shortcuts

| Shortcut            | Description                       |
|---------------------|-----------------------------------|
| `Ctrl-A Z`          | Show help menu                    |
| `Ctrl-A O`          | Enter configuration/settings      |
| `Ctrl-A Q`          | Quit without saving               |
| `Ctrl-A X`          | Quit and save settings            |
| `Ctrl-A F`          | Toggle local echo (see what you type) |
| `Ctrl-A L`          | Clear the screen                  |
| `Ctrl-A S`          | Send file                         |
| `Ctrl-A R`          | Receive file                      |

> ⚠️ When you type `Ctrl-A`, then press the **next key** (e.g. `Z`) quickly.

---

## 🧪 Testing UART with ESP32

1. **ESP32 Side**: Make sure it's listening on UART and responds to text input.
2. **Minicom Side**:
   - Start typing and hit Enter
   - You should see a response from ESP32 (e.g. "Hello from ESP!")
