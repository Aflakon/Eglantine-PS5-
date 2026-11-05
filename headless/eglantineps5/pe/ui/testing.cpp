// EglantinePS5 - Launcher: Settings > Diagnostics > Testing switches. The emulator's switches
// (EDEN_NX2_...) as choices, saved to config/env.txt (test_switches.h). The core reads them once,
// as the app starts, so applying them closes the app after a confirmation.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "pe/ui/launcher.hpp"

#include <algorithm>
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace pe::ui
{

using audio::Cue;

namespace
{

// Placed as a game's own settings are (game_options.cpp): four rows show over what the
// highlighted switch does, and the list scrolls.
constexpr Rect kDialog{550.0f, 180.0f, 820.0f, 720.0f};
constexpr float kRowsTop = 334.0f;
constexpr float kRowPitch = 96.0f;
constexpr float kRowHeight = 94.0f;
constexpr int kRowsShown = 4;
constexpr Rect kWindow{592.0f, kRowsTop, 736.0f, kRowPitch * (kRowsShown - 1) + kRowHeight};
constexpr float kHints = 848.0f;
// How long "closes now" shows before the app closes.
constexpr float kClosingSeconds = 2.5f;

// A choice: its name and the value of each of the switch's variables (nullptr: left unset).
struct Choice
{
    const char *name;
    std::array<const char *, 2> values;
};
// A switch: its name, what it does, the variables it sets (the second may be none) and its
// choices, the core's default first.
struct Switch
{
    const char *name;
    const char *about;
    std::array<const char *, 2> variables;
    std::array<Choice, 4> choices;
    int count;
};

constexpr Switch kSwitches[] = {
    {TR("Upscaling networks"),
     TR("Upscaling with a neural network, by default where it is known to work (Ocarina of Time, "
        "Donkey Kong Bananza). Off: a plain upscale, much faster and softer."),
     {"EDEN_NX2_TENSOR", nullptr},
     {{{TR("Default"), {nullptr, nullptr}}, {TR("Off"), {"0", nullptr}}, {TR("Every game"), {"1", nullptr}}}},
     3},
    {TR("Task and mesh draws"),
     TR("Off by default in Mario Kart World, where they hang the GPU. Off skips them in every game; "
        "On draws them in Mario Kart World too."),
     {"EDEN_NX2_NO_MESH", "EDEN_NX2_MKW_MESH"},
     {{{TR("Default"), {nullptr, nullptr}}, {TR("Off"), {"1", nullptr}}, {TR("On"), {nullptr, "1"}}}},
     3},
    {TR("Split GPU thread"),
     TR("Runs the GPU's commands on two threads. Off uses one: slower, and a test when a game freezes "
        "or crashes in the GPU."),
     {"EDEN_NX2_SPLIT", nullptr},
     {{{TR("Default"), {nullptr, nullptr}}, {TR("Off"), {"0", nullptr}}, {TR("Macros on the second thread"), {"1", nullptr}}}},
     3},
    {TR("Texture memory limit"),
     TR("Where the texture cache starts freeing memory. A lower limit helps a game that runs out of "
        "memory, and it can stutter more."),
     {"EDEN_NX2_TC_CRITICAL_MIB", "EDEN_NX2_TC_EXPECTED_MIB"},
     {{{TR("Default"), {nullptr, nullptr}}, {"4 GB", {"4096", "3328"}}, {"3 GB", {"3072", "2304"}}, {"2 GB", {"2048", "1280"}}}},
     4},
    {TR("GPU thread profile"),
     TR("Writes to the log, every 10 seconds, where the GPU thread spends its time. A little slower."),
     {"EDEN_NX2_GPU_PROFILE", nullptr},
     {{{TR("Off"), {nullptr, nullptr}}, {TR("On"), {"1", nullptr}}}},
     2},
    {TR("Fence batching"),
     TR("Groups the GPU's fences. Off makes them one at a time: slower, for comparisons."),
     {"EDEN_NX2_FENCE_BATCH", nullptr},
     {{{TR("On"), {nullptr, nullptr}}, {TR("Off"), {"0", nullptr}}}},
     2},
    {TR("Native matrix lowering"),
     TR("How neural networks are translated for the PS5's GPU. Off uses the older, larger code."),
     {"EDEN_NX2_HMMA_NATIVE", nullptr},
     {{{TR("On"), {nullptr, nullptr}}, {TR("Off"), {"0", nullptr}}}},
     2},
    {TR("GPU-only memory"),
     TR("By default it gets pages as they are written. Whole backs it all at once: more memory, and a "
        "test for wrong colours."),
     {"EDEN_NX2_GPU_ONLY_LAZY", nullptr},
     {{{TR("Default"), {nullptr, nullptr}}, {TR("Whole"), {"0", nullptr}}}},
     2},
};
constexpr int kSwitchCount = static_cast<int>(std::size(kSwitches));

const std::string *file_value(const std::vector<std::pair<std::string, std::string>> &file, const char *variable)
{
    for (auto it = file.rbegin(); it != file.rend(); ++it) // the last line wins, as when the app reads them
        if (it->first == variable)
            return &it->second;
    return nullptr;
}

// The choice the file holds for a switch; -1 for a value the menu does not offer.
int saved_choice(const std::vector<std::pair<std::string, std::string>> &file, const Switch &item)
{
    for (int choice = 0; choice < item.count; ++choice)
    {
        bool same = true;
        for (std::size_t i = 0; i < item.variables.size() && same; ++i)
        {
            if (item.variables[i] == nullptr)
                continue;
            const std::string *value = file_value(file, item.variables[i]);
            const char *wanted = item.choices[static_cast<std::size_t>(choice)].values[i];
            same = wanted == nullptr ? value == nullptr : value != nullptr && *value == wanted;
        }
        if (same)
            return choice;
    }
    return -1;
}

} // namespace

void Launcher::read_testing()
{
    testing_file_ = services_.test_switches();
    testing_saved_.assign(static_cast<std::size_t>(kSwitchCount), 0);
    for (int i = 0; i < kSwitchCount; ++i)
        testing_saved_[static_cast<std::size_t>(i)] = saved_choice(testing_file_, kSwitches[i]);
    testing_chosen_ = testing_saved_;
}

int Launcher::testing_active() const
{
    return static_cast<int>(testing_file_.size());
}

void Launcher::open_testing()
{
    read_testing();
    testing_rows_.visible = kRowsShown;
    testing_rows_.pitch = kRowPitch;
    testing_rows_.reset(kSwitchCount, 0);
    testing_armed_ = false;
    modal_ = modal_shown_ = Modal::testing;
    message_.clear();
    cue(Cue::open);
}

void Launcher::update_testing(float dt)
{
    if (testing_closing_ < 0.0f)
        return;
    testing_closing_ += dt;
    if (testing_closing_ >= kClosingSeconds)
        done_ = true;
}

void Launcher::press_testing(Key key)
{
    if (testing_closing_ >= 0.0f)
        return; // saved: the app is closing
    const bool changed = testing_chosen_ != testing_saved_;
    switch (key)
    {
    case Key::circle:
        if (testing_armed_)
        {
            // Not now: back to the list, with the changes still to apply.
            testing_armed_ = false;
            message_.clear();
            cue(Cue::back);
            return;
        }
        // Back to Diagnostics, on this row; what was not applied is dropped.
        testing_chosen_ = testing_saved_;
        open_modal(Modal::diagnostics);
        option_ = 2;
        option_cursor_.snap(dialog_row_top(Modal::diagnostics, 2));
        return;
    case Key::up:
    case Key::down:
        if (testing_rows_.move(key == Key::down ? 1 : -1))
        {
            testing_armed_ = false;
            message_.clear();
            cue(Cue::focus);
        }
        return;
    case Key::left:
    case Key::right:
    {
        const int row = std::clamp(testing_rows_.selected, 0, kSwitchCount - 1);
        const int count = kSwitches[row].count;
        int &chosen = testing_chosen_[static_cast<std::size_t>(row)];
        const int step = key == Key::left ? -1 : 1;
        chosen = chosen < 0 ? (step > 0 ? 0 : count - 1) : (chosen + step + count) % count;
        testing_armed_ = false;
        message_.clear();
        cue(Cue::toggle);
        return;
    }
    case Key::cross:
        break;
    default:
        return;
    }
    if (!changed)
    {
        say(tr("Nothing to apply: change a switch with Left and Right first."), true);
        cue(Cue::error);
        return;
    }
    if (!testing_armed_)
    {
        testing_armed_ = true;
        say(tr("Apply the changes? EglantinePS5 closes, and they take effect when you open it again. "
               "Press again to confirm."),
            true);
        cue(Cue::focus);
        return;
    }
    // Every variable the menu offers, with what is chosen; a value the menu does not offer stays.
    std::vector<std::string> names;
    std::vector<std::pair<std::string, std::string>> values;
    for (int i = 0; i < kSwitchCount; ++i)
    {
        const Switch &item = kSwitches[i];
        const int chosen = testing_chosen_[static_cast<std::size_t>(i)];
        for (std::size_t v = 0; v < item.variables.size(); ++v)
        {
            if (item.variables[v] == nullptr)
                continue;
            names.emplace_back(item.variables[v]);
            if (chosen >= 0)
            {
                const char *value = item.choices[static_cast<std::size_t>(chosen)].values[v];
                values.emplace_back(item.variables[v], value != nullptr ? value : "");
            }
            else if (const std::string *kept = file_value(testing_file_, item.variables[v]))
            {
                values.emplace_back(item.variables[v], *kept);
            }
        }
    }
    testing_armed_ = false;
    if (!services_.set_test_switches(names, values))
    {
        say(tr("Could not save. Please try again."), true);
        cue(Cue::error);
        return;
    }
    say(tr("Saved. EglantinePS5 closes now: open it again to use the switches."));
    testing_closing_ = 0.0f;
    cue(Cue::saved);
}

void Launcher::draw_testing(Canvas &c, float open)
{
    gfx::DrawList &list = c.list;
    list.push_opacity(open);
    list.push_transform(1.0f - 0.03f * (1.0f - open) * motion(), 960.0f, 540.0f, 0.0f,
                        (1.0f - open) * 26.0f * motion());
    glass(c, kDialog, 26.0f, theme::kPanel.with_alpha(0.97f), theme::kPanelEdge.with_alpha(0.66f), 1.6f);
    text_shrink(c, tr("Testing switches"), 592.0f, baseline(218.0f, 62.0f, theme::kDisplay), theme::kDisplay,
                theme::kTitle, 736.0f);
    text_shrink(c, tr("For tests: they change how the emulator works in every game."), 592.0f,
                baseline(291.0f, 32.0f, theme::kSmall), theme::kSmall, Color::rgb(0xcbb9be), 736.0f);

    list.push_clip({kWindow.x - 24.0f, kWindow.y - 6.0f, kWindow.w + 48.0f, kWindow.h + 12.0f});
    const auto row_top = [&](int row)
    { return kRowsTop + static_cast<float>(row) * kRowPitch - testing_rows_.scroll(); };
    for (int row = testing_rows_.first_row(); row <= testing_rows_.last_row(); ++row)
    {
        list.push_opacity(testing_rows_.row_alpha(row, kRowHeight));
        plate_rest(c, kRowPlate, {592.0f, row_top(row), 736.0f, kRowHeight});
        list.pop_opacity();
    }
    plate_focus(c, kRowPlate, {592.0f, kRowsTop + testing_rows_.cursor() - testing_rows_.scroll(), 736.0f, kRowHeight},
                1.0f);
    for (int row = testing_rows_.first_row(); row <= testing_rows_.last_row(); ++row)
    {
        if (row >= kSwitchCount || row >= static_cast<int>(testing_chosen_.size()))
            break;
        const Switch &item = kSwitches[row];
        const int chosen = testing_chosen_[static_cast<std::size_t>(row)];
        const int saved = testing_saved_[static_cast<std::size_t>(row)];
        const float top = row_top(row);
        list.push_opacity(testing_rows_.row_alpha(row, kRowHeight));
        // A value the menu does not offer shows as it is in the file.
        const std::string *kept = item.variables[0] != nullptr ? file_value(testing_file_, item.variables[0]) : nullptr;
        const std::string value = chosen >= 0 ? std::string{tr(item.choices[static_cast<std::size_t>(chosen)].name)} :
                                                fill(tr("Custom ({0})"), {kept != nullptr ? *kept : std::string{"?"}});
        // Not applied yet: the warning colour. Applied and not the default: lime.
        const Color ink = chosen != saved ? theme::kWarning : chosen != 0 ? theme::kLime : theme::kLimePale;
        const float taken = chooser(c, value, 1296.0f, baseline(top, kRowHeight, theme::kText24),
                                    row == testing_rows_.selected ? 1.0f : 0.0f, ink);
        text_shrink(c, tr(item.name), 628.0f, baseline(top, kRowHeight, theme::kText24), theme::kText24,
                    theme::kValue, 664.0f - taken - 28.0f);
        list.pop_opacity();
    }
    list.pop_clip();
    scrollbar(c, testing_rows_, 1340.0f, kWindow.y, kWindow.h);

    // What the highlighted switch does, under the window.
    text_block(c, tr(kSwitches[std::clamp(testing_rows_.selected, 0, kSwitchCount - 1)].about), 592.0f,
               baseline(kWindow.y + kWindow.h + 22.0f, 30.0f, theme::kSmall), theme::kSmall, 30.0f, theme::kMeta,
               736.0f, 2, kShrink);

    const bool pending = testing_chosen_ != testing_saved_;
    if (!message_.empty())
    {
        notice_block(c, message_, 592.0f, kHints - 6.0f, theme::kSmall, 26.0f,
                     message_warning_ ? theme::kWarning : theme::kLimePale, 736.0f, 2, message_warning_);
    }
    else if (pending)
    {
        static constexpr Hint kApply[] = {
            {Pad::leftright, TR("Change")}, {Pad::cross, TR("Apply")}, {Pad::circle, TR("Back")}};
        draw_hints(c, kApply, 3, 592.0f, kHints, theme::kCopy, 736.0f);
    }
    else
    {
        static constexpr Hint kChange[] = {
            {Pad::updown, TR("Select")}, {Pad::leftright, TR("Change")}, {Pad::circle, TR("Back")}};
        draw_hints(c, kChange, 3, 592.0f, kHints, theme::kCopy, 736.0f);
    }
    list.pop_transform();
    list.pop_opacity();
}

} // namespace pe::ui
