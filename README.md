# Dante CLI — Windows Native

> Terminal nativo Windows com superpoderes. C++20/Qt 6, ConPTY, zero Electron.

Reimplementação Windows-native do app Dante CLI (originalmente macOS/SwiftUI), seguindo o plano técnico em `prompt.md`. Sem Electron, sem WebView na UI principal, sem JavaScript no terminal. Camada Core portável reaproveitada no futuro para Linux/macOS.

## Status atual (1.0.28-alpha)

**Fases concluídas (0–2 do roadmap):**
- ✅ Scaffolding completo (CMake superbuild, 5 libs + app)
- ✅ ConPTY MVP com Job Object kill-on-close
- ✅ Parser ANSI VT100/VT220 tabela-driven + 256/truecolor
- ✅ TerminalWidget QPainter com glyph rendering
- ✅ SQLite WAL + schema migrations + JSON config
- ✅ MainWindow + TabBar + sidebar dock + toolbar AI launchers
- ✅ 10 color schemes (Dracula, Tokyo Night, Nord, One Dark, …)
- ✅ Detecção automática de shells (cmd, PowerShell, pwsh, Git Bash, WSL)
- ✅ i18n pt-BR + EN
- ✅ Pipeline de build (PowerShell) + instalador (NSIS + Inno Setup)

**Fases pendentes (3–9 do roadmap):**
- ⏳ Split workspace com 25 presets
- ⏳ Editor pane (KSyntaxHighlighting)
- ⏳ Preview pane (imagens, PDF, áudio)
- ⏳ Voz (QtMultimedia + Groq Whisper)
- ⏳ Update checker
- ⏳ MSIX signing

## Arquitetura

```
app  →  ui  →  terminal  →  platform  →  core
              ↘ persistence ↗
```

Dependência one-way. Core sem Qt UI. Platform isola Win32. UI nunca acessa HANDLE.

```
dante-cli/
├── CMakeLists.txt
├── cmake/                       DanteVersion, DanteCompileOptions
├── src/
│   ├── core/                    libdante_core      (domínio puro, sem Qt UI)
│   │   ├── domain/              Tab, Session, Favorite, Snippet, …
│   │   ├── parsing/             AnsiParser, AnsiTypes
│   │   ├── services/            IShellLauncher, ITabRepository
│   │   └── util/                Result, Uuid, Logger
│   ├── platform/                libdante_platform  (Win32 + ConPTY)
│   │   ├── pty/conpty/          WindowsHandle RAII, ConPtyBackend
│   │   ├── process/             ShellResolver
│   │   ├── memstat/             ProcessStats (PROCESS_MEMORY_COUNTERS_EX)
│   │   └── fs/                  PathUtils (long paths, UNC)
│   ├── persistence/             libdante_persistence (SQLite + JSON)
│   │   ├── schema/              001_initial.sql + Migrations
│   │   ├── repository/          Database, SqliteTabRepository
│   │   └── config/              AppConfig (JSON debounced)
│   ├── terminal/                libdante_terminal  (engine + widget)
│   │   ├── engine/              TerminalBuffer, Scrollback, ColorPalette
│   │   ├── widget/              TerminalWidget (QPainter)
│   │   └── registry/            TerminalRegistry (singleton por session-id)
│   ├── ui/                      libdante_ui        (Qt Widgets)
│   │   ├── main_window/         MainWindow
│   │   ├── tabs/                TabBar
│   │   ├── sidebar/             SidebarDock (4 modos)
│   │   ├── toolbar/             ToolbarWidget (AI launchers, stats)
│   │   └── settings/            SettingsDialog
│   └── app/                     main.cpp + .rc + resources
├── resources/                   icons, themes (.qss), translations (.ts)
├── installer/
│   ├── inno/dante-cli.iss       Inno Setup script
│   ├── nsis/dante-cli.nsi       NSIS script
│   └── assets/                  wizard BMPs
├── scripts/
│   ├── build.ps1                Configure + build + windeployqt
│   ├── make-installer.ps1       Build app + package installer
│   ├── make-icon.py             Generates app.ico (multi-res)
│   └── make-wizard-images.py    Generates wizard BMPs
└── tests/                       (Catch2/Qt Test — placeholders)
```

## Requisitos pra build

| Ferramenta | Versão mínima | Onde obter |
|---|---|---|
| Windows | 10 1809+ ou Windows 11 | (ConPTY requer Windows 10 build 17763+) |
| Visual Studio Build Tools | 2022 (MSVC v143) | https://visualstudio.microsoft.com/downloads/ |
| Qt | 6.5 LTS ou superior (recomendado 6.8) | https://www.qt.io/download-qt-installer |
| CMake | 3.25+ | https://cmake.org/download/ |
| Ninja | 1.11+ | já incluso no VS Build Tools |
| PowerShell | 5.1 ou pwsh 7+ | já no Windows |
| Inno Setup 6 *ou* NSIS 3.x | (para gerar instalador) | https://jrsoftware.org/isdl.php  ·  https://nsis.sourceforge.io |

## Como buildar (Windows)

```powershell
# Clone o repo, abra o PowerShell na pasta raiz
cd C:\Dev\DANTE-CLI-WIN

# Build Release com Qt auto-detectado (em C:\Qt\6.X.Y\msvc2022_64)
.\scripts\build.ps1

# Ou força um Qt específico
.\scripts\build.ps1 -QtRoot "C:\Qt\6.8.1\msvc2022_64"

# Executa
.\build\Release\bin\"Dante CLI.exe"

# Gerar instalador (Inno Setup ou NSIS)
.\scripts\make-installer.ps1
# → dist\installer\DanteCLI-Setup-1.0.28-x64.exe
```

## Como instalar (usuário final)

1. Baixe `DanteCLI-Setup-1.0.28-x64.exe` da seção Releases.
2. Execute. O instalador pede privilégios de administrador.
3. Aceite a licença MIT.
4. (Opcional) Marque "Adicionar 'dante-cli' ao PATH do sistema" durante a instalação.
5. Pronto — Dante CLI aparece no menu Iniciar.

> Compatível com Windows 10 (build 17763+) e Windows 11, em arquiteturas x64 e ARM64.

## Métricas de aceite (alvo da fase de release)

| Métrica | Target |
|---|---|
| RSS idle 4 abas | < 200 MB |
| CPU idle | < 1% |
| Startup frio (LCP) | < 500 ms |
| Frame time scroll 50k linhas | < 16 ms |
| Tamanho do instalador | < 80 MB |

## Princípios de arquitetura (resumo)

1. **Nunca bloqueie a UI thread.** Worker threads + signals/QueuedConnection.
2. **HANDLE sempre via RAII** (`WindowsHandle`).
3. **Job Object com KILL_ON_JOB_CLOSE** garante limpeza transitiva ao fechar o app.
4. **TerminalWidget é singleton por session-id** (`TerminalRegistry`), nunca destruído em re-render.
5. **Slots vazios de split = UUID nulo, sem PTY** (não auto-spawn).
6. **`Result<T,E>`** em hot paths, `throw` reservado pra falha excepcional.

## Convenções

- `namespace dante::{core,platform,persistence,terminal,ui}`
- Headers `.h` com `#pragma once`
- `QString` em interfaces de UI; `std::string` no Core (zero dep de Qt)
- Commits em imperativo presente: `add ConPtySession spawn flow`

## Licença

MIT. Veja [`LICENSE.txt`](LICENSE.txt).
