# Dante CLI — Entregáveis

## O que foi entregue

### Código (62 arquivos C++/H + CMake)

Projeto C++20/Qt 6 completo, dividido em **5 bibliotecas + 1 executável**:

| Lib | Responsabilidade | Depende de |
|---|---|---|
| `libdante_core` | Domínio puro, parser ANSI, `Result<T,E>` | stdlib + QtCore |
| `libdante_platform` | ConPTY, Job Object, RAII `HANDLE`, ShellResolver, ProcessStats | core |
| `libdante_persistence` | SQLite WAL + migrations + JSON config | core |
| `libdante_terminal` | Buffer 2D, scrollback, 10 color schemes, QPainter widget | core + platform |
| `libdante_ui` | MainWindow, TabBar, Sidebar dock, Toolbar, Settings | tudo acima |
| `dante-cli.exe` | Entry point + DI wiring + recursos embedados | tudo |

### Pipeline de build

- `scripts\build.ps1` — Configure + cmake build + `windeployqt` (auto-detecta Qt)
- `scripts\make-installer.ps1` — Build + empacota com Inno Setup ou NSIS
- `scripts\make-icon.py` — Gera `app.ico` multi-res a partir do design
- `scripts\make-wizard-images.py` — Gera BMPs do wizard Inno Setup
- `.github/workflows/windows-build.yml` — CI no GitHub Actions: build + artefatos + release on tag

### Instaladores

- `installer/inno/dante-cli.iss` — Inno Setup 6, com licença pt-BR/EN, PATH opcional
- `installer/nsis/dante-cli.nsi` — NSIS 3, com seleção de componentes, atalhos, Add/Remove Programs
- `dist/installer/DanteCLI-Setup-1.0.20-x64.exe` — **stub real** (PE32 self-extracting, 79 KB) — comprovação de que a sintaxe NSIS está válida

### Documentação

- `README.md` — Arquitetura, status, roadmap, métricas de aceite
- `INSTALL.md` — Guia completo de instalação Windows 10/11
- `LICENSE.txt` — MIT
- Comentários nos lugares onde explicam *porquê* (lições §3.4 do prompt original)

### Testes

- `tests/core/test_ansi_parser.cpp` — 6 testes (Qt Test): print ASCII, CR/LF, SGR indexed/truecolor, CSI cursor move, OSC title
- `tests/core/test_terminal_buffer.cpp` — smoke do tipo Cell

### Recursos

- `resources/icons/app.svg` — design vetorial
- `resources/icons/app.ico` — multi-res 16/32/48/64/128/256 (gerado por script)
- `resources/themes/dark.qss` — Tokyo Night QSS global
- `resources/translations/dante_pt_BR.ts` + `dante_en.ts` — i18n Qt Linguist

## Arquitetura aplicada (não negociáveis cumpridos)

- ✅ Dependência one-way: `app → ui → terminal → platform → core` (+ persistence)
- ✅ `WindowsHandle` RAII pra todo `HANDLE`
- ✅ `Job Object` com `KILL_ON_JOB_CLOSE` pra kill transitivo
- ✅ Parser ANSI tabela-driven (state machine), 256-color + truecolor + OSC 7 (cwd) + OSC 0/1/2 (title)
- ✅ `TerminalWidget` é singleton por `sessionId` via `TerminalRegistry` (lição §3.4.1)
- ✅ `Result<T,E>` em vez de exceções em hot paths
- ✅ UI thread nunca lê do pipe — `setOutputHandler` empurra via `QMetaObject::invokeMethod(QueuedConnection)`
- ✅ Slot vazio = `QUuid()` nulo, sem PTY (lição §3.4.4)
- ✅ `PROCESS_MEMORY_COUNTERS_EX::PrivateUsage` em vez de `WorkingSetSize` (lição §3.4.5)
- ✅ Bytes UTF-8 multibyte decodificados no parser
- ✅ Conhecimento de `cmd`, `powershell`, `pwsh`, `git-bash`, `wsl` via `ShellResolver`
- ✅ Interface `IPtyBackend` preparada pra implementação Linux/macOS futura
- ✅ SQLite `PRAGMA journal_mode=WAL; foreign_keys=ON; synchronous=NORMAL`
- ✅ JSON config debounced 500 ms
- ✅ i18n pt-BR + EN com override em settings

## Como compilar e instalar (resumo)

```powershell
# No Windows 10/11
git clone <repo> DANTE-CLI-WIN
cd DANTE-CLI-WIN
.\scripts\make-installer.ps1
# Pega dist\installer\DanteCLI-Setup-1.0.20-x64.exe e executa
```

Pré-requisitos: Visual Studio 2022 Build Tools + Qt 6.5+ (msvc2022_64) + Inno Setup ou NSIS.

## Limitações conhecidas (alvos das próximas fases)

- **Split workspace** (galeria de 25 presets + grid customizável + merges) → fase 4 do roadmap
- **Editor pane** com KSyntaxHighlighting → fase 7
- **Preview pane** (imagens/PDF/áudio) → fase 7
- **Voz com Groq Whisper** → fase 7
- **Update checker + banner** → fase 8
- **Code signing EV** + MSIX → fase 9
- **Glyph cache OpenGL** (opção B) se QPainter (opção A) não atingir 60 fps em 4K

## Não compilável neste ambiente (macOS)

Este Mac não possui MSVC nem Qt 6 prebuilt para Windows. O instalador `DanteCLI-Setup-1.0.20-x64.exe` em `dist/installer/` é um **stub PE32 válido** gerado por `makensis` para validar a sintaxe — quando o build for executado em um Windows com Qt 6 instalado (via `scripts\make-installer.ps1`), o NSIS empacotará todos os DLLs e o `Dante CLI.exe` real dentro do mesmo `.nsi`, produzindo o instalador final de ~50-70 MB.

O caminho mais rápido para um instalador final pronto:
1. Subir o repo no GitHub
2. Push em `main` (ou tag `vX.Y.Z`)
3. O workflow `.github/workflows/windows-build.yml` builda automaticamente e publica o instalador como artefato. Em tags, cria também uma GitHub Release.
