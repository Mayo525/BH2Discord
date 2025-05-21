""""
MIT License

Copyright(c) 2025 Mayo525

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files(the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions :

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
""""

#include <discord.h>
#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <chrono>
#include <windows.h>

 // Discord Core instance
std::unique_ptr<discord::Core> core{};

// Initialize Discord Core with the app ID
bool InitDiscord() {
    discord::Core* rawCore{};
    if (discord::Core::Create(1361255157580566558, DiscordCreateFlags_Default, &rawCore) != discord::Result::Ok)
        return false;
    core.reset(rawCore);
    return true;
}

// Calculate how long the user has been idle (in seconds)
DWORD GetIdleTime() {
    LASTINPUTINFO lii = { sizeof(LASTINPUTINFO) };
    GetLastInputInfo(&lii);
    return (GetTickCount() - lii.dwTime) / 1000;
}

// Get path to the log file where the game writes its status
std::string GetLogPath() {
    char docs[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, docs);
    return std::string(docs) + "\\ColdBeamGames\\BeatHazard2\\discord.txt";
}

// Read the log file and extract current game state and details (like song name, score)
void ParseLog(std::string& state, std::string& details) {
    std::ifstream file(GetLogPath());
    if (!file.is_open()) return;

    std::string line;
    while (std::getline(file, line)) {
        if (line.find("Playing") != std::string::npos) {
            size_t dash = line.find('-');
            std::string song = (dash != std::string::npos) ? line.substr(dash + 1) : "";
            song.erase(0, song.find_first_not_of(" -)"));

            if (song.ends_with(".mp3"))
                song = song.substr(0, song.size() - 4);

            std::string score, mult;
            size_t s = song.find("Score");
            if (s != std::string::npos) {
                score = song.substr(s + 6, song.find(" -", s) - (s + 6));
                size_t m = song.find("Multiplier");
                if (m != std::string::npos)
                    mult = song.substr(m + 11, song.find(" ", m) - (m + 11));
                song = song.substr(0, s - 1);
            }

            details = song;
            if (!score.empty() && !mult.empty())
                details += " - Score: " + score + " - Mult: " + mult;

            state = "Playing";
            break;
        }
    }
}

// Update Discord Rich Presence with current state and details
// If user is idle for more than 60 seconds, mark them as AFK
void UpdatePresence(const std::string& state, const std::string& details, int64_t startTime, bool isIdle) {
    discord::Activity a{};
    a.SetState(state.c_str());
    a.SetDetails(details.c_str());
    a.SetType(discord::ActivityType::Playing);
    a.GetAssets().SetLargeImage("beat_hazard");
    a.GetAssets().SetLargeText("Beat Hazard 2");

    if (isIdle) {
        a.GetAssets().SetSmallImage("idle_icon");
        a.GetAssets().SetSmallText("AFK");
    }

    a.GetTimestamps().SetStart(startTime);
    core->ActivityManager().UpdateActivity(a, [](discord::Result r) {
        if (r != discord::Result::Ok) std::cerr << "Presence update failed.\n";
        });
}

int main() {
    // Start Discord connection
    if (!InitDiscord()) return 1;

    // Get current time to show elapsed play time
    int64_t start = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    // Main loop: run Discord callbacks, read log, update presence every second
    while (true) {
        core->RunCallbacks();

        std::string state = "Idle";
        std::string details = "Waiting for data...";

        ParseLog(state, details);

        DWORD idleTime = GetIdleTime();
        bool isIdle = idleTime > 60;

        UpdatePresence(state, details, start, isIdle);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
