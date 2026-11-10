// SPDX-License-Identifier: GPL-3.0-or-later
// How many fingers the touchpad hardware reports right now (BTN_TOOL_FINGER .. BTN_TOOL_QUINTTAP).
// Uses duplicates of the touchpad file descriptors KWin already has open; only ioctl queries, no reads,
// so nothing is taken away from KWin.
// Optionally (off by default) a resting thumb is left out of the count: a contact in the bottom quarter of the
// touchpad that has stayed still since it landed.
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace tpg
{
inline bool testBit(const unsigned long *bits, int bit)
{
    constexpr int perLong = 8 * sizeof(unsigned long);
    return (bits[bit / perLong] >> (bit % perLong)) & 1UL;
}

class TouchCount
{
public:
    ~TouchCount()
    {
        closeAll();
    }

    int fingers(bool ignoreRestingThumb)
    {
        if (m_devices.empty()) {
            rescan();
        }
        int best = 0;
        bool failed = false;
        for (Device &d : m_devices) {
            unsigned long keys[(KEY_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
            if (ioctl(d.fd, EVIOCGKEY(sizeof(keys)), keys) < 0) {
                failed = true;
                continue;
            }
            int n = toolCount(keys);
            if (ignoreRestingThumb && n > 1) {
                const int thumbs = restingThumbs(d);
                if (thumbs < 0) {
                    failed = true;
                    continue;
                }
                n = std::max(1, n - thumbs);
            }
            best = std::max(best, n);
        }
        if (failed) { // device re-opened (resume, re-plug): find it again next time
            closeAll();
        }
        return best;
    }

private:
    struct Slot {
        int id = -1; // tracking id of the contact in this slot, -1 = empty
        int startX = 0; // where it landed
        int startY = 0;
        bool moved = false; // has it moved away from there since
    };
    struct Device {
        int fd = -1;
        int slots = 0;
        input_absinfo x{};
        input_absinfo y{};
        std::vector<Slot> state;
    };

    static int toolCount(const unsigned long *keys)
    {
        if (testBit(keys, BTN_TOOL_QUINTTAP)) {
            return 5;
        }
        if (testBit(keys, BTN_TOOL_QUADTAP)) {
            return 4;
        }
        if (testBit(keys, BTN_TOOL_TRIPLETAP)) {
            return 3;
        }
        if (testBit(keys, BTN_TOOL_DOUBLETAP)) {
            return 2;
        }
        return testBit(keys, BTN_TOOL_FINGER) ? 1 : 0;
    }

    // Contacts in the bottom quarter of the touchpad that have not moved more than about 3 mm since they landed.
    // Returns -1 if the device could not be queried.
    static int restingThumbs(Device &d)
    {
        if (d.slots <= 0) {
            return 0;
        }
        const auto query = [&d](int code, std::vector<int32_t> &out) {
            std::vector<int32_t> buf(d.slots + 1);
            buf[0] = code;
            if (ioctl(d.fd, EVIOCGMTSLOTS(buf.size() * sizeof(int32_t)), buf.data()) < 0) {
                return false;
            }
            out.assign(buf.begin() + 1, buf.end());
            return true;
        };
        std::vector<int32_t> ids, xs, ys;
        if (!query(ABS_MT_TRACKING_ID, ids) || !query(ABS_MT_POSITION_X, xs) || !query(ABS_MT_POSITION_Y, ys)) {
            return -1;
        }
        const int height = d.y.maximum - d.y.minimum;
        const int bottomZone = d.y.maximum - height / 4;
        const int still = d.y.resolution > 0 ? 3 * d.y.resolution : std::max(1, height / 30); // resolution: units per mm
        int thumbs = 0;
        for (int i = 0; i < d.slots; ++i) {
            Slot &s = d.state[i];
            if (ids[i] < 0) {
                s.id = -1;
                continue;
            }
            if (s.id != ids[i]) {
                s = {ids[i], xs[i], ys[i], false}; // a new contact: remember where it landed
            } else if (!s.moved && (std::abs(xs[i] - s.startX) > still || std::abs(ys[i] - s.startY) > still)) {
                s.moved = true;
            }
            if (!s.moved && ys[i] >= bottomZone) {
                ++thumbs;
            }
        }
        return thumbs;
    }

    void closeAll()
    {
        for (const Device &d : m_devices) {
            ::close(d.fd);
        }
        m_devices.clear();
    }

    void rescan()
    {
        const auto now = std::chrono::steady_clock::now();
        if (now - m_lastScan < std::chrono::seconds(2)) {
            return;
        }
        m_lastScan = now;
        closeAll();
        DIR *dir = opendir("/proc/self/fd");
        if (!dir) {
            return;
        }
        while (dirent *e = readdir(dir)) {
            const int fd = atoi(e->d_name);
            if (fd <= 2) {
                continue;
            }
            char path[64];
            char target[128] = {};
            snprintf(path, sizeof(path), "/proc/self/fd/%d", fd);
            const ssize_t len = readlink(path, target, sizeof(target) - 1);
            if (len <= 0 || strncmp(target, "/dev/input/event", 16) != 0) {
                continue;
            }
            unsigned long keybits[(KEY_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
            unsigned long props[(INPUT_PROP_MAX + 1) / (8 * sizeof(unsigned long)) + 1] = {};
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybits)), keybits) < 0 || ioctl(fd, EVIOCGPROP(sizeof(props)), props) < 0) {
                continue;
            }
            const bool touchpad = testBit(props, INPUT_PROP_POINTER) && testBit(keybits, BTN_TOOL_FINGER) && testBit(keybits, BTN_TOOL_DOUBLETAP);
            if (!touchpad) {
                continue;
            }
            Device d;
            d.fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
            if (d.fd < 0) {
                continue;
            }
            input_absinfo slot{};
            if (ioctl(d.fd, EVIOCGABS(ABS_MT_SLOT), &slot) == 0 && ioctl(d.fd, EVIOCGABS(ABS_MT_POSITION_X), &d.x) == 0
                && ioctl(d.fd, EVIOCGABS(ABS_MT_POSITION_Y), &d.y) == 0) {
                d.slots = std::clamp(slot.maximum + 1, 0, 16);
                d.state.resize(d.slots);
            }
            m_devices.push_back(std::move(d));
        }
        closedir(dir);
    }

    std::vector<Device> m_devices;
    std::chrono::steady_clock::time_point m_lastScan{};
};
}
