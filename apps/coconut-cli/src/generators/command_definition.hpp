#ifndef COMMAND_DEFINITION_HPP
#define COMMAND_DEFINITION_HPP

#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include "type_parser.hpp"
#include "utils.hpp"

namespace coconut::generator {

  enum class State {
    LineStart,         // beginning of a new line, decide what to do
    SkipLine,          // not a --- line, consume until \n
    ReadPrefix,        // saw ---, next char decides: '@' or ' '
    ReadTag,           // reading tag name: command/param/return/description
    ReadName,          // reading value after @command
    ReadText,          // reading free text (@description or continuation)
    ReadContinuation,  // "--- text" appended to current description
    ReadParamName,     // reading param identifier
    ReadParamType,     // reading param type { ... } (track brace depth)
    ReadReturnType,    // reading return type string
    Done               // hit a non-comment line (local function ...) → emit
  };

  enum class Tag { None, Command, Description, Param, Return, Thread };
  struct CommandDefinitionParameter {
    std::string name;
    std::string type;
    std::string description;  // optional
  };

  struct CommandDefinition {
    std::string                             name;
    std::string                             description;  // optional
    std::vector<CommandDefinitionParameter> parameters;
    std::string                             returnTypes;
    std::string                             thread;  // "main" or empty (default = background)
  };

  struct TagDefinition {
    Tag type = Tag::None;
  };

  struct ParseContext {
    Tag                            current_tag = Tag::None;
    std::string                    buf;  // current token being built
    CommandDefinition              current_command;
    CommandDefinitionParameter     current_param;
    std::vector<CommandDefinition> commands;
  };
  std::vector<CommandDefinition> commentsFsm(std::string code) {
    int start  = 0;
    int cursor = 0;

    ParseContext ctx;
    State        currentState = State::LineStart;
    while (cursor < code.length()) {
      cursor++;
      ctx.buf = code.substr(start, cursor - start);

      switch (currentState) {
        case State::LineStart:
          if (trim(ctx.buf) == "---") {
            currentState = State::ReadPrefix;
            if (code[cursor] == '@') {
              currentState = State::ReadTag;
              start        = cursor;
            }
            start = cursor;
          } else if (ctx.buf.length() >= 4 && code[cursor] != '\n' && code[cursor] != '\r') {
            // 4+ chars consumed — determine if this is a non-comment line.
            std::string trimmed = trim(ctx.buf);
            if (trimmed.length() >= 3 && trimmed.substr(0, 3) != "---") {
              // Non-comment line: finalize any pending command.
              if (!ctx.current_command.name.empty()) {
                ctx.commands.push_back(ctx.current_command);
                ctx.current_command = CommandDefinition{};
              }
              currentState = State::SkipLine;
            }
          }
          if (code[cursor] == '\n' || code[cursor] == '\r') {
            start = cursor;
          }
          break;
        case State::SkipLine:
          // We may enter SkipLine when `cursor` is already sitting on a newline.
          // The next loop iteration increments `cursor`, so check the previous
          // char.
          if (cursor > 0 && (code[cursor - 1] == '\n' || code[cursor - 1] == '\r')) {
            start        = cursor;
            currentState = State::LineStart;
          }
          break;
        case State::ReadPrefix:
          if (code[cursor] == '@') {
            currentState = State::ReadTag;
            start        = cursor;
          } else if (code[cursor] == '\n' || code[cursor] == '\r') {
            start        = cursor;
            currentState = State::LineStart;
          } else {
            // Continuation line: --- text without @tag -> append to description
            start        = cursor;
            currentState = State::ReadContinuation;
          }
          break;
        case State::ReadTag:
          if (isWhitespace(code[cursor])) {
            const std::string token = trim(ctx.buf);
            // When we switch states, skip the whitespace char currently under
            // `cursor` so the next state's buffer doesn't start with a leading
            // space/newline.
            const int nextStart = std::min(cursor + 1, (int)code.length());

            if (token == "@command") {
              // Push any previously accumulated command before starting a new one.
              if (!ctx.current_command.name.empty()) {
                ctx.commands.push_back(ctx.current_command);
                ctx.current_command = CommandDefinition{};
              } else {
                // No previous command — clear only fields that could have leaked
                // from non-command functions (e.g. @param on helpers like
                // is_image/text_type).  Keep the description which may have been
                // set by a preceding @description tag.
                ctx.current_command.name.clear();
                ctx.current_command.parameters.clear();
                ctx.current_command.returnTypes.clear();
              }
              ctx.current_tag = Tag::Command;
              start           = nextStart;
              currentState    = State::ReadName;
            } else if (token == "@description") {
              ctx.current_tag = Tag::Description;
              start           = nextStart;
              if (code[nextStart] == '\n' || code[nextStart] == '\r') {
                currentState = State::SkipLine;
              } else {
                currentState = State::ReadText;
              }
            } else if (token == "@param" || token == "param") {
              // Only accept @param when a command name is active.
              // Otherwise (e.g. helper functions without @command) we'd leak
              // parameters into the next command block.
              if (!ctx.current_command.name.empty()) {
                ctx.current_tag = Tag::Param;
                start           = nextStart;
                if (code[nextStart] == '\n' || code[nextStart] == '\r') {
                  // Empty param — skip
                  currentState = State::SkipLine;
                } else {
                  currentState = State::ReadParamName;
                }
              } else {
                start        = nextStart;
                currentState = State::SkipLine;
              }
            } else if (token == "@thread") {
              ctx.current_tag = Tag::Thread;
              start           = nextStart;
              if (code[nextStart] == '\n' || code[nextStart] == '\r') {
                // Empty thread specifier — skip
                currentState = State::SkipLine;
              } else {
                currentState = State::ReadText;
              }
            } else if (token == "@return") {
              // Only accept @return when a command name is active.
              if (!ctx.current_command.name.empty()) {
                ctx.current_tag = Tag::Return;
                start           = nextStart;
                if (code[nextStart] == '\n' || code[nextStart] == '\r') {
                  // Empty return type — skip
                  currentState = State::SkipLine;
                } else {
                  currentState = State::ReadReturnType;
                }
              } else {
                start        = nextStart;
                currentState = State::SkipLine;
              }
            } else {
              // Unknown tag — skip
              ctx.current_tag = Tag::None;
              start           = nextStart;
              currentState    = State::SkipLine;
            }
          }
          break;
        case State::ReadName:
          if (isWhitespace(code[cursor])) {
            ctx.current_command.name = trim(ctx.buf);
            currentState             = State::SkipLine;
            start                    = cursor;
          }
          break;
        case State::ReadText:
          if (code[cursor] == '\n' || code[cursor] == '\r') {
            if (!ctx.buf.empty()) {
              if (ctx.current_tag == Tag::Thread) {
                ctx.current_command.thread = trim(ctx.buf);
              } else {
                ctx.current_command.description = trim(ctx.buf);
              }
            }
            start        = cursor;
            currentState = State::SkipLine;
          }
          break;
        case State::ReadContinuation:
          if (code[cursor] == '\n' || code[cursor] == '\r') {
            if (!ctx.buf.empty()) {
              const std::string line = trim(ctx.buf);
              if (!line.empty()) {
                if (!ctx.current_command.description.empty()) {
                  ctx.current_command.description += "\n";
                }
                ctx.current_command.description += line;
              }
            }
            start        = cursor;
            currentState = State::SkipLine;
          }
          break;
        case State::ReadParamName:
          if (isWhitespace(code[cursor])) {
            ctx.current_param.name = trim(ctx.buf);
            if (code[cursor] == '\n' || code[cursor] == '\r') {
              // No type annotation — push param with empty type.
              ctx.current_command.parameters.push_back(ctx.current_param);
              ctx.current_param = CommandDefinitionParameter{};
              start             = cursor;
              currentState      = State::SkipLine;
            } else {
              // Skip the whitespace and read the raw type string.
              start        = std::min(cursor + 1, (int)code.length());
              currentState = State::ReadParamType;
            }
          }
          break;
        case State::ReadParamType:
          // Read the raw type annotation until EOL.
          if (code[cursor] == '\n' || code[cursor] == '\r') {
            ctx.current_param.type = trim(ctx.buf);
            ctx.current_command.parameters.push_back(ctx.current_param);
            ctx.current_param = CommandDefinitionParameter{};
            start             = cursor;
            currentState      = State::SkipLine;
          }
          break;
        case State::ReadReturnType:
          // Read the raw return type until EOL.
          if (code[cursor] == '\n' || code[cursor] == '\r') {
            ctx.current_command.returnTypes = trim(ctx.buf);
            start                           = cursor;
            currentState                    = State::SkipLine;
          }
          break;
        case State::Done:
          break;
      }
    }

    // Finalize any remaining command (e.g., no trailing code line).
    if (!ctx.current_command.name.empty()) {
      ctx.commands.push_back(ctx.current_command);
    }

    return ctx.commands;
  }
  /// Safely parse a LuaCATS type string, falling back to raw text on error.
  static std::string formatTypeOrPassthrough(
      const std::string& raw, std::string (*fmt)(const Type&)
  ) {
    if (raw.empty())
      return "any";
    try {
      Type parsed = parseType(raw);
      return fmt(parsed);
    } catch (const TypeParseError&) {
      return raw;  // fallback: use the raw string as-is
    }
  }

  /// Write the preamble of runtime function declarations that the generated
  /// JS wrappers depend on (__coconut_call, etc.).  These are ambient
  /// declarations so editors/LSP don't flag undeclared variables.
  static void writeRuntimeDeclarations(std::ostream& out) {
    out << "// Declares the global `coconut` object provided by the Coconut Milk bridge.\n";
    out << "// Full type definition is in scripts/coconut.d.ts.\n";
    out << "/// <reference path=\"../scripts/coconut.d.ts\" />\n";
    out << "\n";
  }

  std::string generateTSDefinition(std::vector<CommandDefinition> commandDefs) {
    std::stringstream generatedDTS;
    writeRuntimeDeclarations(generatedDTS);
    for (auto def : commandDefs) {
      // JSDoc string
      generatedDTS << "/**" << std::endl;
      generatedDTS << "@description " << def.description << std::endl;
      generatedDTS << "*/" << std::endl;
      generatedDTS << "declare function " << def.name << "(";
      for (auto param : def.parameters) {
        std::string tsType = formatTypeOrPassthrough(param.type, formatTypeTS);
        generatedDTS << param.name << ":" << tsType << ",";
      }
      generatedDTS << ") : Promise<[";
      {
        std::string ret = formatTypeOrPassthrough(def.returnTypes, formatTypeTS);
        generatedDTS << ret;
      }
      generatedDTS << "]>;" << std::endl;
    }
    return generatedDTS.str();
  }

  /// Generate a JavaScript wrapper with JSDoc type annotations.
  ///
  /// The output is plain JS that works without a build step.  Editors with
  /// JSDoc support (VS Code, etc.) provide full type hints from the
  /// annotations, so consumers don't need TypeScript or a bundler.
  std::string generateJSWrapper(std::vector<CommandDefinition> commandDefs) {
    std::stringstream out;
    out << "// Auto-generated command wrappers. Do not edit.\n";
    out << "// Uses coconut.call() for Lua command invocation.\n";
    out << "// Plain JS with JSDoc — no build step required.\n";
    out << "// @ts-check\n";
    out << "\n";
    // Wrap in IIFE so exported functions are accessible as regular script
    // (no ES module needed).  Sets window.* so app.js IIFE can call them.
    out << "(function () {\n";
    out << "  'use strict';\n";
    out << "\n";

    std::vector<std::string> funcNames;

    for (const auto& def : commandDefs) {
      funcNames.push_back(def.name);

      // JSDoc block comment
      out << "  /**\n";
      {
        std::string d    = def.description;
        size_t      pos  = 0;
        size_t      prev = 0;
        while ((pos = d.find('\n', prev)) != std::string::npos) {
          out << "   * " << d.substr(prev, pos - prev) << "\n";
          prev = pos + 1;
        }
        if (prev < d.length()) {
          out << "   * " << d.substr(prev) << "\n";
        }
      }
      // JSDoc @param entries with parsed types
      for (const auto& p : def.parameters) {
        if (p.name == "ctx")
          continue;
        std::string jsType = formatTypeOrPassthrough(p.type, formatTypeJS);
        std::string jsName = p.name;
        if (!jsName.empty() && jsName.back() == '?') {
          jsName.pop_back();
          out << "   * @param {" << jsType << "} [" << jsName << "]\n";
        } else {
          out << "   * @param {" << jsType << "} " << jsName << "\n";
        }
      }
      // JSDoc @returns — wrap in Promise since all Lua calls are async
      {
        std::string retType = formatTypeOrPassthrough(def.returnTypes, formatTypeJS);
        out << "   * @returns {Promise<" << retType << ">}\n";
      }
      out << "   */\n";

      // Function signature — plain JS, no 'export' keyword
      out << "  async function " << def.name << "(";
      bool                     first = true;
      std::vector<std::string> visibleParams;
      for (const auto& p : def.parameters) {
        if (p.name == "ctx")
          continue;
        if (!first)
          out << ", ";
        first              = false;
        std::string jsName = p.name;
        if (!jsName.empty() && jsName.back() == '?')
          jsName.pop_back();
        out << jsName;
        visibleParams.push_back(jsName);
      }
      out << ") {\n";

      // Build the payload object
      if (visibleParams.empty()) {
        out << "    return coconut.call(\"" << def.name << "\", {});\n";
      } else if (visibleParams.size() == 1) {
        out << "    return coconut.call(\"" << def.name << "\", " << visibleParams[0] << ");\n";
      } else {
        out << "    return coconut.call(\"" << def.name << "\", {";
        first = true;
        for (const auto& n : visibleParams) {
          if (!first)
            out << ", ";
          first = false;
          out << n;
        }
        out << "});\n";
      }
      out << "  }\n\n";
    }

    // Expose all functions on window for IIFE consumers (like app.js)
    if (!funcNames.empty()) {
      out << "  // Expose to window for non-module scripts\n";
      for (const auto& name : funcNames) {
        out << "  window." << name << " = " << name << ";\n";
      }
      out << "\n";

      // CommonJS / Node.js
      out << "  if (typeof module !== 'undefined' && module.exports) {\n";
      out << "    module.exports = { ";
      bool first = true;
      for (const auto& name : funcNames) {
        if (!first)
          out << ", ";
        first = false;
        out << name;
      }
      out << " };\n";
      out << "  }\n";
    }

    out << "})();\n";

    return out.str();
  }

  /// Generate a .g.lua wrapper (background thread, uses ctx:bind())
  static std::string generateLuaBgWrapper(
      const std::vector<CommandDefinition>& commands, const std::string& modulePath
  ) {
    std::stringstream out;
    out << "-- Generated by Coconut Milk build pipeline.\n";
    out << "-- Do not edit by hand.\n";
    out << "\n";
    out << "local impl = require(\"" << modulePath << "\")\n";
    out << "\n";
    out << "---@type fun(ctx: CoconutContext)\n";
    out << "local function register(ctx)\n";

    for (const auto& cmd : commands) {
      out << "  ctx:bind(\"" << cmd.name << "\", impl." << cmd.name << ")\n";
    }

    out << "  return ctx\n";
    out << "end\n";
    out << "\n";
    out << "return register\n";
    return out.str();
  }

  /// Generate a .g_mt.lua wrapper (main thread, uses ctx:bind_mt())
  static std::string generateLuaMtWrapper(
      const std::vector<CommandDefinition>& commands, const std::string& modulePath
  ) {
    std::stringstream out;
    out << "-- Generated by Coconut Milk build pipeline.\n";
    out << "-- Do not edit by hand.\n";
    out << "-- Main-thread variant (@thread main).\n";
    out << "\n";
    out << "local impl = require(\"" << modulePath << "\")\n";
    out << "\n";
    out << "---@type fun(ctx: CoconutContext)\n";
    out << "local function register(ctx)\n";

    for (const auto& cmd : commands) {
      out << "  ctx:bind_mt(\"" << cmd.name << "\", impl." << cmd.name << ")\n";
    }

    out << "  return ctx\n";
    out << "end\n";
    out << "\n";
    out << "return register\n";
    return out.str();
  }
}  // namespace coconut::generator

#endif
