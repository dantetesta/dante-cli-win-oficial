-- Dante CLI initial schema (migration 001)
-- All tables use TEXT for UUIDs (UUID v4 string form, no braces).

CREATE TABLE IF NOT EXISTS schema_version (
    version INTEGER PRIMARY KEY
);

CREATE TABLE IF NOT EXISTS tabs (
    id              TEXT PRIMARY KEY,
    title           TEXT NOT NULL,
    color_hex       TEXT NOT NULL,
    emoji           TEXT,
    pinned          INTEGER NOT NULL DEFAULT 0,
    kind            INTEGER NOT NULL DEFAULT 0,
    cwd             TEXT,
    initial_command TEXT,
    shell_profile   TEXT,
    pane_tree_json  TEXT,
    created_at      TEXT NOT NULL,
    sort_order      INTEGER NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS idx_tabs_sort ON tabs(sort_order);

CREATE TABLE IF NOT EXISTS sessions (
    id              TEXT PRIMARY KEY,
    tab_id          TEXT NOT NULL REFERENCES tabs(id) ON DELETE CASCADE,
    cwd             TEXT,
    shell_profile   TEXT NOT NULL,
    initial_command TEXT,
    started_at      TEXT NOT NULL,
    exited_at       TEXT,
    exit_code       INTEGER
);

CREATE TABLE IF NOT EXISTS splits (
    id           TEXT PRIMARY KEY,
    tab_ids_json TEXT NOT NULL,
    layout_cols  INTEGER NOT NULL,
    layout_rows  INTEGER NOT NULL,
    spans_json   TEXT,
    active       INTEGER NOT NULL DEFAULT 0,
    created_at   TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS favorites (
    id              TEXT PRIMARY KEY,
    name            TEXT NOT NULL,
    path            TEXT NOT NULL,
    tags_json       TEXT,
    color_hex       TEXT,
    emoji           TEXT,
    initial_command TEXT,
    created_at      TEXT NOT NULL,
    sort_order      INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS snippets (
    id         TEXT PRIMARY KEY,
    name       TEXT NOT NULL,
    command    TEXT NOT NULL,
    tags_json  TEXT,
    emoji      TEXT,
    created_at TEXT NOT NULL,
    sort_order INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS credentials (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    kind        INTEGER NOT NULL,
    fields_json TEXT NOT NULL,
    notes       TEXT,
    tags_json   TEXT,
    emoji       TEXT,
    created_at  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS ai_providers (
    id        TEXT PRIMARY KEY,
    name      TEXT NOT NULL,
    command   TEXT NOT NULL,
    icon      TEXT,
    color_hex TEXT,
    shortcut  TEXT
);

CREATE TABLE IF NOT EXISTS layout_templates (
    id          TEXT PRIMARY KEY,
    name        TEXT NOT NULL,
    emoji       TEXT,
    layout_cols INTEGER NOT NULL,
    layout_rows INTEGER NOT NULL,
    spans_json  TEXT,
    created_at  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS recent_emojis (
    emoji   TEXT PRIMARY KEY,
    used_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_recent_emojis ON recent_emojis(used_at DESC);

-- Seed default AI providers if table is empty
INSERT INTO ai_providers (id, name, command, icon, color_hex, shortcut)
SELECT '11111111-1111-4111-8111-111111111111', 'Claude', 'claude', ':/icons/ai-claude.svg', '#D97706', 'Ctrl+Shift+C'
WHERE NOT EXISTS (SELECT 1 FROM ai_providers);

INSERT INTO ai_providers (id, name, command, icon, color_hex, shortcut)
SELECT '22222222-2222-4222-8222-222222222222', 'Gemini', 'gemini', ':/icons/ai-gemini.svg', '#1F77FF', 'Ctrl+Shift+G'
WHERE NOT EXISTS (SELECT 1 FROM ai_providers WHERE name = 'Gemini');

INSERT INTO ai_providers (id, name, command, icon, color_hex, shortcut)
SELECT '33333333-3333-4333-8333-333333333333', 'Codex', 'codex', ':/icons/ai-codex.svg', '#16A34A', NULL
WHERE NOT EXISTS (SELECT 1 FROM ai_providers WHERE name = 'Codex');
