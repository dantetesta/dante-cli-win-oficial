# Dante CLI — Guia de instalação (Windows 10 e 11)

## Opção A — Instalador pronto (recomendado)

1. Baixe `DanteCLI-Setup-1.0.22-x64.exe`.
2. Clique duas vezes pra executar.
3. Aceite o aviso de UAC (precisa de admin pra escrever em `Program Files`).
4. Siga o assistente: Welcome → License → Componentes → Diretório → Instalar.
5. Componentes opcionais:
   - ☑ Atalho no menu Iniciar
   - ☐ Atalho na área de trabalho
   - ☐ Adicionar `dante-cli` ao PATH do sistema

### Requisitos mínimos

- **SO**: Windows 10 versão 1809 (build 17763) ou Windows 11
- **Arquitetura**: x64 ou ARM64
- **RAM**: 200 MB
- **Disco**: 150 MB
- **Privilégios**: Admin para instalar em `Program Files`. Pode optar por instalação por usuário.

## Opção B — Compilar do código-fonte

Útil pra desenvolvedores ou pra debugar.

### 1. Pré-requisitos

```powershell
# Instale via Chocolatey
choco install visualstudio2022buildtools cmake ninja git -y

# Instale Qt 6.8 via Online Installer (https://www.qt.io/download-qt-installer)
# Marque: MSVC 2022 64-bit, Qt 5 Compatibility Module, Qt Multimedia, Qt SQL

# Opcional pro instalador:
choco install innosetup nsis -y
```

### 2. Build

```powershell
git clone <repo-url> DANTE-CLI-WIN
cd DANTE-CLI-WIN
.\scripts\build.ps1
.\build\Release\bin\"Dante CLI.exe"
```

### 3. Gerar instalador

```powershell
.\scripts\make-installer.ps1
# → dist\installer\DanteCLI-Setup-1.0.22-x64.exe
```

## Desinstalação

- **Configurações → Apps → Dante CLI → Desinstalar**, OU
- **Painel de Controle → Programas e Recursos → Dante CLI**, OU
- Execute `"%ProgramFiles%\Dante CLI\uninstall.exe"`.

Os dados do usuário em `%APPDATA%\Dante CLI\` (banco SQLite, config, logs) são preservados na desinstalação. Apague manualmente se desejar.

## Pastas usadas pelo app

| Caminho | Conteúdo |
|---|---|
| `%ProgramFiles%\Dante CLI\` | Binários (read-only) |
| `%APPDATA%\Dante CLI\dante.db` | Banco SQLite (tabs, favoritos, snippets, credenciais) |
| `%APPDATA%\Dante CLI\config.json` | Configurações leves (tema, fonte, API keys) |
| `%APPDATA%\Dante CLI\logs\dante-YYYYMMDD.log` | Logs diários |

## Solução de problemas

**"VCRUNTIME140.dll não encontrada"** — instale o [Visual C++ Redistributable 2015-2022](https://aka.ms/vs/17/release/vc_redist.x64.exe).

**Smart Screen bloqueia a execução** — o EXE não está assinado por certificado EV. Clique em "Mais informações" → "Executar mesmo assim". A assinatura digital virá em uma release futura.

**O terminal não abre** — Windows 10 anterior ao build 17763 não tem ConPTY. Atualize o sistema.

**Quero rodar como portátil (sem instalar)** — após buildar, copie `build\Release\bin\` pra qualquer pasta e execute o `.exe`. O app salva config em `%APPDATA%\` por padrão (não é totalmente portátil).
