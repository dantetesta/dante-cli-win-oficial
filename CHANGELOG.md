# Dante CLI — Changelog

Formato baseado em [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/).
Versionamento [SemVer](https://semver.org/lang/pt-BR/).

## [1.0.22] — 2026-05-20

### Adicionado
- **Botão ⇲ no header de cada célula** (split mode) abre popup pra redimensionar a célula.
- **Popup "Tamanho desta célula"**:
  - Mostra dimensão atual em "% da área" (ex: `60×100 %`).
  - **4 setas pra Expandir** (verde): ↑ ↓ ← → — cada click aumenta 10 pp.
  - **4 setas pra Reduzir** (laranja): ↑ ↓ ← → — diminui 10 pp.
  - **"Voltar ao preset"** (vermelho) zera a customização — célula volta exato ao preset original.
  - Click fora ou Esc fecha.
- `g_app.customCells[]` armazena overrides por célula (zerados quando layout muda). `resolve_cell()` retorna custom se setado, senão o preset.

## [1.0.21] — 2026-05-20

### Adicionado
- **Zoom 90%** da célula ativa em modo split:
  - Botão **⤢** (ou **⤡** quando ativo) no canto direito do header de cada célula, ao lado do chip de RAM.
  - **Ctrl+Shift+Z** ativa/desativa zoom da célula ativa.
  - Quando zoom está ativo, só a célula selecionada é renderizada, ocupando 90% da área do terminal (5 % de padding em torno), com fundo dim atrás. As outras células continuam vivas no background — sessions não são reiniciados.
  - **Esc** sai do zoom.
  - Mudar de layout via `▦ Layout` zera o zoom automaticamente (evita zoom em célula que sumiu).

## [1.0.20] — 2026-05-20

### Adicionado
- **Monitor de Recursos** — popover não-modal igual ao do macOS.
  - **Chip clicável** no canto direito da TabBar mostrando `📊 [Mem] · [CPU%]`. A borda do chip muda de cor: verde até 40%, laranja até 80%, vermelho acima.
  - Click no chip abre popover de 540×720 com:
    - **CPU total** (App + soma das shells) em destaque grande, colorido por nível.
    - **Sparkline** de 60 amostras (1 amostra/s) do CPU% histórico — linha verde sobre canvas dark.
    - **Memória total** (`PrivateUsage` somado).
    - **Barra App vs Shells** com duas cores (azul = app, magenta = shells), proporcional ao uso real.
    - **Legenda** com bytes humanizados (`M`, `GB`).
    - **Grid de cards 2 colunas** com cada shell ativo: emoji, nome (com `…` se longo), `mem · cpu%`.
  - Click fora / Esc / outro chip click fecha.
  - Auto-refresh a cada 1 s (mesmo timer que faz o relógio do REC).
- Computação `cpu_pct_for_pid` via `GetProcessTimes` com cache LRU por PID + normalização por número de cores.
- `mem_for_pid` via `GetProcessMemoryInfo` (`PrivateUsage`).
- Função `fmt_bytes` humaniza B/KB/M/GB automaticamente.

## [1.0.19] — 2026-05-20

### Adicionado
- **Header colorido em cada terminal** — a área do terminal agora começa com uma barra de 38 px na cor da aba (mesma `kTabColors[colorIdx]`). Conecta visualmente a chip da aba com o conteúdo abaixo, igual ao macOS:
  - **Hambúrguer** (3 traços) à esquerda → reservado pro menu por-célula (próxima versão).
  - **Emoji** da aba (clicável via menu de contexto).
  - **Título em bold** com cor de contraste automática (texto preto sobre claro / branco sobre escuro via cálculo de luminância).
  - **cwd em cinza translúcido** com `DT_END_ELLIPSIS` quando não cabe.
  - **Chip de RAM** à direita (`PrivateUsage` do PID da sessão) — atualiza junto com o repaint.
- **Emoji picker visual** — grid 8×5 com 40 emojis devs-friendly (📁 📂 ⚡ 🔥 🚀 🐍 🦀 🐶 🐱 etc). Click aplica. Acessível pelo menu de contexto da aba (botão direito → "Emoji...").
- Item "Remover emoji" no menu de contexto (só aparece se a aba tem emoji).
- O strip lateral de 3 px (que existia antes) foi substituído pelo header — fica mais limpo e mais informativo.

### Detalhes técnicos
- `contrast_text_color()` calcula luminância sRGB pra decidir texto preto/branco — funciona com toda a paleta de 12 cores.
- `mix_color()` mistura RGB pra calcular `cwdDim` (texto secundário) e o chip de RAM (sutil escurecido sobre o accent).
- Nova fonte `hFontUIBold` (Segoe UI Bold, -14px) criada no `WM_CREATE`.

## [1.0.18] — 2026-05-20

### Adicionado
- **Galeria visual de layouts** (igual à do macOS) substitui o popup menu de texto. Janela de 960×720 com:
  - Header com título "Galeria de layouts" e dica de Esc.
  - 4 **pills de categoria** (Simples / IDE / Dashboard / Operações) — a categoria do layout atual é pré-selecionada.
  - **Grid de cards 3 colunas**, cada um com:
    - **Thumbnail real do layout** desenhado dinamicamente a partir das células `{x,y,w,h}` em coordenadas % — cada cell vira um retângulo azul arredondado num canvas dark, com gaps de 2 px entre elas.
    - Nome do preset em branco.
    - "N painéis" em cinza.
  - Card do layout atualmente ativo tem borda azul.
  - Hover muda o card pra uma cor mais clara.
- **Navegação por teclado**: ←/→ navegam entre cards, Enter aplica, Esc fecha.
- Botão **▦ Layout** no toolbar agora abre a galeria. `Shift+click` no botão ainda abre o popup menu de texto (mais rápido para usuários power).

### Detalhes técnicos
- `gallery_paint_thumbnail()` renderiza as células do preset proporcionalmente em qualquer tamanho de canvas — mesmo código que valida visualmente os 21 presets sem precisar de PNGs/imagens.
- Janela usa `WS_POPUP | WS_CAPTION | WS_SYSMENU` com double-buffering para evitar flicker.
- `WM_DESTROY` segue o padrão `PostThreadMessageW(WM_NULL)` (não `PostQuitMessage`) — não mata o app inteiro.

## [1.0.17] — 2026-05-20

### Corrigido
- **Relógio "REC mm:ss" travado em 00:00 durante gravação**. O label era recalculado dentro do `WM_PAINT`, mas nada disparava `InvalidateRect` enquanto o microfone gravava (nenhum byte chega do ConPTY pra acordar o repaint). Agora o timer de 1 s força `InvalidateRect` quando `g_voice.recording == TRUE`.
- Timer de status acelerado de 2 s → **1 s** (granularidade necessária pro relógio do mic; o RAM no rodapé também atualiza mais rápido como bônus).
- Cap de gravação de 60 s passou a ser verdadeiramente aplicado — se o usuário esquecer o mic ligado, ele para e faz upload sozinho aos 60 s.

## [1.0.16] — 2026-05-20

### Estabilidade

Rodada inteira focada em **eliminar crashes aleatórios**:

- **Use-after-free no pipeline ConPTY → UI**: a thread que lê bytes do shell mandava o `Session*` na mensagem cross-thread. Se o usuário fechasse a aba antes da mensagem ser despachada, esse ponteiro virava lixo. Trocado por `tabId` (int estável). Em paralelo, busca de tab passou a usar id em vez de comparar ponteiros.
- **Race em `session_destroy`**: wait do reader thread aumentado de 1s → 5s; se mesmo assim a thread não morrer, a `Session*` **não é mais liberada** (leak intencional de algumas centenas de bytes em vez do risco de UAF).
- **`splitSlots` ficavam apontando pra índices inválidos** depois de `close_tab`. Agora qualquer slot que aponte para o índice removido vira `-1`, e qualquer slot apontando pra um índice acima é decrementado.
- **`activeTab` mal-shiftado** depois de close: se você fechava a aba 1 e tinha foco na 2, o foco ficava na 2 antiga (que agora é a 3 antiga). Corrigido.
- **`PostMessageW` no reader thread** agora libera o buffer se a janela já não existe.
- **WM_QUERYENDSESSION/WM_ENDSESSION** capturados — agora `persist_save()` roda no logoff/shutdown do Windows.
- **WM_DESTROY** zera `g_app.tabs[i]` depois do free pra qualquer dangling read não encontrar lixo.
- `schedule_persist()` agora dispara também em `open_new_tab` e em `close_tab` — antes só salvava em mudanças explícitas.

### Adicionado
- **Padding interno do terminal**: agora `20px` horizontal e `14px` vertical (era `8/4px`). Texto não fica mais grudado na borda da janela ou na chip da aba.

## [1.0.15] — 2026-05-20

### Corrigido
- **Crash crítico ao clicar Salvar nas Configurações**: o diálogo modal chamava `PostQuitMessage(0)` no `WM_DESTROY`, que injeta `WM_QUIT` na fila do thread, e o **loop principal** do app pegava o `WM_QUIT` em seguida e encerrava o app inteiro. Trocado por `PostThreadMessageW(WM_NULL)` que só desbloqueia o GetMessage modal sem afetar o loop principal.
- **Chave Groq sumia** após o "crash": a save era debounced (timer 1.5s). Como o app morria antes do timer disparar, nada era escrito em disco. Agora o OK do Settings chama `persist_save()` **síncrono na hora** — chave salva antes do diálogo fechar.
- Mesmo fix aplicado ao `PromptWndProc` (Adicionar favorito/snippet/credencial) — qualquer cancelar/OK também não vai mais matar o app.

## [1.0.14] — 2026-05-20

### Internal
- Preparação do fix do PostQuitMessage (não shipou). Substituída em 1.0.15.

## [1.0.13] — 2026-05-20

### Corrigido
- **Backspace puro estava apagando uma palavra inteira** em vez de 1 caractere. Causa: na 1.0.10 troquei o byte de `0x7F` (DEL) para `0x08` (BS) achando que cmd.exe não aceitava DEL — na verdade o `0x08` é interpretado como `BackwardKillWord` por algumas configurações do PSReadLine no PowerShell 7, daí a palavra inteira sumir. Voltei pra `0x7F` que é o que xterm/Windows Terminal/PSReadLine esperam universalmente para "apagar 1 char".

### Adicionado
- **Ctrl+Backspace** → envia `\x17` (ETB / Ctrl+W) → PSReadLine mapeia para `BackwardKillWord` (apaga a palavra anterior).
- **Alt+Backspace** → envia `\x1B\x7F` (ESC + DEL) → variante com word boundary diferente em PSReadLine/bash/zsh.
- Captura de `WM_SYSKEYDOWN` no message loop. Antes, qualquer combinação com Alt era engolida pelo DefWindowProc (que tocava o bell do sistema). Agora só Alt+Backspace é interceptado; Alt+F4 e os accelerators de menu continuam funcionando.
- `WM_SYSCHAR` para Alt+Backspace é descartado para evitar o "bing" do sistema.
- Atalhos novos adicionados ao cheatsheet (Ctrl+/).

## [1.0.12] — 2026-05-20

### Corrigido
- **Bug crítico do parser ANSI** — caracteres `25h`, `25l`, `1049h`, etc. apareciam impressos no terminal. Causa: o parser CSI não tratava o **DEC Private Mode prefix** (`?`, `>`, `=`, `<`). Quando PowerShell enviava `\x1B[?25h` (mostrar cursor), o `?` botava o estado em GROUND, então `25h` virava texto literal. Agora o range inteiro de parameter bytes (0x30–0x3F) é capturado e sequências com prefixo `?` são absorvidas silenciosamente (cursor visibility, alt-screen, bracketed paste, focus reporting).
- Suporte CSI ampliado: `X` (erase chars), `P` (delete chars), `@` (insert blanks), `s`/`u` (save/restore cursor), `h`/`l` (SM/RM sem prefixo).

### Adicionado
- **21 presets de split** (era 5), em **4 categorias** com submenus:
  - **Simples** (6): Sem split · Lado a lado · Empilhados · 3 colunas · 3 empilhados · Quartos 2×2
  - **Dashboard** (5): Header + 3 colunas · Cinema 1+4 · Mosaico 2+1 · Grid 3×2 · Grid 3×3
  - **IDE** (7): Editor + Terminal · Editor + Sidebar · Editor + 2 painéis direita · Editor + Terminal + Logs · Sidebar + Main + Inspector · Master/Detail · Foco esquerdo 80%
  - **Operações** (3): Main + 4 painéis lateral · Quadrante + faixas · Editor + 4 logs
- Engine de split refeito com células em **coordenadas %** (não mais grids regulares cols×rows). Suporta layouts irregulares como o "Mosaico 2+1" (2 quadrantes esquerda + 1 painel altura cheia direita) e "Cinema 1+4" (header largo + 4 painéis em baixo).
- Cada item do menu mostra quantos painéis o preset tem.
- Suporte a até 9 células simultâneas (Grid 3×3).

## [1.0.11] — 2026-05-20

### Adicionado
- `installer/manifest_windows.json` — exemplo pronto pra subir num endpoint HTTPS, com schema documentado em `_schema_doc`.

## [1.0.10] — 2026-05-20

### Corrigido
- **Bug crítico**: cada tecla `Enter`, `Tab`, `Backspace` ou `Esc` estava sendo enviada **duas vezes** ao ConPTY. Causa: `WM_KEYDOWN` (em `on_keypress`) já mandava o byte; em seguida o Windows entregava `WM_CHAR` com o mesmo control char (0x0D/0x09/0x08/0x1B) e `on_char` mandava de novo. Resultado: prompt aparecia 2x, Backspace deletava 2 caracteres, Tab pulava 2 paradas, caracteres tipo `²` aparecendo do nada.
- `on_char` agora descarta TODOS os C0 controls (< 0x20) e o DEL (0x7F) — esses são responsabilidade de `on_keypress`.
- Backspace agora envia `0x08` (BS) em vez de `0x7F` (DEL). `cmd.exe` e `PowerShell` esperam BS; DEL era literalmente impresso no buffer.

## [1.0.9] — 2026-05-20

### Corrigido
- `bump-version.sh` agora também atualiza `APP_VERSION_W` em `dante_app.c` (estava ficando preso em 1.0.2 no rodapé da janela).

## [1.0.8] — 2026-05-20

### Adicionado
- About modal (F1) com lista de features e licença.
- Update check assíncrono no startup (`raw.githubusercontent.com/dantetesta/dante-cli/main/updates/win.json`); avisa só se versão remota > local.
- Atalho `Ctrl+D` duplica aba ativa.
- Cheatsheet atualizado com todos os novos atalhos.

## [1.0.7] — 2026-05-20

### Adicionado
- **Split workspace**: 5 layouts (Single, 1×2, 2×1, 2×2, 1×3). Botão `▦ Layout` no toolbar abre popup menu.
- Foco por célula: clique numa célula vira a "active cell", teclas vão pra ela.
- Borda colorida na célula com foco (cor da chip do tab).
- Slots vazios mostram placeholder "⊕ slot vazio".

## [1.0.6] — 2026-05-20

### Adicionado
- **Voz via Groq Whisper**: clique no botão Voz inicia gravação (waveIn 16kHz mono PCM 16-bit), clique novamente para → empacota WAV + POST multipart/form-data → `whisper-large-v3-turbo` → injeta transcrição no terminal ativo.
- Botão muda visual (`REC 00:12`) durante gravação, `Enviando…` durante upload.
- Validação: rejeita áudio < 0.4s; exige Groq API key.
- Idioma configurável (pt/en) nos settings.

## [1.0.5] — 2026-05-20

### Adicionado
- **Explicar via Groq Chat**: pega últimas 25 linhas do terminal ativo, manda como user prompt + system prompt em PT-BR para `llama-3.3-70b-versatile`, mostra resposta em modal próprio com botão "Copiar tudo".
- WinHTTP foundation (handler genérico de POST JSON + multipart).
- JSON encoder/decoder mínimo (escape, parse string com `\uXXXX`).
- API call em worker thread, resultado via `PostMessage` na UI thread.

## [1.0.4] — 2026-05-20

### Adicionado
- **19 color schemes** selecionáveis em runtime: Tokyo Night, Dracula, Nord, One Dark, Solarized Dark/Light, Gruvbox Dark/Light, Monokai, Catppuccin Mocha/Latte, GitHub Dark/Light, Material Dark, Night Owl, Synthwave '84, Cobalt, Palenight, Apple Classic.
- **Settings dialog** (Ctrl+, ou botão ⚙): tema, tamanho de fonte (recria a fonte mono no fly), scrollback, Groq API key (password field), idioma de voz.
- Config persistida em `state.json` (scheme id, fontPx, scrollback, groqApiKey, voiceLang).

## [1.0.3] — 2026-05-20

### Adicionado
- Persistência JSON completa em `%APPDATA%\Dante CLI\state.json` (debounce 1.5s, flush no exit).
- Sidebar funcional: ⊕ Adicionar abre wizards de 2-4 passos para Favoritos, Snippets, Credenciais.
- Click em favorito → `cd <path>`; em snippet → injeta comando; em credencial → injeta bloco comentado.
- Menu de contexto na tab (botão direito): renomear, duplicar, fixar, **12 cores**, nova aba (pwsh/cmd), fechar, fechar outras.
- Duplo-clique na tab renomeia.
- Drag-reorder de tabs.
- Modal cheatsheet (Ctrl+/) com AlphaBlend backdrop.
- Atalhos globais: Ctrl+T, Ctrl+W, Ctrl+1-9, Ctrl+Tab, Esc.

## [1.0.2] — 2026-05-20

### Adicionado
- App de fato funcional substituindo o placeholder bootstrap.
- 1639 LOC C puro Win32: ConPTY + parser ANSI + GDI render + multi-tab + sidebar dock + AI toolbar + status bar.
- Layout fiel ao SwiftUI: sidebar 240px / tabbar / terminal / toolbar / status.
- ConPTY real com Job Object kill-on-close.
- Parser ANSI (CSI cursor/erase/SGR + OSC 0/1/2/7).
- 10 color schemes iniciais (substituídos por 19 na 1.0.4).
- AI launchers funcionais (Claude/Gemini/Codex).

## [1.0.1] — 2026-05-20

### Adicionado
- Bootstrap launcher Win32 nativo cross-compilado (`installer/bootstrap/`) — garante que o atalho criado pelo instalador funcione desde a primeira instalação, mesmo antes do build Qt 6 completo.
- Novo ícone DanteCLI (mascote hacker + accent verde) substituindo o placeholder vetorial. ICO multi-resolução 16/24/32/48/64/96/128/192/256 px.
- `scripts/bump-version.sh` — incremento atômico de versão em todos os arquivos que carregam número de versão (cmake, .iss, .nsi, .c, .rc, .manifest, .md).
- `scripts/rebuild-installer.sh` — recompilação one-shot do bootstrap + instalador NSIS.
- `scripts/import-icon.py` — converte PNG do mascote em `app.ico` multi-res.
- BMPs do wizard Inno Setup com o mascote.

### Corrigido
- "Atalho Não Encontrado" após instalar: `dist/Release/` agora contém um `Dante CLI.exe` real (PE32+ x86-64, 283 KB, standalone) em vez de só um README.
- Dupla entrega de bytes do PTY (handler + signal `PtyEvents::output`). Agora há um único caminho: worker thread → `setOutputHandler` → `QMetaObject::invokeMethod(QueuedConnection)` → UI thread.
- `DANTE_VERSION_STRING` indefinido em `MainWindow` (substituído por `QApplication::applicationVersion()`).
- `dynamic_cast` morto removido de `TerminalWidget::attachBackend`.
- Includes faltantes (`QTimer`, `QStyle`, `QApplication`) adicionados.
- `TabBar.cpp`: setters de property redundantes consolidados.

## [1.0.0] — 2026-05-20

### Adicionado
- Scaffolding completo C++20/Qt 6 com 5 bibliotecas modulares + executável (62 arquivos source).
- `libdante_core`: domain (Tab, Session, Favorite, Snippet, Credential, AIProvider, SplitWorkspace), parser ANSI VT100/VT220 tabela-driven (256-color + truecolor + OSC 7/0/1/2), `Result<T,E>`, Uuid, Logger.
- `libdante_platform`: `ConPtyBackend` com Job Object `KILL_ON_JOB_CLOSE`, `WindowsHandle` RAII, `ShellResolver` (cmd/PowerShell/pwsh/Git Bash/WSL), `ProcessStats` (PrivateUsage).
- `libdante_persistence`: SQLite WAL + migrations versionadas, JSON config debounced 500 ms, `SqliteTabRepository`.
- `libdante_terminal`: `TerminalBuffer` 2D + `Scrollback` ring (50k linhas), 10 color schemes, `TerminalWidget` QPainter, `TerminalRegistry` singleton por session-id.
- `libdante_ui`: `MainWindow`, `TabBar`, `SidebarDock` (4 modos), `ToolbarWidget` com AI launchers + stats live, `SettingsDialog`.
- Pipeline de build: `scripts/build.ps1`, `scripts/make-installer.ps1` (Inno Setup + NSIS), `.github/workflows/windows-build.yml`.
- Instalador NSIS + Inno Setup com licença pt-BR/EN, atalhos opcionais, PATH opcional, Add/Remove Programs.
- Testes Qt Test do parser ANSI (6 casos).
- i18n pt-BR + EN (Qt Linguist .ts).
- Documentação: README, INSTALL, LICENSE (MIT), ENTREGAVEIS.
