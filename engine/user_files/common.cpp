// Copyright 2011 Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// * Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the distribution.
// * Neither the name of Google Inc. nor the names of its contributors
//   may be used to endorse or promote products derived from this software
//   without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "engine/user_files/common.hpp"

#include <lutok/exceptions.hpp>
#include <lutok/operations.hpp>
#include <lutok/stack_cleaner.hpp>

#include "utils/env.hpp"
#include "utils/format/macros.hpp"
#include "utils/fs/lua_module.hpp"
#include "utils/fs/path.hpp"
#include "utils/logging/lua_module.hpp"
#include "utils/logging/macros.hpp"

namespace fs = utils::fs;
namespace logging = utils::logging;
namespace user_files = engine::user_files;


/// Loads a user-provided file that follows any of the Kyua formats.
///
/// \param state The Lua state.
/// \param file The name of the file to process.
///
/// \return The syntax definition (format, version) of the file.  The caller
/// must validate the return value before accessing the Lua state.
///
/// \throw lutok::error If there is any problem processing the provided Lua
///     file or any of its dependent libraries.
user_files::syntax_def
user_files::do_user_file(lutok::state& state, const fs::path& file)
{
    lutok::stack_cleaner cleaner(state);
    init(state, file);
    lutok::do_file(state, file.str());
    return get_syntax(state);
}


/// Gets the syntax definition of an already loaded file.
///
/// \param state The Lua state.
///
/// \return The syntax definition (format, version) of the file.
///
/// \throw lutok::error If there is a problem querying the file syntax.
user_files::syntax_def
user_files::get_syntax(lutok::state& state)
{
    lutok::stack_cleaner cleaner(state);

    lutok::eval(state, "init.get_syntax()", 1);
    if (!state.is_table())
        throw lutok::error("init.get_syntax() is not a table");

    state.push_string("format");
    state.get_table(-2);
    state.push_string("version");
    state.get_table(-3);

    if (state.is_nil(-2) || state.is_nil(-1))
        throw lutok::error("Syntax not defined; must call syntax()");
    if (!state.is_string(-2))
        throw lutok::error("init.get_syntax().format is not a string");
    if (!state.is_number(-1))
        throw lutok::error("init.get_syntax().version is not an integer");

    const std::string format = state.to_string(-2);
    const int version = state.to_integer(-1);

    return std::make_pair(format, version);
}


/// Loads the init.lua module into a Lua state and initializes it.
///
/// The init.lua module provides the necessary boilerplate code to process user
/// files consumed by Kyua.  It must be imported into the environment before
/// processing a user file.
///
/// Use do_user_file() to execute a user file.  This function is exposed mostly
/// for testing purposes only.
///
/// \param state The Lua state.
/// \param file The name of the file to process.  The file is not actually
///     opened in this call; this name is only used to initialize internal
///     state.
///
/// \throw lutok::error If there is any problem processing the init.lua file
///     or initializing its internal state.
void
user_files::init(lutok::state& state, const fs::path& file)
{
    LI(F("Loading user file '%s'") % file);

    const fs::path luadir(utils::getenv_with_default(
        "KYUA_LUADIR", KYUA_LUADIR));

    lutok::stack_cleaner cleaner(state);

    state.open_base();
    state.open_string();
    state.open_table();
    fs::open_fs(state);
    logging::open_logging(state);

    lutok::do_file(state, (luadir / "init.lua").str(), 1);
    state.push_string("export");
    state.get_table();
    state.pcall(0, 0, 0);

    lutok::eval(state, "init.bootstrap");
    state.push_string(luadir.str());
    state.push_string(file.c_str());
    state.pcall(2, 0, 0);
}
