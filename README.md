# AynThor: Dual Screen GDExtension Plugin

AynThor is a powerful Godot plugin designed to handle multiple screens on Android handhelds like **Ayn Thor**, **Retroid Pocket 5**, and **RG DS**. It provides a low-latency Vulkan-based renderer to project a secondary viewport onto a physical second display.

---

## Quick Start (Automated Build)

The easiest way to build the plugin is using **Docker**. You don't need to install Android SDK, NDK, Python, or SCons manually.

### Prerequisites
- [Docker](https://www.docker.com/)
- Docker Compose

### Building the Plugin
1. Open your terminal in the `AynThor/` directory.
2. Run the following command:
   ```bash
   docker-compose up --build
   ```
3. Once completed, the ready-to-use plugin will be in `AynThor/dist/AynThor`.
4. Copy the `dist/AynThor` folder into your Godot project's `addons/` directory.

> **Note**: If you don't want to wait for the build process, you can find the pre-compiled binaries (for both **Android** and **Windows**) already available in the `AynThor/dist/AynThor` folder within this repository.

---

## How to Use

### 1. Engine Setup
*   Go to **Project -> Install Android Build Template**.
*   In **Project Settings**, ensure **Export -> Android -> Use Gradle Build** is ENABLED. The plugin will not work without a Gradle build.
*   Enable the `AynThor` plugin in **Project Settings -> Plugins**.

### 2. Scene Setup
1.  Create two `SubViewportContainer` nodes in your scene (one for the main screen, one for the secondary screen).
2.  Add a `SubViewport` as a child to each container.
3.  Create an empty `Node` and attach the `AynThorManager.gd` script to it.
4.  In the Inspector for the Manager node, assign the references to your ViewportContainers and SubViewports.

### Pro Tip: Single-Screen Mode
If you don't want to wrap your entire game into `SubViewportContainers`, you can use the plugin **only for the secondary screen**. Just create a viewport for the second display and leave the main game as is.
*   **Limitation**: In this mode, you **cannot use the swapping feature**, as the plugin will not have control over the main window's rendering.
---

## How it Works

AynThor uses a hybrid architecture to bridge Godot's Rendering Device with Android's Presentation API:

1.  **Android Layer (Kotlin)**: Detects secondary displays and creates a `Presentation` window with a `SurfaceView`.
2.  **JNI Bridge (C++)**: Passes the native `ANativeWindow` handle from the Android Surface to the GDExtension.
3.  **Vulkan Renderer (C++/GDExtension)**:
    *   Creates a separate Vulkan Swapchain for the secondary display.
    *   Uses `vkCmdBlitImage` to copy the frame from a Godot `SubViewport` texture directly to the second screen's swapchain.
    *   Supports hardware-level rotation and scaling.

---

## Technical Details

### Compatibility
- **Godot Engine**: 4.5+ (including 4.6.x)
- **Architecture**: `arm64-v8a` (Android), `x86_64` (Windows Editor support)
- **Minimum Android API**: 24 (Nougat)

### Project Structure
- `CPP/`: Native Vulkan implementation and JNI hooks.
- `Kotlin/`: Android Presentation and Activity lifecycle management.
- `GDScript/`: Godot-side manager (`AynThorManager.gd`) and editor export scripts.

---