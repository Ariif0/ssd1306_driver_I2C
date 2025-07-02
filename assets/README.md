# 🚀 Panduan Simulasi dengan Wokwi 🔍

This document provides a comprehensive guide to simulating this SSD1306 project using **Wokwi**. With Wokwi, you can run, test, and interact with your code on a virtual ESP32 directly from Visual Studio Code, **without requiring physical hardware**.

This method is ideal for rapid development, debugging, and ensuring your code works before flashing it to a real device.

---

## ⚙️ Simulation in Visual Studio Code

This approach integrates Wokwi into your development workflow in VS Code, allowing you to view simulation results in real-time.

For more in-depth information, refer to the official documentation at **[Wokwi for VS Code](https://docs.wokwi.com/vscode/getting-started)**.

### ✅ Prerequisites

Ensure you meet the following requirements:

- ✔️ **Visual Studio Code** is installed.
- ✔️ This project has been cloned to your computer.
- ✔️ The **[Wokwi for VS Code](https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode)** extension is installed from the Marketplace.

### 📋 Project Configuration

To enable simulation, add the following configuration files to the root directory of your project.

1. **Copy Configuration Files**:
   Copy the `diagram.json` and `wokwi.toml` files from the `assets/wokwi/` folder in this repository.

   ```
   assets/wokwi/
   ├── diagram.json
   └── wokwi.toml
   ```

   Move both files to your project’s root directory, alongside `platformio.ini` or `CMakeLists.txt`.

2. **Configuration File Contents (For Reference)**:
   <details>
   <summary>View contents of <code>wokwi.toml</code></summary>

   ```toml
   # File: wokwi.toml
   # This configuration tells Wokwi where to find the compiled firmware.
   [wokwi]
   version = 1
   firmware = '.pio/build/esp32dev/firmware.bin' # For PlatformIO
   elf = '.pio/build/esp32dev/firmware.elf'      # For PlatformIO
   # If using pure ESP-IDF, adjust the path to the 'build/' folder.
   ```
   </details>

   <details>
   <summary>View contents of <code>diagram.json</code></summary>

   ```json
   // File: diagram.json
   // This file describes the virtual circuit and component connections.
   {
     "version": 1,
     "author": "Muhamad Arif Hidayat",
     "editor": "wokwi",
     "parts": [
       {
         "type": "board-esp32-devkit-c-v4",
         "id": "esp",
         "top": 0,
         "left": -4.76,
         "attrs": { "builder": "esp-idf" }
       },
       {
         "type": "wokwi-pushbutton",
         "id": "btn1",
         "top": 131,
         "left": 115.2,
         "attrs": { "color": "black", "xray": "1" }
       },
       {
         "type": "board-ssd1306",
         "id": "oled1",
         "top": 31.94,
         "left": 125.03,
         "attrs": { "i2cAddress": "0x3c" }
       },
       {
         "type": "wokwi-pushbutton",
         "id": "btn2",
         "top": 131,
         "left": 201.6,
         "attrs": { "color": "black", "xray": "1" }
       }
     ],
     "connections": [
       [ "esp:TX", "$serialMonitor:RX", "", [] ],
       [ "esp:RX", "$serialMonitor:TX", "", [] ],
       [ "oled1:SDA", "esp:21", "cyan", [ "v-38.4", "h-76.73", "v76.8" ] ],
       [ "oled1:SCL", "esp:22", "blue", [ "v-48", "h-76.5", "v124.8" ] ],
       [ "oled1:VCC", "esp:3V3", "red", [ "v-19.2", "h-172.65", "v76.8" ] ],
       [ "oled1:GND", "esp:GND.2", "black", [ "v-28.8", "h182.4" ] ],
       [ "btn1:2.l", "esp:GND.3", "black", [ "h-9.6", "v-76.6" ] ],
       [ "btn2:2.l", "btn1:2.r", "black", [ "h0" ] ],
       [ "esp:16", "btn1:1.l", "purple", [ "h9.6", "v9.6" ] ],
       [ "esp:17", "btn2:1.l", "magenta", [ "h0" ] ]
     ],
     "dependencies": {}
   }
   ```
   > **Note**: This circuit connects SDA to **GPIO21** and SCL to **GPIO22**. Ensure your code is configured to use the same pins.
   </details>

### ▶️ Running the Simulation

Follow these three simple steps to start the simulation.

1. **🛠️ Build the Project**  
   Before simulating, you **must** compile the project to generate the `.bin` and `.elf` files required by Wokwi.
   - **PlatformIO**: `pio run`
   - **ESP-IDF**: `idf.py build`

2. **🚀 Start the Simulator**  
   Open the *Command Palette* in VS Code (`F1` or `Ctrl+Shift+P`), then type and select:  
   `Wokwi: Start Simulator`

3. **👀 View the Results**  
   A new tab will open in VS Code displaying your virtual circuit diagram. The simulation will run automatically:
   - The OLED display will light up and show graphical output.
   - Logs from `ESP_LOG` will appear in the VS Code "Serial Monitor" panel.

---

### 💡 Additional Tips

- **Update Code? Rebuild!** Whenever you modify your code, rerun the build command before restarting the simulation to ensure Wokwi loads the latest firmware.
- **Interactive Experience.** Click components in the diagram (e.g., buttons or sensors, if included) to interact with them during simulation.
- **Customize the Circuit.** The `diagram.json` file is highly flexible. You can add other components like DHT22 sensors, LEDs, potentiometers, and more. Check the [Wokwi documentation](https://docs.wokwi.com/parts/wokwi-dht22) for a full list of supported parts.
