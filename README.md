# LowPS5 — Оптимизированный эмулятор PlayStation 5 для слабых ПК (Windows 11)

**LOWps5** is an experimental PS5 emulator for PC based on **KytyPS5**, with a low-spec profile for **Windows 11 x64**. It includes reduced initial cache allocations, configurable render scale, shader/pipeline caching, and linear/nearest presentation filtering. These changes may reduce overhead on some integrated GPUs, but they do not guarantee that a game will boot or run at playable speed.

## Важный статус функций

- **540p (960×540):** доступно как разрешение окна лаунчера. Это само по себе не заставляет игру рендерить внутренний кадр в 540p; за это отвечает отдельный render scale, если игра/рендер-путь его допускает.
- **Текущий --fsr:** в swapchain.cpp выбирает линейную фильтрацию при vkCmdBlitImage (или nearest при выключенном флаге). Это **не AMD FSR 1/2/3**.
- **FSR 2 / FSR 3 и генерация кадров:** не реализованы в текущем коде. Для настоящего temporal upscaling нужны история кадров, глубина, motion vectors и jitter; frame generation дополнительно требует корректной интеграции frame pacing и latency.
- **DualSense / DualShock 4:** в коде присутствуют обработка событий тачпада и пути вибрации/гаптики. Совместимость зависит от устройства, режима USB/Bluetooth и драйверов; это не равнозначно проверке на всех контроллерах.
- **Управление лаунчером геймпадом:** эмуляторный ввод и навигация по самому окну Qt — разные задачи. Полная навигация по меню контроллером пока не заявлена как готовая функция.
- **8 ГБ RAM:** сниженные кэши уменьшают стартовые аллокации, но не делают 8 ГБ гарантированным системным минимумом. Эмуляция PS5 остаётся экспериментальной и может упираться в память, драйвер или несовместимость конкретной игры.

---

## Что реализовано в этой сборке LowPS5 (Lightweight Optimization)

### 1. Импортированные библиотеки и подсистемы KytyPS5
В репозиторий `lowps5` полностью интегрировано ядро и все системные и сторонние библиотеки **KytyPS5**:
- **`src/common/`** — базовые примитивы, виртуальная память Windows (`VirtualAlloc2` / placeholder mappings), высокоточные таймеры Windows 11 (`CREATE_WAITABLE_TIMER_HIGH_RESOLUTION`), пул потоков, система логирования и конфигуратор `emulatorConfig`.
- **`src/kernel/`** — эмуляция ядра PS5 (`libkernel`: управление виртуальной и прямой памятью `DirectMemory` / `FlexibleMemory`, `EventQueue`, `EventFlag`, `Semaphore`, `Pthread`, `SyncOnAddress`, виртуальная файловая система).
- **`src/graphics/`** — графический стек PS5 (`GuestGpu`, командный процессор `PM4`, рекомпилятор шейдеров `ShaderRecompiler` в `SPIR-V`, рендерер `Vulkan 1.3`, `BufferCache`, `TextureCache`, `SamplerCache`, `PipelineCache`, `Presenter` и `Swapchain`).
- **`src/libs/`** — системные модули PS5 (`libAgc`, `libVideoOut`, `libAudio` / `libAudio2` / `ngs2` / `ajm`, `libPad` / `DualSense`, `libSaveData`, `libTrophy`, `libSystemService`, `libUserService`, `libNet`, `libHttp`, `libVdecsw`, `avPlayer`, `libImeDialog`).
- **`src/loader/`** — загрузчик `ELF` / `ZArchive`, динамический компоновщик (`RuntimeLinker`), патчер инструкций x86-64 (`guestInstructionPatcher`, `x64InstructionEmulator`).
- **`src/launcher/`** — графический лаунчер на базе **Qt 6** с поддержкой низких разрешений (`640x360`, `854x480`, `960x540`, `1280x720`) и параметров `LowPS5`.
- **`3rdparty/`** — все необходимые зависимости (`Vulkan-Headers`, `VulkanMemoryAllocator`, `SPIRV-Headers`, `SPIRV-Tools`, `SDL3`, `fmt`, `spdlog`, `xxHash`, `cpuinfo`, `imgui`, `LibAtrac9`, `nlohmann_json`, `ffmpeg-core`, `winpthread`).

### 2. Легкая оптимизация под слабые ПК (Windows 11 Low-Spec Profile)
По умолчанию в `LowPS5` активирован профиль **`--low-spec true`**, включающий следующие оптимизации:
1. **Оптимизация планировщика и энергопотребления Windows 11 (`src/emulator.cpp`, `src/common/threads.cpp`)**:
   - Автоматическое повышение приоритета процесса (`ABOVE_NORMAL_PRIORITY_CLASS`) и включение динамического буста приоритетов (`SetProcessPriorityBoost`).
   - Принудительное отключение **Windows 11 Power Throttling / EcoQoS** (`PROCESS_POWER_THROTTLING_EXECUTION_SPEED` и `THREAD_POWER_THROTTLING_EXECUTION_SPEED`) для процесса и всех рабочих потоков эмулятора, чтобы планировщик Windows 11 не переводил потоки эмуляции в энергосберегающие состояния на гибридных и бюджетных процессорах.
   - Снижение количества холостых циклов ожидания (`KYTY_CS_SPIN_COUNT_LOW_SPEC = 1500` вместо `4000`) на процессорах с числом потоков `<= 8` и ускоренный опрос старта потоков (`250 мкс` вместо `1000 мкс`).
2. **Экономия видеопамяти (VRAM) и оперативной памяти (`src/graphics/host_gpu/vma.cpp`, `bufferCache.cpp`, `textureCache.cpp`)**:
   - Уменьшение стартового размера сервисных кольцевых буферов `BufferCache` в режиме `Low-Spec`:
     - `Staging (Upload)`: **192 МБ** вместо **512 МБ**
     - `Stream`: **32 МБ** вместо **64 МБ**
     - `Download`: **32 МБ** вместо **64 МБ**
     - `DeviceLocal`: **64 МБ** вместо **128 МБ**
     - **Суммарная экономия при старте:** **448 МБ VRAM/RAM** (320 МБ вместо 768 МБ).
   - Адаптация расчета бюджета памяти `VulkanMemoryAllocator` (`vma.cpp`) для встроенной графики (iGPU Intel UHD / Iris Xe / AMD Radeon Vega на ноутбуках с 8–12 ГБ ОЗУ): системный резерв снижен с 8 ГБ до 2 ГБ, что предотвращает обнуление доступного пула памяти.
   - Более ранняя и агрессивная сборка мусора (`RunGarbageCollector`) в `BufferCache` и `TextureCache` для видеокарт с 2–4 ГБ видеопамяти.
3. **Оптимизация рендеринга, фильтрации и шейдеров (`samplerCache.cpp`, `pipelineCache.cpp`, `shaders.cpp`, `vulkanWindow.cpp`)**:
   - Снятие жесткого ограничения `maxSamplerAnisotropy < 16.0f` при инициализации Vulkan-устройства в режиме `Low-Spec` (позволяет запускать эмулятор на бюджетных и встроенных видеокартах).
   - Ограничение анизотропной фильтрации по умолчанию до **2x** (`--max-anisotropy 2`) и легкий положительный сдвиг `mipLodBias (+0.25)` в `SamplerCache` для снижения нагрузки на ПСП видеопамяти.
   - Снижение нагрузки от мультисэмплинга (`minSampleShading = 0.5f` вместо `1.0f` в `shaders.cpp`).
   - Встроенная оптимизация `SPIR-V` через `spvtools::Optimizer` (`RegisterPerformancePasses` / `RegisterSizePasses`) и постоянное сохранение дискового кэша конвейеров Vulkan (`_PipelineCache/<TITLE_ID>.bin`) даже при локальной сборке без тегов Git.
   - Отключение отладочного спама ввода (`KYTY_DBG_INPUT`) и пропускание стартовой заставки (`--no-splash`, `--skip-notice-screen true`), что устраняет микрофризы при движении мыши/геймпада и ускоряет загрузку игр.

---

## Параметры командной строки LowPS5

```text
lowps5.exe --game <dir|elf|zar> [options]

Основные параметры оптимизации LowPS5:
  --low-spec <true|false>              Включить оптимизации для слабых ПК (по умолчанию: true)
  --render-scale <25-100>              Масштаб внутреннего разрешения рендеринга в % (по умолчанию: 75)
  --max-anisotropy <1-16>              Лимит анизотропной фильтрации текстур (по умолчанию: 2)
  --fsr <true|false>                   Линейная/nearest-фильтрация вывода (не AMD FSR; по умолчанию: true)
  --screen-width <num>                 Ширина окна (например: 640, 854, 960, 1280; по умолчанию: 1280)
  --screen-height <num>                Высота окна (например: 360, 480, 540, 720; по умолчанию: 720)
  --present-mode <Fifo|Mailbox|Immediate> Режим синхронизации кадров Vulkan (по умолчанию: Mailbox)
  --shader-optimization-type <None|Size|Performance> Оптимизация SPIR-V шейдеров (по умолчанию: Performance)
  --skip-notice-screen <true|false>    Пропуск стартовых экранов предупреждений (по умолчанию: true)
```

Пример запуска на слабом ПК (Windows 11):
```cmd
lowps5.exe --game "C:\Games\PS5Game" --screen-width 960 --screen-height 540 --low-spec true --render-scale 75 --max-anisotropy 2 --present-mode Fifo
```

---

## Сборка на Windows 11 (x64)

### Требования
- **ОС:** Windows 11 (64-bit)
- **Компилятор:** Visual Studio 2022 с компонентом **LLVM (`clang-cl`) и `lld-link`**
- **Сборка:** `CMake >= 3.19`, `Ninja`, `glslangValidator` (`vcpkg install glslang[tools,opt]:x64-windows`)
- **Опционально (для GUI-лаунчера):** `Qt 6.10+` (`msvc2022_64`). Если собирать только ядро эмулятора без лаунчера, передайте `-DKYTY_BUILD_LAUNCHER=OFF`.

### Команды сборки (Developer Command Prompt for VS 2022)
```cmd
cmake -S . -B _Build/windows ^
  -G Ninja ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ^
  -DLOWPS5_OPTIMIZE_LOW_SPEC=ON ^
  -DKYTY_BUILD_LAUNCHER=OFF ^
  -DCMAKE_C_COMPILER=clang-cl ^
  -DCMAKE_CXX_COMPILER=clang-cl

cmake --build _Build/windows --target kyty_emulator --parallel
```
После сборки в папке `_Build/windows` будут созданы исполняемые файлы `lowps5.exe` и `kyty_emulator.exe`.
