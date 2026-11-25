"""Check all default layouts and execute the native shortcut-priority code."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def native_function(source, signature):
    start = source.index(signature)
    depth = 0
    for offset in range(source.index('{', start), len(source)):
        depth += (source[offset] == '{') - (source[offset] == '}')
        if not depth:
            return source[start:offset + 1]
    raise AssertionError(signature)


def main():
    menu = (ROOT / 'src/ui/MainMenu.cpp').read_text(encoding='utf-8')
    for slot, key in ((1, 'Q'), (2, 'E')):
        defaults = re.findall(r'\{"Delve Ability ' + str(slot) + r'", "([^"]+)"', menu)
        assert defaults == [key] * 4, defaults
    assert 'Q / E ABILITIES | HUD BUTTONS' in menu
    assert 'bindings[binding] = input;' in menu
    assert 'if (j == 0 && delveShortcutVersion < 1) migrateDelveAbilityBindings(bindings);' in menu
    builds = (ROOT / 'src/delve_builds.cpp').read_text(encoding='utf-8')
    assert 'addSpell(' not in native_function(builds, 'void prepareAbilities(')
    ui = (ROOT / 'src/ui/GameUI.cpp').read_text(encoding='utf-8')
    assert 'position.x -= (2 - slot) * (maxWidth + 16);' in ui
    assert 'glyph->pos.y = iconPos.y + iconPos.h;' in ui
    source = (ROOT / 'src/input.cpp').read_text(encoding='utf-8')
    harness = r"""
#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <unordered_map>
constexpr int SINGLE = 0;
int multiplayer = SINGLE;
int clientnum = 0;
bool intro = false;
namespace Raid { bool enabled = false; bool active() { return enabled; } }
struct Player { bool shootmode = true; };
Player localPlayers[4];
Player* players[] = {&localPlayers[0], &localPlayers[1], &localPlayers[2], &localPlayers[3]};
struct Input {
    struct Binding { std::string input; bool binary = true; bool consumed = false; };
    static Input inputs[4];
    int player = 0;
    bool disabled = false;
    std::unordered_map<std::string, Binding> bindings;
    const char* binding(const char* action) const {
        auto found = bindings.find(action);
        return found == bindings.end() ? "" : found->second.input.c_str();
    }
    bool binaryToggle(const char* action) const;
    bool consumeBinaryToggle(const char* action);
};
Input Input::inputs[4];
"""
    for signature in ('static bool delveAbilityOverridesFollower(', 'bool Input::binaryToggle(',
                      'bool Input::consumeBinaryToggle('):
        harness += native_function(source, signature) + '\n'
    harness += native_function(menu, 'static void migrateDelveAbilityBindings(') + '\n'
    harness += r"""
int main() {
    std::unordered_map<std::string, std::string> saved = {{"Delve Ability 1", "F7"}, {"Delve Ability 2", "F8"}};
    migrateDelveAbilityBindings(saved);
    assert(saved["Delve Ability 1"] == "Q" && saved["Delve Ability 2"] == "E");
    saved["Delve Ability 1"] = "R"; saved["Delve Ability 2"] = "";
    migrateDelveAbilityBindings(saved);
    assert(saved["Delve Ability 1"] == "R" && saved["Delve Ability 2"].empty());
    saved["Delve Ability 1"] = "F7"; saved["Delve Ability 2"] = "Z";
    migrateDelveAbilityBindings(saved);
    assert(saved["Delve Ability 1"] == "F7" && saved["Delve Ability 2"] == "Z");
    auto& input = Input::inputs[0];
    input.bindings = {{"Command NPC", {"Q"}}, {"Cycle NPCs", {"E"}},
        {"Delve Ability 1", {"Q"}}, {"Delve Ability 2", {"E"}}, {"Cast Spell", {"F"}}};
    assert(input.binaryToggle("Command NPC"));
    assert(input.binaryToggle("Cycle NPCs"));
    Raid::enabled = true;
    for (const char* action : {"Command NPC", "Cycle NPCs"}) {
        assert(!input.binaryToggle(action));
        assert(!input.consumeBinaryToggle(action));
        assert(!input.bindings[action].consumed);
    }
    assert(input.consumeBinaryToggle("Delve Ability 1"));
    assert(input.consumeBinaryToggle("Delve Ability 2"));
    assert(!input.consumeBinaryToggle("Delve Ability 1"));
    assert(!input.binaryToggle("Command NPC"));
    assert(input.binaryToggle("Cast Spell"));
    players[0]->shootmode = false;
    assert(input.binaryToggle("Command NPC"));
    assert(input.binaryToggle("Cycle NPCs"));
    players[0]->shootmode = true;
    intro = true;
    assert(input.binaryToggle("Command NPC"));
    intro = false;
    players[0] = nullptr;
    assert(input.binaryToggle("Command NPC"));
    players[0] = &localPlayers[0];
    input.bindings["Delve Ability 1"].input = "F7";
    input.bindings["Delve Ability 2"].input = "F8";
    assert(input.binaryToggle("Command NPC"));
    assert(input.binaryToggle("Cycle NPCs"));
    input.bindings["Delve Ability 1"].input = "E";
    assert(input.binaryToggle("Command NPC"));
    assert(!input.binaryToggle("Cycle NPCs"));
    input.bindings["Cycle NPCs"].input = "DpadY-";
    assert(input.binaryToggle("Cycle NPCs"));
    input.bindings["Command NPC"].input = "";
    input.bindings["Delve Ability 2"].input = "";
    assert(!delveAbilityOverridesFollower(input, "Command NPC"));
    input.bindings["Command NPC"].input = "E";
    multiplayer = 1;
    clientnum = 1;
    players[0]->shootmode = false;
    Input::inputs[1].player = 1;
    assert(!Input::inputs[1].binaryToggle("Command NPC"));
    assert(!Input::inputs[1].consumeBinaryToggle("Command NPC"));
    players[1]->shootmode = false;
    assert(Input::inputs[1].binaryToggle("Command NPC"));
    players[0]->shootmode = true;
    players[1]->shootmode = true;
    multiplayer = SINGLE;
    input.disabled = true;
    assert(!input.binaryToggle("Cycle NPCs"));
    assert(!input.consumeBinaryToggle("Cycle NPCs"));
    assert(!input.binaryToggle("Missing"));
    std::cout << "PASS: native ability priority, normal campaigns, menus, remaps, controllers, routing and disabled input\n";
}
"""
    with tempfile.TemporaryDirectory(prefix='delve-hotkeys-') as temporary:
        directory = Path(temporary)
        source_path = directory / 'hotkeys.cpp'
        source_path.write_text(harness, encoding='utf-8')
        linux_path = '/mnt/' + directory.drive[0].lower() + directory.as_posix()[2:]
        executable = linux_path + '/hotkeys'
        subprocess.run(['wsl.exe', '-d', 'Ubuntu-24.04', '--exec', 'g++', '-std=c++17',
                        '-Wall', '-Wextra', linux_path + '/hotkeys.cpp', '-o', executable], check=True)
        subprocess.run(['wsl.exe', '-d', 'Ubuntu-24.04', '--exec', executable], check=True)
    print('PASS: Q/E defaults in all four layouts; legacy defaults migrate; custom and unbound controls survive')


if __name__ == '__main__':
    main()
