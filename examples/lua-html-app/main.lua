-- lua-html-app: A Coconut Milk app using the lua-html DSL for all views.
-- No raw .html files — every view is generated from Lua tables.
-- External CSS/JS served via coconut://assets/ for reliable loading.

-- Add lib/ to package.path so `require "lib.html"` works.
package.path = "lib/?.lua;" .. package.path

local html = require "lib.html"

-- ── Layout wrapper ───────────────────────────────────────────────────────

local function page(title, body)
  return tostring(html.Document{
    lang = "en",
    html.head{
      html.meta{ charset = "UTF-8" },
      html.meta{ name = "viewport", content = "width=device-width, initial-scale=1.0" },
      html.title{ title },
      html.link{ rel = "stylesheet", href = "coconut://assets/style.css" },
    },
    html.body{
      html.nav{
        html.a{ href = "coconut://home", "Home" },
        html.a{ href = "coconut://about", "About" },
        html.a{ href = "coconut://counter", "Counter" },
      },
      body,
      html.script{ src = "coconut://assets/app.js" },
    },
  })
end

-- ── Views ────────────────────────────────────────────────────────────────

function coconut.views()
  return {
    home = View.html(page("Home", html.fragment{
      html.h1{ "Welcome to lua-html-app" },
      html.div{ class = "card",
        html.p{
          "This entire page was generated from Lua using the ",
          html.a{ href = "https://riki.house/lua-html", "lua-html DSL" },
          ". There are no .html files anywhere in this project."
        },
        html.p{
          "HTML is constructed using Lua table constructors, auto-escaped, ",
          "and passed to ", html.code{ "View.html()" }, " as a string. ",
          "No build step, no template engine, no JSX."
        },
      },
      html.div{ class = "card",
        html.h2{ "Using the DSL" },
        html.p{ "In Lua, write:" },
        html.pre{ html.code{ [[html.div{ class = "card",
  html.h1{ "Hello" },
  html.p{ "This is ", html.b{ "bold" }, " text." },
}]] }},
        html.p{ "It produces the equivalent HTML without any raw strings." },
      },
    })),

    about = View.html(page("About", html.fragment{
      html.h1{ "About" },
      html.div{ class = "card",
        html.p{
          "This example demonstrates the lua-html library ",
          "(117 lines of pure Lua) for defining all views inline ",
          "without static HTML files."
        },
      },
      html.div{ class = "card",
        html.h2{ "How it works" },
        html.ol{
          html.li{ "Each view is built using ", html.code{ "html.div{...}" } },
          html.li{ "Rendered to a string via ", html.code{ "tostring(...)" } },
          html.li{ "Wrapped in ", html.code{ "View.html(...)" },
                    " and returned from ", html.code{ "coconut.views()" } },
        },
        html.p{
          "The library handles escaping, void elements, ",
          "underscore-to-dash conversion in attributes, ",
          "and document-type declaration."
        },
      },
    })),

    counter = View.html(page("Counter", html.fragment{
      html.h1{ "Counter" },
      html.div{ class = "card",
        html.p{ "Click below. The counter uses JS from coconut://assets/app.js." },
        html.div{ class = "counter", id = "counter", "0" },
        html.button{ id = "inc-btn", "Increment" },
      },
    })),
  }
end

-- ── Config ───────────────────────────────────────────────────────────────

function coconut.config(ctx)
  ctx
    :setWindowSize({ w = 720, h = 560 })
    :setTitle("lua-html-app")
    :setResizable(false)
    :setInitialView("home")
  return ctx
end

-- ── Navigation Events ────────────────────────────────────────────────────

function coconut.events(name, payload, ctx)
  if name == "navigate" then
    ctx:show(payload.view)
  end
end
