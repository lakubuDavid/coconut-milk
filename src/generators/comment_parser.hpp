#ifndef COMMENT_PARSER_HPP
#define COMMENT_PARSER_HPP

#include "utils.hpp"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace coconut::generator {

enum class State {
  LineStart,        // beginning of a new line, decide what to do
  SkipLine,         // not a --- line, consume until \n
  ReadPrefix,       // saw ---, next char decides: '@' or ' '
  ReadTag,          // reading tag name: command/param/return/description
  ReadName,         // reading value after @command
  ReadText,         // reading free text (@description or continuation)
  ReadContinuation, // "--- text" appended to current description
  ReadParamName,    // reading param identifier
  ReadParamType,    // reading param type { ... } (track brace depth)
  ReadReturnType,   // reading return type string
  Done              // hit a non-comment line (local function ...) → emit
};

enum class Tag { None, Command, Description, Param, Return };
struct CommandDefinitionParameter {
  std::string name;
  std::string type;
  std::string description; // optional
};

struct CommandDefinition {
  std::string name;
  std::string description; // optional
  std::vector<CommandDefinitionParameter> parameters;
  std::string returnTypes;
};

struct TagDefinition {
  Tag type = Tag::None;
};

struct ParseContext {
  Tag current_tag = Tag::None;
  std::string buf; // current token being built
  CommandDefinition current_command;
  CommandDefinitionParameter current_param;
  std::vector<CommandDefinition> commands;
};
std::vector<CommandDefinition> commentsFsm(std::string code) {
  int start = 0;
  int cursor = 0;

  ParseContext ctx;
  State currentState = State::LineStart;
  while (cursor < code.length()) {
    cursor++;
    ctx.buf = code.substr(start, cursor - start);

    switch (currentState) {
    case State::LineStart:
      if (trim(ctx.buf) == "---") {
        std::cout << "->prefix" << std::endl;
        currentState = State::ReadPrefix;
        if (code[cursor] == '@') {
          std::cout << "->tag" << std::endl;
          currentState = State::ReadTag;
          start = cursor;
        }
        start = cursor;
      } else if (ctx.buf.length() >= 4 && code[cursor] != '\n' &&
                 code[cursor] != '\r') {
        // 4+ chars consumed — determine if this is a non-comment line.
        std::string trimmed = trim(ctx.buf);
        if (trimmed.length() >= 3 && trimmed.substr(0, 3) != "---") {
          // Non-comment line: finalize any pending command.
          if (!ctx.current_command.name.empty()) {
            std::cout << "[done] finalized command='"
                      << ctx.current_command.name
                      << "' params=" << ctx.current_command.parameters.size()
                      << std::endl;
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
      if (cursor > 0 &&
          (code[cursor - 1] == '\n' || code[cursor - 1] == '\r')) {
        start = cursor;
        currentState = State::LineStart;
      }
      break;
    case State::ReadPrefix:
      if (code[cursor] == '@') {
        std::cout << "->tag" << std::endl;
        currentState = State::ReadTag;
        start = cursor;
      } else if (code[cursor] == '\n' || code[cursor] == '\r') {
        start = cursor;
        std::cout << "->line start : " << ctx.buf << std::endl;
        currentState = State::LineStart;
      } else {
        // Continuation line: --- text without @tag -> append to description
        start = cursor;
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
          ctx.current_tag = Tag::Command;
          start = nextStart;
          currentState = State::ReadName;
          std::cout << "[state:tag] command = " << token << std::endl;
        } else if (token == "@description") {
          ctx.current_tag = Tag::Description;
          start = nextStart;
          std::cout << "[state:tag] desc = " << token << std::endl;
          if (code[nextStart] == '\n' || code[nextStart] == '\r') {
            currentState = State::SkipLine;
          } else {
            currentState = State::ReadText;
          }
        } else if (token == "@param" || token == "param") {
          ctx.current_tag = Tag::Param;
          start = nextStart;
          std::cout << "[state:tag] param = " << token << std::endl;
          if (code[nextStart] == '\n' || code[nextStart] == '\r') {
            // Empty param — skip
            currentState = State::SkipLine;
          } else {
            currentState = State::ReadParamName;
          }
        } else if (token == "@return") {
          ctx.current_tag = Tag::Return;
          start = nextStart;
          std::cout << "[state:tag] return = " << token << std::endl;
          if (code[nextStart] == '\n' || code[nextStart] == '\r') {
            // Empty return type — skip
            currentState = State::SkipLine;
          } else {
            currentState = State::ReadReturnType;
          }
        } else {
          // Unknown tag
          ctx.current_tag = Tag::None;
          start = nextStart;
          currentState = State::SkipLine;
          std::cout << " unknown tag" << ctx.buf << std::endl;
        }
      }
      break;
    case State::ReadName:
      if (isWhitespace(code[cursor])) {
        ctx.current_command.name = trim(ctx.buf);
        std::cout << "[state:name] " << ctx.current_command.name << std::endl;
        currentState = State::SkipLine;
        start = cursor;
      }
      break;
    case State::ReadText:
      if (code[cursor] == '\n' || code[cursor] == '\r') {
        if (!ctx.buf.empty()) {
          ctx.current_command.description = trim(ctx.buf);
          std::cout << "[state:text] '" << ctx.current_command.description
                    << "'" << std::endl;
        }
        start = cursor;
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
            std::cout << "[state:cont] '" << line << "'" << std::endl;
          }
        }
        start = cursor;
        currentState = State::SkipLine;
      }
      break;
    case State::ReadParamName:
      if (isWhitespace(code[cursor])) {
        ctx.current_param.name = trim(ctx.buf);
        std::cout << "[state:param_name] '" << ctx.current_param.name << "'"
                  << std::endl;
        if (code[cursor] == '\n' || code[cursor] == '\r') {
          // No type annotation — push param with empty type.
          ctx.current_command.parameters.push_back(ctx.current_param);
          ctx.current_param = CommandDefinitionParameter{};
          start = cursor;
          currentState = State::SkipLine;
        } else {
          // Skip the whitespace and read the raw type string.
          start = std::min(cursor + 1, (int)code.length());
          currentState = State::ReadParamType;
        }
      }
      break;
    case State::ReadParamType:
      // Read the raw type annotation until EOL.
      if (code[cursor] == '\n' || code[cursor] == '\r') {
        ctx.current_param.type = trim(ctx.buf);
        std::cout << "[state:param_type] '" << ctx.current_param.type << "'"
                  << std::endl;
        ctx.current_command.parameters.push_back(ctx.current_param);
        ctx.current_param = CommandDefinitionParameter{};
        start = cursor;
        currentState = State::SkipLine;
      }
      break;
    case State::ReadReturnType:
      // Read the raw return type until EOL.
      if (code[cursor] == '\n' || code[cursor] == '\r') {
        ctx.current_command.returnTypes = trim(ctx.buf);
        std::cout << "[state:return_type] '" << ctx.current_command.returnTypes
                  << "'" << std::endl;
        start = cursor;
        currentState = State::SkipLine;
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
std::string generateTSDefinition(std::vector<CommandDefinition> commandDefs) {
  std::stringstream generatedDTS;
  for (auto def : commandDefs) {
    // JSDoc string
    generatedDTS << "/**" << std::endl;
    generatedDTS << "@description " << def.description << std::endl;
    generatedDTS << "*/" << std::endl;
    generatedDTS << "declare function " << def.name << "(";
    for (auto param : def.parameters) {
      generatedDTS << param.name << ":" << param.type << ",";
    }
    generatedDTS << ") : Promise<[";
    generatedDTS << def.returnTypes << "]>;" << std::endl;
  }
  return generatedDTS.str();
}
std::string generateTSWrapper(std::vector<CommandDefinition> commandDefs) {
  std::stringstream out;
  out << "// Auto-generated command wrappers. Do not edit.\n";
  out << "// Uses __coconut_call for Lua command invocation.\n\n";

  for (const auto& def : commandDefs) {
    // JSDoc comment — handle multiline descriptions
    out << "/**\n";
    {
      std::string d = def.description;
      size_t pos = 0;
      size_t prev = 0;
      while ((pos = d.find('\n', prev)) != std::string::npos) {
        out << " * " << d.substr(prev, pos - prev) << "\n";
        prev = pos + 1;
      }
      if (prev < d.length()) {
        out << " * " << d.substr(prev) << "\n";
      } else if (prev == 0 && d.empty()) {
        // no description
      }
    }
    for (const auto& p : def.parameters) {
      out << " * @param " << p.name << " " << p.type << "\n";
    }
    if (!def.returnTypes.empty()) {
      out << " * @returns " << def.returnTypes << "\n";
    }
    out << " */\n";

    // Build param list for the TS function signature
    // Skip 'ctx' — it's injected by the Lua runtime, not from JS.
    // Collect non-ctx params into a single payload object.
    out << "export async function " << def.name << "(";
    bool first = true;
    for (const auto& p : def.parameters) {
      if (p.name == "ctx") continue;
      if (!first) out << ", ";
      first = false;
      out << p.name << ": " << p.type;
    }
    out << "): Promise<" << def.returnTypes << "> {\n";

    // Build the payload object
    out << "  return __coconut_call(\"" << def.name << "\", {";
    first = true;
    for (const auto& p : def.parameters) {
      if (p.name == "ctx") continue;
      if (!first) out << ", ";
      first = false;
      out << p.name;
    }
    out << "});\n";
    out << "}\n\n";
  }

  return out.str();
}

std::string generateLuaWrapper(const std::vector<CommandDefinition>& commands,
                                const std::string& modulePath) {
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
} // namespace coconut::generator

#endif
