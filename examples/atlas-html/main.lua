-- atlas-html: Atlas Tool built entirely with the lua-html DSL
package.path = "lib/?.lua;" .. package.path
local html = require "lib.html"

-- ============================================================
--  Inline helpers (plain tables — no html.Element calls inside)
-- ============================================================
local function row(label, child)
  return { class = "settings-row", html.label{ label }, child }
end

local function rh(id, dir)
  return { id = id, class = "resize-handle-" .. dir }
end

-- ============================================================
--  Atlas Packer page (plain table)
-- ============================================================
local function page_atlas()
  return {
    id = "atlasPackerPage",
    class = "tool-page active",
    { class = "layout",
      -- left sidebar
      { id = "atlasLeftSidebar", class = "sidebar",
        { class = "sidebar-section", id = "atlasSettingsSection",
          html.h2{ "Settings" },
          row("Columns",    html.input{ type = "number", id = "colsInput",       value = "8",  min = "1", max = "32" }),
          row("Padding",    html.input{ type = "number", id = "paddingInput",     value = "2",  min = "0", max = "32" }),
          row("Fit mode",   html.select{ id = "fitMode",
            html.option{ value = "contain",  "contain" },
            html.option{ value = "cover",    "cover"    },
            html.option{ value = "stretch", "stretch"  },
          }),
          row("Keep original size", html.input{ type = "checkbox", id = "keepOriginalSize" }),
          row("Transparent BG",   html.input{ type = "checkbox", id = "transparentBg",    checked = "" }),
          row("BG color",          html.input{ type = "color",   id = "bgColor",         value = "#ffffff", disabled = "" }),
          row("Export name",        html.input{ type = "text",    id = "exportName",      value = "atlas" }),
          html.button{ class = "pack-btn", id = "packBtn", disabled = "", "Pack" },
        },
        rh("atlasSettingsHandle", "v"),
        { class = "sidebar-section", id = "atlasExportSection",
          html.h2{ "Export" },
          { style = "display:flex;gap:4px;flex-wrap:wrap",
            html.button{ class = "output-btn", id = "downloadPngBtn",  disabled = "", "PNG"  },
            html.button{ class = "output-btn", id = "downloadMetaBtn", disabled = "", "JSON" },
            html.button{ class = "output-btn", id = "downloadAllBtn",  disabled = "", "ZIP"  },
          },
        },
      },
      rh("atlasLeftHandle", "h"),
      -- centre
      { id = "atlasMain", class = "main",
        { class = "tabs",
          html.button{ class = "tab active", ["data-tab"] = "image", "Image"     },
          html.button{ class = "tab",         ["data-tab"] = "meta",  "Metadata"  },
        },
        { id = "atlasInfo", style = "display:none;flex-shrink:0;padding:5px 16px;border-bottom:1px solid #ccc;background:#fafafa;font-size:10px;color:#666;font-family:JetBrains Mono,monospace;align-items:center",
          html.span{ id = "infoCanvas" },
          html.span{ style = "margin:0 8px;color:#ccc", "|" },
          html.span{ id = "infoCell" },
          html.button{ class = "output-btn", id = "resetAtlasZoom", style = "margin-left:auto;padding:3px 8px;font-size:10px", "Reset view" },
        },
        { id = "imagePane", class = "preview-area empty",
          html.span{ "add sprites & pack" },
          html.canvas{ id = "atlasCanvas", style = "display:none", class = "zoomable-canvas" },
        },
        { id = "metaPane", class = "meta-pane", style = "display:none",
          html.div{ class = "empty-state", "no atlas data" },
          html.pre{ id = "metaOutput", style = "display:none", class = "line-numbers match-braces", ["data-download-link"] = "", ["data-download-link-label"] = "Download",
            html.code{ class = "language-json", id = "metaCode" },
          },
        },
      },
      rh("atlasRightHandle", "h"),
      -- right sidebar
      { id = "atlasRightSidebar", class = "sidebar right",
        { class = "sidebar-section", id = "atlasImportSection",
          html.h2{ "Import" },
          { class = "import-row",
            html.button{ class = "import-btn", id = "importBtn",    "+ Add sprites" },
            html.button{ class = "import-btn", id = "importZipBtn", "Import ZIP"    },
          },
          html.input{ type = "file", id = "fileInput", accept = "image/*", multiple = "", style = "display:none" },
          html.input{ type = "file", id = "zipInput",  accept = ".zip",             style = "display:none" },
        },
        rh("atlasImportHandle", "v"),
        { class = "sidebar-section grow", id = "atlasSpritesSection",
          html.h2{ "Sprites ", html.span{ class = "count-badge", id = "spriteCount", "0" } },
          html.div{ class = "sprite-list", id = "spriteList" },
        },
      },
    },
  }
end

-- ============================================================
--  Tileset Resizer page (plain table)
-- ============================================================
local function page_tileset()
  return {
    id = "tilesetResizerPage", class = "tool-page",
    { class = "layout",
      { id = "tilesetLeftSidebar", class = "sidebar",
        { class = "sidebar-section", id = "tilesetSourceSection",
          html.h2{ "Source" },
          { class = "settings-row",
            html.label{ "Tileset" },
            html.button{ class = "import-btn", id = "tilesetImportBtn", style = "width:auto;flex:0", "Choose file" },
          },
          html.input{ type = "file", id = "tilesetFileInput", accept = "image/*", style = "display:none" },
          row("Tile width",  html.input{ type = "number", id = "origTileW",   value = "16", min = "1" }),
          row("Tile height", html.input{ type = "number", id = "origTileH",   value = "16", min = "1" }),
          row("Spacing",     html.input{ type = "number", id = "origSpacing", value = "0",  min = "0" }),
          row("Margin",      html.input{ type = "number", id = "origMargin",  value = "0",  min = "0" }),
        },
        rh("tilesetSourceHandle", "v"),
        { class = "sidebar-section", id = "tilesetTargetSection",
          html.h2{ "Target" },
          row("Tile width",  html.input{ type = "number", id = "targetTileW",   value = "32", min = "1" }),
          row("Tile height", html.input{ type = "number", id = "targetTileH",   value = "32", min = "1" }),
          row("Spacing",     html.input{ type = "number", id = "targetSpacing", value = "0",  min = "0" }),
          row("Margin",      html.input{ type = "number", id = "targetMargin",  value = "0",  min = "0" }),
          row("Scale mode", html.select{ id = "scaleMode",
            html.option{ value = "nearest", "nearest-neighbor" },
            html.option{ value = "smooth",  "smooth"           },
          }),
          html.button{ class = "pack-btn", id = "processTilesetBtn", disabled = "", "Process" },
        },
        rh("tilesetTargetHandle", "v"),
        { class = "sidebar-section", id = "tilesetExportSection",
          html.h2{ "Export" },
          row("File name", html.input{ type = "text", id = "tilesetExportName", value = "tileset" }),
          html.button{ class = "output-btn", id = "downloadTilesetBtn", disabled = "", style = "width:100%", "Download PNG" },
        },
      },
      rh("tilesetLeftHandle", "h"),
      { id = "tilesetMain", class = "main",
        { class = "tileset-preview-layout",
          { class = "preview-half",
            { class = "preview-header", style = "display:flex;align-items:center;justify-content:space-between",
              html.span{ "Original" },
              html.button{ class = "output-btn", id = "resetOriginalZoom", style = "padding:3px 8px;font-size:10px", "Reset" },
            },
            { id = "originalWrap", class = "preview-canvas-wrap",
              html.canvas{ id = "tilesetOriginalCanvas", class = "zoomable-canvas" },
              html.span{ class = "empty-label", id = "tilesetEmptyLabel", "no tileset loaded" },
            },
            html.div{ class = "preview-meta", id = "originalMeta" },
          },
          rh("tilesetPreviewHandle", "h"),
          { class = "preview-half",
            { class = "preview-header", style = "display:flex;align-items:center;justify-content:space-between",
              html.span{ "Resized" },
              html.button{ class = "output-btn", id = "resetResizedZoom", style = "padding:3px 8px;font-size:10px", "Reset" },
            },
            { id = "resizedWrap", class = "preview-canvas-wrap",
              html.canvas{ id = "tilesetResizedCanvas", class = "zoomable-canvas" },
              html.span{ class = "empty-label", id = "resizedEmptyLabel", "process to preview" },
            },
            html.div{ class = "preview-meta", id = "resizedMeta" },
          },
        },
      },
    },
  }
end

-- ============================================================
--  Main view — wrapped in html.Document at the end
-- ============================================================
function coconut.views()
  return {
    app = View.html(tostring(html.Document{
      lang = "en",
      html.head{
        html.meta{ charset = "UTF-8" },
        html.meta{ name = "viewport", content = "width=device-width, initial-scale=1.0" },
        html.title{ "Atlas Tool" },
        html.link{ rel = "preconnect", href = "https://fonts.googleapis.com" },
        html.link{ rel = "preconnect", href = "https://fonts.gstatic.com" },
        html.link{ href = "https://fonts.googleapis.com/css2?family=JetBrains+Mono:wght@400;600&display=swap", rel = "stylesheet" },
        html.link{ href = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/themes/prism.min.css", rel = "stylesheet" },
        html.link{ href = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/line-numbers/prism-line-numbers.min.css", rel = "stylesheet" },
        html.link{ href = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/line-highlight/prism-line-highlight.min.css", rel = "stylesheet" },
        html.link{ href = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/match-braces/prism-match-braces.min.css", rel = "stylesheet" },
        html.link{ href = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/toolbar/prism-toolbar.min.css", rel = "stylesheet" },
        html.link{ rel = "stylesheet", href = "coconut://assets/app.css" },
        html.script{ defer = "", src = "https://cdn.jsdelivr.net/npm/alpinejs@3.14.8/dist/cdn.min.js" },
        html.style{ "[un-cloak]{display:none}" },
      },
      html.body{
        -- Project manager
        html.div{ ["x-data"] = "projectManager()",
          html.header{
            html.h1{ "atlas-tool" },
            html.button{ class = "header-btn", ["@click"] = "newProject()",        "New"     },
            html.button{ class = "header-btn", ["@click"] = "saveCurrent()",      "Save"    },
            html.button{ class = "header-btn", ["@click"] = "showSave = true",       "Save As" },
            html.button{ class = "header-btn", ["@click"] = "refreshList(); showLoad = true", "Load" },
            html.span{ class = "header-name", ["x-text"] = "currentName()" },
          },
          -- Save dialog
          html.template{ ["x-if"] = "showSave",
            html.div{ class = "overlay", ["@click.self"] = "showSave = false",
              html.div{ class = "dialog",
                html.div{ class = "dialog-header",
                  html.span{ "Save project" },
                  html.button{ ["@click"] = "showSave = false", style = "background:none;border:none;cursor:pointer;font-size:14px;color:#999", "&times;" },
                },
                html.div{ class = "dialog-body",
                  html.input{ type = "text", ["x-model"] = "saveName", ["@keydown.enter"] = "doSave()", placeholder = "project name" },
                },
                html.div{ class = "dialog-footer",
                  html.button{ class = "output-btn", ["@click"] = "showSave = false", "Cancel" },
                  html.button{ class = "pack-btn", style = "width:auto;margin:0", ["@click"] = "doSave()", "Save" },
                },
              },
            },
          },
          -- Load dialog
          html.template{ ["x-if"] = "showLoad",
            html.div{ class = "overlay", ["@click.self"] = "showLoad = false",
              html.div{ class = "dialog",
                html.div{ class = "dialog-header",
                  html.span{ "Load project" },
                  html.button{ ["@click"] = "showLoad = false", style = "background:none;border:none;cursor:pointer;font-size:14px;color:#999", "&times;" },
                },
                html.div{ class = "dialog-body",
                  html.template{ ["x-if"] = "list.length == 0",
                    html.p{ style = "color:#aaa;font-size:11px;text-align:center;padding:16px", "no saved projects" },
                  },
                  html.div{ class = "project-list",
                    html.template{ ["x-for"] = "(p, i) in list", [":key"] = "p.name",
                      html.div{ class = "project-row",
                        html.span{ class = "name", ["@click"] = "doLoad(p.name)",
                          html.span{ ["x-text"] = "p.name" },
                        },
                        html.span{ class = "date", ["x-text"] = "p.date" },
                        html.span{ class = "del", ["@click.stop"] = "doDelete(i, p.name)", "&times;" },
                      },
                    },
                  },
                },
                html.div{ class = "dialog-footer",
                  html.button{ class = "output-btn", ["@click"] = "showLoad = false", "Close" },
                },
              },
            },
          },
        },
        -- App body
        html.div{ class = "app-body",
          html.nav{ class = "app-nav",
            html.a{ class = "nav-item active", href = "coconut://app",
              html.span{ class = "nav-icon",
                html.img{ src = "coconut://assets/lib/icons/package.svg", width = "20", height = "20", alt = "Atlas Packer" },
              },
              html.span{ class = "nav-label", "Atlas", html.br(), "Packer" },
            },
            html.a{ class = "nav-item", href = "coconut://tileset",
              html.span{ class = "nav-icon",
                html.img{ src = "coconut://assets/lib/icons/maximize.svg", width = "20", height = "20", alt = "Tileset Resizer" },
              },
              html.span{ class = "nav-label", "Tileset", html.br(), "Resizer" },
            },
          },
          html.div{ class = "app-content",
            html.div(page_atlas()),
            html.div(page_tileset()),
          },
        },
        html.div{ class = "toast", id = "toast" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/jszip@3.10.1/dist/jszip.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/prism.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/components/prism-json.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/line-numbers/prism-line-numbers.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/line-highlight/prism-line-highlight.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/match-braces/prism-match-braces.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/toolbar/prism-toolbar.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/copy-to-clipboard/prism-copy-to-clipboard.min.js" },
        html.script{ src = "https://cdn.jsdelivr.net/npm/prismjs@1.29.0/plugins/download-button/prism-download-button.min.js" },
        html.script{ src = "coconut://assets/app.js" },
      },
    })),
  }
end

function coconut.config(ctx)
  return ctx
    :setWindowSize({ w = 1280, h = 760 })
    :setMinimumWindowSize({ w = 900, h = 600 })
    :setTitle("Atlas Tool")
    :setInitialView("app")
end

function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end
